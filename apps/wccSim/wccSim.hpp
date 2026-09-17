/** \file wccSim.hpp
 * \brief The MagAO-X WCC all-sky sensor array simulator.
 * \author Adam Schilperoort
 *
 * \ingroup wccSim_files
 */

#ifndef wccSim_hpp
#define wccSim_hpp

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ImageStreamIO/ImageStreamIO.h>
#include <mx/sys/timeUtils.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include "../wccCommon/wccFocalPlane.hpp"
#include "../wccCommon/wccIndiRate.hpp"
#include "../wccCommon/wccPSF.hpp"
#include "../wccCommon/wccPhotometry.hpp"
#include "../wccCommon/wccSensorConfig.hpp"
#include "../wccCommon/wccSensorModel.hpp"
#include "../wccCommon/wccSkyWCS.hpp"
#include "../wccCommon/wccStarCatalog.hpp"

/** \defgroup wccSim WCC All-Sky Simulator
 * \brief Real time simulation of star fields on the WCC sensor array.
 *
 * Converts the offline `uasal_star_catalog_simulator_prysm.py` star field
 * simulator into a MagAO-X application that publishes a live ImageStreamIO
 * stream per sensor, at each sensor's own commanded frame rate, exposure time
 * and region of interest.
 *
 * Each configured sensor is tied to an INDI camera device, real or simulated.
 * wccSim subscribes to that device's `fps`, `exptime`, `emgain` and
 * `roi_region_*` properties, so when a camera is reconfigured the simulated
 * frames follow it with no operator action. Telescope pointing comes either from
 * a telescope device over INDI or from wccSim's own `pointing` property, and the
 * sensor array geometry maps the boresight onto each sensor's own world
 * coordinate system.
 *
 * <a href="../handbook/operating/software/apps/wccSim.html">Application Documentation</a>
 *
 * \ingroup apps
 */

/** \defgroup wccSim_files WCC All-Sky Simulator Files
 * \ingroup wccSim
 */

namespace MagAOX
{
namespace app
{

/// Live state and worker thread for one simulated sensor.
/** One instance per configured sensor. The INDI callbacks write the live camera
 * parameters under m_mutex and the worker thread reads a snapshot of them once
 * per frame, so a camera reconfiguration takes effect on the next frame without
 * either side blocking on the other.
 *
 * \ingroup wccSim
 */
struct wccSimSensor
{
    /** \name Identity - Data
     *@{
     */
    size_t m_index{ 0 }; ///< Index of this sensor in the focal plane model.

    std::string m_name; ///< Logical sensor name, e.g. "IMX-18".

    std::string m_indiDevice; ///< INDI camera device supplying live parameters.

    std::string m_shmimOut; ///< ImageStreamIO stream frames are published to.

    std::string m_bankKey; ///< Key of the shared PSF bank this sensor uses.

    uint64_t m_seed{ 1 }; ///< RNG seed for this sensor's noise.
    ///@}

    /** \name Live Camera Parameters - Data
     *@{
     */
    std::mutex m_mutex; ///< Guards the live parameters below.

    double m_fps{ 1.0 }; ///< Commanded frame rate [Hz].

    double m_expTime{ 1.0 }; ///< Commanded exposure time [s].

    double m_gain{ 1.0 }; ///< Commanded camera gain, applied as electrons per DN.

    int m_bitDepth{ 16 }; ///< Commanded ADC bit depth.

    wcc::roiSpec m_roi; ///< Commanded region of interest.
    ///@}

    /** \name Output Stream - Data
     *@{
     */
    IMAGE m_stream{}; ///< The output ImageStreamIO stream.

    bool m_streamOpen{ false }; ///< True while m_stream is created.

    uint32_t m_streamW{ 0 }; ///< Width the stream was created at [pixels].

    uint32_t m_streamH{ 0 }; ///< Height the stream was created at [pixels].
    ///@}

    /** \name Worker and Statistics - Data
     *@{
     */
    std::thread m_thread; ///< Worker thread rendering this sensor.

    std::atomic<double> m_achievedFps{ 0 }; ///< Frame rate actually achieved [Hz].

    std::atomic<double> m_renderMs{ 0 }; ///< Wall time of the last frame render [ms].

    std::atomic<uint64_t> m_frames{ 0 }; ///< Frames published since startup.

    std::atomic<int> m_nStars{ 0 }; ///< Stars placed in the last frame.

    std::atomic<uint64_t> m_saturated{ 0 }; ///< Saturated pixels in the last frame.

    std::atomic<bool> m_ready{ false }; ///< True once this sensor has published a frame.
    ///@}

    /** \name INDI - Data
     *@{
     */
    pcf::IndiProperty m_ipStatus; ///< Read-only status property published for this sensor.

    /// Remote camera properties this sensor subscribes to, held so they stay registered.
    std::vector<std::shared_ptr<pcf::IndiProperty>> m_remote;
    ///@}
};

/// The MagAO-X WCC all-sky sensor array simulator.
/** \ingroup wccSim
 */
class wccSim : public MagAOXApp<true>
{

    // Give the test harness access.
    friend class wccSim_test;

    /** \name Telescope Configuration - Data
     *@{
     */
  protected:
    double m_diameter{ 6.5 }; ///< Telescope clear aperture diameter [m].

    double m_fNumber{ 12.0 }; ///< Telescope focal ratio.

    double m_centralObscuration{ 0.0 }; ///< Central obscuration as a fraction of the diameter.

    int m_spiderVanes{ 0 }; ///< Number of spider vanes. 0 disables the spider.

    double m_spiderWidth{ 0.0 }; ///< Spider vane width [m].

    /// Handedness of focal plane X relative to increasing right ascension, +1 or -1.
    double m_parity{ -1.0 };

    double m_throughput{ 0.5 }; ///< Total optical throughput, excluding detector quantum efficiency.
    ///@}

    /** \name Catalog Configuration - Data
     *@{
     */
  protected:
    std::string m_catalogPath; ///< Path to the star catalog CSV.

    std::string m_magColumn{ "mag" }; ///< Catalog column supplying the magnitude.

    std::string m_idColumn{ "gsc2ID" }; ///< Catalog column supplying the identifier.

    double m_magLimit{ 22.0 }; ///< Drop catalog sources fainter than this.
    ///@}

    /** \name Pointing Configuration - Data
     *@{
     */
  protected:
    /// INDI telescope device supplying the pointing. Empty means wccSim owns the pointing.
    /** Normally `telescopeSim`, whose `pointing` property reports where the mount
     * currently is, jitter included. Pointing this at `tcsInterface` instead needs
     * only the element names below changed to `telpos` / `rotoff`.
     */
    std::string m_telDevice;

    std::string m_telProperty{ "pointing" }; ///< Telescope property carrying the current pointing.

    std::string m_telRAElement{ "ra" }; ///< Element carrying right ascension [deg].

    std::string m_telDecElement{ "dec" }; ///< Element carrying declination [deg].

    std::string m_telPAElement{ "pa" }; ///< Element carrying the position angle [deg].

    double m_startRA{ 0 }; ///< Boresight right ascension at startup [deg].

    double m_startDec{ 0 }; ///< Boresight declination at startup [deg].

    double m_startPA{ 0 }; ///< Boresight position angle at startup [deg].
    ///@}

    /** \name Simulation Configuration - Data
     *@{
     */
  protected:
    /// Names of the sensors to simulate. Each must have a like named config section.
    std::vector<std::string> m_sensorNames;

    int m_npixPupil{ 256 }; ///< Pupil samples across, the `npix_pupil` of the Python simulator.

    int m_psfSamples{ 48 }; ///< PSF stamp size on a side [pixels].

    int m_psfSubSteps{ 8 }; ///< Sub-pixel PSF bank bins per axis.

    double m_starMargin{ 1.0 }; ///< Cone search margin, in stamp widths, beyond the ROI.

    std::string m_noiseModeStr{ "full" }; ///< Noise model to evaluate: off, read or full.

    double m_bias{ 100.0 }; ///< Detector bias pedestal [electrons].

