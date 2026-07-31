#pragma once

#include "lina/array.h"
#include "lina/stream.h"

#include <cstddef>
#include <functional>
#include <string>

namespace lina {

class ShmimStream : public Stream2D {
public:
    ShmimStream();
    explicit ShmimStream(const std::string& name);
    ~ShmimStream() override;

    void open(const std::string& name);
    void create(const std::string& name,
                std::size_t rows,
                std::size_t cols,
                int datatype = 9, // _DATATYPE_FLOAT
                std::size_t cbsize = 1);
    void close();

    Array2D<double> grab_latest() override;
    Array2D<double> grab_mean(std::size_t nframes, std::size_t wait_frames = 0,
                              const std::function<bool()>& stop = {}) override;
    void write(const Array2D<double>& data) override;

    std::size_t rows() const override;
    std::size_t cols() const override;

    void write_scaled(const Array2D<double>& data, double scale);
    void zero();

private:
    struct Impl;
    Impl* impl_;
};

} // namespace lina
