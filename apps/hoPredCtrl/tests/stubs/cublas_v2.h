/** \file cublas_v2.h
 * \brief Test stub for the cuBLAS v2 header, declaring only what the hoPredCtrl `.cuh` headers use.
 * \author Claude Code
 *
 * The functions are only called from unused `static inline` wrappers in `utils.cuh`, so they are declared but never
 * defined: no code referencing them is emitted.
 */

#ifndef hoPredCtrl_tests_stubs_cublas_v2_h
#define hoPredCtrl_tests_stubs_cublas_v2_h

#include "cuda_runtime.h"

/// Opaque cuBLAS context.
struct cublasContext;

/// The cuBLAS handle type.
typedef struct cublasContext *cublasHandle_t;

/// cuBLAS status codes (subset).
typedef enum
{
    CUBLAS_STATUS_SUCCESS = 0 ///< The operation succeeded.
} cublasStatus_t;

/// cuBLAS matrix operations.
typedef enum
{
    CUBLAS_OP_N = 0, ///< No transpose.
    CUBLAS_OP_T = 1, ///< Transpose.
    CUBLAS_OP_C = 2  ///< Conjugate transpose.
} cublasOperation_t;

/// Strided batched single-precision matrix multiply.
cublasStatus_t cublasSgemmStridedBatched( cublasHandle_t    handle /**< [in] cuBLAS handle */,
                                          cublasOperation_t transa /**< [in] operation on A */,
                                          cublasOperation_t transb /**< [in] operation on B */,
                                          int               m /**< [in] rows of op(A) and C */,
                                          int               n /**< [in] columns of op(B) and C */,
                                          int               k /**< [in] columns of op(A) */,
                                          const float      *alpha /**< [in] scale of A*B */,
                                          const float      *A /**< [in] A matrices */,
                                          int               lda /**< [in] leading dimension of A */,
                                          long long int     strideA /**< [in] stride between A matrices */,
                                          const float      *B /**< [in] B matrices */,
                                          int               ldb /**< [in] leading dimension of B */,
                                          long long int     strideB /**< [in] stride between B matrices */,
                                          const float      *beta /**< [in] scale of C */,
                                          float            *C /**< [in,out] C matrices */,
                                          int               ldc /**< [in] leading dimension of C */,
                                          long long int     strideC /**< [in] stride between C matrices */,
                                          int               batchCount /**< [in] number of matrices */ );

/// Strided batched double-precision matrix multiply.
cublasStatus_t cublasDgemmStridedBatched( cublasHandle_t    handle /**< [in] cuBLAS handle */,
                                          cublasOperation_t transa /**< [in] operation on A */,
                                          cublasOperation_t transb /**< [in] operation on B */,
                                          int               m /**< [in] rows of op(A) and C */,
                                          int               n /**< [in] columns of op(B) and C */,
                                          int               k /**< [in] columns of op(A) */,
                                          const double     *alpha /**< [in] scale of A*B */,
                                          const double     *A /**< [in] A matrices */,
                                          int               lda /**< [in] leading dimension of A */,
                                          long long int     strideA /**< [in] stride between A matrices */,
                                          const double     *B /**< [in] B matrices */,
                                          int               ldb /**< [in] leading dimension of B */,
                                          long long int     strideB /**< [in] stride between B matrices */,
                                          const double     *beta /**< [in] scale of C */,
                                          double           *C /**< [in,out] C matrices */,
                                          int               ldc /**< [in] leading dimension of C */,
                                          long long int     strideC /**< [in] stride between C matrices */,
                                          int               batchCount /**< [in] number of matrices */ );

/// Batched single-precision matrix multiply.
cublasStatus_t cublasSgemmBatched( cublasHandle_t     handle /**< [in] cuBLAS handle */,
                                   cublasOperation_t  transa /**< [in] operation on A */,
                                   cublasOperation_t  transb /**< [in] operation on B */,
                                   int                m /**< [in] rows of op(A) and C */,
                                   int                n /**< [in] columns of op(B) and C */,
                                   int                k /**< [in] columns of op(A) */,
                                   const float       *alpha /**< [in] scale of A*B */,
                                   const float *const Aarray[] /**< [in] A matrices */,
                                   int                lda /**< [in] leading dimension of A */,
                                   const float *const Barray[] /**< [in] B matrices */,
                                   int                ldb /**< [in] leading dimension of B */,
                                   const float       *beta /**< [in] scale of C */,
                                   float *const       Carray[] /**< [in,out] C matrices */,
                                   int                ldc /**< [in] leading dimension of C */,
                                   int                batchCount /**< [in] number of matrices */ );

