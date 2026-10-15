/** \file centroidCtrl.hpp
 * \brief MagAO-X app for PSF centroiding from a camera shmim.
 *
 * \ingroup centroidCtrl_files
 */

#ifndef centroidCtrl_hpp
#define centroidCtrl_hpp

#include <mx/improc/eigenImage.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <mutex>
#include <vector>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

/** \defgroup centroidCtrl
 * \brief MagAO-X app to compute PSF (x,y) from an ImageStreamIO camera stream.
 *
 * <a href="../handbook/operating/software/apps/centroidCtrl.html">Application Documentation</a>
 *
 * \ingroup apps
 */

/** \defgroup centroidCtrl_files
 * \ingroup centroidCtrl
 */

namespace MagAOX
{
namespace app
{

/// MagAO-X application for PSF centroid calculation
/** Monitors an input camera shmim via ImageStreamIO.  On each new frame,
 * detects stars and publishes a selected centroid as INDI `x.current` /
 * `y.current`, with validity and diagnostics in `detection`.
 *
 * \ingroup centroidCtrl
 */
class centroidCtrl : public MagAOXApp<true>, public dev::shmimMonitor<centroidCtrl>
{
    // Give the test harness access.
    friend class centroidCtrl_test;

    friend class dev::shmimMonitor<centroidCtrl>;

  public:
    /// The base shmimMonitor type
    typedef dev::shmimMonitor<centroidCtrl> shmimMonitorT;

    /// Floating point type used for centroid calculations
    typedef float realT;

  protected:
    /** \name Configurable Parameters
     *@{
     */

    std::string m_camName; ///< Input ImageStreamIO shmim name (`/tmp/<cam_name>.im.shm`)

    uint32_t m_camWidth{ 0 };  ///< Expected camera width [pixels]. 0 = accept stream size.
    uint32_t m_camHeight{ 0 }; ///< Expected camera height [pixels]. 0 = accept stream size.

    ///@}

    mx::improc::eigenImage<realT> m_image; ///< Working copy of the latest camera frame

    realT m_x{ 0 }; ///< Latest centroid x [pixels]
    realT m_y{ 0 }; ///< Latest centroid y [pixels]

    uint8_t m_dataType{ 0 }; ///< ImageStreamIO datatype of the connected stream

    // All controls use INDI Number/current/target. Integer-valued controls
    // are checked explicitly; changing settings takes effect on the next frame.
    struct Settings
    {
        double method{0};          ///< 0 = windowed COM, 1 = Gaussian correlation peak
        double selection{0};       ///< 0 = nearest expected position, 1 = strongest correlation
        double expected_x{-1};     ///< -1 = geometric ROI center, independently per axis
        double expected_y{-1};
        double search_radius{0};   ///< Pixels; 0 = unrestricted candidate selection
        double window{16};         ///< Exact square side length, including even sizes
        double psf_sigma{1.5};      ///< Gaussian template sigma in pixels (FWHM = 2.355 sigma)
        double plate_scale_mas{10}; ///< Milliarcseconds per pixel in the CURRENT stream/ROI
        double fwhm_mas{0};         ///< Angular PSF FWHM; 0 = use psf_sigma directly
        double detect_sigma{5};     ///< Threshold in filtered background-noise standard deviations
        double noise_floor{1};      ///< Minimum background-noise estimate, in input ADU
        double ignore_sigma{2};     ///< COM/support pixel threshold in background sigma
        double ignore_adu{0};       ///< Additional COM/support threshold above background, ADU
        double min_peak{0};         ///< Minimum peak above background, ADU
        double min_flux{0};         ///< Minimum thresholded window flux, ADU
        double min_pixels{3};       ///< Minimum above-threshold raw pixels in the seed's 3x3 core
        double separation{8};       ///< Suppress weaker candidates within this many pixels
        double saturation{0};       ///< Raw ADU ceiling; 0 = disabled; reject affected windows
    };

