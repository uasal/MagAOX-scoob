#include "lina/shmim_utils.h"
#include "lina/iefc.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <vector>

#ifdef LINA_USE_IMAGESTREAMIO
#include <ImageStreamIO/ImageStreamIO.h>
#endif

namespace lina {

struct ShmimStream::Impl {
#ifdef LINA_USE_IMAGESTREAMIO
    IMAGE image{};
    bool open = false;
    int sem_index = -1;
#endif
    std::size_t rows = 0;
    std::size_t cols = 0;
};

ShmimStream::ShmimStream() : impl_(new Impl()) {}

ShmimStream::ShmimStream(const std::string& name) : ShmimStream() {
    open(name);
}

ShmimStream::~ShmimStream() {
    close();
    delete impl_;
}

void ShmimStream::open(const std::string& name) {
#ifndef LINA_USE_IMAGESTREAMIO
    throw std::runtime_error("ImageStreamIO not enabled");
#else
    if (impl_->open) {
        close();
    }
    if (ImageStreamIO_openIm(&impl_->image, name.c_str()) != IMAGESTREAMIO_SUCCESS) {
        throw std::runtime_error("ImageStreamIO_openIm failed");
    }
    impl_->open = true;
    impl_->sem_index = -1;
    impl_->rows = impl_->image.md->size[0];
    impl_->cols = impl_->image.md->size[1];
#endif
}

void ShmimStream::create(const std::string& name,
                         std::size_t rows,
                         std::size_t cols,
                         int datatype,
                         std::size_t cbsize) {
#ifndef LINA_USE_IMAGESTREAMIO
    throw std::runtime_error("ImageStreamIO not enabled");
#else
    if (impl_->open) {
        close();
    }
    uint32_t sizes[2] = {static_cast<uint32_t>(rows), static_cast<uint32_t>(cols)};
    if (ImageStreamIO_createIm(&impl_->image, name.c_str(), 2, sizes,
                               static_cast<uint8_t>(datatype), 1, 8,
                               static_cast<int>(cbsize)) != IMAGESTREAMIO_SUCCESS) {
        throw std::runtime_error("ImageStreamIO_createIm failed");
    }
    impl_->open = true;
    impl_->sem_index = -1;
    impl_->rows = rows;
    impl_->cols = cols;
#endif
}

void ShmimStream::close() {
#ifdef LINA_USE_IMAGESTREAMIO
    if (impl_->open) {
        ImageStreamIO_closeIm(&impl_->image);
        impl_->open = false;
    }
#endif
}

std::size_t ShmimStream::rows() const {
    return impl_->rows;
}

std::size_t ShmimStream::cols() const {
    return impl_->cols;
}

Array2D<double> ShmimStream::grab_latest() {
#ifndef LINA_USE_IMAGESTREAMIO
    throw std::runtime_error("ImageStreamIO not enabled");
#else
    if (!impl_->open) {
        throw std::runtime_error("ShmimStream not open");
    }
    void* buffer = nullptr;
    if (ImageStreamIO_readLastWroteBuffer(&impl_->image, &buffer) != IMAGESTREAMIO_SUCCESS) {
        throw std::runtime_error("ImageStreamIO_readLastWroteBuffer failed");
    }

    const std::size_t n = impl_->rows * impl_->cols;
    Array2D<double> out(impl_->rows, impl_->cols, 0.0);
    if (impl_->image.md->datatype == _DATATYPE_FLOAT) {
        const float* src = static_cast<const float*>(buffer);
        for (std::size_t i = 0; i < n; ++i) {
            out.data()[i] = static_cast<double>(src[i]);
        }
    } else if (impl_->image.md->datatype == _DATATYPE_DOUBLE) {
        const double* src = static_cast<const double*>(buffer);
        std::memcpy(out.data(), src, sizeof(double) * n);
    } else {
        throw std::runtime_error("Unsupported shmim datatype");
    }
    return out;
#endif
}

namespace {

#ifdef LINA_USE_IMAGESTREAMIO
void copy_buffer_to_array(IMAGE* image, void* buffer, Array2D<double>& out) {
    const std::size_t n = out.size();
    if (image->md->datatype == _DATATYPE_FLOAT) {
        const float* src = static_cast<const float*>(buffer);
        for (std::size_t i = 0; i < n; ++i) out.data()[i] = static_cast<double>(src[i]);
    } else if (image->md->datatype == _DATATYPE_DOUBLE) {
        const double* src = static_cast<const double*>(buffer);
        std::memcpy(out.data(), src, sizeof(double) * n);
    } else {
        throw std::runtime_error("Unsupported shmim datatype");
    }
}

int ensure_sem_index(IMAGE* image, int* cached) {
    if (*cached >= 0 && *cached < image->md->sem) return *cached;
    const int sem = ImageStreamIO_getsemwaitindex(image, 0);
    if (sem < 0) {
        throw std::runtime_error(
            "ShmimStream: no free ImageStreamIO semaphore (is the stream missing sems?)");
    }
    *cached = sem;
    return sem;
}

bool sem_wait_timeout(IMAGE* image, int sem, double timeout_s) {
    timespec ts{};
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return ImageStreamIO_semwait(image, sem) == 0;
    }
    const time_t whole = static_cast<time_t>(timeout_s);
    const long nsec = static_cast<long>((timeout_s - static_cast<double>(whole)) * 1e9);
    ts.tv_sec += whole;
    ts.tv_nsec += nsec;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    return ImageStreamIO_semtimedwait(image, sem, &ts) == 0;
}
#endif

} // namespace