    uint64_t m_seed{ 20260909 }; ///< Base RNG seed; each sensor derives its own from this.

    int m_circBuffLength{ 1 }; ///< Circular buffer depth of the published streams.

    double m_defaultFps{ 4.0 }; ///< Frame rate used before a camera reports one [Hz].

    double m_defaultExpTime{ 1.0 }; ///< Exposure time used before a camera reports one [s].

    bool m_startStreaming{ false }; ///< Whether to begin publishing without an operator toggle.
    ///@}

    /** \name Simulation State - Data
     *@{
     */
  protected:
    /// The star catalog. Immutable after appStartup, so workers read it without locking.
    wcc::starCatalog m_catalog;

    /// Sensor array geometry and the current boresight.
    /** The array layout is immutable after appStartup. The pointing is written
     * under m_pointingMutex and copied out by each worker once per frame.
     */
    wcc::focalPlaneModel m_focalPlane;

    std::mutex m_pointingMutex; ///< Guards the pointing held in m_focalPlane.

    double m_ra{ 0 }; ///< Current boresight right ascension [deg].

    double m_dec{ 0 }; ///< Current boresight declination [deg].

    double m_pa{ 0 }; ///< Current position angle of focal plane +Y, east of north [deg].

    wcc::noiseMode m_noiseMode{ wcc::noiseMode::full }; ///< Parsed form of m_noiseModeStr.

    /// The simulated sensors. Held by pointer because each owns a mutex and a thread.
    std::vector<std::unique_ptr<wccSimSensor>> m_sensors;

    /// Shared PSF banks, keyed by bankKey(). Entries are immutable once inserted.
    std::map<std::string, std::shared_ptr<const wcc::psfBank>> m_banks;

    std::mutex m_bankMutex; ///< Guards m_banks while the builder thread populates it.

    std::atomic<bool> m_banksReady{ false }; ///< True once every required PSF bank is built.

    std::atomic<bool> m_bankFault{ false }; ///< True if a PSF bank failed to build.

    std::atomic<bool> m_streaming{ false }; ///< The `streaming` toggle; gates frame publication.

    std::atomic<bool> m_shutdownWorkers{ false }; ///< Tells the worker threads to exit.

    std::thread m_bankBuilder; ///< Thread that builds the PSF banks at startup.

    /// What a subscribed remote property updates.
    struct remoteBinding
    {
        wccSimSensor *m_sensor{ nullptr }; ///< Sensor the property belongs to, null for the telescope.

        std::string m_what; ///< Parameter selector: fps, exptime, emgain, bitDepth, roi_*, or telpos.
    };

    /// Subscriptions indexed by pcf::IndiProperty::createUniqueKey().
    std::map<std::string, remoteBinding> m_bindings;
    ///@}

    /** \name INDI - Data
     *@{
     */
  protected:
    pcf::IndiProperty m_indiP_streaming; ///< Toggle gating frame publication.
    INDI_NEWCALLBACK_DECL( wccSim, m_indiP_streaming );

    pcf::IndiProperty m_indiP_pointing; ///< Boresight pointing: ra, dec and pa.
    INDI_NEWCALLBACK_DECL( wccSim, m_indiP_pointing );

    pcf::IndiProperty m_indiP_offset; ///< Relative boresight offset: x, y [arcsec] and roll [deg].
    INDI_NEWCALLBACK_DECL( wccSim, m_indiP_offset );

    pcf::IndiProperty m_indiP_catalog; ///< Read-only catalog status.

    pcf::IndiProperty m_indiP_status; ///< Read-only simulator status.

    /// Telescope pointing property, registered when m_telDevice is configured.
    pcf::IndiProperty m_indiP_telPos;
    ///@}

  public:
    /// Default c'tor.
    wccSim();

    /// D'tor, declared and defined for noexcept.
    ~wccSim() noexcept;

    virtual void setupConfig();

    /// Implementation of loadConfig logic, separated for testing.
    /** \returns 0 on success
     * \returns -1 on a configuration error, in which case the app shuts down
     */
    int loadConfigImpl( mx::app::appConfigurator &_config /**< [in] configuration to load from */ );

    virtual void loadConfig();

    virtual int appStartup();

    /// Implementation of the FSM for wccSim.
    /** \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
    virtual int appLogic();

    virtual int appShutdown();

    /** \name Configuration Helpers
     *@{
     */
  protected:
    /// Read one sensor's parameters from its own configuration section.
    /** \returns 0 on success
     * \returns -1 if the section is missing or malformed
     */
    int loadSensorSection( mx::app::appConfigurator &_config /**< [in] configuration to load from */,
                           const std::string &name /**< [in] sensor name, also the section name */,
                           wcc::sensorConfig &sc /**< [out] the sensor geometry and detector parameters */,
                           std::string &indiDevice /**< [out] INDI camera device */,
                           std::string &shmimOut /**< [out] output stream name */,
                           wcc::roiSpec &roi /**< [out] the startup region of interest */ );

    /// Key identifying the PSF bank a sensor needs, from its pitch and bandpass.
    /** Sensors sharing a pixel pitch and bandpass share one bank, which is the
     * usual case across an array of identical detectors and keeps startup short.
     *
     * \returns the bank key
     */
    std::string bankKey( const wcc::sensorConfig &sc /**< [in] the sensor */ ) const;
    ///@}

    /** \name PSF Banks
     *@{
     */
  protected:
    /// Thread entry for the PSF bank builder.
    static void bankBuilderStart( wccSim *s /**< [in] this */ );

    /// Build every PSF bank the configured sensors require.
    void bankBuilderExec();

    /// Look up a built PSF bank.
    /** \returns the bank
     * \returns nullptr if it is not built yet
     */
    std::shared_ptr<const wcc::psfBank> bank( const std::string &key /**< [in] bank key */ );
    ///@}

    /** \name Simulation Workers
     *@{
     */
  protected:
    /// Thread entry for a sensor worker.
    static void sensorWorkerStart( wccSim *s /**< [in] this */, wccSimSensor *sen /**< [in] the sensor */ );

    /// Render and publish frames for one sensor at its commanded cadence.
    void sensorWorkerExec( wccSimSensor *sen /**< [in] the sensor */ );

    /// Render one frame for a sensor into an electron buffer.
    /** \returns the number of stars placed
     * \returns -1 on an error
     */
    int renderFrame( const wcc::sensorConfig &sc /**< [in] sensor geometry and detector parameters */,
                     const wcc::psfBank &bnk /**< [in] PSF bank for this sensor */,
                     const wcc::roiSpec &roi /**< [in] region of interest to render */,
                     double expTime /**< [in] exposure time [s] */,
                     double ra /**< [in] boresight right ascension [deg] */,
                     double dec /**< [in] boresight declination [deg] */,
                     double pa /**< [in] boresight position angle [deg] */,
                     std::vector<float> &frame /**< [out] electrons, row major */,
                     int &bx0 /**< [out] illuminated bounding box min column */,
                     int &by0 /**< [out] illuminated bounding box min row */,
                     int &bx1 /**< [out] illuminated bounding box max column */,
                     int &by1 /**< [out] illuminated bounding box max row */ );

    /// Create or resize a sensor's output stream to match a ROI.
    /** \returns 0 on success
     * \returns -1 if the stream cannot be created
     */
    int ensureStream( wccSimSensor *sen /**< [in,out] the sensor */,
                      uint32_t w /**< [in] width [pixels] */,
                      uint32_t h /**< [in] height [pixels] */ );

    /// Publish a digitized frame to a sensor's output stream.
    /** \returns 0 on success
     * \returns -1 if the stream is not open or is the wrong size
     */
    int publishFrame( wccSimSensor *sen /**< [in,out] the sensor */,
                      const std::vector<uint16_t> &pixels /**< [in] digital numbers, row major */ );
    ///@}

    /** \name INDI Helpers
     *@{
     */
  protected:
    /// Register the INDI subscriptions for one sensor's camera device.
    /** \returns 0 on success
     * \returns -1 on a registration failure
     */
    int registerSensorSubscriptions( wccSimSensor *sen /**< [in,out] the sensor */ );

