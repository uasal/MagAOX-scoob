import os
import time
import typing
from scipy.stats import mode
from tqdm import tqdm
import concurrent.futures
import shutil
import tempfile
import sys
import fixr
import fsspec
import logging
import numcodecs
from numcodecs import Blosc, Delta
import zarr
import numpy as np
from magaox import db
import xconf

from ..constants import FOLDER_TIMESTAMP_FORMAT
from .core import datestamp_strings_from_ts


logging.basicConfig(level=logging.INFO)
log = logging.getLogger(__name__)
log.setLevel(logging.DEBUG)


class EmptyKeyError(Exception):
    pass

def infer_dtype(val):
    if not np.isscalar(val):
        raise ValueError(f"Got {repr(val)} ({type(val)}) but this only works on scalar types")
    elif isinstance(val, str):
        return str
    else:
        return np.result_type(val)

def tel2zarr(msg: dict, path: list[str], root, row_count, chunk_size):
    flattened_msg_keys = []
    for k in msg:
        if not isinstance(msg[k], dict):
            p = '/'.join(path + [k])
            # infer dtype from msg[k]
            shape = (row_count,)
            if isinstance(msg[k], list) and len(msg[k]) > 0:
                shape += (len(msg[k]),)
                dtype = infer_dtype(msg[k][0])
            elif isinstance(msg[k], list):
                # length-zero lists cannot have useful information
                continue
            else:
                dtype = infer_dtype(msg[k])
            arr = root.zeros(p, shape=shape, chunks=(chunk_size,), dtype=dtype)
            def accessor(the_msg, k=k):
                for part in path:
                    the_msg = the_msg[part]
                return the_msg[k]
            flattened_msg_keys.append((p, arr, accessor))
        else:
            flattened_msg_keys.extend(tel2zarr(msg[k], path + [k], root, row_count, chunk_size))
    return flattened_msg_keys

def datetime_to_seconds_nanos(dt):
    seconds = int(dt.timestamp())
    nanoseconds = int(dt.microsecond * 1000)
    return (seconds, nanoseconds)

def unpack_one_xrif(local_path, log):
    # log.debug(f"Unpacking {local_path}")
    with open(local_path, 'rb') as f:
        img = fixr.XrifReader(f).copy_data()
        # n.b. after reading `img`, file `f` has seeked (sought?) to
        # the beginning of the timing xrif archive concatenated onto
        # the image data
        times = fixr.XrifReader(f).copy_data()
        img = img.reshape((-1,) + img.shape[2:])
        times = times.reshape((-1,) + (times.shape[-1],))
    return img, times

def preprocess_path(origin_host, origin_path):
    if origin_host == 'exao2':
        local_path = origin_path.replace('/opt/MagAOX', '/srv/rtc/data')
    elif origin_host == 'exao3':
        local_path = origin_path.replace('/opt/MagAOX', '/srv/icc/data')
    else:
        local_path = origin_path
    return local_path

def infer_common_xrif_cube_size_dtype(paths):
    frames = []
    planes = []
    height = []
    width = []
    shapes = set()
    img_dtype = None
    times_dtype = None
    for p in paths:
        with open(p, "rb") as fh:
            xr = fixr.XrifReader(fh)
            img_dtype = xr.array.dtype
            fr, pl, ht, wd = xr.shape
            shapes.add(xr.shape)
            frames.append(fr)
            planes.append(pl)
            height.append(ht)
            width.append(wd)
            xr2 = fixr.XrifReader(fh)
            times_dtype = xr2.array.dtype
    
    common_shape = np.max(frames), mode(planes).mode, mode(height).mode, mode(width).mode
    log.debug(f"Guessed {common_shape=} from {shapes=}")
    return common_shape, img_dtype, times_dtype

