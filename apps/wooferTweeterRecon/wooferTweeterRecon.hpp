/** \file wooferTweeterRecon.hpp
 * \brief The MagAO-X woofer-tweeter pseudo-open-loop reconstructor
 *
 * \ingroup wooferTweeterRecon_files
 */

#ifndef wooferTweeterRecon_hpp
#define wooferTweeterRecon_hpp

#include <limits>

#include <mx/improc/eigenCube.hpp>
#include <mx/improc/eigenImage.hpp>
#include <mx/sigproc/gramSchmidt.hpp>
#include <mx/math/templateBLAS.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

namespace MagAOX
{
namespace app
{

/** \defgroup wooferTweeterRecon Woofer Tweeter Pseudo-Open-Loop Reconstructor
 * \brief Reconstruct the open-loop wavefront from the woofer and tweeter surfaces
 *
 * Reconstructs the tweeter shape corresponding to the woofer shape, and combines the woofer and tweeter shapes
 * and the measured delta.
 *
 * <a href="../handbook/operating/software/apps/wooferTweeterRecon.html">Application Documentation</a>
 *
 * \ingroup apps
 *
 */

/** \defgroup wooferTweeterRecon_files Woofer Tweeter Pseudo-Open-Loop Reconstructor Files
 * \ingroup wooferTweeterRecon
 */

struct wooferCommandShmimT
{
    static std::string configSection()
    {
        return "wooferCommand";
    };

    static std::string indiPrefix()
    {
        return "wooferCommand";
    };
};

struct tweeterModesShmimT
{
    static std::string configSection()
    {
        return "tweeterModes";
    };

    static std::string indiPrefix()
    {
        return "tweeterModes";
    };
};

struct tweeterMaskShmimT
{
    static std::string configSection()
    {
        return "tweeterMask";
    };

    static std::string indiPrefix()
    {
        return "tweeterMask";
    };
};

struct tweeterCommandShmimT
{
    static std::string configSection()
    {
        return "tweeterCommand";
    };

    static std::string indiPrefix()
    {
        return "tweeterCommand";
    };
};

struct wfsModesShmimT
{
    static std::string configSection()
    {
        return "wfsModes";
    };

