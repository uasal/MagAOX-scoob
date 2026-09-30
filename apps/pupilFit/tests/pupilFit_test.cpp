/** \file pupilFit_test.cpp
 * \brief Catch2 tests for the pupilFit app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup pupilFit_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdlib>
#include <filesystem>
#include <semaphore.h>
#include <string>
#include <vector>

#include "../pupilFit.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup pupilFit_unit_test pupilFit Unit Tests
 * \brief Unit tests for the pupilFit application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `pupilFit` unit tests.
/** \ingroup pupilFit_unit_test
 */
namespace pupilFitTest
{

/// Directory used as `MILK_SHM_DIR` for the output streams created by allocate().
constexpr const char *c_shmDir = "/tmp/pupilFit_test_shm";

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing pupilFit internals.
class pupilFit_test : public pupilFit
{
  public:
    /// Construct a harness with the given device name.
    explicit pupilFit_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        sem_init( &m_smSemaphore, 0, 0 );
    }

    /// Destroy the harness, releasing the semaphore.
    ~pupilFit_test() noexcept
    {
        sem_destroy( &m_smSemaphore );
    }

    using pupilFit::m_averaging;
    using pupilFit::m_avg_dx;
    using pupilFit::m_avg_dy;
    using pupilFit::m_avgD4sq_accum;
    using pupilFit::m_avgmedAll_accum;
    using pupilFit::m_avgx1_accum;
    using pupilFit::m_avgy2sq_accum;
    using pupilFit::m_defSetD1;
    using pupilFit::m_defSetD4;
    using pupilFit::m_defSetx1;
    using pupilFit::m_defSetx2;
    using pupilFit::m_defSetx3;
    using pupilFit::m_defSetx4;
    using pupilFit::m_defSety1;
    using pupilFit::m_defSety2;
    using pupilFit::m_defSety3;
    using pupilFit::m_defSety4;
    using pupilFit::m_edgeIm;
    using pupilFit::m_edgeShmimConnected;
    using pupilFit::m_edgeShmimName;
    using pupilFit::m_fitIm;
    using pupilFit::m_fitter;
    using pupilFit::m_indiP_quad1;
    using pupilFit::m_indiP_quad2;
    using pupilFit::m_indiP_quad3;
    using pupilFit::m_indiP_quad4;
    using pupilFit::m_navg;
    using pupilFit::m_numPupils;
    using pupilFit::m_refIm;
    using pupilFit::m_setD1;
    using pupilFit::m_setD2;
    using pupilFit::m_setD3;
    using pupilFit::m_setD4;
    using pupilFit::m_setPointSource;
    using pupilFit::m_setx1;
    using pupilFit::m_setx2;
    using pupilFit::m_setx3;
    using pupilFit::m_setx4;
    using pupilFit::m_sety1;
    using pupilFit::m_sety2;
    using pupilFit::m_sety3;
    using pupilFit::m_sety4;
    using pupilFit::m_threshold;
    using pupilFit::m_threshShmimConnected;
    using pupilFit::m_threshShmimName;
    using pupilFit::m_updated;
    using pupilFit::m_userSetD1;
    using pupilFit::m_userSetx1;
    using pupilFit::m_userSetx4;
    using pupilFit::m_userSety2;

    using pupilFit::newCallBack_m_indiP_averaging;
    using pupilFit::newCallBack_m_indiP_refmode;
    using pupilFit::newCallBack_m_indiP_reload;
    using pupilFit::newCallBack_m_indiP_thresh;
    using pupilFit::newCallBack_m_indiP_update;

    /// Read a config file and run loadConfigImpl().
    /**
     * \returns the loadConfigImpl() return value
     */
    int loadConfigFromFile( const std::string &fname /**< [in] config file path */ )
    {
        setupConfig();
        config.readConfig( fname );
        return loadConfigImpl( config );
    }

    /// Create the INDI properties the callbacks use, as appStartup() would, without registering them.
    void initIndiProps()
    {
        createStandardIndiNumber<float>( m_indiP_thresh, "threshold", 0, 1, 0, "%0.2f", "Threshold" );
        m_indiP_thresh["current"].set( m_threshold );
        m_indiP_thresh["target"].set( m_threshold );

        createStandardIndiToggleSw( m_indiP_averaging, "averaging", "Start/Stop Averaging" );
        createStandardIndiRequestSw( m_indiP_reload, "setpt_reload", "Reload Calibration" );
        createStandardIndiRequestSw( m_indiP_update, "setpt_current", "Set Reference" );
        createStandardIndiSelectionSw(
            m_indiP_refmode, "setpt_mode", std::vector<std::string>( { "default", "refim", "user" } ) );

        makeQuad( m_indiP_quad1, "quadrant1" );
        makeQuad( m_indiP_quad2, "quadrant2" );
        makeQuad( m_indiP_quad3, "quadrant3" );
        makeQuad( m_indiP_quad4, "quadrant4" );
    }

