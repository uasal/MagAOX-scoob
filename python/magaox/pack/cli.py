import xconf

from . import core
from .db import config as dbconfig

@xconf.config
class Pack(dbconfig.BaseConfig, xconf.Command):
    def main(self):
        pass