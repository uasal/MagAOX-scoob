/** \file irisaoCtrl_test.cpp
 * \brief Catch2 tests for the irisaoCtrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup irisaoCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "../irisaoCtrl.hpp"

// Included after the app header so the dev::dm callback bodies stay live.  These callbacks check the
// property key directly and return 0 for a property with no elements.
#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// One captured SetMirrorPosition() call.
struct irisaoSetCall
{
    SegmentNumber m_segment; ///< The segment number.
    float         m_z;       ///< The piston.
    float         m_xgrad;   ///< The x gradient.
    float         m_ygrad;   ///< The y gradient.
};

/// Fake state for the IrisAO SDK stubs declared in `stubs/irisao.mirrors.h`.
struct irisaoStubState
{
    irisaoMirrorStub m_mirror{ 1 }; ///< The fake mirror returned by MirrorConnect().

    bool        m_connectThrows{ false };    ///< If true MirrorConnect() throws.
    int         m_connectCalls{ 0 };         ///< Number of MirrorConnect() calls.
    std::string m_connectMirror;             ///< Mirror serial number passed to the last MirrorConnect().
    std::string m_connectDriver;             ///< Driver serial number passed to the last MirrorConnect().
    bool        m_connectDisableHW{ false }; ///< Hardware-disable flag passed to the last MirrorConnect().

    SegmentNumber m_nSegments{ 0 }; ///< Number of valid segments reported by MirrorIterate().

    std::vector<irisaoSetCall> m_setCalls; ///< Captured SetMirrorPosition() calls.

    std::set<SegmentNumber> m_unreachable;   ///< Segments reported as not reachable by GetMirrorPosition().
    int                     m_getCalls{ 0 }; ///< Number of GetMirrorPosition() calls.

    int m_sendSettingsCalls{ 0 }; ///< Number of MirrorCommand( MirrorSendSettings ) calls.
    int m_releaseCalls{ 0 };      ///< Number of MirrorRelease() calls.
};

/// The global IrisAO stub state.
irisaoStubState g_iris;

/// Reset the IrisAO stub state to defaults.
void resetIrisaoStub()
{
    g_iris = irisaoStubState();
}

MirrorHandle MirrorConnect( const char *mirrorSerial, const char *driverSerial, bool disableHW )
{
    ++g_iris.m_connectCalls;
    g_iris.m_connectMirror    = mirrorSerial;
    g_iris.m_connectDriver    = driverSerial;
    g_iris.m_connectDisableHW = disableHW;

    if( g_iris.m_connectThrows )
    {
        throw std::runtime_error( "stub connection failure" );
    }

    return &g_iris.m_mirror;
}

bool MirrorIterate( MirrorHandle mirror, SegmentNumber segment )
{
    static_cast<void>( mirror );
    return segment < g_iris.m_nSegments;
}

void SetMirrorPosition( MirrorHandle mirror, SegmentNumber segment, float z, float xgrad, float ygrad )
{
    static_cast<void>( mirror );
    g_iris.m_setCalls.push_back( { segment, z, xgrad, ygrad } );
}

void GetMirrorPosition( MirrorHandle mirror, SegmentNumber segment, MirrorPosition *position )
{
    static_cast<void>( mirror );
    ++g_iris.m_getCalls;
    position->z         = 0;
    position->xgrad     = 0;
    position->ygrad     = 0;
    position->reachable = ( g_iris.m_unreachable.count( segment ) == 0 );
}

void MirrorCommand( MirrorHandle mirror, MirrorCommandType command )
{
    static_cast<void>( mirror );
    if( command == MirrorSendSettings )
    {
        ++g_iris.m_sendSettingsCalls;
    }
}

void MirrorRelease( MirrorHandle mirror )
{
    static_cast<void>( mirror );
    ++g_iris.m_releaseCalls;
}

/// \endcond

namespace libXWCTest
{

/** \defgroup irisaoCtrl_unit_test irisaoCtrl Unit Tests
 * \brief Unit tests for the irisaoCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `irisaoCtrl` unit tests.
/** \ingroup irisaoCtrl_unit_test
 */
namespace irisaoCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Scratch calibration directory used by these tests.
const std::string g_tmpDir = "/tmp/irisaoCtrl_test";