    struct Parameter
    {
        const char *name;
        double Settings::*value;
        double minimum, maximum;
        bool integer;
        const char *description;
    };
    static const size_t nParameters = 18;
    static const std::array<Parameter, nParameters> &parameters();
    static double effectiveSigma(const Settings &s);
    static bool validSettings(const Settings &s);
    Settings m_settings;
    std::mutex m_settingsMutex; ///< Never held while publishing INDI
    uint64_t m_settingsVersion{0}; ///< Prevent publishing a frame processed with obsolete settings
    bool m_configValid{true};
    std::array<pcf::IndiProperty, nParameters> m_indiP_settings;
    static int st_newSettings(void *app, const pcf::IndiProperty &ipRecv);
    int newSettings(const pcf::IndiProperty &ipRecv);

    struct Measurement
    {
        double background{0}, noise{0}, flux{0}, peak{0}, snr{0};
        double seed_x{-1}, seed_y{-1};
        size_t candidates{0};
    };
    Measurement m_measurement; ///< Owned by the shmim processing thread
    pcf::IndiProperty m_indiP_detection; ///< valid, candidates, background, noise, flux, peak, snr, seed_x/y

    struct Candidate { int x, y; double score, flux, peak; };
    // Reused scratch storage; calculateCentroid must have only one caller at a time.
    std::vector<double> m_work, m_horizontal, m_correlation, m_kernel;
    std::vector<Candidate> m_candidates, m_stars;
    static double median(std::vector<double> &values);
    int measure(realT &x, realT &y, const realT *image, uint32_t width, uint32_t height,
                const Settings &s);

  public:
    /// Default c'tor.
    centroidCtrl();

    /// D'tor, declared and defined for noexcept.
    ~centroidCtrl() noexcept
    {
    }

    virtual void setupConfig();

    /// Implementation of loadConfig logic, separated for testing.
    /** This is called by loadConfig().
     */
    int loadConfigImpl(
        mx::app::appConfigurator &_config /**< [in] an application configuration from which to load values*/ );

    virtual void loadConfig();

    /// Startup function
    /**
     *
     */
    virtual int appStartup();

    /// Implementation of the FSM for centroidCtrl.
    /**
     * \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
    virtual int appLogic();

    /// Shutdown the app.
    /**
     *
     */
    virtual int appShutdown();

    /** \name shmimMonitor Interface
     * @{
     */

    /// Called after connecting to the shared memory image.
    int allocate( const dev::shmimT & /**< [in] tag to differentiate shmimMonitor parents.*/ );

