/** \file streamCircBuff_test.cpp
 * \brief Catch2 tests for the streamCircBuff app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup streamCircBuff_files
 */

#include "../../../tests/testXWC.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <semaphore.h>
#include <string>
#include <thread>
#include <vector>

#include "../streamCircBuff.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup streamCircBuff_unit_test streamCircBuff Unit Tests
 * \brief Unit tests for the streamCircBuff application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `streamCircBuff` unit tests.
/** \ingroup streamCircBuff_unit_test
 */
namespace streamCircBuffTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing streamCircBuff internals.
class streamCircBuff_test : public streamCircBuff
{
  public:
    /// Construct a harness with the given device name, initializing the handoff semaphore.
    explicit streamCircBuff_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        // normally done in appStartup()
        sem_init( &m_smSemaphore, 0, 0 );
    }

    /// Destroy the harness, releasing the semaphore.
    ~streamCircBuff_test() noexcept
    {
        sem_destroy( &m_smSemaphore );
    }

    /// Register the configuration options.
    void setupConfigForTest()
    {
        setupConfig();
    }

    /// Read a config file and run loadConfigImpl(), returning its result.
    int loadConfigFromFile( const std::string &fname /**< [in] config file path */ )
    {
        config.readConfig( fname );
        return loadConfigImpl( config );
    }

    /// Set the input stream geometry as the shmimMonitor would.
    void setInputStream( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */, uint8_t dt /**< [in] type */ )
    {
        shmimMonitorT::m_width    = w;
        shmimMonitorT::m_height   = h;
        shmimMonitorT::m_dataType = dt;
    }

    /// Call allocate().
    int allocateForTest()
    {
        return allocate( dev::shmimT() );
    }

    /// Call processImage().
    int processImageForTest( void *src /**< [in] source frame */ )
    {
        return processImage( src, dev::shmimT() );
    }

    /// Call configureAcquisition().
    int configureAcquisitionForTest()
    {
        return configureAcquisition();
    }

    /// Call startAcquisition().
    int startAcquisitionForTest()
    {
        return startAcquisition();
    }

    /// Call acquireAndCheckValid().
    int acquireAndCheckValidForTest()
    {
        return acquireAndCheckValid();
    }

    /// Call loadImageIntoStream().
    int loadImageIntoStreamForTest( void *dest /**< [out] destination float buffer */ )
    {
        return loadImageIntoStream( dest );
    }

    /// Call reconfig().
    int reconfigForTest()
    {
        return reconfig();
    }

    /// Call fps().
    float fpsForTest()
    {
        return fps();
    }

    /// Current source pointer stored by processImage().
    char *currSrc()
    {
        return m_currSrc;
    }

    /// Try to take the semaphore without blocking; returns 0 if a post was pending.
    int trySem()
    {
        return sem_trywait( &m_smSemaphore );
    }

    /// Whether the pixel getter has been set by allocate().
    bool pixgetSet()
    {
        return ( pixget != nullptr );
    }

    /// Read one pixel through the pixel getter.
    float pix( void *src /**< [in] source frame */, size_t n /**< [in] linear pixel index */ )
    {
        return pixget( src, n );
    }

    /// frameGrabber reconfig flag.
    bool &reconfigFlag()
    {
        return frameGrabberT::m_reconfig;
    }

    /// frameGrabber width.
    uint32_t fgWidth()
    {
        return frameGrabberT::m_width;
    }

    /// frameGrabber height.
    uint32_t fgHeight()
    {
        return frameGrabberT::m_height;
    }

    /// frameGrabber data type.
    uint8_t fgDataType()
    {
        return frameGrabberT::m_dataType;
    }

    /// frameGrabber output shmim name.
    std::string fgShmimName()
    {
        return frameGrabberT::m_shmimName;
    }

    /// frameGrabber circular buffer length.
    uint32_t fgCircBuffLength()
    {
        return frameGrabberT::m_circBuffLength;
    }

    /// frameGrabber latency buffer maximum time.
    float fgLatencyTime()
    {
        return frameGrabberT::m_latencyCircBuffMaxTime;
    }

    /// frameGrabber latency buffer maximum length.
    int32_t fgLatencySize()
    {
        return frameGrabberT::m_latencyCircBuffMaxLength;
    }

    /// frameGrabber thread priority.
    int fgThreadPrio()
    {
        return frameGrabberT::m_fgThreadPrio;
    }

    /// frameGrabber current image timestamp.
    timespec fgTimestamp()
    {
        return frameGrabberT::m_currImageTimestamp;
    }

    /// Set the frameGrabber current image timestamp.
    void setFgTimestamp( const timespec &ts /**< [in] new timestamp */ )
    {
        frameGrabberT::m_currImageTimestamp = ts;
    }

    /// shmimMonitor input shmim name.
    std::string smShmimName()
    {
        return shmimMonitorT::m_shmimName;
    }

    /// shmimMonitor thread priority.
    int smThreadPrio()
    {
        return shmimMonitorT::m_smThreadPrio;
    }

    /// shmimMonitor cpuset.
    std::string smCpuset()
    {
        return shmimMonitorT::m_smCpuset;
    }

    /// shmimMonitor get-existing-first flag.
    bool smGetExistingFirst()
    {
        return shmimMonitorT::m_getExistingFirst;
    }

    /// Telemeter maximum interval.
    double telMaxInterval()
    {
        return telemeterT::m_maxInterval;
    }

    /// Telemeter log name.
    std::string telLogName()
    {
        return telemeterT::m_tel.logName();
    }

    /// Telemeter log extension.
    std::string telLogExt()
    {
        return telemeterT::m_tel.logExt();
    }
};
/// \endcond