    /// Set the main image stream geometry as the shmimMonitor would.
    void setImageStream( uint32_t w, /**< [in] the image width */
                         uint32_t h, /**< [in] the image height */
                         uint8_t  dt /**< [in] the ImageStreamIO data type */
    )
    {
        shmimMonitorT::m_width    = w;
        shmimMonitorT::m_height   = h;
        shmimMonitorT::m_dataType = dt;
    }

    /// Set the reference image stream geometry as the shmimMonitor would.
    void setRefStream( uint32_t w, /**< [in] the image width */
                       uint32_t h, /**< [in] the image height */
                       uint8_t  dt /**< [in] the ImageStreamIO data type */
    )
    {
        refShmimMonitorT::m_width    = w;
        refShmimMonitorT::m_height   = h;
        refShmimMonitorT::m_dataType = dt;
    }

    /// Call the main image allocate().
    /**
     * \returns the allocate() return value
     */
    int allocateImage()
    {
        return allocate( dev::shmimT() );
    }

    /// Call the reference allocate().
    /**
     * \returns the allocate() return value
     */
    int allocateRef()
    {
        return allocate( refShmimT() );
    }

    /// Call the reference processImage().
    /**
     * \returns the processImage() return value
     */
    int processRef( std::vector<float> &im /**< [in] the reference frame */ )
    {
        return processImage( static_cast<void *>( im.data() ), refShmimT() );
    }

    /// Post to the frame-grabber semaphore.
    void postSem()
    {
        sem_post( &m_smSemaphore );
    }

    /// Access the main shmimMonitor restart flag.
    /**
     * \returns a reference to the flag
     */
    bool &restart()
    {
        return shmimMonitorT::m_restart;
    }

    /// Get the main shmimMonitor stream name.
    /**
     * \returns the stream name
     */
    std::string imShmimName()
    {
        return shmimMonitorT::m_shmimName;
    }

    /// Get the reference shmimMonitor stream name.
    /**
     * \returns the stream name
     */
    std::string refShmimName()
    {
        return refShmimMonitorT::m_shmimName;
    }

    /// Get whether the reference shmimMonitor loads an existing image first.
    /**
     * \returns the flag value
     */
    bool refGetExistingFirst()
    {
        return refShmimMonitorT::m_getExistingFirst;
    }

    /// Get the frameGrabber stream name.
    /**
     * \returns the stream name
     */
    std::string fgShmimName()
    {
        return frameGrabberT::m_shmimName;
    }

    /// Get the frameGrabber width.
    /**
     * \returns the width
     */
    uint32_t fgWidth()
    {
        return frameGrabberT::m_width;
    }

    /// Get the frameGrabber height.
    /**
     * \returns the height
     */
    uint32_t fgHeight()
    {
        return frameGrabberT::m_height;
    }

    /// Get the frameGrabber data type.
    /**
     * \returns the ImageStreamIO data type code
     */
    uint8_t fgDataType()
    {
        return frameGrabberT::m_dataType;
    }

    /// Get the frameGrabber current image timestamp.
    /**
     * \returns the timestamp
     */
    timespec fgTimestamp()
    {
        return frameGrabberT::m_currImageTimestamp;
    }

  protected:
    /// Create a read-only quadrant property with x, y and D elements.
    void makeQuad( pcf::IndiProperty &prop, /**< [out] the property to create */
                   const std::string &name  /**< [in] the property name */
    )
    {
        createROIndiNumber( prop, name );
        MagAOX::app::indi::addNumberElement<float>( prop, "x", 0, 59, 0, "%0.2f", "" );
        MagAOX::app::indi::addNumberElement<float>( prop, "y", 0, 59, 0, "%0.2f", "" );
        MagAOX::app::indi::addNumberElement<float>( prop, "D", 0, 59, 0, "%0.2f", "" );
    }
};
/// \endcond

/// Build a switch property request with one element.
/**
 * \returns the property
 */