/// Batched double-precision matrix multiply.
cublasStatus_t cublasDgemmBatched( cublasHandle_t      handle /**< [in] cuBLAS handle */,
                                   cublasOperation_t   transa /**< [in] operation on A */,
                                   cublasOperation_t   transb /**< [in] operation on B */,
                                   int                 m /**< [in] rows of op(A) and C */,
                                   int                 n /**< [in] columns of op(B) and C */,
                                   int                 k /**< [in] columns of op(A) */,
                                   const double       *alpha /**< [in] scale of A*B */,
                                   const double *const Aarray[] /**< [in] A matrices */,
                                   int                 lda /**< [in] leading dimension of A */,
                                   const double *const Barray[] /**< [in] B matrices */,
                                   int                 ldb /**< [in] leading dimension of B */,
                                   const double       *beta /**< [in] scale of C */,
                                   double *const       Carray[] /**< [in,out] C matrices */,
                                   int                 ldc /**< [in] leading dimension of C */,
                                   int                 batchCount /**< [in] number of matrices */ );

/// Batched single-precision matrix inverse.
cublasStatus_t cublasSmatinvBatched( cublasHandle_t     handle /**< [in] cuBLAS handle */,
                                     int                n /**< [in] matrix size */,
                                     const float *const A[] /**< [in] matrices to invert */,
                                     int                lda /**< [in] leading dimension of A */,
                                     float *const       Ainv[] /**< [out] inverses */,
                                     int                lda_inv /**< [in] leading dimension of Ainv */,
                                     int               *info /**< [out] per-matrix status */,
                                     int                batchSize /**< [in] number of matrices */ );

/// Batched double-precision matrix inverse.
cublasStatus_t cublasDmatinvBatched( cublasHandle_t      handle /**< [in] cuBLAS handle */,
                                     int                 n /**< [in] matrix size */,
                                     const double *const A[] /**< [in] matrices to invert */,
                                     int                 lda /**< [in] leading dimension of A */,
                                     double *const       Ainv[] /**< [out] inverses */,
                                     int                 lda_inv /**< [in] leading dimension of Ainv */,
                                     int                *info /**< [out] per-matrix status */,
                                     int                 batchSize /**< [in] number of matrices */ );

/// Single-precision vector scale.
cublasStatus_t cublasSscal( cublasHandle_t handle /**< [in] cuBLAS handle */,
                            int            n /**< [in] number of elements */,
                            const float   *alpha /**< [in] the scale */,
                            float         *x /**< [in,out] the vector */,
                            int            incx /**< [in] stride of x */ );

/// Double-precision vector scale.
cublasStatus_t cublasDscal( cublasHandle_t handle /**< [in] cuBLAS handle */,
                            int            n /**< [in] number of elements */,
                            const double  *alpha /**< [in] the scale */,
                            double        *x /**< [in,out] the vector */,
                            int            incx /**< [in] stride of x */ );

/// Single-precision y = alpha*x + y.
cublasStatus_t cublasSaxpy( cublasHandle_t handle /**< [in] cuBLAS handle */,
                            int            n /**< [in] number of elements */,
                            const float   *alpha /**< [in] the scale */,
                            const float   *x /**< [in] the x vector */,
                            int            incx /**< [in] stride of x */,
                            float         *y /**< [in,out] the y vector */,
                            int            incy /**< [in] stride of y */ );

/// Double-precision y = alpha*x + y.
cublasStatus_t cublasDaxpy( cublasHandle_t handle /**< [in] cuBLAS handle */,
                            int            n /**< [in] number of elements */,
                            const double  *alpha /**< [in] the scale */,
                            const double  *x /**< [in] the x vector */,
                            int            incx /**< [in] stride of x */,
                            double        *y /**< [in,out] the y vector */,
                            int            incy /**< [in] stride of y */ );

/// Single-precision vector copy.
cublasStatus_t cublasScopy( cublasHandle_t handle /**< [in] cuBLAS handle */,
                            int            n /**< [in] number of elements */,
                            const float   *x /**< [in] the source */,
                            int            incx /**< [in] stride of x */,
                            float         *y /**< [out] the destination */,
                            int            incy /**< [in] stride of y */ );

/// Double-precision vector copy.
cublasStatus_t cublasDcopy( cublasHandle_t handle /**< [in] cuBLAS handle */,
                            int            n /**< [in] number of elements */,
                            const double  *x /**< [in] the source */,
                            int            incx /**< [in] stride of x */,
                            double        *y /**< [out] the destination */,
                            int            incy /**< [in] stride of y */ );

#endif // hoPredCtrl_tests_stubs_cublas_v2_h
