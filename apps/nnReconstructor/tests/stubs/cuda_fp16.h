/** \file cuda_fp16.h
 * \brief Test stub for the CUDA `cuda_fp16.h` header, declaring only what nnReconstructor uses.
 * \author Claude Code
 *
 * `__half` is a 16-bit IEEE 754 half-precision value, as in CUDA.  The host conversion functions are defined in
 * `nnReconstructor_test.cpp`.
 */

#ifndef nnReconstructor_tests_stubs_cuda_fp16_h
#define nnReconstructor_tests_stubs_cuda_fp16_h

#include <cstdint>

/// A half-precision floating point value.
struct __half
{
    uint16_t __x; ///< The raw IEEE 754 binary16 bits.
};

/// The half-precision type name used by applications.
typedef __half half;

/// Convert a half-precision value to single precision.
/**
 * \returns the value as a float
 */
float __half2float( const __half a /**< [in] the half-precision value */ );

/// Convert a single-precision value to half precision, rounding to nearest even.
/**
 * \returns the value as a half
 */
__half __float2half( const float a /**< [in] the single-precision value */ );

#endif // nnReconstructor_tests_stubs_cuda_fp16_h
