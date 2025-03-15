/** \file psdGainOpt.hpp
 * \brief The MagAO-X PSD-based gain optimizer header file
 *
 * \ingroup psdGainOpt_files
 */

#ifndef psdGainOpt_hpp
#define psdGainOpt_hpp

#include <mx/ao/analysis/clGainOpt.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

/** \defgroup psdGainOpt
 * \brief The MagAO-X application to perform PSD-based gain optimization
 *
 * <a href="../handbook/operating/software/apps/XXXXXX.html">Application Documentation</a>
 *
 * \ingroup apps
 *
 */

/** \defgroup psdGainOpt_files
 * \ingroup psdGainOpt
 */

namespace MagAOX
{
namespace app
{

struct psdShmimT
{
    static std::string configSection()
    {
        return "psdShmim";
    };

    static std::string indiPrefix()
    {
        return "psd";
    };
};

struct freqShmimT
{
    static std::string configSection()
    {
        return "freqShmim";
    };

    static std::string indiPrefix()
    {
        return "freq";
    };
};

struct gainShmimT
{
    static std::string configSection()
    {
        return "gainShmim";
    };

    static std::string indiPrefix()
    {
        return "gain";
    };
};

struct multcoShmimT
{
    static std::string configSection()
    {
        return "multcoShmim";
    };

    static std::string indiPrefix()
    {
        return "multco";
    };
};

struct gainCalShmimT
{
    static std::string configSection()
    {
        return "gainCalShmim";
    };

    static std::string indiPrefix()
    {
        return "gainCal";
    };
};

struct tauShmimT
{
    static std::string configSection()
    {
        return "tauShmim";
    };

