/** \file refRMS_test.cpp
 * \brief Catch2 tests for the refRMS app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup refRMS_files
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../refRMS.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup refRMS_unit_test refRMS Unit Tests
 * \brief Unit tests for the refRMS application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `refRMS` unit tests.
/** \ingroup refRMS_unit_test
 */
namespace refRMSTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing refRMS internals.
class refRMS_test : public refRMS
{
  public:
    /// Construct a harness with the given device name.
    explicit refRMS_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
        m_indiP_fpsSource.setDevice( "camwfs" );
        m_indiP_fpsSource.setName( "fps" );
    }

    /// Register the app configuration options.
    void setupConfigForTest()
    {
        setupConfig();
    }

    /// Read a config file and load the configuration.
    void loadConfigFromFile( const std::string &fname /**< [in] config file path */ )
    {
        config.readConfig( fname );
        loadConfig();
    }

    /// Call loadConfigImpl() directly on the app configurator.
    int loadConfigImplForTest()
    {
        return loadConfigImpl( config );
    }

    /// Access the configured fps source device.
    const std::string &fpsSource() const
    {
        return m_fpsSource;
    }

    /// Access the reference shmim name.
    std::string refShmimName()
    {
        return refShmimMonitorT::m_shmimName;
    }

    /// Access the mask shmim name.
    std::string maskShmimName()
    {
        return maskShmimMonitorT::m_shmimName;
    }

    /// Access the reference getExistingFirst flag.
    bool refGetExistingFirst()
    {
        return refShmimMonitorT::m_getExistingFirst;
    }

    /// Access the mask getExistingFirst flag.
    bool maskGetExistingFirst()
    {
        return maskShmimMonitorT::m_getExistingFirst;
    }

    /// Set the reference stream dimensions as the shmimMonitor would.
    void setRefSize( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        refShmimMonitorT::m_width  = w;
        refShmimMonitorT::m_height = h;
    }

    /// Set the mask stream dimensions as the shmimMonitor would.
    void setMaskSize( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        maskShmimMonitorT::m_width  = w;
        maskShmimMonitorT::m_height = h;
    }

    /// Call the reference allocate().
    int allocateRef()
    {
        return allocate( refShmimT() );
    }

    /// Call the mask allocate().
    int allocateMask()
    {
        return allocate( maskShmimT() );
    }

    /// Call the reference processImage().
    int processRef( std::vector<float> &im /**< [in] image data, column-major */ )
    {
        return processImage( static_cast<void *>( im.data() ), refShmimT() );
    }

    /// Call the mask processImage().
    int processMask( std::vector<float> &im /**< [in] mask data, column-major */ )
    {
        return processImage( static_cast<void *>( im.data() ), maskShmimT() );
    }

    /// Set the current FPS.
    void setFps( float fps /**< [in] new fps */ )
    {
        m_fps = fps;
    }

    /// Get the current FPS.
    float fps() const
    {
        return m_fps;
    }

    /// Get the reference shmimMonitor restart flag.
    bool refRestart()
    {
        return refShmimMonitorT::m_restart;
    }

    /// Set the reference shmimMonitor restart flag.
    void setRefRestart( bool r /**< [in] new value */ )
    {
        refShmimMonitorT::m_restart = r;
    }

    /// Access the current reference image.
    mx::improc::eigenImage<realT> &currRef()
    {
        return m_currRef;
    }

    /// Access the current mask.
    mx::improc::eigenImage<realT> &mask()
    {
        return m_mask;
    }

    /// Whether the mask is flagged valid.
    bool maskValid() const
    {
        return m_maskValid;
    }

    /// The current mask sum.
    realT maskSum() const
    {
        return m_maskSum;
    }

    /// Access the rms circular buffer.
    mx::sigproc::circularBufferIndex<float, cbIndexT> &rmsBuff()
    {
        return m_rms;
    }

    /// Access the mean circular buffer.
    mx::sigproc::circularBufferIndex<float, cbIndexT> &meanBuff()
    {
        return m_mean;
    }

    /// Call the fps source SET callback.
    int fpsCallback( const pcf::IndiProperty &ip /**< [in] received property */ )
    {
        return setCallBack_m_indiP_fpsSource( ip );
    }
};
/// \endcond

