/** \file psfFit.hpp
 * \brief The MagAO-X PSF Fitter application header
 *
 * \ingroup psfFit_files
 */

#ifndef psfFit_hpp
#define psfFit_hpp

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

/** \defgroup psfFit
 * \brief The MagAO-X PSF fitter.
 *
 * <a href="../handbook/operating/software/apps/psfFit.html">Application Documentation</a>
 *
 * \ingroup apps
 *
 */

/** \defgroup psfFit_files
 * \ingroup psfFit
 */

namespace MagAOX
{
namespace app
{

struct darkShmimT
{
    static std::string configSection()
    {
        return "darkShmim";
    };

    static std::string indiPrefix()
    {
        return "dark";
    };
};

struct refShmimT
{
    static std::string configSection()
    {
        return "refShmim";
    };

    static std::string indiPrefix()
    {
        return "ref";
    };
};

/// The MagAO-X PSF Fitter
/**
 * \ingroup psfFit
 */
class psfFit : public MagAOXApp<true>,
               public dev::shmimMonitor<psfFit>,
               public dev::shmimMonitor<psfFit, darkShmimT>,
               public dev::shmimMonitor<psfFit, refShmimT>,
               public dev::frameGrabber<psfFit>,
               public dev::telemeter<psfFit>
{
    // Give the test harness access.
    friend class psfFit_test;

    friend class dev::shmimMonitor<psfFit>;
    friend class dev::shmimMonitor<psfFit, darkShmimT>;
    friend class dev::shmimMonitor<psfFit, refShmimT>;
    friend class dev::frameGrabber<psfFit>;

    friend class dev::telemeter<psfFit>;

  public:
    /// The base shmimMonitor type
    typedef dev::shmimMonitor<psfFit> shmimMonitorT;

    /// The dark shmimMonitor type
    typedef dev::shmimMonitor<psfFit, darkShmimT> darkShmimMonitorT;

    /// The reference shmimMonitor type
    typedef dev::shmimMonitor<psfFit, refShmimT> refShmimMonitorT;

    // The base frameGrabber type
    typedef dev::frameGrabber<psfFit> frameGrabberT;

    // The base telemeter type
    typedef dev::telemeter<psfFit> telemeterT;

    /// Floating point type in which to do all calculations.
    typedef float realT;

    /** \name app::dev Configurations
     *@{
     */

    static constexpr bool c_frameGrabber_flippable =
        false; ///< app:dev config to tell framegrabber these images can not be flipped

    ///@}

  protected:
    /** \name Configurable Parameters
     *@{
     */

    std::string m_fpsSource; ///< Device name for getting fps if time-based averaging is used.  This device should have
                             ///< *.fps.current.

    uint16_t m_fitCircBuffMaxLength{ 5 * 10000 }; ///< Maximum length of the latency measurement circular buffers

    float m_fitCircBuffMaxTime{ 5 }; ///< Maximum time of the latency meaurement circular buffers

    float m_deltaPixThresh{ 2 }; /**< Threshold in pixels for skipping frame due to mismatch between max and c.o.l.
                                      Default 2.*/

    float m_sigmaMaxThreshUp{ 5 }; /**< Threshold in rms for skipping frame due to max positive difference from mean
                                      max. Default 5.*/

    float m_sigmaMaxThreshDown{ 5 }; /**< Threshold in rms for skipping frame due to max negative difference from mean
                                      mas. Default 5.*/

    float m_sigmaPixThresh{ 10 }; /**< Threshold in rms for skipping frame due to max difference from last value.
                                     Example: if this is set to 10, then the pixel postion has to change from -5 sigma
                                     to + 5 sigma to be rejected. Default 10.*/

    ///@}

    mx::improc::eigenImage<float> m_image; ///< Holds the raw image

    mx::improc::eigenImage<float> m_dark; ///< Holds the dark image

    mx::improc::eigenImage<float> m_ref; ///< Holds the reference image

    bool m_updated{ false }; ///< Indicates that the coordinates were updated was updated

    bool m_skipped{ false }; ///< Indicates that the image failed quality control and this is a skip frame

    float m_x{ 0 };      ///< The current x coordinate
    float m_last_x{ 0 }; ///< The previous x coordinate

    float m_y{ 0 };      ///< The current y coordinate
    float m_last_y{ 0 }; ///< The previous y coordinate

    float m_dx{ 0 }; ///< The offset in x to apply to non-skipped measurements
    float m_dy{ 0 }; ///< The offset in y to apply to non-skipped measurements

    float m_fps{ 0 }; ///< The frame rate from the source camera

    mx::sigproc::circularBufferIndex<float, cbIndexT> m_pcb; ///< Circular buffer for max pixel (p=peak)
    mx::sigproc::circularBufferIndex<float, cbIndexT> m_xcb; ///< Circular buffer for x COL coords
    mx::sigproc::circularBufferIndex<float, cbIndexT> m_ycb; ///< Circular buffer for y COL coords

    std::vector<float> m_pcbD; ///< Vector for doing calcs on max pixel
    std::vector<float> m_xcbD; ///< Vector for doing calcs on x COL coords
    std::vector<float> m_ycbD; ///< Vector for doing calcs on y COL coords

    float m_mnp{ 0 };  ///< The mean max pixel over the stats time
    float m_rmsp{ 0 }; ///< The rms max pixel over the stats time

    float m_mnx{ 0 };  ///< The mean x coord over the stats time
    float m_rmsx{ 0 }; ///< The rms x coord over the stats time
    float m_mny{ 0 };  ///< The mean y coord over the stats time
    float m_rmsy{ 0 }; ///< The rms y coord over the stats time

  public:
    /// Default c'tor.
    psfFit();

    /// D'tor, declared and defined for noexcept.
    ~psfFit() noexcept;

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

    /// Implementation of the FSM for psfFit.
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

    // shmimMonitor interface:
    int allocate( const dev::shmimT & );

    int processImage( void *curr_src, const dev::shmimT & );

    // shmimMonitor interface for dark:
    int allocate( const darkShmimT & );

    int processImage( void *curr_src, const darkShmimT & );

    // shmimMonitor interface for reference:
    int allocate( const refShmimT & );

    int processImage( void *curr_src, const refShmimT & );

  protected:
    std::mutex m_imageMutex;

    sem_t m_smSemaphore{ 0 }; ///< Semaphore used to synchronize the fg thread and the sm thread.

  public:
    /** \name dev::frameGrabber interface
     *
     * @{
     */

    /// Implementation of the framegrabber configureAcquisition interface
    /**
     * \returns 0 on success
     * \returns -1 on error
     */
    int configureAcquisition();

    /// Implementation of the framegrabber fps interface
    /**
     * \todo this needs to infer the stream fps and return it
     */
    float fps()
    {
        return m_fps;
    }

    /// Implementation of the framegrabber startAcquisition interface
    /**
     * \returns 0 on success
     * \returns -1 on error
     */
    int startAcquisition();

    /// Implementation of the framegrabber acquireAndCheckValid interface
    /**
     * \returns 0 on success
     * \returns -1 on error
     */
    int acquireAndCheckValid();

    /// Implementation of the framegrabber loadImageIntoStream interface
    /**
     * \returns 0 on success
     * \returns -1 on error
     */
    int loadImageIntoStream( void *dest /**< [in] */ );

    /// Implementation of the framegrabber reconfig interface
    /**
     * \returns 0 on success
     * \returns -1 on error
     */
    int reconfig();

    ///@}

  protected:
    /** \name INDI
     * @{
     */

    pcf::IndiProperty m_indiP_values;

    pcf::IndiProperty m_indiP_reset;
    INDI_NEWCALLBACK_DECL( psfFit, m_indiP_reset );

    pcf::IndiProperty m_indiP_statsTime;
    INDI_NEWCALLBACK_DECL( psfFit, m_indiP_statsTime );

    pcf::IndiProperty m_indiP_deltaPixThresh;
    INDI_NEWCALLBACK_DECL( psfFit, m_indiP_deltaPixThresh );

    pcf::IndiProperty m_indiP_sigmaMaxThreshUp;
    INDI_NEWCALLBACK_DECL( psfFit, m_indiP_sigmaMaxThreshUp );

    pcf::IndiProperty m_indiP_sigmaMaxThreshDown;
    INDI_NEWCALLBACK_DECL( psfFit, m_indiP_sigmaMaxThreshDown );

    pcf::IndiProperty m_indiP_sigmaPixThresh;
    INDI_NEWCALLBACK_DECL( psfFit, m_indiP_sigmaPixThresh );

    pcf::IndiProperty m_indiP_dx;
    INDI_NEWCALLBACK_DECL( psfFit, m_indiP_dx );

    pcf::IndiProperty m_indiP_dy;
    INDI_NEWCALLBACK_DECL( psfFit, m_indiP_dy );

    pcf::IndiProperty m_indiP_fpsSource;
    INDI_SETCALLBACK_DECL( psfFit, m_indiP_fpsSource );

    ///@}

    /** \name Telemeter Interface
     *
     * @{
     */
    int checkRecordTimes();

    int recordTelem( const telem_fgtimings * );

    ///@}
};

inline psfFit::psfFit() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    darkShmimMonitorT::m_getExistingFirst = true;
    refShmimMonitorT::m_getExistingFirst = true;