def repack_cam_channel(camera_channel, root, bounds, cur, chunk_size) -> typing.Optional[tuple[zarr.Array, zarr.Array]]:
    other_files_args = bounds + (camera_channel,)
    other_files_q_body = '''
    FROM file_origins 
    WHERE creation_time 
        BETWEEN %s AND %s
        AND origin_path LIKE '%%' || %s || '%%.xrif'
    '''
    other_files_count_q = '''SELECT COUNT(*) as count''' + other_files_q_body
    cur.execute(other_files_count_q, other_files_args)
    xrifs_row_count = cur.fetchone()['count']
    if xrifs_row_count == 0:
        log.debug(f"No {camera_channel} frames to process")
        return None
    else:
        log.debug(f"Got {xrifs_row_count} XRIF files to process")
    other_files_q = '''
    SELECT
        origin_host, origin_path, creation_time, size_bytes
    ''' + other_files_q_body
    
    # xrif archives contain an unpredictable number of frames.
    # guessing a shape from the largest (by bytes) archive can get
    # confused when there's an outlier xrif (i.e. changed ROI during obs)
    #
    # instead, maybe just find the most common shape, and drop the rest
    # with a warning?
    n_shape_samples = 10
    cur.execute(other_files_q + f" order by random() desc limit {n_shape_samples}", other_files_args)
    ex_rows = cur.fetchall()
    ex_paths = [preprocess_path(r['origin_host'], r['origin_path']) for r in ex_rows]
    # skip planes per frame (for now?)
    (frames_per_xrif_chunk, _, img_height, img_width), img_dtype, times_dtype = infer_common_xrif_cube_size_dtype(ex_paths)
    times_width = 5  # five time values per frame are stored (idx, acq sec, acq nsec, wrt sec, wrt nsec)

    pool = concurrent.futures.ThreadPoolExecutor(max_workers=10)

    with tempfile.TemporaryDirectory() as td:
        rechunk = zarr.open_group(f"file://" + td)
        frames_tmp = rechunk.zeros(
            'frames',
            shape=(frames_per_xrif_chunk * xrifs_row_count,) + (img_height, img_width),
            chunks=(frames_per_xrif_chunk,),
            dtype=img_dtype,
        )
        times_tmp = rechunk.zeros(
            'times',
            shape=(frames_per_xrif_chunk * xrifs_row_count,) + (times_width,),
            chunks=(frames_per_xrif_chunk,),
            dtype=times_dtype,
        )

        results = []
        cur.execute(other_files_q + " ORDER BY creation_time ASC", bounds + (camera_channel,))
        log.info("Submitting futures...")
        total_bytes = 0
        for idx, row in enumerate(cur):
            total_bytes += row['size_bytes']
            local_path = preprocess_path(row['origin_host'], row['origin_path'])
            unpack_args =  local_path, log
            results.append(pool.submit(unpack_one_xrif, *unpack_args))
        log.info(f"Submitted {idx + 1} futures, total compressed size {total_bytes} bytes")
        start_idx = 0
        # iterate in-order because `other_files_q` has an `ORDER BY` and we want to
        # preserve time ordering
        for res in tqdm(results):
            frames, times = res.result()
            if frames.shape[0] != times.shape[0]:
                log.warning(f"Discarding {frames.shape[0]} frames because xrif wrote {times.shape[0]} timestamps for this archive")
                continue
            n_frames = frames.shape[0]
            if frames.shape[1:] != (img_height, img_width):
                log.warning(f"Skipping {n_frames} frames because {frames.shape=} but {frames_tmp.shape=}")
                continue
            # log.debug(f"Got {frames.shape=}, {n_frames=}, going in to {frames_tmp.shape=} (guessed {frames_per_xrif_chunk, img_height, img_width=}) at {start_idx=}")
            frames_tmp[start_idx:start_idx + n_frames] = frames
            times_tmp[start_idx:start_idx + n_frames] = times
            start_idx += n_frames
        total_frames = start_idx
        log.debug(f"Results gotten: {total_frames=}, assigning into final archives")

        compressor = Blosc(cname='zstd', clevel=5, shuffle=Blosc.SHUFFLE)
        # filters = [Delta(dtype='u2')]
        # test showed no delta compresses better / decompresses faster
        filters = []
        cam_frames = root.zeros(
            f'frames',
            shape=(total_frames,) + (img_height, img_width),
            chunks=(chunk_size,),
            dtype=img_dtype,
            filters=filters,
            compressor=compressor,
        )
        cam_times = root.zeros(
            f'times',
            shape=(total_frames,) + (times_width,),
            chunks=(chunk_size,),
            dtype=times_dtype,
        )
        log.info(f"Created {cam_frames} with {filters=} {compressor=}")
        start = time.perf_counter()
        cam_frames[:] = frames_tmp[:start_idx]
        dt = time.perf_counter() - start
        log.debug(f"Compressed and wrote frames in {dt} sec")
        
        start = time.perf_counter()
        cam_times[:] = times_tmp[:start_idx]
        dt = time.perf_counter() - start
        log.debug(f"Compressed and wrote times in {dt} sec")
    return cam_frames, cam_times

def pack_one_obs(start_ts, obs_email, obs_name):
    raise NotImplementedError()
    semester, night = datestamp_strings_from_ts(start_ts)
    title = f"{start_ts.strftime(FOLDER_TIMESTAMP_FORMAT)}_{obs_name}"

    if not obs_email:  # can be empty
        obs_email = "_no_email_"

    # ... / 2022B / a@b.edu / 2022-02-02_020304_label
    simple_observer_prefix = f"{semester}/{obs_email}/{title}.zarr"
    root = zarr.open_group(f"file://./output/{simple_observer_prefix}/")