    /// Shared SET callback for every remote property this app subscribes to.
    /** A single callback is used because the number of cameras is a runtime
     * quantity, so the per-property INDI_SETCALLBACK macros cannot be applied.
     * Dispatch is by pcf::IndiProperty::createUniqueKey(), the approach
     * indiTSAccumulator uses for the same reason.
     *
     * \returns 0 always, so an unexpected property is ignored rather than fatal
     */
    int setCallBack_remote( const pcf::IndiProperty &ipRecv /**< [in] the received property */ );

    /// Static trampoline for setCallBack_remote().
    /** \returns the result of setCallBack_remote()
     */
    static int st_setCallBack_remote( void *app /**< [in] the wccSim instance */,
                                      const pcf::IndiProperty &ipRecv /**< [in] the received property */ );

    /// Publish the per sensor and aggregate status properties.
    void updateStatus();

    /// Apply a boresight pointing, wrapping right ascension and clamping declination.
    void applyPointing( double ra /**< [in] right ascension [deg] */,
                        double dec /**< [in] declination [deg] */,
                        double pa /**< [in] position angle [deg] */ );

    /// Read a numeric element from a received property, tolerating text encoding.
    /** \returns true if the element was present and parsed to a finite number
     */
    static bool elementValue( const pcf::IndiProperty &ip /**< [in] the property */,
                              const std::string &el /**< [in] element name */,
                              double &out /**< [out] the value */ );

    /// Sanitize a sensor name into a valid INDI property name.
    /** \returns the sanitized name
     */
    static std::string indiSafeName( const std::string &name /**< [in] raw sensor name */ );
    ///@}
};

inline wccSim::wccSim() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    // No PDU: the simulator is always available.
    m_powerMgtEnabled = false;

    // The per sensor worker threads carry the frame cadence, which is unbounded and
    // set by each camera. appLogic only reads INDI and publishes status, so it runs
    // at the 1 Hz limit every WCC application holds itself to.
    m_loopPause = wcc::indiLoopPause;

    return;
}

inline wccSim::~wccSim() noexcept
{
    return;
}

inline void wccSim::setupConfig()
{
    config.add( "telescope.diameter", "", "telescope.diameter", argType::Required, "telescope", "diameter", false,
                "double", "Clear aperture diameter [m]." );
    config.add( "telescope.f_number", "", "telescope.f_number", argType::Required, "telescope", "f_number", false,
                "double", "Focal ratio. With the diameter this sets the plate scale." );
    config.add( "telescope.central_obscuration", "", "telescope.central_obscuration", argType::Required,
                "telescope", "central_obscuration", false, "double",
                "Central obscuration as a fraction of the diameter. 0 disables it." );
    config.add( "telescope.spider_vanes", "", "telescope.spider_vanes", argType::Required, "telescope",
                "spider_vanes", false, "int", "Number of spider vanes. 0 disables the spider." );
    config.add( "telescope.spider_width", "", "telescope.spider_width", argType::Required, "telescope",
                "spider_width", false, "double", "Spider vane width [m]." );
    config.add( "telescope.parity", "", "telescope.parity", argType::Required, "telescope", "parity", false,
                "double", "Handedness of focal plane X against increasing RA, +1 or -1. WCC is -1." );
    config.add( "telescope.throughput", "", "telescope.throughput", argType::Required, "telescope", "throughput",
                false, "double", "Total optical throughput, excluding detector quantum efficiency." );

    config.add( "catalog.path", "", "catalog.path", argType::Required, "catalog", "path", false, "string",
                "Path to the star catalog CSV, e.g. gsc31_north.csv." );
    config.add( "catalog.mag_column", "", "catalog.mag_column", argType::Required, "catalog", "mag_column", false,
                "string", "Header name of the catalog column supplying the magnitude." );
    config.add( "catalog.id_column", "", "catalog.id_column", argType::Required, "catalog", "id_column", false,
                "string", "Header name of the catalog column supplying the identifier." );
    config.add( "catalog.mag_limit", "", "catalog.mag_limit", argType::Required, "catalog", "mag_limit", false,
                "double", "Drop catalog sources fainter than this magnitude." );

    config.add( "pointing.tel_device", "", "pointing.tel_device", argType::Required, "pointing", "tel_device",
                false, "string",
                "INDI telescope device supplying the pointing. Empty means wccSim owns the pointing and "
                "accepts local pointing and offset commands." );
    config.add( "pointing.tel_property", "", "pointing.tel_property", argType::Required, "pointing",
                "tel_property", false, "string", "Telescope property carrying the pointing. Default telpos." );
    config.add( "pointing.tel_ra_element", "", "pointing.tel_ra_element", argType::Required, "pointing",
                "tel_ra_element", false, "string", "Element carrying right ascension [deg]." );
    config.add( "pointing.tel_dec_element", "", "pointing.tel_dec_element", argType::Required, "pointing",
                "tel_dec_element", false, "string", "Element carrying declination [deg]." );
    config.add( "pointing.tel_pa_element", "", "pointing.tel_pa_element", argType::Required, "pointing",
                "tel_pa_element", false, "string", "Element carrying the position angle [deg]." );
    config.add( "pointing.ra", "", "pointing.ra", argType::Required, "pointing", "ra", false, "double",
                "Boresight right ascension at startup [deg]." );
    config.add( "pointing.dec", "", "pointing.dec", argType::Required, "pointing", "dec", false, "double",
                "Boresight declination at startup [deg]." );
    config.add( "pointing.pa", "", "pointing.pa", argType::Required, "pointing", "pa", false, "double",
                "Position angle of focal plane +Y, east of north, at startup [deg]." );

    config.add( "sim.sensors", "", "sim.sensors", argType::Required, "sim", "sensors", false, "vector<string>",
                "Comma separated sensor names. Each needs a configuration section of the same name." );
    config.add( "sim.npix_pupil", "", "sim.npix_pupil", argType::Required, "sim", "npix_pupil", false, "int",
                "Pupil samples across for the diffraction calculation." );
    config.add( "sim.psf_samples", "", "sim.psf_samples", argType::Required, "sim", "psf_samples", false, "int",
                "PSF postage stamp size on a side [pixels]." );
    config.add( "sim.psf_substeps", "", "sim.psf_substeps", argType::Required, "sim", "psf_substeps", false, "int",
                "Sub-pixel PSF bank bins per axis. Placement error is 0.5/substeps pixels." );
    config.add( "sim.star_margin", "", "sim.star_margin", argType::Required, "sim", "star_margin", false, "double",
                "Cone search margin beyond the ROI, in PSF stamp widths." );
    config.add( "sim.noise_mode", "", "sim.noise_mode", argType::Required, "sim", "noise_mode", false, "string",
                "Noise model to evaluate: off, read or full. Full frames at high rate may need read or off." );
    config.add( "sim.bias", "", "sim.bias", argType::Required, "sim", "bias", false, "double",
                "Detector bias pedestal [electrons]." );
    config.add( "sim.seed", "", "sim.seed", argType::Required, "sim", "seed", false, "int",
                "Base RNG seed. Each sensor derives its own so frames are reproducible." );
    config.add( "sim.circ_buff_length", "", "sim.circ_buff_length", argType::Required, "sim", "circ_buff_length",
                false, "int", "Circular buffer depth of the published streams." );
    config.add( "sim.default_fps", "", "sim.default_fps", argType::Required, "sim", "default_fps", false, "double",
                "Frame rate used before a camera reports one [Hz]." );
    config.add( "sim.default_exp_time", "", "sim.default_exp_time", argType::Required, "sim", "default_exp_time",
                false, "double", "Exposure time used before a camera reports one [s]." );
    config.add( "sim.start_streaming", "", "sim.start_streaming", argType::Required, "sim", "start_streaming",
                false, "bool", "Begin publishing at startup rather than waiting for the streaming toggle." );
}

inline int wccSim::loadConfigImpl( mx::app::appConfigurator &_config )
{
    _config( m_diameter, "telescope.diameter" );
    _config( m_fNumber, "telescope.f_number" );
    _config( m_centralObscuration, "telescope.central_obscuration" );
    _config( m_spiderVanes, "telescope.spider_vanes" );
    _config( m_spiderWidth, "telescope.spider_width" );
    _config( m_parity, "telescope.parity" );
    _config( m_throughput, "telescope.throughput" );

    _config( m_catalogPath, "catalog.path" );
    _config( m_magColumn, "catalog.mag_column" );
    _config( m_idColumn, "catalog.id_column" );
    _config( m_magLimit, "catalog.mag_limit" );

    _config( m_telDevice, "pointing.tel_device" );
    _config( m_telProperty, "pointing.tel_property" );
    _config( m_telRAElement, "pointing.tel_ra_element" );
    _config( m_telDecElement, "pointing.tel_dec_element" );
    _config( m_telPAElement, "pointing.tel_pa_element" );
    _config( m_startRA, "pointing.ra" );
    _config( m_startDec, "pointing.dec" );
    _config( m_startPA, "pointing.pa" );

    _config( m_sensorNames, "sim.sensors" );
    _config( m_npixPupil, "sim.npix_pupil" );
    _config( m_psfSamples, "sim.psf_samples" );
    _config( m_psfSubSteps, "sim.psf_substeps" );
    _config( m_starMargin, "sim.star_margin" );
    _config( m_noiseModeStr, "sim.noise_mode" );
    _config( m_bias, "sim.bias" );

    {
        int seed = static_cast<int>( m_seed );
        _config( seed, "sim.seed" );
        if( seed > 0 )
        {
            m_seed = static_cast<uint64_t>( seed );
        }
    }

    _config( m_circBuffLength, "sim.circ_buff_length" );
    _config( m_defaultFps, "sim.default_fps" );
    _config( m_defaultExpTime, "sim.default_exp_time" );
    _config( m_startStreaming, "sim.start_streaming" );

    m_noiseMode = wcc::parseNoiseMode( m_noiseModeStr );

    if( m_diameter <= 0 || m_fNumber <= 0 )
    {
        return log<software_critical, -1>(
            { __FILE__, __LINE__, "telescope.diameter and telescope.f_number must both be positive" } );
    }

    if( m_npixPupil < 32 )
    {
        m_npixPupil = 32;
    }

    if( m_psfSamples < 8 )
    {
        m_psfSamples = 8;
    }

    // An odd stamp has no FFT origin sample, so force even.
    if( m_psfSamples % 2 != 0 )
    {
        ++m_psfSamples;
    }

    if( m_psfSubSteps < 1 )
    {
        m_psfSubSteps = 1;
    }

    if( m_circBuffLength < 1 )
    {
        m_circBuffLength = 1;
    }

    if( m_sensorNames.empty() )
    {
        return log<software_critical, -1>(
            { __FILE__, __LINE__, "sim.sensors is empty; at least one sensor must be configured" } );
    }

    m_focalPlane.setTelescope( m_diameter, m_fNumber, m_parity );

    // Each named sensor gets its geometry from its own like named section.
    for( const std::string &name : m_sensorNames )
    {
        wcc::sensorConfig sc;
        std::string indiDevice, shmimOut;
        wcc::roiSpec roi;

        if( loadSensorSection( _config, name, sc, indiDevice, shmimOut, roi ) < 0 )
        {
            return -1;
        }

        std::unique_ptr<wccSimSensor> sen( new wccSimSensor );
        sen->m_index = m_focalPlane.nSensors();
        sen->m_name = sc.m_name;
        sen->m_indiDevice = indiDevice;
        sen->m_shmimOut = shmimOut;
        sen->m_bankKey = bankKey( sc );
        sen->m_fps = m_defaultFps;
        sen->m_expTime = m_defaultExpTime;
        sen->m_roi = roi;

        // Derive a per sensor seed so each detector's noise is independent yet
        // reproducible from the single configured base seed.
        sen->m_seed = m_seed + 0x9E3779B97F4A7C15ULL * ( sen->m_index + 1 );

        m_focalPlane.addSensor( sc );
        m_sensors.push_back( std::move( sen ) );
    }

    m_ra = m_startRA;
    m_dec = m_startDec;
    m_pa = m_startPA;
    m_focalPlane.setPointing( m_ra, m_dec, m_pa );

    return 0;
}

inline void wccSim::loadConfig()
{
    if( loadConfigImpl( config ) < 0 )
    {
        m_shutdown = true;
    }
}

inline int wccSim::loadSensorSection( mx::app::appConfigurator &_config,
                                      const std::string &name,
                                      wcc::sensorConfig &sc,
                                      std::string &indiDevice,
                                      std::string &shmimOut,
                                      wcc::roiSpec &roi )
{
    std::string err;

    if( wcc::loadSensorGeometry( _config, name, sc, err ) < 0 )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, err } );
    }

    if( !wcc::sensorConfigString( _config, name, "indi_device", indiDevice ) )
    {
        return log<software_critical, -1>(
            { __FILE__, __LINE__, "sensor section [" + name + "] is missing indi_device" } );
    }

    shmimOut = indiDevice + "sim";
    wcc::sensorConfigString( _config, name, "shmim_out", shmimOut );

    roi = wcc::loadSensorROI( _config, name, sc );

    return 0;
}

