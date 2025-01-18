import time
import os
import typing
from concurrent import futures
import logging
import datetime
from datetime import timezone
import pathlib
import argparse
from ...constants import HISTORY_FILENAME, ALL_CAMERAS, LOOKYLOO_DATA_ROOTS, QUICKLOOK_PATH, DEFAULT_CUBE, DEFAULT_SEPARATE, CHECK_INTERVAL_SEC, LOG_PATH
from ...utils import parse_iso_datetime, format_timestamp_for_filename, utcnow, get_search_start_end_timestamps
from ..core import (
    TimestampedFile,
    ObservationSpan,
    load_file_history,
    do_quicklook_for_camera,
    get_new_observation_spans,
    process_span,
    decide_to_process,
    create_bundle_from_span,
)



import xconf
from ._base import BaseQuicklookCommand

from ... import constants

log = logging.getLogger(__name__)

@xconf.config
class Pack(BaseQuicklookCommand):
    output_dir : pathlib.Path = xconf.field(default=pathlib.Path('.'), help="Path or URI to destination")
    parallel_jobs : int = xconf.field(default=10, help="How many export jobs to start in parallel")

    def main(self):
        if not self.output_dir.is_dir():
            self.output_dir.mkdir(parents=True, exist_ok=True)
        log.debug(f"Starting the packer with {self.output_dir}")
