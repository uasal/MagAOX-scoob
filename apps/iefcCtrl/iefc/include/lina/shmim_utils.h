/** \file shmim_utils.h
  * \brief Milk ImageStreamIO adapter implementing lina::Stream2D.
  */

#pragma once

#include "lina/array.h"
#include "lina/stream.h"

#include <cstddef>
#include <functional>
#include <string>

namespace lina {

/// Milk ImageStreamIO stream as a 2-D grab/write interface.
/** Camera streams are often UINT16; cacao DM channels are typically FLOAT.
  * Grabs convert integer and floating milk types to double. Writes require
  * FLOAT or DOUBLE.
  */
class ShmimStream : public Stream2D {
public:
    ShmimStream();
    explicit ShmimStream(const std::string& name);
    ~ShmimStream() override;

    /// Open an existing ImageStreamIO stream by name.
    void open(const std::string& name /**< [in] milk stream name */);

    /// Create (or recreate) a 2-D ImageStreamIO stream.
    void create(const std::string& name, /**< [in] milk stream name */
                std::size_t rows, /**< [in] size[0] */
                std::size_t cols, /**< [in] size[1] */
                int datatype = 9, /**< [in] milk _DATATYPE_* (default FLOAT) */
                std::size_t cbsize = 1 /**< [in] circular-buffer slices */);

    /// Close the stream if open.
    void close();

    Array2D<double> grab_latest() override;
    Array2D<double> grab_mean(std::size_t nframes, std::size_t wait_frames = 0,
                              const std::function<bool()>& stop = {}) override;
    void write(const Array2D<double>& data) override;

    std::size_t rows() const override;
    std::size_t cols() const override;

    /// Milk `_DATATYPE_*` code, or 0 if the stream is not open.
    int datatype() const;

    /// Human-readable milk datatype (e.g. `UINT16`), or `UNKNOWN`.
    std::string datatype_name() const;

    /// Name passed to open/create (empty if never opened).
    const std::string& name() const;

    /// Compact log string: `name RxC DTYPE`.
    std::string describe() const;

    void write_scaled(const Array2D<double>& data, double scale);
    void zero();

private:
    struct Impl;
    Impl* impl_;
};

} // namespace lina
