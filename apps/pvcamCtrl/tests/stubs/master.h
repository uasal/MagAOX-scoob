/** \file master.h
 * \brief Test stub for the Teledyne PVCAM `master.h` header, declaring only the basic types pvcamCtrl uses.
 * \author Claude Code
 *
 * The typedefs match those of the real PVCAM SDK on 64-bit Linux.
 *
 * \ingroup pvcamCtrl_files
 */

#ifndef pvcamCtrl_tests_stubs_master_h
#define pvcamCtrl_tests_stubs_master_h

/// PVCAM boolean type.
typedef unsigned short rs_bool;

/// PVCAM signed 8 bit integer.
typedef signed char int8;

/// PVCAM unsigned 8 bit integer.
typedef unsigned char uns8;

/// PVCAM signed 16 bit integer.
typedef short int16;

/// PVCAM unsigned 16 bit integer.
typedef unsigned short uns16;

/// PVCAM signed 32 bit integer.
typedef int int32;

/// PVCAM unsigned 32 bit integer.
typedef unsigned int uns32;

/// PVCAM 32 bit float.
typedef float flt32;

/// PVCAM 64 bit float.
typedef double flt64;

/// PVCAM unsigned 64 bit integer.
typedef unsigned long long ulong64;

/// PVCAM signed 64 bit integer.
typedef signed long long long64;

/// PVCAM failure return value.
#define PV_FAIL ( (rs_bool)0 )

/// PVCAM success return value.
#define PV_OK ( (rs_bool)1 )

#endif // pvcamCtrl_tests_stubs_master_h