    /// Called for each new image on the input stream.
    int processImage( void *curr_src,     ///< [in] pointer to the start of the current frame
                      const dev::shmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    /// Compute the PSF centroid from a camera image.
    /**
     * Detect candidates with Gaussian correlation, select one star, then measure a local centroid.
     *
     * \param[out] x      centroid x coordinate in pixels (0-based, along width)
     * \param[out] y      centroid y coordinate in pixels (0-based, along height)
     * \param[in]  image  contiguous row-major image data, size width*height
     * \param[in]  width  image width in pixels
     * \param[in]  height image height in pixels
     *
     * \returns 0 on success
     * \returns -1 on invalid image or no acceptable star; x/y are unchanged
     */
    int calculateCentroid( realT &x, realT &y, const realT *image, uint32_t width, uint32_t height );

  protected:

    pcf::IndiProperty m_indiP_x; ///< Read-only property `x` with element `current`
    pcf::IndiProperty m_indiP_y; ///< Read-only property `y` with element `current`

};

inline const std::array<centroidCtrl::Parameter, centroidCtrl::nParameters> &centroidCtrl::parameters()
{
    static const std::array<Parameter, nParameters> p = {{
        {"method", &Settings::method, 0, 1, true, "0: windowed COM; 1: Gaussian correlation peak"},
        {"selection", &Settings::selection, 0, 1, true, "0: nearest expected position; 1: strongest correlation"},
        {"expected_x", &Settings::expected_x, -1, 1000000, false, "Expected X in ROI pixels; -1: ROI center"},
        {"expected_y", &Settings::expected_y, -1, 1000000, false, "Expected Y in ROI pixels; -1: ROI center"},
        {"search_radius", &Settings::search_radius, 0, 1000000, false, "Candidate search radius in pixels; 0: full ROI"},
        {"window", &Settings::window, 4, 256, true, "Square centroid window side in pixels"},
        {"psf_sigma", &Settings::psf_sigma, 0.5, 10, false, "Gaussian template sigma in pixels"},
        {"plate_scale_mas", &Settings::plate_scale_mas, 0.000001, 1000000, false, "Current stream plate scale in mas per pixel"},
        {"fwhm_mas", &Settings::fwhm_mas, 0, 1000000, false, "Angular PSF FWHM in mas; 0: use psf_sigma"},
        {"detect_sigma", &Settings::detect_sigma, 1, 1000, false, "Detection threshold in filtered noise sigma"},
        {"noise_floor", &Settings::noise_floor, 0.000001, 1000000000, false, "Minimum background noise sigma in ADU"},
        {"ignore_sigma", &Settings::ignore_sigma, 0, 1000, false, "COM/support pixel threshold in background sigma"},
        {"ignore_adu", &Settings::ignore_adu, 0, 1000000000000.0, false, "COM/support threshold above background in ADU"},
        {"min_peak", &Settings::min_peak, 0, 1000000000000.0, false, "Minimum window peak above background in ADU"},
        {"min_flux", &Settings::min_flux, 0, 1000000000000000.0, false, "Minimum thresholded window flux in ADU"},
        {"min_pixels", &Settings::min_pixels, 1, 9, true, "Minimum above-threshold raw pixels in the seed 3x3 core"},
        {"separation", &Settings::separation, 1, 1000000, false, "Minimum separation of candidate peaks in pixels"},
        {"saturation", &Settings::saturation, 0, 1000000000000.0, false, "Reject windows at or above this raw ADU; 0: disabled"}
    }};
    return p;
}

inline double centroidCtrl::effectiveSigma(const Settings &s)
{
    return s.fwhm_mas > 0 ? s.fwhm_mas / (2.3548200450309493 * s.plate_scale_mas) : s.psf_sigma;
}

inline bool centroidCtrl::validSettings(const Settings &s)
{
    for(const auto &p : parameters())
    {
        const double v = s.*(p.value);
        if(!std::isfinite(v) || v < p.minimum || v > p.maximum || (p.integer && v != std::floor(v)))
            return false;
    }
    // Leave the complete Gaussian footprint plus one pixel on either side
    // inside the centroid window for the subpixel correlation interpolation.
    const double sigma = effectiveSigma(s);
    return sigma >= 0.5 && sigma <= 10 &&
           s.window >= 2 * (std::ceil(3 * sigma) + 1) + 1 &&
           s.min_pixels <= s.window * s.window &&
           (s.expected_x == -1 || s.expected_x >= 0) &&
           (s.expected_y == -1 || s.expected_y >= 0);
}

inline int centroidCtrl::st_newSettings(void *app, const pcf::IndiProperty &ipRecv)
{
    return static_cast<centroidCtrl *>(app)->newSettings(ipRecv);
}

inline int centroidCtrl::newSettings(const pcf::IndiProperty &ipRecv)
{
    // Lock order is INDI -> settings everywhere; the image calculation holds neither.
    std::lock_guard<std::mutex> indiLock(m_indiMutex);
    for(size_t i = 0; i < nParameters; ++i)
    {
        auto &prop = m_indiP_settings[i];
        if(prop.createUniqueKey() != ipRecv.createUniqueKey()) continue;
        if(!ipRecv.find("target")) return 0;
        double value;
        try { value = ipRecv["target"].get<double>(); }
        catch(const std::exception &) { return -1; }
        {
            std::lock_guard<std::mutex> settingsLock(m_settingsMutex);
            Settings proposed = m_settings;
            proposed.*(parameters()[i].value) = value;
            if(!validSettings(proposed)) return -1; // Never acknowledge an invalid target.
            m_settings = proposed;
            ++m_settingsVersion;
        }
        updateIfChanged(prop, std::vector<std::string>{"current", "target"},
                         std::vector<double>{value, value});
        // The last measurement belongs to the previous settings until a new frame arrives.
        updateIfChanged(m_indiP_detection, "valid", 0, pcf::IndiProperty::Alert);
        updateIfChanged(m_indiP_x, "current", m_x, pcf::IndiProperty::Alert);
        updateIfChanged(m_indiP_y, "current", m_y, pcf::IndiProperty::Alert);
        return 0;
    }
    return -1;
}

inline centroidCtrl::centroidCtrl() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    // Process the existing buffer on connect so we publish a first centroid quickly.
    shmimMonitorT::m_getExistingFirst = true;