    static std::string indiPrefix()
    {
        return "wfsModes";
    };
};

/** MagAO-X application to perform pseudo-open-loop reconstruction of an offloading woofer-tweeter system
 *
 * \ingroup wooferTweeterRecon
 *
 */
class wooferTweeterRecon : public MagAOXApp<true>,
                           public dev::shmimMonitor<wooferTweeterRecon, wooferCommandShmimT>,
                           public dev::shmimMonitor<wooferTweeterRecon, tweeterCommandShmimT>,
                           public dev::shmimMonitor<wooferTweeterRecon, wfsModesShmimT>
//, public dev::telemeter<wooferTweeterRecon>
{
    // Give the test harness access.
    friend class wooferTweeterRecon_test;

    friend class dev::shmimMonitor<wooferTweeterRecon, wooferCommandShmimT>;
    typedef dev::shmimMonitor<wooferTweeterRecon, wooferCommandShmimT> wooferCommandSMT;

    friend class dev::shmimMonitor<wooferTweeterRecon, tweeterCommandShmimT>;
    typedef dev::shmimMonitor<wooferTweeterRecon, tweeterCommandShmimT> tweeterCommandSMT;

    friend class dev::shmimMonitor<wooferTweeterRecon, wfsModesShmimT>;
    typedef dev::shmimMonitor<wooferTweeterRecon, wfsModesShmimT> wfsModesSMT;

    // friend class dev::telemeter<wooferTweeterRecon>;

    // typedef dev::telemeter<wooferTweeterRecon> telemeterT;

    /// Floating point type in which to do all calculations.
    typedef float realT;

  protected:
    /** \name Configurable Parameters
     *@{
     */

    std::string m_fpsSource{ "camwfs" };

    ///@}

    float m_fps{ 0 }; ///< Current FPS from the FPS source.

    /// Mutex for locking shared memory access.
    std::mutex m_shmimMutex;

  public:
    /// Default c'tor.
    wooferTweeterRecon();

    /// D'tor, declared and defined for noexcept.
    ~wooferTweeterRecon() noexcept
    {
    }

    virtual void setupConfig();

    /// Implementation of loadConfig logic, separated for testing.
    /** This is called by loadConfig().
     */
    int loadConfigImpl( mx::app::appConfigurator &_config /**< [in] an application configuration
                        from which to load values*/
    );

    virtual void loadConfig();

    /// Startup function
    /**
     *
     */
    virtual int appStartup();

    /// Implementation of the FSM for wooferTweeterRecon.
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

    /// Allocate method for the woofer command shmimMonitor
    /**
     * \returns 0 on success
     * \returns -1 on an error
     */
    int allocate( const wooferCommandShmimT & /**< [in] tag to differentiate shmimMonitor parents.*/ );

    /// Process images for the woofer command shmimMonitor
    /**
     * \returns 0 on sucess
     * \returns -1 on an error
     */
    int processImage( void *curr_src,             ///< [in] pointer to start of current frame.
                      const wooferCommandShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    /// Allocate method for the tweeter command shmimMonitor
    /**
     * \returns 0 on success
     * \returns -1 on an error
     */
    int allocate( const tweeterCommandShmimT & /**< [in] tag to differentiate shmimMonitor parents.*/ );

    /// Process images for the tweeter command shmimMonitor
    /**
     * \returns 0 on sucess
     * \returns -1 on an error
     */
    int processImage( void *curr_src,              ///< [in] pointer to start of current frame.
                      const tweeterCommandShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    /// Allocate method for the wfs modes shmimMonitor
    /**
     * \returns 0 on success
     * \returns -1 on an error
     */
    int allocate( const wfsModesShmimT & /**< [in] tag to differentiate shmimMonitor parents.*/ );

    /// Process images for the wfs modes shmimMonitor
    /**
     * \returns 0 on sucess
     * \returns -1 on an error
     */
    int processImage( void *curr_src,        ///< [in] pointer to start of current frame.
                      const wfsModesShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int prepareModes();

  protected:
    /** \name INDI Interface
     *
     * @{
     */
    pcf::IndiProperty m_indiP_fpsSource;
    INDI_SETCALLBACK_DECL( wooferTweeterRecon, m_indiP_fpsSource );

    pcf::IndiProperty m_indiP_fps;

    ///@}

    /** \name Telemeter Interface
     *
     * @{
     */
    int checkRecordTimes();

    int recordTelem( const telem_loopgain * );

    int recordLoopGain( bool force = false );

    int recordTelem( const telem_offloading * );

    int recordOffloading( bool force = false );

    ///@}
};

inline wooferTweeterRecon::wooferTweeterRecon() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    return;
}

inline void wooferTweeterRecon::setupConfig()
{
    tweeterCommandSMT::m_shmimName = "dm00disp_delta";
    SHMIMMONITORT_SETUP_CONFIG( wooferCommandSMT, config );

    tweeterCommandSMT::m_shmimName = "dm01disp_delta";
    SHMIMMONITORT_SETUP_CONFIG( tweeterCommandSMT, config );

    wfsModesSMT::m_shmimName = "aol1_modevalWFS";
    SHMIMMONITORT_SETUP_CONFIG( wfsModesSMT, config );

    // TELEMETER_SETUP_CONFIG( config );

    config.add( "integrator.fpsSource",
                "",
                "integrator.fpsSource",
                argType::Required,
                "integrator",
                "fpsSource",
                false,
                "string",
                "Device name for getting fps of the loop.  This device should have *.fps.current.  Default is camwfs" );
}

inline int wooferTweeterRecon::loadConfigImpl( mx::app::appConfigurator &_config )
{

    SHMIMMONITORT_LOAD_CONFIG( wooferCommandSMT, _config );
    SHMIMMONITORT_LOAD_CONFIG( tweeterCommandSMT, _config );
    SHMIMMONITORT_LOAD_CONFIG( wfsModesSMT, _config );

    // TELEMETER_LOAD_CONFIG( _config );

    _config( m_fpsSource, "integrator.fpsSource" );

    return 0;
}

inline void wooferTweeterRecon::loadConfig()
{
    loadConfigImpl( config );
}

inline int wooferTweeterRecon::appStartup()
{

    REG_INDI_SETPROP( m_indiP_fpsSource, m_fpsSource, std::string( "fps" ) );

    createROIndiNumber( m_indiP_fps, "fps" );
    m_indiP_fps.add( pcf::IndiElement( "current" ) );
    if( registerIndiPropertyReadOnly( m_indiP_fps ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    SHMIMMONITORT_APP_STARTUP( wooferCommandSMT );
    SHMIMMONITORT_APP_STARTUP( tweeterCommandSMT );
    SHMIMMONITORT_APP_STARTUP( wfsModesSMT );

    // TELEMETER_APP_STARTUP;

    state( stateCodes::OPERATING );

    return 0;
}

int wooferTweeterRecon::appLogic()
{
    SHMIMMONITORT_APP_LOGIC( wooferCommandSMT );
    SHMIMMONITORT_APP_LOGIC( tweeterCommandSMT );
    SHMIMMONITORT_APP_LOGIC( wfsModesSMT );

    // TELEMETER_APP_LOGIC;

    std::unique_lock<std::mutex> lock( m_indiMutex );

    SHMIMMONITORT_UPDATE_INDI( wooferCommandSMT );
    SHMIMMONITORT_UPDATE_INDI( tweeterCommandSMT );
    SHMIMMONITORT_UPDATE_INDI( wfsModesSMT );

    return 0;
}

inline int wooferTweeterRecon::appShutdown()
{
    SHMIMMONITORT_APP_SHUTDOWN( wooferCommandSMT );
    SHMIMMONITORT_APP_SHUTDOWN( tweeterCommandSMT );
    SHMIMMONITORT_APP_SHUTDOWN( wfsModesSMT );

    // TELEMETER_APP_SHUTDOWN;

    return 0;
}

int wooferTweeterRecon::allocate( const wooferCommandShmimT & )
{
    m_wooferCommandReady = false;

    if(wooferCommandSMT::m_width != tweeterCommandSMT::m_width || !m_tweeterCommandReady)
    {
        mx::sys::milliSleep( 1000 );
        wooferCommandSMT::m_restart = true;
        return 0; // This won't log an error, but setting m_restart will cause it to loop again until sizes match
    }

    return 0;
}

int wooferTweeterRecon::processImage( void *curr_src, const wooferCommandShmimT & )
{
    return 0;
}

int wooferTweeterRecon::allocate( const tweeterCommandShmimT & )
{
    m_tweeterCommandReady = false;


    return 0;
}

int wooferTweeterRecon::processImage( void *curr_src, const tweeterCommandShmimT & )
{
    return 0;
}

int wooferTweeterRecon::allocate( const wfsModesShmimT & )
{
    m_wfsModesReady = false;

    return 0;
}

int wooferTweeterRecon::processImage( void *curr_src, const wfsModesShmimT & )
{
    return 0;
}



INDI_SETCALLBACK_DEFN( wooferTweeterRecon, m_indiP_fpsSource )( const pcf::IndiProperty &ipRecv )
{
    if( ipRecv.getName() != m_indiP_fpsSource.getName() )
    {
        log<software_error>( { __FILE__, __LINE__, "Invalid INDI property." } );
        return -1;
    }

    if( ipRecv.find( "current" ) != true ) // this isn't valie
    {
        return 0;
    }

    std::lock_guard<std::mutex> guard( m_indiMutex );

    realT fps = ipRecv["current"].get<float>();

    if( fps != m_fps )
    {
        m_fps = fps;
        updateIfChanged( m_indiP_fps, "current", m_fps );
    }

    return 0;
}

/*
int wooferTweeterRecon::checkRecordTimes()
{
    return telemeterT::checkRecordTimes( telem_loopgain(), telem_offloading() );
}

int wooferTweeterRecon::recordTelem( const telem_loopgain * )
{
    return recordLoopGain( true );
}

int wooferTweeterRecon::recordLoopGain( bool force )
{
    static uint8_t state{ 0 };
    static float   gain{ -1000 };
    static float   leak{ 0 };
    static float   limit{ 0 };

    if( state != m_offloading || gain != m_gain || leak != m_leak || limit != m_actLim || force )
    {
        state = m_offloading;
        gain  = m_gain;
        leak  = m_leak;
        limit = m_actLim;

        telem<telem_loopgain>( { state, m_gain, 1 - leak, limit } );
    }

    return 0;
}

int wooferTweeterRecon::recordTelem( const telem_offloading * )
{
    return recordOffloading( true );
}

int wooferTweeterRecon::recordOffloading( bool force )
{
    static uint32_t num_modes{ 0 };
    static uint32_t num_average{ 0 };
    float           fps{ 0 };

    if( num_modes != m_numModes || num_average != m_navg || fps != m_effFPS || force )
    {
        num_modes   = m_numModes;
        num_average = m_navg;
        fps         = m_effFPS;

        telem<telem_offloading>( { num_modes, num_average, fps } );
    }

    return 0;
}
*/

} // namespace app
} // namespace MagAOX

#endif // wooferTweeterRecon_hpp