/// Test harness exposing irisaoCtrl internals.
class irisaoCtrl_test : public irisaoCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit irisaoCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName      = device;
        m_calibDir        = g_tmpDir + "/calib";
        m_hardwareDisable = false; // not initialized by the app unless configured

        createStandardIndiRequestSw( m_indiP_init, "initDM" );
        createStandardIndiRequestSw( m_indiP_zero, "zeroDM" );
        createStandardIndiRequestSw( m_indiP_release, "releaseDM" );
        createStandardIndiRequestSw( m_indiP_zeroAll, "zeroAll" );
    }

    using irisaoCtrl::m_calibDir;
    using irisaoCtrl::m_calibRelDir;
    using irisaoCtrl::m_dm;
    using irisaoCtrl::m_dminputs;
    using irisaoCtrl::m_dmopen;
    using irisaoCtrl::m_dserialNumber;
    using irisaoCtrl::m_hardwareDisable;
    using irisaoCtrl::m_instSatMap;
    using irisaoCtrl::m_mserialNumber;
    using irisaoCtrl::m_nbAct;
    using irisaoCtrl::m_powerMgtEnabled;
    using irisaoCtrl::m_shutdown;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Put the app in the state of a connected mirror without calling initDM().
    /** Also sets m_shutdown so that releaseDM() does not signal the (never started) shmim monitor thread.
     */
    void fakeInit( SegmentNumber nSegments /**< [in] the number of mirror segments */ )
    {
        g_iris.m_nSegments = nSegments;

        m_dm     = &g_iris.m_mirror;
        m_dmopen = true;
        m_nbAct  = 3 * nSegments;

        m_shutdown = 1;
    }
};

/// \endcond

/// Verify irisaoCtrl construction and configuration defaults.
/**
 * \ingroup irisaoCtrl_unit_test
 */
TEST_CASE( "irisaoCtrl configuration defaults", "[irisaoCtrl]" )
{
    // clang-format off
    #ifdef IRISAOCTRL_TEST_DOXYGEN_REF
    irisaoCtrl::irisaoCtrl();
    irisaoCtrl::setupConfig();
    irisaoCtrl::loadConfig();
    irisaoCtrl::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    resetIrisaoStub();

    mx::app::writeConfigFile( "/tmp/irisaoCtrl_test_defaults.conf", { "none" }, { "nada" }, { "0" } );

    irisaoCtrl_test app( "irisaoCtrl" );

    REQUIRE( app.m_powerMgtEnabled == true );
    REQUIRE( app.m_dmopen == false );
    REQUIRE( app.m_nbAct == 0 );

    app.configure( "/tmp/irisaoCtrl_test_defaults.conf" );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_mserialNumber == "" );
    REQUIRE( app.m_dserialNumber == "" );
    REQUIRE( app.m_hardwareDisable == false ); // as set by the harness
    REQUIRE( app.m_calibRelDir == "" );
    REQUIRE( app.calibPath() == g_tmpDir + "/calib/" );
    REQUIRE( app.shmimName() == "" );

    REQUIRE( g_iris.m_connectCalls == 0 );

    std::remove( "/tmp/irisaoCtrl_test_defaults.conf" );
}

/// Verify irisaoCtrl configuration overrides.
/**
 * \ingroup irisaoCtrl_unit_test
 */
TEST_CASE( "irisaoCtrl configuration overrides", "[irisaoCtrl]" )
{
    // clang-format off
    #ifdef IRISAOCTRL_TEST_DOXYGEN_REF
    irisaoCtrl::setupConfig();
    irisaoCtrl::loadConfig();
    irisaoCtrl::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    resetIrisaoStub();

    mx::app::writeConfigFile( "/tmp/irisaoCtrl_test_over.conf",
                              { "dm", "dm", "dm", "dm", "dm" },
                              { "mserialNumber", "dserialNumber", "hardwareDisable", "calibRelDir", "shmimName" },
                              { "PWA37-05-04-0404", "09150004", "true", "dm/irisao", "dm02disp" } );

    irisaoCtrl_test app( "irisaoCtrl" );
    app.configure( "/tmp/irisaoCtrl_test_over.conf" );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_mserialNumber == "PWA37-05-04-0404" );
    REQUIRE( app.m_dserialNumber == "09150004" );
    REQUIRE( app.m_hardwareDisable == true );
    REQUIRE( app.m_calibRelDir == "dm/irisao" );
    REQUIRE( app.calibPath() == g_tmpDir + "/calib/dm/irisao" );
    REQUIRE( app.flatPath() == g_tmpDir + "/calib/dm/irisao/flats" );
    REQUIRE( app.shmimName() == "dm02disp" );
    REQUIRE( app.shmimFlat() == "dm02disp00" );
    REQUIRE( app.shmimTest() == "dm02disp02" );

    std::remove( "/tmp/irisaoCtrl_test_over.conf" );
}