pcf::IndiProperty switchRequest( const std::string                &device, /**< [in] the device name */
                                 const std::string                &name,   /**< [in] the property name */
                                 const std::string                &el,     /**< [in] the element name */
                                 pcf::IndiElement::SwitchStateType state   /**< [in] the switch state */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, state ) );
    return ip;
}

/// Paint a filled circular pupil into an image.
void addPupil( mx::improc::eigenImage<float> &im, /**< [in.out] the image to paint into */
               float                          xc, /**< [in] the row (x) coordinate of the pupil center */
               float                          yc, /**< [in] the column (y) coordinate of the pupil center */
               float                          r,  /**< [in] the pupil radius in pixels */
               float                          val /**< [in] the value to assign to pupil pixels */
)
{
    for( int i = 0; i < im.rows(); ++i )
    {
        for( int j = 0; j < im.cols(); ++j )
        {
            if( ( i - xc ) * ( i - xc ) + ( j - yc ) * ( j - yc ) <= r * r )
            {
                im( i, j ) = val;
            }
        }
    }
}

/// Verify construction-time settings.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFit construction defaults", "[pupilFit]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFit::pupilFit();
    pupilFit::fps();
    #endif
    // clang-format on

    pupilFit_test app( "pupilfit" );

    REQUIRE( app.refGetExistingFirst() == true );
    REQUIRE( pupilFit::c_frameGrabber_flippable == false );
    REQUIRE( app.fps() == Approx( 1.0 ) );
    REQUIRE( app.m_setPointSource == USEDEFSET );
    REQUIRE( app.m_threshShmimConnected == false );
    REQUIRE( app.m_edgeShmimConnected == false );
    REQUIRE( app.m_averaging == false );
}

/// Verify configuration defaults.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFit configuration defaults", "[pupilFit]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFit::setupConfig();
    pupilFit::loadConfigImpl( config );
    #endif
    // clang-format on

    pupilFit_test app( "pupilfit" );

    const std::string fname = "/tmp/pupilFit_test_defaults.conf";
    mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );
    REQUIRE( app.loadConfigFromFile( fname ) == 0 );
    std::filesystem::remove( fname );

    REQUIRE( app.m_threshold == Approx( 0.5 ) );
    REQUIRE( app.m_threshShmimName == "camwfs_thresh" );
    REQUIRE( app.m_edgeShmimName == "camwfs_edge" );
    REQUIRE( app.m_numPupils == 4 );
    REQUIRE( app.m_fitter.m_pupMedIndex == Approx( 0.6867 ) );

    REQUIRE( app.imShmimName() == "camwfs_avg" );
    REQUIRE( app.fgShmimName() == "pupilfit" );

    // shmimMonitor::setupConfig defaults the reference stream name to the config name,
    // so the reference-image set point source is selected unless refShmim.shmimName is changed.
    REQUIRE( app.refShmimName() == "pupilfit" );
    REQUIRE( app.m_setPointSource == USEREFIM );

    REQUIRE( app.m_defSetx1 == Approx( 29.5 ) );
    REQUIRE( app.m_defSety1 == Approx( 29.5 ) );
    REQUIRE( app.m_defSetD1 == Approx( 56.0 ) );
    REQUIRE( app.m_defSetx2 == Approx( 89.5 ) );
    REQUIRE( app.m_defSety2 == Approx( 29.5 ) );
    REQUIRE( app.m_defSetx3 == Approx( 29.5 ) );
    REQUIRE( app.m_defSety3 == Approx( 89.5 ) );
    REQUIRE( app.m_defSetx4 == Approx( 89.5 ) );
    REQUIRE( app.m_defSety4 == Approx( 89.5 ) );
    REQUIRE( app.m_defSetD4 == Approx( 56.0 ) );
}