    return;
}

inline void centroidCtrl::setupConfig()
{
    SHMIMMONITOR_SETUP_CONFIG( config );

    for(const auto &p : parameters())
    {
        const std::string key = std::string("centroid.") + p.name;
        config.add(key, "", key, argType::Required, "centroid", p.name, false,
                   "double", p.description);
    }

    config.add( "cam.cam_name",
                "",
                "cam.cam_name",
                argType::Required,
                "cam",
                "cam_name",
                false,
                "string",
                "Name of the input ImageStreamIO shared memory image (shmim). "
                "Used as /tmp/<cam_name>.im.shm." );

    config.add( "cam.cam_width",
                "",
                "cam.cam_width",
                argType::Required,
                "cam",
                "cam_width",
                false,
                "uint",
                "Expected camera image width in pixels. 0 means accept the stream size. "
                "Eventually this can be populated from a linked camera INDI property." );

    config.add( "cam.cam_height",
                "",
                "cam.cam_height",
                argType::Required,
                "cam",
                "cam_height",
                false,
                "uint",
                "Expected camera image height in pixels. 0 means accept the stream size. "
                "Eventually this can be populated from a linked camera INDI property." );
}

inline int centroidCtrl::loadConfigImpl( mx::app::appConfigurator &_config )
{
    SHMIMMONITOR_LOAD_CONFIG( _config );

    _config( m_camName, "cam.cam_name" );
    _config( m_camWidth, "cam.cam_width" );
    _config( m_camHeight, "cam.cam_height" );

    for(const auto &p : parameters())
        _config(m_settings.*(p.value), std::string("centroid.") + p.name);
    m_configValid = validSettings(m_settings);
    if(!m_configValid) return -1;

    // Prefer the dedicated cam_name option as the shmim to monitor.
    if( m_camName != "" )
    {
        shmimMonitorT::m_shmimName = m_camName;
    }

    return 0;
}

inline void centroidCtrl::loadConfig()
{
    if(loadConfigImpl(config) < 0)
    {
        m_configValid = false;
        log<software_error>({__FILE__, __LINE__, "Invalid centroid settings (check ranges and window/psf_sigma)"});
    }
}

inline int centroidCtrl::appStartup()
{
    if(!m_configValid) return -1;

    CREATE_REG_INDI_RO_NUMBER( m_indiP_x, "x", "Centroid X", "Centroid" );
    m_indiP_x.add( pcf::IndiElement( "current", 0 ) );

    CREATE_REG_INDI_RO_NUMBER( m_indiP_y, "y", "Centroid Y", "Centroid" );
    m_indiP_y.add( pcf::IndiElement( "current", 0 ) );

    CREATE_REG_INDI_RO_NUMBER(m_indiP_detection, "detection", "Star detection", "Centroid");
    for(const char *name : {"valid", "candidates", "background", "noise", "flux", "peak", "snr",
                            "seed_x", "seed_y", "x", "y"})
        m_indiP_detection.add(pcf::IndiElement(name, 0));

    for(size_t i = 0; i < nParameters; ++i)
    {
        const auto &p = parameters()[i];
        auto &prop = m_indiP_settings[i];
        if(createStandardIndiNumber<double>(prop, p.name, p.minimum, p.maximum,
                                            p.integer ? 1 : 0, "%g", p.description, "Centroid") < 0)
            return -1;
        const double value = m_settings.*(p.value);
        prop["current"] = value;
        prop["target"] = value;
        if(registerIndiPropertyNew(prop, &centroidCtrl::st_newSettings) < 0) return -1;
    }

    SHMIMMONITOR_APP_STARTUP;

    if( shmimMonitorT::m_shmimName == "" )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "cam.cam_name (or shmimMonitor.shmimName) must be set" } );
    }

    state( stateCodes::OPERATING );

    return 0;
}

