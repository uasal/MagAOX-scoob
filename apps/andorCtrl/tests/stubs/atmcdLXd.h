/** \file atmcdLXd.h
 * \brief Test stub for the Andor SDK2 (Linux) header, declaring only what andorCtrl uses.
 * \author Claude Code
 *
 * The `DRV_*` return codes use the values of the real SDK so that the `andorSDKErrorName()` switch in
 * andorCtrl has distinct case labels.  The function bodies, and the fake camera state they act on, are
 * defined in `andorCtrl_test.cpp`.
 *
 * \ingroup andorCtrl_files
 */

#ifndef andorCtrl_tests_stubs_atmcdLXd_h
#define andorCtrl_tests_stubs_atmcdLXd_h

/// Andor SDK 32-bit integer type (int on 64-bit Linux).
typedef int at_32;

/// Maximum path length used for string buffers, as in the real SDK.
#define MAX_PATH 256

/// Andor SDK return code DRV_ERROR_CODES.
#define DRV_ERROR_CODES 20001

/// Andor SDK return code DRV_SUCCESS.
#define DRV_SUCCESS 20002

/// Andor SDK return code DRV_VXDNOTINSTALLED.
#define DRV_VXDNOTINSTALLED 20003

/// Andor SDK return code DRV_ERROR_SCAN.
#define DRV_ERROR_SCAN 20004

/// Andor SDK return code DRV_ERROR_CHECK_SUM.
#define DRV_ERROR_CHECK_SUM 20005

/// Andor SDK return code DRV_ERROR_FILELOAD.
#define DRV_ERROR_FILELOAD 20006

/// Andor SDK return code DRV_UNKNOWN_FUNCTION.
#define DRV_UNKNOWN_FUNCTION 20007

/// Andor SDK return code DRV_ERROR_VXD_INIT.
#define DRV_ERROR_VXD_INIT 20008

/// Andor SDK return code DRV_ERROR_ADDRESS.
#define DRV_ERROR_ADDRESS 20009

/// Andor SDK return code DRV_ERROR_PAGELOCK.
#define DRV_ERROR_PAGELOCK 20010

/// Andor SDK return code DRV_ERROR_PAGEUNLOCK.
#define DRV_ERROR_PAGEUNLOCK 20011

/// Andor SDK return code DRV_ERROR_BOARDTEST.
#define DRV_ERROR_BOARDTEST 20012

/// Andor SDK return code DRV_ERROR_ACK.
#define DRV_ERROR_ACK 20013

/// Andor SDK return code DRV_ERROR_UP_FIFO.
#define DRV_ERROR_UP_FIFO 20014

/// Andor SDK return code DRV_ERROR_PATTERN.
#define DRV_ERROR_PATTERN 20015

/// Andor SDK return code DRV_ACQUISITION_ERRORS.
#define DRV_ACQUISITION_ERRORS 20017

/// Andor SDK return code DRV_ACQ_BUFFER.
#define DRV_ACQ_BUFFER 20018

/// Andor SDK return code DRV_ACQ_DOWNFIFO_FULL.
#define DRV_ACQ_DOWNFIFO_FULL 20019

/// Andor SDK return code DRV_PROC_UNKONWN_INSTRUCTION.
#define DRV_PROC_UNKONWN_INSTRUCTION 20020

/// Andor SDK return code DRV_ILLEGAL_OP_CODE.
#define DRV_ILLEGAL_OP_CODE 20021

/// Andor SDK return code DRV_KINETIC_TIME_NOT_MET.
#define DRV_KINETIC_TIME_NOT_MET 20022

/// Andor SDK return code DRV_ACCUM_TIME_NOT_MET.
#define DRV_ACCUM_TIME_NOT_MET 20023

/// Andor SDK return code DRV_NO_NEW_DATA.
#define DRV_NO_NEW_DATA 20024

/// Andor SDK return code DRV_SPOOLERROR.
#define DRV_SPOOLERROR 20026

/// Andor SDK return code DRV_SPOOLSETUPERROR.
#define DRV_SPOOLSETUPERROR 20027

/// Andor SDK return code DRV_FILESIZELIMITERROR.
#define DRV_FILESIZELIMITERROR 20028

/// Andor SDK return code DRV_ERROR_FILESAVE.
#define DRV_ERROR_FILESAVE 20029

/// Andor SDK return code DRV_TEMPERATURE_CODES.
#define DRV_TEMPERATURE_CODES 20033

/// Andor SDK return code DRV_TEMPERATURE_OFF.
#define DRV_TEMPERATURE_OFF 20034

