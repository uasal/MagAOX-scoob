/** \file PixeLINKApi.h
 * \brief Stand-in for the PixeLINK SDK API header, for the pixelinkCtrl unit tests.
 * \author Claude Code
 *
 * Declares only the types, constants and functions that pixelinkCtrl uses.  The functions are
 * defined in pixelinkCtrl_test.cpp, which drives them from a fake SDK state.
 *
 * \ingroup pixelinkCtrl_files
 */

#ifndef pixelinkCtrl_tests_stubs_PixeLINKApi_h
#define pixelinkCtrl_tests_stubs_PixeLINKApi_h

#include <cstdint>

/// Unsigned 8 bit integer.
typedef uint8_t U8;

/// Unsigned 16 bit integer.
typedef uint16_t U16;

/// Unsigned 32 bit integer.
typedef uint32_t U32;

/// 32 bit float, used for all feature parameters.
typedef float F32;

/// Untyped pointer.
typedef void *LPVOID;

/// Camera handle.
typedef void *HANDLE;

/// SDK return code, negative (high bit set) on error.
typedef int PXL_RETURN_CODE;

/// True if a return code indicates success.
#define API_SUCCESS( rc ) ( !( ( rc ) & 0x80000000 ) )

/// Success return code.
#define ApiSuccess ( (PXL_RETURN_CODE)0 )

/// Generic error return code.
#define ApiUnknownError ( (PXL_RETURN_CODE)0x80000001 )

/// Start streaming (PxLSetStreamState).
#define START_STREAM 0

/// Pause streaming (PxLSetStreamState).
#define PAUSE_STREAM 1

/// Stop streaming (PxLSetStreamState).
#define STOP_STREAM 2

/// Feature flag: the feature is in manual mode.
#define FEATURE_FLAG_MANUAL 0x00000002

/// Shutter (exposure time) feature, in seconds.
#define FEATURE_SHUTTER 7

/// Exposure is an alias of the shutter feature.
#define FEATURE_EXPOSURE FEATURE_SHUTTER

/// Gain feature, in dB.
#define FEATURE_GAIN 8

/// Sensor temperature feature, in C.
#define FEATURE_SENSOR_TEMPERATURE 11

/// Frame rate feature, in frames per second.
#define FEATURE_FRAME_RATE 18

/// Region of interest feature.
#define FEATURE_ROI 19

/// Pixel addressing (binning/decimation) feature.
#define FEATURE_PIXEL_ADDRESSING 21

/// Pixel format feature.
#define FEATURE_PIXEL_FORMAT 22

/// Camera body temperature feature, in C.
#define FEATURE_BODY_TEMPERATURE 32

/// Index of the ROI left edge parameter.
#define FEATURE_ROI_PARAM_LEFT 0

/// Index of the ROI top edge parameter.
#define FEATURE_ROI_PARAM_TOP 1

/// Index of the ROI width parameter.
#define FEATURE_ROI_PARAM_WIDTH 2

/// Index of the ROI height parameter.
#define FEATURE_ROI_PARAM_HEIGHT 3

/// Number of ROI parameters.
#define FEATURE_ROI_NUM_PARAMS 4

/// Index of the pixel addressing value parameter.
#define FEATURE_PIXEL_ADDRESSING_PARAM_VALUE 0

/// Index of the pixel addressing mode parameter.
#define FEATURE_PIXEL_ADDRESSING_PARAM_MODE 1

/// Index of the pixel addressing x value parameter.
#define FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE 2

/// Index of the pixel addressing y value parameter.
#define FEATURE_PIXEL_ADDRESSING_PARAM_Y_VALUE 3

/// Number of pixel addressing parameters.
#define FEATURE_PIXEL_ADDRESSING_NUM_PARAMS 4

/// Pixel addressing mode: binning.
#define PIXEL_ADDRESSING_MODE_BIN 2

/// Pixel format: 16 bit monochrome.
#define PIXEL_FORMAT_MONO16 1

/// Frame descriptor filled by PxLGetNextFrame.
typedef struct _FRAME_DESC
{
    U32 uSize;        ///< Size of this structure, set by the caller.
    F32 fFrameTime;   ///< Frame time stamp, in seconds.
    U32 uFrameNumber; ///< Frame number.
} FRAME_DESC;

/// Pointer to a frame descriptor.
typedef FRAME_DESC *PFRAME_DESC;

extern "C"
{
    /// Connect to a camera by serial number (0 for the first camera found).
    PXL_RETURN_CODE PxLInitializeEx( U32     serialNumber, /**< [in] camera serial number, 0 for any */
                                     HANDLE *phCamera,     /**< [out] the camera handle */
                                     U32     flags         /**< [in] initialization flags */
    );

    /// Disconnect from a camera.
    PXL_RETURN_CODE PxLUninitialize( HANDLE hCamera /**< [in] the camera handle */ );

    /// Start, pause or stop the camera stream.
    PXL_RETURN_CODE PxLSetStreamState( HANDLE hCamera,    /**< [in] the camera handle */
                                       U32    streamState /**< [in] START_STREAM, PAUSE_STREAM or STOP_STREAM */
    );

    /// Get the parameters of a camera feature.
    PXL_RETURN_CODE PxLGetFeature( HANDLE hCamera,      /**< [in] the camera handle */
                                   U32    featureId,    /**< [in] the feature */
                                   U32   *pFlags,       /**< [out] the feature flags */
                                   U32   *pNumberParms, /**< [in.out] number of parameters */
                                   F32   *pParms        /**< [out] the parameters */
    );

    /// Set the parameters of a camera feature.
    PXL_RETURN_CODE PxLSetFeature( HANDLE     hCamera,     /**< [in] the camera handle */
                                   U32        featureId,   /**< [in] the feature */
                                   U32        flags,       /**< [in] the feature flags */
                                   U32        numberParms, /**< [in] number of parameters */
                                   const F32 *pParms       /**< [in] the parameters */
    );

    /// Get the next frame from the camera stream.
    PXL_RETURN_CODE PxLGetNextFrame( HANDLE      hCamera,    /**< [in] the camera handle */
                                     U32         bufferSize, /**< [in] size of the frame buffer, in bytes */
                                     LPVOID      pFrame,     /**< [out] the frame buffer */
                                     PFRAME_DESC pDescriptor /**< [in.out] the frame descriptor */
    );
}

#endif // pixelinkCtrl_tests_stubs_PixeLINKApi_h
