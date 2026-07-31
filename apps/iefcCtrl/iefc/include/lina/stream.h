#pragma once

#include "lina/array.h"

#include <cstddef>
#include <functional>

namespace lina {

class Stream2D {
public:
    virtual ~Stream2D() = default;

    virtual Array2D<double> grab_latest() = 0;
    // Average `nframes` new frames. If wait_frames > 0, skip that many new
    // frames first (post-DM settle), matching magpyx ImageStream.grab_after.
    // Optional stop: polled while waiting; may throw lina::Cancelled.
    virtual Array2D<double> grab_mean(std::size_t nframes,
                                      std::size_t wait_frames = 0,
                                      const std::function<bool()>& stop = {}) = 0;
    virtual void write(const Array2D<double>& data) = 0;
    virtual std::size_t rows() const = 0;
    virtual std::size_t cols() const = 0;
};

} // namespace lina