/// Andor SDK return code DRV_TEMPERATURE_NOT_STABILIZED.
#define DRV_TEMPERATURE_NOT_STABILIZED 20035

/// Andor SDK return code DRV_TEMPERATURE_STABILIZED.
#define DRV_TEMPERATURE_STABILIZED 20036

/// Andor SDK return code DRV_TEMPERATURE_NOT_REACHED.
#define DRV_TEMPERATURE_NOT_REACHED 20037

/// Andor SDK return code DRV_TEMPERATURE_OUT_RANGE.
#define DRV_TEMPERATURE_OUT_RANGE 20038

/// Andor SDK return code DRV_TEMPERATURE_NOT_SUPPORTED.
#define DRV_TEMPERATURE_NOT_SUPPORTED 20039

/// Andor SDK return code DRV_TEMPERATURE_DRIFT.
#define DRV_TEMPERATURE_DRIFT 20040

/// Andor SDK return code DRV_GENERAL_ERRORS.
#define DRV_GENERAL_ERRORS 20049

/// Andor SDK return code DRV_INVALID_AUX.
#define DRV_INVALID_AUX 20050

/// Andor SDK return code DRV_COF_NOTLOADED.
#define DRV_COF_NOTLOADED 20051

/// Andor SDK return code DRV_FPGAPROG.
#define DRV_FPGAPROG 20052

/// Andor SDK return code DRV_FLEXERROR.
#define DRV_FLEXERROR 20053

/// Andor SDK return code DRV_GPIBERROR.
#define DRV_GPIBERROR 20054

/// Andor SDK return code DRV_EEPROMVERSIONERROR.
#define DRV_EEPROMVERSIONERROR 20055

/// Andor SDK return code DRV_DATATYPE.
#define DRV_DATATYPE 20064

/// Andor SDK return code DRV_DRIVER_ERRORS.
#define DRV_DRIVER_ERRORS 20065

/// Andor SDK return code DRV_P1INVALID.
#define DRV_P1INVALID 20066

/// Andor SDK return code DRV_P2INVALID.
#define DRV_P2INVALID 20067

/// Andor SDK return code DRV_P3INVALID.
#define DRV_P3INVALID 20068

/// Andor SDK return code DRV_P4INVALID.
#define DRV_P4INVALID 20069

/// Andor SDK return code DRV_INIERROR.
#define DRV_INIERROR 20070

/// Andor SDK return code DRV_COFERROR.
#define DRV_COFERROR 20071

/// Andor SDK return code DRV_ACQUIRING.
#define DRV_ACQUIRING 20072

/// Andor SDK return code DRV_IDLE.
#define DRV_IDLE 20073

/// Andor SDK return code DRV_TEMPCYCLE.
#define DRV_TEMPCYCLE 20074

/// Andor SDK return code DRV_NOT_INITIALIZED.
#define DRV_NOT_INITIALIZED 20075

/// Andor SDK return code DRV_P5INVALID.
#define DRV_P5INVALID 20076

/// Andor SDK return code DRV_P6INVALID.
#define DRV_P6INVALID 20077

/// Andor SDK return code DRV_INVALID_MODE.
#define DRV_INVALID_MODE 20078

/// Andor SDK return code DRV_INVALID_FILTER.
#define DRV_INVALID_FILTER 20079

/// Andor SDK return code DRV_I2CERRORS.
#define DRV_I2CERRORS 20080

/// Andor SDK return code DRV_I2CDEVNOTFOUND.
#define DRV_I2CDEVNOTFOUND 20081

/// Andor SDK return code DRV_I2CTIMEOUT.
#define DRV_I2CTIMEOUT 20082

/// Andor SDK return code DRV_P7INVALID.
#define DRV_P7INVALID 20083

/// Andor SDK return code DRV_P8INVALID.
#define DRV_P8INVALID 20084

/// Andor SDK return code DRV_P9INVALID.
#define DRV_P9INVALID 20085

/// Andor SDK return code DRV_P10INVALID.
#define DRV_P10INVALID 20086

/// Andor SDK return code DRV_P11INVALID.
#define DRV_P11INVALID 20087

/// Andor SDK return code DRV_USBERROR.
#define DRV_USBERROR 20089

/// Andor SDK return code DRV_IOCERROR.
#define DRV_IOCERROR 20090

/// Andor SDK return code DRV_VRMVERSIONERROR.
#define DRV_VRMVERSIONERROR 20091

/// Andor SDK return code DRV_GATESTEPERROR.
#define DRV_GATESTEPERROR 20092

/// Andor SDK return code DRV_USB_INTERRUPT_ENDPOINT_ERROR.
#define DRV_USB_INTERRUPT_ENDPOINT_ERROR 20093

