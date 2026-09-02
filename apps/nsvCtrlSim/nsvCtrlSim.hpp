/** \file nsvCtrlSim.hpp
  * \brief MagAO-X NSV camera simulator (stdCamera + frameGrabber, ROI modes, streaming).
  *
  * Simulates live capture into an ImageStreamIO shmim at a mode-limited framerate.
  * Frames are filled with a constant max-DN value (no noise / sensor model).
  *
  * \ingroup nsvCtrlSim_files
  */

#ifndef nsvCtrlSim_hpp
#define nsvCtrlSim_hpp

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

#include <ImageStreamIO/ImageStreamIO.h>
#include <mx/sys/timeUtils.hpp>

#include "../../libMagAOX/libMagAOX.hpp"
#include "../../magaox_git_version.h"

namespace MagAOX
{
namespace app
{

/** \defgroup nsvCtrlSim NSV Camera Simulator
  * \brief Simulate an NSV-style camera with stdCamera modes / ROI / fps / exptime.
  * \ingroup apps
  */

/** \defgroup nsvCtrlSim_files NSV Camera Simulator Files
  * \ingroup nsvCtrlSim
  */

class nsvCtrlSim : public MagAOXApp<>,
                   public dev::stdCamera<nsvCtrlSim>,
                   public dev::frameGrabber<nsvCtrlSim>,
                   public dev::telemeter<nsvCtrlSim>
{
    friend class dev::stdCamera<nsvCtrlSim>;
    friend class dev::frameGrabber<nsvCtrlSim>;
    friend class dev::telemeter<nsvCtrlSim>;

  public:
    typedef dev::stdCamera<nsvCtrlSim> stdCameraT;
    typedef dev::frameGrabber<nsvCtrlSim> frameGrabberT;
    typedef dev::telemeter<nsvCtrlSim> telemeterT;

    /** \name app::dev Configurations
     *@{
     */
    static constexpr bool c_stdCamera_tempControl = false;
    static constexpr bool c_stdCamera_temp = false;
    static constexpr bool c_stdCamera_readoutSpeed = false;
    static constexpr bool c_stdCamera_vShiftSpeed = false;
    static constexpr bool c_stdCamera_emGain = true;
    static constexpr bool c_stdCamera_blacklevel = true;
    static constexpr bool c_stdCamera_exptimeCtrl = true;
    static constexpr bool c_stdCamera_fpsCtrl = true;
    static constexpr bool c_stdCamera_fps = true;
    static constexpr bool c_stdCamera_synchro = false;
    static constexpr bool c_stdCamera_usesModes = true;
    static constexpr bool c_stdCamera_usesROI = true;
    static constexpr bool c_stdCamera_cropMode = false;
    static constexpr bool c_stdCamera_hasShutter = false;
    static constexpr bool c_stdCamera_usesStateString = false;
    static constexpr bool c_frameGrabber_flippable = false;
    ///@}

  protected:
    int m_bitDepth{ 16 }; ///< Simulated ADC bit depth (8–16). Frames stored as UINT16.
    double m_readoutOverhead_s{ 100e-9 }; ///< Reserved readout time (config; not used to cap exptime).
    uint16_t m_fillValue{ 65535 }; ///< Constant DN written into every pixel.

    bool m_streaming{ false }; ///< INDI "streaming" toggle — gates frame publication.
    bool m_fastCam{ false }; ///< Bypass mode maxFPS.

    /// Absolute fps ceiling while fast_cam is On (no ROI/mode coupling).
    static constexpr float c_fastCamAbsMaxFps{ 2000.0f };

    std::vector<uint16_t> m_frame; ///< Current simulated frame buffer.
    double m_lastTime{ 0 };
    double m_offset{ 0 };

    pcf::IndiProperty m_indiP_streamSwitch;
    INDI_NEWCALLBACK_DECL( nsvCtrlSim, m_indiP_streamSwitch );

    pcf::IndiProperty m_indiP_fastCam;
    INDI_NEWCALLBACK_DECL( nsvCtrlSim, m_indiP_fastCam );

    pcf::IndiProperty m_indiP_bitDepth;
    INDI_NEWCALLBACK_DECL( nsvCtrlSim, m_indiP_bitDepth );

  public:
    nsvCtrlSim();
    ~nsvCtrlSim() noexcept;

    virtual void setupConfig();
    /// Prefer int-returning impl — FRAMEGRABBER/TELEMETER_LOAD_CONFIG macros `return -1`.
    virtual int loadConfigImpl( mx::app::appConfigurator &cfg );
    virtual void loadConfig();
    virtual int appStartup();
    virtual int appLogic();
    virtual int appShutdown();

    // frameGrabber interface
    int configureAcquisition();
    float fps();
    int startAcquisition();
    int acquireAndCheckValid();
    int loadImageIntoStream( void *dest );
    int reconfig();

  protected:
    // stdCamera interface
    int powerOnDefaults();
    /// Stub required: /opt MagAOX stdCamera newCallBack_temp_controller wrongly
    /// dispatches on c_stdCamera_emGain (true here), so derived::setTempControl() is called.
    int setTempControl();
    int setEMGain();
    int setBlacklevel();
    int setExpTime();
    int setFPS();
    int checkNextROI();
    int setNextROI();

    int setBitDepth( int bitDepth );

    // telemeter
    int checkRecordTimes();
    int recordTelem( const telem_stdcam * );

    /// Apply named mode geometry / maxFPS from m_cameraModes. Optionally reset ROI to full mode.
    int applyMode( const std::string &modeName, bool resetROI );

    /// Clamp fps to the live mode (or fast_cam) limit. Exposure is not capped.
    void clampFpsAndExp();

    /// Default exposure if unset: 1/fps − readout overhead (not an enforced max).
    float defaultExpForFps( float fpsVal ) const;

    /// Enter/leave fast_cam (members + state() only — never sendSetProperty here).
    void applyFastCamMode( bool enable );

    /// INDI fps min/max follow fast_cam; exptime.max is unbounded.
    void publishFpsExpLimits();

    /// Refresh m_fillValue from m_bitDepth.
    void updateFillValue();

    float m_pubFpsMax{ -1.0f }; ///< Last fps.max sent on INDI (avoid SET every loop).
    float m_pubExpMax{ -1.0f }; ///< Last exptime.max sent on INDI.
};

inline nsvCtrlSim::nsvCtrlSim() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    // Always "powered" — no PDU needed for the simulator.
    m_powerMgtEnabled = false;

