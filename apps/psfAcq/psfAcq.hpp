/** \file psfAcq.hpp
 * \brief The MagAO-X PSF Fitter application header
 *
 * \ingroup psfAcq_files
 */

#ifndef psfAcq_hpp
#define psfAcq_hpp

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include <mx/math/fit/fitGaussian.hpp>
#include <mx/improc/imageFilters.hpp>

/** \defgroup psfAcq
 * \brief The MagAO-X PSF fitter.
 *
 * <a href="../handbook/operating/software/apps/psfAcq.html">Application Documentation</a>
 *
 * \ingroup apps
 *
 */

/** \defgroup psfAcq_files
 * \ingroup psfAcq
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

struct Star
{
    float x, y; // Star coordinates
    float max;  // Star brightness
    float fwhm; // Star FWHM
};

/// The MagAO-X PSF Fitter
/**
 * \ingroup psfAcq
 */
class psfAcq : public MagAOXApp<true>,
               public dev::shmimMonitor<psfAcq>,
               public dev::shmimMonitor<psfAcq, darkShmimT>,
               public dev::frameGrabber<psfAcq>,
               public dev::telemeter<psfAcq>
{
    // Give the test harness access.
    friend class psfAcq_test;

    friend class dev::shmimMonitor<psfAcq>;
    friend class dev::shmimMonitor<psfAcq, darkShmimT>;
    friend class dev::frameGrabber<psfAcq>;

    friend class dev::telemeter<psfAcq>;

  public:
    // The base shmimMonitor type
    typedef dev::shmimMonitor<psfAcq> shmimMonitorT;

    typedef dev::shmimMonitor<psfAcq, darkShmimT> darkShmimMonitorT;

    // The base frameGrabber type
    typedef dev::frameGrabber<psfAcq> frameGrabberT;

    // The base telemeter type
    typedef dev::telemeter<psfAcq> telemeterT;

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

    uint16_t m_fitCircBuffMaxLength{ 3600 }; ///< Maximum length of the latency measurement circular buffers
    float m_fitCircBuffMaxTime{ 5 };         ///< Maximum time of the latency meaurement circular buffers

    float m_fwhmGuess{ 2 };
    ///@}

    mx::improc::eigenImage<float> m_image;
    mx::improc::eigenImage<float> m_sm;

    mx::improc::eigenImage<float> m_dark;

    bool m_updated{ false };
    float m_x{ 0 };
    float m_y{ 0 };
    int m_max_loops{ 5 };        // default to detecting a max of 5 stars
    int m_zero_area{ 8 };        // default to zeroing an 8x8 pixel area around stars it finds
    float m_threshold = { 7.0 }; // how many sigma away from the mean you want to classify a detection, default to
                                 // 7sigma
    float m_fwhm_threshold = { 4.0 }; // minumum fwhm to consider something a star

    int m_len_x_vec = 1;

    std::vector<float> m_first_x_vals = {};
    std::vector<float> m_first_y_vals = {};
    std::vector<Star> m_detectedStars; // vector to store all the stars and properties

    float m_dx{ 0 };
    float m_dy{ 0 };

    int m_old_num_stars{ 0 };
    int m_num_stars{ 0 };
    int m_acquire_star{}; // Testing for user to select star
    int m_x_center{};     // 'center' of image or hot spot
    int m_y_center{};

    float m_fps{ 0 };

    mx::sigproc::circularBufferIndex<float, cbIndexT> m_xcb;
    mx::sigproc::circularBufferIndex<float, cbIndexT> m_ycb;

    std::vector<float> m_xcbD;
    std::vector<float> m_ycbD;

    float m_mnx{ 0 };
    float m_rmsx{ 0 };
    float m_mny{ 0 };
    float m_rmsy{ 0 };

    // Working memory for poke fitting
    mx::math::fit::fitGaussian2Dsym<float> m_gfit;

  public:
    /// Default c'tor.
    psfAcq();

    /// D'tor, declared and defined for noexcept.
    ~psfAcq() noexcept;

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

    /// Implementation of the FSM for psfAcq.
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

    // shmimMonitor interface for referenc:
    int allocate( const darkShmimT & );

    int processImage( void *curr_src, const darkShmimT & );

  protected:
    std::mutex m_imageMutex;

    sem_t m_smSemaphore{ 0 }; ///< Semaphore used to synchronize the fg \thread and the sm thread.

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

    std::vector<pcf::IndiProperty> m_indiP_star; // INDI Property for stars

    // Testing for num stars prop
    pcf::IndiProperty m_indiP_num_stars;
    INDI_NEWCALLBACK_DECL( psfAcq, m_indiP_num_stars );

    pcf::IndiProperty m_indiP_fpsSource;
    INDI_SETCALLBACK_DECL( psfAcq, m_indiP_fpsSource );

    // Testing for user to select star
    pcf::IndiProperty m_indiP_acquire_star;
    INDI_NEWCALLBACK_DECL( psfAcq, m_indiP_acquire_star );

    ///@}

    /** \name Telemeter Interface
     *
     * @{
     */
    int checkRecordTimes();

    int recordTelem( const telem_fgtimings * );

    ///@}
};

inline psfAcq::psfAcq() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    darkShmimMonitorT::m_getExistingFirst = true;
    return;
}

inline psfAcq::~psfAcq() noexcept
{
}

