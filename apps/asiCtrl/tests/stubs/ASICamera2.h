/** \file ASICamera2.h
 * \brief Test stub for the ZWO ASI camera SDK header, declaring only what asiCtrl uses.
 * \author Claude Code
 *
 * The function bodies, and the fake camera state they act on, are defined in `asiCtrl_test.cpp`.
 *
 * Unlike the real SDK, `ASI_ERROR_CODE` has a fixed `int` underlying type so that tests can inject
 * negative return codes, which are the only failures asiCtrl checks for.
 *
 * \ingroup asiCtrl_files
 */

#ifndef asiCtrl_tests_stubs_ASICamera2_h
#define asiCtrl_tests_stubs_ASICamera2_h

/// Boolean type used by the ASI SDK.
typedef enum ASI_BOOL
{
    ASI_FALSE = 0, ///< False
    ASI_TRUE       ///< True
} ASI_BOOL;

/// Image formats supported by the ASI SDK.
typedef enum ASI_IMG_TYPE
{
    ASI_IMG_RAW8  = 0, ///< 8 bit raw image
    ASI_IMG_RGB24 = 1, ///< 24 bit RGB image
    ASI_IMG_RAW16 = 2, ///< 16 bit raw image
    ASI_IMG_Y8    = 3, ///< 8 bit luminance image
    ASI_IMG_END   = -1 ///< End marker
} ASI_IMG_TYPE;

/// Camera control identifiers used with ASIGetControlValue and ASISetControlValue.
typedef enum ASI_CONTROL_TYPE
{
    ASI_GAIN            = 0,  ///< Gain
    ASI_EXPOSURE        = 1,  ///< Exposure time, in microseconds
    ASI_OFFSET          = 5,  ///< Offset (black level)
    ASI_TEMPERATURE     = 8,  ///< Sensor temperature, in units of 0.1 C
    ASI_HIGH_SPEED_MODE = 14, ///< High speed readout mode
    ASI_TARGET_TEMP     = 16, ///< Cooler target temperature, in C
    ASI_COOLER_ON       = 17  ///< Cooler on/off
} ASI_CONTROL_TYPE;

/// Alias for ASI_OFFSET, as in the real SDK.
#define ASI_BRIGHTNESS ASI_OFFSET

/// Return codes from the ASI SDK functions.
typedef enum ASI_ERROR_CODE : int
{
    ASI_SUCCESS                    = 0,  ///< Success
    ASI_ERROR_INVALID_INDEX        = 1,  ///< No camera connected or index out of range
    ASI_ERROR_INVALID_ID           = 2,  ///< Invalid camera ID
    ASI_ERROR_INVALID_CONTROL_TYPE = 3,  ///< Invalid control type
    ASI_ERROR_CAMERA_CLOSED        = 4,  ///< Camera is not open
    ASI_ERROR_TIMEOUT              = 11, ///< Timeout
    ASI_ERROR_INVALID_SIZE         = 13  ///< Invalid ROI size
} ASI_ERROR_CODE;

/// Camera information returned by ASIGetCameraProperty.
typedef struct _ASI_CAMERA_INFO
{
    char Name[64]; ///< The camera model name
    int  CameraID; ///< The ID used to open and control the camera
} ASI_CAMERA_INFO;

#ifdef __cplusplus
extern "C"
{
#endif

    /// Get the number of connected ASI cameras.
    int ASIGetNumOfConnectedCameras();

    /// Get the properties of the camera at an index.
    ASI_ERROR_CODE ASIGetCameraProperty( ASI_CAMERA_INFO *pASICameraInfo, /**< [out] the camera information */
                                         int              iCameraIndex    /**< [in] the camera index */
    );

    /// Open a camera.
    ASI_ERROR_CODE ASIOpenCamera( int iCameraID /**< [in] the camera ID */ );

    /// Initialize an opened camera.
    ASI_ERROR_CODE ASIInitCamera( int iCameraID /**< [in] the camera ID */ );

    /// Close a camera.
    ASI_ERROR_CODE ASICloseCamera( int iCameraID /**< [in] the camera ID */ );

    /// Get the value of a camera control.
    ASI_ERROR_CODE ASIGetControlValue( int              iCameraID,   /**< [in] the camera ID */
                                       ASI_CONTROL_TYPE ControlType, /**< [in] the control to read */
                                       long            *plValue,     /**< [out] the control value */
                                       ASI_BOOL        *pbAuto       /**< [out] whether auto mode is on */
    );

    /// Set the value of a camera control.
    ASI_ERROR_CODE ASISetControlValue( int              iCameraID,   /**< [in] the camera ID */
                                       ASI_CONTROL_TYPE ControlType, /**< [in] the control to set */
                                       long             lValue,      /**< [in] the new value */
                                       ASI_BOOL         bAuto        /**< [in] whether to use auto mode */
    );

    /// Set the ROI size, binning, and image format.
    ASI_ERROR_CODE ASISetROIFormat( int          iCameraID, /**< [in] the camera ID */
                                    int          iWidth,    /**< [in] the ROI width, in binned pixels */
                                    int          iHeight,   /**< [in] the ROI height, in binned pixels */
                                    int          iBin,      /**< [in] the binning */
                                    ASI_IMG_TYPE Img_type   /**< [in] the image format */
    );

    /// Get the ROI size, binning, and image format.
    ASI_ERROR_CODE ASIGetROIFormat( int           iCameraID, /**< [in] the camera ID */
                                    int          *piWidth,   /**< [out] the ROI width */
                                    int          *piHeight,  /**< [out] the ROI height */
                                    int          *piBin,     /**< [out] the binning */
                                    ASI_IMG_TYPE *pImg_type  /**< [out] the image format */
    );

    /// Set the ROI start position.
    ASI_ERROR_CODE ASISetStartPos( int iCameraID, /**< [in] the camera ID */
                                   int iStartX,   /**< [in] the ROI start x, in binned pixels */
                                   int iStartY    /**< [in] the ROI start y, in binned pixels */
    );

    /// Get the ROI start position.
    ASI_ERROR_CODE ASIGetStartPos( int  iCameraID, /**< [in] the camera ID */
                                   int *piStartX,  /**< [out] the ROI start x */
                                   int *piStartY   /**< [out] the ROI start y */
    );

    /// Start video capture.
    ASI_ERROR_CODE ASIStartVideoCapture( int iCameraID /**< [in] the camera ID */ );

    /// Stop video capture.
    ASI_ERROR_CODE ASIStopVideoCapture( int iCameraID /**< [in] the camera ID */ );

    /// Read one video frame.
    ASI_ERROR_CODE ASIGetVideoData( int            iCameraID, /**< [in] the camera ID */
                                    unsigned char *pBuffer,   /**< [out] the image buffer */
                                    long           lBuffSize, /**< [in] the image buffer size, in bytes */
                                    int            iWaitms    /**< [in] the timeout, in milliseconds */
    );

#ifdef __cplusplus
}
#endif

#endif // asiCtrl_tests_stubs_ASICamera2_h
