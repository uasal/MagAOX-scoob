#include "lina/iefc_package.h"
#include "lina/dm.h"
#include "lina/linalg.h"
#include "lina/utils.h"

#include <sys/stat.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

#if defined(LINA_USE_EIGEN_SVD) || defined(LINA_USE_EIGEN)
#include <Eigen/Dense>
#endif

namespace lina {
namespace {

std::string join_path(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    if (dir.back() == '/') return dir + name;
    return dir + "/" + name;
}

void ensure_dir(const std::string& dir) {
    if (dir.empty()) return;
    if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
        throw std::runtime_error("mkdir failed: " + dir);
    }
}

FitsCube modes_to_cube(const Array2D<double>& modes, std::size_t nact) {
    if (modes.cols() != nact * nact)
        throw std::runtime_error("modes second dim must be nact*nact");
    return FitsCube{nact, nact, modes.rows(), modes};
}

Array2D<double> cube_to_modes(const FitsCube& cube) {
    if (cube.rows != cube.cols) throw std::runtime_error("mode planes must be square");
    if (cube.data.rows() != cube.nplanes ||
        cube.data.cols() != cube.rows * cube.cols)
        throw std::runtime_error("cube data shape mismatch");
    return cube.data;
}

void write_modes_fits(const std::string& path, const Array2D<double>& modes,
                      std::size_t nact, const char* kind) {
    auto cube = modes_to_cube(modes, nact);
    FitsHeader hdr = {{"KIND", std::string("'") + kind + "'"},
                      {"NACT", std::to_string(nact)},
                      {"NMODES", std::to_string(cube.nplanes)}};
    save_fits_cube(path, cube, hdr, true);
    std::cout << "wrote " << path << " (" << cube.nplanes << "x" << nact << "x" << nact
              << ")\n";
}

double image_mean(const Array2D<double>& im) {
    double s = 0.0;
    for (std::size_t i = 0; i < im.size(); ++i) s += im.data()[i];
    return im.size() ? s / static_cast<double>(im.size()) : 0.0;
}

double image_max(const Array2D<double>& im) {
    double m = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < im.size(); ++i) m = std::max(m, im.data()[i]);
    return m;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void save_matrix_bin(const std::string& path, const Array2D<double>& m) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("failed to write " + path);
    const std::uint64_t rows = m.rows(), cols = m.cols();
    out.write(reinterpret_cast<const char*>(&rows), sizeof(rows));
    out.write(reinterpret_cast<const char*>(&cols), sizeof(cols));
    out.write(reinterpret_cast<const char*>(m.data()),
              static_cast<std::streamsize>(m.size() * sizeof(double)));
}

Array2D<double> load_matrix_bin(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("failed to read " + path);
    std::uint64_t rows = 0, cols = 0;
    in.read(reinterpret_cast<char*>(&rows), sizeof(rows));
    in.read(reinterpret_cast<char*>(&cols), sizeof(cols));
    Array2D<double> m(static_cast<std::size_t>(rows),
                      static_cast<std::size_t>(cols), 0.0);
    in.read(reinterpret_cast<char*>(m.data()),
            static_cast<std::streamsize>(m.size() * sizeof(double)));
    if (!in) throw std::runtime_error("truncated matrix file: " + path);
    return m;
}

FitsCube response_full_to_cube(const Array2D<double>& response_full, std::size_t ncam) {
    const std::size_t npix = ncam * ncam;
    if (response_full.cols() % npix != 0) {
        throw std::runtime_error("response_full cols not divisible by ncam*ncam");
    }
    const std::size_t nprobes = response_full.cols() / npix;
    const std::size_t nmodes = response_full.rows();
    FitsCube cube;
    cube.rows = ncam;
    cube.cols = ncam;
    cube.nplanes = nmodes * nprobes;
    cube.data = Array2D<double>(cube.nplanes, npix, 0.0);
    for (std::size_t i = 0; i < nmodes; ++i) {
        for (std::size_t p = 0; p < nprobes; ++p) {
            const std::size_t plane = i * nprobes + p;
            for (std::size_t idx = 0; idx < npix; ++idx) {
                cube.data(plane, idx) = response_full(i, p * npix + idx);
            }
        }
    }
    return cube;
}

void log_matrix_size(const std::string& label, const Array2D<double>& m) {
    const std::size_t bytes = m.size() * sizeof(double);
    std::cout << label << ": " << m.rows() << " x " << m.cols()
              << " (~" << bytes << " bytes)\n";
}

} // namespace