    m_maxEMGain = 360;
    m_emGainSet = 100;
    m_emGain = 100;
    m_blacklevelSet = 0;
    m_blacklevel = 0;
    m_maxBlacklevel = 65535;
    m_minBlacklevel = 0;

    m_minExpTime = 1e-6f;
    m_maxExpTime = std::numeric_limits<float>::max();
    m_minFPS = 0.01f;
    m_maxFPS = 1000.0f;

    return;
}

inline nsvCtrlSim::~nsvCtrlSim() noexcept
{
    return;
}

inline void nsvCtrlSim::setupConfig()
{
    config.add( "camera.bitDepth",
                "",
                "camera.bitDepth",
                argType::Required,
                "camera",
                "bitDepth",
                false,
                "int",
                "Simulated ADC bit depth (8–16). Pixel fill = 2^bitDepth-1." );
    config.add( "camera.readoutOverhead_ns",
                "",
                "camera.readoutOverhead_ns",
                argType::Required,
                "camera",
                "readoutOverhead_ns",
                false,
                "double",
                "Readout overhead in nanoseconds (kept for config compatibility; does not cap exptime)." );

    stdCameraT::setupConfig( config );
    FRAMEGRABBER_SETUP_CONFIG( config );
    TELEMETER_SETUP_CONFIG( config );
}

inline int nsvCtrlSim::loadConfigImpl( mx::app::appConfigurator &cfg )
{
    cfg( m_bitDepth, "camera.bitDepth" );
    if( m_bitDepth < 8 )
        m_bitDepth = 8;
    if( m_bitDepth > 16 )
        m_bitDepth = 16;
    updateFillValue();
    m_maxBlacklevel = m_fillValue;

    double overhead_ns = 100.0;
    cfg( overhead_ns, "camera.readoutOverhead_ns" );
    if( overhead_ns < 0 )
        overhead_ns = 0;
    m_readoutOverhead_s = overhead_ns * 1e-9;

    stdCameraT::loadConfig( cfg );

    if( m_cameraModes.empty() )
    {
        return log<software_critical, -1>(
            { __FILE__,
              __LINE__,
              "No camera modes loaded. Each mode section needs "
              "configFile=<anything> plus centerX/Y, sizeX/Y, maxFPS." } );
    }

    if( m_startupMode.empty() || m_cameraModes.count( m_startupMode ) == 0 )
    {
        return log<software_critical, -1>(
            { __FILE__, __LINE__, "camera.startupMode missing or not found among mode sections." } );
    }

    m_modeName = m_startupMode;
    m_nextMode.clear();

    if( applyMode( m_modeName, false ) < 0 )
        return -1;

    // Prefer configured default_* ROI if present; otherwise full mode frame.
    if( m_default_w > 0 && m_default_h > 0 )
    {
        m_currentROI.x = m_default_x;
        m_currentROI.y = m_default_y;
        m_currentROI.w = m_default_w;
        m_currentROI.h = m_default_h;
        m_currentROI.bin_x = ( m_default_bin_x > 0 ) ? m_default_bin_x : 1;
        m_currentROI.bin_y = ( m_default_bin_y > 0 ) ? m_default_bin_y : 1;
        m_nextROI = m_currentROI;
    }

    FRAMEGRABBER_LOAD_CONFIG( cfg );
    TELEMETER_LOAD_CONFIG( cfg );
    return 0;
}

inline void nsvCtrlSim::loadConfig()
{
    if( loadConfigImpl( config ) < 0 )
        m_shutdown = true;
}

inline int nsvCtrlSim::appStartup()
{
    createStandardIndiToggleSw( m_indiP_streamSwitch, "streaming", "Start/stop simulated streaming" );
    if( registerIndiPropertyNew( m_indiP_streamSwitch, INDI_NEWCALLBACK( m_indiP_streamSwitch ) ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }
    // Default Off — operator must toggle streaming to begin publishing frames.
    m_indiP_streamSwitch["toggle"].setSwitchState( pcf::IndiElement::Off );

    createStandardIndiToggleSw( m_indiP_fastCam, "fast_cam", "Bypass ROI mode maxFPS" );
    if( registerIndiPropertyNew( m_indiP_fastCam, INDI_NEWCALLBACK( m_indiP_fastCam ) ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }
    m_indiP_fastCam["toggle"].setSwitchState( pcf::IndiElement::Off );

    createStandardIndiNumber<int>( m_indiP_bitDepth, "bitDepth", 8, 16, 2, "%d", "ADC bit depth", "camera" );
    m_indiP_bitDepth["current"] = m_bitDepth;
    m_indiP_bitDepth["target"] = m_bitDepth;
    if( registerIndiPropertyNew( m_indiP_bitDepth, INDI_NEWCALLBACK( m_indiP_bitDepth ) ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( stdCameraT::appStartup() < 0 )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__ } );
    }

    // Seed currents so GetProperties Def is not empty. Empty current makes
    // subscribers' IndiElement::get<double>() leave an uninitialized value (often nan).
    m_indiP_exptime["current"] = m_expTime;
    m_indiP_exptime["target"] = m_expTimeSet;
    m_indiP_fps["current"] = m_fps;
    m_indiP_fps["target"] = m_fpsSet;
    m_indiP_emGain["current"] = m_emGain;
    m_indiP_emGain["target"] = m_emGainSet;
    m_indiP_blacklevel["current"] = m_blacklevel;
    m_indiP_blacklevel["target"] = m_blacklevelSet;
    m_indiP_roi_x["current"] = m_currentROI.x;
    m_indiP_roi_x["target"] = m_nextROI.x;
    m_indiP_roi_y["current"] = m_currentROI.y;
    m_indiP_roi_y["target"] = m_nextROI.y;
    m_indiP_roi_w["current"] = m_currentROI.w;
    m_indiP_roi_w["target"] = m_nextROI.w;
    m_indiP_roi_h["current"] = m_currentROI.h;
    m_indiP_roi_h["target"] = m_nextROI.h;
    FRAMEGRABBER_APP_STARTUP;
    TELEMETER_APP_STARTUP;

    m_lastTime = mx::sys::get_curr_time();
    m_offset = 0;

    // Connected but not yet streaming (mirrors pixelinkCtrl pattern).
    state( stateCodes::CONNECTED );
    log<text_log>( "nsvCtrlSim ready (mode=" + m_modeName + ", maxFPS=" + std::to_string( m_maxFPS ) +
                   "). Toggle streaming to publish; fast_cam raises fps.max to 2000 Hz. "
                   "exptime has no maximum." );
    return 0;
}

inline int nsvCtrlSim::appLogic()
{
    if( stdCameraT::appLogic() < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    FRAMEGRABBER_APP_LOGIC;
    TELEMETER_APP_LOGIC;

    // Keep FSM state consistent with streaming / fast_cam when not reconfiguring.
    if( state() != stateCodes::CONFIGURING && state() != stateCodes::ERROR )
    {
        if( m_streaming || m_fastCam )
            state( stateCodes::OPERATING );
        else if( state() == stateCodes::OPERATING || state() == stateCodes::READY )
            state( stateCodes::CONNECTED );
    }

    // Stock MagAOX pattern: try_lock then publish currents. Do not add extra SETs here —
    // holding m_indiMutex across many sendXml calls while the IndiDriver thread waits on
    // the same mutex can wedge the FIFOs and drop the device from indiserver.
    if( state() == stateCodes::READY || state() == stateCodes::OPERATING ||
        state() == stateCodes::CONNECTED )
    {
        std::unique_lock<std::mutex> lock( m_indiMutex, std::try_to_lock );
        if( !lock.owns_lock() )
            return 0;

        if( stdCameraT::updateINDI() < 0 )
        {
            log<software_error>( { __FILE__, __LINE__ } );
            state( stateCodes::ERROR );
            return 0;
        }

        // ROI currents are not in stdCamera::updateINDI (only published at
        // configureAcquisition). updateIfChanged emits a SET only when the value
        // changes, same as exptime/emgain above.
        updateIfChanged( m_indiP_roi_x, "current", m_currentROI.x, INDI_OK );
        updateIfChanged( m_indiP_roi_y, "current", m_currentROI.y, INDI_OK );
        updateIfChanged( m_indiP_roi_w, "current", m_currentROI.w, INDI_OK );
        updateIfChanged( m_indiP_roi_h, "current", m_currentROI.h, INDI_OK );

        FRAMEGRABBER_UPDATE_INDI;
        publishFpsExpLimits();
    }

    return 0;
}

inline int nsvCtrlSim::appShutdown()
{
    stdCameraT::appShutdown();
    FRAMEGRABBER_APP_SHUTDOWN;
    TELEMETER_APP_SHUTDOWN;
    return 0;
}

inline int nsvCtrlSim::applyMode( const std::string &modeName, bool resetROI )
{
    auto it = m_cameraModes.find( modeName );
    if( it == m_cameraModes.end() )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "unknown camera mode: " + modeName } );
    }

