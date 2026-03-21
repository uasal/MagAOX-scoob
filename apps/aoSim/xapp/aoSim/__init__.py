import logging
from enum import Enum
import time
import numpy as np
import hcipy as hp

import ImageStreamIOWrap
import xconf

from magaox.indi.device import XDevice, BaseConfig
from magaox.shmim import Image
from magaox.constants import StateCodes
from magaox.state_manager import XStateMachine

from purepyindi2 import device, properties, constants
from purepyindi2.messages import DefNumber, DefSwitch, DefLight, DefText

def create_if_not_exist_shmim(name, shape, dtype=np.float32):
    img = Image()
    
    do_make=False
    # Check if the shmim exists
    if img.open(name) == 40:
        do_make = True
    else:
        # the shmim opened successfully, check if it has the right shape and dtype, if not destroy and recreate
        if img.md.size[0] == shape[0] and img.md.size[1] == shape[1] and img.md.data_type == (ISIO.ImageStreamIODataType.FLOAT if dtype == np.float32 else ISIO.ImageStreamIODataType.DOUBLE):
            img.destroy(name)
            do_make = True

    # Remake the shmim if it doesn't exist or if the existing one has the wrong shape/dtype
    if do_make:
        if dtype == np.float32:
                img.create(name, shape, ImageStreamIOWrap.ImageStreamIODataType.FLOAT, 1, 8)
        elif dtype == np.float64:
            img.create(name, shape, ImageStreamIOWrap.ImageStreamIODataType.DOUBLE, 1, 8)
    
    # Close the connection to the shmim.
    img.close(name)

@xconf.config
class aoSimConfig(BaseConfig):
    """Configuration for the aoSim application
    """
    #wfs_shmim : str = xconf.field(help="Output wavefront sensor shmim name.")
    #disturbance_shmim : str = xconf.field(help="Output disturbance shmim name.")
    #dm_shmim : str = xconf.field(help="Output wavefront sensor shmim name.")

class States(Enum):
    IDLE = 0
    FWPUPIL = 1
    FWLYOT = 2
    CENTROID = 3

class aoSim(XDevice):
    config : aoSimConfig

    def setup(self):
        self.log.debug(f"I was configured! See? {self.config=}")

        self._nmodes = 1

        # Make this configurable in the future
        create_if_not_exist_shmim('aoSim_dm', [self._nmodes, 1], dtype=np.float32)
        self._dm = Image('aoSim_dm')

        create_if_not_exist_shmim('aoSim_wfs', [self._nmodes, 1], dtype=np.float32)
        self._wfs = Image('aoSim_wfs')

        create_if_not_exist_shmim('aoSim_disturbance', [self._nmodes, 1], dtype=np.float32)
        self._disturbance = Image('aoSim_disturbance')

        self._t = 0
        self._dt = 0.01
        self._current_disturbance = np.zeros((self._nmodes, 1), dtype=np.float32)
        self._current_dm_state = np.zeros((self._nmodes, 1), dtype=np.float32)

        self._lag = 1
        self._dm_command_history = np.zeros((self._lag + 1, self._nmodes, 1), dtype=np.float32)

        self.log.info(f'aoSim app is fully setup.')

    def connect_shmims(self):
        pass

    def update_dm(self):
        self._current_dm_state = self._dm_command_history[0]
        self._dm_command_history = np.roll(self._dm_command_history, shift=-1, axis=0)
        self._dm_command_history[-1] = self._dm.get_data(wait=False)
        
    def update_wfs(self):
        self.err = self._current_disturbance + self._current_dm_state
        self._wfs.write(self.err)

    def update_disturbance(self):
        # In the future, we can make this more complex and configurable, but for now just use a simple sinusoidal disturbance
        self._current_disturbance = 0.1 * np.sin(2 * np.pi * 0.5 * self._t) * np.ones((self._nmodes, 1), dtype=np.float32)
        self._disturbance.write(self._current_disturbance)

    def loop(self):
        '''
        '''
        self.update_dm()
        self.update_disturbance()
        self.update_wfs()
        
        print(f'{self._t=}, {self._current_disturbance.flatten()=}, {self._current_dm_state.flatten()=}, {self.err.flatten()=}')
        self._t += self._dt

    def update_properties(self):
        '''
        '''
        pass

main = aoSim.console_app