    return;
}

inline psfFit::~psfFit() noexcept
{
}

inline void psfFit::setupConfig()
{
    SHMIMMONITOR_SETUP_CONFIG( config );
    SHMIMMONITORT_SETUP_CONFIG( darkShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( refShmimMonitorT, config );
    FRAMEGRABBER_SETUP_CONFIG( config );
    TELEMETER_SETUP_CONFIG( config );

    config.add(
        "fitter.fpsSource",
        "",
        "fitter.fpsSource",
        argType::Required,
        "fitter",
        "fpsSource",
        false,
        "string",
        "Device name for getting fps if time-based averaging is used.  This device should have *.fps.current." );

    config.add( "fitter.deltaPixThresh",
                "",
                "fitter.deltaPixThresh",
                argType::Required,
                "fitter",
                "deltaPixThresh",
                false,
                "float",
                "Threshold in pixels for skipping frame due to mismatch between max and c.o.l.  Default 2." );
    config.add( "fitter.sigmaMaxThreshUp",
                "",
                "fitter.sigmaMaxThreshUp",
                argType::Required,
                "fitter",
                "sigmaMaxThreshUp",
                false,
                "float",
                "Threshold in rms for skipping frame due to max positive difference from mean max. Default 5." );
    config.add( "fitter.sigmaMaxThreshDown",
                "",
                "fitter.sigmaMaxThreshDown",
                argType::Required,
                "fitter",
                "sigmaMaxThreshDown",
                false,
                "float",
                "Threshold in rms for skipping frame due to max negative difference from mean max. Default 5." );

    config.add( "fitter.sigmaPixThresh",
                "",
                "fitter.sigmaPixThresh",
                argType::Required,
                "fitter",
                "sigmaPixThresh",
                false,
                "float",
                "Threshold in rms for skipping frame due to max difference from last value.  Example: if this is set "
                "to 10, then the pixel postion has to change from -5 sigma to + 5 sigma to be rejected. Default 10." );
}

inline int psfFit::loadConfigImpl( mx::app::appConfigurator &_config )
{
    SHMIMMONITOR_LOAD_CONFIG( _config );
    SHMIMMONITORT_LOAD_CONFIG( darkShmimMonitorT, _config );
    SHMIMMONITORT_LOAD_CONFIG( refShmimMonitorT, _config );

    FRAMEGRABBER_LOAD_CONFIG( _config );
    TELEMETER_LOAD_CONFIG( _config );

    _config( m_fpsSource, "fitter.fpsSource" );
    _config( m_deltaPixThresh, "fitter.deltaPixThresh" );
    _config( m_sigmaMaxThreshUp, "fitter.sigmaMaxThreshUp" );
    _config( m_sigmaMaxThreshDown, "fitter.sigmaMaxThreshDown" );
    _config( m_sigmaPixThresh, "fitter.sigmaPixThresh" );

    return 0;
}

inline void psfFit::loadConfig()
{
    loadConfigImpl( config );
}

inline int psfFit::appStartup()
{
    SHMIMMONITOR_APP_STARTUP;
    SHMIMMONITORT_APP_STARTUP( darkShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( refShmimMonitorT );

    if( sem_init( &m_smSemaphore, 0, 0 ) < 0 )
    {
        log<software_critical>( { __FILE__, __LINE__, errno, 0, "Initializing S.M. semaphore" } );
        return -1;
    }

    FRAMEGRABBER_APP_STARTUP;
    TELEMETER_APP_STARTUP;

    if( m_fpsSource != "" )
    {
        REG_INDI_SETPROP( m_indiP_fpsSource, m_fpsSource, std::string( "fps" ) );
    }

    CREATE_REG_INDI_RO_NUMBER( m_indiP_values, "values", "", "" );
    m_indiP_values.add( pcf::IndiElement( "max_mean" ) );
    m_indiP_values.add( pcf::IndiElement( "max_rms" ) );
    m_indiP_values.add( pcf::IndiElement( "x_mean" ) );
    m_indiP_values.add( pcf::IndiElement( "x_rms" ) );
    m_indiP_values.add( pcf::IndiElement( "y_mean" ) );
    m_indiP_values.add( pcf::IndiElement( "y_rms" ) );

    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_reset, "reset" );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_statsTime, "statsTime", 0, 5, 1, "%0.1f", "", "" );
    m_indiP_statsTime["current"].setValue( m_fitCircBuffMaxTime );
    m_indiP_statsTime["target"].setValue( m_fitCircBuffMaxTime );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_deltaPixThresh, "deltaPixThresh", 0, 512, 1, "%0.1f", "", "" );
    m_indiP_deltaPixThresh["current"].setValue( m_deltaPixThresh );
    m_indiP_deltaPixThresh["target"].setValue( m_deltaPixThresh );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_sigmaMaxThreshUp, "sigmaMaxThreshUp", 0, 512, 1, "%0.1f", "", "" );
    m_indiP_sigmaMaxThreshUp["current"].setValue( m_sigmaMaxThreshUp );
    m_indiP_sigmaMaxThreshUp["target"].setValue( m_sigmaMaxThreshUp );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_sigmaMaxThreshDown, "sigmaMaxThreshDown", 0, 512, 1, "%0.1f", "", "" );
    m_indiP_sigmaMaxThreshDown["current"].setValue( m_sigmaMaxThreshDown );
    m_indiP_sigmaMaxThreshDown["target"].setValue( m_sigmaMaxThreshDown );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_sigmaPixThresh, "sigmaPixThresh", 0, 512, 1, "%0.1f", "", "" );
    m_indiP_sigmaPixThresh["current"].setValue( m_sigmaPixThresh );
    m_indiP_sigmaPixThresh["target"].setValue( m_sigmaPixThresh );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_dx, "dx", -100, 100, 1e-2, "%0.02f", "", "" );
    m_indiP_dx["current"].setValue( m_dx );
    m_indiP_dx["target"].setValue( m_dx );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_dy, "dy", -100, 100, 1e-2, "%0.02f", "", "" );
    m_indiP_dy["current"].setValue( m_dy );
    m_indiP_dy["target"].setValue( m_dy );

    state( stateCodes::OPERATING );

    return 0;
}