Array2D<double> ShmimStream::grab_mean(std::size_t nframes, std::size_t wait_frames,
                                       const std::function<bool()>& stop) {
#ifndef LINA_USE_IMAGESTREAMIO
    (void)wait_frames;
    (void)stop;
    throw std::runtime_error("ImageStreamIO not enabled");
#else
    if (!impl_->open) {
        throw std::runtime_error("ShmimStream not open");
    }
    if (nframes == 0) {
        throw std::invalid_argument("nframes must be > 0");
    }

    const std::size_t rows = impl_->rows;
    const std::size_t cols = impl_->cols;
    const std::size_t n = rows * cols;
    Array2D<double> mean(rows, cols, 0.0);
    Array2D<double> frame(rows, cols, 0.0);

    // Prefer semaphore-synced new frames (magpyx grab_many / grab_after).
    // Fallback: average the most recent circular-buffer slices if no sems.
    if (impl_->image.md->sem > 0) {
        const int sem = ensure_sem_index(&impl_->image, &impl_->sem_index);
        ImageStreamIO_semflush(&impl_->image, sem);

        const uint64_t cnt0_min = impl_->image.md->cnt0 + static_cast<uint64_t>(wait_frames);
        constexpr double k_slice_s = 0.2; // short so stop is responsive
        constexpr double k_total_timeout_s = 30.0;
        const auto t0 = std::chrono::steady_clock::now();
        std::size_t collected = 0;
        while (collected < nframes) {
            throw_if_stopped(stop);
            const auto elapsed = std::chrono::duration<double>(
                                     std::chrono::steady_clock::now() - t0)
                                     .count();
            if (elapsed > k_total_timeout_s) {
                throw std::runtime_error(
                    "ShmimStream::grab_mean timed out waiting for a new frame "
                    "(camsci not updating?)");
            }
            if (!sem_wait_timeout(&impl_->image, sem, k_slice_s)) {
                continue;
            }
            if (impl_->image.md->cnt0 < cnt0_min) continue;

            void* buffer = nullptr;
            if (ImageStreamIO_readLastWroteBuffer(&impl_->image, &buffer) !=
                IMAGESTREAMIO_SUCCESS) {
                throw std::runtime_error("ImageStreamIO_readLastWroteBuffer failed");
            }
            copy_buffer_to_array(&impl_->image, buffer, frame);
            for (std::size_t j = 0; j < n; ++j) mean.data()[j] += frame.data()[j];
            ++collected;
        }
    } else {
        // No semaphores: best-effort historical mean (legacy path).
        throw_if_stopped(stop);
        if (wait_frames > 0) {
            // Approximate settle with a short sleep (~wait_frames * 10 ms).
            // Prefer creating the stream with semaphores for RT use.
            timespec req{};
            req.tv_sec = 0;
            req.tv_nsec = static_cast<long>(wait_frames) * 10000000L;
            nanosleep(&req, nullptr);
        }
        const uint64_t n_slices = ImageStreamIO_nbSlices(&impl_->image);
        const uint64_t last_index = ImageStreamIO_readLastWroteIndex(&impl_->image);
        const std::size_t frames = std::min<std::size_t>(nframes, n_slices);
        for (std::size_t i = 0; i < frames; ++i) {
            throw_if_stopped(stop);
            const uint64_t idx = (last_index + n_slices - i) % n_slices;
            void* buffer = nullptr;
            if (ImageStreamIO_readBufferAt(&impl_->image, static_cast<unsigned int>(idx),
                                          &buffer) != IMAGESTREAMIO_SUCCESS) {
                throw std::runtime_error("ImageStreamIO_readBufferAt failed");
            }
            copy_buffer_to_array(&impl_->image, buffer, frame);
            for (std::size_t j = 0; j < n; ++j) mean.data()[j] += frame.data()[j];
        }
        nframes = frames;
    }

    const double inv = 1.0 / static_cast<double>(nframes);
    for (std::size_t j = 0; j < n; ++j) mean.data()[j] *= inv;
    return mean;
#endif
}

