import pathlib
import logging

import xconf

from magaox.db import ingest
from magaox.constants import DEFAULT_PREFIX, DEFAULT_DATA_DIRS
from ._base import BaseDbCommand

log = logging.getLogger(__name__)

@xconf.config
class Inventory(BaseDbCommand):
    '''Find files that aren't yet inventoried and create records for
    them in the file_origins table
    '''
    data_dirs : list[pathlib.Path] = xconf.field(default_factory=lambda: [DEFAULT_PREFIX / x for x in DEFAULT_DATA_DIRS])

    def main(self):
        for db_name in self.databases:
            conn = self.databases[db_name].connect()
            log.info(f"Updating file inventory for database {db_name}")
            with conn.transaction():
                cur = conn.cursor()
                ingest.update_file_inventory(cur, self.hostname, self.data_dirs, self.ignore_patterns.files, self.ignore_patterns.directories)
