#include "lina/iefc.h"
#include "lina/coro_utils.h"
#include "lina/linalg.h"
#include "lina/utils.h"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace lina {
namespace {

std::vector<std::size_t> mask_indices(const Array2D<std::uint8_t>& mask) {
    std::vector<std::size_t> indices;
    indices.reserve(mask.size());
    for (std::size_t i = 0; i < mask.size(); ++i) {
        if (mask.data()[i]) {
            indices.push_back(i);
        }
    }
    return indices;
}

void sleep_interruptible(double delay_s, const StopCheck& stop) {
    if (delay_s <= 0.0) {
        throw_if_stopped(stop);
        return;
    }
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::duration<double>(delay_s);
    while (std::chrono::steady_clock::now() < deadline) {
        throw_if_stopped(stop);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    throw_if_stopped(stop);
}

void restore_dm_meters(Stream2D& dm, const Array2D<double>& command_m, double dm_scale) {
    Array2D<double> write_cmd = command_m;
    for (std::size_t idx = 0; idx < write_cmd.size(); ++idx) {
        write_cmd.data()[idx] /= dm_scale;
    }
    dm.write(write_cmd);
}

} // namespace

void check_saturation(const Array2D<double>& raw,
                      const Array2D<std::uint8_t>& sat_mask,
                      double sat_thresh) {
    if (sat_mask.size() == 0 || !(sat_thresh > 0.0))
        return;
    if (sat_mask.size() != raw.size()) {
        throw std::runtime_error("sat_mask size does not match camera image");
    }
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (sat_mask.data()[i] && raw.data()[i] >= sat_thresh) {
            throw std::runtime_error(
                "saturation detected in sat_mask region (raw>=" +
                std::to_string(sat_thresh) + " ADU at linear index " +
                std::to_string(i) + ")");
        }
    }
}

Array2D<double> mask_response_full(const Array2D<double>& response_full,
                                   const Array2D<std::uint8_t>& mask,
                                   std::size_t nprobes) {
    if (nprobes == 0)
        throw std::runtime_error("mask_response_full: nprobes==0");
    if (mask.size() == 0)
        throw std::runtime_error("mask_response_full: empty mask");
    const std::size_t npix = mask.size();
    if (response_full.cols() != nprobes * npix) {
        throw std::runtime_error(
            "mask_response_full: response_full cols (" +
            std::to_string(response_full.cols()) + ") != nprobes*npix (" +
            std::to_string(nprobes * npix) + ")");
    }
    const std::vector<std::size_t> mask_idx = mask_indices(mask);
    if (mask_idx.empty())
        throw std::runtime_error("mask_response_full: mask has no positive pixels");
    const std::size_t nmask = mask_idx.size();
    const std::size_t nmodes = response_full.rows();
    Array2D<double> out(nmodes, nprobes * nmask, 0.0);
    for (std::size_t i = 0; i < nmodes; ++i) {
        for (std::size_t p = 0; p < nprobes; ++p) {
            for (std::size_t k = 0; k < nmask; ++k) {
                out(i, p * nmask + k) =
                    response_full(i, p * npix + mask_idx[k]);
            }
        }
    }
    return out;
}