/// Verify initDM() connects to the mirror, counts segments, and zeroes the mirror.
/**
 * \ingroup irisaoCtrl_unit_test
 */
TEST_CASE( "irisaoCtrl initDM", "[irisaoCtrl]" )
{
    // clang-format off
    #ifdef IRISAOCTRL_TEST_DOXYGEN_REF
    irisaoCtrl::initDM();
    #endif
    // clang-format on

    resetIrisaoStub();

    irisaoCtrl_test app( "irisaoCtrl" );
    app.m_mserialNumber   = "pwa37-05";
    app.m_dserialNumber   = "09150004a";
    app.m_hardwareDisable = true;
    app.m_shutdown        = 1;

    SECTION( "success" )
    {
        g_iris.m_nSegments = 37;

        REQUIRE( app.initDM() == 0 );

        // Serial numbers are upper-cased, and the hardware-disable flag is passed through
        REQUIRE( g_iris.m_connectCalls == 1 );
        REQUIRE( g_iris.m_connectMirror == "PWA37-05" );
        REQUIRE( g_iris.m_connectDriver == "09150004A" );
        REQUIRE( g_iris.m_connectDisableHW == true );

        REQUIRE( app.m_dmopen == true );
        REQUIRE( app.m_dm == &g_iris.m_mirror );

        // Three actuators (piston, tip, tilt) per segment
        REQUIRE( app.m_nbAct == 111 );
        REQUIRE( app.m_dminputs != nullptr );

        // Every segment is zeroed and the settings are sent once
        REQUIRE( g_iris.m_setCalls.size() == 37 );
        for( size_t n = 0; n < g_iris.m_setCalls.size(); ++n )
        {
            REQUIRE( g_iris.m_setCalls[n].m_segment == n );
            REQUIRE( g_iris.m_setCalls[n].m_z == 0 );
            REQUIRE( g_iris.m_setCalls[n].m_xgrad == 0 );
            REQUIRE( g_iris.m_setCalls[n].m_ygrad == 0 );
        }
        REQUIRE( g_iris.m_sendSettingsCalls == 1 );

        REQUIRE( app.state() == stateCodes::OPERATING );

        // A second initDM is refused without reconnecting
        REQUIRE( app.initDM() == -1 );
        REQUIRE( g_iris.m_connectCalls == 1 );
    }

    SECTION( "an exception from MirrorConnect is an error" )
    {
        g_iris.m_connectThrows = true;
        g_iris.m_nSegments     = 37;

        REQUIRE( app.initDM() == -1 );
        REQUIRE( app.m_dmopen == false );
        REQUIRE( g_iris.m_setCalls.size() == 0 );
    }

    app.m_dmopen = false;
}

/// Verify zeroDM().
/**
 * \ingroup irisaoCtrl_unit_test
 */
TEST_CASE( "irisaoCtrl zeroDM", "[irisaoCtrl]" )
{
    // clang-format off
    #ifdef IRISAOCTRL_TEST_DOXYGEN_REF
    irisaoCtrl::zeroDM();
    #endif
    // clang-format on

    resetIrisaoStub();

    irisaoCtrl_test app( "irisaoCtrl" );

    SECTION( "not open" )
    {
        REQUIRE( app.zeroDM() == -1 );
        REQUIRE( g_iris.m_sendSettingsCalls == 0 );
    }

    SECTION( "no actuators" )
    {
        app.fakeInit( 0 );
        REQUIRE( app.zeroDM() == -1 );
        REQUIRE( g_iris.m_sendSettingsCalls == 0 );
    }

    SECTION( "success zeroes every segment" )
    {
        app.fakeInit( 5 );

        REQUIRE( app.zeroDM() == 0 );
        REQUIRE( g_iris.m_setCalls.size() == 5 );
        for( size_t n = 0; n < 5; ++n )
        {
            REQUIRE( g_iris.m_setCalls[n].m_segment == n );
            REQUIRE( g_iris.m_setCalls[n].m_z == 0 );
            REQUIRE( g_iris.m_setCalls[n].m_xgrad == 0 );
            REQUIRE( g_iris.m_setCalls[n].m_ygrad == 0 );
        }
        REQUIRE( g_iris.m_sendSettingsCalls == 1 );
    }

    app.m_dmopen = false;
}

