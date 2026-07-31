#pragma once

#include "lina/array.h"

#include <vector>

namespace lina {

Array2D<double> gemm(const Array2D<double>& a,
                     const Array2D<double>& b,
                     bool transpose_a = false,
                     bool transpose_b = false);

std::vector<double> gemv(const Array2D<double>& a,
                         const std::vector<double>& x,
                         bool transpose_a = false);

struct SvdResult {
    Array2D<double> u;
    std::vector<double> s;
    Array2D<double> vt;
};

struct SvdResultF {
    Array2D<float> u;
    std::vector<float> s;
    Array2D<float> vt;
};

SvdResult svd(const Array2D<double>& a);
// Economy ("thin") SVD: u is m x k and vt is k x n with k = min(m, n).
// Avoids the O(m^2) full-U allocation of svd(), which is catastrophic for
// tall matrices (e.g. least-squares over a flattened image: m ~ 1e5).
SvdResult svd_thin(const Array2D<double>& a);

SvdResultF svd_float(const Array2D<float>& a);
SvdResultF svd_float_cpu(const Array2D<float>& a);

void set_num_threads(int nthreads);
int get_num_threads();

} // namespace lina