inline int psfFit::appLogic()
{
    SHMIMMONITOR_APP_LOGIC;
    SHMIMMONITORT_APP_LOGIC( darkShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( refShmimMonitorT );
    FRAMEGRABBER_APP_LOGIC;
    TELEMETER_APP_LOGIC;

    if( state() == stateCodes::OPERATING && m_xcb.size() > 0 )
    {
        if( m_xcb.size() >= m_xcb.maxEntries() )
        {
            cbIndexT refEntry = m_xcb.earliest();

            m_pcbD.resize( m_pcb.maxEntries() - 1 );
            m_xcbD.resize( m_xcb.maxEntries() - 1 );
            m_ycbD.resize( m_xcb.maxEntries() - 1 );

            for( size_t n = 0; n < m_xcb.size(); ++n )
            {
                m_pcbD[n] = m_pcb.at( refEntry, n );
                m_xcbD[n] = m_xcb.at( refEntry, n );
                m_ycbD[n] = m_ycb.at( refEntry, n );
            }

            m_mnp = mx::math::vectorMean( m_pcbD );
            m_rmsp = sqrt( mx::math::vectorVariance( m_pcbD, m_mnp ) );

            m_mnx = mx::math::vectorMean( m_xcbD );

            m_rmsx = sqrt( mx::math::vectorVariance( m_xcbD, m_mnx ) );

            m_mny = mx::math::vectorMean( m_ycbD );
            m_rmsy = sqrt( mx::math::vectorVariance( m_ycbD, m_mny ) );
        }
        else
        {
            m_mnp = 0;
            m_rmsp = 0;

            m_mnx = 0;
            m_rmsx = 0;

            m_mny = 0;
            m_rmsy = 0;
        }
    }
    else
    {
        m_mnp = 0;
        m_rmsp = 0;

        m_mnx = 0;
        m_rmsx = 0;

        m_mny = 0;
        m_rmsy = 0;
    }

    SHMIMMONITOR_UPDATE_INDI;
    SHMIMMONITORT_UPDATE_INDI( darkShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( refShmimMonitorT );

    FRAMEGRABBER_UPDATE_INDI;

    updateIfChanged( m_indiP_statsTime, "current", m_fitCircBuffMaxTime );

    updateIfChanged( m_indiP_values,
                     std::vector<std::string>( { "max_mean", "max_rms", "x_mean", "x_rms", "y_mean", "y_rms" } ),
                     std::vector<float>( { m_mnp, m_rmsp, m_mnx, m_rmsx, m_mny, m_rmsy } ) );

    updateIfChanged( m_indiP_dx, "current", m_dx );
    updateIfChanged( m_indiP_dy, "current", m_dy );

    return 0;
}

inline int psfFit::appShutdown()
{
    SHMIMMONITOR_APP_SHUTDOWN;
    SHMIMMONITORT_APP_SHUTDOWN( darkShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( refShmimMonitorT );
    FRAMEGRABBER_APP_SHUTDOWN;
    TELEMETER_APP_SHUTDOWN;

    return 0;
}

inline int psfFit::allocate( const dev::shmimT &dummy )
{
    static_cast<void>( dummy );

    std::lock_guard<std::mutex> guard( m_imageMutex );

    m_image.resize( shmimMonitorT::m_width, shmimMonitorT::m_height );
    m_image.setZero();

    if( m_fitCircBuffMaxLength == 0 || m_fitCircBuffMaxTime == 0 || m_fps <= 0 )
    {
        m_pcb.maxEntries( 0 );
        m_xcb.maxEntries( 0 );
        m_ycb.maxEntries( 0 );

        m_mnp = 0;
        m_rmsp = 0;

        m_mnx = 0;
        m_rmsx = 0;

        m_mny = 0;
        m_rmsy = 0;
    }
    else
    {
        // Set up the fit circ. buffs
        cbIndexT cbSz = m_fitCircBuffMaxTime * m_fps;
        if( cbSz > m_fitCircBuffMaxLength )
            cbSz = m_fitCircBuffMaxLength;
        if( cbSz < 3 )
            cbSz = 3; // Make variance meaningful

        m_pcb.maxEntries( cbSz );
        m_xcb.maxEntries( cbSz );
        m_ycb.maxEntries( cbSz );

        m_mnp = 0;
        m_rmsp = 0;

        m_mnx = 0;
        m_rmsx = 0;

        m_mny = 0;
        m_rmsy = 0;
    }

    m_updated = false;
    return 0;
}

inline int psfFit::processImage( void *curr_src, const dev::shmimT &dummy )
{
    static_cast<void>( dummy );

    // counters for managing printing of delta-pix skips when shutter closed
    static int skip_interval = 10;
    static int last_passed = skip_interval + 1;

    std::unique_lock<std::mutex> lock( m_imageMutex );

    if( m_dark.rows() == m_image.rows() && m_dark.cols() == m_image.cols() )
    {
        if( shmimMonitorT::m_dataType == _DATATYPE_UINT16 )
        {
            for( unsigned nn = 0; nn < shmimMonitorT::m_width * shmimMonitorT::m_height; ++nn )
            {
                m_image.data()[nn] = ( (uint16_t *)curr_src )[nn] - m_dark.data()[nn];
            }
        }
        else if( shmimMonitorT::m_dataType == _DATATYPE_FLOAT )
        {
            for( unsigned nn = 0; nn < shmimMonitorT::m_width * shmimMonitorT::m_height; ++nn )
            {
                m_image.data()[nn] = ( (float *)curr_src )[nn] - m_dark.data()[nn];
            }
        }
    }
    else
    {
        if( shmimMonitorT::m_dataType == _DATATYPE_UINT16 )
        {
            for( unsigned nn = 0; nn < shmimMonitorT::m_width * shmimMonitorT::m_height; ++nn )
            {
                m_image.data()[nn] = ( (uint16_t *)curr_src )[nn];
            }
        }
        else if( shmimMonitorT::m_dataType == _DATATYPE_FLOAT )
        {
            for( unsigned nn = 0; nn < shmimMonitorT::m_width * shmimMonitorT::m_height; ++nn )
            {
                m_image.data()[nn] = ( (float *)curr_src )[nn];
            }
        }
    }

    lock.unlock();

    float max;
    int x = 0;
    int y = 0;

    max = m_image.maxCoeff( &x, &y );

    mx::improc::imageCenterOfLight( m_x, m_y, m_image );

    m_skipped = false;

    if( fabs( m_x - x ) > m_deltaPixThresh || fabs( m_y - y ) > m_deltaPixThresh )
    {
        if( last_passed > skip_interval ) // We only print if it's passed skip_interval times in between without failing
        {
            std::cerr << "skip frame (delta from max) " << m_x << " " << x << " " << m_y << " " << y << "("
                      << ( m_x - x ) << " " << ( m_y - y ) << ")\n";
        }
        last_passed = 0; // if we fail this threshold, we set last_pass to 0 to start over.
        m_skipped = true;

        // We do not add these measurements to stats b/c this means bad PSF
    }
    else if( last_passed < skip_interval + 1 ) // increment but don't overflow
    {
        ++last_passed;
    }

    // still filling circular buffer
    if( ( m_rmsx == 0 || m_rmsy == 0 || m_rmsp == 0 ) && !m_skipped )
    {
        if( m_xcb.maxEntries() > 0 )
        {
            m_pcb.nextEntry( max );
            m_xcb.nextEntry( m_x );
            m_ycb.nextEntry( m_y );
        }

        m_last_x = m_x;
        m_last_y = m_y;

        m_skipped = true;
    }

    // The remaining checks are for wild motions but otherwise valid fits (good PSF)

    if( ( ( max - m_mnp ) / m_rmsp > m_sigmaMaxThreshUp ) && !m_skipped )
    {
        if( m_pcb.maxEntries() > 0 )
        {
            m_pcb.nextEntry( max );
            m_xcb.nextEntry( m_x );
            m_ycb.nextEntry( m_y );
        }

        std::cerr << "skip frame (max-rms up) " << max << " " << m_mnp << " " << m_rmsp << " ("
                  << ( max - m_mnp ) / m_rmsp << ")\n";

        m_last_x = m_x;
        m_last_y = m_y;

        m_skipped = true;
    }

    if( ( ( max - m_mnp ) / m_rmsp < -m_sigmaMaxThreshDown ) && !m_skipped )
    {
        if( m_pcb.maxEntries() > 0 )
        {
            m_pcb.nextEntry( max );
            m_xcb.nextEntry( m_x );
            m_ycb.nextEntry( m_y );
        }

        std::cerr << "skip frame (max-rms down) " << max << " " << m_mnp << " " << m_rmsp << " ("
                  << ( max - m_mnp ) / m_rmsp << ")\n";

        m_last_x = m_x;
        m_last_y = m_y;

        m_skipped = true;
    }

    if( ( fabs( m_x - m_last_x ) / m_rmsx > m_sigmaPixThresh ) && !m_skipped )
    {
        if( m_xcb.maxEntries() > 0 )
        {
            m_pcb.nextEntry( max );
            m_xcb.nextEntry( m_x );
            m_ycb.nextEntry( m_y );
        }

        std::cerr << "skip frame (x-rms) " << m_x << " " << m_last_x << " " << m_rmsx << " ("
                  << ( m_x - m_last_x ) / m_rmsx << ")\n";

        m_last_x = m_x;
        m_last_y = m_y;

        m_skipped = true;
    }

    if( ( fabs( m_y - m_last_y ) / m_rmsy > m_sigmaPixThresh ) && !m_skipped )
    {
        if( m_ycb.maxEntries() > 0 )
        {
            m_pcb.nextEntry( max );
            m_xcb.nextEntry( m_x );
            m_ycb.nextEntry( m_y );
        }

        std::cerr << "skip frame (y-rms) " << m_y << " " << m_last_y << " " << m_rmsy << " ("
                  << ( m_y - m_last_y ) / m_rmsy << ")\n";

        m_last_x = m_x;
        m_last_y = m_y;

        m_skipped = true;
    }

    if( m_skipped )
    {
        if( m_ref.rows() == 2 && m_ref.cols() == 1 )
        {
            m_x = m_ref( 0, 0 );
            m_y = m_ref( 1, 0 );
        }
        else
        {
            m_x = 0;
            m_y = 0;
        }
    }

    m_updated = true;

    // signal framegrabber
    // Now tell the f.g. to get going
    if( sem_post( &m_smSemaphore ) < 0 )
    {
        log<software_critical>( { __FILE__, __LINE__, errno, 0, "Error posting to semaphore" } );
        return -1;
    }

    if( !m_skipped )
    {
        if( m_xcb.maxEntries() > 0 )
        {
            m_pcb.nextEntry( max );
            m_xcb.nextEntry( m_x );
            m_ycb.nextEntry( m_y );
        }

        m_last_x = m_x;
        m_last_y = m_y;
    }

    return 0;
}

int psfFit::allocate( const darkShmimT &dummy )
{
    static_cast<void>( dummy );

    std::lock_guard<std::mutex> guard( m_imageMutex );

    if( darkShmimMonitorT::m_dataType != IMAGESTRUCT_FLOAT )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "dark is not float" } );
    }

    m_dark.resize( darkShmimMonitorT::m_width, darkShmimMonitorT::m_height );
    m_dark.setZero();

    return 0;
}

