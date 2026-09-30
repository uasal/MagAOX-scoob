/** \file pvcam.h
 * \brief Test stub for the Teledyne PVCAM `pvcam.h` header, declaring only what pvcamCtrl uses.
 * \author Claude Code
 *
 * The parameter IDs are arbitrary unique values rather than the encoded IDs of the real SDK.  The enumeration
 * values follow the real SDK ordering.  The `pl_*` function bodies, and the fake camera state they act on, are
 * defined in `pvcamCtrl_test.cpp`.
 *
 * `PL_ERR_LIBRARY_NOT_INITIALIZED` is deliberately not defined here, so pvcamCtrl's own fallback definition is used.
 *
 * \ingroup pvcamCtrl_files
 */

#ifndef pvcamCtrl_tests_stubs_pvcam_h
#define pvcamCtrl_tests_stubs_pvcam_h

#include "master.h"

/// Maximum length of a PVCAM error message, including the terminating null.
#define ERROR_MSG_LEN 255

/// Maximum length of a PVCAM camera name, including the terminating null.
#define CAM_NAME_LEN 32

/// Maximum length of the alphanumeric camera serial number, including the terminating null.
#define MAX_ALPHA_SER_NUM_LEN 32

/** \name Parameter IDs
 * @{
 */

/// Detector temperature, in hundredths of a degree C (int16).
#define PARAM_TEMP ( (uns32)1001 )

/// Detector temperature set point, in hundredths of a degree C (int16).
#define PARAM_TEMP_SETPOINT ( (uns32)1002 )

/// Fan speed set point, a PL_FAN_SPEEDS value (int32 enum).
#define PARAM_FAN_SPEED_SETPOINT ( (uns32)1003 )

/// Exposure time (uns64).
#define PARAM_EXPOSURE_TIME ( (uns32)1004 )

/// Readout port, an enumerated parameter (int32 enum).
#define PARAM_READOUT_PORT ( (uns32)1005 )

/// Frame readout time, in microseconds.
#define PARAM_READOUT_TIME ( (uns32)1006 )

/// Pre-trigger delay.
#define PARAM_PRE_TRIGGER_DELAY ( (uns32)1007 )

/// Clearing time.
#define PARAM_CLEARING_TIME ( (uns32)1008 )

/// Post-trigger delay, in nanoseconds.
#define PARAM_POST_TRIGGER_DELAY ( (uns32)1009 )

/// Alphanumeric camera head serial number (char array).
#define PARAM_HEAD_SER_NUM_ALPHA ( (uns32)1010 )

/// Exposure resolution index (uns16).
#define PARAM_EXP_RES_INDEX ( (uns32)1011 )

/// Exposure resolution (int32 enum).
#define PARAM_EXP_RES ( (uns32)1012 )

/// Speed table index.
#define PARAM_SPDTAB_INDEX ( (uns32)1013 )

/// Pixel time, in nanoseconds (uns16).
#define PARAM_PIX_TIME ( (uns32)1014 )

/// Gain index (int16).
#define PARAM_GAIN_INDEX ( (uns32)1015 )

/// Bit depth (int16).
#define PARAM_BIT_DEPTH ( (uns32)1016 )

///@}

/// Parameter attributes queried with pl_get_param.
enum PL_PARAM_ATTRIBUTES
{
    ATTR_CURRENT,   ///< The current value.
    ATTR_COUNT,     ///< The number of values (uns32).
    ATTR_TYPE,      ///< The data type.
    ATTR_MIN,       ///< The minimum value.
    ATTR_MAX,       ///< The maximum value.
    ATTR_DEFAULT,   ///< The default value.
    ATTR_INCREMENT, ///< The step size.
    ATTR_ACCESS,    ///< The access mode.
    ATTR_AVAIL,     ///< Whether the parameter is available (rs_bool).
    ATTR_LIVE       ///< Whether the parameter can be changed during acquisition.
};

/// Fan speed set points for PARAM_FAN_SPEED_SETPOINT.
enum PL_FAN_SPEEDS
{
    FAN_SPEED_HIGH,   ///< Full fan speed.
    FAN_SPEED_MEDIUM, ///< Medium fan speed.
    FAN_SPEED_LOW,    ///< Low fan speed.
    FAN_SPEED_OFF     ///< Fan off.
};

/// Callback events for pl_cam_register_callback_ex3.
enum PL_CALLBACK_TYPE
{
    PL_CALLBACK_BOF, ///< Beginning of frame.
    PL_CALLBACK_EOF  ///< End of frame.
};

/// Exposure modes for pl_exp_setup_cont.
enum PL_EXPOSURE_MODES
{
    TIMED_MODE ///< Internally timed exposures.
};

/// Circular buffer modes for pl_exp_setup_cont.
enum PL_CIRC_MODES
{
    CIRC_NONE,        ///< Not circular.
    CIRC_OVERWRITE,   ///< Circular, overwriting old frames.
    CIRC_NO_OVERWRITE ///< Circular, without overwriting.
};

/// Camera open modes for pl_cam_open.
enum PL_OPEN_MODES
{
    OPEN_EXCLUSIVE ///< Exclusive access.
};

/// Abort modes for pl_exp_stop_cont.
enum PL_CCS_ABORT_MODES
{
    CCS_NO_CHANGE, ///< Do not change the camera state.
    CCS_HALT       ///< Halt the camera.
};

