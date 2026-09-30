/** \file picam.h
 * \brief Test stub for the Princeton Instruments PICam SDK header, declaring only what picamCtrl uses.
 * \author Claude Code
 *
 * The function bodies, and the fake camera state they act on, are defined in `picamCtrl_test.cpp`.
 *
 * The enumerator values are arbitrary (they do not match the real SDK), and `PicamError` has a fixed `int`
 * underlying type as it does in practice, so that the app's `< 0` comparisons compile as they do with the real SDK.
 *
 * \ingroup picamCtrl_files
 */

#ifndef picamCtrl_tests_stubs_picam_h
#define picamCtrl_tests_stubs_picam_h

/// PICam integer type.
typedef int piint;

/// PICam floating point type.
typedef double piflt;

/// PICam boolean type.
typedef int pibln;

/// PICam character type.
typedef char pichar;

/// PICam byte type.
typedef unsigned char pibyte;

/// PICam 64-bit signed integer type.
typedef long long pi64s;

/// Handle to an open camera device or camera model.
typedef void *PicamHandle;

/// Error codes returned by the PICam functions.
typedef enum PicamError : int
{
    PicamError_None                  = 0,  ///< Success
    PicamError_UnexpectedError       = 4,  ///< An unexpected error
    PicamError_InvalidHandle         = 12, ///< The handle is not valid
    PicamError_ParameterDoesNotExist = 21, ///< The parameter does not exist for this camera
    PicamError_TimeOutOccurred       = 32, ///< A wait timed out
    PicamError_CameraFaulted         = 40  ///< The camera faulted
} PicamError;

/// Enumerated types which can be converted to strings with Picam_GetEnumerationString.
typedef enum PicamEnumeratedType
{
    PicamEnumeratedType_Error         = 1, ///< PicamError
    PicamEnumeratedType_Model         = 4, ///< PicamModel
    PicamEnumeratedType_AdcAnalogGain = 9, ///< PicamAdcAnalogGain
    PicamEnumeratedType_AdcQuality    = 10 ///< PicamAdcQuality
} PicamEnumeratedType;

/// Camera models.
typedef enum PicamModel
{
    PicamModel_ProEMHS1024BExcelon = 1216 ///< ProEM-HS 1024B eXcelon
} PicamModel;

/// Camera computer interfaces.
typedef enum PicamComputerInterface
{
    PicamComputerInterface_Usb2            = 1, ///< USB 2
    PicamComputerInterface_GigabitEthernet = 3  ///< Gigabit ethernet
} PicamComputerInterface;

/// Camera parameters used by picamCtrl.
typedef enum PicamParameter
{
    PicamParameter_ExposureTime              = 1,  ///< Exposure time [ms], floating point
    PicamParameter_AdcSpeed                  = 2,  ///< ADC speed [MHz], floating point
    PicamParameter_AdcQuality                = 3,  ///< ADC quality, integer (PicamAdcQuality)
    PicamParameter_AdcAnalogGain             = 4,  ///< ADC analog gain, integer
    PicamParameter_AdcEMGain                 = 5,  ///< EM gain, integer
    PicamParameter_VerticalShiftRate         = 6,  ///< Vertical shift rate [us], floating point
    PicamParameter_SensorTemperatureSetPoint = 7,  ///< Temperature set point [C], floating point
    PicamParameter_SensorTemperatureReading  = 8,  ///< Temperature reading [C], floating point
    PicamParameter_SensorTemperatureStatus   = 9,  ///< Temperature status, integer
    PicamParameter_DisableCoolingFan         = 10, ///< Disable the cooling fan, integer boolean
    PicamParameter_CoolingFanStatus          = 11, ///< Cooling fan status, integer (PicamCoolingFanStatus)
    PicamParameter_ReadoutControlMode        = 12, ///< Readout control mode, integer
    PicamParameter_Rois                      = 13, ///< Regions of interest
    PicamParameter_ReadoutStride             = 14, ///< Readout stride [bytes], integer
    PicamParameter_FrameStride               = 15, ///< Frame stride [bytes], integer
    PicamParameter_FramesPerReadout          = 16, ///< Frames per readout, integer
    PicamParameter_FrameSize                 = 17, ///< Frame size [bytes], integer
    PicamParameter_PixelBitDepth             = 18, ///< Pixel bit depth, integer
    PicamParameter_ReadoutTimeCalculation    = 19, ///< Readout time [ms], floating point
    PicamParameter_FrameRateCalculation      = 20, ///< Frame rate [Hz], floating point
    PicamParameter_TimeStamps                = 21, ///< Time stamp mask, integer
    PicamParameter_TimeStampResolution       = 22, ///< Time stamp resolution [ticks/s], large integer
    PicamParameter_TriggerDetermination      = 23, ///< Trigger determination, integer
    PicamParameter_TriggerResponse           = 24, ///< Trigger response, integer
    PicamParameter_ReadoutCount              = 25  ///< Number of readouts, large integer
} PicamParameter;