int psfFit::processImage( void *curr_src, const darkShmimT &dummy )
{
    static_cast<void>( dummy );

    std::unique_lock<std::mutex> lock( m_imageMutex );

    for( unsigned nn = 0; nn < darkShmimMonitorT::m_width * darkShmimMonitorT::m_height; ++nn )
    {
        m_dark.data()[nn] = ( (float *)curr_src )[nn];
    }

    lock.unlock();

    log<text_log>( "dark updated", logPrio::LOG_INFO );

    return 0;
}

int psfFit::allocate( const refShmimT &dummy )
{
    static_cast<void>( dummy );

    std::lock_guard<std::mutex> guard( m_imageMutex );

    if( refShmimMonitorT::m_dataType != IMAGESTRUCT_FLOAT )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "ref is not float" } );
    }

    m_ref.resize( refShmimMonitorT::m_width, refShmimMonitorT::m_height );
    m_ref.setZero();

    return 0;
}

int psfFit::processImage( void *curr_src, const refShmimT &dummy )
{
    static_cast<void>( dummy );

    std::unique_lock<std::mutex> lock( m_imageMutex );

    for( unsigned nn = 0; nn < refShmimMonitorT::m_width * refShmimMonitorT::m_height; ++nn )
    {
        m_ref.data()[nn] = ( (float *)curr_src )[nn];
    }

    lock.unlock();

    log<text_log>( "reference updated", logPrio::LOG_INFO );

    return 0;
}

