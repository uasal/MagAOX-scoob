/** \file dmRecon.hpp
 * \brief The MagAO-X DM shape reconstructor
 *
 * \ingroup dmRecon_files
 */

#ifndef dmRecon_hpp
#define dmRecon_hpp

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

/** \defgroup dmRecon DM Shape Reconstructor
 * \brief Reconstruct the wavefront corresponding to a DM shape
 *
 * <a href="../handbook/operating/software/apps/dmRecon.html">Application Documentation</a>
 *
 * \ingroup apps
 *
 */

/** \defgroup dmRecon_files DM Shape Reconstructor Files
 * \ingroup dmRecon
 */

struct dmModesShmimT
{
    static std::string configSection()
    {
        return "dmModes";
    };

    static std::string indiPrefix()
    {
        return "dmModes";
    };
};

struct dmMaskShmimT
{
    static std::string configSection()
    {
        return "dmMask";
    };

    static std::string indiPrefix()
    {
        return "dmMask";
    };
};

struct dmCommandShmimT
{
    static std::string configSection()
    {
        return "dmCommand";
    };

    static std::string indiPrefix()
    {
        return "dmCommand";
    };
};

/** MagAO-X application to perform wavefront reconstruction from a DM surface
 *
 * \ingroup dmRecon
 *
 */
class dmRecon : public MagAOXApp<true>,
                public dev::shmimMonitor<dmRecon, dmModesShmimT>,
                public dev::shmimMonitor<dmRecon, dmMaskShmimT>,
                public dev::shmimMonitor<dmRecon, dmCommandShmimT>