def main():
    obs_name = 'HD141569_2x2binning_iz_unsats'
    sci_chunk_size, wfs_chunk_size = 400, 5 * 2000
    root = zarr.open_group(f"file:///home/jlong/packr/output/{obs_name}.zarr/")
    
    # dbconfig = db.DbConfig(user='jlong', host='localhost')
    os.environ['XTELEMDB_PASSWORD'] = 'extremeAO!'
    dbconfig = db.DbConfig(user='xsup')
    conn = dbconfig.connect()
    cur = conn.cursor()
    cur.execute("SELECT * FROM observations WHERE obsName = %s", (obs_name,))
    rows = cur.fetchall()
    if len(rows) > 1:
        print("Got more than one matching obs:", rows)
    res = rows[0]
    bounds = start_ts, end_ts = res['start_ts'], res['end_ts']

    # bounds = '2023-03-13 07:17:30.589645+00', '2023-03-13 07:19:54.23374+00'
    start_ts, end_ts = "2024-05-25T07:44:01.893517+00:00", "2024-05-25T09:27:42.979882+00:00"

    if False:
        cur.execute('''
        SELECT DISTINCT device, ec FROM telem WHERE ts BETWEEN %s AND %s;
        ''', bounds)
        device_event_code_pairs = cur.fetchall()

        for dev_ec in device_event_code_pairs:
            log.debug(f"Processing {dev_ec}")
            telem_query_args = (dev_ec['device'], dev_ec['ec']) + bounds
            cur.execute("""
    SELECT COUNT(*)
    FROM telem
    WHERE
        device = %s
        AND ec = %s
        AND ts BETWEEN %s AND %s
    """, telem_query_args)
            row_count = cur.fetchone()['count']
            log.debug(f"{dev_ec['device']} {dev_ec['ec']} has {row_count} rows")

            telem_range_query = "SELECT * FROM telem WHERE device = %s AND ec = %s AND ts BETWEEN %s AND %s ORDER BY ts ASC"
            cur.execute(telem_range_query + " LIMIT 1", telem_query_args)
            one_example = cur.fetchone()

            this_dev_ec_path = f"telem/{dev_ec['device']}/{dev_ec['ec']}"
            this_ec_root = root.require_group(this_dev_ec_path)
            telem_element_sequence = tel2zarr(one_example['msg'], [], this_ec_root, row_count, 1000)
            ts_dtype = [('sec', 'i4'), ('nsec', 'i4')]
            ts_arr = this_ec_root.zeros('ts', shape=(row_count,), chunks=(wfs_chunk_size,), dtype=ts_dtype)
            chunks = max(1, row_count // wfs_chunk_size + 1)
            log.debug(f"Using {row_count=} {wfs_chunk_size=} gives {chunks=}")
            # allocate chunks per telem element
            per_telem_chunks = []
            buffer_size = min(wfs_chunk_size, row_count)
            for path, arr, accessor in telem_element_sequence:
                per_telem_chunks.append(np.zeros((buffer_size,) + arr.shape[1:], dtype=arr.dtype))
                log.debug(f"Preallocating {buffer_size} {arr.dtype} elements for {path}")
            ts_chunk = np.zeros(buffer_size, dtype=ts_dtype)
            for i in range(chunks):
                log.debug(f"Chunk {i+1}")
                q = telem_range_query
                if chunks == 1:
                    q = telem_range_query
                else:
                    q = telem_range_query + f" LIMIT {wfs_chunk_size}"
                if i != 0:
                    q += f" OFFSET {i * wfs_chunk_size}"
                log.debug(f"Querying: {q}")
                cur.execute(q, telem_query_args)

                # _count = 0
                for idx, row in enumerate(cur):
                    # log.debug(f"Chunk {i+1} row {idx}")
                    sec, nsec = datetime_to_seconds_nanos(row['ts'])
                    ts_chunk[idx]['sec'] = sec
                    ts_chunk[idx]['nsec'] = nsec
                    # log.debug(f"{ts_chunk[idx]=}")
                    for buffer_array, (path, _, accessor) in zip(per_telem_chunks, telem_element_sequence):
                        # log.debug(f"{path} {buffer_array[idx]=}")
                        buffer_array[idx] = accessor(row['msg'])
                    # _count += 1

                # assert _count == idx + 1
                slice_start = i * wfs_chunk_size
                chunk_len = idx + 1  # for partial chunks: after loop, `idx` is last index, exclusive bound is one more 
                slice_stop = slice_start + chunk_len
                
                # assign chunk into zarr
                for buffer_array, (path, arr, accessor) in zip(per_telem_chunks, telem_element_sequence):
                    log.debug(f"{path} {arr} {slice_start=} {slice_stop=}")
                    arr[slice_start:slice_stop] = buffer_array[:chunk_len]
                ts_arr[slice_start:slice_stop] = ts_chunk[:chunk_len]

    # loop over other matching files
    
    for camera_channel in [
        # 'camsci1',
        # 'camsci2',
        # 'camtip',
        # 'camacq',
    ]:
        log.info(f"Checking for {camera_channel}...")
        res = repack_cam_channel(camera_channel, root.create_group('sci/' + camera_channel), bounds, cur, sci_chunk_size)
        if res is not None:
            frames, times = res
            log.debug(f"{frames.info=}")
        else:
            log.debug(f"No {camera_channel} frames")
    for camera_channel in [
        # 'camwfs',
        'camlowfs',
    ]:
        log.info(f"Checking for {camera_channel}...")
        res = repack_cam_channel(camera_channel, root.create_group('wfs/' + camera_channel), bounds, cur, wfs_chunk_size)
        if res is not None:
            frames, times = res
            log.debug(f"{frames.info=}")
        else:
            log.debug(f"No {camera_channel} frames")
        
    import IPython
    IPython.embed()


if __name__ == "__main__":
    main()