    const auto &m = it->second;
    m_full_x = m.m_centerX;
    m_full_y = m.m_centerY;
    m_full_w = m.m_sizeX;
    m_full_h = m.m_sizeY;
    m_maxFPS = m.m_maxFPS;
    if( m_maxFPS <= 0 )
        m_maxFPS = 1.0f;

    if( resetROI || m_nextROI.w == 0 || m_nextROI.h == 0 )
    {
        m_nextROI.x = m_full_x;
        m_nextROI.y = m_full_y;
        m_nextROI.w = m_full_w;
        m_nextROI.h = m_full_h;
        m_nextROI.bin_x = 1;
        m_nextROI.bin_y = 1;
        m_currentROI = m_nextROI;
    }
    else
    {
        // Keep requested ROI but clamp into the new mode frame.
        if( m_nextROI.w > m_full_w )
            m_nextROI.w = m_full_w;
        if( m_nextROI.h > m_full_h )
            m_nextROI.h = m_full_h;
        if( m_nextROI.x + 0.5 * m_nextROI.w > m_full_w || m_nextROI.x < 0.5 * m_nextROI.w )
            m_nextROI.x = m_full_x;
        if( m_nextROI.y + 0.5 * m_nextROI.h > m_full_h || m_nextROI.y < 0.5 * m_nextROI.h )
            m_nextROI.y = m_full_y;
    }