/// Verify configuration overrides.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFit configuration overrides", "[pupilFit]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFit::setupConfig();
    pupilFit::loadConfigImpl( config );
    #endif
    // clang-format on

    pupilFit_test app( "pupilfit" );

    const std::string fname = "/tmp/pupilFit_test_overrides.conf";
    mx::app::writeConfigFile( fname,
                              { "fit",
                                "fit",
                                "fit",
                                "fit",
                                "fit",
                                "cal",
                                "cal",
                                "cal",
                                "cal",
                                "cal",
                                "cal",
                                "shmimMonitor",
                                "refShmim",
                                "framegrabber" },
                              { "threshold",
                                "threshShmimName",
                                "edgeShmimName",
                                "numPupils",
                                "pupMedIndex",
                                "setx1",
                                "sety1",
                                "setD1",
                                "setx2",
                                "sety3",
                                "setD4",
                                "shmimName",
                                "shmimName",
                                "shmimName" },
                              { "0.3",
                                "pf_thresh",
                                "pf_edge",
                                "3",
                                "0.6",
                                "30.25",
                                "31.5",
                                "50",
                                "88.75",
                                "90.5",
                                "55",
                                "camtest",
                                "pfref",
                                "pfout" } );
    REQUIRE( app.loadConfigFromFile( fname ) == 0 );
    std::filesystem::remove( fname );

    REQUIRE( app.m_threshold == Approx( 0.3 ) );
    REQUIRE( app.m_threshShmimName == "pf_thresh" );
    REQUIRE( app.m_edgeShmimName == "pf_edge" );
    REQUIRE( app.m_numPupils == 3 );
    REQUIRE( app.m_fitter.m_pupMedIndex == Approx( 0.6 ) );

    REQUIRE( app.m_defSetx1 == Approx( 30.25 ) );
    REQUIRE( app.m_defSety1 == Approx( 31.5 ) );
    REQUIRE( app.m_defSetD1 == Approx( 50 ) );
    REQUIRE( app.m_defSetx2 == Approx( 88.75 ) );
    REQUIRE( app.m_defSety2 == Approx( 29.5 ) );
    REQUIRE( app.m_defSety3 == Approx( 90.5 ) );
    REQUIRE( app.m_defSetD4 == Approx( 55 ) );

    REQUIRE( app.imShmimName() == "camtest" );
    REQUIRE( app.refShmimName() == "pfref" );
    REQUIRE( app.fgShmimName() == "pfout" );
    REQUIRE( app.m_setPointSource == USEREFIM );
}

/// Verify the reference stream allocate() and processImage().
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFit reference stream handling", "[pupilFit]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFit::allocate( refShmimT() );
    pupilFit::processImage( curr_src, refShmimT() );
    #endif
    // clang-format on

    pupilFit_test app( "pupilfit" );

    SECTION( "non-float reference is rejected" )
    {
        app.setRefStream( 4, 6, _DATATYPE_UINT16 );
        REQUIRE( app.allocateRef() == -1 );
        REQUIRE( app.m_refIm.rows() == 0 );
    }

    SECTION( "float reference is allocated and copied" )
    {
        app.setRefStream( 4, 6, _DATATYPE_FLOAT );
        REQUIRE( app.allocateRef() == 0 );
        REQUIRE( app.m_refIm.rows() == 4 );
        REQUIRE( app.m_refIm.cols() == 6 );

        std::vector<float> ref( 24 );
        for( size_t n = 0; n < ref.size(); ++n )
        {
            ref[n] = n + 0.5;
        }

        app.restart() = false;
        REQUIRE( app.processRef( ref ) == 0 );

        REQUIRE( app.m_refIm.data()[0] == Approx( 0.5 ) );
        REQUIRE( app.m_refIm.data()[23] == Approx( 23.5 ) );
        REQUIRE( app.m_refIm.sum() == Approx( 24 * 12.0 ) );

        // A new reference triggers a restart of the main stream so set points are re-fit
        REQUIRE( app.restart() == true );
    }
}

