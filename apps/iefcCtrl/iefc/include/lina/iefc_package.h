#pragma once

#include "lina/array.h"
#include "lina/coro_utils.h"
#include "lina/utils.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace lina {

struct LoopInputs {
    Array2D<double> probe_modes;
    Array2D<double> calib_modes;
    Array2D<std::uint8_t> control_mask;
    ImParams im_params{1.0, 0.0, 0.0, 1.0};
    ImParams ref_params{1.0, 0.0, 0.0, 1.0};
    double calib_probe_amp = 5e-9;
    double run_probe_amp = 1e-9;
    double calib_amp = 2e-9;
    double dm_scale = 1e-6;
    double delay_s = 0.05;
    std::size_t wait_frames = 1;
    double reg_cond = -2.5;
    double gain = 1.0;
    double leakage = 0.0;
    double pxscl = 0.169;
    double dh_iwa = 3.0;
    double dh_owa = 10.0;
    double fourier_iwa = 2.0;
    double fourier_owa = 14.0;
    double fourier_rot = 90.0;
    double dh_rot = 90.0;
    std::size_t nframes = 5;
    std::size_t num_iters = 3;
    std::size_t nact = 34;
    std::size_t ncam = 0;
};

struct PackagePaths {
    std::string dir;
    std::string response = "response_matrix.fits";
    std::string response_full = "response_full.fits";
    std::string control = "control_matrix.fits";
    std::string probes = "probe_modes.fits";
    std::string calib_modes = "calib_modes.fits";
    std::string wfs_mask = "wfs_mask.fits";
    std::string config = "config.txt";
    std::string dark = "dark_avg.fits";

    std::string response_path() const;
    std::string response_full_path() const;
    std::string control_path() const;
    /// Per-reg control matrix: control_matrix_reg_<±X.XXXXXX>.fits
    std::string control_path_for_reg( double reg_cond ) const;
    std::string probes_path() const;
    std::string calib_modes_path() const;
    std::string wfs_mask_path() const;
    std::string config_path() const;
    std::string dark_path() const;

    /// Stable filename tag for a regularization value (fixed 6 decimals).
    static std::string control_reg_tag( double reg_cond );
};

/// Remove control_matrix_reg_*.fits under dir (call when response is replaced).
void clear_control_reg_files( const std::string &dir );

struct SetupData {
    bool loaded = false;
    std::string dir;
    Array2D<double> dark;
    double Imax_ref = 1.0;
    double dark_exptime = 1.0;
    double psf_exptime = 1.0;
    double gain = 0.0;
    std::string dark_path_used;
    /// |requested live exptime - library entry exptime| for the chosen dark.
    double dark_match_err = -1.0;
    bool dark_from_library = false;
};

/// Absolute exptime tolerance [s] for library match warnings / refresh.
inline constexpr double kDarkExptimeMatchTol = 1e-4;

using ConfigMap = std::map<std::string, std::string>;

Array2D<double> transpose(const Array2D<double>& a);
Array2D<double> beta_reg_cpu(const Array2D<double>& S, double beta);
void save_matrix(const std::string& path, const Array2D<double>& m,
                 const FitsHeader& header = {});
Array2D<double> load_matrix(const std::string& path);

/// Inverse of response_full_to_cube packing: planes are (mode, probe) major.
Array2D<double> cube_to_response_full(const FitsCube& cube,
                                      std::size_t nmodes,
                                      std::size_t nprobes);

/// Load dir_cal/response_full.fits into (nmodes, nprobes*ncam*ncam).
Array2D<double> load_response_full(const PackagePaths& pkg,
                                   std::size_t ncam,
                                   std::size_t nmodes,
                                   std::size_t nprobes);

void write_config(const std::string& path, const ConfigMap& cfg);
ConfigMap read_config(const std::string& path);
double cfg_d(const ConfigMap& cfg, const char* key, double fallback);
std::size_t cfg_z(const ConfigMap& cfg, const char* key, std::size_t fallback);
std::string cfg_s(const ConfigMap& cfg, const char* key, const std::string& fallback);

LoopInputs default_loop_inputs(std::size_t ncam, std::size_t nact);

SetupData load_setup_dir(const std::string& dir,
                         std::size_t expect_ncam = 0,
                         double target_exptime = -1.0);

SetupData load_setup_from_package(const PackagePaths& pkg,
                                  std::size_t expect_ncam,
                                  double target_exptime = -1.0);

void apply_setup(LoopInputs& in, const SetupData& s, double live_exptime = -1.0);

void generate_modes(LoopInputs& in, const std::string& calib_modes_fits_override = {});

void load_modes_from_package(LoopInputs& in,
                             const PackagePaths& pkg,
                             const std::string& probes_override = {},
                             const std::string& calib_override = {},
                             const std::string& mask_override = {});

void save_package(const PackagePaths& pkg,
                  const LoopInputs& in,
                  const Array2D<double>& response_masked,
                  const Array2D<double>& control,
                  const SetupData& setup = {},
                  const Array2D<double>* response_full = nullptr);

} // namespace lina
