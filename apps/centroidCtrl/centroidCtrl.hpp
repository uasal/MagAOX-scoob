/** \file centroidCtrl.hpp
 * \brief MagAO-X template app for PSF centroiding from a camera shmim.
 *
 * \ingroup centroidCtrl_files
 */

#ifndef centroidCtrl_hpp
#define centroidCtrl_hpp

#include <mx/improc/eigenImage.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

/** \defgroup centroidCtrl
 * \brief Template MagAO-X app to compute PSF (x,y) from an ImageStreamIO camera stream.
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

/// MagAO-X template application for PSF centroid calculation
/** Monitors an input camera shmim via ImageStreamIO.  On each new frame,
 * runs #calculateCentroid and publishes the result as INDI `x.current` /
 * `y.current`.
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
     * Template implementation: center-of-mass.
     *
     * \param[out] x      centroid x coordinate in pixels (0-based, along width)
     * \param[out] y      centroid y coordinate in pixels (0-based, along height)
     * \param[in]  image  contiguous row-major image data, size width*height
     * \param[in]  width  image width in pixels
     * \param[in]  height image height in pixels
     *
     * \returns 0 on success
     * \returns -1 on error (e.g. empty image / zero flux)
     */
    int calculateCentroid( realT &x, realT &y, const realT *image, uint32_t width, uint32_t height );

  protected:

    pcf::IndiProperty m_indiP_x; ///< Read-only property `x` with element `current`
    pcf::IndiProperty m_indiP_y; ///< Read-only property `y` with element `current`

};

inline centroidCtrl::centroidCtrl() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    // Process the existing buffer on connect so we publish a first centroid quickly.
    shmimMonitorT::m_getExistingFirst = true;

    return;
}

inline void centroidCtrl::setupConfig()
{
    SHMIMMONITOR_SETUP_CONFIG( config );

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

    // Prefer the dedicated cam_name option as the shmim to monitor.
    if( m_camName != "" )
    {
        shmimMonitorT::m_shmimName = m_camName;
    }

    return 0;
}

inline void centroidCtrl::loadConfig()
{
    loadConfigImpl( config );
}

inline int centroidCtrl::appStartup()
{
    CREATE_REG_INDI_RO_NUMBER( m_indiP_x, "x", "Centroid X", "Centroid" );
    m_indiP_x.add( pcf::IndiElement( "current", 0 ) );

    CREATE_REG_INDI_RO_NUMBER( m_indiP_y, "y", "Centroid Y", "Centroid" );
    m_indiP_y.add( pcf::IndiElement( "current", 0 ) );

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
        return 0;
    }

    realT x = 0;
    realT y = 0;

    // compute centroid
    if( calculateCentroid( x, y, m_image.data(), width, height ) < 0 )
    {
        return 0;
    }

    // update member variables with result
    m_x = x;
    m_y = y;

    // Publish to INDI on every new frame (x.current / y.current).
    {
        std::unique_lock<std::mutex> lock( m_indiMutex );
        updateIfChanged( m_indiP_x, "current", m_x );
        updateIfChanged( m_indiP_y, "current", m_y );
    }

    return 0;
}

inline int centroidCtrl::calculateCentroid( realT &x, realT &y, const realT *image, uint32_t width, uint32_t height )
{
    // =====================================================================
    // MODIFY THIS FUNCTION to test different PSF centroid algorithms.
    //
    // Inputs:
    //   image  - float pixels, length width*height, row-major
    //            index = ix + iy * width, with ix in [0, width), iy in [0, height)
    //   width  - image width  [pixels]  (from the connected shmim)
    //   height - image height [pixels]  (from the connected shmim)
    //
    // Outputs:
    //   x, y   - centroid position in pixels (0-based). These are reported
    //            to INDI as properties x.current and y.current.
    //
    // Return:
    //          - -1 on failure, 0 on success
    //
    // Template algorithm below: intensity-weighted center of mass.
    // =====================================================================

    if( image == nullptr || width == 0 || height == 0 )
    {
        return -1;
    }

    double sum  = 0;
    double sumx = 0;
    double sumy = 0;

    for( uint32_t iy = 0; iy < height; ++iy )
    {
        for( uint32_t ix = 0; ix < width; ++ix )
        {
            const double pix = image[ix + iy * width];
            sum += pix;
            sumx += pix * static_cast<double>( ix );
            sumy += pix * static_cast<double>( iy );
        }
    }

    if( sum <= 0 )
    {
        // No flux: fall back to geometric center so INDI still updates.
        x = 0.5f * static_cast<realT>( width - 1 );
        y = 0.5f * static_cast<realT>( height - 1 );
        return 0;
    }

    x = static_cast<realT>( sumx / sum );
    y = static_cast<realT>( sumy / sum );

    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // centroidCtrl_hpp