inline void psfAcq::setupConfig()
{
    shmimMonitorT::setupConfig( config );
    darkShmimMonitorT::setupConfig( config );
    frameGrabberT::setupConfig( config );
    telemeterT::setupConfig( config );

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
    config.add( "fitter.max_loops",
                "",
                "fitter.max_loops",
                argType::Required,
                "fitter",
                "max_loops",
                false,
                "int",
                "Setting the number of stars to detect in processImage function." );
    config.add( "fitter.zero_area",
                "",
                "fitter.zero_area",
                argType::Required,
                "fitter",
                "zero_area",
                false,
                "int",
                "Setting the pixel area to zero out after detecting stars in processImage function." );
    config.add( "fitter.threshold",
                "",
                "fitter.threshold",
                argType::Required,
                "fitter",
                "threshold",
                false,
                "float",
                "setting how many sigma away from the mean you want to classify a detection." );
    config.add( "fitter.fwhm_threshold",
                "",
                "fitter.fwhm_threshold",
                argType::Required,
                "fitter",
                "fwhm_threshold",
                false,
                "float",
                "minumum fwhm to consider something a star." );

    config.add( "acquisition.acquire_star",
                "",
                "acquisition.acquire_star",
                argType::Required,
                "acquisition",
                "acquire_star",
                false,
                "int",
                "Star number for telescope to move to." ); // Testing for user to acquire star
    config.add( "acquisition.x_center",
                "",
                "acquisition.x_center",
                argType::Required,
                "acquisition",
                "x_center",
                false,
                "int",
                "X value for 'center' of image." );
    config.add( "acquisition.y_center",
                "",
                "acquisition.y_center",
                argType::Required,
                "acquisition",
                "y_center",
                false,
                "int",
                "Y value for 'center' of image." );
}

inline int psfAcq::loadConfigImpl( mx::app::appConfigurator &_config )
{
    shmimMonitorT::loadConfig( _config );
    darkShmimMonitorT::loadConfig( _config );

    frameGrabberT::loadConfig( _config );
    telemeterT::loadConfig( _config );

    _config( m_fpsSource, "fitter.fpsSource" );
    _config( m_max_loops, "fitter.max_loops" ); // Max number of stars to detect in processImage
    _config( m_zero_area, "fitter.zero_area" ); // pixel area to zero out in processImage when a star is detected
    _config( m_threshold, "fitter.threshold" ); // how many sigma away from the mean you want to classify a detection
    _config( m_fwhm_threshold,
             "fitter.fwhm_threshold" ); // how many sigma away from the mean you want to classify a detection

    _config( m_acquire_star, "acquisition.acquire_star" ); // star number to acquire
    _config( m_x_center, "acquisition.x_center" );         // star number to acquire
    _config( m_y_center, "acquisition.y_center" );         // star number to acquire

    return 0;
}

inline void psfAcq::loadConfig()
{
    loadConfigImpl( config );
}