void ShmimStream::write(const Array2D<double>& data) {
    write_scaled(data, 1.0);
}

void ShmimStream::write_scaled(const Array2D<double>& data, double scale) {
#ifndef LINA_USE_IMAGESTREAMIO
    throw std::runtime_error("ImageStreamIO not enabled");
#else
    if (!impl_->open) {
        throw std::runtime_error("ShmimStream not open");
    }
    if (data.rows() != impl_->rows || data.cols() != impl_->cols) {
        throw std::invalid_argument("write size mismatch");
    }
    // MagAO-X / cacao DMcomb only reacts to md->cnt0 changes. A bare
    // ImageStreamIO_sempost does NOT bump cnt0, so channel writes never
    // appear in dmXXdisp. ImageStreamIO_UpdateIm increments cnt0 and posts
    // semaphores (the correct producer publish).
    impl_->image.md->write = 1;
    void* buffer = nullptr;
    if (ImageStreamIO_writeBuffer(&impl_->image, &buffer) != IMAGESTREAMIO_SUCCESS) {
        impl_->image.md->write = 0;
        throw std::runtime_error("ImageStreamIO_writeBuffer failed");
    }
    const std::size_t n = impl_->rows * impl_->cols;
    if (impl_->image.md->datatype == _DATATYPE_FLOAT) {
        float* dst = static_cast<float*>(buffer);
        for (std::size_t i = 0; i < n; ++i) {
            dst[i] = static_cast<float>(data.data()[i] * scale);
        }
    } else if (impl_->image.md->datatype == _DATATYPE_DOUBLE) {
        double* dst = static_cast<double*>(buffer);
        for (std::size_t i = 0; i < n; ++i) {
            dst[i] = data.data()[i] * scale;
        }
    } else {
        impl_->image.md->write = 0;
        throw std::runtime_error("Unsupported shmim datatype");
    }
    if (ImageStreamIO_UpdateIm(&impl_->image) != IMAGESTREAMIO_SUCCESS) {
        throw std::runtime_error("ImageStreamIO_UpdateIm failed");
    }
#endif
}

void ShmimStream::zero() {
    Array2D<double> zeros(rows(), cols(), 0.0);
    write(zeros);
}

} // namespace lina
