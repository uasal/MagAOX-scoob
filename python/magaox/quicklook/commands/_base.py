import datetime
from datetime import timezone
import socket
import typing
import psycopg
import xconf
import upath

import magaox.db.config as dbconfig

from ... import constants, utils

@xconf.config
class BaseQuicklookCommand(dbconfig.BaseConfig, xconf.Command):
    database : dbconfig.DbConfig = xconf.field(default=dbconfig.DbConfig(), help="PostgreSQL database connection")
    dry_run : bool = xconf.field(default=False, help="Whether to perform a dry run or actually execute the necessary commands")
    title : typing.Optional[str] = xconf.field(default=None, help="All or part of the observation name to process")
    email : typing.Optional[str] = xconf.field(default=None, help="Email address for the observer to process")
    semester : typing.Optional[str] = xconf.field(default=utils.get_current_semester(), help="Semester to search in, 202XXA/20XXB format")
    utc_start : typing.Optional[datetime.datetime] = xconf.field(default=None, help="ISO UTC datetime stamp of earliest observation start time to process (supersedes semester)")
    utc_end : typing.Optional[datetime.datetime] = xconf.field(default=None, help="ISO UTC datetime stamp of latest observation end time to process (supersedes semester)")
    data_roots : list[upath.UPath] = xconf.field(default_factory=constants.LOOKYLOO_DATA_ROOTS.copy, help=f"Search directory for telem and rawimages subdirectories, repeat to specify multiple roots. (default: {constants.LOOKYLOO_DATA_ROOTS})")
    common_path_prefix : str = xconf.field(default=constants.DEFAULT_PREFIX, help="Prefix for all instrument data and config directories")

    def get_search_start_end_timestamps(
        self,
        semester : str,
        utc_start : typing.Optional[datetime.datetime] = None,
        utc_end : typing.Optional[datetime.datetime] = None,
    ):
        letter = semester[-1].upper()
        try:
            if len(semester) != 5 or semester[-1].upper() not in ['A', 'B']:
                raise ValueError()
            year = int(semester[:-1])
            month = 1 if letter == 'A' else 6
            day = 15 if month == 6 else 1
        except ValueError:
            raise RuntimeError(f"Got {semester=} but need a 4 digit year + A or B (e.g. 2022A)")
        semester_start_dt = datetime.datetime(year, month, day)
        semester_start_dt = semester_start_dt.replace(tzinfo=timezone.utc)
        start_dt = semester_start_dt
        semester_end_dt = datetime.datetime(
            year=year + 1 if letter == 'B' else year,
            month=1 if letter == 'B' else 6,
            day = 15 if letter == 'A' else 1,
        ).replace(tzinfo=timezone.utc)
        end_dt = semester_end_dt

        if utc_start is not None:
            start_dt = utc_start

        if utc_end is not None:
            end_dt = utc_end

        if end_dt < start_dt:
            raise ValueError("End time is before start time")
        return start_dt, end_dt

    def main(self):
        raise NotImplementedError("Command subclasses must implement main()")
