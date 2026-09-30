/** \file cuda_runtime_api.h
 * \brief Test stub for the CUDA runtime API header, declaring only what po4ao uses.
 * \author Claude Code
 *
 * The functions are defined in `po4ao_test.cpp`, where "device" memory is ordinary host memory.
 *
 * This header must not declare `cudaIpcMemHandle_t`: ImageStreamIO's `ImageStruct.h` typedefs it itself when
 * `HAVE_CUDA` is not defined, as in the test build.
 */

#ifndef po4ao_tests_stubs_cuda_runtime_api_h
#define po4ao_tests_stubs_cuda_runtime_api_h

#include <cstddef>

/// CUDA runtime error codes (subset).
enum cudaError
{
    cudaSuccess               = 0, ///< No error.
    cudaErrorInvalidValue     = 1, ///< An invalid argument.
    cudaErrorMemoryAllocation = 2  ///< Allocation failed.
};

/// The CUDA runtime error type.
typedef enum cudaError cudaError_t;

/// The direction of a cudaMemcpy().
enum cudaMemcpyKind
{
    cudaMemcpyHostToHost     = 0, ///< Host to host.
    cudaMemcpyHostToDevice   = 1, ///< Host to device.
    cudaMemcpyDeviceToHost   = 2, ///< Device to host.
    cudaMemcpyDeviceToDevice = 3, ///< Device to device.
    cudaMemcpyDefault        = 4  ///< Inferred from the pointers.
};

/// Allocate device memory.
cudaError_t cudaMalloc( void **devPtr /**< [out] the allocated pointer */,
                        size_t size /**< [in] the number of bytes */ );

/// Free device memory.
cudaError_t cudaFree( void *devPtr /**< [in] the pointer to free */ );

/// Copy memory between host and device.
cudaError_t cudaMemcpy( void               *dst /**< [out] the destination */,
                        const void         *src /**< [in] the source */,
                        size_t              count /**< [in] the number of bytes */,
                        enum cudaMemcpyKind kind /**< [in] the copy direction */ );

#endif // po4ao_tests_stubs_cuda_runtime_api_h