inline int centroidCtrl::appLogic()
{
    SHMIMMONITOR_APP_LOGIC;
    SHMIMMONITOR_UPDATE_INDI;

    return 0;
}

inline int centroidCtrl::appShutdown()
{
    SHMIMMONITOR_APP_SHUTDOWN;

    return 0;
}

// memory allocation, setting member variables
inline int centroidCtrl::allocate( const dev::shmimT &dummy )
{
    static_cast<void>( dummy );

    m_dataType = shmimMonitorT::m_dataType;

    {
        std::lock_guard<std::mutex> lock(m_indiMutex);
        updateIfChanged(m_indiP_detection, "valid", 0, pcf::IndiProperty::Alert);
        updateIfChanged(m_indiP_x, "current", m_x, pcf::IndiProperty::Alert);
        updateIfChanged(m_indiP_y, "current", m_y, pcf::IndiProperty::Alert);
    }

    if( m_camWidth > 0 && shmimMonitorT::m_width != m_camWidth )
    {
        log<text_log>( "cam_width (" + std::to_string( m_camWidth ) + ") does not match shmim width (" +
                           std::to_string( shmimMonitorT::m_width ) + "); using shmim size",
                       logPrio::LOG_WARNING );
    }

    if( m_camHeight > 0 && shmimMonitorT::m_height != m_camHeight )
    {
        log<text_log>( "cam_height (" + std::to_string( m_camHeight ) + ") does not match shmim height (" +
                           std::to_string( shmimMonitorT::m_height ) + "); using shmim size",
                       logPrio::LOG_WARNING );
    }

    m_image.resize( shmimMonitorT::m_width, shmimMonitorT::m_height );
    m_image.setZero();

    log<text_log>( "connected to " + shmimMonitorT::m_shmimName + " (" + std::to_string( shmimMonitorT::m_width ) +
                   " x " + std::to_string( shmimMonitorT::m_height ) + ", datatype " +
                   std::to_string( m_dataType ) + ")" );

    return 0;
}