/// Verify commandDM() sends piston/tip/tilt per segment and updates the saturation map.
/**
 * \ingroup irisaoCtrl_unit_test
 */
TEST_CASE( "irisaoCtrl commandDM", "[irisaoCtrl]" )
{
    // clang-format off
    #ifdef IRISAOCTRL_TEST_DOXYGEN_REF
    irisaoCtrl::commandDM(void *);
    #endif
    // clang-format on

    resetIrisaoStub();

    irisaoCtrl_test app( "irisaoCtrl" );
    app.fakeInit( 3 );

    // 3 segments x (z, xgrad, ygrad)
    app.m_instSatMap.resize( 3, 3 );
    app.m_instSatMap.setConstant( 7 );

    float src[9] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f };

    SECTION( "all segments reachable" )
    {
        REQUIRE( app.commandDM( src ) == 0 );

        REQUIRE( g_iris.m_setCalls.size() == 3 );
        for( size_t n = 0; n < 3; ++n )
        {
            REQUIRE( g_iris.m_setCalls[n].m_segment == n );
            REQUIRE( g_iris.m_setCalls[n].m_z == src[3 * n] );
            REQUIRE( g_iris.m_setCalls[n].m_xgrad == src[3 * n + 1] );
            REQUIRE( g_iris.m_setCalls[n].m_ygrad == src[3 * n + 2] );
        }

        REQUIRE( g_iris.m_getCalls == 3 );
        REQUIRE( g_iris.m_sendSettingsCalls == 1 );

        for( int n = 0; n < 9; ++n )
        {
            REQUIRE( app.m_instSatMap.data()[n] == 0 );
        }
    }

    SECTION( "an unreachable segment flags all three of its actuators" )
    {
        g_iris.m_unreachable.insert( 1 );

        REQUIRE( app.commandDM( src ) == 0 );

        REQUIRE( app.m_instSatMap.data()[0] == 0 );
        REQUIRE( app.m_instSatMap.data()[1] == 0 );
        REQUIRE( app.m_instSatMap.data()[2] == 0 );
        REQUIRE( app.m_instSatMap.data()[3] == 1 );
        REQUIRE( app.m_instSatMap.data()[4] == 1 );
        REQUIRE( app.m_instSatMap.data()[5] == 1 );
        REQUIRE( app.m_instSatMap.data()[6] == 0 );
        REQUIRE( app.m_instSatMap.data()[7] == 0 );
        REQUIRE( app.m_instSatMap.data()[8] == 0 );
    }

    app.m_dmopen = false;
}

/// Verify releaseDM() zeroes and releases the mirror.
/**
 * \ingroup irisaoCtrl_unit_test
 */
TEST_CASE( "irisaoCtrl releaseDM", "[irisaoCtrl]" )
{
    // clang-format off
    #ifdef IRISAOCTRL_TEST_DOXYGEN_REF
    irisaoCtrl::releaseDM();
    #endif
    // clang-format on

    resetIrisaoStub();

    irisaoCtrl_test app( "irisaoCtrl" );

    SECTION( "not open is an error" )
    {
        REQUIRE( app.releaseDM() == -1 );
        REQUIRE( g_iris.m_releaseCalls == 0 );
    }

    SECTION( "success" )
    {
        app.fakeInit( 4 );
        app.state( stateCodes::OPERATING );

        REQUIRE( app.releaseDM() == 0 );
        REQUIRE( g_iris.m_setCalls.size() == 4 );
        REQUIRE( g_iris.m_sendSettingsCalls == 1 );
        REQUIRE( g_iris.m_releaseCalls == 1 );
        REQUIRE( app.m_dmopen == false );
        REQUIRE( app.m_dm == nullptr );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "zeroing failure aborts the release" )
    {
        app.fakeInit( 0 );

        REQUIRE( app.releaseDM() == -1 );
        REQUIRE( g_iris.m_releaseCalls == 0 );
        REQUIRE( app.m_dmopen == true );
    }

    app.m_dmopen = false;
}

/// Verify appShutdown() and onPowerOff() release a connected mirror.
/**
 * \ingroup irisaoCtrl_unit_test
 */