/// Verify allocate() for the main stream selects the set points and creates the output streams.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFit allocate sets up the fitter and set points", "[pupilFit]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFit::allocate( dev::shmimT() );
    pupilFit::~pupilFit();
    #endif
    // clang-format on

    std::filesystem::create_directories( c_shmDir );
    setenv( "MILK_SHM_DIR", c_shmDir, 1 );

    const std::string threshFile = std::string( c_shmDir ) + "/pupilFit_test_thresh.im.shm";
    const std::string edgeFile   = std::string( c_shmDir ) + "/pupilFit_test_edge.im.shm";

    { // app scope
        pupilFit_test app( "pupilfit" );
        app.m_threshShmimName = "pupilFit_test_thresh";
        app.m_edgeShmimName   = "pupilFit_test_edge";
        app.setImageStream( 120, 120, _DATATYPE_FLOAT );

        app.m_defSetx1 = 31;
        app.m_defSety1 = 32;
        app.m_defSetD1 = 50;
        app.m_defSetx4 = 88;
        app.m_defSety4 = 87;
        app.m_defSetD4 = 51;

        app.m_userSetx1 = 28;
        app.m_userSetD1 = 57;
        app.m_userSety2 = 30;
        app.m_userSetx4 = 90;

        app.m_threshold = 0.4;

        SECTION( "default set points" )
        {
            app.m_setPointSource = USEDEFSET;

            REQUIRE( app.allocateImage() == 0 );

            REQUIRE( app.m_fitIm.rows() == 120 );
            REQUIRE( app.m_fitIm.cols() == 120 );
            REQUIRE( app.m_edgeIm.rows() == 120 );
            REQUIRE( app.m_edgeIm.cols() == 120 );

            REQUIRE( app.m_fitter.m_numPupils == 4 );
            REQUIRE( app.m_fitter.m_rows == 60 );
            REQUIRE( app.m_fitter.m_cols == 60 );
            REQUIRE( app.m_fitter.m_thresh == Approx( 0.4 ) );

            REQUIRE( app.m_setx1 == Approx( 31 ) );
            REQUIRE( app.m_sety1 == Approx( 32 ) );
            REQUIRE( app.m_setD1 == Approx( 50 ) );
            REQUIRE( app.m_setx4 == Approx( 88 ) );
            REQUIRE( app.m_sety4 == Approx( 87 ) );
            REQUIRE( app.m_setD4 == Approx( 51 ) );

            REQUIRE( app.m_threshShmimConnected == true );
            REQUIRE( app.m_edgeShmimConnected == true );
            REQUIRE( std::filesystem::exists( threshFile ) );
            REQUIRE( std::filesystem::exists( edgeFile ) );

            // Re-allocating destroys and re-creates the output streams
            REQUIRE( app.allocateImage() == 0 );
            REQUIRE( app.m_threshShmimConnected == true );
            REQUIRE( std::filesystem::exists( threshFile ) );
        }

        SECTION( "user set points" )
        {
            app.m_setPointSource = USEUSERSET;

            REQUIRE( app.allocateImage() == 0 );

            REQUIRE( app.m_setx1 == Approx( 28 ) );
            REQUIRE( app.m_setD1 == Approx( 57 ) );
            REQUIRE( app.m_sety2 == Approx( 30 ) );
            REQUIRE( app.m_setx4 == Approx( 90 ) );
        }

        SECTION( "3 pupils leave the 4th set point alone" )
        {
            app.m_setPointSource = USEDEFSET;
            app.m_numPupils      = 3;
            app.m_setx4          = -1;

            REQUIRE( app.allocateImage() == 0 );

            REQUIRE( app.m_fitter.m_numPupils == 3 );
            REQUIRE( app.m_setx1 == Approx( 31 ) );
            REQUIRE( app.m_setx4 == Approx( -1 ) );
        }

        SECTION( "reference image of the wrong size falls back to the defaults" )
        {
            app.m_setPointSource = USEREFIM;
            app.m_refIm.resize( 60, 60 );
            app.m_refIm.setZero();

            REQUIRE( app.allocateImage() == 0 );

            REQUIRE( app.m_setx1 == Approx( 31 ) );
            REQUIRE( app.m_setD4 == Approx( 51 ) );
        }

        SECTION( "reference image sets the set points by fitting it" )
        {
            app.m_setPointSource = USEREFIM;
            app.m_threshold      = 0.5;

            app.m_refIm.resize( 120, 120 );
            app.m_refIm.setConstant( 10 );
            addPupil( app.m_refIm, 30.5, 28.5, 24, 1000 );
            addPupil( app.m_refIm, 89.5, 29.5, 24, 1000 );
            addPupil( app.m_refIm, 29.5, 89.5, 24, 1000 );
            addPupil( app.m_refIm, 89.5, 89.5, 24, 1000 );

            mx::improc::eigenImage<float> refCopy = app.m_refIm;

            REQUIRE( app.allocateImage() == 0 );

            // Set points come from the fit, not the defaults
            REQUIRE( app.m_setx1 == Approx( 30.5 ).margin( 1.0 ) );
            REQUIRE( app.m_sety1 == Approx( 28.5 ).margin( 1.0 ) );
            REQUIRE( app.m_setD1 == Approx( 2 * 24.5 ).margin( 2.0 ) );
            REQUIRE( app.m_setx2 - app.m_setx1 == Approx( 59 ).margin( 0.3 ) );
            REQUIRE( app.m_sety4 - app.m_sety1 == Approx( 61 ).margin( 0.3 ) );
            REQUIRE( app.m_setD4 == Approx( app.m_setD1 ).margin( 0.3 ) );

            REQUIRE( app.m_setx1 == Approx( app.m_fitter.m_avgx[0] ) );
            REQUIRE( app.m_setD3 == Approx( 2 * app.m_fitter.m_avgr[2] ) );

            // The reference image itself is not modified
            REQUIRE( ( app.m_refIm - refCopy ).abs().maxCoeff() == 0 );
        }
    }

    // The destructor destroys the output streams, removing their files
    REQUIRE_FALSE( std::filesystem::exists( threshFile ) );
    REQUIRE_FALSE( std::filesystem::exists( edgeFile ) );

    std::filesystem::remove_all( c_shmDir );
}

