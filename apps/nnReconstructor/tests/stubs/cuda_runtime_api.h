/** \file cuda_runtime_api.h
 * \brief Test stub for the CUDA `cuda_runtime_api.h` header, declaring only what nnReconstructor uses.
 * \author Claude Code
 *
 * The functions are defined in `nnReconstructor_test.cpp`, where "device" memory is host memory, so that
 * copies to and from the device, and the fake TensorRT execution, can be checked.
 */

#ifndef nnReconstructor_tests_stubs_cuda_runtime_api_h
#define nnReconstructor_tests_stubs_cuda_runtime_api_h

#include <cstddef>

/// CUDA error codes (subset, with the real values).
typedef enum cudaError
{
    cudaSuccess               = 0, ///< No error.
    cudaErrorMemoryAllocation = 2  ///< Unable to allocate memory.
} cudaError_t;

/// The direction of a memory copy.
enum cudaMemcpyKind
{
    cudaMemcpyHostToHost     = 0, ///< Host to host.
    cudaMemcpyHostToDevice   = 1, ///< Host to device.
    cudaMemcpyDeviceToHost   = 2, ///< Device to host.
    cudaMemcpyDeviceToDevice = 3, ///< Device to device.
    cudaMemcpyDefault        = 4  ///< Inferred from the pointers.
};

extern "C"
{
    /// Allocate device memory.
    /**
     * \returns cudaSuccess or an error code
     */
    cudaError_t cudaMalloc( void **devPtr, /**< [out] the allocated device pointer */
                            size_t size /**< [in] the number of bytes to allocate */ );

    /// Free device memory.
    /**
     * \returns cudaSuccess or an error code
     */
    cudaError_t cudaFree( void *devPtr /**< [in] the device pointer to free */ );

    /// Copy memory between host and device.
    /**
     * \returns cudaSuccess or an error code
     */
    cudaError_t cudaMemcpy( void               *dst,   /**< [out] the destination */
                            const void         *src,   /**< [in] the source */
                            size_t              count, /**< [in] the number of bytes to copy */
                            enum cudaMemcpyKind kind /**< [in] the direction of the copy */ );
}

#endif // nnReconstructor_tests_stubs_cuda_runtime_api_h
