#pragma once

#include "lina/array.h"
#include "lina/coro_utils.h"
#include "lina/stream.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace lina {

/// Thrown when a stop check returns true mid calibrate/run/grab.
struct Cancelled : public std::runtime_error {
    Cancelled() : std::runtime_error("operation cancelled") {}
};

using StopCheck = std::function<bool()>;

inline bool stop_requested(const StopCheck& stop) {
    return static_cast<bool>(stop) && stop();
}

inline void throw_if_stopped(const StopCheck& stop) {
    if (stop_requested(stop))
        throw Cancelled();
}

struct IefcData {
    std::vector<Array2D<double>> raw_images;
    std::vector<Array2D<double>> dark_images;
    std::vector<Array2D<double>> ni_images;
    std::vector<double> contrasts;
    std::vector<Array2D<double>> commands;
    std::vector<Array2D<double>> del_commands;
    // Per-iteration: count of (raw - dark) < 0 over the full frame / control mask.
    std::vector<std::size_t> n_negative_dark_sub;
    std::vector<std::size_t> n_negative_in_mask;
    std::vector<std::size_t> n_pixels;
    std::vector<std::size_t> n_mask_pixels;
    // Pixels used in the contrast mean (mask ∩ NI > 0); ≤0 are excluded.
    std::vector<std::size_t> n_contrast_positive;
};

std::vector<Array2D<double>> measure_probe_response(Stream2D& camsci,
                                                    std::size_t ncamsci,
                                                    Stream2D& dm,
                                                    const ImParams& im_params,
                                                    const ImParams& ref_params,
                                                    const Array2D<double>& probe_modes,
                                                    double probe_amplitude,
                                                    double delay_s = 0.01,
                                                    double dm_scale = 1e-6,
                                                    const Array2D<double>* dark_im = nullptr,
                                                    std::size_t wait_frames = 1,
                                                    const StopCheck& stop = {});

struct CalibrateResult {
    Array2D<double> response_masked; // (nmodes, nprobes * nmask)
    Array2D<double> response_full;   // (nmodes, nprobes * npix); empty if keep_full=false
};

/// Build IEFC response matrix. Optional progress(mode_1based, nmodes) is called
/// at the start of each calibration mode (and once with 0 before the loop).
/// dark_im is ignored: calibration responses are difference images, so no dark
/// subtraction is applied (Imax/exp/gain normalization still is).
/// If keep_full_response is false (default), response_full is left empty — full-frame
/// storage is O(nmodes*nprobes*npix) and can be many GB for large camsci.
/// Throws Cancelled if stop() returns true; DM restored to entry command.
CalibrateResult calibrate(Stream2D& camsci,
                          std::size_t ncamsci,
                          Stream2D& dm,
                          const ImParams& im_params,
                          const ImParams& ref_params,
                          const Array2D<std::uint8_t>& control_mask,
                          double probe_amplitude,
                          const Array2D<double>& probe_modes,
                          double calibration_amplitude,
                          const Array2D<double>& calibration_modes,
                          double delay_s = 0.01,
                          double dm_scale = 1e-6,
                          const Array2D<double>* dark_im = nullptr,
                          std::size_t wait_frames = 1,
                          const std::function<void(std::size_t, std::size_t)>& progress = {},
                          bool keep_full_response = false,
                          const StopCheck& stop = {});

void run(IefcData& iefc_data,
         Stream2D& camsci,
         std::size_t ncamsci,
         Stream2D& dm,
         const ImParams& im_params,
         const ImParams& ref_params,
         const Array2D<double>& dark_im,
         const Array2D<double>& control_matrix,
         double probe_amplitude,
         const Array2D<double>& probe_modes,
         const Array2D<double>& calib_modes,
         const Array2D<std::uint8_t>& control_mask,
         double delay_s = 0.01,
         std::size_t num_iterations = 3,
         double gain = 0.75,
         double leakage = 0.0,
         double dm_scale = 1e-6,
         std::size_t wait_frames = 1,
         const StopCheck& stop = {});

} // namespace lina