std::vector<Array2D<double>> measure_probe_response(Stream2D& camsci,
                                                    std::size_t ncamsci,
                                                    Stream2D& dm,
                                                    const ImParams& im_params,
                                                    const ImParams& ref_params,
                                                    const Array2D<double>& probe_modes,
                                                    double probe_amplitude,
                                                    double delay_s,
                                                    double dm_scale,
                                                    const Array2D<double>* dark_im,
                                                    std::size_t wait_frames,
                                                    const StopCheck& stop,
                                                    const Array2D<std::uint8_t>* sat_mask,
                                                    double sat_thresh) {
    const std::size_t nprobes = probe_modes.rows();
    const std::size_t nact = static_cast<std::size_t>(std::sqrt(probe_modes.cols()));

    Array2D<double> current_command = dm.grab_latest();
    for (std::size_t j = 0; j < current_command.size(); ++j) {
        current_command.data()[j] *= dm_scale;
    }

    std::vector<Array2D<double>> responses;
    responses.reserve(nprobes);

    try {
        for (std::size_t i = 0; i < nprobes; ++i) {
            throw_if_stopped(stop);
            Array2D<double> probe(nact, nact, 0.0);
            for (std::size_t r = 0; r < nact; ++r) {
                for (std::size_t c = 0; c < nact; ++c) {
                    probe(r, c) = probe_amplitude * probe_modes(i, r * nact + c);
                }
            }

            Array2D<double> cmd_pos(nact, nact, 0.0);
            Array2D<double> cmd_neg(nact, nact, 0.0);
            for (std::size_t r = 0; r < nact; ++r) {
                for (std::size_t c = 0; c < nact; ++c) {
                    cmd_pos(r, c) = current_command(r, c) + probe(r, c);
                    cmd_neg(r, c) = current_command(r, c) - probe(r, c);
                }
            }

            Array2D<double> write_pos = cmd_pos;
            for (std::size_t idx = 0; idx < write_pos.size(); ++idx) {
                write_pos.data()[idx] /= dm_scale;
            }
            dm.write(write_pos);
            sleep_interruptible(delay_s, stop);
            Array2D<double> im_pos = camsci.grab_mean(ncamsci, wait_frames, stop);
            if (sat_mask)
                check_saturation(im_pos, *sat_mask, sat_thresh);

            Array2D<double> write_neg = cmd_neg;
            for (std::size_t idx = 0; idx < write_neg.size(); ++idx) {
                write_neg.data()[idx] /= dm_scale;
            }
            dm.write(write_neg);
            sleep_interruptible(delay_s, stop);
            Array2D<double> im_neg = camsci.grab_mean(ncamsci, wait_frames, stop);
            if (sat_mask)
                check_saturation(im_neg, *sat_mask, sat_thresh);

            // Flux-normalize each frame, then form the differential probe response.
            // Dark cancels in (pos-neg); callers may still pass dark_im (e.g. closed loop)
            // but calibration intentionally skips it. Imax/exp/gain scaling does not cancel.
            Array2D<double> im_pos_ni;
            Array2D<double> im_neg_ni;
            if (dark_im) {
                im_pos_ni = normalize_coro_im(im_pos, im_params, ref_params, *dark_im);
                im_neg_ni = normalize_coro_im(im_neg, im_params, ref_params, *dark_im);
            } else {
                im_pos_ni = normalize_coro_im(im_pos, im_params, ref_params, 0.0);
                im_neg_ni = normalize_coro_im(im_neg, im_params, ref_params, 0.0);
            }

            Array2D<double> diff_ni(im_pos_ni.rows(), im_pos_ni.cols(), 0.0);
            for (std::size_t idx = 0; idx < diff_ni.size(); ++idx) {
                diff_ni.data()[idx] =
                    (im_pos_ni.data()[idx] - im_neg_ni.data()[idx]) / (2.0 * probe_amplitude);
            }
            responses.push_back(diff_ni);
        }
    } catch (...) {
        restore_dm_meters(dm, current_command, dm_scale);
        throw;
    }

    restore_dm_meters(dm, current_command, dm_scale);
    return responses;
}

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
                          double delay_s,
                          double dm_scale,
                          const Array2D<double>* dark_im,
                          std::size_t wait_frames,
                          const std::function<void(std::size_t, std::size_t)>& progress,
                          bool keep_full_response,
                          const StopCheck& stop,
                          const Array2D<std::uint8_t>* sat_mask,
                          double sat_thresh) {
    const std::size_t nact = static_cast<std::size_t>(std::sqrt(probe_modes.cols()));
    const std::size_t nprobes = probe_modes.rows();
    const std::size_t nmodes = calibration_modes.rows();

    const std::vector<std::size_t> mask_idx = mask_indices(control_mask);
    const std::size_t nmask = mask_idx.size();

    Array2D<double> current_command = dm.grab_latest();
    for (std::size_t j = 0; j < current_command.size(); ++j) {
        current_command.data()[j] *= dm_scale;
    }

    const std::size_t image_size = control_mask.size();
    Array2D<double> response_matrix(nmodes, nprobes * nmask, 0.0);
    Array2D<double> response_full_matrix;
    if (keep_full_response) {
        response_full_matrix = Array2D<double>(nmodes, nprobes * image_size, 0.0);
    }

    if (progress) {
        progress(0, nmodes);
    }

    try {
        for (std::size_t i = 0; i < nmodes; ++i) {
            throw_if_stopped(stop);
            if (progress) {
                progress(i + 1, nmodes);
            }
            Array2D<double> response(nprobes, image_size, 0.0);
            for (int s : {-1, 1}) {
                throw_if_stopped(stop);
                Array2D<double> calib_mode(nact, nact, 0.0);
                for (std::size_t r = 0; r < nact; ++r) {
                    for (std::size_t c = 0; c < nact; ++c) {
                        calib_mode(r, c) =
                            calibration_amplitude * calibration_modes(i, r * nact + c);
                    }
                }

                Array2D<double> cmd(nact, nact, 0.0);
                for (std::size_t r = 0; r < nact; ++r) {
                    for (std::size_t c = 0; c < nact; ++c) {
                        cmd(r, c) = current_command(r, c) + s * calib_mode(r, c);
                    }
                }
                Array2D<double> write_cmd = cmd;
                for (std::size_t idx = 0; idx < write_cmd.size(); ++idx) {
                    write_cmd.data()[idx] /= dm_scale;
                }
                dm.write(write_cmd);
                sleep_interruptible(delay_s, stop);

                // No dark subtraction during calibration: responses are difference images.
                (void)dark_im;
                const auto probed = measure_probe_response(
                    camsci, ncamsci, dm, im_params, ref_params, probe_modes, probe_amplitude,
                    delay_s, dm_scale, /*dark_im=*/nullptr, wait_frames, stop, sat_mask,
                    sat_thresh);

                for (std::size_t p = 0; p < nprobes; ++p) {
                    if (probed[p].size() != image_size) {
                        throw std::runtime_error(
                            "probe response image size does not match control_mask");
                    }
                    for (std::size_t idx = 0; idx < image_size; ++idx) {
                        response(p, idx) += static_cast<double>(s) * probed[p].data()[idx] /
                                            (2.0 * calibration_amplitude);
                    }
                }
            }
            restore_dm_meters(dm, current_command, dm_scale);

            for (std::size_t p = 0; p < nprobes; ++p) {
                for (std::size_t k = 0; k < nmask; ++k) {
                    response_matrix(i, p * nmask + k) = response(p, mask_idx[k]);
                }
                if (keep_full_response) {
                    for (std::size_t idx = 0; idx < image_size; ++idx) {
                        response_full_matrix(i, p * image_size + idx) = response(p, idx);
                    }
                }
            }
        }
    } catch (...) {
        restore_dm_meters(dm, current_command, dm_scale);
        throw;
    }

    return {response_matrix, response_full_matrix};
}

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
         double delay_s,
         std::size_t num_iterations,
         double gain,
         double leakage,
         double dm_scale,
         std::size_t wait_frames,
         const StopCheck& stop) {
    const std::size_t nact = static_cast<std::size_t>(std::sqrt(probe_modes.cols()));
    const std::size_t nmodes = calib_modes.rows();
    const std::vector<std::size_t> mask_idx = mask_indices(control_mask);
    const std::size_t nmask = mask_idx.size();

    Array2D<double> modal_matrix(nact * nact, nmodes, 0.0);
    for (std::size_t m = 0; m < nmodes; ++m) {
        for (std::size_t r = 0; r < nact; ++r) {
            for (std::size_t c = 0; c < nact; ++c) {
                modal_matrix(r * nact + c, m) = calib_modes(m, r * nact + c);
            }
        }
    }

    Array2D<double> total_command(nact, nact, 0.0);
    if (!iefc_data.commands.empty()) {
        total_command = iefc_data.commands.back();
    }

    try {
        for (std::size_t i = 0; i < num_iterations; ++i) {
            throw_if_stopped(stop);
            const auto diff_ims = measure_probe_response(
                camsci, ncamsci, dm, im_params, ref_params, probe_modes, probe_amplitude,
                delay_s, dm_scale, &dark_im, wait_frames, stop);

            std::vector<double> measurement_vector;
            measurement_vector.reserve(diff_ims.size() * nmask);
            for (const auto& im : diff_ims) {
                for (std::size_t k = 0; k < nmask; ++k) {
                    measurement_vector.push_back(im.data()[mask_idx[k]]);
                }
            }

            std::vector<double> modal_coeff = gemv(control_matrix, measurement_vector);
            for (double& v : modal_coeff) {
                v = -v;
            }
            std::vector<double> del_command_vec = gemv(modal_matrix, modal_coeff);

            Array2D<double> del_command(nact, nact, 0.0);
            for (std::size_t r = 0; r < nact; ++r) {
                for (std::size_t c = 0; c < nact; ++c) {
                    del_command(r, c) = gain * del_command_vec[r * nact + c];
                }
            }

            for (std::size_t idx = 0; idx < total_command.size(); ++idx) {
                total_command.data()[idx] = (1.0 - leakage) * total_command.data()[idx] +
                                             del_command.data()[idx];
            }

            Array2D<double> write_cmd = total_command;
            for (std::size_t idx = 0; idx < write_cmd.size(); ++idx) {
                write_cmd.data()[idx] /= dm_scale;
            }
            dm.write(write_cmd);
            sleep_interruptible(delay_s, stop);

            const Array2D<double> coro_im = camsci.grab_mean(ncamsci, wait_frames, stop);
            const Array2D<double> coro_im_ni =
                normalize_coro_im(coro_im, im_params, ref_params, dark_im);
            const ContrastResult contrast = compute_contrast(coro_im_ni, control_mask);

            std::size_t n_neg = 0;
            std::size_t n_neg_mask = 0;
            std::size_t n_mask = 0;
            for (std::size_t idx = 0; idx < coro_im.size(); ++idx) {
                const double ds = coro_im.data()[idx] - dark_im.data()[idx];
                if (ds < 0.0) ++n_neg;
            }
            for (std::size_t k = 0; k < mask_idx.size(); ++k) {
                const std::size_t idx = mask_idx[k];
                ++n_mask;
                const double ds = coro_im.data()[idx] - dark_im.data()[idx];
                if (ds < 0.0) ++n_neg_mask;
            }

            iefc_data.raw_images.push_back(coro_im);
            iefc_data.dark_images.push_back(dark_im);
            iefc_data.ni_images.push_back(coro_im_ni);
            iefc_data.contrasts.push_back(contrast.contrast);
            iefc_data.commands.push_back(total_command);
            iefc_data.del_commands.push_back(del_command);
            iefc_data.n_negative_dark_sub.push_back(n_neg);
            iefc_data.n_negative_in_mask.push_back(n_neg_mask);
            iefc_data.n_pixels.push_back(coro_im.size());
            iefc_data.n_mask_pixels.push_back(n_mask);
            iefc_data.n_contrast_positive.push_back(contrast.n_positive);
        }
    } catch (const Cancelled&) {
        // Leave the last applied command (safer mid-loop than reverting all CL work),
        // but ensure DM is in a defined published state.
        restore_dm_meters(dm, total_command, dm_scale);
        throw;
    }
}

} // namespace lina