Array2D<double> cube_to_response_full(const FitsCube& cube,
                                      std::size_t nmodes,
                                      std::size_t nprobes) {
    if (nmodes == 0 || nprobes == 0)
        throw std::runtime_error("cube_to_response_full: nmodes/nprobes must be >0");
    if (cube.nplanes != nmodes * nprobes) {
        throw std::runtime_error(
            "cube_to_response_full: nplanes (" + std::to_string(cube.nplanes) +
            ") != nmodes*nprobes (" + std::to_string(nmodes * nprobes) + ")");
    }
    const std::size_t npix = static_cast<std::size_t>(cube.rows) *
                             static_cast<std::size_t>(cube.cols);
    if (cube.data.cols() != npix && cube.data.size() != cube.nplanes * npix) {
        // FitsCube stores planes as rows of data with cols=npix
    }
    if (cube.data.rows() != cube.nplanes || cube.data.cols() != npix) {
        throw std::runtime_error("cube_to_response_full: unexpected cube data layout");
    }
    Array2D<double> out(nmodes, nprobes * npix, 0.0);
    for (std::size_t i = 0; i < nmodes; ++i) {
        for (std::size_t p = 0; p < nprobes; ++p) {
            const std::size_t plane = i * nprobes + p;
            for (std::size_t idx = 0; idx < npix; ++idx) {
                out(i, p * npix + idx) = cube.data(plane, idx);
            }
        }
    }
    return out;
}

Array2D<double> load_response_full(const PackagePaths& pkg,
                                   std::size_t ncam,
                                   std::size_t nmodes,
                                   std::size_t nprobes) {
    const auto cube = load_fits_cube(pkg.response_full_path());
    if (cube.rows != ncam || cube.cols != ncam) {
        throw std::runtime_error(
            "response_full.fits ncam mismatch: got " + std::to_string(cube.rows) + "x" +
            std::to_string(cube.cols) + ", expected " + std::to_string(ncam) + "x" +
            std::to_string(ncam));
    }
    return cube_to_response_full(cube, nmodes, nprobes);
}

std::string PackagePaths::response_path() const { return join_path(dir, response); }
std::string PackagePaths::response_full_path() const { return join_path(dir, response_full); }
std::string PackagePaths::control_path() const { return join_path(dir, control); }
std::string PackagePaths::control_reg_tag( double reg_cond )
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision( 6 ) << reg_cond;
    return oss.str();
}
std::string PackagePaths::control_path_for_reg( double reg_cond ) const
{
    return join_path( dir, "control_matrix_reg_" + control_reg_tag( reg_cond ) + ".fits" );
}
std::string PackagePaths::probes_path() const { return join_path(dir, probes); }
std::string PackagePaths::calib_modes_path() const { return join_path(dir, calib_modes); }
std::string PackagePaths::wfs_mask_path() const { return join_path(dir, wfs_mask); }
std::string PackagePaths::config_path() const { return join_path(dir, config); }
std::string PackagePaths::dark_path() const { return join_path(dir, dark); }

void clear_control_reg_files( const std::string &dir )
{
    if( dir.empty() )
        return;
    DIR *d = opendir( dir.c_str() );
    if( !d )
        return;
    const std::string prefix = "control_matrix_reg_";
    const std::string suffix = ".fits";
    while( dirent *ent = readdir( d ) )
    {
        const std::string name = ent->d_name;
        if( name.size() <= prefix.size() + suffix.size() )
            continue;
        if( name.compare( 0, prefix.size(), prefix ) != 0 )
            continue;
        if( name.compare( name.size() - suffix.size(), suffix.size(), suffix ) != 0 )
            continue;
        const std::string path = join_path( dir, name );
        if( ::unlink( path.c_str() ) == 0 )
            std::cout << "removed stale " << path << "\n";
    }
    closedir( d );
}

Array2D<double> transpose(const Array2D<double>& a) {
    Array2D<double> out(a.cols(), a.rows(), 0.0);
    for (std::size_t r = 0; r < a.rows(); ++r)
        for (std::size_t c = 0; c < a.cols(); ++c) out(c, r) = a(r, c);
    return out;
}

