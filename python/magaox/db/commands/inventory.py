import xconf
import pathlib
from datetime import timezone
import datetime
import logging
import os
import os.path
import psycopg
import socket
from tqdm import tqdm

from magaox.db import FileOrigin, ingest
from magaox.constants import DEFAULT_PREFIX, DEFAULT_DATA_DIRS

import xconf
from ._base import BaseDbCommand

log = logging.getLogger(__name__)

@xconf.config
class Inventory(BaseDbCommand):
    '''Find files that aren't yet inventoried and create records for
    them in the file_origins table
    '''
    data_dirs : list[pathlib.Path] = xconf.field(default_factory=lambda: [DEFAULT_PREFIX / x for x in DEFAULT_DATA_DIRS])

    def main(self):
        connections = self.connect_to_databases()
        for conn in connections:
            log.info(f"Updating file inventory for {conn.info.dsn}")
            with conn.transaction():
                cur = conn.cursor()
                ingest.update_file_inventory(cur, self.hostname, self.data_dirs)