/// Verify the frameGrabber interface.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFit frameGrabber interface", "[pupilFit]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFit::configureAcquisition();
    pupilFit::startAcquisition();
    pupilFit::acquireAndCheckValid();
    pupilFit::loadImageIntoStream( dest );
    pupilFit::reconfig();
    #endif
    // clang-format on

    pupilFit_test app( "pupilfit" );

    SECTION( "configureAcquisition sets a 2x1 float stream" )
    {
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.fgWidth() == 2 );
        REQUIRE( app.fgHeight() == 1 );
        REQUIRE( app.fgDataType() == _DATATYPE_FLOAT );
    }

    SECTION( "startAcquisition and reconfig are no-ops" )
    {
        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( app.reconfig() == 0 );
    }

    SECTION( "loadImageIntoStream writes the average deltas" )
    {
        app.m_avg_dx  = 1.5;
        app.m_avg_dy  = -2.25;
        app.m_updated = true;

        float dest[2] = { 0, 0 };
        REQUIRE( app.loadImageIntoStream( dest ) == 0 );
        REQUIRE( dest[0] == Approx( 1.5 ) );
        REQUIRE( dest[1] == Approx( -2.25 ) );
        REQUIRE( app.m_updated == false );
    }

    SECTION( "acquireAndCheckValid returns 0 for an updated frame" )
    {
        app.m_updated = true;
        app.postSem();

        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( app.fgTimestamp().tv_sec > 0 );
    }

    SECTION( "acquireAndCheckValid returns 1 when not updated" )
    {
        app.m_updated = false;
        app.postSem();

        REQUIRE( app.acquireAndCheckValid() == 1 );
    }

    SECTION( "acquireAndCheckValid returns 1 on timeout" )
    {
        app.m_updated = true;

        // waits the 1 second timeout
        REQUIRE( app.acquireAndCheckValid() == 1 );
    }
}

/// Verify the threshold INDI callback.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFit threshold INDI callback", "[pupilFit][indi]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFit::newCallBack_m_indiP_thresh( ip );
    #endif
    // clang-format on

    pupilFit_test app( "pupilfit" );
    app.initIndiProps();

    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "pupilfit" );
    ip.setName( "threshold" );

    SECTION( "wrong name is rejected" )
    {
        ip.setName( "wrong" );
        ip.add( pcf::IndiElement( "target", 0.3f ) );
        REQUIRE( app.newCallBack_m_indiP_thresh( ip ) == -1 );
        REQUIRE( app.m_threshold == Approx( 0.5 ) );
    }

    SECTION( "wrong device is rejected" )
    {
        ip.setDevice( "wrong" );
        ip.add( pcf::IndiElement( "target", 0.3f ) );
        REQUIRE( app.newCallBack_m_indiP_thresh( ip ) == -1 );
        REQUIRE( app.m_threshold == Approx( 0.5 ) );
    }

    SECTION( "no target or current is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_thresh( ip ) == -1 );
        REQUIRE( app.m_threshold == Approx( 0.5 ) );
    }

    SECTION( "target sets the threshold" )
    {
        app.m_setPointSource = USEDEFSET;
        app.restart()        = false;

        ip.add( pcf::IndiElement( "target", 0.3f ) );
        REQUIRE( app.newCallBack_m_indiP_thresh( ip ) == 0 );
        REQUIRE( app.m_threshold == Approx( 0.3 ) );
        REQUIRE( app.restart() == false );
    }

    SECTION( "current sets the threshold" )
    {
        ip.add( pcf::IndiElement( "current", 0.7f ) );
        REQUIRE( app.newCallBack_m_indiP_thresh( ip ) == 0 );
        REQUIRE( app.m_threshold == Approx( 0.7 ) );
    }

    SECTION( "with the reference image in use the stream is restarted" )
    {
        app.m_setPointSource = USEREFIM;
        app.restart()        = false;

        ip.add( pcf::IndiElement( "target", 0.45f ) );
        REQUIRE( app.newCallBack_m_indiP_thresh( ip ) == 0 );
        REQUIRE( app.m_threshold == Approx( 0.45 ) );
        REQUIRE( app.restart() == true );
    }
}