Array2D<double> beta_reg_cpu(const Array2D<double>& S, double beta) {
    // S is (nmeas, nmodes). Build regularized Gram matrix G = S^T S + reg*I
    // (nmodes x nmodes), then control = G^{-1} S^T via Cholesky.
    //
    // IMPORTANT: do not use Eigen JacobiSVD here — for nmodes~1024 it can take
    // many minutes. Cholesky matches the Python beta_reg path and is O(n^3/3).
    auto sts = gemm(S, S, true, false);
    if (sts.rows() == 0 || sts.cols() == 0) {
        return Array2D<double>();
    }
    double alpha2 = 0.0;
    for (std::size_t i = 0; i < sts.rows() && i < sts.cols(); ++i)
        alpha2 = std::max(alpha2, sts(i, i));
    const double reg = alpha2 * std::pow(10.0, beta);
    for (std::size_t i = 0; i < sts.rows() && i < sts.cols(); ++i) sts(i, i) += reg;

#if defined(LINA_USE_EIGEN_SVD) || defined(LINA_USE_EIGEN)
    using Mat = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    Eigen::Map<Mat> G(sts.data(), static_cast<int>(sts.rows()), static_cast<int>(sts.cols()));
    Eigen::LLT<Mat> llt(G);
    if (llt.info() != Eigen::Success) {
        throw std::runtime_error("beta_reg_cpu: Cholesky failed (matrix not SPD?)");
    }
    // Solve G * X = S^T  => X is (nmodes, nmeas) = control
    Eigen::Map<const Mat> Smap(S.data(), static_cast<int>(S.rows()), static_cast<int>(S.cols()));
    Mat X = llt.solve(Smap.transpose());
    Array2D<double> control(static_cast<std::size_t>(X.rows()),
                            static_cast<std::size_t>(X.cols()), 0.0);
    for (int r = 0; r < X.rows(); ++r)
        for (int c = 0; c < X.cols(); ++c)
            control(static_cast<std::size_t>(r), static_cast<std::size_t>(c)) = X(r, c);
    return control;
#else
    // Fallback: thin SVD pseudo-inverse (slower; prefer Eigen Cholesky builds).
    const auto svd = svd_thin(sts);
    const std::size_t n = sts.rows();
    const std::size_t r = svd.s.size();
    const double smax = r ? *std::max_element(svd.s.begin(), svd.s.end()) : 0.0;
    const double cutoff = std::numeric_limits<double>::epsilon() *
                          static_cast<double>(std::max(sts.rows(), sts.cols())) * smax;
    Array2D<double> M(r, n, 0.0);
    for (std::size_t i = 0; i < r; ++i) {
        const double coeff = (svd.s[i] > cutoff) ? (1.0 / svd.s[i]) : 0.0;
        if (coeff == 0.0) continue;
        for (std::size_t col = 0; col < n; ++col) M(i, col) = coeff * svd.u(col, i);
    }
    const auto inv = gemm(svd.vt, M, true, false);
    return gemm(inv, S, false, true);
#endif
}

void save_matrix(const std::string& path, const Array2D<double>& m,
                 const FitsHeader& header) {
    if (ends_with(path, ".bin")) {
        save_matrix_bin(path, m);
        return;
    }
    save_fits(path, m, header, true);
}

Array2D<double> load_matrix(const std::string& path) {
    if (ends_with(path, ".bin")) {
        return load_matrix_bin(path);
    }
    if (ends_with(path, ".fits")) {
        try {
            return load_fits_double(path);
        } catch (const std::exception&) {
            const std::string bin_path =
                path.substr(0, path.size() - 5) + ".bin";
            return load_matrix_bin(bin_path);
        }
    }
    try {
        return load_fits_double(path);
    } catch (const std::exception&) {
        return load_matrix_bin(path);
    }
}

void write_config(const std::string& path, const ConfigMap& cfg) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("failed to write " + path);
    out << "# iefc calibration package\n";
    for (const auto& kv : cfg) out << kv.first << "=" << kv.second << "\n";
}