inline int psfFit::configureAcquisition()
{

    frameGrabberT::m_width = 2;
    frameGrabberT::m_height = 1;
    frameGrabberT::m_dataType = _DATATYPE_FLOAT;

    return 0;
}

inline int psfFit::startAcquisition()
{
    return 0;
}

inline int psfFit::acquireAndCheckValid()
{
    timespec ts;

    if( clock_gettime( CLOCK_ISIO, &ts ) < 0 )
    {
        log<software_critical>( { __FILE__, __LINE__, errno, 0, "clock_gettime" } );
        return -1;
    }

    ts.tv_sec += 1;

    if( sem_timedwait( &m_smSemaphore, &ts ) == 0 )
    {
        if( m_updated )
        {
            clock_gettime( CLOCK_ISIO, &m_currImageTimestamp );
            return 0;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return 1;
    }
}

inline int psfFit::loadImageIntoStream( void *dest )
{
    if( !m_skipped )
    {
        ( (float *)dest )[0] = m_x - m_dx;
        ( (float *)dest )[1] = m_y - m_dy;
    }
    else
    {
        ( (float *)dest )[0] = m_x;
        ( (float *)dest )[1] = m_y;
    }

    m_updated = false;

    return 0;
}

inline int psfFit::reconfig()
{
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfFit, m_indiP_reset )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_reset, ipRecv );

    if( ipRecv.find( "request" ) != true ) // this isn't valid
    {
        return -1;
    }

    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        log<text_log>( "reset requested", logPrio::LOG_NOTICE );
        shmimMonitorT::m_restart = true;
    }

    return 0;
}

