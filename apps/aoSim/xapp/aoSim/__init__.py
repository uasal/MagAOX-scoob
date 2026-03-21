import logging
from enum import Enum
import time
import numpy as np
import hcipy as hp

import ImageStreamIOWrap
import xconf

from magaox.indi.device import XDevice, BaseConfig
from magaox.constants import StateCodes
from magaox.state_manager import XStateMachine

from purepyindi2 import device, properties, constants
from purepyindi2.messages import DefNumber, DefSwitch, DefLight, DefText

def open_shmim(name, shape, dtype=np.float32, create_if_not_exists=True):
    img = ImageStreamIOWrap.Image()

    
    if dtype == np.float32:
        img.create(name, shape, ISIO.ImageStreamIODataType.FLOAT, 1, 8)
    elif dtype == np.float64:
        img.create(name, shape, ISIO.ImageStreamIODataType.DOUBLE, 1, 8)

    return ImageStreamIOWrap.ImageStreamIOWrap(name, shape, dtype)

@xconf.config
class aoSimConfig(BaseConfig):
    """Configuration for the aoSim application
    """
    #wfs_shmim : str = xconf.field(help="Output wavefront sensor shmim name.")
    #disturbance_shmim : str = xconf.field(help="Output disturbance shmim name.")
    #dm_shmim : str = xconf.field(help="Output wavefront sensor shmim name.")
    #sleep_interval_sec

class States(Enum):
    IDLE = 0
    FWPUPIL = 1
    FWLYOT = 2
    CENTROID = 3

class aoSim(XDevice):
    config : aoSimConfig

    def setup(self):
        self.log.debug(f"I was configured! See? {self.config=}")

        #fsm = properties.TextVector(name='fsm')
        #fsm.add_element(DefText(name='state', _value=StateCodes.INITIALIZED.name))
        #self.add_property(fsm)

        # self._state_names = ['idle', 'fwpupil', 'fwlyot', 'centroid']
        # self._state_callbacks = [None, self.handle_fwpupil, self.handle_fwlyot, self.handle_centroid]
        # self._state_machine = XStateMachine(self, self._state_names, States, self._state_callbacks)
        self.connect_shmims()
        self._dm = None
        self._wfs = None
        self._disturbance = None

        self._lag = 1

        self.log.info(f'aoSim app is fully setup.')

    def connect_shmims(self):
        pass

    def get_dm_state(self):
        self._current_dm_state = self._dm.get_data()

    def set_wfs_data(self):
        self.err = self._disturbance.get_data() + self._dm.get_data()
        # Now set the wavefront sensor data

    def loop(self):
        '''
        '''
        #self._state_machine.loop()
        #self._reference_state_machine.loop()
        #self.update_properties()
        self.get_dm_state()
        self.set_wfs_data()

    def update_properties(self):
        '''
        '''
        pass


main = aoSim.console_app