    static std::string indiPrefix()
    {
        return "tau";
    };
};

/// The MagAO-X PSD-based gain optimizer
/**
 * \ingroup psdGainOpt
 */
class psdGainOpt : public MagAOXApp<true>,
                   dev::shmimMonitor<psdGainOpt, psdShmimT>,
                   dev::shmimMonitor<psdGainOpt, freqShmimT>,
                   dev::shmimMonitor<psdGainOpt, gainShmimT>,
                   dev::shmimMonitor<psdGainOpt, multcoShmimT>,
                   dev::shmimMonitor<psdGainOpt, gainCalShmimT>,
                   dev::shmimMonitor<psdGainOpt, tauShmimT>
{

    // Give the test harness access.
    friend class psdGainOpt_test;

    friend class dev::shmimMonitor<psdGainOpt, psdShmimT>;
    friend class dev::shmimMonitor<psdGainOpt, freqShmimT>;
    friend class dev::shmimMonitor<psdGainOpt, gainShmimT>;
    friend class dev::shmimMonitor<psdGainOpt, multcoShmimT>;
    friend class dev::shmimMonitor<psdGainOpt, gainCalShmimT>;
    friend class dev::shmimMonitor<psdGainOpt, tauShmimT>;

  public:
    typedef dev::shmimMonitor<psdGainOpt, psdShmimT>     psdShmimMonitorT;
    typedef dev::shmimMonitor<psdGainOpt, freqShmimT>    freqShmimMonitorT;
    typedef dev::shmimMonitor<psdGainOpt, gainShmimT>    gainShmimMonitorT;
    typedef dev::shmimMonitor<psdGainOpt, multcoShmimT>  multcoShmimMonitorT;
    typedef dev::shmimMonitor<psdGainOpt, gainCalShmimT> gainCalShmimMonitorT;
    typedef dev::shmimMonitor<psdGainOpt, tauShmimT>     tauShmimMonitorT;

    typedef std::chrono::time_point<std::chrono::steady_clock> timePointT;
    typedef std::chrono::duration<double>                      durationT;

  protected:
    /** \name Configurable Parameters
     *@{
     */

    int         m_loopNum{ 1 }; ///< The number of the loop. Used to set shmim names, as in aolN_mgainfact.
    std::string m_loopName;     ///< The name of the loop control INDI device name.

    std::string m_psdDevice; /**< The INDI device name of the PSD calculator.  Defaults to aolN_modevalPSDs
                                  where N is m_loopNum.*/

    bool m_autoUpdate{ false }; ///< Flag controlling whether gains are automatically updated
    ///@}

    float m_fps{ 0 };

    /// Each mode gets its own gain optimizer
    std::vector<mx::AO::analysis::clGainOpt<float>> m_gopt;
    bool m_goptUpdated{ true }; ///< Tracks if a parameter has updated requiring updates to the m_gopt entries.
    bool m_freqUpdated{ true }; /**< Tracks if the frequency scale has updated, which necessitates additional calcs.
                                     If true, implies m_goptUpdate == true.*/
    float m_psdTime{ 1 };
    float m_psdAvgTime{ 10 };
    float m_psdOverlapFraction{ 0.5 };

    std::vector<float> m_freq;

    mx::improc::eigenImage<float>   m_clPSDs;
    std::vector<std::vector<float>> m_olPSDs;
    std::vector<std::vector<float>> m_nPSDs;

    std::vector<float> m_optGain;
    std::vector<float> m_modeVar;

    bool m_loop{ false };

    float m_gain{ 0 };

    std::vector<float> m_gains;

    float m_mc{ 1 };

    std::vector<float> m_mcs;

    std::vector<float> m_gainCals;

    std::vector<float> m_taus;

    int m_sinceChange{ -1 };

  public:
    /// Default c'tor.
    psdGainOpt();

    /// D'tor, declared and defined for noexcept.
    ~psdGainOpt() noexcept
    {
    }

    virtual void setupConfig();

    /// Implementation of loadConfig logic, separated for testing.
    /** This is called by loadConfig().
     */
    int loadConfigImpl( mx::app::appConfigurator &_config /**< [in] an application configuration
                                                                    from which to load values*/ );

    virtual void loadConfig();

    /// Startup function
    /**
     *
     */
    virtual int appStartup();

    /// Implementation of the FSM for psdGainOpt.
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

    int allocate( const psdShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,   ///< [in] pointer to the start of the current frame
                      const psdShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const freqShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,    ///< [in] pointer to the start of the current frame
                      const freqShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const gainShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,    ///< [in] pointer to the start of the current frame
                      const gainShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const multcoShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,      ///< [in] pointer to the start of the current frame
                      const multcoShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const gainCalShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,       ///< [in] pointer to the start of the current frame
                      const gainCalShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const tauShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,   ///< [in] pointer to the start of the current frame
                      const tauShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

  protected:
    /// Mutex for synchronizing updates.
    std::mutex m_goptMutex;

    /// Flag used to indicate to the goptThread that it should stop calculations ASAP
    bool m_updating{ false };

    /** \name Gain Optimization Thread
     *
     * @{
     */
    int         m_goptThreadPrio{ 0 }; ///< Priority of the gain optimization thread.
    std::string m_goptThreadCpuset;    ///< The cpuset to use for the gain optimization thread.

    std::thread m_goptThread; ///< The gain optimization thread.

    bool m_goptThreadInit{ true }; ///< Initialization flag for the gain optimization thread.

    pid_t m_goptThreadID{ 0 }; ///< gain optimization thread PID.

    pcf::IndiProperty m_goptThreadProp; ///< The property to hold the gain optimization thread details.

    sem_t m_goptSemaphore; ///< Semaphore used to synchronize the psdShmim thread and the gopt thread.

    /// Gain Optimization thread starter function
    static void goptThreadStart( psdGainOpt *p /**< [in] pointer to this */ );

    /// Gain optimization thread function
    /** Runs until m_shutdown is true.
     */
    void goptThreadExec();

    ///@}

  public:
    /** \name INDI
     * @{
     */

    pcf::IndiProperty m_indiP_autoUpdate;
    pcf::IndiProperty m_indiP_fps;
    pcf::IndiProperty m_indiP_psdTime;
    pcf::IndiProperty m_indiP_psdAvgTime;
    pcf::IndiProperty m_indiP_loop;
    pcf::IndiProperty m_indiP_gain;
    pcf::IndiProperty m_indiP_mc;

    INDI_NEWCALLBACK_DECL( psdGainOpt, m_indiP_autoUpdate );

    INDI_SETCALLBACK_DECL( psdGainOpt, m_indiP_fps );
    INDI_SETCALLBACK_DECL( psdGainOpt, m_indiP_psdTime );
    INDI_SETCALLBACK_DECL( psdGainOpt, m_indiP_psdAvgTime );
    INDI_SETCALLBACK_DECL( psdGainOpt, m_indiP_loop );
    INDI_SETCALLBACK_DECL( psdGainOpt, m_indiP_gain );
    INDI_SETCALLBACK_DECL( psdGainOpt, m_indiP_mc );

    ///@}
};

psdGainOpt::psdGainOpt() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    psdShmimMonitorT::m_getExistingFirst     = true;
    freqShmimMonitorT::m_getExistingFirst    = true;
    gainShmimMonitorT::m_getExistingFirst    = true;
    multcoShmimMonitorT::m_getExistingFirst  = true;
    gainCalShmimMonitorT::m_getExistingFirst = true;
    tauShmimMonitorT::m_getExistingFirst     = true;

    return;
}

void psdGainOpt::setupConfig()
{
    config.add( "loop.number",
                "",
                "loop.number",
                argType::Required,
                "loop",
                "number",
                false,
                "int",
                "The number of the loop. Used to set shmim names, as in aolN_mgainfact." );

    config.add( "loop.name",
                "",
                "loop.name",
                argType::Required,
                "loop",
                "name",
                false,
                "string",
                "The name of the loop control INDI device name." );
    config.add( "loop.psdDev",
                "",
                "loop.psdDev",
                argType::Required,
                "loop",
                "psdDev",
                false,
                "string",
                "The INDI device name of the PSD calculator.  Defaults to aolN_modevalPSDs where N is loop.number." );

    config.add( "loop.autoUpdate",
                "",
                "loop.autoUpdate",
                argType::Required,
                "loop",
                "autoUpdate",
                false,
                "bool",
                "Flag controlling whether the gains are auto updated.  Also settable via INDI." );

    SHMIMMONITORT_SETUP_CONFIG( psdShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( freqShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( gainShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( multcoShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( gainCalShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( tauShmimMonitorT, config );
}

int psdGainOpt::loadConfigImpl( mx::app::appConfigurator &_config )
{
    _config( m_loopNum, "loop.number" );
    _config( m_loopName, "loop.name" );
    _config( m_autoUpdate, "loop.audoUpdate" );

    char shmim[1024];

    snprintf( shmim, sizeof( shmim ), "aol%d_modevalPSDs", m_loopNum );
    m_psdDevice = shmim;
    _config( m_psdDevice, "loop.psdDev" );

    psdShmimMonitorT::m_shmimName = m_psdDevice + "_psds";
    SHMIMMONITORT_LOAD_CONFIG( psdShmimMonitorT, _config );

    freqShmimMonitorT::m_shmimName = m_psdDevice + "_freq";
    SHMIMMONITORT_LOAD_CONFIG( freqShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_mgainfact", m_loopNum );
    gainShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( gainShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_mmultfact", m_loopNum );
    multcoShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( multcoShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_modalgaincal", m_loopNum );
    gainCalShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( gainCalShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_looptau", m_loopNum );
    tauShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( tauShmimMonitorT, _config );

    return 0;
}

void psdGainOpt::loadConfig()
{
    loadConfigImpl( config );
}

int psdGainOpt::appStartup()
{
    SHMIMMONITORT_APP_STARTUP( psdShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( freqShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( gainShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( multcoShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( gainCalShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( tauShmimMonitorT );

    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_autoUpdate, "auto_update" );

    REG_INDI_SETPROP( m_indiP_psdTime, m_psdDevice, "psdTime" );
    REG_INDI_SETPROP( m_indiP_psdAvgTime, m_psdDevice, "psdAvgTime" );
    REG_INDI_SETPROP( m_indiP_loop, m_loopName, "loop_state" );
    REG_INDI_SETPROP( m_indiP_gain, m_loopName, "loop_gain" );
    REG_INDI_SETPROP( m_indiP_mc, m_loopName, "loop_multcoeff" );

    if( sem_init( &m_goptSemaphore, 0, 0 ) < 0 )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, errno, 0, "Initializing gopt semaphore" } );
    }

    XWCAPP_THREAD_START( m_goptThread,
                         m_goptThreadInit,
                         m_goptThreadID,
                         m_goptThreadProp,
                         m_goptThreadPrio,
                         m_goptThreadCpuset,
                         "gainopt",
                         goptThreadStart );

    state( stateCodes::OPERATING );
    return 0;
}

int psdGainOpt::appLogic()
{
    SHMIMMONITORT_APP_LOGIC( psdShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( freqShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( gainShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( multcoShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( gainCalShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( tauShmimMonitorT );

    XWCAPP_THREAD_CHECK( m_goptThread, "gainopt" );

    SHMIMMONITORT_UPDATE_INDI( psdShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( freqShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( gainShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( multcoShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( gainCalShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( tauShmimMonitorT );

    if( m_autoUpdate )
    {
        updateSwitchIfChanged( m_indiP_autoUpdate, "toggle", pcf::IndiElement::On, INDI_OK );
    }
    else
    {
        updateSwitchIfChanged( m_indiP_autoUpdate, "toggle", pcf::IndiElement::Off, INDI_IDLE );
    }
    return 0;
}

int psdGainOpt::appShutdown()
{
    XWCAPP_THREAD_STOP( m_goptThread );

    SHMIMMONITORT_APP_SHUTDOWN( psdShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( freqShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( gainShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( multcoShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( gainCalShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( tauShmimMonitorT );

    return 0;
}

int psdGainOpt::allocate( const psdShmimT &dummy )
{
    static_cast<void>( dummy );

    m_updating = true;
    std::lock_guard<std::mutex> lock( m_goptMutex );
    m_updating = true;

    m_clPSDs.resize( psdShmimMonitorT::m_width, psdShmimMonitorT::m_height );
    m_olPSDs.resize( psdShmimMonitorT::m_height );
    m_nPSDs.resize( psdShmimMonitorT::m_height );

    for( size_t n = 0; n < m_olPSDs.size(); ++n )
    {
        m_olPSDs[n].resize( psdShmimMonitorT::m_width );
        m_nPSDs[n].resize( psdShmimMonitorT::m_width );
    }

    m_optGain.resize( psdShmimMonitorT::m_height );
    m_modeVar.resize( psdShmimMonitorT::m_height );

    m_sinceChange = -1;

    m_updating = false;
    return 0;
}

int psdGainOpt::processImage( void *curr_src, const psdShmimT &dummy )
{
    static_cast<void>( dummy );

    ++m_sinceChange;

    if( m_psdAvgTime <= 0 || m_psdTime <= 0 ) // Safety check, shouldn't happen but means we need to wait.
    {
        return 0;
    }

    int deadTime = ( m_psdAvgTime / m_psdTime ) / m_psdOverlapFraction;

    if( m_sinceChange < deadTime )
    {
        return 0;
    }

    // Here we would update psds, but don't do that if we're in the middle of calculating
    std::unique_lock<std::mutex> lock( m_goptMutex, std::try_to_lock );
    if( !lock.owns_lock() )
    {
        ///\todo update a frame-missed counter
        return 0;
    }

    m_updating = true;

    m_clPSDs = Eigen::Map<Eigen::Array<float, -1, -1>>(
        static_cast<float *>( curr_src ), psdShmimMonitorT::m_width, psdShmimMonitorT::m_height );

    m_updating = false;

    if( sem_post( &m_goptSemaphore ) < 0 )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, errno, 0, "Error posting to semaphore" } );
    }

    // Trigger calculation

    return 0;
}

int psdGainOpt::allocate( const freqShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int psdGainOpt::processImage( void *curr_src, const freqShmimT &dummy )
{
    static_cast<void>( dummy );

    if( freqShmimMonitorT::m_width != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got freq with width not 1" } );
    }

    bool change = false;

    float *f = static_cast<float *>( curr_src );

    size_t sz = freqShmimMonitorT::m_height;

    if( sz != m_freq.size() )
    {
        change = true;
    }

    if( !change ) // f is same size
    {
        for( size_t n = 0; n < sz; ++n )
        {
            if( f[n] != m_freq[n] )
            {
                change = true;
                break;
            }
        }
    }

    if( change )
    {
        m_updating = true;
        std::lock_guard<std::mutex> lock( m_goptMutex );
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        m_freq.resize( sz );

        for( size_t n = 0; n < sz; ++n )
        {
            m_freq[n] = f[n];
        }

        m_fps = 2 * m_freq.back();

        m_sinceChange = -1;
        m_goptUpdated = true;
        m_freqUpdated = true;

        m_updating = false;
        std::cerr << "got freq: " << sz << '\n';
        std::cerr << "     fps: " << m_fps << '\n';
    }

    return 0;
}

int psdGainOpt::allocate( const gainShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int psdGainOpt::processImage( void *curr_src, const gainShmimT &dummy )
{
    static_cast<void>( dummy );

    if( gainShmimMonitorT::m_height != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got gains with height not 1" } );
    }

    bool change = false;

    uint32_t w = gainShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_gains.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_gains.resize( w );
    }

    float *g = static_cast<float *>( curr_src );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_gains[n] != g[n] )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock
                change     = true;
            }

            m_gains[n] = g[n];
        }
    }

    if( change )
    {
        if( m_loop )
        {
            m_sinceChange = -1;
        }

        m_updating = false;
        lock.unlock();
        std::cerr << "got gains: " << m_gains.size() << "\n";
    }

    return 0;
}

int psdGainOpt::allocate( const multcoShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int psdGainOpt::processImage( void *curr_src, const multcoShmimT &dummy )
{
    static_cast<void>( dummy );

    if( multcoShmimMonitorT::m_height != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got multcoeffs with height not 1" } );
    }

    bool change = false;

    uint32_t w = multcoShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_mcs.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_mcs.resize( w );
    }

    float *m = static_cast<float *>( curr_src );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_mcs[n] != m[n] )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_mcs[n] = m[n];
        }
    }

    if( change )
    {
        if( m_loop )
        {
            m_sinceChange = -1;
        }

        m_updating    = false;
        m_goptUpdated = true;

        lock.unlock();
        std::cerr << "got mcs: " << m_mcs.size() << "\n";
    }

    return 0;
}

int psdGainOpt::allocate( const gainCalShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int psdGainOpt::processImage( void *curr_src, const gainCalShmimT &dummy )
{
    static_cast<void>( dummy );

    if( gainCalShmimMonitorT::m_height != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got gainCals with height not 1" } );
    }

    bool change = false;

    uint32_t w = gainCalShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_gainCals.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_gainCals.resize( w );
    }

    float *g = static_cast<float *>( curr_src );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_gainCals[n] != g[n] )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_gainCals[n] = g[n];
        }
    }

    if( change )
    {
        m_sinceChange = -1;
        m_updating    = false;
        lock.unlock();
        std::cerr << "got gainCals: " << m_gainCals.size() << "\n";
    }

    return 0;
}

int psdGainOpt::allocate( const tauShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int psdGainOpt::processImage( void *curr_src, const tauShmimT &dummy )
{
    static_cast<void>( dummy );

    if( tauShmimMonitorT::m_height != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got tau with height not 1" } );
    }

    bool change = false;

    uint32_t w = tauShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_taus.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_taus.resize( w );
    }

    float *t = static_cast<float *>( curr_src );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_taus[n] != t[n] )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_taus[n] = t[n];
        }
    }

    if( change )
    {
        m_sinceChange = -1;
        m_updating    = false;
        m_goptUpdated = true;
        lock.unlock();
        std::cerr << "got taus: " << m_taus.size() << "\n";
    }
    return 0;
}

void psdGainOpt::goptThreadStart( psdGainOpt *p )
{
    p->goptThreadExec();
}

void psdGainOpt::goptThreadExec()
{
    m_goptThreadID = syscall( SYS_gettid );

    while( m_goptThreadInit == true && shutdown() == 0 )
    {
        sleep( 1 );
    }

    while( shutdown() == 0 )
    {
        timespec ts;
        XWC_SEM_WAIT_TS_RETVOID( ts, 1, 0 );

        if( sem_timedwait( &m_goptSemaphore, &ts ) == 0 )
        {
            std::lock_guard<std::mutex> lock( m_goptMutex );

            if( (size_t)m_clPSDs.rows() == 0 || m_clPSDs.cols() == 0 ) // somehow here without any data
            {
                log<software_error>( { __FILE__, __LINE__, "PSDs have not been updated" } );
                continue;
            }

            if( (size_t)m_clPSDs.rows() != m_freq.size() )
            {
                log<software_error>( { __FILE__, __LINE__, "PSDs and freq size mismatch" } );
                continue;
            }

            if( (size_t)m_clPSDs.cols() != m_gains.size() )
            {
                log<software_error>( { __FILE__, __LINE__, "PSDs and gains number of modes mismatch" } );
                continue;
            }

            if( (size_t)m_clPSDs.cols() != m_mcs.size() )
            {
                log<software_error>( { __FILE__, __LINE__, "PSDs and mult coeffs number of modes mismatch" } );
                continue;
            }

            if( (size_t)m_clPSDs.cols() != m_gainCals.size() )
            {
                log<software_error>( { __FILE__, __LINE__, "PSDs and gain cals number of modes mismatch" } );
                continue;
            }

            if( (size_t)m_clPSDs.cols() != m_taus.size() )
            {
                log<software_error>( { __FILE__, __LINE__, "Loop taus have not been set" } );
                continue;
            }

            if( m_fps <= 0 )
            {
                log<software_error>( { __FILE__, __LINE__, "Loop fps has not been set" } );
                continue;
            }

            if( m_goptUpdated || m_freqUpdated || m_gopt.size() != m_gains.size() )
            {
                if( m_gopt.size() != m_gains.size() )
                {
                    m_freqUpdated = true; // force freq update in this case
                }

                std::cerr << "updating gopt structures\n";

                m_gopt.resize( m_gains.size() );

                for( size_t n = 0; n < m_gopt.size(); ++n )
                {
                    m_gopt[n].Ti( 1.0 / m_fps );
                    m_gopt[n].tau( m_taus[n] );
                    m_gopt[n].setLeakyIntegrator( m_mc * m_mcs[n] );

                    if( m_freqUpdated )
                    {
                        m_gopt[n].f( m_freq );
                    }
                }

                m_goptUpdated = false;
                m_freqUpdated = false;
            }

            if( m_updating )
            {
                continue;
            }

            float gain = m_gain;
            if( !m_loop )
            {
                gain = 0;
            }

            timePointT t0 = std::chrono::steady_clock::now();

#pragma omp parallel for
            for( size_t n = 0; n < m_gopt.size(); ++n )
            {
                if( m_updating )
                {
                    continue; // don't break b/c of omp
                }

                for( size_t f = 1; f < m_gopt[n].f_size(); ++f )
                {
                    m_olPSDs[n][f] = m_clPSDs( f, n ) / m_gopt[n].clETF2( f, gain * m_gains[n] * m_gainCals[n] );
                    m_nPSDs[n][f]  = 1e-20;
                }

                m_olPSDs[n][0] = m_olPSDs[n][1];
                m_nPSDs[n][0]  = m_nPSDs[n][1];

                m_optGain[n] = m_gopt[n].optGainOpenLoop( m_modeVar[n], m_olPSDs[n], m_nPSDs[n], false );
            }

            timePointT t1 = std::chrono::steady_clock::now();
            durationT  dt = t1 - t0;
            if( m_updating )
            {
                continue; // don't break b/c of omp
            }

            std::cerr << "Optimization took " << dt.count() << " seconds\n";
            size_t Np = m_optGain.size();
            if( Np > 10 )
                Np = 10;
            std::cerr << "Optimal gains:";
            for( size_t n = 0; n < Np; ++n )
            {
                std::cerr << ' ' << m_optGain[n];
            }
            std::cerr << '\n';
            std::cerr << "Calibrated Optimal gains:";
            for( size_t n = 0; n < Np; ++n )
            {
                std::cerr << ' ' << m_optGain[n] / m_gainCals[n];
            }
            std::cerr << '\n';

            if( m_autoUpdate )
            {
                float *f = (float *)gainShmimMonitorT::m_imageStream.array.raw;

                gainShmimMonitorT::m_imageStream.md->write = 1;

                for( size_t n = 0; n < m_optGain.size(); ++n )
                {
                    f[n] = m_optGain[n] / m_gainCals[n];
                }

                clock_gettime( CLOCK_ISIO, &gainShmimMonitorT::m_imageStream.md->writetime );
                gainShmimMonitorT::m_imageStream.md->atime = gainShmimMonitorT::m_imageStream.md->writetime;
                gainShmimMonitorT::m_imageStream.md->write = 0;
                ImageStreamIO_sempost( &( gainShmimMonitorT::m_imageStream ), -1 );

                std::cerr << "time to update!\n";
            }

            if( m_loop & m_autoUpdate )
            {
                m_sinceChange = -1;
            }
        }
        else
        {
            /* Check for why we timed out */
            /* ETIMEDOUT just means keep waiting */
            if( errno == ETIMEDOUT )
            {
                // Could Update gopts if needed (requires size checks and requires mutex lock)
                // Probably not worth it for pred. control anyway.
                continue;
            }

            /* EINTER probably indicates time to shutdown, loop wil exit if m_shutdown is set */
            if( errno == EINTR )
            {
                continue;
            }

            /*Otherwise, report an error.*/
            log<software_error>( { __FILE__, __LINE__, errno, "sem_timedwait" } );
            break;
        }
    }
}

INDI_NEWCALLBACK_DEFN( psdGainOpt, m_indiP_autoUpdate )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_autoUpdate, ipRecv );

    if( ipRecv.find( "toggle" ) )
    {
        if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On )
        {
            m_autoUpdate = true;
        }
        else
        {
            m_autoUpdate = false;
        }
    }

    return 0;
}

INDI_SETCALLBACK_DEFN( psdGainOpt, m_indiP_psdTime )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psdTime, ipRecv );

    if( ipRecv.find( "current" ) )
    {
        float psdTime = ipRecv["current"].get<float>();

        if( psdTime != m_psdTime )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_psdTime = psdTime;

            m_sinceChange = -1;
            m_updating    = false;

            std::cerr << "Got psdTime: " << m_psdTime << '\n';
        }
    }
    return 0;
}

INDI_SETCALLBACK_DEFN( psdGainOpt, m_indiP_psdAvgTime )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psdAvgTime, ipRecv );

    if( ipRecv.find( "current" ) )
    {
        float psdAvgTime = ipRecv["current"].get<float>();

        if( psdAvgTime != m_psdAvgTime )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_psdAvgTime = psdAvgTime;

            m_sinceChange = -1;
            m_updating    = false;

            std::cerr << "Got psdAvgTime: " << m_psdAvgTime << '\n';
        }
    }

    return 0;
}

INDI_SETCALLBACK_DEFN( psdGainOpt, m_indiP_loop )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_loop, ipRecv );