inline int psfAcq::appStartup()
{
    if( shmimMonitorT::appStartup() < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( darkShmimMonitorT::appStartup() < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( sem_init( &m_smSemaphore, 0, 0 ) < 0 )
    {
        log<software_critical>( { __FILE__, __LINE__, errno, 0, "Initializing S.M. semaphore" } );
        return -1;
    }

    if( frameGrabberT::appStartup() < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( telemeterT::appStartup() < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( m_fpsSource != "" )
    {
        REG_INDI_SETPROP( m_indiP_fpsSource, m_fpsSource, std::string( "fps" ) );
    }

    // Testing for user to select star
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_acquire_star, "acquire_star", 0, 20, 1, "%d", "", "" );
    m_indiP_acquire_star["current"].setValue( m_acquire_star );
    m_indiP_acquire_star["target"].setValue( m_acquire_star );

    // Testing for num stars prop
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_num_stars, "num_stars", 0, 20, 1, "%d", "", "" );
    m_indiP_num_stars["current"].setValue( m_num_stars );
    m_indiP_num_stars["target"].setValue(m_num_stars);

    state( stateCodes::OPERATING );

    return 0;
}

inline int psfAcq::appLogic()
{
    if( shmimMonitorT::appLogic() < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( darkShmimMonitorT::appLogic() < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( frameGrabberT::appLogic() < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( telemeterT::appLogic() < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( state() == stateCodes::OPERATING && m_xcb.size() > 0 )
    {
        if( m_xcb.size() >= m_xcb.maxEntries() )
        {
            cbIndexT refEntry = m_xcb.earliest();

            m_xcbD.resize( m_xcb.maxEntries() - 1 );
            m_ycbD.resize( m_xcb.maxEntries() - 1 );

            for( size_t n = 0; n <= m_atimesD.size(); ++n )
            {
                m_xcbD[n] = m_xcb.at( refEntry, n );
                m_ycbD[n] = m_ycb.at( refEntry, n );
            }

            m_mnx = mx::math::vectorMean( m_xcbD );
            m_rmsx = sqrt( mx::math::vectorVariance( m_xcbD, m_mnx ) );

            m_mny = mx::math::vectorMean( m_ycbD );
            m_rmsy = sqrt( mx::math::vectorVariance( m_ycbD, m_mny ) );
        }
        else
        {
            m_mnx = 0;
            m_rmsx = 0;
            m_mny = 0;
            m_rmsy = 0;
        }
    }
    else
    {
        m_mnx = 0;
        m_rmsx = 0;
        m_mny = 0;
        m_rmsy = 0;
    }
    std::unique_lock<std::mutex> lock( m_imageMutex );

    shmimMonitorT::updateINDI();
    darkShmimMonitorT::updateINDI();

    if( frameGrabberT::updateINDI() < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
    }

    updateIfChanged( m_indiP_num_stars, "current", m_num_stars );
    std::cout << __LINE__ << std::endl;
    std::cout << "m_detected=" << m_detectedStars.size() << std::endl;
    for( size_t n = 0; n < m_detectedStars.size(); ++n )
    {
       std::cout << __LINE__ << std::endl;
       updateIfChanged( m_indiP_star[n], "x", m_detectedStars[n].x );
       updateIfChanged( m_indiP_star[n], "y", m_detectedStars[n].y );
       updateIfChanged( m_indiP_star[n], "peak", m_detectedStars[n].max );
       updateIfChanged( m_indiP_star[n], "fwhm", m_detectedStars[n].fwhm );
    }
    std::cout << __LINE__ << std::endl;
    return 0;
}

inline int psfAcq::appShutdown()
{
    shmimMonitorT::appShutdown();
    darkShmimMonitorT::appShutdown();
    frameGrabberT::appShutdown();
    telemeterT::appShutdown();

    return 0;
}

inline int psfAcq::allocate( const dev::shmimT &dummy )
{
    static_cast<void>( dummy );

    std::lock_guard<std::mutex> guard( m_imageMutex );

    m_image.resize( shmimMonitorT::m_width, shmimMonitorT::m_height );
    m_image.setZero();

    m_sm.resize( m_image.rows(), m_image.cols() );

    if( m_fitCircBuffMaxLength == 0 || m_fitCircBuffMaxTime == 0 || m_fps <= 0 )
    {
        m_xcb.maxEntries( 0 );
        m_ycb.maxEntries( 0 );
    }
    else
    {
        // Set up the fit circ. buffs
        cbIndexT cbSz = m_fitCircBuffMaxTime * m_fps;
        if( cbSz > m_fitCircBuffMaxLength )
            cbSz = m_fitCircBuffMaxLength;
        if( cbSz < 3 )
            cbSz = 3; // Make variance meaningful
        m_xcb.maxEntries( cbSz );
        m_ycb.maxEntries( cbSz );
    }

    m_updated = false;
    return 0;
}

// Function to calculate Euclidean distance between two stars
float calculateDistance( float x1, float y1, float x2, float y2 )
{
    return sqrt( ( x2 - x1 ) * ( x2 - x1 ) + ( y2 - y1 ) * ( y2 - y1 ) );
}

inline int psfAcq::processImage( void *curr_src, const dev::shmimT &dummy )
{
   std::cout << __LINE__ << std::endl;
    static_cast<void>( dummy );

    std::unique_lock<std::mutex> lock( m_imageMutex );

    if( m_dark.rows() == m_image.rows() && m_dark.cols() == m_image.cols() )
    {
        for( unsigned nn = 0; nn < shmimMonitorT::m_width * shmimMonitorT::m_height; ++nn )
        {
            m_image.data()[nn] = ( (uint16_t *)curr_src )[nn] - m_dark.data()[nn];
        }
    }
    else
    {
        for( unsigned nn = 0; nn < shmimMonitorT::m_width * shmimMonitorT::m_height; ++nn )
        {
            m_image.data()[nn] = ( (uint16_t *)curr_src )[nn];
        }
    }

    lock.unlock();
    std::cout << __LINE__ << std::endl;
    float max;
    // int max_loops=5;
    int x = 0;
    int y = 0;
    int N_loops = 0;
    // 1. find brightest star
    max = m_image.maxCoeff( &x, &y );
    // mx::improc::medianSmooth(m_sm, x, y, max, m_image, 3);

    // mx::improc::imageCenterOfLight(m_x, m_y, m_image);
    // std::cerr << __LINE__ << std::endl;

    /*if(fabs(m_x-x) > 2 || fabs(m_y-y) > 2)
    {
        std::cerr << "skip frame\n";
        return 0;
    }*/

    eigenImage<float> llcorn = m_image.block( 0, 0, 32, 32 ); // calc std dev of 32x32 block in lower left corner
    float mean = llcorn.mean();                               // Calculate the mean
    float variance = ( llcorn.array() - mean ).square().sum() / ( llcorn.size() ); // calculate variance
    float stddev = std::sqrt( variance );                                          // Calculate the standard deviation
    float z_score = ( max - mean ) / stddev;                                       // how many std dev away from mean
    float fwhm = mx::math::func::sigma2fwhm( m_fwhmGuess ); // getting intial fwhm before entering while loop
    //std::cout << "Beginning: " << "mean=" << mean << "  max=" << max << "  variance=" << variance << "  stddev=" << stddev << "  z-score=" << z_score << "  fwhm=" << fwhm << std::endl;
    std::size_t numStars = m_detectedStars.size();
    std::cout << __LINE__ << std::endl;
    if( numStars == 0 )
    { // TESTING This runs when the vector of stars is empty (usually the first time)
        while( ( z_score > m_threshold ) && ( fwhm > m_fwhm_threshold ) && ( N_loops < m_max_loops ) )
        { // m_max_loops, m_fwhm_threshold, and m_threshold are configurable variables
            m_gfit.set_itmax( 1000 );
            // m_zero_area is used to zero out the pixel array once a star is detected but can also be used to set up a
            // sub image around the max pixel
            if( x < m_zero_area )
            {
                x = m_zero_area;
            }
            if( x >= ( m_image.rows() - m_zero_area ) )
            {
                x = m_image.rows() - m_zero_area;
            }
            if( y < m_zero_area )
            {
                y = m_zero_area;
            }
            if( y >= ( m_image.cols() - m_zero_area ) )
            {
                y = m_image.cols() - m_zero_area;
            }
            eigenImage<float> subImage = m_image.block(
                x - m_zero_area,
                y - m_zero_area,
                m_zero_area * 2,
                m_zero_area * 2 ); // set m_image to subImage to speed up gaussian, x,y is position of max pixel
            m_gfit.setArray( subImage.data(), subImage.rows(), subImage.cols() );
            // m_gfit.setArray(m_image.data(), m_image.rows(), m_image.cols());
            m_gfit.setGuess( 0, max, m_zero_area, m_zero_area, mx::math::func::sigma2fwhm( m_fwhmGuess ) );
            m_gfit.fit();
            m_x = ( x - m_zero_area ) + m_gfit.x0();
            m_y = ( y - m_zero_area ) + m_gfit.y0();
            m_first_x_vals.push_back( m_x ); // adding first detected x value to vector
            m_first_y_vals.push_back( m_y ); // adding first detected y value to vector
            fwhm = mx::math::func::sigma2fwhm( m_gfit.fwhm() );
            max = m_gfit.G();
            int x_value = static_cast<int>(
                m_x ); // convert m_x to an int so we can 0 out a rectangular area around the detected star
            int y_value = static_cast<int>( m_y );
            Star newStar;
            newStar.x = m_x; // Adding attributes to the new star
            newStar.y = m_y;
            newStar.max = max;
            newStar.fwhm = fwhm;
            m_detectedStars.push_back( newStar );
            if( x_value < m_zero_area )
            {
                x_value = m_zero_area;
            }
            if( x_value >= ( m_image.rows() - m_zero_area ) )
            {
                x_value = m_image.rows() - m_zero_area;
            }
            if( y_value < m_zero_area )
            {
                y_value = m_zero_area;
            }
            if( y_value >= ( m_image.cols() - m_zero_area ) )
            {
                y_value = m_image.cols() - m_zero_area;
            }
            for( int i = x_value - m_zero_area; i < ( x_value + m_zero_area ); i++ )
            { // zeroing out area around the star centered at m_x and m_y(8x8 pixel area)
                for( int j = y_value - m_zero_area; j < ( y_value + m_zero_area ); j++ )
                {
                    m_image( i, j ) = 0; // m_zero_area is defaulted to 20 to zero out a pixel array around the star
                }
            }
            max = m_image.maxCoeff( &x, &y );
            N_loops = N_loops + 1;
            z_score = ( max - mean ) / stddev;
            //std::cout << "N_loops=" << N_loops << "  z-score=" << z_score << "  max=" << max << std::endl;
        }
        size_t starCount = m_detectedStars.size();
        std::unique_lock<std::mutex> lock( m_imageMutex );
        m_indiP_star.resize( starCount );
        // Create and register new properties for X and Y positions
        for( size_t n = 0; n < starCount; ++n )
        {
            std::cout << "First starcount=" << starCount << std::endl;
            std::string starPrefix = "star_" + std::to_string( n );
            
            createROIndiNumber(
                m_indiP_star[n], starPrefix, "Star " + std::to_string( n ) + " Properties", "Star Acq" );
            m_indiP_star[n].add( pcf::IndiElement( "x" ) );
            m_indiP_star[n]["x"].set( m_detectedStars[n].x );
            m_indiP_star[n].add( pcf::IndiElement( "y" ) );
            m_indiP_star[n]["y"].set( m_detectedStars[n].y );
            m_indiP_star[n].add( pcf::IndiElement( "peak" ) );
            m_indiP_star[n]["peak"].set( m_detectedStars[n].max );
            m_indiP_star[n].add( pcf::IndiElement( "fwhm" ) );
            m_indiP_star[n]["fwhm"].set( m_detectedStars[n].fwhm );
            registerIndiPropertyReadOnly( m_indiP_star[n] );
            if( m_indiDriver )
                m_indiDriver->sendSetProperty( m_indiP_star[n] );
        }
    }

    else
    {
        std::cout << __LINE__ << std::endl;
        // In here is where we track the stars using cross correlation between the first frame and subsequent frames
        while( ( z_score > m_threshold ) && ( fwhm > m_fwhm_threshold ) && ( N_loops < m_max_loops ) )
        { 

            std::cout << __LINE__ << std::endl;
            m_gfit.set_itmax( 1000 );
            // m_zero_area is used to zero out the pixel array once a star is detected but can also be used to set up a
            // sub image around the max pixel
            if( x < m_zero_area )
            {
                x = m_zero_area;
            }
            if( x >= ( m_image.rows() - m_zero_area ) )
            {
                x = m_image.rows() - m_zero_area;
            }
            if( y < m_zero_area )
            {
                y = m_zero_area;
            }
            if( y >= ( m_image.cols() - m_zero_area ) )
            {
                y = m_image.cols() - m_zero_area;
            }
            std::cout << __LINE__ << std::endl;
            eigenImage<float> subImage = m_image.block(
                x - m_zero_area,
                y - m_zero_area,
                m_zero_area * 2,
                m_zero_area * 2 ); // set m_image to subImage to speed up gaussian, x,y is position of max pixel
            // 2. fit it's position
            std::cout << __LINE__ << std::endl;
            m_gfit.setArray( subImage.data(), subImage.rows(), subImage.cols() );
            std::cout << __LINE__ << std::endl;
            m_gfit.setGuess( 0, max, m_zero_area, m_zero_area, mx::math::func::sigma2fwhm( m_fwhmGuess ) );
            std::cout << __LINE__ << std::endl;
            fwhm = mx::math::func::sigma2fwhm( m_gfit.fwhm() );
            std::cout << __LINE__ << std::endl;
            m_gfit.fit();
            std::cout << __LINE__ << std::endl;
            max = m_gfit.G();
            std::cout << "this is the max (m_gfit.G())=" << max << std::endl;
            m_x = ( x - m_zero_area ) + m_gfit.x0();
            m_y = ( y - m_zero_area ) + m_gfit.y0();
            int x_value = static_cast<int>(
                m_x ); // convert m_x to an int so we can 0 out a rectangular area around the detected star
            int y_value = static_cast<int>( m_y );

            if( x_value < m_zero_area )
            {
                x_value = m_zero_area;
            }
            if( x_value >= ( m_image.rows() - m_zero_area ) )
            {
                x_value = m_image.rows() - m_zero_area;
            }
            if( y_value < m_zero_area )
            {
                y_value = m_zero_area;
            }
            if( y_value >= ( m_image.cols() - m_zero_area ) )
            {
                y_value = m_image.cols() - m_zero_area;
            }
            std::cout << __LINE__ << std::endl;
            for( int i = x_value - m_zero_area; i < ( x_value + m_zero_area ); i++ )
            { // zeroing out area around the star centered at m_x and m_y(8x8 pixel area)
                for( int j = y_value - m_zero_area; j < ( y_value + m_zero_area ); j++ )
                {
                    m_image( i, j ) = 0; // m_zero_area is defaulted to 20 to zero out a pixel array around the star
                }
            }
 
            // 3. search through all known stars to figure out which one it corresponds to.  You can NOT assume it is in the order of the vector.
            // This simple for loop calculate the distance from the detected star to the cloest star already in the list
            // and updates the values
            int threshold_distance = 20; // distance between new stars should be a small positive number so this updates
            int n = 0;
            int tracker = 0; // tracks if the current star detected updated an already known star
            std::cout << __LINE__ << std::endl;
            for( Star &star : m_detectedStars )
            {
                float dist = calculateDistance( star.x, star.y, x_value, y_value );
                // 4. if it is found, update that star's data in the vector
                if( dist < threshold_distance ) 
                {
                    star.x = m_x;
                    star.y = m_y;
                    star.max = max;
                    star.fwhm = fwhm;
                    tracker = 1;
                    break;
                }
                n++;
            }
            std::cout << __LINE__ << std::endl;

            if (tracker == 0) { // 5. if it is not found, add a star and corresponding INDI Property using push_back
                if (m_detectedStars.size() > m_max_loops){
                  break;
                }
                std::cout << "New star found" << std::endl;
                Star newStar;
                newStar.x = m_x; // Adding attributes to the new star
                newStar.y = m_y;
                newStar.max = max;
                newStar.fwhm = fwhm;
                m_detectedStars.push_back( newStar );
                std::unique_lock<std::mutex> lock( m_imageMutex );
               
                m_indiP_star.push_back(pcf::IndiProperty());

                int index = m_detectedStars.size() - 1; 
                std::string starPrefix = "star_" + std::to_string( index );
                //std::cout << m_indiP_star.size() << std::endl;
                std::cout << __LINE__ << std::endl;
                createROIndiNumber(m_indiP_star[index], starPrefix, "Star " + std::to_string( index ) + " Properties", "Star Acq" ); 
                m_indiP_star[index].add( pcf::IndiElement( "x" ) );
                m_indiP_star[index]["x"].set( m_detectedStars[index].x );
                m_indiP_star[index].add( pcf::IndiElement( "y" ) );
                m_indiP_star[index]["y"].set( m_detectedStars[index].y );
                m_indiP_star[index].add( pcf::IndiElement( "peak" ) );
                m_indiP_star[index]["peak"].set( m_detectedStars[index].max );
                m_indiP_star[index].add( pcf::IndiElement( "fwhm" ) );
                m_indiP_star[index]["fwhm"].set( m_detectedStars[index].fwhm );
                std::cout << __LINE__ << std::endl;
                registerIndiPropertyReadOnly( m_indiP_star[index] );
                if( m_indiDriver )
                   m_indiDriver->sendSetProperty( m_indiP_star[index] );
                std::cout << __LINE__ << std::endl << "tests";
                   
            }
            std::cout << __LINE__ << std::endl;

            max = m_image.maxCoeff( &x, &y );
            std::cout << "This is the other max (m_image.maxCoeff( &x, &y )) = " << max << std::endl;
            N_loops = N_loops + 1;
            z_score = ( max - mean ) / stddev;
            //std::cout << "In the !!!: " << "mean=" << mean << "  max=" << max << "  variance=" << variance << "  stddev=" << stddev << "  z-score=" << z_score << "  fwhm=" << fwhm << std::endl;
        }
        std::cout << __LINE__ << std::endl;
    }

         
         
/*       
         // m_max_loops, m_fwhm_threshold, and m_threshold are configurable variables
            int star_count = m_detectedStars.size();

            if( star_count <= N_loops )  // PROBLEM WITH THIS IF STATEMENT WHEN ADDING NEW STARS 
            { // This if statement is used to add additional stars if they are detected later on
            std::cout << "Beginning: " << "z_score:" << z_score << " m_threshold:" << m_threshold << " fwhm:" << fwhm
            << " m_fwhm:" << m_fwhm_threshold << " N_loops:" << N_loops << " m_max_loops:" << m_max_loops << " max:"
            << max << std::endl;
                m_gfit.set_itmax( 1000 );
                // m_zero_area is used to zero out the pixel array once a star is detected but can also be used to set
                // up a sub image around the max pixel
                if( x < m_zero_area )
                {
                    x = m_zero_area;
                }
                if( x >= ( m_image.rows() - m_zero_area ) )
                {
                    x = m_image.rows() - m_zero_area;
                }
                if( y < m_zero_area )
                {
                    y = m_zero_area;
                }
                if( y >= ( m_image.cols() - m_zero_area ) )
                {
                    y = m_image.cols() - m_zero_area;
                }
                eigenImage<float> subImage = m_image.block(
                    x - m_zero_area,
                    y - m_zero_area,
                    m_zero_area * 2,
                    m_zero_area * 2 ); // set m_image to subImage to speed up gaussian, x,y is position of max pixel
                m_gfit.setArray( subImage.data(), subImage.rows(), subImage.cols() );
                // m_gfit.setArray(m_image.data(), m_image.rows(), m_image.cols());
                m_gfit.setGuess( 0, max, m_zero_area, m_zero_area, mx::math::func::sigma2fwhm( m_fwhmGuess ) );
                m_gfit.fit();
                // std::cout << "m_gfit.x0 = " << m_gfit.x0() << "     m_gfit.y0 = " << m_gfit.y0() << std::endl;
                m_x = ( x - m_zero_area ) + m_gfit.x0();
                m_y = ( y - m_zero_area ) + m_gfit.y0();
                // std::cout << "m_x = " << m_x << "     m_y = " << m_y << std::endl;

                m_first_x_vals.push_back( m_x ); // adding first detected x value to vector
                m_first_y_vals.push_back( m_y ); // adding first detected y value to vector
                float fwhm = mx::math::func::sigma2fwhm( m_fwhmGuess );

                Star newStar;
                newStar.x = m_x; // Adding attributes to the new star
                newStar.y = m_y;
                newStar.max = max;
                newStar.fwhm = fwhm;
                m_detectedStars.push_back( newStar );
                int x_value = static_cast<int>(
                    m_x ); // convert m_x to an int so we can 0 out a rectangular area around the detected star
                int y_value = static_cast<int>( m_y );

                if( x_value < m_zero_area )
                {
                    x_value = m_zero_area;
                }
                if( x_value >= ( m_image.rows() - m_zero_area ) )
                {
                    x_value = m_image.rows() - m_zero_area;
                }
                if( y_value < m_zero_area )
                {
                    y_value = m_zero_area;
                }
                if( y_value >= ( m_image.cols() - m_zero_area ) )
                {
                    y_value = m_image.cols() - m_zero_area;
                }
                for( int i = x_value - m_zero_area; i < ( x_value + m_zero_area ); i++ )
                { // zeroing out area around the star centered at m_x and m_y
                    for( int j = y_value - m_zero_area; j < ( y_value + m_zero_area ); j++ )
                    {
                        m_image( i, j ) = 0; // m_zero_area is defaulted to 36 to zero out a pixel array around the star
                    }
                }
                
                
                std::cout << "xvalue= " << x_value << "  yvalue=" << y_value << std::endl;
                m_detectedStars[star_count].x = x_value; // m_x
                m_detectedStars[star_count].y = y_value; // m_y
                m_detectedStars[star_count].max = max;
                m_detectedStars[star_count].fwhm = fwhm;
                
                for (int i=0; i <= m_detectedStars.size(); i++){ //troubleshooting
                   std::cout << i << " x is   : " << m_detectedStars[i].x << std::endl;
                   std::cout << i << " y is   : " << m_detectedStars[i].y << std::endl;
                   std::cout << i << " max is : " << m_detectedStars[i].max << std::endl;
                   std::cout << i << " fwhm is: " << m_detectedStars[i].fwhm << std::endl;
                }

                max = m_image.maxCoeff( &x, &y );
                N_loops = N_loops + 1;
                z_score = ( max - mean ) / stddev;

                m_indiP_star.resize( star_count );
                // Create and register new properties for X and Y positions
                std::cout << __LINE__ << std::endl;
                std::cout << "starcount=" << star_count << std::endl;
                std::string starPrefix = "star_" + std::to_string( star_count );
                std::cout << __LINE__ << std::endl;
                createROIndiNumber(
                   m_indiP_star[star_count], starPrefix, "Star " + std::to_string( star_count ) + " Properties", "Star Acq" );
                   std::cout << __LINE__ << std::endl;
                m_indiP_star[star_count].add( pcf::IndiElement( "x" ) );
                m_indiP_star[star_count]["x"].set( m_detectedStars[star_count].x );
                m_indiP_star[star_count].add( pcf::IndiElement( "y" ) );
                m_indiP_star[star_count]["y"].set( m_detectedStars[star_count].y );
                m_indiP_star[star_count].add( pcf::IndiElement( "peak" ) );
                m_indiP_star[star_count]["peak"].set( m_detectedStars[star_count].max );
                m_indiP_star[star_count].add( pcf::IndiElement( "fwhm" ) );
                m_indiP_star[star_count]["fwhm"].set( m_detectedStars[star_count].fwhm );
                std::cout << __LINE__ << std::endl;
                registerIndiPropertyReadOnly( m_indiP_star[star_count] );
                std::cout << __LINE__ << std::endl;
                if( m_indiDriver )
                   m_indiDriver->sendSetProperty( m_indiP_star[star_count] );
                std::cout << __LINE__ << std::endl;
/*               
                for( size_t n = 0; n < starCount; ++n )
                {
                    std::cout << "This is the _ in the loop " << n << std::endl;
                    std::string starPrefix = "star_" + std::to_string( n );

                    createROIndiNumber(
                        m_indiP_star[n], starPrefix, "Star " + std::to_string( n ) + " Properties", "Star Acq" );
                    m_indiP_star[n].add( pcf::IndiElement( "x" ) );
                    m_indiP_star[n]["x"].set( m_detectedStars[n].x );
                    m_indiP_star[n].add( pcf::IndiElement( "y" ) );
                    m_indiP_star[n]["y"].set( m_detectedStars[n].y );
                    m_indiP_star[n].add( pcf::IndiElement( "peak" ) );
                    m_indiP_star[n]["peak"].set( m_detectedStars[n].max );
                    m_indiP_star[n].add( pcf::IndiElement( "fwhm" ) );
                    m_indiP_star[n]["fwhm"].set( m_detectedStars[n].fwhm );
                    registerIndiPropertyReadOnly( m_indiP_star[n] );
                    if( m_indiDriver )
                        m_indiDriver->sendSetProperty( m_indiP_star[n] );
                }
                std::cout << __LINE__ << std::endl;

            }
      
            m_gfit.set_itmax( 1000 );
            // m_zero_area is used to zero out the pixel array once a star is detected but can also be used to set up a
            // sub image around the max pixel
            if( x < m_zero_area )
            {
                x = m_zero_area;
            }
            if( x >= ( m_image.rows() - m_zero_area ) )
            {
                x = m_image.rows() - m_zero_area;
            }
            if( y < m_zero_area )
            {
                y = m_zero_area;
            }
            if( y >= ( m_image.cols() - m_zero_area ) )
            {
                y = m_image.cols() - m_zero_area;
            }
            eigenImage<float> subImage = m_image.block(
                x - m_zero_area,
                y - m_zero_area,
                m_zero_area * 2,
                m_zero_area * 2 ); // set m_image to subImage to speed up gaussian, x,y is position of max pixel
            m_gfit.setArray( subImage.data(), subImage.rows(), subImage.cols() );
            m_gfit.setGuess( 0, max, m_zero_area, m_zero_area, mx::math::func::sigma2fwhm( m_fwhmGuess ) );
            float fwhm = mx::math::func::sigma2fwhm( m_fwhmGuess );
            m_gfit.fit();
            m_x = ( x - m_zero_area ) + m_gfit.x0();
            m_y = ( y - m_zero_area ) + m_gfit.y0();
            int x_value = static_cast<int>(
                m_x ); // convert m_x to an int so we can 0 out a rectangular area around the detected star
            int y_value = static_cast<int>( m_y );

            if( x_value < m_zero_area )
            {
                x_value = m_zero_area;
            }
            if( x_value >= ( m_image.rows() - m_zero_area ) )
            {
                x_value = m_image.rows() - m_zero_area;
            }
            if( y_value < m_zero_area )
            {
                y_value = m_zero_area;
            }
            if( y_value >= ( m_image.cols() - m_zero_area ) )
            {
                y_value = m_image.cols() - m_zero_area;
            }
            for( int i = x_value - m_zero_area; i < ( x_value + m_zero_area ); i++ )
            { // zeroing out area around the star centered at m_x and m_y(8x8 pixel area)
                for( int j = y_value - m_zero_area; j < ( y_value + m_zero_area ); j++ )
                {
                    m_image( i, j ) = 0; // m_zero_area is defaulted to 20 to zero out a pixel array around the star
                }
            }
 
            // This simple for loop calculate the distance from the detected star to the cloest star already in the list
            // and updates the values
            float closest_dist = 10000; // distance between new stars should be a small positive number so this updates
            int n = 0;
            int star_number = 0;
            for( const Star &star : m_detectedStars )
            {
                float dist = calculateDistance( star.x, star.y, x_value, y_value );
                if( dist < closest_dist )
                {
                    closest_dist = dist;
                    star_number = n;
                }
                n++;
            }
            // if (star_number >= 0){
            // star_number keeps track of what star in the vector was the closest and updates its values
            m_detectedStars[star_number].x = x_value; // m_x
            m_detectedStars[star_number].y = y_value; // m_y
            m_detectedStars[star_number].max = max;
            m_detectedStars[star_number].fwhm = fwhm;
            //}
            max = m_image.maxCoeff( &x, &y );
            N_loops = N_loops + 1;
            z_score = ( max - mean ) / stddev;
            star_count = m_detectedStars.size();
            //std::cout << "In the !!!: " << "mean=" << mean << "  max=" << max << "  variance=" << variance << "  stddev=" << stddev << "  z-score=" << z_score << "  fwhm=" << fwhm << std::endl;
        }
    }
*/


    m_num_stars = m_detectedStars.size();
    // std::cout << "Acquire Star number: " << m_acquire_star << std::endl;
    // If statement that get the delta x and delta y from the 'center' of image
    static int delta_x;
    static int delta_y;
    //double theta = 3.14;  // rotation angle in radians 
    double scale = 1;//12.9/100;  // plate scale factor: deltatheta/deltapixel  
    //deltatheta -> Angular seperation between two stars in arcsec (from published data)
    //deltapixel -> Pixel seperation between same two stars on our detector 
    if( m_acquire_star >= 0 )
    {
        delta_x = m_detectedStars[m_acquire_star].x - m_x_center;
        delta_y = m_detectedStars[m_acquire_star].y - m_y_center;
        std::cout << "delta_x = " << delta_x << "    delta_y = " << delta_y << std::endl;
        m_acquire_star = -1;
        // DO MATH TO CONVERT FROM CHANGE OF PIXELS TO ALT AZ

        double theta = atan(delta_y/(delta_x+.000001)); //.00001 prevents division by zero 
        // Apply scaling and rotation to convert to new coordinates
        double new_x = scale * (delta_x * cos(theta) - delta_y * sin(theta));
        double new_y = scale * (delta_x * sin(theta) + delta_y * cos(theta));
        
        std::cout << "new_x = " << new_x << "    new_y = " << new_y << "     theta = " << theta << std::endl;
    }

    m_updated = true;


    // signal framegrabber
    // Now tell the f.g. to get going
    if( sem_post( &m_smSemaphore ) < 0 )
    {
        log<software_critical>( { __FILE__, __LINE__, errno, 0, "Error posting to semaphore" } );
        return -1;
    }

    // Update the latency circ. buffs
    if( m_xcb.maxEntries() > 0 )
    {
        m_xcb.nextEntry( m_x );
        m_ycb.nextEntry( m_y );
    }

    return 0;
}

inline int psfAcq::allocate( const darkShmimT &dummy )
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

inline int psfAcq::processImage( void *curr_src, const darkShmimT &dummy )
{
    static_cast<void>( dummy );

    std::unique_lock<std::mutex> lock( m_imageMutex );

    for( unsigned nn = 0; nn < darkShmimMonitorT::m_width * darkShmimMonitorT::m_height; ++nn )
    {
        m_dark.data()[nn] += ( (float *)curr_src )[nn];
    }

    lock.unlock();

    log<text_log>( "dark updated", logPrio::LOG_INFO );

    return 0;
}

inline int psfAcq::configureAcquisition()
{

    frameGrabberT::m_width = 2;
    frameGrabberT::m_height = m_first_x_vals.size() + 2; 
    frameGrabberT::m_dataType = _DATATYPE_FLOAT;

    return 0;
}

inline int psfAcq::startAcquisition()
{
    return 0;
}

inline int psfAcq::acquireAndCheckValid()
{
    timespec ts;

    if( clock_gettime( CLOCK_REALTIME, &ts ) < 0 )
    {
        log<software_critical>( { __FILE__, __LINE__, errno, 0, "clock_gettime" } );
        return -1;
    }

    ts.tv_sec += 1;

    if( sem_timedwait( &m_smSemaphore, &ts ) == 0 )
    {
        if( m_updated )
        {
            clock_gettime( CLOCK_REALTIME, &m_currImageTimestamp );
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

inline int psfAcq::loadImageIntoStream( void *dest )
{
    // Eigen::Map<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
    // destMap(reinterpret_cast<float*>(dest), frameGrabberT::m_width, frameGrabberT::m_height);
    Eigen::Map<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> destMap(
        reinterpret_cast<float *>( dest ),
        m_first_x_vals.size(),
        1 + m_first_y_vals.size() ); // 1+ keeps it from crashing when only one star is detected
    for( size_t i = 1; i <= m_first_x_vals.size(); i++ )
    {
        // Using destMap to store the x, y values with the respective offsets
        destMap( 2 * i - 2 ) = m_first_x_vals[i - 1] - m_dx; // Store x-coordinate
        destMap( 2 * i - 1 ) = m_first_y_vals[i - 1] - m_dy; // Store y-coordinate
    }

    m_updated = false;

    return 0;
}

inline int psfAcq::reconfig()
{
    return 0;
}

// Testing for user to select star number
INDI_NEWCALLBACK_DEFN( psfAcq, m_indiP_acquire_star )( const pcf::IndiProperty &ipRecv )
{
    if( ipRecv.getName() != m_indiP_acquire_star.getName() )
    {
        log<software_error>( { __FILE__, __LINE__, "wrong INDI property received." } );
        return -1;
    }

    float target;

    if( indiTargetUpdate( m_indiP_acquire_star, target, ipRecv, true ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    m_acquire_star = target;

    log<text_log>( "set acquire_star = " + std::to_string( m_acquire_star ), logPrio::LOG_NOTICE );
    return 0;
}

// Testing for num stars prop
INDI_NEWCALLBACK_DEFN( psfAcq, m_indiP_num_stars )( const pcf::IndiProperty &ipRecv )
{
    if( ipRecv.getName() != m_indiP_num_stars.getName() )
    {
        log<software_error>( { __FILE__, __LINE__, "wrong INDI property received." } );
        return -1;
    }

    float target;

    if( indiTargetUpdate( m_indiP_num_stars, target, ipRecv, true ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    m_num_stars = target;

    log<text_log>( "set acquire_star = " + std::to_string( m_num_stars ), logPrio::LOG_NOTICE );

    return 0;
}

INDI_SETCALLBACK_DEFN( psfAcq, m_indiP_fpsSource )( const pcf::IndiProperty &ipRecv )
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
        shmimMonitorT::m_restart = true;
    }

    return 0;
}

inline int psfAcq::checkRecordTimes()
{
    return telemeterT::checkRecordTimes( telem_fgtimings() );
}

inline int psfAcq::recordTelem( const telem_fgtimings * )
{
    return recordFGTimings( true );
}

} // namespace app
} // namespace MagAOX

#endif // psfAcq_hpp