ConfigMap read_config(const std::string& path) {
    ConfigMap cfg;
    std::ifstream in(path);
    if (!in) throw std::runtime_error("failed to read " + path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        cfg[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return cfg;
}

double cfg_d(const ConfigMap& cfg, const char* key, double fallback) {
    auto it = cfg.find(key);
    return it == cfg.end() ? fallback : std::atof(it->second.c_str());
}

std::size_t cfg_z(const ConfigMap& cfg, const char* key, std::size_t fallback) {
    auto it = cfg.find(key);
    return it == cfg.end() ? fallback : static_cast<std::size_t>(std::atoi(it->second.c_str()));
}

std::string cfg_s(const ConfigMap& cfg, const char* key, const std::string& fallback) {
    auto it = cfg.find(key);
    return it == cfg.end() ? fallback : it->second;
}

LoopInputs default_loop_inputs(std::size_t ncam, std::size_t nact) {
    LoopInputs in;
    in.ncam = ncam;
    in.nact = nact;
    return in;
}

std::vector<DarkLibraryEntry> load_dark_library_manifest(const std::string& lib_dir) {
    const std::string path = join_path(lib_dir, "dark_library.txt");
    std::ifstream in(path);
    if (!in) return {};
    std::vector<DarkLibraryEntry> entries;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        DarkLibraryEntry e;
        if (!(ss >> e.exptime >> e.relpath)) continue;
        // Optional v2 columns (ignored by older writers / partial lines).
        if (ss >> e.ndark) {
            if (ss >> e.cam_name) {
                if (e.cam_name == "-") e.cam_name.clear();
                unsigned w = 0, h = 0, bd = 0;
                int rx = 0, ry = 0;
                if (ss >> w >> h) {
                    e.width = w;
                    e.height = h;
                    if (ss >> bd) e.bitdepth = bd;
                    if (ss >> rx >> ry) {
                        e.roi_x = rx;
                        e.roi_y = ry;
                    }
                    // v3: roi_width roi_height gain blacklevel
                    // v2: gain blacklevel
                    double a = std::numeric_limits<double>::quiet_NaN();
                    double b = std::numeric_limits<double>::quiet_NaN();
                    double c = std::numeric_limits<double>::quiet_NaN();
                    double d = std::numeric_limits<double>::quiet_NaN();
                    if (ss >> a >> b) {
                        if (ss >> c >> d) {
                            e.roi_width = static_cast<unsigned>( a );
                            e.roi_height = static_cast<unsigned>( b );
                            e.gain = c;
                            e.blacklevel = d;
                        } else {
                            e.gain = a;
                            e.blacklevel = b;
                        }
                    }
                }
            }
        }
        entries.push_back(e);
    }
    return entries;
}

std::vector<DarkLibraryEntry>
filter_dark_library_entries(const std::vector<DarkLibraryEntry>& entries,
                            const DarkMatchFilter& filter) {
    std::vector<DarkLibraryEntry> out;
    out.reserve(entries.size());
    for (const auto& e : entries) {
        if (!filter.cam_name.empty() && !e.cam_name.empty() && e.cam_name != filter.cam_name)
            continue;
        // Legacy v1 rows (empty cam_name) still match any filter.cam_name.
        if (filter.width > 0 && e.width > 0 && e.width != filter.width) continue;
        if (filter.height > 0 && e.height > 0 && e.height != filter.height) continue;
        if (std::isfinite(filter.gain) && std::isfinite(e.gain) &&
            std::fabs(e.gain - filter.gain) > filter.gain_tol)
            continue;
        if (std::isfinite(filter.blacklevel) && std::isfinite(e.blacklevel) &&
            std::fabs(e.blacklevel - filter.blacklevel) > filter.blacklevel_tol)
            continue;
        out.push_back(e);
    }
    return out;
}

std::string pick_dark_from_library(const std::string& lib_dir, double target_exptime,
                                   const DarkMatchFilter& filter, DarkLibraryEntry* matched,
                                   double* match_err) {
    auto entries = filter_dark_library_entries(load_dark_library_manifest(lib_dir), filter);
    if (entries.empty()) return {};
    std::size_t best = 0;
    double best_err = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const double err = std::fabs(entries[i].exptime - target_exptime);
        if (err < best_err) {
            best_err = err;
            best = i;
        }
    }
    if (matched) *matched = entries[best];
    if (match_err) *match_err = best_err;
    std::cout << "dark library: " << entries.size() << " matching entries; target exptime="
              << target_exptime << " -> closest " << entries[best].exptime
              << " (err=" << best_err << " s, " << entries[best].relpath << ")\n";
    if (best_err > kDarkExptimeMatchTol) {
        std::cerr << "warning: nearest dark exptime " << entries[best].exptime
                  << " differs by " << best_err << " s from requested " << target_exptime
                  << " (tol=" << kDarkExptimeMatchTol << " s)\n";
    }
    return join_path(lib_dir, entries[best].relpath);
}

void write_dark_library_manifest(const std::string& lib_dir,
                                 const std::vector<DarkLibraryEntry>& entries) {
    ensure_dir(lib_dir);
    const std::string path = join_path(lib_dir, "dark_library.txt");
    std::ofstream out(path);
    if (!out) throw std::runtime_error("failed to write " + path);
    out << "# dark_library_format=3\n";
    out << "# exptime relative_path ndark cam_name width height bitdepth roi_x roi_y "
           "roi_width roi_height gain blacklevel\n";
    out << std::setprecision(17);
    for (const auto& e : entries) {
        out << e.exptime << "  " << e.relpath << "  " << e.ndark << "  "
            << (e.cam_name.empty() ? "-" : e.cam_name) << "  " << e.width << "  " << e.height
            << "  " << e.bitdepth << "  " << e.roi_x << "  " << e.roi_y << "  "
            << e.roi_width << "  " << e.roi_height << "  ";
        if (std::isfinite(e.gain))
            out << e.gain;
        else
            out << "nan";
        out << "  ";
        if (std::isfinite(e.blacklevel))
            out << e.blacklevel;
        else
            out << "nan";
        out << "\n";
    }
}

