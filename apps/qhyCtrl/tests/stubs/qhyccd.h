/** \file qhyccd.h
 * \brief Test stub for the QHYCCD SDK header, declaring only what qhyCtrl uses.
 * \author Claude Code
 *
 * The function bodies, and the fake camera state they act on, are defined in `qhyCtrl_test.cpp`.
 * As in the real SDK, `qhyccd_handle` is `void`.
 *
 * \ingroup qhyCtrl_files
 */

#ifndef qhyCtrl_tests_stubs_qhyccd_h
#define qhyCtrl_tests_stubs_qhyccd_h

#include <stdint.h>

/// Return code for a successful QHYCCD SDK call.
#define QHYCCD_SUCCESS 0

/// Return code for a failed QHYCCD SDK call.
#define QHYCCD_ERROR 0xFFFFFFFF

/// The camera handle type, `void` as in the real SDK.
typedef void qhyccd_handle;

/// Camera parameter identifiers used with GetQHYCCDParam and SetQHYCCDParam.
enum CONTROL_ID
{
    CONTROL_BRIGHTNESS = 0, ///< Image brightness
    CONTROL_GAIN       = 6, ///< Gain
    CONTROL_OFFSET     = 7, ///< Offset
    CONTROL_EXPOSURE   = 8, ///< Exposure time, in microseconds
    CONTROL_CURTEMP    = 14 ///< Current sensor temperature, in C
};

#ifdef __cplusplus
extern "C"
{
#endif

    /// Get the SDK version.
    uint32_t GetQHYCCDSDKVersion( uint32_t *year,  /**< [out] the version year */
                                  uint32_t *month, /**< [out] the version month */
                                  uint32_t *day,   /**< [out] the version day */
                                  uint32_t *subday /**< [out] the version sub-day */
    );

    /// Get the camera firmware version.
    uint32_t GetQHYCCDFWVersion( qhyccd_handle *handle, /**< [in] the camera handle */
                                 uint8_t       *buf     /**< [out] the firmware version bytes */
    );

    /// Initialize the SDK resources.
    uint32_t InitQHYCCDResource();

    /// Release the SDK resources.
    uint32_t ReleaseQHYCCDResource();

    /// Open the camera with the given ID.
    qhyccd_handle *OpenQHYCCD( char *id /**< [in] the camera ID */ );

    /// Close a camera.
    uint32_t CloseQHYCCD( qhyccd_handle *handle /**< [in] the camera handle */ );

    /// Get the camera status.
    uint32_t GetQHYCCDCameraStatus( qhyccd_handle *h,  /**< [in] the camera handle */
                                    uint8_t       *buf /**< [out] the status bytes */
    );

    /// Set the binning.
    uint32_t SetQHYCCDBinMode( qhyccd_handle *handle, /**< [in] the camera handle */
                               uint32_t       wbin,   /**< [in] the x binning */
                               uint32_t       hbin    /**< [in] the y binning */
    );

    /// Set the ROI.
    uint32_t SetQHYCCDResolution( qhyccd_handle *handle, /**< [in] the camera handle */
                                  uint32_t       x,      /**< [in] the ROI start x */
                                  uint32_t       y,      /**< [in] the ROI start y */
                                  uint32_t       xsize,  /**< [in] the ROI width */
                                  uint32_t       ysize   /**< [in] the ROI height */
    );

    /// Get the size, in bytes, of the image buffer needed for a frame.
    uint32_t GetQHYCCDMemLength( qhyccd_handle *handle /**< [in] the camera handle */ );

    /// Cancel an exposure.
    uint32_t CancelQHYCCDExposing( qhyccd_handle *handle /**< [in] the camera handle */ );

    /// Read a single frame.
    uint32_t GetQHYCCDSingleFrame( qhyccd_handle *handle,   /**< [in] the camera handle */
                                   uint32_t      *w,        /**< [out] the frame width */
                                   uint32_t      *h,        /**< [out] the frame height */
                                   uint32_t      *bpp,      /**< [out] the bits per pixel */
                                   uint32_t      *channels, /**< [out] the number of channels */
                                   uint8_t       *imgdata   /**< [out] the frame data */
    );

    /// Get a camera parameter.
    double GetQHYCCDParam( qhyccd_handle *handle,   /**< [in] the camera handle */
                           CONTROL_ID     controlId /**< [in] the parameter to get */
    );

    /// Set a camera parameter.
    uint32_t SetQHYCCDParam( qhyccd_handle *handle,    /**< [in] the camera handle */
                             CONTROL_ID     controlId, /**< [in] the parameter to set */
                             double         value      /**< [in] the new value */
    );

#ifdef __cplusplus
}
#endif

#endif // qhyCtrl_tests_stubs_qhyccd_h
