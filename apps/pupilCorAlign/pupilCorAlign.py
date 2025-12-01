import sys
import logging
from enum import Enum
import time
import numpy as np
import datetime
import os 
import hcipy as hp

import xconf

from magaox.indi.device import XDevice, BaseConfig
from magaox.camera import XCam
from magaox.deformable_mirror import XDeformableMirror
from magaox.constants import StateCodes
from magaox.state_manager import XStateMachine

from purepyindi2 import device, properties, constants
from purepyindi2.messages import DefNumber, DefSwitch, DefLight, DefText

### Make the save directory
def make_savedir_for_today(start_path='./Data/'):
    ct = datetime.datetime.now()
    savedir = start_path + '{:>04d}{:>02d}{:>02d}/'.format(ct.year, ct.month, ct.day)
    make_savedir(savedir)
    return savedir

def make_savedir(new_path):
    try:
        os.makedirs(new_path, exist_ok=True)
    except FileExistsError:
        print("Directory already exists.")

def make_time_stamp():
    t = time.time()
    t_seconds = int(t)
    ms = int(1000 * (t - t_seconds))
    stamp = '{:d}{:d}'.format(t_seconds, ms)
    return stamp

def wait_for_ready(client, device, timeout=0.1):
    time.sleep(timeout)
    while client[device + '.fsm.state'] != 'READY':
        time.sleep(timeout)