/// ADC qualities.
typedef enum PicamAdcQuality
{
    PicamAdcQuality_LowNoise           = 1, ///< Conventional low-noise amplifier
    PicamAdcQuality_HighCapacity       = 2, ///< Conventional high-capacity amplifier
    PicamAdcQuality_ElectronMultiplied = 3  ///< Electron multiplying amplifier
} PicamAdcQuality;

/// Time stamp masks.
typedef enum PicamTimeStampsMask
{
    PicamTimeStampsMask_None            = 0, ///< No time stamps
    PicamTimeStampsMask_ExposureStarted = 1, ///< Time stamp at the start of exposure
    PicamTimeStampsMask_ExposureEnded   = 2  ///< Time stamp at the end of exposure
} PicamTimeStampsMask;

/// Sensor temperature status values.
typedef enum PicamSensorTemperatureStatus
{
    PicamSensorTemperatureStatus_Unlocked = 1, ///< Not at the set point
    PicamSensorTemperatureStatus_Locked   = 2, ///< At the set point
    PicamSensorTemperatureStatus_Faulted  = 3  ///< Temperature control faulted
} PicamSensorTemperatureStatus;

/// Cooling fan status values.
typedef enum PicamCoolingFanStatus
{
    PicamCoolingFanStatus_Off      = 1, ///< The fan is off
    PicamCoolingFanStatus_On       = 2, ///< The fan is on
    PicamCoolingFanStatus_ForcedOn = 3  ///< The fan was forced on by the camera
} PicamCoolingFanStatus;

/// Readout control modes.
typedef enum PicamReadoutControlMode
{
    PicamReadoutControlMode_FullFrame     = 1, ///< Full frame readout
    PicamReadoutControlMode_FrameTransfer = 2  ///< Frame transfer readout
} PicamReadoutControlMode;

/// Trigger determination values.
typedef enum PicamTriggerDetermination
{
    PicamTriggerDetermination_PositivePolarity = 1, ///< Positive polarity
    PicamTriggerDetermination_RisingEdge       = 3  ///< Rising edge
} PicamTriggerDetermination;

/// Trigger response values.
typedef enum PicamTriggerResponse
{
    PicamTriggerResponse_NoResponse        = 1, ///< Ignore triggers
    PicamTriggerResponse_ReadoutPerTrigger = 2  ///< One readout per trigger
} PicamTriggerResponse;

/// Acquisition error mask.
typedef enum PicamAcquisitionErrorsMask
{
    PicamAcquisitionErrorsMask_None = 0 ///< No errors
} PicamAcquisitionErrorsMask;

/// Identifies a camera.
typedef struct PicamCameraID
{
    PicamModel             model;              ///< The camera model
    PicamComputerInterface computer_interface; ///< The computer interface
    pichar                 sensor_name[64];    ///< The sensor name
    pichar                 serial_number[64];  ///< The serial number
} PicamCameraID;

/// One region of interest.
typedef struct PicamRoi
{
    piint x;         ///< Left edge [pixels]
    piint width;     ///< Width [pixels]
    piint x_binning; ///< Horizontal binning
    piint y;         ///< Top edge [pixels]
    piint height;    ///< Height [pixels]
    piint y_binning; ///< Vertical binning
} PicamRoi;

/// A set of regions of interest.
typedef struct PicamRois
{
    PicamRoi *roi_array; ///< The regions
    piint     roi_count; ///< The number of regions
} PicamRois;

/// A range constraint on a parameter.
typedef struct PicamRangeConstraint
{
    pibln        empty_set;             ///< True if no value is allowed
    piflt        minimum;               ///< The minimum value
    piflt        maximum;               ///< The maximum value
    piflt        increment;             ///< The step size
    const piflt *excluded_values_array; ///< Values excluded from the range
    piint        excluded_values_count; ///< The number of excluded values
    const piflt *outlying_values_array; ///< Values allowed outside the range
    piint        outlying_values_count; ///< The number of outlying values
} PicamRangeConstraint;

/// Data made available by an acquisition update.
typedef struct PicamAvailableData
{
    void *initial_readout; ///< Pointer to the first available readout, or NULL
    pi64s readout_count;   ///< The number of available readouts
} PicamAvailableData;

/// Status of an acquisition.
typedef struct PicamAcquisitionStatus
{
    pibln                      running;      ///< True while acquiring
    PicamAcquisitionErrorsMask errors;       ///< Errors that occurred
    piflt                      readout_rate; ///< The readout rate [Hz]
} PicamAcquisitionStatus;

/// Initialize the library.
PicamError Picam_InitializeLibrary();

/// Uninitialize the library.
PicamError Picam_UninitializeLibrary();

/// Get the string for an enumerated value.
PicamError Picam_GetEnumerationString( PicamEnumeratedType type,  /**< [in] the enumerated type */
                                       piint               value, /**< [in] the value */
                                       const pichar      **s /**< [out] the string, free with Picam_DestroyString */
);

/// Free a string returned by the library.
PicamError Picam_DestroyString( const pichar *s /**< [in] the string to free */ );

/// Get the IDs of the available cameras.
PicamError Picam_GetAvailableCameraIDs( const PicamCameraID **id_array, /**< [out] the camera IDs */
                                        piint                *id_count  /**< [out] the number of IDs */
);