    // Default to mode maxFPS on mode apply if unset / over limit (unless fast_cam).
    if( m_fastCam )
    {
        if( m_fpsSet <= 0 )
            m_fpsSet = m_maxFPS;
        m_fps = m_fpsSet;
    }
    else
    {
        if( m_fps <= 0 || m_fps > m_maxFPS )
            m_fps = m_maxFPS;
        m_fpsSet = m_fps;
    }
    clampFpsAndExp();

    log<text_log>( "mode=" + modeName + " full=" + std::to_string( m_full_w ) + "x" +
                   std::to_string( m_full_h ) + " maxFPS=" + std::to_string( m_maxFPS ) );
    return 0;
}

inline float nsvCtrlSim::defaultExpForFps( float fpsVal ) const
{
    if( fpsVal <= 0 )
        return m_minExpTime;
    const double period = 1.0 / static_cast<double>( fpsVal );
    const double exp = period - m_readoutOverhead_s;
    if( exp < static_cast<double>( m_minExpTime ) )
        return m_minExpTime;
    return static_cast<float>( exp );
}

inline void nsvCtrlSim::clampFpsAndExp()
{
    if( m_fastCam )
    {
        if( m_fpsSet < m_minFPS )
            m_fpsSet = m_minFPS;
        if( m_fpsSet > c_fastCamAbsMaxFps )
            m_fpsSet = c_fastCamAbsMaxFps;
        m_fps = m_fpsSet;
    }
    else
    {
        if( m_fpsSet < m_minFPS )
            m_fpsSet = m_minFPS;
        if( m_fpsSet > m_maxFPS )
            m_fpsSet = m_maxFPS;
        if( m_fps < m_minFPS )
            m_fps = m_minFPS;
        if( m_fps > m_maxFPS )
            m_fps = m_maxFPS;
        m_fpsSet = m_fps;
    }

    // Exposure is independent of fps. Only a floor; INDI max stays unbounded.
    m_maxExpTime = std::numeric_limits<float>::max();
    if( m_expTimeSet <= 0 && m_expTime <= 0 )
    {
        m_expTime = defaultExpForFps( m_fps );
        m_expTimeSet = m_expTime;
    }
    if( m_expTimeSet < m_minExpTime )
        m_expTimeSet = m_minExpTime;
    if( m_expTime < m_minExpTime )
        m_expTime = m_minExpTime;
    if( m_expTimeSet > 0 )
        m_expTime = m_expTimeSet;
}