/// Andor SDK return code DRV_RANDOM_TRACK_ERROR.
#define DRV_RANDOM_TRACK_ERROR 20094

/// Andor SDK return code DRV_INVALID_TRIGGER_MODE.
#define DRV_INVALID_TRIGGER_MODE 20095

/// Andor SDK return code DRV_LOAD_FIRMWARE_ERROR.
#define DRV_LOAD_FIRMWARE_ERROR 20096

/// Andor SDK return code DRV_DIVIDE_BY_ZERO_ERROR.
#define DRV_DIVIDE_BY_ZERO_ERROR 20097

/// Andor SDK return code DRV_INVALID_RINGEXPOSURES.
#define DRV_INVALID_RINGEXPOSURES 20098

/// Andor SDK return code DRV_BINNING_ERROR.
#define DRV_BINNING_ERROR 20099

/// Andor SDK return code DRV_INVALID_AMPLIFIER.
#define DRV_INVALID_AMPLIFIER 20100

/// Andor SDK return code DRV_INVALID_COUNTCONVERT_MODE.
#define DRV_INVALID_COUNTCONVERT_MODE 20101

/// Andor SDK return code DRV_USB_INTERRUPT_ENDPOINT_TIMEOUT.
#define DRV_USB_INTERRUPT_ENDPOINT_TIMEOUT 20102

/// Andor SDK return code DRV_ERROR_NOCAMERA.
#define DRV_ERROR_NOCAMERA 20990

/// Andor SDK return code DRV_NOT_SUPPORTED.
#define DRV_NOT_SUPPORTED 20991

/// Andor SDK return code DRV_NOT_AVAILABLE.
#define DRV_NOT_AVAILABLE 20992

/// Andor SDK return code DRV_ERROR_MAP.
#define DRV_ERROR_MAP 20115

/// Andor SDK return code DRV_ERROR_UNMAP.
#define DRV_ERROR_UNMAP 20116

/// Andor SDK return code DRV_ERROR_MDL.
#define DRV_ERROR_MDL 20117

/// Andor SDK return code DRV_ERROR_UNMDL.
#define DRV_ERROR_UNMDL 20118

/// Andor SDK return code DRV_ERROR_BUFFSIZE.
#define DRV_ERROR_BUFFSIZE 20119

/// Andor SDK return code DRV_ERROR_NOHANDLE.
#define DRV_ERROR_NOHANDLE 20121

/// Andor SDK return code DRV_GATING_NOT_AVAILABLE.
#define DRV_GATING_NOT_AVAILABLE 20130

/// Andor SDK return code DRV_FPGA_VOLTAGE_ERROR.
#define DRV_FPGA_VOLTAGE_ERROR 20131

/// Andor SDK return code DRV_OW_CMD_FAIL.
#define DRV_OW_CMD_FAIL 20150

/// Andor SDK return code DRV_OWMEMORY_BAD_ADDR.
#define DRV_OWMEMORY_BAD_ADDR 20151

/// Andor SDK return code DRV_OWCMD_NOT_AVAILABLE.
#define DRV_OWCMD_NOT_AVAILABLE 20152

/// Andor SDK return code DRV_OW_NO_SLAVES.
#define DRV_OW_NO_SLAVES 20153

/// Andor SDK return code DRV_OW_NOT_INITIALIZED.
#define DRV_OW_NOT_INITIALIZED 20154

/// Andor SDK return code DRV_OW_ERROR_SLAVE_NUM.
#define DRV_OW_ERROR_SLAVE_NUM 20155

/// Andor SDK return code DRV_MSTIMINGS_ERROR.
#define DRV_MSTIMINGS_ERROR 20156

/// Andor SDK return code DRV_OA_NULL_ERROR.
#define DRV_OA_NULL_ERROR 20173

/// Andor SDK return code DRV_OA_PARSE_DTD_ERROR.
#define DRV_OA_PARSE_DTD_ERROR 20174

/// Andor SDK return code DRV_OA_DTD_VALIDATE_ERROR.
#define DRV_OA_DTD_VALIDATE_ERROR 20175

/// Andor SDK return code DRV_OA_FILE_ACCESS_ERROR.
#define DRV_OA_FILE_ACCESS_ERROR 20176

/// Andor SDK return code DRV_OA_FILE_DOES_NOT_EXIST.
#define DRV_OA_FILE_DOES_NOT_EXIST 20177

/// Andor SDK return code DRV_OA_XML_INVALID_OR_NOT_FOUND_ERROR.
#define DRV_OA_XML_INVALID_OR_NOT_FOUND_ERROR 20178