/// Verify default configuration values.
/**
 * \ingroup streamCircBuff_unit_test
 */
TEST_CASE( "streamCircBuff configuration defaults", "[streamCircBuff]" )
{
    // clang-format off
    #ifdef STREAMCIRCBUFF_TEST_DOXYGEN_REF
    streamCircBuff::streamCircBuff();
    streamCircBuff::setupConfig();
    streamCircBuff::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    streamCircBuff_test app( "camsci1-cb" );
    app.setupConfigForTest();

    mx::app::writeConfigFile( "/tmp/streamCircBuff_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    REQUIRE( app.loadConfigFromFile( "/tmp/streamCircBuff_test_defaults.conf" ) == 0 );

    CHECK( app.smShmimName() == "camsci1-cb" );
    CHECK( app.fgShmimName() == "camsci1-cb" );
    CHECK( app.smGetExistingFirst() == false );
    CHECK( app.fgCircBuffLength() == 1 );
    CHECK( app.fgLatencyTime() == Approx( 5.0 ) );
    CHECK( app.fgLatencySize() == 100000 );
    CHECK( app.telMaxInterval() == Approx( 10.0 ) );
    CHECK( app.telLogName() == "camsci1-cb" );
    CHECK( app.telLogExt() == "bintel" );

    std::remove( "/tmp/streamCircBuff_test_defaults.conf" );
}

/// Verify configuration overrides for the input stream, output circular buffer and telemetry.
/**
 * \ingroup streamCircBuff_unit_test
 */
TEST_CASE( "streamCircBuff configuration overrides", "[streamCircBuff]" )
{
    // clang-format off
    #ifdef STREAMCIRCBUFF_TEST_DOXYGEN_REF
    streamCircBuff::setupConfig();
    streamCircBuff::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    streamCircBuff_test app( "camsci1-cb" );
    app.setupConfigForTest();

    mx::app::writeConfigFile(
        "/tmp/streamCircBuff_test_override.conf",
        { "shmimMonitor",
          "shmimMonitor",
          "shmimMonitor",
          "shmimMonitor",
          "framegrabber",
          "framegrabber",
          "framegrabber",
          "framegrabber",
          "framegrabber",
          "telemeter" },
        { "shmimName",
          "threadPrio",
          "cpuset",
          "getExistingFirst",
          "shmimName",
          "threadPrio",
          "circBuffLength",
          "latencyTime",
          "latencySize",
          "maxInterval" },
        { "camsci1", "10", "cb_sm", "true", "camsci1_circbuff", "20", "500", "2.5", "2000", "3.5" } );

    REQUIRE( app.loadConfigFromFile( "/tmp/streamCircBuff_test_override.conf" ) == 0 );

    CHECK( app.smShmimName() == "camsci1" );
    CHECK( app.smThreadPrio() == 10 );
    CHECK( app.smCpuset() == "cb_sm" );
    CHECK( app.smGetExistingFirst() == true );
    CHECK( app.fgShmimName() == "camsci1_circbuff" );
    CHECK( app.fgThreadPrio() == 20 );
    CHECK( app.fgCircBuffLength() == 500 );
    CHECK( app.fgLatencyTime() == Approx( 2.5 ) );
    CHECK( app.fgLatencySize() == 2000 );
    CHECK( app.telMaxInterval() == Approx( 3.5 ) );

    std::remove( "/tmp/streamCircBuff_test_override.conf" );
}

/// Verify out-of-range framegrabber buffer settings are clamped at load time.
/**
 * \ingroup streamCircBuff_unit_test
 */
TEST_CASE( "streamCircBuff configuration clamps invalid buffer settings", "[streamCircBuff]" )
{
    // clang-format off
    #ifdef STREAMCIRCBUFF_TEST_DOXYGEN_REF
    streamCircBuff::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    streamCircBuff_test app( "camsci1-cb" );
    app.setupConfigForTest();

    mx::app::writeConfigFile( "/tmp/streamCircBuff_test_clamp.conf",
                              { "framegrabber", "framegrabber" },
                              { "circBuffLength", "latencyTime" },
                              { "0", "-1" } );

    REQUIRE( app.loadConfigFromFile( "/tmp/streamCircBuff_test_clamp.conf" ) == 0 );

    CHECK( app.fgCircBuffLength() == 1 );
    CHECK( app.fgLatencyTime() == Approx( 0.0 ) );

    std::remove( "/tmp/streamCircBuff_test_clamp.conf" );
}

/// Verify allocate() selects a pixel getter for the input type and requests a framegrabber reconfiguration.
/**
 * \ingroup streamCircBuff_unit_test
 */
TEST_CASE( "streamCircBuff allocate selects the pixel getter", "[streamCircBuff]" )
{
    // clang-format off
    #ifdef STREAMCIRCBUFF_TEST_DOXYGEN_REF
    streamCircBuff::allocate(const dev::shmimT &);
    #endif
    // clang-format on

    SECTION( "uint16 input" )
    {
        streamCircBuff_test app( "cb" );
        app.setInputStream( 2, 2, IMAGESTRUCT_UINT16 );
        app.reconfigFlag() = false;

        REQUIRE( app.allocateForTest() == 0 );
        REQUIRE( app.pixgetSet() );
        CHECK( app.reconfigFlag() == true );

        std::vector<uint16_t> im = { 0, 1, 65535, 1000 };
        CHECK( app.pix( im.data(), 2 ) == Approx( 65535.0 ) );
        CHECK( app.pix( im.data(), 3 ) == Approx( 1000.0 ) );
    }

    SECTION( "int16 input keeps its sign" )
    {
        streamCircBuff_test app( "cb" );
        app.setInputStream( 2, 1, IMAGESTRUCT_INT16 );

        REQUIRE( app.allocateForTest() == 0 );
        REQUIRE( app.pixgetSet() );

        std::vector<int16_t> im = { -123, 456 };
        CHECK( app.pix( im.data(), 0 ) == Approx( -123.0 ) );
        CHECK( app.pix( im.data(), 1 ) == Approx( 456.0 ) );
    }

    SECTION( "double input" )
    {
        streamCircBuff_test app( "cb" );
        app.setInputStream( 1, 2, IMAGESTRUCT_DOUBLE );

        REQUIRE( app.allocateForTest() == 0 );
        REQUIRE( app.pixgetSet() );

        std::vector<double> im = { 1.25, -2.5 };
        CHECK( app.pix( im.data(), 1 ) == Approx( -2.5 ) );
    }

    SECTION( "unsupported input type leaves no getter" )
    {
        streamCircBuff_test app( "cb" );
        app.setInputStream( 2, 2, IMAGESTRUCT_COMPLEX_FLOAT );

        REQUIRE( app.allocateForTest() == 0 );
        CHECK( app.pixgetSet() == false );
        CHECK( app.reconfigFlag() == true );
    }
}

/// Verify configureAcquisition() copies the input geometry to the float output stream.
/**
 * \ingroup streamCircBuff_unit_test
 */
TEST_CASE( "streamCircBuff configureAcquisition output geometry", "[streamCircBuff]" )
{
    // clang-format off
    #ifdef STREAMCIRCBUFF_TEST_DOXYGEN_REF
    streamCircBuff::configureAcquisition();
    streamCircBuff::startAcquisition();
    streamCircBuff::reconfig();
    streamCircBuff::fps();
    #endif
    // clang-format on

    SECTION( "input geometry is copied and output is float" )
    {
        streamCircBuff_test app( "cb" );
        app.setInputStream( 64, 32, IMAGESTRUCT_UINT16 );

        REQUIRE( app.configureAcquisitionForTest() == 0 );
        CHECK( app.fgWidth() == 64 );
        CHECK( app.fgHeight() == 32 );
        CHECK( app.fgDataType() == IMAGESTRUCT_FLOAT );

        CHECK( app.startAcquisitionForTest() == 0 );
        CHECK( app.reconfigForTest() == 0 );
        CHECK( app.fpsForTest() == Approx( 1.0 ) );
    }

    SECTION( "no input stream yet returns -1 (after a 1 s wait)" )
    {
        streamCircBuff_test app( "cb" );
        app.setInputStream( 0, 32, IMAGESTRUCT_UINT16 );

        REQUIRE( app.configureAcquisitionForTest() == -1 );
        CHECK( app.fgWidth() == 0 );
        CHECK( app.fgHeight() == 0 );
    }
}

/// Verify processImage() stores the source frame and posts the handoff semaphore.
/**
 * \ingroup streamCircBuff_unit_test
 */
TEST_CASE( "streamCircBuff processImage frame handoff", "[streamCircBuff]" )
{
    // clang-format off
    #ifdef STREAMCIRCBUFF_TEST_DOXYGEN_REF
    streamCircBuff::processImage(void *, const dev::shmimT &);
    #endif
    // clang-format on

    streamCircBuff_test app( "cb" );

    REQUIRE( app.currSrc() == nullptr );
    REQUIRE( app.trySem() != 0 ); // nothing posted yet

    std::vector<uint16_t> im = { 1, 2, 3, 4 };

    REQUIRE( app.processImageForTest( im.data() ) == 0 );
    CHECK( app.currSrc() == reinterpret_cast<char *>( im.data() ) );

    CHECK( app.trySem() == 0 ); // exactly one post
    CHECK( app.trySem() != 0 );
}

/// Verify acquireAndCheckValid() returns 0 with a timestamp for a posted frame and 1 on timeout.
/**
 * \ingroup streamCircBuff_unit_test
 */
TEST_CASE( "streamCircBuff acquireAndCheckValid", "[streamCircBuff]" )
{
    // clang-format off
    #ifdef STREAMCIRCBUFF_TEST_DOXYGEN_REF
    streamCircBuff::acquireAndCheckValid();
    streamCircBuff::processImage(void *, const dev::shmimT &);
    #endif
    // clang-format on

    SECTION( "posted frame is valid and timestamped" )
    {
        streamCircBuff_test   app( "cb" );
        std::vector<uint16_t> im = { 1, 2, 3, 4 };

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.acquireAndCheckValidForTest() == 0 );

        timespec ts = app.fgTimestamp();
        CHECK( ts.tv_sec > 0 );
    }

    SECTION( "frame posted from another thread wakes the consumer" )
    {
        streamCircBuff_test   app( "cb" );
        std::vector<uint16_t> im = { 1, 2, 3, 4 };

        std::thread producer(
            [&app, &im]()
            {
                std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
                app.processImageForTest( im.data() );
            } );

        int rv = app.acquireAndCheckValidForTest();
        producer.join();

        CHECK( rv == 0 );
        CHECK( app.currSrc() == reinterpret_cast<char *>( im.data() ) );
    }

    SECTION( "no frame times out with 1 (after a 1 s wait)" )
    {
        streamCircBuff_test app( "cb" );
        app.setFgTimestamp( { 0, 0 } );

        REQUIRE( app.acquireAndCheckValidForTest() == 1 );
        CHECK( app.fgTimestamp().tv_sec == 0 );
    }
}