/// Verify the averaging INDI callback.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFit averaging INDI callback", "[pupilFit][indi]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFit::newCallBack_m_indiP_averaging( ip );
    #endif
    // clang-format on

    pupilFit_test app( "pupilfit" );
    app.initIndiProps();

    SECTION( "wrong name is rejected" )
    {
        pcf::IndiProperty ip = switchRequest( "pupilfit", "wrong", "toggle", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_averaging( ip ) == -1 );
        REQUIRE( app.m_averaging == false );
    }

    SECTION( "toggle on resets the accumulators and starts averaging" )
    {
        app.m_navg            = 12;
        app.m_avgx1_accum     = 3;
        app.m_avgy2sq_accum   = 4;
        app.m_avgD4sq_accum   = 5;
        app.m_avgmedAll_accum = 6;

        pcf::IndiProperty ip = switchRequest( "pupilfit", "averaging", "toggle", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_averaging( ip ) == 0 );

        REQUIRE( app.m_averaging == true );
        REQUIRE( app.m_navg == 0 );
        REQUIRE( app.m_avgx1_accum == 0 );
        REQUIRE( app.m_avgy2sq_accum == 0 );
        REQUIRE( app.m_avgD4sq_accum == 0 );
        REQUIRE( app.m_avgmedAll_accum == 0 );

        SECTION( "toggle off stops averaging" )
        {
            app.m_navg = 7;

            pcf::IndiProperty ip2 = switchRequest( "pupilfit", "averaging", "toggle", pcf::IndiElement::Off );
            REQUIRE( app.newCallBack_m_indiP_averaging( ip2 ) == 0 );

            REQUIRE( app.m_averaging == false );
            REQUIRE( app.m_navg == 7 );
        }
    }
}

/// Verify the set point reload INDI callback.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFit set point reload INDI callback", "[pupilFit][indi]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFit::newCallBack_m_indiP_reload( ip );
    #endif
    // clang-format on

    pupilFit_test app( "pupilfit" );
    app.initIndiProps();
    app.restart() = false;

    SECTION( "wrong name is rejected" )
    {
        pcf::IndiProperty ip = switchRequest( "pupilfit", "wrong", "request", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_reload( ip ) == -1 );
        REQUIRE( app.restart() == false );
    }

    SECTION( "request off does nothing" )
    {
        pcf::IndiProperty ip = switchRequest( "pupilfit", "setpt_reload", "request", pcf::IndiElement::Off );
        REQUIRE( app.newCallBack_m_indiP_reload( ip ) == 0 );
        REQUIRE( app.restart() == false );
    }

    SECTION( "request on restarts the main stream" )
    {
        pcf::IndiProperty ip = switchRequest( "pupilfit", "setpt_reload", "request", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_reload( ip ) == 0 );
        REQUIRE( app.restart() == true );
    }
}