inline int centroidCtrl::processImage( void *curr_src, const dev::shmimT &dummy )
{
    static_cast<void>( dummy );

    const uint32_t width  = shmimMonitorT::m_width;
    const uint32_t height = shmimMonitorT::m_height;
    const size_t   npix   = static_cast<size_t>( width ) * static_cast<size_t>( height );

    // Copy / convert the shared-memory frame into a float working buffer.
    // Bit depth comes from the ImageStreamIO datatype of the connected stream.
    if( m_dataType == _DATATYPE_UINT16 )
    {
        uint16_t *src = reinterpret_cast<uint16_t *>( curr_src );
        for( size_t nn = 0; nn < npix; ++nn )
        {
            m_image.data()[nn] = static_cast<realT>( src[nn] );
        }
    }
    else if( m_dataType == _DATATYPE_INT16 )
    {
        int16_t *src = reinterpret_cast<int16_t *>( curr_src );
        for( size_t nn = 0; nn < npix; ++nn )
        {
            m_image.data()[nn] = static_cast<realT>( src[nn] );
        }
    }
    else if( m_dataType == _DATATYPE_FLOAT )
    {
        float *src = reinterpret_cast<float *>( curr_src );
        for( size_t nn = 0; nn < npix; ++nn )
        {
            m_image.data()[nn] = static_cast<realT>( src[nn] );
        }
    }
    else if( m_dataType == _DATATYPE_UINT8 )
    {
        uint8_t *src = reinterpret_cast<uint8_t *>( curr_src );
        for( size_t nn = 0; nn < npix; ++nn )
        {
            m_image.data()[nn] = static_cast<realT>( src[nn] );
        }
    }
    else
    {
        log<software_error>( { __FILE__, __LINE__, "unsupported ImageStreamIO datatype: " + std::to_string( m_dataType ) } );
        std::lock_guard<std::mutex> lock(m_indiMutex);
        updateIfChanged(m_indiP_detection, "valid", 0, pcf::IndiProperty::Alert);
        updateIfChanged(m_indiP_x, "current", m_x, pcf::IndiProperty::Alert);
        updateIfChanged(m_indiP_y, "current", m_y, pcf::IndiProperty::Alert);
        return 0;
    }

    Settings settings;
    uint64_t version;
    {
        std::lock_guard<std::mutex> lock(m_settingsMutex);
        settings = m_settings;
        version = m_settingsVersion;
    }
    realT x = 0, y = 0;
    const bool valid = measure(x, y, m_image.data(), width, height, settings) == 0;

    std::lock_guard<std::mutex> indiLock(m_indiMutex);
    {
        std::lock_guard<std::mutex> settingsLock(m_settingsMutex);
        if(version != m_settingsVersion) return 0;
    }
    // Keep last good coordinates on failure, explicitly marked invalid/Alert.
    if(valid) { m_x = x; m_y = y; }
    const auto status = valid ? pcf::IndiProperty::Ok : pcf::IndiProperty::Alert;
    updateIfChanged(m_indiP_x, "current", m_x, status);
    updateIfChanged(m_indiP_y, "current", m_y, status);
    // Publish the complete detection record in one SetProperty message.
    static const std::vector<std::string> elements = {"valid", "candidates", "background", "noise",
                                                    "flux", "peak", "snr", "seed_x", "seed_y", "x", "y"};
    updateIfChanged(m_indiP_detection, elements, std::vector<double>{
        valid ? 1.0 : 0.0, static_cast<double>(m_measurement.candidates),
        m_measurement.background, m_measurement.noise, m_measurement.flux, m_measurement.peak,
        m_measurement.snr, m_measurement.seed_x, m_measurement.seed_y,
        static_cast<double>(m_x), static_cast<double>(m_y)}, status);
    return 0;
}

inline double centroidCtrl::median(std::vector<double> &values)
{
    const size_t n = values.size();
    auto mid = values.begin() + n / 2;
    std::nth_element(values.begin(), mid, values.end());
    if(n % 2) return *mid;
    return 0.5 * (*mid + *std::max_element(values.begin(), mid));
}

inline int centroidCtrl::calculateCentroid(realT &x, realT &y, const realT *image,
                                          uint32_t width, uint32_t height)
{
    Settings settings;
    {
        std::lock_guard<std::mutex> lock(m_settingsMutex);
        settings = m_settings;
    }
    return measure(x, y, image, width, height, settings);
}

