import sys
import logging
from enum import Enum
import time
import numpy as np

import xconf

from magaox.indi.device import XDevice, BaseConfig
from magaox.camera import XCam
from magaox.constants import StateCodes

from purepyindi2 import device, properties, constants
from purepyindi2.messages import DefNumber, DefSwitch, DefLight, DefText

import hcipy as hp
from scipy.optimize import minimize

from utils import *

@xconf.config
class CameraConfig:
    """
    """
    shmim : str = xconf.field(help="Name of the camera device (specifically, the associated shmim, if different)")
    dark_shmim : str = xconf.field(help="Name of the dark frame shmim associated with this camera device")
    #TODO: Does camtip have a dark frame?

@xconf.config
class camtipSRConfig(BaseConfig):
    """ Active ADC control 
    """
    camera : CameraConfig = xconf.field(help="Camera to use")
    sleep_interval_sec : float = xconf.field(default=0.25, help="Sleep interval between loop() calls")

class States(Enum):
    IDLE = 0
    CLOSED_LOOP = 1         
    ONESHOT = 2 

class camtipSR(XDevice):
    config: camtipSRConfig

    def setup(self):
        # Finite State Machine (FSM) current state (I think)
        fsm = properties.TextVector(name='fsm')
        fsm.add_element(DefText(name='state', _value=StateCodes.INITIALIZED.name))
        self.add_property(fsm)
        
        # init INDI PROPERTY: loop state
        sv = properties.SwitchVector(
            name='state',
            rule=constants.SwitchRule.ONE_OF_MANY,
            perm=constants.PropertyPerm.READ_WRITE,
        )
        sv.add_element(DefSwitch(name="idle", _value=constants.SwitchState.ON))
        sv.add_element(DefSwitch(name="SRLoop", _value=constants.SwitchState.OFF)) 
        sv.add_element(DefSwitch(name="oneshot", _value=constants.SwitchState.OFF)) 
        self.add_property(sv, callback=self.handle_state) 
        
        # init INDI PROPERTY: n_avg
        nv = properties.NumberVector(name='n_avg')
        nv.add_element(DefNumber(
            name='current', label='Number of frames', format='%i',
            min=1, max=150, step=1, _value=1
        ))
        nv.add_element(DefNumber(
            name='target', label='Number of frames', format='%i',
            min=1, max=150, step=1, _value=1
        ))
        self.add_property(nv, callback=self.handle_n_avg)

        # init INDI PROPERTY: SR_est
        # a property that updates every recaculation, can't be set
        nv = properties.NumberVector(name='SR_est')
        nv.add_element(DefNumber(
            name='current', label='SR_est', format='%i',
            min=0, max=1, step=0.01, _value=1
        ))
        self.add_property(nv)

        # get INDI PROPERTIES
        #TODO: get modulator status
        self.client.get_properties_and_wait(['modwfs', 'fxngenmodwfs', 'fxngensync'])
        self.client['modwfs.modRadius.current']

        # find the camera
        self.log.info("Found camera: {:s}".format(self.config.camera.shmim))
        self.camera = XCam(
            self.config.camera.shmim,
            pixel_size=6.0/21.0, #TODO: I need to redo that plate scale methinks
            use_hcipy=True,
            indi_client=self.client,
        )

        # Starting values for states
        self._state = States.IDLE
        self._n_avg = 1
        self._SR_est = 0.0 

        #SETUP: for the camtip fitter
        self.camFit = camtipFitter()

        #SETTING UP LAB
        # TODO: get some calibration working
        # self.camFit.fitLab(lab_path)
        # I'm gonna hardcode this and learn a lesson -> not sure best way to do this
        # this only works when they're running at mod 3
        lab_fit = [6.776e+00, 3.333e+01, 2.870e+00, -4.846e-01, 4.571e+00]
        self.camFit.set_lab(lab_fit) #TODO: get the lab_fit from some kind of conf?

        self.properties['fsm']['state'] = StateCodes.READY.name
        self.update_property(self.properties['fsm'])

    def handle_state(self, existing_property, new_message):        
        target_list = ['idle', 'SRLoop', 'oneshot']
        for key in target_list: 
            if existing_property[key] == constants.SwitchState.ON: 
                current_state = key
        
        if current_state not in new_message: 

            for key in target_list:
                existing_property[key] = constants.SwitchState.OFF 
                if key in new_message: 
                    existing_property[key] = new_message[key] 

                    if key == 'idle': 
                        self._state = States.IDLE
                        self.properties['fsm']['state'] = StateCodes.READY.name
                        self._command = 0
                        self.log.debug('State changed to idle')                    
                    elif key == 'SRLoop':
                        self._state = States.CLOSED_LOOP
                        self.update_wavelength()
                        self.properties['fsm']['state'] = StateCodes.OPERATING.name
                        self.log.debug('State changed to continuous')
                    elif key == 'oneshot':
                        self._state = States.ONESHOT
                        self.update_wavelength()
                        self.properties['fsm']['state'] = StateCodes.OPERATING.name
                        self.log.debug('State changed to oneshot')

            self.update_property(existing_property)
            self.update_property(self.properties['fsm'])

    def handle_n_avg(self, existing_property, new_message):
        if 'target' in new_message and new_message['target'] != existing_property['current']: 
            existing_property['current'] = new_message['target']
            existing_property['target'] = new_message['target']
            self._n_avg = int(new_message['target'])
        self.update_property(existing_property)
    
    def transition_to_idle(self):
        self._command = 0
        self.properties['state']['oneshot'] = constants.SwitchState.OFF
        self.properties['state']['SRLoop'] = constants.SwitchState.OFF
        self.properties['state']['idle'] = constants.SwitchState.ON
        self.update_property(self.properties['state'])
        self._state = States.IDLE    

    def fit_one_img(self, img):
        # size check: will only work if image is desired size
        if img.shape != (128, 128): # TODO: check if this works
            self.log.error(f"Image size is {img.shape}, expected (128, 128), set proper ROI")
            self.transition_to_idle()

        # TODO: check if the frame looks bad...
        self.log.info(f'image intensity:{np.sum(img):.2f}')
        self.log.info(f'min intensity: {np.min(img):.2f}')
        self.log.info(f'max intensity: {np.max(img):.2f}')

        # Set the data in the camtipFitter
        self.camFit.set_data(img) # background subtracted here

        # Fit the data
        self.fit_data()

        # Calculate the SR
        self._SR_est = self.camFit.calc_SR()

        # set the SR
        self.properties['SR_est']['current'] = self._SR_est
        
        return


    def loop(self):
        if self._state == States.CLOSED_LOOP:
            # grab stack gives average of that stack 
            img = self.camera.grab_stack(self._n_avg) # stack is already averaged
            transpose = img.shaped.T # vs. how it would be in a jupyter notebook
            #TODO: check to see if this is the expected orientation (matters for BG subtraction)
            
            self.fit_one_img(transpose)
            # will then continue to loop

        elif self._state == States.ONESHOT:
            img = self.camera.grab_stack(self._n_avg)
            transpose = img.shaped.T 

            ## CALL FIT FUNCTION
            self.fit_one_img(transpose)

            # will now exit out
            self.transition_to_idle()






