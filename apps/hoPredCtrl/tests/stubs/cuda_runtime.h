/** \file cuda_runtime.h
 * \brief Test stub for the CUDA runtime header, declaring only what the hoPredCtrl `.cuh` headers use.
 * \author Claude Code
 *
 * The CUDA function-space qualifiers are defined as empty macros so that the `__global__` kernel declarations in
 * `utils.cuh` parse as ordinary function declarations under g++.
 *
 * This header must not declare `cudaIpcMemHandle_t`: ImageStreamIO's `ImageStruct.h` typedefs it itself when
 * `HAVE_CUDA` is not defined, as in the test build.
 */

#ifndef hoPredCtrl_tests_stubs_cuda_runtime_h
#define hoPredCtrl_tests_stubs_cuda_runtime_h

#include <cstddef>
#include <cstdint>

#ifndef __global__
    /// CUDA kernel qualifier, empty for host-only compilation.
    #define __global__
#endif

#ifndef __device__
    /// CUDA device-function qualifier, empty for host-only compilation.
    #define __device__
#endif

#ifndef __host__
    /// CUDA host-function qualifier, empty for host-only compilation.
    #define __host__
#endif

/// CUDA runtime error codes (subset).
enum cudaError
{
    cudaSuccess = 0 ///< No error.
};

/// The CUDA runtime error type.
typedef enum cudaError cudaError_t;

#endif // hoPredCtrl_tests_stubs_cuda_runtime_h