inline std::string wccSim::bankKey( const wcc::sensorConfig &sc ) const
{
    // The PSF depends on the pupil, the bandpass and the detector sampling, so
    // those are exactly the quantities in the key.
    char buf[192];
    snprintf( buf, sizeof( buf ), "px%.6f_wl%.4f_bw%.6f_n%d_s%d_ss%d", sc.m_pixelSize, sc.m_pivotWavelength,
              sc.m_bandwidth, m_npixPupil, m_psfSamples, m_psfSubSteps );

    return std::string( buf );
}

inline std::string wccSim::indiSafeName( const std::string &name )
{
    std::string out;

    for( char c : name )
    {
        if( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) )
        {
            out.push_back( c );
        }
        else
        {
            out.push_back( '_' );
        }
    }

    return out;
}

inline int wccSim::appStartup()
{
    // ------------------------------------------------------------- catalog
    if( m_catalogPath.empty() )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, "catalog.path is not set" } );
    }

    if( m_catalog.load( m_catalogPath, m_magColumn, m_idColumn, m_magLimit ) < 0 )
    {
        return log<software_critical, -1>(
            { __FILE__, __LINE__,
              "failed to load catalog " + m_catalogPath + " (column " + m_magColumn + ")" } );
    }

    if( m_catalog.empty() )
    {
        return log<software_critical, -1>(
            { __FILE__, __LINE__, "catalog " + m_catalogPath + " yielded no usable sources" } );
    }

    log<text_log>( "loaded " + std::to_string( m_catalog.size() ) + " sources from " + m_catalogPath +
                   " (skipped " + std::to_string( m_catalog.skipped() ) + ", mag " +
                   std::to_string( m_catalog.magMin() ) + " to " + std::to_string( m_catalog.magMax() ) + ")" );

    // ---------------------------------------------------------- local INDI
    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_streaming, "streaming" );

    // A three vector reads better as named elements than as current/target pairs,
    // the same choice tcsInterface makes for pyrNudge.
    REG_INDI_NEWPROP( m_indiP_pointing, "pointing", pcf::IndiProperty::Number );
    m_indiP_pointing.add( pcf::IndiElement( "ra" ) );
    m_indiP_pointing.add( pcf::IndiElement( "dec" ) );
    m_indiP_pointing.add( pcf::IndiElement( "pa" ) );
    m_indiP_pointing["ra"].set( m_ra );
    m_indiP_pointing["dec"].set( m_dec );
    m_indiP_pointing["pa"].set( m_pa );

    REG_INDI_NEWPROP( m_indiP_offset, "offset", pcf::IndiProperty::Number );
    m_indiP_offset.add( pcf::IndiElement( "x" ) );
    m_indiP_offset.add( pcf::IndiElement( "y" ) );
    m_indiP_offset.add( pcf::IndiElement( "roll" ) );
    m_indiP_offset["x"].set( 0.0 );
    m_indiP_offset["y"].set( 0.0 );
    m_indiP_offset["roll"].set( 0.0 );

    if( createROIndiNumber( m_indiP_catalog, "catalog", "Star catalog", "sim" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber catalog" } );
    }
    m_indiP_catalog.add( pcf::IndiElement( "nsources" ) );
    m_indiP_catalog.add( pcf::IndiElement( "mag_min" ) );
    m_indiP_catalog.add( pcf::IndiElement( "mag_max" ) );
    m_indiP_catalog["nsources"].set( static_cast<double>( m_catalog.size() ) );
    m_indiP_catalog["mag_min"].set( m_catalog.magMin() );
    m_indiP_catalog["mag_max"].set( m_catalog.magMax() );
    registerIndiPropertyReadOnly( m_indiP_catalog );

    if( createROIndiNumber( m_indiP_status, "sim_status", "Simulator status", "sim" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber sim_status" } );
    }
    m_indiP_status.add( pcf::IndiElement( "nsensors" ) );
    m_indiP_status.add( pcf::IndiElement( "banks_ready" ) );
    m_indiP_status.add( pcf::IndiElement( "frames" ) );
    m_indiP_status["nsensors"].set( static_cast<double>( m_sensors.size() ) );
    m_indiP_status["banks_ready"].set( 0.0 );
    m_indiP_status["frames"].set( 0.0 );
    registerIndiPropertyReadOnly( m_indiP_status );

    // -------------------------------------------------- per sensor INDI + subs
    for( std::unique_ptr<wccSimSensor> &sen : m_sensors )
    {
        const std::string pname = "cam_" + indiSafeName( sen->m_name );

        if( createROIndiNumber( sen->m_ipStatus, pname, sen->m_name + " status", "sensors" ) < 0 )
        {
            return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber " + pname } );
        }

        sen->m_ipStatus.add( pcf::IndiElement( "fps" ) );
        sen->m_ipStatus.add( pcf::IndiElement( "achieved_fps" ) );
        sen->m_ipStatus.add( pcf::IndiElement( "exptime" ) );
        sen->m_ipStatus.add( pcf::IndiElement( "roi_x" ) );
        sen->m_ipStatus.add( pcf::IndiElement( "roi_y" ) );
        sen->m_ipStatus.add( pcf::IndiElement( "roi_w" ) );
        sen->m_ipStatus.add( pcf::IndiElement( "roi_h" ) );
        sen->m_ipStatus.add( pcf::IndiElement( "nstars" ) );
        sen->m_ipStatus.add( pcf::IndiElement( "render_ms" ) );
        sen->m_ipStatus.add( pcf::IndiElement( "frames" ) );
        sen->m_ipStatus.add( pcf::IndiElement( "saturated" ) );
        registerIndiPropertyReadOnly( sen->m_ipStatus );

        if( registerSensorSubscriptions( sen.get() ) < 0 )
        {
            return -1;
        }
    }

    // ------------------------------------------------------ telescope subs
    if( !m_telDevice.empty() )
    {
        if( registerIndiPropertySet( m_indiP_telPos, m_telDevice, m_telProperty, st_setCallBack_remote ) < 0 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, "failed to subscribe to " + m_telDevice + "." + m_telProperty } );
        }

        remoteBinding rb;
        rb.m_sensor = nullptr;
        rb.m_what = "telpos";
        m_bindings[m_indiP_telPos.createUniqueKey()] = rb;

        log<text_log>( "pointing slaved to " + m_telDevice + "." + m_telProperty + " (" + m_telRAElement + ", " +
                       m_telDecElement + ", " + m_telPAElement + "); local pointing and offset are ignored" );
    }
    else
    {
        log<text_log>( "no telescope device configured; wccSim owns the pointing and accepts local "
                       "pointing and offset commands" );
    }

    // ----------------------------------------------- workers and PSF banks
    m_streaming = m_startStreaming;
    if( m_startStreaming )
    {
        m_indiP_streaming["toggle"].setSwitchState( pcf::IndiElement::On );
    }

    m_bankBuilder = std::thread( bankBuilderStart, this );

    for( std::unique_ptr<wccSimSensor> &sen : m_sensors )
    {
        sen->m_thread = std::thread( sensorWorkerStart, this, sen.get() );
    }

    state( stateCodes::NOTCONNECTED );

    log<text_log>( "wccSim starting with " + std::to_string( m_sensors.size() ) + " sensors, plate scale " +
                   std::to_string( m_focalPlane.arcsecPerMM() ) + " arcsec/mm, noise mode " +
                   wcc::noiseModeName( m_noiseMode ) );

    return 0;
}