/// Verify refRMS configuration defaults.
/**
 * \ingroup refRMS_unit_test
 */
TEST_CASE( "refRMS configuration defaults", "[refRMS]" )
{
    // clang-format off
    #ifdef REFRMS_TEST_DOXYGEN_REF
    refRMS::refRMS();
    refRMS::setupConfig();
    refRMS::loadConfig();
    refRMS::loadConfigImpl();
    #endif
    // clang-format on

    refRMS_test app( "refrms" );

    // constructor sets getExistingFirst on both monitors
    REQUIRE( app.refGetExistingFirst() == true );
    REQUIRE( app.maskGetExistingFirst() == true );

    app.setupConfigForTest();

    const std::string fname = "/tmp/refRMS_test_defaults.conf";
    mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );
    app.loadConfigFromFile( fname );

    REQUIRE( app.fpsSource() == "" );
    REQUIRE( app.refShmimName() == "refrms" );
    REQUIRE( app.maskShmimName() == "refrms" );
    REQUIRE( app.refGetExistingFirst() == true );
    REQUIRE( app.maskGetExistingFirst() == true );

    REQUIRE( app.loadConfigImplForTest() == 0 );

    std::remove( fname.c_str() );
}

/// Verify refRMS configuration overrides.
/**
 * \ingroup refRMS_unit_test
 */
TEST_CASE( "refRMS configuration overrides", "[refRMS]" )
{
    // clang-format off
    #ifdef REFRMS_TEST_DOXYGEN_REF
    refRMS::setupConfig();
    refRMS::loadConfig();
    #endif
    // clang-format on

    refRMS_test app( "refrms" );
    app.setupConfigForTest();

    const std::string fname = "/tmp/refRMS_test_overrides.conf";
    mx::app::writeConfigFile( fname,
                              { "rms", "refShmim", "refShmim", "maskShmim", "maskShmim" },
                              { "fpsSource", "shmimName", "getExistingFirst", "shmimName", "getExistingFirst" },
                              { "camwfs", "camwfs_ref", "false", "camwfs_mask", "false" } );
    app.loadConfigFromFile( fname );

    REQUIRE( app.fpsSource() == "camwfs" );
    REQUIRE( app.refShmimName() == "camwfs_ref" );
    REQUIRE( app.maskShmimName() == "camwfs_mask" );
    REQUIRE( app.refGetExistingFirst() == false );
    REQUIRE( app.maskGetExistingFirst() == false );

    std::remove( fname.c_str() );
}

/// Verify mask allocation and processing.
/**
 * \ingroup refRMS_unit_test
 */
TEST_CASE( "refRMS mask allocate and processImage", "[refRMS]" )
{
    // clang-format off
    #ifdef REFRMS_TEST_DOXYGEN_REF
    refRMS::allocate(const maskShmimT &);
    refRMS::processImage(void *, const maskShmimT &);
    #endif
    // clang-format on

    refRMS_test app( "refrms" );

    app.setMaskSize( 2, 3 );
    REQUIRE( app.allocateMask() == 0 );
    REQUIRE( app.mask().rows() == 2 );
    REQUIRE( app.mask().cols() == 3 );

    std::vector<float> mask = { 1, 1, 0, 1, 0.5, 1 };
    REQUIRE( app.processMask( mask ) == 0 );

    REQUIRE( app.maskSum() == Approx( 4.5 ) );
    REQUIRE( app.mask()( 0, 0 ) == Approx( 1 ) );
    REQUIRE( app.mask()( 0, 1 ) == Approx( 0 ) ); // column-major: element 2 is (0,1)
    REQUIRE( app.mask()( 0, 2 ) == Approx( 0.5 ) );
    REQUIRE( app.mask()( 1, 2 ) == Approx( 1 ) );

    // No reference allocated yet, so the mask is not valid
    REQUIRE( app.maskValid() == false );
}

/// Verify reference allocation resets a mismatched mask and sizes the buffers.
/**
 * \ingroup refRMS_unit_test
 */