SetupData load_setup_dir(const std::string& dir, std::size_t expect_ncam,
                         double target_exptime, const std::string& dark_lib_path,
                         const DarkMatchFilter& dark_filter) {
    if (dir.empty()) return {};
    SetupData s;
    s.loaded = true;
    s.dir = dir;

    const std::string cfg_path = join_path(dir, "config.txt");
    ConfigMap cfg;
    try {
        cfg = read_config(cfg_path);
    } catch (const std::exception& e) {
        throw std::runtime_error("setupdir missing config.txt (" + std::string(e.what()) +
                                 "): " + dir);
    }

    s.psf_exptime = cfg_d(cfg, "cam_exp", cfg_d(cfg, "cal_psf_exp",
                        cfg_d(cfg, "psf_exptime", cfg_d(cfg, "exptime", 1.0))));
    s.gain = cfg_d(cfg, "cam_gain", cfg_d(cfg, "cal_psf_gain",
                   cfg_d(cfg, "psf_gain", cfg_d(cfg, "gain", 0.0))));

    std::string dark_path;
    double matched_t = -1.0;
    double match_err = -1.0;
    DarkLibraryEntry matched;
    DarkMatchFilter filter = dark_filter;
    if (!std::isfinite(filter.gain) && std::isfinite(s.gain))
        filter.gain = s.gain;

    if (target_exptime > 0.0) {
        if (!dark_lib_path.empty()) {
            dark_path = pick_dark_from_library(dark_lib_path, target_exptime, filter,
                                              &matched, &match_err);
        }
        if (dark_path.empty()) {
            // Legacy: dark_library.txt inside the ref-PSF / setup directory.
            dark_path =
                pick_dark_from_library(dir, target_exptime, filter, &matched, &match_err);
        }
        if (!dark_path.empty())
            matched_t = matched.exptime;
    }
    if (dark_path.empty()) {
        const std::string dark_name = cfg_s(cfg, "dark_file", "dark_avg.fits");
        dark_path = join_path(dir, dark_name);
        matched_t = s.psf_exptime;
        match_err = -1.0;
        s.dark_from_library = false;
        std::cout << "using single setup dark: " << dark_path << "\n";
    } else {
        s.dark_from_library = true;
        s.dark_match_err = match_err;
    }
    s.dark = load_fits_double(dark_path);
    s.dark_path_used = dark_path;
    s.dark_exptime = matched_t > 0 ? matched_t : s.psf_exptime;

    if (expect_ncam && (s.dark.rows() != expect_ncam || s.dark.cols() != expect_ncam)) {
        throw std::runtime_error(
            "setup dark size " + std::to_string(s.dark.rows()) + "x" +
            std::to_string(s.dark.cols()) + " != camsci " + std::to_string(expect_ncam) +
            "x" + std::to_string(expect_ncam));
    }

    const std::string psf_sub_name =
        cfg_s(cfg, "ref_psf_dark_sub_file", "ref_psf_dark_sub.fits");
    s.Imax_ref = cfg_d(cfg, "psf_max_ref", cfg_d(cfg, "Imax_ref", cfg_d(cfg, "peak_dark_sub", 0.0)));
    if (!(s.Imax_ref > 0.0)) {
        const auto psf_sub = load_fits_double(join_path(dir, psf_sub_name));
        s.Imax_ref = image_max(psf_sub);
    }
    if (!(s.Imax_ref > 0.0)) {
        throw std::runtime_error(
            "setupdir psf_max_ref/Imax_ref <= 0; check config.txt / ref_psf_dark_sub.fits in " + dir);
    }

    std::cout << "setupdir=" << dir << "\n"
              << "  dark        = " << s.dark_path_used << "  mean=" << image_mean(s.dark)
              << "  (dark exptime=" << s.dark_exptime << ")\n"
              << "  psf_max_ref = " << s.Imax_ref << "  (psf exptime=" << s.psf_exptime
              << ")\n"
              << "  gain        = " << s.gain << "\n";
    return s;
}