inline void nsvCtrlSim::applyFastCamMode( bool enable )
{
    // Members + state() only. Never call updateIfChanged / sendSetProperty here.
    m_fastCam = enable;
    if( m_fastCam )
    {
        clampFpsAndExp();
        // Ensure shmim publishing without touching INDI from this path.
        if( !m_streaming )
        {
            m_streaming = true;
            log<text_log>( "fast_cam ON auto-enabled streaming (member only)" );
        }
        state( stateCodes::OPERATING );
        log<text_log>( "fast_cam ON (bypass mode maxFPS; INDI publish via appLogic)" );
    }
    else
    {
        clampFpsAndExp();
        if( m_streaming )
            state( stateCodes::OPERATING );
        else
            state( stateCodes::CONNECTED );
        log<text_log>( "fast_cam OFF fps=" + std::to_string( m_fps ) +
                       " exptime=" + std::to_string( m_expTime ) +
                       " maxFPS=" + std::to_string( m_maxFPS ) );
    }

    m_lastTime = mx::sys::get_curr_time();
    m_offset = 0;
}

inline void nsvCtrlSim::publishFpsExpLimits()
{
    if( !m_indiDriver )
        return;

    const float fpsMax = m_fastCam ? c_fastCamAbsMaxFps : m_maxFPS;
    const float expMax = std::numeric_limits<float>::max();

    if( fpsMax != m_pubFpsMax && m_indiP_fps.find( "current" ) && m_indiP_fps.find( "target" ) )
    {
        m_indiP_fps["current"].setMin( m_minFPS );
        m_indiP_fps["current"].setMax( fpsMax );
        m_indiP_fps["target"].setMin( m_minFPS );
        m_indiP_fps["target"].setMax( fpsMax );
        m_pubFpsMax = fpsMax;
        m_indiDriver->sendSetProperty( m_indiP_fps );
    }

    if( expMax != m_pubExpMax && m_indiP_exptime.find( "current" ) && m_indiP_exptime.find( "target" ) )
    {
        m_indiP_exptime["current"].setMin( m_minExpTime );
        m_indiP_exptime["current"].setMax( expMax );
        m_indiP_exptime["target"].setMin( m_minExpTime );
        m_indiP_exptime["target"].setMax( expMax );
        m_pubExpMax = expMax;
        m_indiDriver->sendSetProperty( m_indiP_exptime );
    }
}

inline void nsvCtrlSim::updateFillValue()
{
    if( m_bitDepth >= 16 )
        m_fillValue = 65535;
    else
        m_fillValue = static_cast<uint16_t>( ( 1u << m_bitDepth ) - 1u );
}

//------------------------------------------------------------------------
// frameGrabber
//------------------------------------------------------------------------