/// Andor SDK return code DRV_OA_PRESET_FILE_NOT_LOADED.
#define DRV_OA_PRESET_FILE_NOT_LOADED 20179

/// Andor SDK return code DRV_OA_USER_FILE_NOT_LOADED.
#define DRV_OA_USER_FILE_NOT_LOADED 20180

/// Andor SDK return code DRV_OA_PRESET_AND_USER_FILE_NOT_LOADED.
#define DRV_OA_PRESET_AND_USER_FILE_NOT_LOADED 20181

/// Andor SDK return code DRV_OA_INVALID_FILE.
#define DRV_OA_INVALID_FILE 20182

/// Andor SDK return code DRV_OA_FILE_HAS_BEEN_MODIFIED.
#define DRV_OA_FILE_HAS_BEEN_MODIFIED 20183

/// Andor SDK return code DRV_OA_BUFFER_FULL.
#define DRV_OA_BUFFER_FULL 20184

/// Andor SDK return code DRV_OA_INVALID_STRING_LENGTH.
#define DRV_OA_INVALID_STRING_LENGTH 20185

/// Andor SDK return code DRV_OA_INVALID_CHARS_IN_NAME.
#define DRV_OA_INVALID_CHARS_IN_NAME 20186

/// Andor SDK return code DRV_OA_INVALID_NAMING.
#define DRV_OA_INVALID_NAMING 20187

/// Andor SDK return code DRV_OA_GET_CAMERA_ERROR.
#define DRV_OA_GET_CAMERA_ERROR 20188

/// Andor SDK return code DRV_OA_MODE_ALREADY_EXISTS.
#define DRV_OA_MODE_ALREADY_EXISTS 20189

/// Andor SDK return code DRV_OA_STRINGS_NOT_EQUAL.
#define DRV_OA_STRINGS_NOT_EQUAL 20190

/// Andor SDK return code DRV_OA_NO_USER_DATA.
#define DRV_OA_NO_USER_DATA 20191

/// Andor SDK return code DRV_OA_VALUE_NOT_SUPPORTED.
#define DRV_OA_VALUE_NOT_SUPPORTED 20192

/// Andor SDK return code DRV_OA_MODE_DOES_NOT_EXIST.
#define DRV_OA_MODE_DOES_NOT_EXIST 20193

/// Andor SDK return code DRV_OA_CAMERA_NOT_SUPPORTED.
#define DRV_OA_CAMERA_NOT_SUPPORTED 20194

/// Andor SDK return code DRV_OA_FAILED_TO_GET_MODE.
#define DRV_OA_FAILED_TO_GET_MODE 20195

/// Andor SDK return code DRV_OA_CAMERA_NOT_AVAILABLE.
#define DRV_OA_CAMERA_NOT_AVAILABLE 20196

/// Andor SDK return code DRV_PROCESSING_FAILED.
#define DRV_PROCESSING_FAILED 20211

