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
    """Create or verify a shared memory image (shmim) with the specified parameters.
    
    Creates a new shared memory image if it does not exist, or verifies that an existing
    image has the correct shape and data type. If an existing image has incompatible
    parameters, it is destroyed and recreated.
    
    Args:
        name (str): The name of the shared memory image to create or access.
        shape (tuple): The desired shape of the array as (height, width).
        dtype (type, optional): The numpy data type for the array. 
            Defaults to np.float32 (FLOAT). Can also be np.float64 (DOUBLE).
    
    Returns:
        None
        
    Note:
        This function closes the connection to the shmim after creation/verification.
        Other processes can open the shmim by its name independently.
    """
    img = ImageStreamIOWrap.Image()
    
    do_make=False
    # Check if the shmim exists
    if img.open(name) == 40:
        do_make = True
    else:
        # the shmim opened successfully, check if it has the right shape and dtype, if not destroy and recreate
        if not (img.md.size[0] == shape[0] and img.md.size[1] == shape[1] and img.md.datatype == (ImageStreamIOWrap.ImageStreamIODataType.FLOAT if dtype == np.float32 else ImageStreamIOWrap.ImageStreamIODataType.DOUBLE)):
            img.destroy()
            do_make = True

    # Remake the shmim if it doesn't exist or if the existing one has the wrong shape/dtype
    if do_make:
        img.create(name, np.zeros(shape, dtype=dtype))
    
    # Close the connection to the shmim.
    img.close()

@xconf.config
class aoSimConfig(BaseConfig):
    """Configuration for the aoSim application.
    """
    num_modes : int = xconf.field(default=2, help="Number of modes to simulate in the AO system.")
    lag : int = xconf.field(default=1, help="Lag in timesteps for DM command updates.")

class aoSim(XDevice):
    """Adaptive Optics System Simulator.
    
    The aoSim application provides a simulated modal AO system for testing and development.
    """
    config : aoSimConfig

    def setup(self):
        """Initialize the AO simulator application.
        
        Sets up shared memory images for DM, WFS, and disturbance, initializes
        simulation state variables, and configures the control loop parameters.
        """

        self._nmodes = self.config.num_modes

        # Make this configurable in the future
        create_if_not_exist_shmim('aoSim_dm', [self._nmodes, 1], dtype=np.float32)
        self._dm = Image('aoSim_dm')

        create_if_not_exist_shmim('aoSim_wfs', [self._nmodes, 1], dtype=np.float32)
        self._wfs = Image('aoSim_wfs')

        create_if_not_exist_shmim('aoSim_disturbance', [self._nmodes, 1], dtype=np.float32)
        self._disturbance = Image('aoSim_disturbance')

        self._t = np.array([0], dtype=np.float32)
        self._dt = np.array([0.01], dtype=np.float32)
        self._current_disturbance = np.zeros((self._nmodes, 1), dtype=np.float32)
        self._current_dm_state = np.zeros((self._nmodes, 1), dtype=np.float32)

        self._lag = self.config.lag
        self._dm_command_history = np.zeros((self._lag + 1, self._nmodes, 1), dtype=np.float32)

        self.update_dm()

        self.log.info(f'aoSim app is fully setup.')

    def update_dm(self):
        """Update the deformable mirror state from its command history.
        """
        self._current_dm_state = self._dm_command_history[0]
        self._dm_command_history = np.roll(self._dm_command_history, shift=-1, axis=0)
        self._dm_command_history[-1,:,0] = self._dm.get_data(wait=False)
        
    def update_wfs(self):
        """Update the wavefront sensor measurement in shared memory.
        """
        self.err = self._current_disturbance + self._current_dm_state
        self._wfs.write(self.err)

    def update_disturbance(self):
        """Update the atmospheric/system disturbance for this timestep.
        """
        self._current_disturbance = 0.1 * np.sin(2 * np.pi * 0.5 * self._t) * np.ones((self._nmodes, 1), dtype=np.float32)
        self._disturbance.write(self._current_disturbance)

    def loop(self):
        """Execute one iteration of the AO simulation loop.
        """
        self.update_dm()
        self.update_disturbance()
        self.update_wfs()
        
        print('t={:.2f}, disturbance={:.3f}, dm_state={:.3f}, wfs={:.3f}'.format(self._t[0], self._current_disturbance[0,0], self._current_dm_state[0,0], self.err[0,0]))
        self._t += self._dt

main = aoSim.console_app
