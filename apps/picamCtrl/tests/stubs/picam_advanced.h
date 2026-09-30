/** \file picam_advanced.h
 * \brief Test stub for the Princeton Instruments PICam advanced SDK header, declaring only what picamCtrl uses.
 * \author Claude Code
 *
 * The function bodies, and the fake camera state they act on, are defined in `picamCtrl_test.cpp`.
 *
 * \ingroup picamCtrl_files
 */

#ifndef picamCtrl_tests_stubs_picam_advanced_h
#define picamCtrl_tests_stubs_picam_advanced_h

#include "picam.h"

/// A user-supplied acquisition buffer.
typedef struct PicamAcquisitionBuffer
{
    void *memory;      ///< The buffer memory
    pi64s memory_size; ///< The buffer size [bytes]
} PicamAcquisitionBuffer;

/// Open a camera device.
PicamError PicamAdvanced_OpenCameraDevice( const PicamCameraID *id,    /**< [in] the camera to open */
                                           PicamHandle         *device /**< [out] the device handle */
);

/// Get the model handle for a camera device.
PicamError PicamAdvanced_GetCameraModel( PicamHandle  device, /**< [in] the device */
                                         PicamHandle *model   /**< [out] the model handle */
);

/// Get the range constraints of a parameter.
PicamError
PicamAdvanced_GetParameterRangeConstraints( PicamHandle                  camera,           /**< [in] the camera */
                                            PicamParameter               parameter,        /**< [in] the parameter */
                                            const PicamRangeConstraint **constraint_array, /**< [out] the constraints */
                                            piint *constraint_count /**< [out] the number of constraints */
);

/// Set the acquisition buffer.
PicamError PicamAdvanced_SetAcquisitionBuffer( PicamHandle                   device, /**< [in] the device */
                                               const PicamAcquisitionBuffer *buffer  /**< [in] the buffer, or NULL */
);

#endif // picamCtrl_tests_stubs_picam_advanced_h