TEST_CASE( "refRMS reference allocate", "[refRMS]" )
{
    // clang-format off
    #ifdef REFRMS_TEST_DOXYGEN_REF
    refRMS::allocate(const refShmimT &);
    #endif
    // clang-format on

    SECTION( "mismatched mask is reset to ones" )
    {
        refRMS_test app( "refrms" );
        app.setFps( 10 ); // allocate() waits forever while fps == 0
        app.setRefSize( 4, 5 );

        REQUIRE( app.allocateRef() == 0 );

        REQUIRE( app.currRef().rows() == 4 );
        REQUIRE( app.currRef().cols() == 5 );
        REQUIRE( app.mask().rows() == 4 );
        REQUIRE( app.mask().cols() == 5 );
        REQUIRE( app.mask().sum() == Approx( 20 ) );
        REQUIRE( app.maskValid() == false );

        REQUIRE( app.rmsBuff().maxEntries() == 110 );
        REQUIRE( app.meanBuff().maxEntries() == 110 );
        REQUIRE( app.rmsBuff().size() == 0 );
    }

    SECTION( "matching mask is preserved and becomes valid when reprocessed" )
    {
        refRMS_test app( "refrms" );
        app.setFps( 2 );

        app.setMaskSize( 2, 3 );
        REQUIRE( app.allocateMask() == 0 );
        std::vector<float> mask = { 1, 0, 1, 0, 1, 0 };
        REQUIRE( app.processMask( mask ) == 0 );
        REQUIRE( app.maskValid() == false );

        app.setRefSize( 2, 3 );
        REQUIRE( app.allocateRef() == 0 );
        REQUIRE( app.maskSum() == Approx( 3 ) );
        REQUIRE( app.mask()( 1, 0 ) == Approx( 0 ) );
        REQUIRE( app.rmsBuff().maxEntries() == 22 );

        REQUIRE( app.processMask( mask ) == 0 );
        REQUIRE( app.maskValid() == true );
    }
}

/// Verify the masked mean and RMS computed for each reference frame.
/**
 * \ingroup refRMS_unit_test
 */
TEST_CASE( "refRMS reference processImage computes masked mean and rms", "[refRMS]" )
{
    // clang-format off
    #ifdef REFRMS_TEST_DOXYGEN_REF
    refRMS::processImage(void *, const refShmimT &);
    #endif
    // clang-format on

    SECTION( "uniform mask" )
    {
        refRMS_test app( "refrms" );
        app.setFps( 1 );

        app.setMaskSize( 2, 3 );
        REQUIRE( app.allocateMask() == 0 );
        std::vector<float> mask( 6, 1.0f );
        REQUIRE( app.processMask( mask ) == 0 );

        app.setRefSize( 2, 3 );
        REQUIRE( app.allocateRef() == 0 );

        std::vector<float> ref = { 1, 2, 3, 4, 5, 6 };
        REQUIRE( app.processRef( ref ) == 0 );

        REQUIRE( app.rmsBuff().size() == 1 );
        REQUIRE( app.meanBuff().size() == 1 );
        REQUIRE( app.meanBuff()[0] == Approx( 3.5 ) );
        REQUIRE( app.rmsBuff()[0] == Approx( std::sqrt( 17.5 / 6.0 ) ) );
        REQUIRE( app.currRef()( 1, 2 ) == Approx( 6 ) );

        // A constant frame has zero rms
        std::vector<float> flat( 6, 7.0f );
        REQUIRE( app.processRef( flat ) == 0 );
        REQUIRE( app.rmsBuff().size() == 2 );
        REQUIRE( app.meanBuff()[1] == Approx( 7 ) );
        REQUIRE( app.rmsBuff()[1] == Approx( 0 ).margin( 1e-6 ) );
    }

    SECTION( "partial mask excludes pixels" )
    {
        refRMS_test app( "refrms" );
        app.setFps( 1 );

        app.setMaskSize( 2, 3 );
        REQUIRE( app.allocateMask() == 0 );
        std::vector<float> mask = { 1, 1, 1, 0, 0, 0 };
        REQUIRE( app.processMask( mask ) == 0 );

        app.setRefSize( 2, 3 );
        REQUIRE( app.allocateRef() == 0 );

        std::vector<float> ref = { 1, 2, 3, 100, 200, 300 };
        REQUIRE( app.processRef( ref ) == 0 );

        REQUIRE( app.meanBuff()[0] == Approx( 2 ) );
        REQUIRE( app.rmsBuff()[0] == Approx( std::sqrt( 2.0 / 3.0 ) ) );
    }

    SECTION( "circular buffer wraps at 11*fps entries" )
    {
        refRMS_test app( "refrms" );
        app.setFps( 1 );

        app.setMaskSize( 1, 2 );
        REQUIRE( app.allocateMask() == 0 );
        std::vector<float> mask( 2, 1.0f );
        REQUIRE( app.processMask( mask ) == 0 );

        app.setRefSize( 1, 2 );
        REQUIRE( app.allocateRef() == 0 );
        REQUIRE( app.rmsBuff().maxEntries() == 11 );

        for( int n = 0; n < 15; ++n )
        {
            std::vector<float> ref = { static_cast<float>( n ), static_cast<float>( n ) + 2.0f };
            REQUIRE( app.processRef( ref ) == 0 );
        }

        REQUIRE( app.rmsBuff().size() == 11 );
        // Oldest retained entry is n = 4, whose mean is 5
        REQUIRE( app.meanBuff()[0] == Approx( 5 ) );
        REQUIRE( app.rmsBuff()[0] == Approx( 1 ) );
    }
}