/// Verify the set-reference-to-current INDI callback.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFit set point update INDI callback", "[pupilFit][indi]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFit::newCallBack_m_indiP_update( ip );
    #endif
    // clang-format on

    pupilFit_test app( "pupilfit" );
    app.initIndiProps();
    app.restart() = false;

    app.m_indiP_quad1["x"] = 30.1f;
    app.m_indiP_quad1["y"] = 29.1f;
    app.m_indiP_quad1["D"] = 55.1f;
    app.m_indiP_quad2["x"] = 90.2f;
    app.m_indiP_quad2["y"] = 28.2f;
    app.m_indiP_quad2["D"] = 55.2f;
    app.m_indiP_quad3["x"] = 31.3f;
    app.m_indiP_quad3["y"] = 88.3f;
    app.m_indiP_quad3["D"] = 55.3f;
    app.m_indiP_quad4["x"] = 91.4f;
    app.m_indiP_quad4["y"] = 87.4f;
    app.m_indiP_quad4["D"] = 55.4f;

    SECTION( "wrong name is rejected" )
    {
        pcf::IndiProperty ip = switchRequest( "pupilfit", "wrong", "request", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_update( ip ) == -1 );
        REQUIRE( app.m_setx1 == Approx( 29.5 ) );
    }

    SECTION( "request off does nothing" )
    {
        pcf::IndiProperty ip = switchRequest( "pupilfit", "setpt_current", "request", pcf::IndiElement::Off );
        REQUIRE( app.newCallBack_m_indiP_update( ip ) == 0 );
        REQUIRE( app.m_setx1 == Approx( 29.5 ) );
    }

    SECTION( "request on records the current measurements (4 pupils)" )
    {
        app.m_setPointSource = USEDEFSET;

        pcf::IndiProperty ip = switchRequest( "pupilfit", "setpt_current", "request", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_update( ip ) == 0 );

        REQUIRE( app.m_setx1 == Approx( 30.1 ) );
        REQUIRE( app.m_sety1 == Approx( 29.1 ) );
        REQUIRE( app.m_setD1 == Approx( 55.1 ) );
        REQUIRE( app.m_setx2 == Approx( 90.2 ) );
        REQUIRE( app.m_sety2 == Approx( 28.2 ) );
        REQUIRE( app.m_setD2 == Approx( 55.2 ) );
        REQUIRE( app.m_setx3 == Approx( 31.3 ) );
        REQUIRE( app.m_sety3 == Approx( 88.3 ) );
        REQUIRE( app.m_setD3 == Approx( 55.3 ) );
        REQUIRE( app.m_setx4 == Approx( 91.4 ) );
        REQUIRE( app.m_sety4 == Approx( 87.4 ) );
        REQUIRE( app.m_setD4 == Approx( 55.4 ) );

        // Only the user set point mode restarts
        REQUIRE( app.restart() == false );
    }

    SECTION( "request on with 3 pupils leaves quad 4 alone and restarts in user mode" )
    {
        app.m_numPupils      = 3;
        app.m_setPointSource = USEUSERSET;

        pcf::IndiProperty ip = switchRequest( "pupilfit", "setpt_current", "request", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_update( ip ) == 0 );

        REQUIRE( app.m_setx3 == Approx( 31.3 ) );
        REQUIRE( app.m_setx4 == Approx( 89.5 ) );
        REQUIRE( app.m_setD4 == Approx( 56.0 ) );
        REQUIRE( app.restart() == true );
    }
}

/// Verify the set point mode INDI callback.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFit set point mode INDI callback", "[pupilFit][indi]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFit::newCallBack_m_indiP_refmode( ip );
    #endif
    // clang-format on

    pupilFit_test app( "pupilfit" );
    app.initIndiProps();
    app.m_setPointSource = USEDEFSET;
    app.restart()        = false;

    SECTION( "wrong name is rejected" )
    {
        pcf::IndiProperty ip = switchRequest( "pupilfit", "wrong", "user", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_refmode( ip ) == -1 );
        REQUIRE( app.m_setPointSource == USEDEFSET );
    }

    SECTION( "selecting the current mode does not restart" )
    {
        pcf::IndiProperty ip = switchRequest( "pupilfit", "setpt_mode", "default", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_refmode( ip ) == 0 );
        REQUIRE( app.m_setPointSource == USEDEFSET );
        REQUIRE( app.restart() == false );
    }

    SECTION( "an off switch does not change the mode" )
    {
        pcf::IndiProperty ip = switchRequest( "pupilfit", "setpt_mode", "user", pcf::IndiElement::Off );
        REQUIRE( app.newCallBack_m_indiP_refmode( ip ) == 0 );
        REQUIRE( app.m_setPointSource == USEDEFSET );
        REQUIRE( app.restart() == false );
    }

    SECTION( "user mode" )
    {
        pcf::IndiProperty ip = switchRequest( "pupilfit", "setpt_mode", "user", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_refmode( ip ) == 0 );
        REQUIRE( app.m_setPointSource == USEUSERSET );
        REQUIRE( app.restart() == true );
    }

    SECTION( "reference image mode" )
    {
        pcf::IndiProperty ip = switchRequest( "pupilfit", "setpt_mode", "refim", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_refmode( ip ) == 0 );
        REQUIRE( app.m_setPointSource == USEREFIM );
        REQUIRE( app.restart() == true );
    }

    SECTION( "back to default mode" )
    {
        app.m_setPointSource = USEUSERSET;

        pcf::IndiProperty ip = switchRequest( "pupilfit", "setpt_mode", "default", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_refmode( ip ) == 0 );
        REQUIRE( app.m_setPointSource == USEDEFSET );
        REQUIRE( app.restart() == true );
    }
}

} // namespace pupilFitTest

} // namespace libXWCTest