SetupData load_setup_from_package(const PackagePaths& pkg, std::size_t expect_ncam,
                                  double target_exptime, const std::string& dark_lib_path,
                                  const DarkMatchFilter& dark_filter) {
    const auto cfg = read_config(pkg.config_path());
    const double psf_exptime =
        cfg_d(cfg, "cam_exp", cfg_d(cfg, "cal_psf_exp",
              cfg_d(cfg, "psf_exptime", cfg_d(cfg, "exptime", 1.0))));
    const double psf_gain =
        cfg_d(cfg, "cam_gain", cfg_d(cfg, "cal_psf_gain",
              cfg_d(cfg, "psf_gain", cfg_d(cfg, "gain", 0.0))));
    const double imax = cfg_d(cfg, "psf_max_ref", cfg_d(cfg, "Imax_ref", 0.0));
    const std::string setupdir = cfg_s(cfg, "setupdir", "");

    // Prefer dedicated dark library when provided.
    if (!dark_lib_path.empty() && target_exptime > 0.0) {
        try {
            DarkMatchFilter filter = dark_filter;
            if (!std::isfinite(filter.gain)) filter.gain = psf_gain;
            DarkLibraryEntry matched;
            double match_err = -1.0;
            const std::string dark_path = pick_dark_from_library(
                dark_lib_path, target_exptime, filter, &matched, &match_err);
            if (!dark_path.empty()) {
                SetupData s;
                s.loaded = true;
                s.dir = setupdir.empty() ? pkg.dir : setupdir;
                s.dark = load_fits_double(dark_path);
                s.dark_path_used = dark_path;
                s.dark_exptime = matched.exptime;
                s.dark_from_library = true;
                s.dark_match_err = match_err;
                s.Imax_ref = imax > 0.0 ? imax : 1.0;
                s.psf_exptime = psf_exptime;
                s.gain = psf_gain;
                if (expect_ncam &&
                    (s.dark.rows() != expect_ncam || s.dark.cols() != expect_ncam)) {
                    throw std::runtime_error("dark library size mismatch vs camsci");
                }
                return s;
            }
        } catch (const std::exception& e) {
            std::cerr << "warning: dark_lib_path load failed (" << e.what()
                      << "); falling back\n";
        }
    }

    if (!setupdir.empty() && target_exptime > 0.0) {
        try {
            SetupData s =
                load_setup_dir(setupdir, expect_ncam, target_exptime, dark_lib_path, dark_filter);
            if (imax > 0.0) s.Imax_ref = imax;
            s.psf_exptime = psf_exptime;
            s.gain = psf_gain;
            return s;
        } catch (const std::exception& e) {
            std::cerr << "warning: could not load dark library from setupdir (" << e.what()
                      << "); falling back to package dark\n";
        }
    }

    SetupData s;
    s.Imax_ref = imax;
    s.psf_exptime = psf_exptime;
    s.gain = psf_gain;
    s.dir = setupdir.empty() ? pkg.dir : setupdir;
    try {
        s.dark = load_fits_double(pkg.dark_path());
        s.dark_path_used = pkg.dark_path();
        s.dark_exptime = psf_exptime;
        s.loaded = true;
    } catch (...) {
        if (!setupdir.empty()) {
            s = load_setup_dir(setupdir, expect_ncam, target_exptime, dark_lib_path, dark_filter);
            s.psf_exptime = psf_exptime;
            if (imax > 0.0) s.Imax_ref = imax;
            s.gain = psf_gain;
            return s;
        }
    }
    if (!s.loaded) return {};
    if (!(s.Imax_ref > 0.0)) {
        std::cerr << "warning: calib package has dark but psf_max_ref/Imax_ref missing/<=0\n";
        s.Imax_ref = 1.0;
    }
    if (expect_ncam && (s.dark.rows() != expect_ncam || s.dark.cols() != expect_ncam)) {
        throw std::runtime_error("package dark size mismatch vs camsci");
    }
    std::cout << "loaded setup from calib package: psf_max_ref=" << s.Imax_ref
              << " dark mean=" << image_mean(s.dark) << "\n";
    return s;
}

void apply_setup(LoopInputs& in, const SetupData& s, double live_exptime) {
    if (!s.loaded) return;
    in.ref_params.Imax = s.Imax_ref;
    in.ref_params.exp_time = s.psf_exptime;
    in.ref_params.gain = s.gain;
    in.im_params.exp_time = (live_exptime > 0.0) ? live_exptime : s.dark_exptime;
    in.im_params.gain = s.gain;
    in.im_params.Imax = s.Imax_ref;
}