/// Free camera IDs returned by Picam_GetAvailableCameraIDs.
PicamError Picam_DestroyCameraIDs( const PicamCameraID *id_array /**< [in] the IDs to free */ );

/// Close a camera.
PicamError Picam_CloseCamera( PicamHandle camera /**< [in] the camera to close */ );

/// Get an integer parameter value.
PicamError Picam_GetParameterIntegerValue( PicamHandle    camera,    /**< [in] the camera */
                                           PicamParameter parameter, /**< [in] the parameter */
                                           piint         *value      /**< [out] the value */
);

/// Set an integer parameter value.
PicamError Picam_SetParameterIntegerValue( PicamHandle    camera,    /**< [in] the camera */
                                           PicamParameter parameter, /**< [in] the parameter */
                                           piint          value      /**< [in] the value */
);

/// Get a large integer parameter value.
PicamError Picam_GetParameterLargeIntegerValue( PicamHandle    camera,    /**< [in] the camera */
                                                PicamParameter parameter, /**< [in] the parameter */
                                                pi64s         *value      /**< [out] the value */
);

/// Set a large integer parameter value.
PicamError Picam_SetParameterLargeIntegerValue( PicamHandle    camera,    /**< [in] the camera */
                                                PicamParameter parameter, /**< [in] the parameter */
                                                pi64s          value      /**< [in] the value */
);

/// Get a floating point parameter value.
PicamError Picam_GetParameterFloatingPointValue( PicamHandle    camera,    /**< [in] the camera */
                                                 PicamParameter parameter, /**< [in] the parameter */
                                                 piflt         *value      /**< [out] the value */
);

/// Set a floating point parameter value.
PicamError Picam_SetParameterFloatingPointValue( PicamHandle    camera,    /**< [in] the camera */
                                                 PicamParameter parameter, /**< [in] the parameter */
                                                 piflt          value      /**< [in] the value */
);

/// Set an integer parameter value while acquiring.
PicamError Picam_SetParameterIntegerValueOnline( PicamHandle    camera,    /**< [in] the camera */
                                                 PicamParameter parameter, /**< [in] the parameter */
                                                 piint          value      /**< [in] the value */
);

/// Set a floating point parameter value while acquiring.
PicamError Picam_SetParameterFloatingPointValueOnline( PicamHandle    camera,    /**< [in] the camera */
                                                       PicamParameter parameter, /**< [in] the parameter */
                                                       piflt          value      /**< [in] the value */
);

/// Get the regions of interest.
PicamError Picam_GetParameterRoisValue( PicamHandle       camera,    /**< [in] the camera */
                                        PicamParameter    parameter, /**< [in] the parameter */
                                        const PicamRois **value      /**< [out] the ROIs, free with Picam_DestroyRois */
);

/// Set the regions of interest.
PicamError Picam_SetParameterRoisValue( PicamHandle      camera,    /**< [in] the camera */
                                        PicamParameter   parameter, /**< [in] the parameter */
                                        const PicamRois *value      /**< [in] the ROIs */
);

/// Free ROIs returned by Picam_GetParameterRoisValue.
PicamError Picam_DestroyRois( const PicamRois *rois /**< [in] the ROIs to free */ );

/// Check whether a parameter exists for a camera.
PicamError Picam_DoesParameterExist( PicamHandle    camera,    /**< [in] the camera */
                                     PicamParameter parameter, /**< [in] the parameter */
                                     pibln         *exists     /**< [out] true if the parameter exists */
);

/// Check whether a parameter can be read.
PicamError Picam_CanReadParameter( PicamHandle    camera,    /**< [in] the camera */
                                   PicamParameter parameter, /**< [in] the parameter */
                                   pibln         *readable   /**< [out] true if the parameter can be read */
);

/// Commit the changed parameters to the camera.
PicamError Picam_CommitParameters( PicamHandle            camera,                  /**< [in] the camera */
                                   const PicamParameter **failed_parameters_array, /**< [out] parameters that failed */
                                   piint                 *failed_parameters_count  /**< [out] number that failed */
);

/// Free a parameter array returned by the library.
PicamError Picam_DestroyParameters( const PicamParameter *parameter_array /**< [in] the array to free */ );

/// Check whether an acquisition is running.
PicamError Picam_IsAcquisitionRunning( PicamHandle camera, /**< [in] the camera */
                                       pibln      *running /**< [out] true if acquiring */
);

/// Start an acquisition.
PicamError Picam_StartAcquisition( PicamHandle camera /**< [in] the camera */ );

/// Stop an acquisition.
PicamError Picam_StopAcquisition( PicamHandle camera /**< [in] the camera */ );

/// Wait for an acquisition update.
PicamError Picam_WaitForAcquisitionUpdate( PicamHandle             camera,           /**< [in] the camera */
                                           piint                   readout_time_out, /**< [in] timeout [ms] */
                                           PicamAvailableData     *available,        /**< [out] the available data */
                                           PicamAcquisitionStatus *status            /**< [out] the status */
);

#endif // picamCtrl_tests_stubs_picam_h