//, public dev::telemeter<dmRecon>
{
    // Give the test harness access.
    friend class dmRecon_test;

    friend class dev::shmimMonitor<dmRecon, dmModesShmimT>;
    typedef dev::shmimMonitor<dmRecon, dmModesShmimT> dmModesSMT;

    friend class dev::shmimMonitor<dmRecon, dmMaskShmimT>;
    typedef dev::shmimMonitor<dmRecon, dmMaskShmimT> dmMaskSMT;

    friend class dev::shmimMonitor<dmRecon, dmCommandShmimT>;
    typedef dev::shmimMonitor<dmRecon, dmCommandShmimT> dmCommandSMT;

    // friend class dev::telemeter<dmRecon>;

    // typedef dev::telemeter<dmRecon> telemeterT;

    /// Floating point type in which to do all calculations.
    typedef float realT;

  protected:
    /** \name Configurable Parameters
     *@{
     */

    std::string m_loopNumber{ "1" }; ///< The loop number.  Default is 1 as in aol1.

    std::string m_fpsSource{ "camwfs" }; /**< Device name for getting fps of the loop.
                                              This device must have *.fps.current.  Default is camwfs*/

    ///@}

    float m_fps{ 0 }; ///< Current FPS from the FPS source.

    mx::improc::eigenImage<float> m_maskedDMModes;

    bool m_dmModesReady{ false }; ///< Flag indicating that the DM modes are ready for processing

    mx::improc::eigenImage<float> m_mask;

    std::vector<size_t> m_maskIDX; ///< The index of masked pixels

    bool m_dmMaskReady{ false }; ///< Flag indicating that the DM mask is ready for processing

    bool m_commandReady{ false }; ///< Flag indicating that all sizes match and arrays are ready for processing

  public:
    /// Default c'tor.
    dmRecon();

    /// D'tor, declared and defined for noexcept.
    ~dmRecon() noexcept
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

    /// Implementation of the FSM for dmRecon.
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

    /// Allocate method for the dm modes shmimMonitor
    /**
     * \returns 0 on success
     * \returns -1 on an error
     */
    int allocate( const dmModesShmimT & /**< [in] tag to differentiate shmimMonitor parents.*/ );

    /// Process images for the dm modes shmimMonitor
    /**
     * \returns 0 on sucess
     * \returns -1 on an error
     */
    int processImage( void *curr_src,       ///< [in] pointer to start of current frame.
                      const dmModesShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    /// Allocate method for the dm mask shmimMonitor
    /**
     * \returns 0 on success
     * \returns -1 on an error
     */
    int allocate( const dmMaskShmimT & /**< [in] tag to differentiate shmimMonitor parents.*/ );

    /// Process images for the dm mask shmimMonitor
    /**
     * \returns 0 on sucess
     * \returns -1 on an error
     */
    int processImage( void *curr_src,      ///< [in] pointer to start of current frame.
                      const dmMaskShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    /// Allocate method for the dm command shmimMonitor
    /**
     * \returns 0 on success
     * \returns -1 on an error
     */
    int allocate( const dmCommandShmimT & /**< [in] tag to differentiate shmimMonitor parents.*/ );

    /// Process images for the dm command shmimMonitor
    /**
     * \returns 0 on sucess
     * \returns -1 on an error
     */
    int processImage( void *curr_src,         ///< [in] pointer to start of current frame.
                      const dmCommandShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int prepareModes();

  protected:
    /** \name INDI Interface
     *
     * @{
     */
    pcf::IndiProperty m_indiP_fpsSource;
    INDI_SETCALLBACK_DECL( dmRecon, m_indiP_fpsSource );

    pcf::IndiProperty m_indiP_fps;

    ///@}

    /** \name Telemeter Interface
     *
     * @{
     */
    /* int checkRecordTimes();

     int recordTelem( const telem_loopgain * );

     int recordLoopGain( bool force = false );

     int recordTelem( const telem_offloading * );

     int recordOffloading( bool force = false );*/

    ///@}
};

inline dmRecon::dmRecon() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    return;
}

inline void dmRecon::setupConfig()
{
    config.add( "recon.loopNumber",
                "",
                "recon.loopNumber",
                argType::Required,
                "recon",
                "loopNumber",
                false,
                "int",
                "The loop number.  Default is 1 as in aol1." );

    config.add( "recon.fpsSource",
                "",
                "recon.fpsSource",
                argType::Required,
                "recon",
                "fpsSource",
                false,
                "string",
                "Device name for getting fps of the loop.  This device should have *.fps.current.  Default is camwfs" );

    std::string loopName = "aol" + m_loopNumber;

    dmModesSMT::m_shmimName = loopName + "_CMmodesDM";
    SHMIMMONITORT_SETUP_CONFIG( dmModesSMT, config );

    dmMaskSMT::m_shmimName = loopName + "_dmmask";
    SHMIMMONITORT_SETUP_CONFIG( dmMaskSMT, config );

    dmCommandSMT::m_shmimName = "dm01disp_delta";
    SHMIMMONITORT_SETUP_CONFIG( dmCommandSMT, config );

    // TELEMETER_SETUP_CONFIG( config );
}

inline int dmRecon::loadConfigImpl( mx::app::appConfigurator &_config )
{

    _config( m_loopNumber, "recon.loopNumber" );
    _config( m_fpsSource, "recon.fpsSource" );

    SHMIMMONITORT_LOAD_CONFIG( dmModesSMT, _config );
    SHMIMMONITORT_LOAD_CONFIG( dmMaskSMT, _config );
    SHMIMMONITORT_LOAD_CONFIG( dmCommandSMT, _config );

    // TELEMETER_LOAD_CONFIG( _config );

    return 0;
}

inline void dmRecon::loadConfig()
{
    loadConfigImpl( config );
}

inline int dmRecon::appStartup()
{

    REG_INDI_SETPROP( m_indiP_fpsSource, m_fpsSource, std::string( "fps" ) );

    createROIndiNumber( m_indiP_fps, "fps" );
    m_indiP_fps.add( pcf::IndiElement( "current" ) );
    if( registerIndiPropertyReadOnly( m_indiP_fps ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    SHMIMMONITORT_APP_STARTUP( dmModesSMT );
    SHMIMMONITORT_APP_STARTUP( dmMaskSMT );
    SHMIMMONITORT_APP_STARTUP( dmCommandSMT );

    // TELEMETER_APP_STARTUP;

    state( stateCodes::OPERATING );

    return 0;
}

int dmRecon::appLogic()
{
    SHMIMMONITORT_APP_LOGIC( dmModesSMT );
    SHMIMMONITORT_APP_LOGIC( dmMaskSMT );
    SHMIMMONITORT_APP_LOGIC( dmCommandSMT );

    // TELEMETER_APP_LOGIC;

    std::unique_lock<std::mutex> lock( m_indiMutex );

    SHMIMMONITORT_UPDATE_INDI( dmModesSMT );
    SHMIMMONITORT_UPDATE_INDI( dmMaskSMT );
    SHMIMMONITORT_UPDATE_INDI( dmCommandSMT );

    return 0;
}

inline int dmRecon::appShutdown()
{
    SHMIMMONITORT_APP_SHUTDOWN( dmModesSMT );
    SHMIMMONITORT_APP_SHUTDOWN( dmMaskSMT );
    SHMIMMONITORT_APP_SHUTDOWN( dmCommandSMT );

    // TELEMETER_APP_SHUTDOWN;

    return 0;
}

int dmRecon::allocate( const dmModesShmimT & )
{
    m_dmModesReady = false;

    dmCommandSMT::m_restart = true;

    // Can't process modes until mask is ready.
    if( dmModesSMT::m_width != dmMaskSMT::m_width || dmModesSMT::m_height != dmMaskSMT::m_height || !m_dmMaskReady )
    {
        mx::sys::milliSleep( 1000 );
        dmModesSMT::m_restart = true;
        return 0; // This won't log an error, but setting m_restart will cause it to loop again until sizes match
    }

    // This will let us go on to processImage

    //Do a type check for float
    return 0;
}

int dmRecon::processImage( void *curr_src, const dmModesShmimT & )
{
    if( m_dmModesReady == true )
    {
        // This means an new image has come in.  We need to reset and restart everything.
        dmModesSMT::m_restart   = true;
        dmMaskSMT::m_restart    = true;
        dmCommandSMT::m_restart = true;
        return 0;
    }

    // Wait for m_commandReady to become false
    while( m_commandReady == true && !m_shutdown && dmModesSMT::m_restart == false )
    {
        mx::sys::milliSleep( 1000 );
    }

    mx::improc::eigenCube<float> dmModes(
        reinterpret_cast<float *>( curr_src ), dmModesSMT::m_width, dmModesSMT::m_height, dmModesSMT::m_depth );

    m_maskedDMModes.resize( dmModesSMT::m_depth, m_maskIDX.size() );

    // Load only the unmasked pixels
    for( int rr = 0; rr < m_maskedDMModes.size(); ++rr )
    {
        for( size_t n = 0; n < m_maskIDX.size(); ++n )
        {
            m_maskedDMModes.row( n ).data()[n] = dmModes.image( n ).data()[n];
        }
    }

    // here do upload to device

    m_dmModesReady = true;
    return 0;
}

int dmRecon::allocate( const dmMaskShmimT & )
{
    m_dmMaskReady = false;

    dmCommandSMT::m_restart = true;

    //Do a type check for float
    return 0;
}

int dmRecon::processImage( void *curr_src, const dmMaskShmimT & )
{
    if( m_dmMaskReady == true )
    {
        // This means an new image has come in.  We need to reset and restart everything.
        dmModesSMT::m_restart   = true;
        dmMaskSMT::m_restart    = true;
        dmCommandSMT::m_restart = true;
        return 0;
    }

    // Wait for m_commandReady to become false
    while( m_commandReady == true && !m_shutdown && dmMaskSMT::m_restart == false )
    {
        mx::sys::milliSleep( 1000 );
    }

    m_mask = mx::improc::eigenMap<float>(reinterpret_cast<float *>(curr_src), dmMaskSMT::m_width, dmMaskSMT::m_height);

    m_maskIDX.clear();

    size_t n = 0;

    for(int rr = 0; rr < m_mask.rows(); ++rr)
    {
        for(int cc = 0; cc < m_mask.cols(); ++cc)
        {
            if(m_mask(rr,cc) == 1)
            {
                m_maskIDX.push_back(n);
            }
        }
    }

    std::cerr << "Got mask of size " << m_mask.rows() << " x " << m_mask.cols() << " with " << m_maskIDX.size() << " good pixels.\n";

    // here upload to device

    m_dmMaskReady = true;
    return 0;
}

int dmRecon::allocate( const dmCommandShmimT & )
{
    // This is the only place that m_commandReady can be changed
    if( !m_dmModesReady || !m_dmMaskReady || dmCommandSMT::m_width != dmModesSMT::m_width ||
        dmCommandSMT::m_height != dmModesSMT::m_height || dmCommandSMT::m_width != dmMaskSMT::m_width ||
        dmCommandSMT::m_height != dmMaskSMT::m_height )
    {
        m_commandReady = false;
        mx::sys::milliSleep( 1000 );
        dmCommandSMT::m_restart = true;
        return 0; // This won't log an error, but setting m_restart will cause it to reconnect again until sizes match
    }

    // do any allocations

    m_commandReady = true;

    return 0;
}

int dmRecon::processImage( void *curr_src, const dmCommandShmimT & )
{
    if( !m_commandReady )
    {
        dmCommandSMT::m_restart = true;
        return 0;
    }

    // upload command (masked pixels)

    // carry out mult

    // download result vector

    // trigger framegrabber

    return 0;
}

int dmRecon::prepareModes()
{
    // here do any processing and normalization, then upload to GPU

    /*
    mx::improc::eigenCube<float> tmodes;

    mx::fits::fitsFile<float> ff;

    ff.read( tmodes, m_dmModeFile );

    ff.read( m_dmMask, m_dmMaskFile );

    ff.read( m_twRespM, m_twRespMPath );

    for( int p = 0; p < tmodes.planes(); ++p )
    {
        tmodes.image( p ) *= m_dmMask;
        float norm = ( tmodes.image( p ) ).square().sum();
        tmodes.image( p ) /= sqrt( norm );
    }

    m_tModesOrtho.resize( tmodes.rows(), tmodes.cols(), m_maxModes );

    for( int p = 0; p < m_tModesOrtho.planes(); ++p )
    {
        m_tModesOrtho.image( p ) = tmodes.image( p );
    }

    ff.write( "/tmp/tModesOrtho.fits", m_tModesOrtho );

    m_wModes.resize( 11, 11, m_tModesOrtho.planes() );
    mx::improc::eigenImage<realT> win, wout;

    win.resize( 11, 11 );
    wout.resize( 11, 11 );

    // Calculate the woofer modes corresponding to the dm modes
    for( int p = 0; p < m_tModesOrtho.planes(); ++p )
    {
        win = m_tModesOrtho.image( p );
        Eigen::Map<Eigen::Matrix<float, -1, -1>>( wout.data(), wout.rows() * wout.cols(), 1 ) =
            m_twRespM.matrix() * Eigen::Map<Eigen::Matrix<float, -1, -1>>( win.data(), win.rows() * win.cols(), 1 );
        m_wModes.image( p ) = wout;
    }

    ff.write( "/tmp/wModes.fits", m_wModes );
*/
    return 0;
}

INDI_SETCALLBACK_DEFN( dmRecon, m_indiP_fpsSource )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fpsSource, ipRecv );

    if( ipRecv.find( "current" ) != true ) // this isn't valid
    {
        return -1;
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
int dmRecon::checkRecordTimes()
{
    return telemeterT::checkRecordTimes( telem_loopgain(), telem_offloading() );
}

int dmRecon::recordTelem( const telem_loopgain * )
{
    return recordLoopGain( true );
}

int dmRecon::recordLoopGain( bool force )
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

int dmRecon::recordTelem( const telem_offloading * )
{
    return recordOffloading( true );
}

int dmRecon::recordOffloading( bool force )
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

#endif // dmRecon_hpp