void generate_modes(LoopInputs& in, const std::string& calib_modes_fits_override) {
    const auto dm_mask = create_mask(in.nact);

    in.probe_modes = create_fourier_probes(
        dm_mask, in.ncam, in.pxscl, in.fourier_iwa, in.fourier_owa, in.fourier_rot, 0.75, 2);
    std::cout << "probe_modes: " << in.probe_modes.rows() << " x " << in.nact << "x"
              << in.nact << "\n";

    if (!calib_modes_fits_override.empty()) {
        const auto cube = load_fits_cube(calib_modes_fits_override);
        if (cube.rows != in.nact || cube.cols != in.nact)
            throw std::runtime_error("modes FITS size mismatch vs DM");
        in.calib_modes = cube_to_modes(cube);
        std::cout << "loaded calib_modes from " << calib_modes_fits_override << " ("
                  << in.calib_modes.rows() << " modes)\n";
    } else {
        in.calib_modes = create_hadamard_modes(dm_mask);
        std::cout << "calib_modes: " << in.calib_modes.rows() << " Hadamard modes\n";
    }

    in.control_mask = create_annular_focal_plane_mask(
        in.ncam, in.pxscl, in.dh_iwa, in.dh_owa,
        in.dh_iwa,
        "odd",
        in.dh_rot);
    std::size_t nmask = 0;
    for (std::size_t i = 0; i < in.control_mask.size(); ++i) {
        if (in.control_mask.data()[i]) ++nmask;
    }
    std::cout << "wfs_mask: half-annulus iwa=" << in.dh_iwa << " owa=" << in.dh_owa
              << " edge=" << in.dh_iwa << " rot=" << in.dh_rot
              << " pixels=" << nmask << "\n";
}

void load_modes_from_package(LoopInputs& in, const PackagePaths& pkg,
                             const std::string& probes_override,
                             const std::string& calib_override,
                             const std::string& mask_override) {
    const std::string probes_path =
        probes_override.empty() ? pkg.probes_path() : probes_override;
    const std::string calib_path =
        calib_override.empty() ? pkg.calib_modes_path() : calib_override;
    const std::string mask_path =
        mask_override.empty() ? pkg.wfs_mask_path() : mask_override;

    if (probes_path.size() >= 5 &&
        probes_path.substr(probes_path.size() - 5) == ".fits") {
        const auto cube = load_fits_cube(probes_path);
        if (cube.rows != in.nact || cube.cols != in.nact)
            throw std::runtime_error("probe FITS size mismatch");
        in.probe_modes = cube_to_modes(cube);
    } else {
        in.probe_modes = load_matrix(probes_path);
    }
    std::cout << "loaded probes " << in.probe_modes.rows() << " from " << probes_path
              << "\n";

    if (calib_path.size() >= 5 && calib_path.substr(calib_path.size() - 5) == ".fits") {
        const auto cube = load_fits_cube(calib_path);
        if (cube.rows != in.nact || cube.cols != in.nact)
            throw std::runtime_error("calib FITS size mismatch");
        in.calib_modes = cube_to_modes(cube);
    } else {
        in.calib_modes = load_matrix(calib_path);
    }
    std::cout << "loaded calib_modes " << in.calib_modes.rows() << " from " << calib_path
              << "\n";

    if (mask_path.size() >= 5 && mask_path.substr(mask_path.size() - 5) == ".fits") {
        auto m = load_fits_double(mask_path);
        if (m.rows() != in.ncam || m.cols() != in.ncam) {
            throw std::runtime_error(
                "wfs_mask size " + std::to_string(m.rows()) + "x" +
                std::to_string(m.cols()) + " != camsci " + std::to_string(in.ncam) +
                "x" + std::to_string(in.ncam));
        }
        in.control_mask = Array2D<std::uint8_t>(m.rows(), m.cols(), 0);
        for (std::size_t i = 0; i < m.size(); ++i)
            in.control_mask.data()[i] = m.data()[i] > 0.5 ? 1 : 0;
    } else {
        throw std::runtime_error("wfs_mask must be a .fits file: " + mask_path);
    }
    std::cout << "loaded wfs_mask from " << mask_path << "\n";
}

