import sys
import logging
from enum import Enum
import time
import numpy as np

import xconf

from magaox.indi.device import XDevice, BaseConfig
from magaox.camera import XCam
from magaox.constants import StateCodes
from magaox.state_manager import XStateMachine

from purepyindi2 import device, properties, constants
from purepyindi2.messages import DefNumber, DefSwitch, DefLight, DefText

@xconf.config
class CameraConfig:
    """
    """
    shmim : str = xconf.field(help="Name of the camera device (specifically, the associated shmim, if different)")
    dark_shmim : str = xconf.field(help="Name of the dark frame shmim associated with this camera device")

@xconf.config
class CalibrationConfig:
    """
    """
    calibration_path : str = xconf.field(help="Path to the calibration files for the pupil alignment.")

@xconf.config
class pupilCorAlignConfig(BaseConfig):
    """Automatic coronagraph alignment assistant
    """
    camera : CameraConfig = xconf.field(help="Camera to use")
    calibration : CalibrationConfig = xconf.field(help='Calibration parameters')

class States(Enum):
    IDLE = 0
    CLOSED_LOOP = 1
    PUPIL_REF = 2
    CENTROID_REF = 3

class PupilState(Enum):
    IDLE = 0        # This is a safety state for enthousiastic button clickers
    ACTUATOR = 1
    BUMPMASK = 2
    LYOTSTOP = 3

class pupilCorAlign(XDevice):
    config : pupilCorAlignConfig
    
    def setup(self):
        self.log.debug(f"I was configured! See? {self.config=}")
        fsm = properties.TextVector(name='fsm')
        fsm.add_element(DefText(name='state', _value=StateCodes.INITIALIZED.name))
        self.add_property(fsm)

        self.log.info("Found camera: {:s}".format(self.config.camera.shmim))
        self.camera = XCam(self.config.camera.shmim, use_hcipy=True)
        self._state = States.IDLE

        fsm = properties.TextVector(name='fsm')
        fsm.add_element(DefText(name='state', _value=StateCodes.INITIALIZED.name))
        self.add_property(fsm)

        self._state_names = ['idle', 'closedLoop', 'pupilRef', 'centroidRef']
        self._state_callbacks = [self.handle_idle, self.handle_closed_loop, self.handle_pupil_ref, self.handle_centroid_ref]
        self._state_machine = XStateMachine(self, self._state_names, States, self._state_callbacks)

        # This select what pupil plane we are aligning to
        self._pupil_state = PupilState.IDLE
    
    def handle_idle(self):
        print("handle_idle")
    
    def handle_closed_loop(self):
        print("handle_closed_loop")

    def handle_pupil_ref(self):
        print("handle_pupil_ref")

    def handle_centroid_ref(self):
        print("handle_centroid_ref")

    def loop(self):
        self._state_machine.loop()
