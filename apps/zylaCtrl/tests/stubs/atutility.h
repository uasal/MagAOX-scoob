/** \file atutility.h
 * \brief Test stub for the Andor SDK3 utility header, declaring only what zylaCtrl uses.
 * \author Claude Code
 *
 * The function bodies are defined in `zylaCtrl_test.cpp`.
 *
 * \ingroup zylaCtrl_files
 */

#ifndef zylaCtrl_tests_stubs_atutility_h
#define zylaCtrl_tests_stubs_atutility_h

#include "atcore.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /// Convert a raw image buffer to another pixel encoding.
    int AT_ConvertBuffer( AT_U8       *inputBuffer,        /**< [in] the raw buffer from the camera */
                          AT_U8       *outputBuffer,       /**< [out] the converted image */
                          AT_64        width,              /**< [in] the image width */
                          AT_64        height,             /**< [in] the image height */
                          AT_64        stride,             /**< [in] the row stride of the raw buffer, in bytes */
                          const AT_WC *inputPixelEncoding, /**< [in] the raw pixel encoding */
                          const AT_WC *outputPixelEncoding /**< [in] the output pixel encoding */
    );

    /// Initialize the SDK3 utility library.
    int AT_InitialiseUtilityLibrary();

    /// Finalize the SDK3 utility library.
    int AT_FinaliseUtilityLibrary();

#ifdef __cplusplus
}
#endif

#endif // zylaCtrl_tests_stubs_atutility_h