/// Verify a reference frame is skipped when the mask size does not match.
/**
 * \ingroup refRMS_unit_test
 */
TEST_CASE( "refRMS reference processImage skips on mask size mismatch", "[refRMS]" )
{
    // clang-format off
    #ifdef REFRMS_TEST_DOXYGEN_REF
    refRMS::processImage(void *, const refShmimT &);
    #endif
    // clang-format on

    refRMS_test app( "refrms" );
    app.setFps( 1 );

    app.setRefSize( 2, 3 );
    REQUIRE( app.allocateRef() == 0 );

    // Mask stream changes size after the reference was allocated
    app.setMaskSize( 3, 3 );
    REQUIRE( app.allocateMask() == 0 );
    std::vector<float> mask( 9, 1.0f );
    REQUIRE( app.processMask( mask ) == 0 );

    std::vector<float> ref = { 1, 2, 3, 4, 5, 6 };
    REQUIRE( app.processRef( ref ) == 0 );

    REQUIRE( app.rmsBuff().size() == 0 );
    REQUIRE( app.meanBuff().size() == 0 );
}

/// Verify the fps source SET callback.
/**
 * \ingroup refRMS_unit_test
 */
TEST_CASE( "refRMS fps source set callback", "[refRMS]" )
{
    // clang-format off
    #ifdef REFRMS_TEST_DOXYGEN_REF
    refRMS::setCallBack_m_indiP_fpsSource(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "wrong property name is rejected" )
    {
        refRMS_test app( "refrms" );

        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "camwfs" );
        ip.setName( "wrong" );
        ip.add( pcf::IndiElement( "current", 50.0 ) );

        REQUIRE( app.fpsCallback( ip ) == -1 );
        REQUIRE( app.fps() == 0 );
    }

    SECTION( "property without a current element is ignored" )
    {
        refRMS_test app( "refrms" );

        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "camwfs" );
        ip.setName( "fps" );
        ip.add( pcf::IndiElement( "target", 50.0 ) );

        REQUIRE( app.fpsCallback( ip ) == 0 );
        REQUIRE( app.fps() == 0 );
        REQUIRE( app.refRestart() == false );
    }

    SECTION( "a new fps value is stored and triggers a restart" )
    {
        refRMS_test app( "refrms" );

        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "camwfs" );
        ip.setName( "fps" );
        ip.add( pcf::IndiElement( "current", 250.0 ) );

        REQUIRE( app.fpsCallback( ip ) == 0 );
        REQUIRE( app.fps() == Approx( 250 ) );
        REQUIRE( app.refRestart() == true );

        // The same value again does not trigger a restart
        app.setRefRestart( false );
        REQUIRE( app.fpsCallback( ip ) == 0 );
        REQUIRE( app.fps() == Approx( 250 ) );
        REQUIRE( app.refRestart() == false );

        // A changed value does
        ip["current"] = 500.0;
        REQUIRE( app.fpsCallback( ip ) == 0 );
        REQUIRE( app.fps() == Approx( 500 ) );
        REQUIRE( app.refRestart() == true );
    }
}

} // namespace refRMSTest

} // namespace libXWCTest