#ifdef __cplusplus
extern "C"
{
#endif

    /// Abort the current acquisition.
    unsigned int AbortAcquisition();

    /// Switch the cooler off.
    unsigned int CoolerOFF();

    /// Switch the cooler on.
    unsigned int CoolerON();

    /// Get the actual exposure, accumulation cycle and kinetic cycle times.
    unsigned int GetAcquisitionTimings( float *exposure,   /**< [out] exposure time [s] */
                                        float *accumulate, /**< [out] accumulation cycle time [s] */
                                        float *kinetic     /**< [out] kinetic cycle time [s] */
    );

    /// Get the number of available cameras.
    unsigned int GetAvailableCameras( at_32 *totalCameras /**< [out] number of cameras */ );

    /// Get the handle of a camera by index.
    unsigned int GetCameraHandle( at_32  cameraIndex, /**< [in] the camera index */
                                  at_32 *cameraHandle /**< [out] the camera handle */
    );

    /// Get the camera serial number.
    unsigned int GetCameraSerialNumber( int *number /**< [out] the serial number */ );

    /// Get the current EM gain.
    unsigned int GetEMCCDGain( int *gain /**< [out] the EM gain */ );

    /// Get the hardware version information.
    unsigned int GetHardwareVersion( unsigned int *PCB,                   /**< [out] PCB version */
                                     unsigned int *Decode,                /**< [out] decode version */
                                     unsigned int *dummy1,                /**< [out] unused */
                                     unsigned int *dummy2,                /**< [out] unused */
                                     unsigned int *CameraFirmwareVersion, /**< [out] firmware version */
                                     unsigned int *CameraFirmwareBuild    /**< [out] firmware build */
    );

    /// Get the camera head model name.
    unsigned int GetHeadModel( char *name /**< [out] the model name, at least MAX_PATH long */ );

    /// Get the number of A/D channels.
    unsigned int GetNumberADChannels( int *channels /**< [out] the number of channels */ );

    /// Get the number of output amplifiers.
    unsigned int GetNumberAmp( int *amp /**< [out] the number of amplifiers */ );

    /// Get the readout time.
    unsigned int GetReadOutTime( float *ReadOutTime /**< [out] the readout time [s] */ );

    /// Get the software version information.
    unsigned int GetSoftwareVersion( unsigned int *eprom,   /**< [out] EPROM version */
                                     unsigned int *coffile, /**< [out] COF file version */
                                     unsigned int *vxdrev,  /**< [out] driver revision */
                                     unsigned int *vxdver,  /**< [out] driver version */
                                     unsigned int *dllrev,  /**< [out] library revision */
                                     unsigned int *dllver   /**< [out] library version */
    );

    /// Get the camera status, e.g. DRV_IDLE or DRV_ACQUIRING.
    unsigned int GetStatus( int *status /**< [out] the status */ );

    /// Get the sensor temperature; the return value gives the temperature status.
    unsigned int GetTemperatureF( float *temperature /**< [out] the temperature [C] */ );

    /// Initialize the SDK.
    unsigned int Initialize( char *dir /**< [in] the directory holding the SDK configuration files */ );

    /// Set the acquisition mode.
    unsigned int SetAcquisitionMode( int mode /**< [in] the acquisition mode */ );

    /// Set the Camera Link output mode.
    unsigned int SetCameraLinkMode( int mode /**< [in] 1 to enable Camera Link output */ );

    /// Select the camera to control.
    unsigned int SetCurrentCamera( at_32 cameraHandle /**< [in] the camera handle */ );

    /// Set the EM gain.
    unsigned int SetEMCCDGain( int gain /**< [in] the EM gain */ );

    /// Set the EM gain mode.
    unsigned int SetEMGainMode( int mode /**< [in] the EM gain mode */ );

    /// Set the exposure time.
    unsigned int SetExposureTime( float time /**< [in] the exposure time [s] */ );

    /// Set frame transfer mode.
    unsigned int SetFrameTransferMode( int mode /**< [in] 1 for frame transfer */ );

    /// Set the horizontal shift speed.
    unsigned int SetHSSpeed( int typ,  /**< [in] the output amplifier, 0 for EMCCD, 1 for conventional */
                             int index /**< [in] the speed index */
    );

    /// Set the image binning and region.
    unsigned int SetImage( int hbin,   /**< [in] horizontal binning */
                           int vbin,   /**< [in] vertical binning */
                           int hstart, /**< [in] first column, 1-based and inclusive */
                           int hend,   /**< [in] last column, inclusive */
                           int vstart, /**< [in] first row, 1-based and inclusive */
                           int vend    /**< [in] last row, inclusive */
    );

    /// Set the isolated crop mode.
    unsigned int SetIsolatedCropModeEx( int active,     /**< [in] 1 to enable crop mode */
                                        int cropheight, /**< [in] crop height */
                                        int cropwidth,  /**< [in] crop width */
                                        int vbin,       /**< [in] vertical binning */
                                        int hbin,       /**< [in] horizontal binning */
                                        int cropleft,   /**< [in] first column */
                                        int cropbottom  /**< [in] first row */
    );

    /// Set the isolated crop mode type.
    unsigned int SetIsolatedCropModeType( int type /**< [in] 1 for low latency */ );

    /// Set the output amplifier.
    unsigned int SetOutputAmplifier( int typ /**< [in] 0 for EMCCD, 1 for conventional */ );

    /// Set the read mode.
    unsigned int SetReadMode( int mode /**< [in] the read mode, 4 for image */ );

    /// Set the shutter mode.
    unsigned int SetShutter( int typ,         /**< [in] the TTL type */
                             int mode,        /**< [in] 0 auto, 1 open, 2 closed */
                             int closingtime, /**< [in] closing time [ms] */
                             int openingtime  /**< [in] opening time [ms] */
    );

    /// Set the temperature setpoint.
    unsigned int SetTemperature( int temperature /**< [in] the setpoint [C] */ );

    /// Set the vertical shift speed.
    unsigned int SetVSSpeed( int index /**< [in] the speed index */ );

    /// Shut down the SDK.
    unsigned int ShutDown();

    /// Start an acquisition.
    unsigned int StartAcquisition();

#ifdef __cplusplus
}
#endif

#endif // andorCtrl_tests_stubs_atmcdLXd_h