/// Verify loadImageIntoStream() converts the source frame to float in pixel order.
/**
 * \ingroup streamCircBuff_unit_test
 */
TEST_CASE( "streamCircBuff loadImageIntoStream converts to float", "[streamCircBuff]" )
{
    // clang-format off
    #ifdef STREAMCIRCBUFF_TEST_DOXYGEN_REF
    streamCircBuff::loadImageIntoStream(void *);
    #endif
    // clang-format on

    SECTION( "no source frame returns -1" )
    {
        streamCircBuff_test app( "cb" );
        app.setInputStream( 2, 2, IMAGESTRUCT_UINT16 );
        REQUIRE( app.allocateForTest() == 0 );

        std::vector<float> dest( 4, -1.0f );
        REQUIRE( app.loadImageIntoStreamForTest( dest.data() ) == -1 );
        CHECK( dest[0] == -1.0f );
    }

    SECTION( "uint16 3x2 frame" )
    {
        streamCircBuff_test app( "cb" );
        app.setInputStream( 3, 2, IMAGESTRUCT_UINT16 );
        REQUIRE( app.allocateForTest() == 0 );

        std::vector<uint16_t> im = { 10, 20, 30, 40, 50, 65535 };
        REQUIRE( app.processImageForTest( im.data() ) == 0 );

        std::vector<float> dest( 6, 0.0f );
        REQUIRE( app.loadImageIntoStreamForTest( dest.data() ) == 0 );

        for( size_t n = 0; n < im.size(); ++n )
        {
            CHECK( dest[n] == Approx( static_cast<float>( im[n] ) ) );
        }
    }

    SECTION( "int32 frame with negative values" )
    {
        streamCircBuff_test app( "cb" );
        app.setInputStream( 2, 2, IMAGESTRUCT_INT32 );
        REQUIRE( app.allocateForTest() == 0 );

        std::vector<int32_t> im = { -5, 0, 7, -100000 };
        REQUIRE( app.processImageForTest( im.data() ) == 0 );

        std::vector<float> dest( 4, 0.0f );
        REQUIRE( app.loadImageIntoStreamForTest( dest.data() ) == 0 );
        CHECK( dest[0] == Approx( -5.0 ) );
        CHECK( dest[1] == Approx( 0.0 ) );
        CHECK( dest[2] == Approx( 7.0 ) );
        CHECK( dest[3] == Approx( -100000.0 ) );
    }

    SECTION( "float frame is copied exactly and only width*height pixels are written" )
    {
        streamCircBuff_test app( "cb" );
        app.setInputStream( 2, 2, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocateForTest() == 0 );

        std::vector<float> im = { 0.5f, -1.5f, 2.25f, 3.0f };
        REQUIRE( app.processImageForTest( im.data() ) == 0 );

        std::vector<float> dest( 5, 99.0f );
        REQUIRE( app.loadImageIntoStreamForTest( dest.data() ) == 0 );
        CHECK( dest[0] == 0.5f );
        CHECK( dest[1] == -1.5f );
        CHECK( dest[2] == 2.25f );
        CHECK( dest[3] == 3.0f );
        CHECK( dest[4] == 99.0f );
    }
}

