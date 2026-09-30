/** \file ImageStruct.h
 * \brief Forwarding header so that qhyCtrl's `#include <ImageStruct.h>` finds the installed ImageStreamIO header.
 * \author Claude Code
 *
 * qhyCtrl.hpp includes `<ImageStruct.h>` without the `ImageStreamIO/` prefix, and the ImageStreamIO
 * include directory is not on the test include path.
 *
 * \ingroup qhyCtrl_files
 */

#ifndef qhyCtrl_tests_stubs_ImageStruct_h
#define qhyCtrl_tests_stubs_ImageStruct_h

#include <ImageStreamIO/ImageStruct.h>

#endif // qhyCtrl_tests_stubs_ImageStruct_h