/// A readout region.
typedef struct rgn_type
{
    uns16 s1;   ///< First serial register pixel.
    uns16 s2;   ///< Last serial register pixel.
    uns16 sbin; ///< Serial binning.
    uns16 p1;   ///< First parallel register pixel.
    uns16 p2;   ///< Last parallel register pixel.
    uns16 pbin; ///< Parallel binning.
} rgn_type;

/// Frame metadata passed to the end-of-frame callback.
typedef struct FRAME_INFO
{
    int16  hCam;         ///< Camera handle.
    int32  FrameNr;      ///< Frame number.
    long64 TimeStamp;    ///< End-of-frame time stamp.
    int32  ReadoutTime;  ///< Readout time.
    long64 TimeStampBOF; ///< Beginning-of-frame time stamp.
} FRAME_INFO;

extern "C"
{

    /// Get the code of the most recent PVCAM error.
    int16 pl_error_code( void );

    /// Get the message for a PVCAM error code.
    rs_bool pl_error_message( int16 err_code, /**< [in] the error code */
                              char *msg       /**< [out] buffer of at least ERROR_MSG_LEN characters */
    );

    /// Initialize the PVCAM library.
    rs_bool pl_pvcam_init( void );

    /// Uninitialize the PVCAM library.
    rs_bool pl_pvcam_uninit( void );

    /// Get the number of cameras.
    rs_bool pl_cam_get_total( int16 *totl_cams /**< [out] the number of cameras */ );

    /// Get the name of a camera.
    rs_bool pl_cam_get_name( int16 cam_num, /**< [in] the camera index */
                             char *cam_name /**< [out] buffer of at least CAM_NAME_LEN characters */
    );

    /// Open a camera.
    rs_bool pl_cam_open( char  *cam_name, /**< [in] the camera name */
                         int16 *hcam,     /**< [out] the camera handle */
                         int16  o_mode    /**< [in] the open mode */
    );

    /// Close a camera.
    rs_bool pl_cam_close( int16 hcam /**< [in] the camera handle */ );

    /// Get an attribute of a parameter.
    rs_bool pl_get_param( int16 hcam,            /**< [in] the camera handle */
                          uns32 param_id,        /**< [in] the parameter ID */
                          int16 param_attribute, /**< [in] the attribute, a PL_PARAM_ATTRIBUTES value */
                          void *param_value      /**< [out] the value */
    );

    /// Set the current value of a parameter.
    rs_bool pl_set_param( int16 hcam,       /**< [in] the camera handle */
                          uns32 param_id,   /**< [in] the parameter ID */
                          void *param_value /**< [in] the value */
    );

    /// Get the value and description of an enumerated parameter entry.
    rs_bool pl_get_enum_param( int16  hcam,     /**< [in] the camera handle */
                               uns32  param_id, /**< [in] the parameter ID */
                               uns32  index,    /**< [in] the entry index */
                               int32 *value,    /**< [out] the entry value */
                               char  *desc,     /**< [out] the entry description */
                               uns32  length    /**< [in] the size of desc */
    );

    /// Get the length of an enumerated parameter entry's description, including the terminating null.
    rs_bool pl_enum_str_length( int16  hcam,     /**< [in] the camera handle */
                                uns32  param_id, /**< [in] the parameter ID */
                                uns32  index,    /**< [in] the entry index */
                                uns32 *length    /**< [out] the description length */
    );

    /// Register an extended callback with context.
    rs_bool pl_cam_register_callback_ex3( int16 hcam,           /**< [in] the camera handle */
                                          int32 callback_event, /**< [in] a PL_CALLBACK_TYPE value */
                                          void *callback,       /**< [in] the callback function */
                                          void *context         /**< [in] context passed to the callback */
    );

    /// Deregister a callback.
    rs_bool pl_cam_deregister_callback( int16 hcam,          /**< [in] the camera handle */
                                        int32 callback_event /**< [in] a PL_CALLBACK_TYPE value */
    );

    /// Set up continuous (circular buffer) acquisition.
    rs_bool pl_exp_setup_cont( int16           hcam,          /**< [in] the camera handle */
                               uns16           rgn_total,     /**< [in] the number of regions */
                               const rgn_type *rgn_array,     /**< [in] the regions */
                               int16           exp_mode,      /**< [in] a PL_EXPOSURE_MODES value */
                               uns32           exposure_time, /**< [in] the exposure time */
                               uns32          *exp_bytes,     /**< [out] the size of one frame in bytes */
                               int16           buffer_mode    /**< [in] a PL_CIRC_MODES value */
    );

    /// Start continuous acquisition into a circular buffer.
    rs_bool pl_exp_start_cont( int16 hcam,         /**< [in] the camera handle */
                               void *pixel_stream, /**< [in] the circular buffer */
                               uns32 size          /**< [in] the size of the buffer in bytes */
    );

    /// Get a pointer to the latest acquired frame.
    rs_bool pl_exp_get_latest_frame( int16  hcam, /**< [in] the camera handle */
                                     void **frame /**< [out] pointer to the frame */
    );

    /// Stop continuous acquisition.
    rs_bool pl_exp_stop_cont( int16 hcam,     /**< [in] the camera handle */
                              int16 cam_state /**< [in] a PL_CCS_ABORT_MODES value */
    );
}

#endif // pvcamCtrl_tests_stubs_pvcam_h