/// Characterize the current single-pointer handoff when the producer gets ahead of the consumer.
/** This documents the limitation described in
 * `agents/plans/2026-03/streamCircBuff-concurrency-hardening.md`: only the newest source pointer is kept, while the
 * semaphore counts every frame, so the newest frame is copied once per posted frame and earlier frames are lost.
 * Update this test when the ordered frame queue is implemented.
 *
 * \ingroup streamCircBuff_unit_test
 */
TEST_CASE( "streamCircBuff producer burst keeps only the newest frame", "[streamCircBuff]" )
{
    // clang-format off
    #ifdef STREAMCIRCBUFF_TEST_DOXYGEN_REF
    streamCircBuff::processImage(void *, const dev::shmimT &);
    streamCircBuff::acquireAndCheckValid();
    streamCircBuff::loadImageIntoStream(void *);
    #endif
    // clang-format on

    streamCircBuff_test app( "cb" );
    app.setInputStream( 2, 1, IMAGESTRUCT_UINT16 );
    REQUIRE( app.allocateForTest() == 0 );

    std::vector<uint16_t> frameA = { 1, 2 };
    std::vector<uint16_t> frameB = { 3, 4 };

    REQUIRE( app.processImageForTest( frameA.data() ) == 0 );
    REQUIRE( app.processImageForTest( frameB.data() ) == 0 );

    std::vector<float> dest( 2, 0.0f );

    // first consume: newest frame
    REQUIRE( app.acquireAndCheckValidForTest() == 0 );
    REQUIRE( app.loadImageIntoStreamForTest( dest.data() ) == 0 );
    CHECK( dest[0] == Approx( 3.0 ) );
    CHECK( dest[1] == Approx( 4.0 ) );

    // second consume: the semaphore still has a count, and the same newest frame is copied again
    dest.assign( 2, 0.0f );
    REQUIRE( app.acquireAndCheckValidForTest() == 0 );
    REQUIRE( app.loadImageIntoStreamForTest( dest.data() ) == 0 );
    CHECK( dest[0] == Approx( 3.0 ) );
    CHECK( dest[1] == Approx( 4.0 ) );

    // the semaphore is now drained
    CHECK( app.trySem() != 0 );
}

} // namespace streamCircBuffTest

} // namespace libXWCTest