inline int wccSim::appLogic()
{
    if( m_bankFault.load() )
    {
        state( stateCodes::ERROR );
        return log<software_error, -1>( { __FILE__, __LINE__, "a PSF bank failed to build; cannot simulate" } );
    }

    if( !m_banksReady.load() )
    {
        state( stateCodes::NOTCONNECTED );
    }
    else if( m_streaming.load() )
    {
        state( stateCodes::OPERATING );
    }
    else
    {
        state( stateCodes::READY );
    }

    updateStatus();

    return 0;
}

inline int wccSim::appShutdown()
{
    m_shutdownWorkers = true;

    for( std::unique_ptr<wccSimSensor> &sen : m_sensors )
    {
        try
        {
            if( sen->m_thread.joinable() )
            {
                sen->m_thread.join();
            }
        }
        catch( ... )
        {
        }
    }

    try
    {
        if( m_bankBuilder.joinable() )
        {
            m_bankBuilder.join();
        }
    }
    catch( ... )
    {
    }

    for( std::unique_ptr<wccSimSensor> &sen : m_sensors )
    {
        if( sen->m_streamOpen )
        {
            ImageStreamIO_destroyIm( &sen->m_stream );
            sen->m_streamOpen = false;
        }
    }

    return 0;
}

//------------------------------------------------------------------------
// PSF banks
//------------------------------------------------------------------------

inline void wccSim::bankBuilderStart( wccSim *s )
{
    s->bankBuilderExec();
}

inline void wccSim::bankBuilderExec()
{
    // Collect the distinct banks needed, so an array of identical detectors
    // builds one bank rather than one per sensor.
    std::map<std::string, size_t> needed;

    for( size_t i = 0; i < m_sensors.size(); ++i )
    {
        needed.emplace( m_sensors[i]->m_bankKey, i );
    }

    log<text_log>( "building " + std::to_string( needed.size() ) + " PSF bank(s) of " +
                   std::to_string( m_psfSubSteps * m_psfSubSteps ) + " x " + std::to_string( m_psfSamples ) +
                   "^2 stamps from a " + std::to_string( m_npixPupil ) + "^2 pupil; this is the slow part of "
                   "startup" );

    for( const auto &kv : needed )
    {
        if( m_shutdownWorkers.load() )
        {
            return;
        }

        const wcc::sensorConfig &sc = m_focalPlane.sensor( m_sensors[kv.second]->m_index );

        wcc::psfGenerator gen;

        wcc::pupilConfig pcfg;
        pcfg.m_npix = m_npixPupil;
        pcfg.m_diameter = m_diameter;
        pcfg.m_fNumber = m_fNumber;
        pcfg.m_centralObscuration = m_centralObscuration;
        pcfg.m_spiderVanes = m_spiderVanes;
        pcfg.m_spiderWidth = m_spiderWidth;

        if( gen.buildPupil( pcfg ) < 0 )
        {
            log<software_error>( { __FILE__, __LINE__, "buildPupil failed for bank " + kv.first } );
            m_bankFault = true;
            return;
        }

        // Sample the bandpass across the filter width so the PSF is polychromatic,
        // which is what the Python simulator's Throughput weighted sum achieves.
        wcc::bandpassConfig bp;
        bp.m_name = sc.m_filter;
        bp.m_wavelengths.clear();
        bp.m_throughput.clear();

        const int nw = 5;
        const double wl0 = sc.m_pivotWavelength - 0.5 * sc.m_bandwidth * 1e3;
        const double dwl = ( nw > 1 ) ? ( sc.m_bandwidth * 1e3 ) / ( nw - 1 ) : 0.0;

        for( int k = 0; k < nw; ++k )
        {
            const double wl = wl0 + k * dwl;

            if( wl <= 0 )
            {
                continue;
            }

            bp.m_wavelengths.push_back( wl );
            bp.m_throughput.push_back( 1.0 );
        }

        if( bp.m_wavelengths.empty() )
        {
            bp.m_wavelengths.push_back( sc.m_pivotWavelength );
            bp.m_throughput.push_back( 1.0 );
        }

        if( gen.setBandpass( bp ) < 0 )
        {
            log<software_error>( { __FILE__, __LINE__, "setBandpass failed for bank " + kv.first } );
            m_bankFault = true;
            return;
        }

        std::shared_ptr<wcc::psfBank> bnk( new wcc::psfBank );

        const double t0 = mx::sys::get_curr_time();

        if( bnk->build( gen, m_psfSamples, m_psfSubSteps, sc.m_pixelSize ) < 0 )
        {
            log<software_error>( { __FILE__, __LINE__, "psfBank::build failed for bank " + kv.first } );
            m_bankFault = true;
            return;
        }

        const double dt = mx::sys::get_curr_time() - t0;

        { //mutex scope
            std::lock_guard<std::mutex> lock( m_bankMutex );
            m_banks[kv.first] = bnk;
        }

        log<text_log>( "PSF bank " + kv.first + " built in " + std::to_string( dt ) + " s (" +
                       std::to_string( bnk->nbytes() / 1024 ) + " kB, worst sub-pixel placement error " +
                       std::to_string( bnk->placementError() ) + " px, " +
                       std::to_string( gen.resolutionElementPixels( sc.m_pixelSize ) ) +
                       " px per resolution element)" );
    }

    m_banksReady = true;

    log<text_log>( "all PSF banks ready" );
}