void save_package(const PackagePaths& pkg, const LoopInputs& in,
                  const Array2D<double>& response_masked,
                  const Array2D<double>& control,
                  const SetupData& setup,
                  const Array2D<double>* response_full) {
    ensure_dir(pkg.dir);

    // New response invalidates any prior per-reg control matrices in this package.
    clear_control_reg_files( pkg.dir );

    const std::size_t nmodes = in.calib_modes.rows();
    const std::size_t nprobes = in.probe_modes.rows();
    const std::size_t nmeas = response_masked.cols();
    std::size_t nmask = 0;
    for (std::size_t i = 0; i < in.control_mask.size(); ++i) {
        if (in.control_mask.data()[i]) ++nmask;
    }

    save_matrix(
        pkg.response_path(), response_masked,
        {{"KIND", "'response_masked'"},
         {"NMODES", std::to_string(nmodes)},
         {"NPROBES", std::to_string(nprobes)},
         {"NMASK", std::to_string(nmask)},
         {"NMEAS", std::to_string(nmeas)},
         {"RESPONSE_LAYOUT", "'nmodes_nmeas'"}});
    log_matrix_size("response_masked", response_masked);

    const FitsHeader control_hdr =
        {{"KIND", "'control_matrix'"},
         {"NMODES", std::to_string(nmodes)},
         {"NPROBES", std::to_string(nprobes)},
         {"NMEAS", std::to_string(nmeas)},
         {"REGCOND", PackagePaths::control_reg_tag( in.reg_cond )},
         {"RESPONSE_LAYOUT", "'nmodes_nmeas'"}};

    // Canonical per-reg file + legacy control_matrix.fits alias for this reg.
    const std::string tagged = pkg.control_path_for_reg( in.reg_cond );
    save_matrix( tagged, control, control_hdr );
    save_matrix( pkg.control_path(), control, control_hdr );
    log_matrix_size("control_matrix", control);
    std::cout << "wrote " << tagged << " (reg_cond=" << in.reg_cond << ")\n";

    if (response_full) {
        const auto cube = response_full_to_cube(*response_full, in.ncam);
        FitsHeader hdr = {{"KIND", "'response_full'"},
                          {"NMODES", std::to_string(nmodes)},
                          {"NPROBES", std::to_string(nprobes)},
                          {"NCAM", std::to_string(in.ncam)}};
        save_fits_cube(pkg.response_full_path(), cube, hdr, true);
        const std::size_t bytes = cube.data.size() * sizeof(double);
        std::cout << "response_full: " << cube.nplanes << " x " << cube.rows << " x "
                  << cube.cols << " (~" << bytes << " bytes)\n";
    }
    write_modes_fits(pkg.probes_path(), in.probe_modes, in.nact, "fourier_probes");
    write_modes_fits(pkg.calib_modes_path(), in.calib_modes, in.nact, "hadamard");

    Array2D<double> mask_f(in.control_mask.rows(), in.control_mask.cols(), 0.0);
    for (std::size_t i = 0; i < mask_f.size(); ++i)
        mask_f.data()[i] = in.control_mask.data()[i] ? 1.0 : 0.0;
    save_fits(pkg.wfs_mask_path(), mask_f, {{"KIND", "'wfs_mask'"}}, true);
    std::cout << "wrote " << pkg.wfs_mask_path() << "\n";

    if (setup.loaded) {
        save_fits(pkg.dark_path(), setup.dark,
                  {{"KIND", "'DARK'"}, {"SOURCE", "'setupdir'"}}, true);
        std::cout << "wrote " << pkg.dark_path() << " (from setupdir)\n";
    }

    ConfigMap cfg;
    cfg["nact"] = std::to_string(in.nact);
    cfg["ncamsci"] = std::to_string(in.ncam);
    cfg["nframes"] = std::to_string(in.nframes);
    cfg["wait_frames"] = std::to_string(in.wait_frames);
    cfg["delay_s"] = std::to_string(in.delay_s);
    cfg["calib_probe_amp"] = std::to_string(in.calib_probe_amp);
    cfg["run_probe_amp"] = std::to_string(in.run_probe_amp);
    cfg["calib_amp"] = std::to_string(in.calib_amp);
    cfg["reg_cond"] = std::to_string(in.reg_cond);
    cfg["pxscl_lamd"] = std::to_string(in.pxscl);
    cfg["dh_iwa"] = std::to_string(in.dh_iwa);
    cfg["dh_owa"] = std::to_string(in.dh_owa);
    cfg["dh_edge"] = std::to_string(in.dh_iwa);
    cfg["dh_rotation"] = std::to_string(in.dh_rot);
    cfg["nprobes"] = std::to_string(in.probe_modes.rows());
    cfg["nmodes"] = std::to_string(in.calib_modes.rows());
    cfg["psf_max_ref"] = std::to_string(in.ref_params.Imax);
    cfg["Imax_ref"] = std::to_string(in.ref_params.Imax); // legacy
    cfg["psf_exptime"] = std::to_string(in.ref_params.exp_time);
    cfg["psf_gain"] = std::to_string(in.ref_params.gain);
    cfg["format"] = "fits";
    cfg["response_file"] = pkg.response;
    cfg["response_full_file"] = pkg.response_full;
    cfg["control_file"] = "control_matrix_reg_" + PackagePaths::control_reg_tag( in.reg_cond ) + ".fits";
    cfg["control_file_legacy"] = pkg.control;
    if (setup.loaded) {
        cfg["setupdir"] = setup.dir;
        cfg["dark_file"] = pkg.dark;
    }
    cfg["response_layout"] = "nmodes_nmeas";
    write_config(pkg.config_path(), cfg);
    std::cout << "wrote " << pkg.config_path() << "\n";
    std::cout << "wrote " << pkg.response_path() << "\n";
    std::cout << "wrote " << pkg.control_path() << " (legacy alias)\n";
    if (response_full) {
        std::cout << "wrote " << pkg.response_full_path() << "\n";
    }
}

} // namespace lina