inline int centroidCtrl::measure(realT &x, realT &y, const realT *image,
                                uint32_t width, uint32_t height, const Settings &s)
{
    m_measurement = Measurement{};
    if(!image || !width || !height || !validSettings(s) ||
       width > static_cast<uint32_t>(std::numeric_limits<int>::max() / 2) ||
       height > static_cast<uint32_t>(std::numeric_limits<int>::max() / 2) ||
       static_cast<size_t>(height) > std::numeric_limits<size_t>::max() / width)
        return -1;
    const int w = static_cast<int>(width), h = static_cast<int>(height);
    const size_t count = static_cast<size_t>(width) * height;

    // Robust scalar background and noise: appropriate for sparse star fields
    // with a roughly uniform background. Reject frames containing NaN/Inf.
    m_work.resize(count);
    for(size_t i = 0; i < count; ++i)
    {
        if(!std::isfinite(image[i])) return -1;
        m_work[i] = image[i];
    }
    const double background = median(m_work);
    for(size_t i = 0; i < count; ++i) m_work[i] = std::abs(image[i] - background);
    const double noise = std::max(s.noise_floor, 1.482602218505602 * median(m_work));
    m_measurement.background = background;
    m_measurement.noise = noise;

    const int side = static_cast<int>(s.window);
    const int left = side / 2, right = side - 1 - left;
    const double sigma = effectiveSigma(s);
    const int radius = static_cast<int>(std::ceil(3 * sigma));
    const int lower = std::max(left, radius + 1);
    const int upper = std::max(right, radius + 1);
    if(w <= lower + upper || h <= lower + upper) return -1;

    // Unnormalized, separable Gaussian cross-correlation of SIGNED residuals.
    // Keeping negative noise samples avoids a positive clipping bias in detection.
    // Only use complete template footprints; no padding-induced edge peaks.
    m_kernel.resize(2 * radius + 1);
    double kernelSquaredSum = 0;
    for(int d = -radius; d <= radius; ++d)
    {
        const double k = std::exp(-0.5 * d * d / (sigma * sigma));
        m_kernel[d + radius] = k;
        kernelSquaredSum += k * k;
    }
    m_horizontal.assign(count, 0);
    m_correlation.assign(count, 0);
    for(int iy = 0; iy < h; ++iy)
        for(int ix = radius; ix < w - radius; ++ix)
        {
            double value = 0;
            for(int d = -radius; d <= radius; ++d)
                value += m_kernel[d + radius] * (image[static_cast<size_t>(iy) * w + ix + d] - background);
            m_horizontal[static_cast<size_t>(iy) * w + ix] = value;
        }
    for(int iy = radius; iy < h - radius; ++iy)
        for(int ix = radius; ix < w - radius; ++ix)
        {
            double value = 0;
            for(int d = -radius; d <= radius; ++d)
                value += m_kernel[d + radius] * m_horizontal[static_cast<size_t>(iy + d) * w + ix];
            m_correlation[static_cast<size_t>(iy) * w + ix] = value;
        }
    // sqrt(sum of squared 2D Gaussian weights) = sum of squared 1D weights.
    // This is background-only significance, not total source photon-noise SNR.
    const double filteredNoise = noise * kernelSquaredSum;
    const double threshold = std::max(s.ignore_adu, s.ignore_sigma * noise);
    const double expectedX = s.expected_x < 0 ? 0.5 * (w - 1) : s.expected_x;
    const double expectedY = s.expected_y < 0 ? 0.5 * (h - 1) : s.expected_y;
    const auto distanceSquared = [&](int ix, int iy)
    {
        return (ix - expectedX) * (ix - expectedX) + (iy - expectedY) * (iy - expectedY);
    };
    m_candidates.clear();
    m_stars.clear();
    for(int iy = lower; iy < h - upper; ++iy)
        for(int ix = lower; ix < w - upper; ++ix)
        {
            if(s.search_radius > 0 && distanceSquared(ix, iy) > s.search_radius * s.search_radius)
                continue;
            const size_t index = static_cast<size_t>(iy) * w + ix;
            const double score = m_correlation[index] / filteredNoise;
            if(score < s.detect_sigma) continue;
            bool maximum = true;
            for(int dy = -1; dy <= 1 && maximum; ++dy)
                for(int dx = -1; dx <= 1; ++dx)
                {
                    if(dx == 0 && dy == 0) continue;
                    const size_t other = static_cast<size_t>(iy + dy) * w + ix + dx;
                    if(m_correlation[other] > m_correlation[index] ||
                       (m_correlation[other] == m_correlation[index] && other < index))
                    { maximum = false; break; }
                }
            if(!maximum) continue;

            double flux = 0, peak = 0;
            int support = 0;
            bool saturated = false;
            for(int wy = iy - left; wy <= iy + right; ++wy)
                for(int wx = ix - left; wx <= ix + right; ++wx)
                {
                    const double raw = image[static_cast<size_t>(wy) * w + wx];
                    if(s.saturation > 0 && raw >= s.saturation) saturated = true;
                    const double signal = raw - background;
                    peak = std::max(peak, signal);
                    if(signal > threshold)
                    {
                        flux += signal;
                        if(std::abs(wx - ix) <= 1 && std::abs(wy - iy) <= 1) ++support;
                    }
                }
            if(saturated || support < s.min_pixels || flux <= 0 || flux < s.min_flux || peak < s.min_peak)
                continue;
            m_candidates.push_back(Candidate{ix, iy, score, flux, peak});
        }

    // Nonmaximum suppression: stronger peaks win within separation pixels.
    std::sort(m_candidates.begin(), m_candidates.end(), [](const Candidate &a, const Candidate &b)
    {
        if(a.score != b.score) return a.score > b.score;
        if(a.y != b.y) return a.y < b.y;
        return a.x < b.x;
    });
    for(const auto &candidate : m_candidates)
    {
        bool duplicate = false;
        for(const auto &star : m_stars)
        {
            const double dx = candidate.x - star.x, dy = candidate.y - star.y;
            if(dx * dx + dy * dy < s.separation * s.separation) { duplicate = true; break; }
        }
        if(!duplicate) m_stars.push_back(candidate);
    }
    m_measurement.candidates = m_stars.size();
    if(m_stars.empty()) return -1;
    const Candidate *selected = &m_stars.front();
    if(s.selection == 0)
        for(const auto &star : m_stars)
            if(distanceSquared(star.x, star.y) < distanceSquared(selected->x, selected->y)) selected = &star;
    const int sx = selected->x, sy = selected->y;
    double resultX, resultY;
    if(s.method == 0)
    {
        double sumX = 0, sumY = 0;
        for(int iy = sy - left; iy <= sy + right; ++iy)
            for(int ix = sx - left; ix <= sx + right; ++ix)
            {
                const double signal = image[static_cast<size_t>(iy) * w + ix] - background;
                if(signal > threshold) { sumX += signal * ix; sumY += signal * iy; }
            }
        resultX = sumX / selected->flux;
        resultY = sumY / selected->flux;
    }
    else
    {
        // Subpixel peak of Gaussian correlation via independent 3-point parabolas.
        // The template at the seed and +/-1 fits entirely inside the selected window.
        const size_t index = static_cast<size_t>(sy) * w + sx;
        const auto offset = [](double minus, double center, double plus)
        {
            const double curvature = minus - 2 * center + plus;
            if(curvature >= 0) return 0.0;
            return std::max(-0.5, std::min(0.5, 0.5 * (minus - plus) / curvature));
        };
        resultX = sx + offset(m_correlation[index - 1], m_correlation[index], m_correlation[index + 1]);
        resultY = sy + offset(m_correlation[index - w], m_correlation[index], m_correlation[index + w]);
    }
    if(!std::isfinite(resultX) || !std::isfinite(resultY)) return -1;
    x = static_cast<realT>(resultX);
    y = static_cast<realT>(resultY);
    m_measurement.flux = selected->flux;
    m_measurement.peak = selected->peak;
    m_measurement.snr = selected->score;
    m_measurement.seed_x = sx;
    m_measurement.seed_y = sy;
    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // centroidCtrl_hpp