INDI_NEWCALLBACK_DEFN( psfFit, m_indiP_statsTime )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_statsTime, ipRecv );

    float target;

    if( indiTargetUpdate( m_indiP_statsTime, target, ipRecv, true ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    m_fitCircBuffMaxTime = target;

    shmimMonitorT::m_restart = true;

    log<text_log>( "set statsTime = " + std::to_string( m_fitCircBuffMaxTime ), logPrio::LOG_NOTICE );

    return 0;
}

INDI_NEWCALLBACK_DEFN( psfFit, m_indiP_deltaPixThresh )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_deltaPixThresh, ipRecv );

    float target;

    if( indiTargetUpdate( m_indiP_deltaPixThresh, target, ipRecv, true ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    m_deltaPixThresh = target;

    log<text_log>( "set deltaPixThresh = " + std::to_string( m_deltaPixThresh ), logPrio::LOG_NOTICE );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfFit, m_indiP_sigmaMaxThreshUp )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_sigmaMaxThreshUp, ipRecv );

    float target;

    if( indiTargetUpdate( m_indiP_sigmaMaxThreshUp, target, ipRecv, true ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    m_sigmaMaxThreshUp = target;

    log<text_log>( "set sigmaMaxThreshUp = " + std::to_string( m_sigmaMaxThreshUp ), logPrio::LOG_NOTICE );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfFit, m_indiP_sigmaMaxThreshDown )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_sigmaMaxThreshDown, ipRecv );

    float target;

    if( indiTargetUpdate( m_indiP_sigmaMaxThreshDown, target, ipRecv, true ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    m_sigmaMaxThreshDown = target;

    log<text_log>( "set sigmaMaxThreshDown = " + std::to_string( m_sigmaMaxThreshDown ), logPrio::LOG_NOTICE );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfFit, m_indiP_sigmaPixThresh )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_sigmaPixThresh, ipRecv );

    float target;

    if( indiTargetUpdate( m_indiP_sigmaPixThresh, target, ipRecv, true ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    m_sigmaPixThresh = target;

    log<text_log>( "set sigmaMaxThreshDown = " + std::to_string( m_sigmaPixThresh ), logPrio::LOG_NOTICE );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfFit, m_indiP_dx )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_dx, ipRecv );

    float target;

    if( indiTargetUpdate( m_indiP_dx, target, ipRecv, true ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    m_dx = target;

    log<text_log>( "set dx = " + std::to_string( m_dx ), logPrio::LOG_NOTICE );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfFit, m_indiP_dy )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_dy, ipRecv );

    float target;

    if( indiTargetUpdate( m_indiP_dy, target, ipRecv, true ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    m_dy = target;

    log<text_log>( "set dy = " + std::to_string( m_dy ), logPrio::LOG_NOTICE );
    return 0;
}

INDI_SETCALLBACK_DEFN( psfFit, m_indiP_fpsSource )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fpsSource, ipRecv );

    if( ipRecv.find( "current" ) != true ) // this isn't valid
    {
        return 0;
    }

    std::lock_guard<std::mutex> guard( m_indiMutex );

    realT fps = ipRecv["current"].get<float>();

    if( fps != m_fps )
    {

        m_fps = fps;

        std::cerr << "got fps: " << m_fps << "\n";

        shmimMonitorT::m_restart = true;
    }

    return 0;
}

inline int psfFit::checkRecordTimes()
{
    return telemeterT::checkRecordTimes( telem_fgtimings() );
}

inline int psfFit::recordTelem( const telem_fgtimings * )
{
    return recordFGTimings( true );
}

} // namespace app
} // namespace MagAOX

#endif // psfFit_hpp