def make_horizontal_probe(grid, direction='left'):
    probe = grid.zeros()
    probe = probe.shaped
    
    width = 4
    height = 4
    x = (-1.0)**(np.arange(width) - width//2)
    y = (-1.0)**(np.arange(height) - height//2)
    
    # 34 - 15 16 17 18 19 20
    probe[15:19, 6:10] = np.outer(x, y)
    probe[15:19, 6:10] = np.outer(y, x)
    if direction == 'right':
        probe = probe[:,::-1]
    
    return probe.ravel()

def make_vertical_probe(grid, direction='down'):
    probe = grid.zeros()
    probe = probe.shaped
    
    width = 4
    height = 4
    x = (-1.0)**(np.arange(width) - width//2)
    y = (-1.0)**(np.arange(height) - height//2)

    # 34 - 15 16 17 18 19 20
    probe[8:12, 15:19] = np.outer(y, x)
    probe[8:12, 15:19] = np.outer(x, y)
    probe = np.sign(probe)
    if direction == 'up':
        probe = probe[::-1,:]
    
    return probe.ravel()

def make_ncpc_alignment_poke_pattern():
    ncpc_act_grid = hp.make_pupil_grid(34, 34/30.0 * np.array([1.0, np.sqrt(2)]))
    probe = make_vertical_probe(ncpc_act_grid) + make_vertical_probe(ncpc_act_grid, 'up')
    probe += make_horizontal_probe(ncpc_act_grid) + make_horizontal_probe(ncpc_act_grid, 'right')
    return probe


def wait_for_state(client, indi_property, value, wait_time=1, tolerance=None):
    if tolerance is None:
        while not (client[indi_property] == value):
            print("Waiting...")
            time.sleep(wait_time)
    else:
        while not abs(client[indi_property] - value) < tolerance:
            print("Waiting...")
            time.sleep(wait_time)

def calibrate_indi_device(client, device_name, device_property, delta_pertubation, camera, measurement_function, num_stack=50, do_wait=False):
    client.get_properties('{:s}'.format(device_name))
    
    property_current = '{:s}.{:s}.current'.format(device_name, device_property)
    property_target = '{:s}.{:s}.target'.format(device_name, device_property)
    current_position = client[property_current]
    print("Probe to {:f} +- {:f}".format(current_position, delta_pertubation))
    slope = 0
    for s in [-1, 1]:
        client[property_target] = current_position + s * delta_pertubation
        if do_wait:
            wait_for_state(client, property_current, current_position + s * delta_pertubation, tolerance=0.1 * delta_pertubation)
        else:
            time.sleep(5)

        # Take measurement
        camera.grab_stack(3)
        im = camera.grab_stack(num_stack)
       
        measurement = measurement_function(im)
        slope += s * measurement / (2 * delta_pertubation)

    client[property_target] = current_position
    if do_wait:
        wait_for_state(client, property_current, current_position, tolerance=0.1 * delta_pertubation)
    else:
        time.sleep(2)
    
    return slope

class XCorrShift():
    def __init__(self, reference_image, domain_pixels=480, domain_size=40, filter_size=None):
        self._reference_image = reference_image
        self._xgrid = hp.make_pupil_grid(domain_pixels, domain_size)
        
        self._fft = hp.FastFourierTransform(self._reference_image.grid)
        self._mft = hp.MatrixFourierTransform(self._xgrid, self._fft.output_grid)
        
        self._filter_size = filter_size
        if filter_size is not None:
            # Change this to a super gaussian filter to remove ringing.
            self._spatial_filter = hp.make_circular_aperture(self._filter_size)(self._fft.output_grid)
        else:
            self._spatial_filter = 1

        self._kernel = np.conj(self._fft.forward(self._reference_image + 0j))

    def cross_correlate(self, image):
        xcorr = np.real(self._mft.backward(self._fft.forward(image + 0j) * self._spatial_filter * self._kernel))
        return xcorr
        
    def measure(self, image):           
        # Do a cross-correlation and find the peak pixel
        # TODO: implement sub-pixel precision with polynomial fitting
        xcorr = self.cross_correlate(image)
        indx_max = np.argmax(xcorr)
        return self._xgrid.points[indx_max]

@xconf.config
class CameraConfig:
    """
    """
    shmim : str = xconf.field(help="Name of the camera device (specifically, the associated shmim, if different).")
    dark_shmim : str = xconf.field(help="Name of the dark frame shmim associated with this camera device.")
    stagepos : float = xconf.field(help="Stage position for pupil control.")

@xconf.config
class CalibrationConfig:
    """
    """
    path : str = xconf.field(help="Path to the calibration files for the pupil alignment.")
    fwpupil_modes : list[str] = xconf.field(help="List of calibrated fwpupil masks.")
    fwlyot_modes : list[str] = xconf.field(help="List of calibrated fwlyot masks.")

@xconf.config
class DMConfig:
    """
    """
    shmim : str = xconf.field(help="Name of the DM device (specifically, the associate shmim, if different).")
    channel : int = xconf.field(help="The DM channel number.")

@xconf.config
class pupilCorAlignConfig(BaseConfig):
    """Automatic coronagraph alignment assistant
    """
    camera : CameraConfig = xconf.field(help="Camera to use")
    dm : DMConfig = xconf.field(help="DM to use")
    calibration : CalibrationConfig = xconf.field(help='Calibration parameters')

class States(Enum):
    IDLE = 0
    FWPUPIL = 1
    FWLYOT = 2
    CENTROID = 3
    FWPUPILREF = 4
    FWLYOTREF = 5
    CENTROIDREF = 6

class pupilCorAlign(XDevice):
    config : pupilCorAlignConfig
    
    def setup(self):
        self.log.debug(f"I was configured! See? {self.config=}")

        self.log.info("Found camera: {:s}".format(self.config.camera.shmim))
        self.camera = XCam(self.config.camera.shmim, use_hcipy=True)
        
        self.ncpc_act_grid = hp.make_pupil_grid(34, 34/30.0 * np.array([1.0, np.sqrt(2)]))
        self.ncpc_dm = XDeformableMirror(dm=self.config.dm.shmim, channel=self.config.dm.channel)

        self.client.get_properties('fwscind')
        self.client.get_properties('fwpupil')
        self.client.get_properties('fwfpm')
        self.client.get_properties('fwpupil')
        self.client.get_properties('fwlyot')
        self.client.get_properties('camsci1')
        self.client.get_properties('camsci1-dark')
        self.client.get_properties('stagesci1')
        self.client.get_properties('picomotors')
        self.client.get_properties('ttmperi')

        fsm = properties.TextVector(name='fsm')
        fsm.add_element(DefText(name='state', _value=StateCodes.INITIALIZED.name))
        self.add_property(fsm)

        self._state_names = ['idle', 'fwpupil', 'fwlyot', 'centroid', 'fwpupilRef', 'fwlyotRef', 'centroidRef']
        self._state_callbacks = [None, self.handle_fwpupil, self.handle_fwlyot, self.handle_centroid, self.handle_fwpupil_ref, self.handle_fwlyot_ref, self.handle_centroid_ref]
        self._state_machine = XStateMachine(self, self._state_names, States, self._state_callbacks)
        
        # Should I make these files configurable?
        self.pupil_reference = hp.read_field(self.config.calibration.path + "reference_pupil_image.fits")
        self.xcorr_pupil = XCorrShift(self.pupil_reference, 1001, 101, filter_size=1)

        self.actuator_reference = hp.read_field(self.config.calibration.path + "actuator_reference_image.fits")
        self.xcorr_actuators = XCorrShift(self.actuator_reference, 1001, 101, filter_size=1)

        self.fwpupil_references = [hp.read_field(self.config.calibration.path + "reference_{:s}_image.fits".format(mode.replace('-', '_'))) for mode in self.config.calibration.fwpupil_modes] 
        self.xcorr_fwpupil = [XCorrShift(im, 1001, 101, filter_size=1) for im in self.fwpupil_references]

        self.fwlyot_references = [hp.read_field(self.config.calibration.path + "reference_{:s}_image.fits".format(mode.replace('-', '_'))) for mode in self.config.calibration.fwlyot_modes]
        self.xcorr_fwlyot = [XCorrShift(im, 1001, 101, filter_size=1) for im in self.fwlyot_references]

        # Load all the calibration files
        self.actuators_shift = np.array([0.0, 0.0])
        self.actuator_probe_pattern = make_ncpc_alignment_poke_pattern()

        self.pupil_targets = ['ttmperi.axis1_voltage', 'ttmperi.axis2_voltage']
        self.fwpupil_targets = ['picomotors.picopupil_pos', 'fwpupil.filter']
        self.fwlyot_targets = ['picomotors.picolyot_pos', 'fwlyot.filter']

        ttmperi_response_matrix = np.loadtxt(self.config.calibration.path + 'ttmperi_pupil_response_matrix.txt')
        fwpupil_response_matrix = np.loadtxt(self.config.calibration.path + 'fwpupil_picopupil_bumpmask_response_matrix.txt')[:,::-1]
        fwlyot_response_matrix = np.loadtxt(self.config.calibration.path + 'fwlyot_picolyot_lyot_response_matrix.txt')[:,::-1]

        #
        #current_position_picolyot = self.client['picomotors.picolyot_pos.current']
        #current_position_picopupil = self.client['picomotors.picolyot_pos.current']

        self.ttm_reconstruction_matrix = np.linalg.pinv(ttmperi_response_matrix)
        self.fwpupil_reconstruction_matrix = np.linalg.pinv(fwpupil_response_matrix)
        self.fwlyot_reconstruction_matrix = np.linalg.pinv(fwlyot_response_matrix)

        # These also need to be exposed to indi
        self.num_stack = 1
        self.gain = 0.25
        self.pattern_repeat = 1
        self.amp = 0.25
        self.sleep_time = 2.0

        nv = properties.NumberVector(name='nstack')
        nv.add_element(DefNumber(
            name='current', label='Number of frames', format='%i',
            min=1, max=150, step=1, _value=1
        ))
        nv.add_element(DefNumber(
            name='target', label='Number of frames', format='%i',
            min=1, max=150, step=1, _value=1
        ))
        self.add_property(nv, callback=self.handle_nstack)

        nv = properties.NumberVector(name='gain')
        nv.add_element(DefNumber(
            name='current', label='Loop Gain', format='%.2f',
            min=0.00, max=1.00, step=0.01, _value=0.10
        ))
        nv.add_element(DefNumber(
            name='target', label='Loop Gain', format='%.2f',
            min=0.00, max=1.00, step=0.01, _value=0.10
        ))
        self.add_property(nv, callback=self.handle_gain)

        # These need to be exposed to INDI
        self.fwpupil_error = np.array([0., 0.])
        self.fwlyot_error = np.array([0., 0.])

        nv = properties.NumberVector(name='fwpupil')
        nv.add_element(DefNumber( #first element
            name='dx', label='dx', format='%.4f',
            min=-50.00, max=50.00, step=0.0001, _value=0.21178766
        ))
        nv.add_element(DefNumber( 
            name='dy', label='dy', format='%.4f',
            min=-50.00, max=50.00, step=0.0001, _value=0.19275196 
        ))
        self.add_property(nv, callback=self.handle_fwpupil_error) 

        nv = properties.NumberVector(name='fwlyot')
        nv.add_element(DefNumber( #first element
            name='dx', label='dx', format='%.4f',
            min=-50.00, max=50.00, step=0.0001, _value=0.21178766
        ))
        nv.add_element(DefNumber( 
            name='dy', label='dy', format='%.4f',
            min=-50.00, max=50.00, step=0.0001, _value=0.19275196 
        ))
        self.add_property(nv, callback=self.handle_fwlyot_error)

        self.taken_reference = False
        self.log.info(f'pupilCorAlign app is fully setup.')

    def stages_are_ready(self):
        '''
        '''
        if client['fwpupil.fsm.state'] != 'READY':
            return False
        elif client['fwlyot.fsm.state'] != 'READY':
            return False
        elif client['picomotors.fsm.state'] != 'READY':
            return False
        return True

    def control_position(self, shift, indi_targets, reconstruction_matrix):
        '''
        '''
        if self.stages_are_ready():
            current_positions = [self.client['{:s}.current'.format(indi_targets[i])] for i in [0, 1]]
            cmd = reconstruction_matrix.dot(shift)        
            self.client['{:s}.target'.format(indi_targets[0])] = current_positions[0] - self.gain * cmd[0]
            self.client['{:s}.target'.format(indi_targets[1])] = current_positions[1] - self.gain * cmd[1]
            time.sleep(self.sleep_time)

    def measure_position(self, correlator):
        '''
        '''
        im = self.camera.grab_stack(self.num_stack)
        shift = correlator.measure(im) - self.actuators_shift
        return shift

    def measure_actuator(self):
        '''
        '''
        actuator_im = 0.
        for i in range(self.pattern_repeat):
            for s in [-1, 1]:
                self.ncpc_dm.actuators += s * self.amp * self.actuator_probe_pattern
                self.ncpc_dm.send(0.05)
                
                self.camera.grab_stack(2)
                actuator_im += s * self.camera.grab_stack(self.num_stack)

                self.ncpc_dm.actuators -= s * self.amp * self.actuator_probe_pattern
        self.ncpc_dm.send()

        return actuator_im

    def get_current_state(self, indi_property):
        '''
        '''
        for name in self.client[indi_property]:
            if self.client[indi_property + '.' + name] == constants.SwitchState.ON:
                return name
   
    def handle_fwpupil(self):
        '''
        '''
        for mi, m in enumerate(self.config.calibration.fwpupil_modes):
            # Check if we are in the correct position
            if self.client['fwpupil.filterName.{:s}'.format(m)] == constants.SwitchState.ON:
                self.fwpupil_error = self.measure_position(self.xcorr_fwpupil[mi])
                if self.taken_reference:
                    self.control_position(self.fwpupil_error, self.fwpupil_targets, self.fwpupil_reconstruction_matrix)
                else:
                    self.log.info(f'No reference taken. Take centroid reference first.')

    def handle_fwlyot(self):
        '''
        '''
        for mi, m in enumerate(self.config.calibration.fwlyot_modes):
            # Check if we are in the correct position
            if self.client['fwlyot.filterName.{:s}'.format(m)] == constants.SwitchState.ON:
                self.fwlyot_error = self.measure_position(self.xcorr_fwlyot[mi])
                if self.taken_reference:
                    self.control_position(self.fwlyot_error, self.fwlyot_targets, self.fwlyot_reconstruction_matrix)
                else:
                    self.log.info(f'No reference taken. Take centroid reference first.')

    def handle_centroid(self):
        '''
        '''
        actuator_im = self.measure_actuator()
        self.actuators_shift = self.xcorr_actuators.measure(actuator_im)
        self.taken_reference = True
        self._state_machine.transition_to_idle()

    def handle_fwpupil_ref(self):
        '''
        '''
        im = self.camera.grab_stack(self.num_stack)
        current_name = self.get_current_state('fwpupil.filterName')
        filename = "reference_{:s}_image.fits".format(current_name.replace('-', '_'))
        write_field(im, self.config.calibration.path + filename)

    def handle_fwlyot_ref(self):
        '''
        '''
        im = self.camera.grab_stack(self.num_stack)
        current_name = self.get_current_state('fwlyot.filterName')
        filename = "reference_{:s}_image.fits".format(current_name.replace('-', '_'))
        write_field(im, self.config.calibration.path + filename)

    def handle_centroid_ref(self):
        '''
        '''
        actuator_im = self.measure_actuator()
        filename = "actuator_reference_image.fits"
        write_field(im, self.config.calibration.path + filename)

    def loop(self):
        '''
        '''
        self._state_machine.loop()
        self.update_properties()

    def update_properties(self):
        '''
        '''
        self.properties['fwpupil']['dx'] = float(self.fwpupil_error[0])
        self.properties['fwpupil']['dy'] = float(self.fwpupil_error[1])
        self.update_property(self.properties['fwpupil'])
        
        self.properties['fwlyot']['dx'] = float(self.fwlyot_error[0])
        self.properties['fwlyot']['dy'] = float(self.fwlyot_error[1])
        self.update_property(self.properties['fwlyot'])

    def handle_nstack(self, existing_property, new_message):
        '''
        '''
        if 'target' in new_message and new_message['target'] != existing_property['current']: 
            existing_property['current'] = new_message['target']
            existing_property['target'] = new_message['target']
            self.num_stack = int(new_message['target'])
            self.log.debug(f'now averaging over {self.self.num_stack} frames')
        self.update_property(existing_property)

    def handle_gain(self, existing_property, new_message):
        '''
        '''
        if 'target' in new_message and new_message['target'] != existing_property['current']: 
            existing_property['current'] = new_message['target']
            existing_property['target'] = new_message['target']
            self.gain = float(new_message['target'])
            self.log.debug(f'new feedback gain {self.gain} ')
        self.update_property(existing_property)

    # Ask joseph for a nicer way to deal with this
    def handle_fwpupil_error(self, existing_property, new_message):
        pass

    def handle_fwlyot_error(self, existing_property, new_message):
        pass