inline std::shared_ptr<const wcc::psfBank> wccSim::bank( const std::string &key )
{
    std::lock_guard<std::mutex> lock( m_bankMutex );

    auto it = m_banks.find( key );

    if( it == m_banks.end() )
    {
        return nullptr;
    }

    return it->second;
}

//------------------------------------------------------------------------
// Simulation workers
//------------------------------------------------------------------------

inline void wccSim::sensorWorkerStart( wccSim *s, wccSimSensor *sen )
{
    s->sensorWorkerExec( sen );
}

inline void wccSim::sensorWorkerExec( wccSimSensor *sen )
{
    std::shared_ptr<const wcc::psfBank> bnk;
    std::vector<float> frame;
    std::vector<uint16_t> pixels;

    wcc::sensorNoise noise( sen->m_seed );
    noise.mode( m_noiseMode );
    noise.bias( m_bias );

    // The sensor geometry is immutable after appStartup, so take a copy once.
    const wcc::sensorConfig sc = m_focalPlane.sensor( sen->m_index );

    double fpsAcc = 0;
    int fpsN = 0;
    double fpsWindow = mx::sys::get_curr_time();

    while( !m_shutdownWorkers.load() )
    {
        if( bnk == nullptr )
        {
            bnk = bank( sen->m_bankKey );

            if( bnk == nullptr )
            {
                mx::sys::milliSleep( 100 );
                continue;
            }
        }

        if( !m_streaming.load() )
        {
            mx::sys::milliSleep( 50 );
            continue;
        }

        const double tStart = mx::sys::get_curr_time();

        // Snapshot the live camera parameters and the boresight.
        double fps, expTime, gain;
        int bitDepth;
        wcc::roiSpec roi;

        { //mutex scope
            std::lock_guard<std::mutex> lock( sen->m_mutex );
            fps = sen->m_fps;
            expTime = sen->m_expTime;
            gain = sen->m_gain;
            bitDepth = sen->m_bitDepth;
            roi = sen->m_roi;
        }

        double ra, dec, pa;

        { //mutex scope
            std::lock_guard<std::mutex> lock( m_pointingMutex );
            ra = m_ra;
            dec = m_dec;
            pa = m_pa;
        }

        const int w = roi.imageW();
        const int h = roi.imageH();

        if( w < 1 || h < 1 )
        {
            mx::sys::milliSleep( 100 );
            continue;
        }

        int bx0 = w, by0 = h, bx1 = -1, by1 = -1;

        const int nStars = renderFrame( sc, *bnk, roi, expTime, ra, dec, pa, frame, bx0, by0, bx1, by1 );

        if( nStars < 0 )
        {
            log<software_error>( { __FILE__, __LINE__, "renderFrame failed for " + sen->m_name } );
            mx::sys::milliSleep( 250 );
            continue;
        }

        noise.apply( frame, w, h, sc, expTime, bx0, by0, bx1, by1 );

        const size_t sat = noise.digitize( frame, pixels, sc, gain, bitDepth );

        if( ensureStream( sen, static_cast<uint32_t>( w ), static_cast<uint32_t>( h ) ) == 0 )
        {
            publishFrame( sen, pixels );
        }

        const double tEnd = mx::sys::get_curr_time();
        const double render = tEnd - tStart;

        sen->m_nStars = nStars;
        sen->m_saturated = sat;
        sen->m_renderMs = 1000.0 * render;
        ++sen->m_frames;
        sen->m_ready = true;

        ++fpsN;
        fpsAcc += render;

        if( tEnd - fpsWindow >= 1.0 )
        {
            sen->m_achievedFps = fpsN / ( tEnd - fpsWindow );

            // A worker that cannot keep up is a configuration problem the operator
            // needs to see, not something to silently absorb.
            const double achieved = sen->m_achievedFps.load();
            if( fps > 0 && achieved < 0.8 * fps )
            {
                log<text_log>( sen->m_name + " cannot keep up: " + std::to_string( achieved ) + " of " +
                                   std::to_string( fps ) + " Hz requested, " +
                                   std::to_string( 1000.0 * fpsAcc / fpsN ) + " ms per frame at " +
                                   std::to_string( w ) + "x" + std::to_string( h ) + ". Reduce the ROI, the "
                                   "frame rate, or sim.noise_mode.",
                               logPrio::LOG_WARNING );
            }

            fpsN = 0;
            fpsAcc = 0;
            fpsWindow = tEnd;
        }

        // Hold the commanded cadence, accounting for the time already spent.
        const double period = ( fps > 0 ) ? 1.0 / fps : 1.0;
        const double remain = period - render;

        if( remain > 0 )
        {
            // Sleep in slices so a frame rate or shutdown change is picked up
            // promptly even when the period is long.
            double slept = 0;
            while( slept < remain && !m_shutdownWorkers.load() )
            {
                const double slice = std::min( 0.05, remain - slept );
                mx::sys::microSleep( static_cast<unsigned>( slice * 1e6 ) );
                slept += slice;
            }
        }
    }
}