inline int nsvCtrlSim::configureAcquisition()
{
    // Geometry / buffer work outside m_indiMutex so appLogic can still publish.
    {
        std::unique_lock<std::mutex> lock( m_indiMutex );
        recordCamera( true );
        m_currentROI = m_nextROI;
    }

    if( m_currentROI.w < 1 )
        m_currentROI.w = 1;
    if( m_currentROI.h < 1 )
        m_currentROI.h = 1;
    if( m_currentROI.bin_x < 1 )
        m_currentROI.bin_x = 1;
    if( m_currentROI.bin_y < 1 )
        m_currentROI.bin_y = 1;

    m_width = static_cast<uint32_t>( m_currentROI.w / m_currentROI.bin_x );
    m_height = static_cast<uint32_t>( m_currentROI.h / m_currentROI.bin_y );
    m_xbinning = m_currentROI.bin_x;
    m_ybinning = m_currentROI.bin_y;

    m_frame.assign( static_cast<size_t>( m_width ) * static_cast<size_t>( m_height ), m_fillValue );

    m_dataType = IMAGESTRUCT_UINT16;
    m_typeSize = imageStructDataType<IMAGESTRUCT_UINT16>::size;

    {
        std::unique_lock<std::mutex> lock( m_indiMutex );
        updateIfChanged( m_indiP_roi_x, "current", m_currentROI.x, INDI_OK );
        updateIfChanged( m_indiP_roi_y, "current", m_currentROI.y, INDI_OK );
        updateIfChanged( m_indiP_roi_w, "current", m_currentROI.w, INDI_OK );
        updateIfChanged( m_indiP_roi_h, "current", m_currentROI.h, INDI_OK );
        updateIfChanged( m_indiP_roi_bin_x, "current", m_currentROI.bin_x, INDI_OK );
        updateIfChanged( m_indiP_roi_bin_y, "current", m_currentROI.bin_y, INDI_OK );
        recordCamera( true );
    }
    return 0;
}

inline float nsvCtrlSim::fps()
{
    return ( m_fps > 0.0f ) ? m_fps : m_fpsSet;
}

inline int nsvCtrlSim::startAcquisition()
{
    m_offset = 0;
    m_lastTime = mx::sys::get_curr_time();
    if( m_streaming || m_fastCam )
        state( stateCodes::OPERATING );
    return 0;
}

inline int nsvCtrlSim::acquireAndCheckValid()
{
    // Frame cadence follows fps.current (re-read so fps changes interrupt long waits).
    while( !shutdown() )
    {
        const double fps = static_cast<double>( this->fps() );
        const double period = ( fps > 0.0 ) ? ( 1.0 / fps ) : 1.0;

        const double et = mx::sys::get_curr_time() - m_lastTime;
        const double remain = period - m_offset - et;
        if( remain <= 0.0 )
            break;

        double sleep_s = remain;
        if( sleep_s > 0.002 )
            sleep_s = 0.002;
        unsigned us = static_cast<unsigned>( sleep_s * 1e6 );
        if( us < 1 )
            us = 1;
        mx::sys::microSleep( us );
    }
    if( shutdown() )
        return -1;

    double dt = mx::sys::get_curr_time( m_currImageTimestamp );
    const double fps = static_cast<double>( this->fps() );
    const double period = ( fps > 0.0 ) ? ( 1.0 / fps ) : 1.0;
    m_offset += 0.1 * ( ( dt - m_lastTime ) - period );
    if( m_offset > 0.25 * period )
        m_offset = 0.25 * period;
    if( m_offset < -0.25 * period )
        m_offset = -0.25 * period;
    m_lastTime = dt;

    if( !m_streaming && !m_fastCam )
        return 1;

    if( m_frame.empty() )
        return 1;

    std::fill( m_frame.begin(), m_frame.end(), m_fillValue );
    return 0;
}

inline int nsvCtrlSim::loadImageIntoStream( void *dest )
{
    if( frameGrabberT::loadImageIntoStreamCopy(
            dest, m_frame.data(), m_width, m_height, sizeof( uint16_t ) ) == nullptr )
    {
        return -1;
    }
    return 0;
}

inline int nsvCtrlSim::reconfig()
{
    std::unique_lock<std::mutex> lock( m_indiMutex );
    recordCamera( true );
    state( stateCodes::CONFIGURING );

    if( !m_nextMode.empty() && m_nextMode != m_modeName )
    {
        if( applyMode( m_nextMode, true ) < 0 )
        {
            state( stateCodes::ERROR );
            return -1;
        }
        m_modeName = m_nextMode;
    }

    m_nextMode.clear();

    if( m_streaming || m_fastCam )
        state( stateCodes::OPERATING );
    else
        state( stateCodes::CONNECTED );

    recordCamera( true );
    return 0;
}

//------------------------------------------------------------------------
// stdCamera
//------------------------------------------------------------------------

inline int nsvCtrlSim::powerOnDefaults()
{
    m_nextROI.x = m_default_x;
    m_nextROI.y = m_default_y;
    m_nextROI.w = m_default_w;
    m_nextROI.h = m_default_h;
    m_nextROI.bin_x = m_default_bin_x > 0 ? m_default_bin_x : 1;
    m_nextROI.bin_y = m_default_bin_y > 0 ? m_default_bin_y : 1;
    m_currentROI = m_nextROI;
    m_reconfig = true;
    return 0;
}

