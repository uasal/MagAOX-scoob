import numpy as np
import hcipy as hp
import scipy
from scipy.optimize import minimize
import time

# TODO: should I throw this in utils?
class camtipFitter:
    #Is this bad form? they won't change
    D = 6.5 # m
    px_scale = 2.2 # mas / px
    wavelength = 800e-9 # m
    conv_rad_to_mas = 206264806 # rad / mas
    """ This class holds the variable necessary to establish a fit to a camtip image """
    def __init__(self, mod_r=3.0):
        self.mod_r = mod_r
        self.mod_r_px = self.calc_R_to_px(mod_r)
        self.lab_fit = None  # could use to compare against above value
        self.data = None
        self.data_bg_sub = None
        self.data_fit = None
        # I hardcode size before passing in data
        self.size = 128
        self.grid = hp.make_uniform_grid((self.size, self.size), (self.size,self.size)) # used in fitting

    def calc_R_to_px(self, R):
        # if god was kind, this would give the radius we expect
        # except life is cruel, and pixel scales are sketchy, so this is not 100%
        rads_to_mas = self.D / self.wavelength * self.conv_rad_to_mas
        return R * rads_to_mas / self.px_scale

    def set_data(self, data):
        ''' data - frame from the camera
        Subtracts first 100 pixels from the data...'''
        self.data = data
        self.data_bg_sub = sub_bg_img(data)

    def clear(self):
        self.data = None

    def set_lab(self, lab_fit):
        '''Take a lab fit to compare for with SR'''
        self.lab_fit = lab_fit # ideally would have radius
        self.lab_sig = lab_fit[0] 
        self.lab_amp = lab_fit[1]
        self.lab_rad = lab_fit[2]

    # TODO: could I set a lab set of information here?
    def fitLab(self, lab_file):
        # TODO: need to decide best way to get lab file in
        return
    
    def fit_data(self):
        # partial fit
        data_fit = fit_img_gauss(self.data_bg_sub, self.lab_rad, self.grid)
        self.data_fit = data_fit

    def calc_SR(self):
        """
        Calculate the SR, requires lab fits and sky fits
        """
        lab_sigma = self.lab_sig
        sky_sigma = self.data_fit[0]
        self.SR = lab_sigma / sky_sigma

        # check quality of this fit, if invalid, not going to list. 
        # if self.SR > 1 or self.SR < 0:
            #self.SR = 0

        return self.SR
    
    def calc_SR_dumb(self, m=64):
        img = self.data_bg_sub
        avg_max = np.average([np.max(img[m,:m]), np.max(img[m,m:]), np.max(img[:m,m]), np.max(img[m:,m])])
        normed_peak = avg_max / np.sum(img)
        self.SR_dumb = normed_peak
        return normed_peak
    
####### Helper funcitons ########

# specifically for smaller images
def sub_bg_img(img):
    bg_mask = np.zeros_like(img)
    bg_mask[:, 0:5] = 1
    bg_mask[:,-5:] = 1    
    # average ans subtract out background
    bg_column = np.sum(img * bg_mask, axis=1, keepdims=True) / 10
    # subtract of the bg:
    return img - bg_column

# "LAB" fits are full, radius also uknown

def make_fit_func(img, grid):
    """
    Returns a fitting function
    Based on the Gaussian rotation. 
    """
    def fitting_func(theta):
        # creating the model
        rad_gaus = make_rad_gaus_model(theta, grid)
        # returning the cost function of the difference of our model vs the guess
        return np.sum((img.ravel() - rad_gaus)**2)
    return fitting_func

def make_rad_gaus_model(theta, grid):
    # theta is a 1D array of parameters
    sig = theta[0] #5
    mod_rad = theta[1] #32.5
    amp = theta[2] #1
    x0 = theta[3] #0
    y0 = theta[4] #1
    # Create the shifted grid 
    R = grid.shifted([-x0, -y0]).as_("polar").r
    # creating the 
    rad_gaus = hp.Field(amp*np.exp(-(R.ravel()-mod_rad)**2/sig**2), grid)
    return rad_gaus

def lab_fit_params(data, grid):
    """
    Lab fits radius in addition to sig, amp, x0, y0
    """
    # initial guess
    theta0 = [5, 32, 1, 0, 0]
    # seeding the funciton with lab image
    rad_gaus_fn = make_fit_func(data, grid)
    # optimizing the fit
    theta_opt = scipy.optimize.minimize(rad_gaus_fn, theta0)
    #creating an image from an optimized fit
    fit_img = make_rad_gaus_model(theta_opt['x'], grid)
    return theta_opt

# "SKY" fits are partial, radius is known

def make_fit_func_sky(img, mod_rad, grid):
    """ Sky fits use precalculated mod_radius"""
    def fitting_func(phi):
        # creating the model
        rad_gaus = make_rad_gaus_model_sky(phi, mod_rad, grid)
        # returning the cost function of the difference of our model vs the guess
        # COST FUNCTION IS L2 NORM
        return np.sum((img.ravel() - rad_gaus)**2)
    return fitting_func

def make_rad_gaus_model_sky(theta, mod_rad, grid):
    # theta is a 1D array of parameters
    sig = theta[0] #5
    amp = theta[1]
    x0 = theta[2]
    y0 = theta[3]
    # Create the shifted grid 
    R = grid.shifted([-x0, -y0]).as_("polar").r
    # creating the 
    rad_gaus = hp.Field(amp*np.exp(-(R.ravel()-mod_rad)**2/sig**2), grid)
    return rad_gaus

def fit_img_gauss(img, lab_R, grid):
    # sig, amp, x, y
    img_sub = sub_bg_img(img)
    # avoiding hotpixels
    max_pix = np.quantile(img_sub, 0.98) 
    # INITIAL GUESS
    phi0 = [5, max_pix, 0, 0]
    # create the sky fitting function
    rad_gaus_fn = make_fit_func_sky(img_sub, lab_R, grid) 
    # optimize parametets
    theta_opt = scipy.optimize.minimize(rad_gaus_fn, phi0)
    # just taking parameters, could also take jacobian later 
    return theta_opt['x']