    if( ipRecv.find( "toggle" ) )
    {
        bool state;

        if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On )
        {
            state = true;
        }
        else
        {
            state = false;
        }

        if( state != m_loop )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_loop = state;

            m_sinceChange = -1;
            m_updating    = false;
            std::cerr << "Got loop: " << m_loop << '\n';
        }
    }

    return 0;
}

INDI_SETCALLBACK_DEFN( psdGainOpt, m_indiP_gain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_gain, ipRecv );

    if( ipRecv.find( "current" ) )
    {
        float gain = ipRecv["current"].get<float>();

        if( gain != m_gain )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_gain = gain;

            if( m_loop )
            {
                m_sinceChange = -1;
            }

            m_updating = false;

            std::cerr << "Got gain: " << m_gain << '\n';
        }
    }

    return 0;
}

INDI_SETCALLBACK_DEFN( psdGainOpt, m_indiP_mc )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_mc, ipRecv );

    if( ipRecv.find( "current" ) )
    {
        float mc = ipRecv["current"].get<float>();

        if( mc != m_mc )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_mc = mc;

            if( m_loop )
            {
                m_sinceChange = -1;
            }

            m_goptUpdated = true;
            m_updating    = false;
            std::cerr << "Got mc: " << m_mc << '\n';
        }
    }

    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // psdGainOpt_hpp