inline int wccSim::renderFrame( const wcc::sensorConfig &sc,
                                const wcc::psfBank &bnk,
                                const wcc::roiSpec &roi,
                                double expTime,
                                double ra,
                                double dec,
                                double pa,
                                std::vector<float> &frame,
                                int &bx0,
                                int &by0,
                                int &bx1,
                                int &by1 )
{
    const int w = roi.imageW();
    const int h = roi.imageH();

    if( w < 1 || h < 1 )
    {
        return -1;
    }

    frame.assign( static_cast<size_t>( w ) * static_cast<size_t>( h ), 0.0f );

    // The focal plane model is shared and its pointing changes, so build a local
    // copy of the geometry at the snapshotted boresight rather than locking here.
    wcc::focalPlaneModel fp;
    fp.setTelescope( m_diameter, m_fNumber, m_parity );
    fp.setPointing( ra, dec, pa );
    fp.addSensor( sc );

    wcc::skyWCS wcs;

    if( fp.roiWCS( 0, roi, wcs ) < 0 )
    {
        return -1;
    }

    // Search a cone that covers the ROI plus a margin, so stars just outside can
    // still land part of their PSF on the detector. The Python simulator gets the
    // same effect with an oversized array and a crop.
    const double margin = m_starMargin * bnk.samples();
    const double radius = fp.roiSearchRadius( 0, roi, margin );

    double cra, cdec;
    wcs.pix2world( 0.5 * ( w - 1 ), 0.5 * ( h - 1 ), cra, cdec );

    std::vector<size_t> hits;
    m_catalog.coneSearch( cra, cdec, radius, hits, m_magLimit );

    wcc::photometryConfig phot;
    phot.m_pivotWavelength = sc.m_pivotWavelength;
    phot.m_bandwidth = sc.m_bandwidth;
    phot.m_apertureArea = wcc::apertureArea( m_diameter, m_centralObscuration );
    phot.m_throughput = m_throughput;
    phot.m_quantumEfficiency = sc.m_quantumEfficiency;

    const double half = 0.5 * bnk.samples();
    int placed = 0;

    for( size_t i : hits )
    {
        const wcc::starEntry &s = m_catalog[i];

        double x, y;

        if( !wcs.world2pix( s.m_ra, s.m_dec, x, y ) )
        {
            continue;
        }

        // Reject stars whose stamp cannot touch the frame at all.
        if( x < -half || y < -half || x > w - 1 + half || y > h - 1 + half )
        {
            continue;
        }

        const double flux = wcc::abMagToElectrons( s.m_mag, expTime, phot );

        if( !( flux > 0 ) )
        {
            continue;
        }

        // Split into an integer pixel and a sub-pixel remainder. The bank already
        // holds the PSF shifted by the remainder, so placement is exact to within
        // half a bank step.
        const int ix = static_cast<int>( std::floor( x + 0.5 ) );
        const int iy = static_cast<int>( std::floor( y + 0.5 ) );

        const float *stamp = bnk.lookup( x - ix, y - iy );

        if( stamp == nullptr )
        {
            continue;
        }

        if( wcc::accumulateStamp( frame, w, h, stamp, bnk.samples(), ix, iy, flux, bx0, by0, bx1, by1 ) > 0 )
        {
            ++placed;
        }
    }

    return placed;
}

inline int wccSim::ensureStream( wccSimSensor *sen, uint32_t w, uint32_t h )
{
    if( sen->m_streamOpen && sen->m_streamW == w && sen->m_streamH == h )
    {
        return 0;
    }

    if( sen->m_streamOpen )
    {
        ImageStreamIO_destroyIm( &sen->m_stream );
        sen->m_streamOpen = false;
    }

    // Match dev::frameGrabber exactly: naxis 3 with size[0] = width and a
    // temporal circular buffer, so downstream consumers cannot tell a simulated
    // stream from a real camera stream.
    uint32_t sizes[3] = { w, h, static_cast<uint32_t>( m_circBuffLength ) };

    if( ImageStreamIO_createIm_gpu( &sen->m_stream, sen->m_shmimOut.c_str(), 3, sizes, IMAGESTRUCT_UINT16, -1, 1,
                                    IMAGE_NB_SEMAPHORE, 0, CIRCULAR_BUFFER | ZAXIS_TEMPORAL, 0 ) !=
        IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to create stream " + sen->m_shmimOut } );
    }

    sen->m_stream.md->cnt1 = m_circBuffLength - 1;

    sen->m_streamOpen = true;
    sen->m_streamW = w;
    sen->m_streamH = h;

    log<text_log>( "created stream " + sen->m_shmimOut + " " + std::to_string( w ) + "x" + std::to_string( h ) +
                   " uint16 for " + sen->m_name );

    return 0;
}

inline int wccSim::publishFrame( wccSimSensor *sen, const std::vector<uint16_t> &pixels )
{
    if( !sen->m_streamOpen || sen->m_stream.md == nullptr )
    {
        return -1;
    }

    const size_t npix = static_cast<size_t>( sen->m_streamW ) * static_cast<size_t>( sen->m_streamH );

    if( pixels.size() < npix )
    {
        return -1;
    }

    sen->m_stream.md->write = 1;

    const uint64_t slice = ( m_circBuffLength > 1 ) ? ( sen->m_stream.md->cnt1 + 1 ) % m_circBuffLength : 0;

    uint16_t *dest = sen->m_stream.array.UI16 + slice * npix;

    memcpy( dest, pixels.data(), npix * sizeof( uint16_t ) );

    clock_gettime( CLOCK_REALTIME, &sen->m_stream.md->writetime );
    sen->m_stream.md->atime = sen->m_stream.md->writetime;

    sen->m_stream.md->cnt1 = slice;

    ImageStreamIO_UpdateIm( &sen->m_stream );

    sen->m_stream.md->write = 0;

    return 0;
}

//------------------------------------------------------------------------
// INDI
//------------------------------------------------------------------------

inline int wccSim::registerSensorSubscriptions( wccSimSensor *sen )
{
    if( sen->m_indiDevice.empty() )
    {
        return 0;
    }

    // dev::stdCamera property names. Anything a given camera does not publish
    // simply never fires a callback, leaving the configured default in place.
    const std::pair<const char *, const char *> props[] = {
        { "fps", "fps" },                   { "exptime", "exptime" },
        { "emgain", "emgain" },             { "bitDepth", "bitDepth" },
        { "roi_region_x", "roi_x" },        { "roi_region_y", "roi_y" },
        { "roi_region_w", "roi_w" },        { "roi_region_h", "roi_h" },
        { "roi_region_bin_x", "roi_bin_x" }, { "roi_region_bin_y", "roi_bin_y" },
    };

    for( const auto &p : props )
    {
        std::shared_ptr<pcf::IndiProperty> ip( new pcf::IndiProperty );

        const std::string devName = sen->m_indiDevice;
        const std::string propName = p.first;

        if( registerIndiPropertySet( *ip, devName, propName, st_setCallBack_remote ) < 0 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, "failed to subscribe to " + devName + "." + propName } );
        }

        remoteBinding rb;
        rb.m_sensor = sen;
        rb.m_what = p.second;
        m_bindings[ip->createUniqueKey()] = rb;

        sen->m_remote.push_back( ip );
    }

    log<text_log>( sen->m_name + " tracking INDI device " + sen->m_indiDevice + ", publishing to " +
                   sen->m_shmimOut );

    return 0;
}

inline int wccSim::st_setCallBack_remote( void *app, const pcf::IndiProperty &ipRecv )
{
    return static_cast<wccSim *>( app )->setCallBack_remote( ipRecv );
}

inline bool wccSim::elementValue( const pcf::IndiProperty &ip, const std::string &el, double &out )
{
    try
    {
        if( !ip.find( el ) )
        {
            return false;
        }

        // Numbers arrive as text on the wire, so parse rather than get<double>()
        // which leaves the value uninitialized on an empty element.
        const std::string s = ip[el].getValue();

        if( s.empty() )
        {
            return false;
        }

        char *end = nullptr;
        const double v = std::strtod( s.c_str(), &end );

        if( end == s.c_str() || !std::isfinite( v ) )
        {
            return false;
        }

        out = v;

        return true;
    }
    catch( ... )
    {
        return false;
    }
}