TEST_CASE( "irisaoCtrl appShutdown and onPowerOff", "[irisaoCtrl]" )
{
    // clang-format off
    #ifdef IRISAOCTRL_TEST_DOXYGEN_REF
    irisaoCtrl::appShutdown();
    irisaoCtrl::onPowerOff();
    irisaoCtrl::whilePowerOff();
    #endif
    // clang-format on

    resetIrisaoStub();

    irisaoCtrl_test app( "irisaoCtrl" );

    SECTION( "appShutdown with no mirror" )
    {
        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_iris.m_releaseCalls == 0 );
    }

    SECTION( "appShutdown releases the mirror" )
    {
        app.fakeInit( 2 );

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_iris.m_releaseCalls == 1 );
        REQUIRE( app.m_dmopen == false );
    }

    SECTION( "onPowerOff releases the mirror" )
    {
        app.fakeInit( 2 );
        app.state( stateCodes::POWEROFF );

        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_iris.m_releaseCalls == 1 );
        REQUIRE( app.m_dmopen == false );
    }

    SECTION( "whilePowerOff with no flat or test directories" )
    {
        REQUIRE( app.whilePowerOff() == 0 );
    }

    app.m_dmopen = false;
}

/// Verify the dev::dm INDI callbacks validate the property device and name.
/**
 * \ingroup irisaoCtrl_unit_test
 */
TEST_CASE( "irisaoCtrl INDI callback validation", "[irisaoCtrl]" )
{
    // clang-format off
    #ifdef IRISAOCTRL_TEST_DOXYGEN_REF
    irisaoCtrl::newCallBack_init(const pcf::IndiProperty &);
    irisaoCtrl::newCallBack_zero(const pcf::IndiProperty &);
    irisaoCtrl::newCallBack_release(const pcf::IndiProperty &);
    irisaoCtrl::newCallBack_zeroAll(const pcf::IndiProperty &);
    #endif
    // clang-format on

    resetIrisaoStub();

    XWCTEST_INDI_ARBNEW_CALLBACK( irisaoCtrl, newCallBack_init, initDM );
    XWCTEST_INDI_ARBNEW_CALLBACK( irisaoCtrl, newCallBack_zero, zeroDM );
    XWCTEST_INDI_ARBNEW_CALLBACK( irisaoCtrl, newCallBack_release, releaseDM );
    XWCTEST_INDI_ARBNEW_CALLBACK( irisaoCtrl, newCallBack_zeroAll, zeroAll );

    REQUIRE( g_iris.m_connectCalls == 0 );
    REQUIRE( g_iris.m_sendSettingsCalls == 0 );
}

/// Verify the dev::dm INDI callbacks drive the irisaoCtrl DM interface.
/**
 * \ingroup irisaoCtrl_unit_test
 */
TEST_CASE( "irisaoCtrl INDI requests", "[irisaoCtrl]" )
{
    // clang-format off
    #ifdef IRISAOCTRL_TEST_DOXYGEN_REF
    irisaoCtrl::newCallBack_init(const pcf::IndiProperty &);
    irisaoCtrl::newCallBack_zero(const pcf::IndiProperty &);
    irisaoCtrl::newCallBack_release(const pcf::IndiProperty &);
    irisaoCtrl::zeroDM();
    irisaoCtrl::releaseDM();
    #endif
    // clang-format on

    resetIrisaoStub();

    irisaoCtrl_test app( "irisaoCtrl" );

    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( "irisaoCtrl" );

    SECTION( "init request in the wrong state is rejected before connecting" )
    {
        ip.setName( "initDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        app.state( stateCodes::OPERATING );
        REQUIRE( app.newCallBack_init( ip ) == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_iris.m_connectCalls == 0 );
    }

    SECTION( "zero request zeroes a connected mirror" )
    {
        ip.setName( "zeroDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        app.fakeInit( 3 );
        REQUIRE( app.newCallBack_zero( ip ) == 0 );
        REQUIRE( g_iris.m_setCalls.size() == 3 );
        REQUIRE( g_iris.m_sendSettingsCalls == 1 );
    }

    SECTION( "zero request on a disconnected mirror fails" )
    {
        ip.setName( "zeroDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        REQUIRE( app.newCallBack_zero( ip ) == -1 );
        REQUIRE( g_iris.m_sendSettingsCalls == 0 );
    }

    SECTION( "release request releases a connected mirror" )
    {
        ip.setName( "releaseDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        app.fakeInit( 3 );
        app.state( stateCodes::OPERATING );
        REQUIRE( app.newCallBack_release( ip ) == 0 );
        REQUIRE( g_iris.m_releaseCalls == 1 );
        REQUIRE( app.m_dmopen == false );
    }

    app.m_dmopen = false;
}

} // namespace irisaoCtrlTest

} // namespace libXWCTest