inline int nsvCtrlSim::setTempControl()
{
    // No-op stub (see class comment). Keep status strings consistent if ever queried.
    m_tempControlStatus = m_tempControlStatusSet;
    m_tempControlStatusStr = m_tempControlStatus ? "ON" : "OFF";
    return 0;
}

inline int nsvCtrlSim::setEMGain()
{
    // NSV maps stdCamera "emgain" to CMOS analog gain.
    if( m_emGainSet < 0 )
        m_emGainSet = 0;
    if( m_emGainSet > m_maxEMGain )
        m_emGainSet = m_maxEMGain;
    m_emGain = m_emGainSet;
    log<text_log>( "Set gain (emgain): " + std::to_string( m_emGain ) );
    return 0;
}

inline int nsvCtrlSim::setBlacklevel()
{
    if( m_blacklevelSet < m_minBlacklevel )
        m_blacklevelSet = m_minBlacklevel;
    if( m_blacklevelSet > m_maxBlacklevel )
        m_blacklevelSet = m_maxBlacklevel;
    m_blacklevel = m_blacklevelSet;
    log<text_log>( "Set blacklevel: " + std::to_string( m_blacklevel ) );
    return 0;
}

inline int nsvCtrlSim::setBitDepth( int bitDepth )
{
    if( bitDepth != 8 && bitDepth != 10 && bitDepth != 12 && bitDepth != 14 && bitDepth != 16 )
    {
        log<text_log>( "invalid bitDepth " + std::to_string( bitDepth ) + "; keeping " +
                           std::to_string( m_bitDepth ),
                       logPrio::LOG_WARNING );
        return -1;
    }
    m_bitDepth = bitDepth;
    updateFillValue();
    m_maxBlacklevel = m_fillValue;
    if( m_blacklevel > m_maxBlacklevel )
    {
        m_blacklevel = m_maxBlacklevel;
        m_blacklevelSet = m_blacklevel;
    }
    // Rebuild frame buffer contents on next acquire; size unchanged.
    log<text_log>( "Set bitDepth to " + std::to_string( m_bitDepth ) +
                   " (fill=" + std::to_string( m_fillValue ) + ")" );
    return 0;
}

inline int nsvCtrlSim::setExpTime()
{
    float exp = m_expTimeSet;
    if( exp < m_minExpTime )
    {
        exp = m_minExpTime;
        log<text_log>( "exptime limited to min " + std::to_string( m_minExpTime ) + " s",
                       logPrio::LOG_WARNING );
    }

    m_maxExpTime = std::numeric_limits<float>::max();
    m_expTime = exp;
    m_expTimeSet = exp;
    log<text_log>( "Set exposure time: " + std::to_string( m_expTime ) + " s" );
    return 0;
}

inline int nsvCtrlSim::setFPS()
{
    float fr = m_fpsSet;
    if( fr < m_minFPS )
    {
        fr = m_minFPS;
        log<text_log>( "FPS limited to min " + std::to_string( m_minFPS ), logPrio::LOG_WARNING );
    }

    if( m_fastCam )
    {
        if( fr > c_fastCamAbsMaxFps )
        {
            fr = c_fastCamAbsMaxFps;
            log<text_log>( "FPS limited to absolute max " + std::to_string( c_fastCamAbsMaxFps ) +
                               " (fast_cam)",
                           logPrio::LOG_WARNING );
        }
        m_fpsSet = fr;
        m_fps = fr;
        // Do not recap exptime from the new fps while fast_cam is on.
        log<text_log>( "Set frame rate: " + std::to_string( m_fps ) + " fps (fast_cam, no exptime recap)" );
    }
    else
    {
        if( fr > m_maxFPS )
        {
            fr = m_maxFPS;
            log<text_log>( "FPS limited to mode max " + std::to_string( m_maxFPS ), logPrio::LOG_WARNING );
        }
        m_fpsSet = fr;
        m_fps = fr;
        log<text_log>( "Set frame rate: " + std::to_string( m_fps ) + " fps" );
    }

    m_lastTime = mx::sys::get_curr_time();
    m_offset = 0;
    return 0;
}