inline int wccSim::setCallBack_remote( const pcf::IndiProperty &ipRecv )
{
    auto it = m_bindings.find( ipRecv.createUniqueKey() );

    if( it == m_bindings.end() )
    {
        return 0;
    }

    const remoteBinding &rb = it->second;

    // ------------------------------------------------------------ telescope
    if( rb.m_sensor == nullptr )
    {
        double ra = 0, dec = 0, pa = 0;
        bool haveRA = elementValue( ipRecv, m_telRAElement, ra );
        bool haveDec = elementValue( ipRecv, m_telDecElement, dec );
        bool havePA = elementValue( ipRecv, m_telPAElement, pa );

        { //mutex scope
            std::lock_guard<std::mutex> lock( m_pointingMutex );

            if( haveRA )
            {
                m_ra = ra;
            }
            if( haveDec )
            {
                m_dec = dec;
            }
            if( havePA )
            {
                m_pa = pa;
            }

            m_focalPlane.setPointing( m_ra, m_dec, m_pa );
        }

        return 0;
    }

    // --------------------------------------------------------------- camera
    double v = 0;

    if( !elementValue( ipRecv, "current", v ) )
    {
        return 0;
    }

    wccSimSensor *sen = rb.m_sensor;

    std::lock_guard<std::mutex> lock( sen->m_mutex );

    if( rb.m_what == "fps" )
    {
        if( v > 0 )
        {
            sen->m_fps = v;
        }
    }
    else if( rb.m_what == "exptime" )
    {
        if( v > 0 )
        {
            sen->m_expTime = v;
        }
    }
    else if( rb.m_what == "emgain" )
    {
        // stdCamera emgain maps to CMOS analog gain on these detectors, which
        // scales electrons per DN rather than the collected signal.
        sen->m_gain = ( v > 0 ) ? v : 1.0;
    }
    else if( rb.m_what == "bitDepth" )
    {
        const int b = static_cast<int>( v );

        if( b >= 8 && b <= 16 )
        {
            sen->m_bitDepth = b;
        }
    }
    else if( rb.m_what == "roi_x" )
    {
        sen->m_roi.m_centerX = v;
    }
    else if( rb.m_what == "roi_y" )
    {
        sen->m_roi.m_centerY = v;
    }
    else if( rb.m_what == "roi_w" )
    {
        if( v >= 1 )
        {
            sen->m_roi.m_w = static_cast<int>( v );
        }
    }
    else if( rb.m_what == "roi_h" )
    {
        if( v >= 1 )
        {
            sen->m_roi.m_h = static_cast<int>( v );
        }
    }
    else if( rb.m_what == "roi_bin_x" )
    {
        if( v >= 1 )
        {
            sen->m_roi.m_binX = static_cast<int>( v );
        }
    }
    else if( rb.m_what == "roi_bin_y" )
    {
        if( v >= 1 )
        {
            sen->m_roi.m_binY = static_cast<int>( v );
        }
    }

    return 0;
}

inline void wccSim::applyPointing( double ra, double dec, double pa )
{
    if( dec > 90.0 )
    {
        dec = 90.0;
    }
    if( dec < -90.0 )
    {
        dec = -90.0;
    }

    ra = std::fmod( ra, 360.0 );
    if( ra < 0 )
    {
        ra += 360.0;
    }

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_pointingMutex );
        m_ra = ra;
        m_dec = dec;
        m_pa = pa;
        m_focalPlane.setPointing( m_ra, m_dec, m_pa );
    }

    updateIfChanged( m_indiP_pointing, "ra", ra );
    updateIfChanged( m_indiP_pointing, "dec", dec );
    updateIfChanged( m_indiP_pointing, "pa", pa );
}

inline void wccSim::updateStatus()
{
    std::unique_lock<std::mutex> lock( m_indiMutex, std::try_to_lock );

    if( !lock.owns_lock() )
    {
        return;
    }

    double ra, dec, pa;

    { //mutex scope
        std::lock_guard<std::mutex> plock( m_pointingMutex );
        ra = m_ra;
        dec = m_dec;
        pa = m_pa;
    }

    updateIfChanged( m_indiP_pointing, "ra", ra );
    updateIfChanged( m_indiP_pointing, "dec", dec );
    updateIfChanged( m_indiP_pointing, "pa", pa );

    uint64_t totalFrames = 0;

    for( std::unique_ptr<wccSimSensor> &sen : m_sensors )
    {
        double fps, expTime;
        wcc::roiSpec roi;

        { //mutex scope
            std::lock_guard<std::mutex> slock( sen->m_mutex );
            fps = sen->m_fps;
            expTime = sen->m_expTime;
            roi = sen->m_roi;
        }

        const uint64_t frames = sen->m_frames.load();
        totalFrames += frames;

        updateIfChanged( sen->m_ipStatus, "fps", fps );
        updateIfChanged( sen->m_ipStatus, "achieved_fps", sen->m_achievedFps.load() );
        updateIfChanged( sen->m_ipStatus, "exptime", expTime );
        updateIfChanged( sen->m_ipStatus, "roi_x", roi.m_centerX );
        updateIfChanged( sen->m_ipStatus, "roi_y", roi.m_centerY );
        updateIfChanged( sen->m_ipStatus, "roi_w", static_cast<double>( roi.m_w ) );
        updateIfChanged( sen->m_ipStatus, "roi_h", static_cast<double>( roi.m_h ) );
        updateIfChanged( sen->m_ipStatus, "nstars", static_cast<double>( sen->m_nStars.load() ) );
        updateIfChanged( sen->m_ipStatus, "render_ms", sen->m_renderMs.load() );
        updateIfChanged( sen->m_ipStatus, "frames", static_cast<double>( frames ) );
        updateIfChanged( sen->m_ipStatus, "saturated", static_cast<double>( sen->m_saturated.load() ) );
    }

    updateIfChanged( m_indiP_status, "banks_ready", m_banksReady.load() ? 1.0 : 0.0 );
    updateIfChanged( m_indiP_status, "frames", static_cast<double>( totalFrames ) );
}

INDI_NEWCALLBACK_DEFN( wccSim, m_indiP_streaming )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_streaming, ipRecv );

    if( !ipRecv.find( "toggle" ) )
    {
        return 0;
    }

    const bool on = ( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On );

    m_streaming = on;

    updateSwitchIfChanged( m_indiP_streaming, "toggle", on ? pcf::IndiElement::On : pcf::IndiElement::Off,
                           on ? INDI_BUSY : INDI_IDLE );

    log<text_log>( std::string( "streaming " ) + ( on ? "ON" : "OFF" ) );

    return 0;
}

INDI_NEWCALLBACK_DEFN( wccSim, m_indiP_pointing )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_pointing, ipRecv );

    if( !m_telDevice.empty() )
    {
        log<text_log>( "pointing command ignored: the pointing is slaved to " + m_telDevice + "." +
                           m_telProperty,
                       logPrio::LOG_WARNING );
        return 0;
    }

    double ra, dec, pa;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_pointingMutex );
        ra = m_ra;
        dec = m_dec;
        pa = m_pa;
    }

    elementValue( ipRecv, "ra", ra );
    elementValue( ipRecv, "dec", dec );
    elementValue( ipRecv, "pa", pa );

    applyPointing( ra, dec, pa );

    log<text_log>( "pointing -> ra " + std::to_string( ra ) + " dec " + std::to_string( dec ) + " pa " +
                   std::to_string( pa ) );

    return 0;
}

INDI_NEWCALLBACK_DEFN( wccSim, m_indiP_offset )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_offset, ipRecv );

    if( !m_telDevice.empty() )
    {
        log<text_log>( "offset command ignored: the pointing is slaved to " + m_telDevice + "." + m_telProperty,
                       logPrio::LOG_WARNING );
        return 0;
    }

    double dx = 0, dy = 0, droll = 0;
    elementValue( ipRecv, "x", dx );
    elementValue( ipRecv, "y", dy );
    elementValue( ipRecv, "roll", droll );

    if( dx == 0 && dy == 0 && droll == 0 )
    {
        return 0;
    }

    double ra, dec, pa;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_pointingMutex );
        ra = m_ra;
        dec = m_dec;
        pa = m_pa;
    }

    // The offset is a focal plane field angle. offsetBoresight() applies the same
    // tangent plane chain the sensor WCS uses, and is shared with telescopeSim so a
    // commanded correction and the resulting image motion cannot disagree.
    double nra, ndec;
    wcc::offsetBoresight( ra, dec, pa, m_parity, dx, dy, nra, ndec );

    applyPointing( nra, ndec, pa + droll );

    log<text_log>( "offset x " + std::to_string( dx ) + " y " + std::to_string( dy ) + " arcsec, roll " +
                   std::to_string( droll ) + " deg -> ra " + std::to_string( nra ) + " dec " +
                   std::to_string( ndec ) + " pa " + std::to_string( pa + droll ) );

    // Offsets are relative, so clear the request once applied.
    updateIfChanged( m_indiP_offset, "x", 0.0 );
    updateIfChanged( m_indiP_offset, "y", 0.0 );
    updateIfChanged( m_indiP_offset, "roll", 0.0 );

    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // wccSim_hpp