inline int nsvCtrlSim::checkNextROI()
{
    updateIfChanged( m_indiP_roi_x, "target", m_nextROI.x, INDI_OK );
    updateIfChanged( m_indiP_roi_y, "target", m_nextROI.y, INDI_OK );
    updateIfChanged( m_indiP_roi_w, "target", m_nextROI.w, INDI_OK );
    updateIfChanged( m_indiP_roi_h, "target", m_nextROI.h, INDI_OK );
    updateIfChanged( m_indiP_roi_bin_x, "target", m_nextROI.bin_x, INDI_OK );
    updateIfChanged( m_indiP_roi_bin_y, "target", m_nextROI.bin_y, INDI_OK );
    return 0;
}

inline int nsvCtrlSim::setNextROI()
{
    m_reconfig = true;
    updateSwitchIfChanged( m_indiP_roi_set, "request", pcf::IndiElement::Off, INDI_IDLE );
    updateSwitchIfChanged( m_indiP_roi_full, "request", pcf::IndiElement::Off, INDI_IDLE );
    updateSwitchIfChanged( m_indiP_roi_last, "request", pcf::IndiElement::Off, INDI_IDLE );
    updateSwitchIfChanged( m_indiP_roi_default, "request", pcf::IndiElement::Off, INDI_IDLE );
    return 0;
}

inline int nsvCtrlSim::checkRecordTimes()
{
    return telemeterT::checkRecordTimes( telem_stdcam() );
}

inline int nsvCtrlSim::recordTelem( const telem_stdcam * )
{
    return recordCamera( true );
}

INDI_NEWCALLBACK_DEFN( nsvCtrlSim, m_indiP_streamSwitch )
( const pcf::IndiProperty &ipRecv )
{
    if( ipRecv.getName() != m_indiP_streamSwitch.getName() )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "wrong INDI property received." } );
    }

    if( !ipRecv.find( "toggle" ) )
        return 0;

    const bool wantOn = ( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On );
    {
        std::unique_lock<std::mutex> lock( m_indiMutex );
        if( wantOn )
        {
            m_streaming = true;
            m_lastTime = mx::sys::get_curr_time();
            m_offset = 0;
            state( stateCodes::OPERATING );
        }
        else
        {
            m_streaming = false;
            if( !m_fastCam )
                state( stateCodes::CONNECTED );
        }
    }
    // Publish AFTER releasing m_indiMutex so we never hold it across sendXml.
    if( wantOn )
    {
        updateSwitchIfChanged( m_indiP_streamSwitch, "toggle", pcf::IndiElement::On, INDI_OK );
        log<text_log>( "streaming ON" );
    }
    else
    {
        updateSwitchIfChanged( m_indiP_streamSwitch, "toggle", pcf::IndiElement::Off, INDI_IDLE );
        log<text_log>( "streaming OFF" );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( nsvCtrlSim, m_indiP_fastCam )
( const pcf::IndiProperty &ipRecv )
{
    if( ipRecv.getName() != m_indiP_fastCam.getName() )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "wrong INDI property received." } );
    }

    if( !ipRecv.find( "toggle" ) )
        return 0;

    const bool wantOn = ( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On );
    bool streamingOn = false;
    {
        std::unique_lock<std::mutex> lock( m_indiMutex );
        if( wantOn != m_fastCam )
            applyFastCamMode( wantOn );
        streamingOn = m_streaming;
    }
    // Ack AFTER unlock — never send fps/exptime from this callback.
    updateSwitchIfChanged( m_indiP_fastCam,
                           "toggle",
                           wantOn ? pcf::IndiElement::On : pcf::IndiElement::Off,
                           wantOn ? INDI_OK : INDI_IDLE );
    // Mirror auto-enabled streaming so INDI matches members.
    if( streamingOn )
        updateSwitchIfChanged( m_indiP_streamSwitch, "toggle", pcf::IndiElement::On, INDI_OK );
    return 0;
}

INDI_NEWCALLBACK_DEFN( nsvCtrlSim, m_indiP_bitDepth )
( const pcf::IndiProperty &ipRecv )
{
    if( ipRecv.getName() != m_indiP_bitDepth.getName() )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "wrong INDI property received." } );
    }

    int target = m_bitDepth;
    int rc = 0;
    {
        std::unique_lock<std::mutex> lock( m_indiMutex );
        if( indiTargetUpdate( m_indiP_bitDepth, target, ipRecv, false ) < 0 )
            return -1;
        rc = setBitDepth( target );
    }
    // indiTargetUpdate already sent target; publish settled values after unlock.
    updateIfChanged( m_indiP_bitDepth, "target", m_bitDepth );
    updateIfChanged( m_indiP_bitDepth, "current", m_bitDepth );
    return rc;
}

} // namespace app
} // namespace MagAOX

#endif // nsvCtrlSim_hpp
