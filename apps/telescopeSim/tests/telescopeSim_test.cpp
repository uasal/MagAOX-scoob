/** \file telescopeSim_test.cpp
 * \brief Catch2 tests for the telescopeSim app.
 * \author Adam Schilperoort
 *
 * \ingroup telescopeSim_files
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>

#include "../telescopeSim.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup telescopeSim_unit_test telescopeSim Unit Tests
 * \brief Unit tests for the telescopeSim application.
 *
 * Covers the mount model: that a commanded target is actually reached at the
 * configured rate rather than teleported to, that staging `goto_target` does not
 * slew until `goto` is submitted, that the state machine passes through settling
 * before reporting tracking, that jitter has the configured amplitude per focal
 * plane axis and does not accumulate into a random walk, that a relative offset
 * moves the boresight the way the shared field-angle convention says it should
 * (including offsets smaller than the arrival tolerance), that stopping tracking
 * switches to the idle drift rate, and that NaN from an INDI client cannot get
 * into the model even though MagAO-X builds with `-ffast-math`.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `telescopeSim` unit tests.
/** \ingroup telescopeSim_unit_test
 */
namespace telescopeSimTest
{

/// Expose the protected surface of telescopeSim for testing.
/** \cond
 */
class telescopeSimTester : public telescopeSim
{
  public:
    using telescopeSim::telSimIndiParam;

    /// Register the config targets, read a file, and load it.
    int testLoadConfigFile( const std::string &path )
    {
        setupConfig();
        config.readConfig( path );
        return loadConfigImpl( config );
    }

    /// Give the INDI properties the tests touch the names and elements appStartup gives them.
    void testSetupIndi()
    {
        auto number = [this]( pcf::IndiProperty &p, const std::string &name, double v )
        {
            p = pcf::IndiProperty( pcf::IndiProperty::Number );
            p.setName( name );
            p.add( pcf::IndiElement( "current" ) );
            p.add( pcf::IndiElement( "target" ) );
            writeNumberPair( p, v );
        };

        number( m_indiP_slewRate, "slew_rate", m_slewRate.load() );
        number( m_indiP_rollRate, "roll_rate", m_rollRate.load() );
        number( m_indiP_jitterX, "jitter_x", m_jitterX.load() );
        number( m_indiP_jitterY, "jitter_y", m_jitterY.load() );
        number( m_indiP_jitterRoll, "jitter_roll", m_jitterRoll.load() );
        number( m_indiP_settleTime, "settle_time", m_settleTime.load() );
        number( m_indiP_arriveTol, "arrive_tol", m_arriveTol.load() );
        number( m_indiP_jitterTau, "jitter_tau", m_jitterTau.load() );

        auto vec = []( pcf::IndiProperty &p, const std::string &name, const std::vector<std::string> &els )
        {
            p = pcf::IndiProperty( pcf::IndiProperty::Number );
            p.setName( name );
            for( const std::string &e : els )
            {
                p.add( pcf::IndiElement( e ) );
                p[e].set( 0.0 );
            }
        };

        vec( m_indiP_offset, "offset", { "x", "y", "roll" } );
        vec( m_indiP_trackDrift, "tracking_drift_rate", { "ra", "dec", "pa" } );
        vec( m_indiP_idleDrift, "idle_drift_rate", { "ra", "dec", "pa" } );

        m_indiP_stopTracking = pcf::IndiProperty( pcf::IndiProperty::Switch );
        m_indiP_stopTracking.setName( "stop_tracking" );
        m_indiP_stopTracking.add( pcf::IndiElement( "toggle" ) );
    }

    /// A NEW for a multi-element number, built as an INDI client would send it.
    /** Values are text, so "nan" arrives exactly as a MagAO-X GUI sends it.
     */
    static pcf::IndiProperty
    testNew( const pcf::IndiProperty &like, const std::vector<std::pair<std::string, std::string>> &els )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( like.getDevice() );
        ip.setName( like.getName() );

        for( const auto &e : els )
        {
            ip.add( pcf::IndiElement( e.first ) );
            ip[e.first].setValue( e.second );
        }

        return ip;
    }

    /// Send an `offset` NEW through the real callback.
    int testOffset( const std::string &x, const std::string &y, const std::string &roll )
    {
        return newCallBack_m_indiP_offset( testNew( m_indiP_offset, { { "x", x }, { "y", y }, { "roll", roll } } ) );
    }

    /// Send a `tracking_drift_rate` or `idle_drift_rate` NEW through the real callback.
    int testDriftNew( bool idle, const std::string &ra, const std::string &dec, const std::string &pa )
    {
        const std::vector<std::pair<std::string, std::string>> els = { { "ra", ra }, { "dec", dec }, { "pa", pa } };

        if( idle )
        {
            return newCallBack_m_indiP_idleDrift( testNew( m_indiP_idleDrift, els ) );
        }

        return newCallBack_m_indiP_trackDrift( testNew( m_indiP_trackDrift, els ) );
    }

    /// Send a `stop_tracking` NEW through the real callback.
    int testStopTrackingNew( bool on )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( m_indiP_stopTracking.getDevice() );
        ip.setName( m_indiP_stopTracking.getName() );
        ip.add( pcf::IndiElement( "toggle" ) );
        ip["toggle"].setSwitchState( on ? pcf::IndiElement::On : pcf::IndiElement::Off );

        return newCallBack_m_indiP_stopTracking( ip );
    }

    /// Apply one live sim number NEW the way the INDI callback does.
    int testSimNew( telSimIndiParam which, const std::string &current, const std::string &target )
    {
        pcf::IndiProperty &prop = simProp( which );
        return handleSimNumberNew( prop, testNew( prop, { { "current", current }, { "target", target } } ), which );
    }

    /// The local INDI property for a live sim number.
    pcf::IndiProperty &simProp( telSimIndiParam which )
    {
        switch( which )
        {
        case telSimIndiParam::slewRate:
            return m_indiP_slewRate;
        case telSimIndiParam::rollRate:
            return m_indiP_rollRate;
        case telSimIndiParam::jitterX:
            return m_indiP_jitterX;
        case telSimIndiParam::jitterY:
            return m_indiP_jitterY;
        case telSimIndiParam::jitterRoll:
            return m_indiP_jitterRoll;
        case telSimIndiParam::settleTime:
            return m_indiP_settleTime;
        case telSimIndiParam::arriveTol:
            return m_indiP_arriveTol;
        default:
            return m_indiP_jitterTau;
        }
    }

    /// Value currently held on an element of a local INDI property.
    static double testEl( pcf::IndiProperty &prop, const std::string &el )
    {
        return std::strtod( prop[el].getValue().c_str(), nullptr );
    }

    /// Local tracking_drift_rate / idle_drift_rate property.
    pcf::IndiProperty &driftProp( bool idle )
    {
        return idle ? m_indiP_idleDrift : m_indiP_trackDrift;
    }

    /// Local offset property.
    pcf::IndiProperty &offsetProp()
    {
        return m_indiP_offset;
    }

    /// Overlay finite coordinates onto the staged goto_target. Does not slew.
    int testStoreGoto( const double *ra, const double *dec, const double *pa )
    {
        return storeGotoCoordinates( ra, dec, pa );
    }

    /// Submit a slew to the staged goto_target, as the `goto` toggle would.
    void testSubmitGoto()
    {
        submitGoto();
    }

    /// Command a target and track it, as start_visit or the `goto` toggle would.
    void testCommand( double ra, double dec, double pa )
    {
        m_trackingStopped = false;
        commandTarget( ra, dec, pa, "test" );
    }

    /// Stop or resume tracking.
    void testSetTrackingStopped( bool stopped )
    {
        setTrackingStopped( stopped );
    }

    /// Whether tracking is stopped.
    bool trackingStopped()
    {
        return m_trackingStopped;
    }

    /// Advance the mount model once using wall-clock elapsed time.
    void testUpdate()
    {
        updateMount();
    }

    /// Advance the mount by this many seconds of simulated time.
    void testAdvance( double seconds )
    {
        advanceMount( seconds );
    }

    /// Pretend this many wall-clock seconds have passed for updateMount().
    /** Only the wall-clock baseline moves. Settle is measured in simulated time
     * and is advanced with testAdvance, not here.
     */
    void testRewind( double seconds )
    {
        m_lastUpdate -= seconds;
    }

    /// The mount state.
    telSimState st()
    {
        return m_state;
    }

    /// Force the mount state, to reach tracking without waiting out a settle.
    void forceState( telSimState s )
    {
        m_state = s;
    }

    /// The reported pointing.
    double reportRA()
    {
        return m_reportRA;
    }
    double reportDec()
    {
        return m_reportDec;
    }
    double reportPA()
    {
        return m_reportPA;
    }

    /// The commanded pointing, before jitter.
    double baseRA()
    {
        return m_baseRA;
    }
    double baseDec()
    {
        return m_baseDec;
    }
    double basePA()
    {
        return m_basePA;
    }

    /// The staged goto_target, not applied until submitGoto.
    double gotoRA()
    {
        return m_gotoRA;
    }
    double gotoDec()
    {
        return m_gotoDec;
    }
    double gotoPA()
    {
        return m_gotoPA;
    }

    /// The target.
    double targetRA()
    {
        return m_targetRA;
    }
    double targetDec()
    {
        return m_targetDec;
    }
    double targetPA()
    {
        return m_targetPA;
    }

    /// The instantaneous focal plane jitter [arcsec].
    double jx()
    {
        return m_jx;
    }
    double jy()
    {
        return m_jy;
    }

    /// Set the jitter amplitudes directly.
    void setJitter( double jx, double jy, double jr )
    {
        m_jitterX = jx;
        m_jitterY = jy;
        m_jitterRoll = jr;
    }

    /// Set the settle time directly.
    void setSettle( double s )
    {
        m_settleTime = s;
    }

    /// Overlay live mount-model parameters, as the INDI numbers do.
    void testApplySim( double slewRate,
                       double rollRate,
                       double jitterX,
                       double jitterY,
                       double jitterRoll,
                       double settleTime,
                       double arriveTol,
                       double jitterTau )
    {
        applySimParameters( slewRate, rollRate, jitterX, jitterY, jitterRoll, settleTime, arriveTol, jitterTau );
    }

    /// Overlay drift rates for one mode.
    void testApplyDrift( bool idle, double ra, double dec, double pa )
    {
        applyDriftRates( idle, ra, dec, pa );
    }

    /// Read a MagAO-X standard-number NEW the way the INDI callbacks do.
    bool testReadIndiNumberRequest( const pcf::IndiProperty &ip, double &out )
    {
        return readIndiNumberRequest( ip, out );
    }

    /// Infect the OU state and a live parameter the way a NaN jitter_tau once did.
    void testPoisonOuState()
    {
        m_jx = std::numeric_limits<double>::quiet_NaN();
        m_jy = std::numeric_limits<double>::quiet_NaN();
        m_jroll = std::numeric_limits<double>::quiet_NaN();
        m_jitterTau.store( std::numeric_limits<double>::quiet_NaN() );
        m_reportPA = std::numeric_limits<double>::quiet_NaN();
    }

    /// Overlay live pointing-buffer parameters, as the INDI write_hz / history_s properties do.
    int testApplyPointingBuffer( double writeHz, double historyS )
    {
        return applyPointingBufferParameters( writeHz, historyS );
    }

    /// Configured pointing write rate [Hz].
    double writeHz()
    {
        return m_writeHz.load();
    }

    /// Configured pointing history span [s].
    double historyS()
    {
        return m_historyS.load();
    }

    /// Circular-buffer depth implied by write_hz and history_s.
    uint32_t pointingDepth()
    {
        return m_pointingDepth;
    }

    double slewRate()
    {
        return m_slewRate.load();
    }

    double rollRate()
    {
        return m_rollRate.load();
    }

    double jitterX()
    {
        return m_jitterX.load();
    }

    double jitterY()
    {
        return m_jitterY.load();
    }

    double jitterRoll()
    {
        return m_jitterRoll.load();
    }

    double trackDriftRA()
    {
        return m_trackDriftRA.load();
    }

    double trackDriftDec()
    {
        return m_trackDriftDec.load();
    }

    double trackDriftPA()
    {
        return m_trackDriftPA.load();
    }

    double idleDriftRA()
    {
        return m_idleDriftRA.load();
    }

    double settleTime()
    {
        return m_settleTime.load();
    }

    double arriveTol()
    {
        return m_arriveTol.load();
    }

    double jitterTau()
    {
        return m_jitterTau.load();
    }

    /// Configured pointing stream name.
    const std::string &pointingShmim()
    {
        return m_pointingShmim;
    }
};
/** \endcond
 */

/// Write a minimal telescopeSim configuration.
/** \returns the path written
 *
 * \ingroup telescopeSim_unit_test
 */
inline std::string writeTelConfig( const std::string &path /**< [in] where to write */,
                                   const std::string &extra = std::string() /**< [in] extra sim keys */ )
{
    std::ofstream fout( path );

    fout << "[visit]\ndevice=visitsim\n";
    fout << "[sim]\nslew_rate=1.0\nroll_rate=1.0\njitter_x=0\njitter_y=0\njitter_roll=0\n";
    fout << "settle_time=0.0\narrive_tol=1.0\nparity=-1\n";
    fout << "ra=192.317\ndec=26.84316038\npa=0.0\nseed=1234\n";
    fout << extra;

    return path;
}

/// Verify configuration handling and the rejection of unusable rates.
/**
 * \ingroup telescopeSim_unit_test
 */
TEST_CASE( "telescopeSim reads configuration and rejects bad rates", "[telescopeSim]" )
{
    // clang-format off
    #ifdef TELESCOPESIM_TEST_DOXYGEN_REF
    telescopeSim();
    telescopeSim::setupConfig;
    telescopeSim::loadConfigImpl;
    telescopeSim::appStartup;
    telescopeSim::appLogic;
    telescopeSim::appShutdown;
    telescopeSim::applySimParameters;
    telescopeSim::applyPointingBufferParameters;
    telSimStateName;
    #endif
    // clang-format on

    SECTION( "default construction succeeds" )
    {
        telescopeSim app;

        REQUIRE( true );
    }

    SECTION( "startup pointing is applied and tracking starts stopped" )
    {
        telescopeSimTester app;
        const std::string path = writeTelConfig( "/tmp/telescopeSim_test_config.conf" );

        REQUIRE( app.testLoadConfigFile( path ) == 0 );
        REQUIRE( app.baseRA() == Approx( 192.317 ).epsilon( 1e-12 ) );
        REQUIRE( app.baseDec() == Approx( 26.84316038 ).epsilon( 1e-12 ) );
        REQUIRE( app.reportRA() == Approx( 192.317 ).epsilon( 1e-12 ) );
        REQUIRE( app.st() == telSimState::idle );
        REQUIRE( app.trackingStopped() );
        REQUIRE( app.writeHz() == Approx( 5000.0 ).epsilon( 1e-12 ) );

        std::remove( path.c_str() );
    }

    SECTION( "a non-positive slew or roll rate is rejected" )
    {
        // A zero rate would mean the mount never arrives, which is worse than
        // refusing to start.
        for( const char *bad : { "slew_rate=0\n", "slew_rate=-1\n", "roll_rate=0\n", "slew_rate=nan\n" } )
        {
            telescopeSimTester app;
            const std::string path = "/tmp/telescopeSim_test_badrate.conf";
            {
                std::ofstream fout( path );
                fout << "[sim]\n" << bad;
            }

            REQUIRE( app.testLoadConfigFile( path ) < 0 );

            std::remove( path.c_str() );
        }
    }

    SECTION( "legacy sim.jitter sets both axes and the per-axis keys override it" )
    {
        {
            telescopeSimTester app;
            const std::string path = "/tmp/telescopeSim_test_legacyjitter.conf";
            {
                std::ofstream fout( path );
                fout << "[sim]\njitter=0.3\ndrift_x=0.0\ndrift_y=0.0\n";
            }

            REQUIRE( app.testLoadConfigFile( path ) == 0 );
            REQUIRE( app.jitterX() == Approx( 0.3 ).epsilon( 1e-12 ) );
            REQUIRE( app.jitterY() == Approx( 0.3 ).epsilon( 1e-12 ) );

            std::remove( path.c_str() );
        }

        {
            telescopeSimTester app;
            const std::string path = "/tmp/telescopeSim_test_axisjitter.conf";
            {
                std::ofstream fout( path );
                fout << "[sim]\njitter=0.3\njitter_y=0.05\n";
            }

            REQUIRE( app.testLoadConfigFile( path ) == 0 );
            REQUIRE( app.jitterX() == Approx( 0.3 ).epsilon( 1e-12 ) );
            REQUIRE( app.jitterY() == Approx( 0.05 ).epsilon( 1e-12 ) );

            std::remove( path.c_str() );
        }
    }

    SECTION( "drift rates are read per axis for tracking and idle" )
    {
        telescopeSimTester app;
        const std::string path = writeTelConfig( "/tmp/telescopeSim_test_driftcfg.conf",
                                                 "tracking_drift_ra=0.1\ntracking_drift_dec=-0.2\n"
                                                 "tracking_drift_pa=0.001\nidle_drift_ra=15.041\n" );

        REQUIRE( app.testLoadConfigFile( path ) == 0 );
        REQUIRE( app.trackDriftRA() == Approx( 0.1 ).epsilon( 1e-12 ) );
        REQUIRE( app.trackDriftDec() == Approx( -0.2 ).epsilon( 1e-12 ) );
        REQUIRE( app.trackDriftPA() == Approx( 0.001 ).epsilon( 1e-12 ) );
        REQUIRE( app.idleDriftRA() == Approx( 15.041 ).epsilon( 1e-12 ) );

        std::remove( path.c_str() );
    }

    SECTION( "every state has a distinct, non-empty name" )
    {
        const telSimState all[] = { telSimState::idle, telSimState::slewing, telSimState::settling,
                                    telSimState::tracking };

        std::vector<std::string> names;

        for( telSimState s : all )
        {
            const std::string n = telSimStateName( s );
            REQUIRE_FALSE( n.empty() );

            for( const std::string &p : names )
            {
                REQUIRE( n != p );
            }

            names.push_back( n );
        }

        REQUIRE( telSimStateName( telSimState::slewing ) == "SLEWING" );
        REQUIRE( telSimStateName( telSimState::tracking ) == "TRACKING" );
    }

    SECTION( "the pointing shmim write rate is configurable" )
    {
        telescopeSimTester app;
        const std::string path =
            writeTelConfig( "/tmp/telescopeSim_test_pointing.conf",
                            "[pointing]\nshmim=telpointing\nwrite_hz=500\nhistory_s=2\n" );

        REQUIRE( app.testLoadConfigFile( path ) == 0 );
        REQUIRE( app.writeHz() == Approx( 500.0 ).epsilon( 1e-12 ) );
        REQUIRE( app.historyS() == Approx( 2.0 ).epsilon( 1e-12 ) );
        REQUIRE( app.pointingDepth() == 1000 );
        REQUIRE( app.pointingShmim() == "telpointing" );

        std::remove( path.c_str() );
    }
}

/// Verify the mount slews at a finite rate and settles before tracking.
/**
 * \ingroup telescopeSim_unit_test
 */
TEST_CASE( "telescopeSim slews at a finite rate then settles", "[telescopeSim]" )
{
    // clang-format off
    #ifdef TELESCOPESIM_TEST_DOXYGEN_REF
    telescopeSim::commandTarget;
    telescopeSim::updateMount;
    telescopeSim::advanceMount;
    #endif
    // clang-format on

    telescopeSimTester app;
    const std::string path = writeTelConfig( "/tmp/telescopeSim_test_slew.conf" );
    REQUIRE( app.testLoadConfigFile( path ) == 0 );

    SECTION( "a large slew takes several steps and does not overshoot" )
    {
        // 5 degrees away at 1 deg/s must take about 5 seconds of simulated time.
        app.testCommand( 192.317 + 5.0, 26.84316038, 0.0 );
        REQUIRE( app.st() == telSimState::slewing );

        int steps = 0;
        while( app.st() == telSimState::slewing && steps < 20 )
        {
            app.testAdvance( 1.0 );
            ++steps;
        }

        // It must not have arrived instantly, and must not have taken forever.
        REQUIRE( steps >= 4 );
        REQUIRE( steps <= 8 );

        // With settle_time 0 the next update promotes it to tracking.
        REQUIRE( ( app.st() == telSimState::settling || app.st() == telSimState::tracking ) );

        app.testAdvance( 1.0 );
        REQUIRE( app.st() == telSimState::tracking );

        // And it arrived at the target, not past it.
        REQUIRE( app.baseRA() == Approx( 192.317 + 5.0 ).margin( 1e-6 ) );
        REQUIRE( app.baseDec() == Approx( 26.84316038 ).margin( 1e-6 ) );
    }

    SECTION( "a settle time is waited out before tracking is reported" )
    {
        app.setSettle( 5.0 );
        app.testCommand( 192.317 + 0.1, 26.84316038, 0.0 );

        app.testAdvance( 1.0 );

        // Arrived, so settling; not yet tracking.
        REQUIRE( app.st() == telSimState::settling );

        app.testAdvance( 1.0 );
        REQUIRE( app.st() == telSimState::settling );

        // Past the settle time it promotes.
        app.testAdvance( 6.0 );
        REQUIRE( app.st() == telSimState::tracking );
    }

    SECTION( "roll slews at its own rate" )
    {
        app.testCommand( 192.317, 26.84316038, 3.0 );

        app.testAdvance( 1.0 );

        // One second at 1 deg/s covers one degree of the three.
        REQUIRE( app.basePA() == Approx( 1.0 ).margin( 1e-6 ) );
        REQUIRE( app.st() == telSimState::slewing );
    }

    SECTION( "a target within arrive_tol is snapped onto, not dropped" )
    {
        // 0.5 arcsec is inside the 1 arcsec tolerance. Declaring arrival without
        // moving is how guiding offsets used to vanish.
        const double dDec = 0.5 / 3600.0;
        app.testCommand( 192.317, 26.84316038 + dDec, 0.0 );
        app.testAdvance( 0.001 );

        REQUIRE( app.baseDec() == Approx( 26.84316038 + dDec ).margin( 1e-12 ) );
        REQUIRE( app.st() != telSimState::slewing );
    }

    SECTION( "a non-finite target is refused" )
    {
        app.testCommand( std::strtod( "nan", nullptr ), 26.84316038, 0.0 );

        REQUIRE( app.st() == telSimState::idle );
        REQUIRE( MagAOX::wcc::isFinite( app.targetRA() ) );
    }

    SECTION( "a huge elapsed time is ignored rather than integrated" )
    {
        // A clock step or a long stall must not teleport the mount.
        app.testCommand( 192.317 + 5.0, 26.84316038, 0.0 );
        app.testUpdate();

        const double before = app.baseRA();
        app.testRewind( 5000.0 );
        app.testUpdate();

        REQUIRE( app.baseRA() == Approx( before ).margin( 1e-12 ) );
    }

    std::remove( path.c_str() );
}

/// Verify goto_target only stages coordinates; the goto toggle submits the slew.
/**
 * \ingroup telescopeSim_unit_test
 */
TEST_CASE( "telescopeSim stages goto_target until goto is submitted", "[telescopeSim]" )
{
    // clang-format off
    #ifdef TELESCOPESIM_TEST_DOXYGEN_REF
    telescopeSim::storeGotoCoordinates;
    telescopeSim::submitGoto;
    telescopeSim::commandTarget;
    telescopeSim::advanceMount;
    #endif
    // clang-format on

    telescopeSimTester app;
    const std::string path = writeTelConfig( "/tmp/telescopeSim_test_goto.conf" );
    REQUIRE( app.testLoadConfigFile( path ) == 0 );

    SECTION( "staging goto_target does not slew until goto is submitted" )
    {
        const double ra1 = 192.317 + 2.0;
        REQUIRE( app.testStoreGoto( &ra1, nullptr, nullptr ) == 0 );
        REQUIRE( app.st() == telSimState::idle );
        REQUIRE( app.gotoRA() == Approx( ra1 ).margin( 1e-12 ) );
        REQUIRE( app.gotoDec() == Approx( 26.84316038 ).margin( 1e-12 ) );
        REQUIRE( app.targetRA() == Approx( 192.317 ).margin( 1e-12 ) );
        REQUIRE( app.baseRA() == Approx( 192.317 ).margin( 1e-12 ) );

        // goto means "go and track that", so it clears stop_tracking.
        app.testSubmitGoto();
        REQUIRE_FALSE( app.trackingStopped() );
        REQUIRE( app.st() == telSimState::slewing );
        REQUIRE( app.targetRA() == Approx( ra1 ).margin( 1e-12 ) );
        REQUIRE( app.targetDec() == Approx( 26.84316038 ).margin( 1e-12 ) );

        int steps = 0;
        while( app.st() == telSimState::slewing && steps < 20 )
        {
            app.testAdvance( 1.0 );
            ++steps;
        }

        REQUIRE( steps >= 1 );
        app.testAdvance( 1.0 );
        REQUIRE( app.st() == telSimState::tracking );
        REQUIRE( app.baseRA() == Approx( ra1 ).margin( 1e-6 ) );
        REQUIRE( app.reportRA() == Approx( app.baseRA() ).margin( 1e-12 ) );
    }

    SECTION( "staging with no finite coordinates does not move the mount" )
    {
        REQUIRE( app.testStoreGoto( nullptr, nullptr, nullptr ) == 0 );
        REQUIRE( app.st() == telSimState::idle );
        REQUIRE( app.baseRA() == Approx( 192.317 ).margin( 1e-12 ) );
    }

    std::remove( path.c_str() );
}

/// Verify jitter has the configured amplitude per axis and does not accumulate.
/**
 * \ingroup telescopeSim_unit_test
 */
TEST_CASE( "telescopeSim jitter is bounded and does not random walk", "[telescopeSim]" )
{
    telescopeSimTester app;
    const std::string path = writeTelConfig( "/tmp/telescopeSim_test_jitter.conf" );
    REQUIRE( app.testLoadConfigFile( path ) == 0 );

    SECTION( "zero jitter reports the commanded pointing exactly" )
    {
        app.setJitter( 0.0, 0.0, 0.0 );
        app.forceState( telSimState::tracking );

        for( int i = 0; i < 20; ++i )
        {
            app.testAdvance( 1.0 );
            REQUIRE( app.reportRA() == Approx( app.baseRA() ).margin( 1e-12 ) );
            REQUIRE( app.reportDec() == Approx( app.baseDec() ).margin( 1e-12 ) );
        }
    }

    SECTION( "equal axis jitter has the configured rms and the base pointing stays put" )
    {
        const double jit = 2.0; // arcsec rms, large enough to measure quickly
        app.setJitter( jit, jit, 0.0 );
        app.forceState( telSimState::tracking );

        const double ra0 = app.baseRA();
        const double dec0 = app.baseDec();

        double s = 0, s2 = 0;
        const int n = 4000;

        for( int i = 0; i < n; ++i )
        {
            app.testAdvance( 1.0 );

            // Offset of the reported pointing from the commanded one, in arcsec.
            const double d = MagAOX::wcc::angularSeparation( app.baseRA(), app.baseDec(), app.reportRA(),
                                                             app.reportDec() ) *
                             3600.0;
            s += d;
            s2 += d * d;

            // The crucial property: jitter is a wander about where the mount is, not
            // a random walk accumulated into it, so the base must never move.
            REQUIRE( app.baseRA() == Approx( ra0 ).margin( 1e-12 ) );
            REQUIRE( app.baseDec() == Approx( dec0 ).margin( 1e-12 ) );
        }

        // For two independent Gaussian axes of sigma j, the radial offset has mean
        // j*sqrt(pi/2) and mean square 2j^2.
        const double meanR = s / n;
        REQUIRE( meanR == Approx( jit * std::sqrt( MagAOX::wcc::pi / 2.0 ) ).epsilon( 0.08 ) );
        REQUIRE( std::sqrt( s2 / n ) == Approx( jit * std::sqrt( 2.0 ) ).epsilon( 0.08 ) );
    }

    SECTION( "x and y jitter are independent focal plane axes" )
    {
        // With PA 0 and parity -1, focal plane Y is north (Dec) and X is minus RA.
        // The tangent-plane projection couples a pure X offset into Dec only at
        // second order (~1e-8 deg for a few arcsec), hence the 1e-7 deg margin.
        app.setJitter( 2.0, 0.0, 0.0 );
        app.forceState( telSimState::tracking );

        double sx2 = 0;
        const int n = 2000;

        for( int i = 0; i < n; ++i )
        {
            app.testAdvance( 1.0 );
            REQUIRE( app.jy() == 0.0 );
            REQUIRE( app.reportDec() == Approx( app.baseDec() ).margin( 1e-7 ) );
            sx2 += app.jx() * app.jx();
        }

        REQUIRE( std::sqrt( sx2 / n ) == Approx( 2.0 ).epsilon( 0.08 ) );

        app.setJitter( 0.0, 2.0, 0.0 );

        for( int i = 0; i < 200; ++i )
        {
            app.testAdvance( 1.0 );
            REQUIRE( app.jx() == 0.0 );
            REQUIRE( app.reportRA() == Approx( app.baseRA() ).margin( 1e-7 ) );
        }
    }

    SECTION( "roll jitter only rotates, it does not translate" )
    {
        app.setJitter( 0.0, 0.0, 0.1 );
        app.forceState( telSimState::tracking );

        bool moved = false;
        for( int i = 0; i < 50; ++i )
        {
            app.testAdvance( 1.0 );
            REQUIRE( app.reportRA() == Approx( app.baseRA() ).margin( 1e-12 ) );
            REQUIRE( app.reportDec() == Approx( app.baseDec() ).margin( 1e-12 ) );
            moved = moved || ( app.reportPA() != app.basePA() );
        }

        REQUIRE( moved );
    }

    SECTION( "no jitter is applied while idle" )
    {
        app.setJitter( 5.0, 5.0, 0.5 );
        app.forceState( telSimState::idle );

        app.testAdvance( 1.0 );

        REQUIRE( app.reportRA() == Approx( app.baseRA() ).margin( 1e-12 ) );
        REQUIRE( app.reportPA() == Approx( app.basePA() ).margin( 1e-12 ) );
    }

    std::remove( path.c_str() );
}

/// Verify a relative offset moves the boresight by the commanded field angle.
/**
 * \ingroup telescopeSim_unit_test
 */
TEST_CASE( "telescopeSim offsets follow the shared field angle convention", "[telescopeSim]" )
{
    // clang-format off
    #ifdef TELESCOPESIM_TEST_DOXYGEN_REF
    MagAOX::wcc::offsetBoresight;
    telescopeSim::newCallBack_m_indiP_offset;
    telescopeSim::commandTarget;
    telescopeSim::advanceMount;
    #endif
    // clang-format on

    SECTION( "an offset target is exactly where offsetBoresight says it is" )
    {
        // telescopeSim and wccSim must agree on this, which is why the conversion is
        // shared rather than duplicated.
        const double ra = 192.317, dec = 26.84316038, pa = 0.0, parity = -1.0;
        const double dx = 30.0, dy = -12.0;

        double wantRA = 0, wantDec = 0;
        MagAOX::wcc::offsetBoresight( ra, dec, pa, parity, dx, dy, wantRA, wantDec );

        // The offset magnitude on the sky must match what was asked for.
        const double sep = MagAOX::wcc::angularSeparation( ra, dec, wantRA, wantDec ) * 3600.0;
        REQUIRE( sep == Approx( std::hypot( dx, dy ) ).epsilon( 1e-6 ) );

        // Parity -1 means focal plane X runs opposite to increasing right ascension.
        double plusX = 0, plusXDec = 0;
        MagAOX::wcc::offsetBoresight( ra, dec, pa, parity, 10.0, 0.0, plusX, plusXDec );
        REQUIRE( plusX < ra );

        // With parity +1 the same offset goes the other way.
        double flipped = 0, flippedDec = 0;
        MagAOX::wcc::offsetBoresight( ra, dec, pa, +1.0, 10.0, 0.0, flipped, flippedDec );
        REQUIRE( flipped > ra );
    }

    SECTION( "declination is clamped at the poles" )
    {
        double ra = 0, dec = 0;
        MagAOX::wcc::offsetBoresight( 0.0, 89.999, 0.0, -1.0, 0.0, 3600.0 * 5, ra, dec );

        REQUIRE( dec <= 90.0 );
        REQUIRE( MagAOX::wcc::isFinite( ra ) );
    }

    SECTION( "the offset INDI callback moves the mount, even below arrive_tol" )
    {
        telescopeSimTester app;
        const std::string path = writeTelConfig( "/tmp/telescopeSim_test_offset.conf" );
        REQUIRE( app.testLoadConfigFile( path ) == 0 );
        app.testSetupIndi();

        app.testCommand( 192.317, 26.84316038, 0.0 );
        app.testAdvance( 1.0 );
        app.testAdvance( 1.0 );
        REQUIRE( app.st() == telSimState::tracking );

        // The exact case that used to be dropped: 1 arcsec in y with arrive_tol 1.
        REQUIRE( app.testOffset( "0", "1.0", "0" ) == 0 );
        app.testAdvance( 0.001 );
        REQUIRE( ( app.baseDec() - 26.84316038 ) * 3600.0 == Approx( 1.0 ).epsilon( 1e-6 ) );

        // A GUI sends the unedited elements as nan; those must be ignored.
        const double ra0 = app.baseRA();
        REQUIRE( app.testOffset( "nan", "0.1", "nan" ) == 0 );
        app.testAdvance( 0.001 );
        REQUIRE( ( app.baseDec() - 26.84316038 ) * 3600.0 == Approx( 1.1 ).epsilon( 1e-6 ) );
        REQUIRE( app.baseRA() == Approx( ra0 ).margin( 1e-12 ) );
        REQUIRE( MagAOX::wcc::isFinite( app.basePA() ) );

        // Roll adds straight to PA.
        REQUIRE( app.testOffset( "nan", "nan", "0.25" ) == 0 );
        app.testAdvance( 1.0 );
        REQUIRE( app.basePA() == Approx( 0.25 ).margin( 1e-9 ) );

        // The request is cleared once applied.
        REQUIRE( telescopeSimTester::testEl( app.offsetProp(), "x" ) == 0.0 );
        REQUIRE( telescopeSimTester::testEl( app.offsetProp(), "y" ) == 0.0 );
        REQUIRE( telescopeSimTester::testEl( app.offsetProp(), "roll" ) == 0.0 );

        std::remove( path.c_str() );
    }

    SECTION( "an offset during a slew offsets the destination instead of abandoning it" )
    {
        telescopeSimTester app;
        const std::string path = writeTelConfig( "/tmp/telescopeSim_test_offsetslew.conf" );
        REQUIRE( app.testLoadConfigFile( path ) == 0 );
        app.testSetupIndi();

        app.testCommand( 192.317 + 5.0, 26.84316038, 0.0 );
        app.testAdvance( 1.0 );
        REQUIRE( app.st() == telSimState::slewing );

        REQUIRE( app.testOffset( "0", "10", "0" ) == 0 );
        REQUIRE( app.targetRA() == Approx( 192.317 + 5.0 ).margin( 1e-9 ) );
        REQUIRE( ( app.targetDec() - 26.84316038 ) * 3600.0 == Approx( 10.0 ).epsilon( 1e-3 ) );

        std::remove( path.c_str() );
    }
}

/// Verify stop_tracking and the tracking / idle drift rates.
/**
 * \ingroup telescopeSim_unit_test
 */
TEST_CASE( "telescopeSim stops tracking and drifts at the configured rates", "[telescopeSim]" )
{
    // clang-format off
    #ifdef TELESCOPESIM_TEST_DOXYGEN_REF
    telescopeSim::setTrackingStopped;
    telescopeSim::applyDriftRates;
    telescopeSim::publishDriftIndi;
    telescopeSim::newCallBack_m_indiP_stopTracking;
    telescopeSim::newCallBack_m_indiP_trackDrift;
    telescopeSim::newCallBack_m_indiP_idleDrift;
    telescopeSim::advanceMount;
    #endif
    // clang-format on

    telescopeSimTester app;
    const std::string path = writeTelConfig( "/tmp/telescopeSim_test_drift.conf" );
    REQUIRE( app.testLoadConfigFile( path ) == 0 );
    app.testSetupIndi();

    const double keep = std::numeric_limits<double>::quiet_NaN();

    SECTION( "tracking drift moves the base per axis while tracking" )
    {
        app.testCommand( 192.317, 26.84316038, 0.0 );
        app.testAdvance( 0.001 );
        app.testAdvance( 0.001 );
        REQUIRE( app.st() == telSimState::tracking );

        app.testApplyDrift( false, 0.5, -0.25, 0.01 );
        const double ra0 = app.baseRA(), dec0 = app.baseDec(), pa0 = app.basePA();

        for( int i = 0; i < 10; ++i )
        {
            app.testAdvance( 1.0 );
        }

        REQUIRE( ( app.baseRA() - ra0 ) * 3600.0 == Approx( 5.0 ).epsilon( 1e-6 ) );
        REQUIRE( ( app.baseDec() - dec0 ) * 3600.0 == Approx( -2.5 ).epsilon( 1e-6 ) );
        REQUIRE( app.basePA() - pa0 == Approx( 0.1 ).epsilon( 1e-9 ) );
    }

    SECTION( "idle drift applies while not tracking and tracking drift does not" )
    {
        app.testApplyDrift( true, 15.041, 0.0, 0.0 );
        app.testApplyDrift( false, 100.0, 100.0, 1.0 );

        REQUIRE( app.st() == telSimState::idle );
        const double ra0 = app.baseRA(), dec0 = app.baseDec();

        app.testAdvance( 2.0 );

        REQUIRE( ( app.baseRA() - ra0 ) * 3600.0 == Approx( 2.0 * 15.041 ).epsilon( 1e-6 ) );
        REQUIRE( app.baseDec() == Approx( dec0 ).margin( 1e-12 ) );
        REQUIRE( app.basePA() == Approx( 0.0 ).margin( 1e-12 ) );
    }

    SECTION( "no drift is applied during a slew" )
    {
        app.testApplyDrift( true, 1000.0, 1000.0, 1.0 );
        app.testApplyDrift( false, 1000.0, 1000.0, 1.0 );

        app.testCommand( 192.317, 26.84316038 + 1.0, 0.0 );
        app.testAdvance( 0.5 );

        REQUIRE( app.st() == telSimState::slewing );
        REQUIRE( app.baseRA() == Approx( 192.317 ).margin( 1e-9 ) );
        REQUIRE( app.basePA() == Approx( 0.0 ).margin( 1e-12 ) );
    }

    SECTION( "stop_tracking sends a tracking mount idle and resuming tracks where it now points" )
    {
        app.setJitter( 1.0, 1.0, 0.01 );
        app.testApplyDrift( true, 15.041, 0.0, 0.0 );

        app.testCommand( 192.317, 26.84316038, 0.0 );
        app.testAdvance( 0.001 );
        app.testAdvance( 0.001 );
        REQUIRE( app.st() == telSimState::tracking );

        REQUIRE( app.testStopTrackingNew( true ) == 0 );
        REQUIRE( app.trackingStopped() );
        REQUIRE( app.st() == telSimState::idle );

        const double ra0 = app.baseRA();
        app.testAdvance( 10.0 );

        // Idle: drifting, and no jitter on the reported pointing.
        REQUIRE( ( app.baseRA() - ra0 ) * 3600.0 == Approx( 150.41 ).epsilon( 1e-6 ) );
        REQUIRE( app.reportRA() == Approx( app.baseRA() ).margin( 1e-12 ) );
        REQUIRE( app.reportPA() == Approx( app.basePA() ).margin( 1e-12 ) );

        const double driftedRA = app.baseRA();

        REQUIRE( app.testStopTrackingNew( false ) == 0 );
        REQUIRE_FALSE( app.trackingStopped() );
        REQUIRE( app.st() == telSimState::settling );
        REQUIRE( app.targetRA() == Approx( driftedRA ).margin( 1e-12 ) );

        app.testAdvance( 0.001 );
        REQUIRE( app.st() == telSimState::tracking );
        REQUIRE( app.baseRA() == Approx( driftedRA ).margin( 1e-9 ) );
    }

    SECTION( "a slew with tracking stopped finishes, then goes idle rather than tracking" )
    {
        app.testCommand( 192.317 + 0.5, 26.84316038, 0.0 );
        app.testSetTrackingStopped( true );
        REQUIRE( app.st() == telSimState::slewing );

        app.testAdvance( 1.0 );

        REQUIRE( app.st() == telSimState::idle );
        REQUIRE( app.baseRA() == Approx( 192.317 + 0.5 ).margin( 1e-9 ) );
    }

    SECTION( "drift NEWs apply finite elements, ignore nan, and publish from members" )
    {
        app.testApplyDrift( false, 0.1, 0.2, 0.003 );

        REQUIRE( app.testDriftNew( false, "nan", "-0.4", "nan" ) == 0 );
        REQUIRE( app.trackDriftRA() == Approx( 0.1 ).epsilon( 1e-12 ) );
        REQUIRE( app.trackDriftDec() == Approx( -0.4 ).epsilon( 1e-12 ) );
        REQUIRE( app.trackDriftPA() == Approx( 0.003 ).epsilon( 1e-12 ) );

        REQUIRE( telescopeSimTester::testEl( app.driftProp( false ), "ra" ) == Approx( 0.1 ).epsilon( 1e-12 ) );
        REQUIRE( telescopeSimTester::testEl( app.driftProp( false ), "dec" ) == Approx( -0.4 ).epsilon( 1e-12 ) );
        REQUIRE( telescopeSimTester::testEl( app.driftProp( false ), "pa" ) == Approx( 0.003 ).epsilon( 1e-12 ) );

        REQUIRE( app.testDriftNew( true, "15.041", "nan", "nan" ) == 0 );
        REQUIRE( app.idleDriftRA() == Approx( 15.041 ).epsilon( 1e-12 ) );
        REQUIRE( telescopeSimTester::testEl( app.driftProp( true ), "ra" ) == Approx( 15.041 ).epsilon( 1e-12 ) );
        REQUIRE( MagAOX::wcc::isFinite( telescopeSimTester::testEl( app.driftProp( true ), "dec" ) ) );

        app.testApplyDrift( false, keep, keep, keep );
        REQUIRE( app.trackDriftDec() == Approx( -0.4 ).epsilon( 1e-12 ) );
    }

    std::remove( path.c_str() );
}

/// Verify live INDI overlays update the mount model and pointing buffer.
/**
 * \ingroup telescopeSim_unit_test
 */
TEST_CASE( "telescopeSim reloads live sim and pointing-buffer parameters", "[telescopeSim]" )
{
    // clang-format off
    #ifdef TELESCOPESIM_TEST_DOXYGEN_REF
    telescopeSim::applySimParameters;
    telescopeSim::applyPointingBufferParameters;
    telescopeSim::readIndiNumberRequest;
    telescopeSim::handleSimNumberNew;
    telescopeSim::handlePointingNumberNew;
    telescopeSim::simParameter;
    telescopeSim::writeNumberPair;
    telescopeSim::sendNumberPair;
    telescopeSim::syncSimIndi;
    telescopeSim::advanceMount;
    MagAOX::wcc::isFinite;
    #endif
    // clang-format on

    telescopeSimTester app;
    const std::string path = writeTelConfig( "/tmp/telescopeSim_test_live.conf" );
    REQUIRE( app.testLoadConfigFile( path ) == 0 );
    app.testSetupIndi();

    const double keep = std::numeric_limits<double>::quiet_NaN();
    using param = telescopeSimTester::telSimIndiParam;

    SECTION( "a live sim overlay replaces each supplied field and keeps the rest" )
    {
        const uint32_t depth0 = app.pointingDepth();

        app.testApplySim( 2.5, 0.5, 1.25, 0.75, 0.01, 4.0, 0.25, 0.1 );

        REQUIRE( app.slewRate() == Approx( 2.5 ).epsilon( 1e-12 ) );
        REQUIRE( app.rollRate() == Approx( 0.5 ).epsilon( 1e-12 ) );
        REQUIRE( app.jitterX() == Approx( 1.25 ).epsilon( 1e-12 ) );
        REQUIRE( app.jitterY() == Approx( 0.75 ).epsilon( 1e-12 ) );
        REQUIRE( app.jitterRoll() == Approx( 0.01 ).epsilon( 1e-12 ) );
        REQUIRE( app.settleTime() == Approx( 4.0 ).epsilon( 1e-12 ) );
        REQUIRE( app.arriveTol() == Approx( 0.25 ).epsilon( 1e-12 ) );
        REQUIRE( app.jitterTau() == Approx( 0.1 ).epsilon( 1e-12 ) );

        app.testApplySim( keep, keep, 0.0, keep, keep, keep, keep, keep );
        REQUIRE( app.jitterX() == Approx( 0.0 ).epsilon( 1e-12 ) );
        REQUIRE( app.jitterY() == Approx( 0.75 ).epsilon( 1e-12 ) );
        REQUIRE( app.slewRate() == Approx( 2.5 ).epsilon( 1e-12 ) );
        REQUIRE( app.pointingDepth() == depth0 );
    }

    SECTION( "a standard-number NEW applies only a finite target and ignores NaN" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setName( "jitter_x" );
        ip.add( pcf::IndiElement( "current" ) );
        ip.add( pcf::IndiElement( "target" ) );
        ip["current"].setValue( "nan" );
        ip["target"].set( 0.42 );

        double v = 0;
        REQUIRE( app.testReadIndiNumberRequest( ip, v ) );
        REQUIRE( v == Approx( 0.42 ).epsilon( 1e-12 ) );

        pcf::IndiProperty nanIp( pcf::IndiProperty::Number );
        nanIp.setName( "slew_rate" );
        nanIp.add( pcf::IndiElement( "current" ) );
        nanIp.add( pcf::IndiElement( "target" ) );
        nanIp["current"].setValue( "nan" );
        nanIp["target"].setValue( "nan" );

        REQUIRE_FALSE( app.testReadIndiNumberRequest( nanIp, v ) );
        REQUIRE( app.slewRate() == Approx( 1.0 ).epsilon( 1e-12 ) );
    }

    SECTION( "non-positive rates are refused so the mount cannot stall" )
    {
        app.testApplySim( -1.0, 0.0, keep, keep, keep, keep, keep, keep );
        REQUIRE( app.slewRate() == Approx( 1.0 ).epsilon( 1e-12 ) );
        REQUIRE( app.rollRate() == Approx( 1.0 ).epsilon( 1e-12 ) );
    }

    SECTION( "the reported bug: jitter NEW with nan siblings leaves them finite" )
    {
        // The session that exposed -ffast-math: jitter 0.02 -> 0.5 while tracking
        // with nonzero jitter_roll and jitter_tau. The GUI sent a NEW for every
        // sim property, unedited ones as target=nan. jitter_roll, jitter_tau and
        // settle_time became nan and pointing.pa followed.
        app.testApplySim( keep, keep, 0.02, 0.02, 0.01, 0.5, keep, 0.05 );
        app.testSetupIndi();
        app.setJitter( 0.02, 0.02, 0.01 );

        app.testCommand( 192.317, 26.84316038, 0.0 );
        app.testAdvance( 0.001 );
        REQUIRE( app.st() == telSimState::settling );

        REQUIRE( app.testSimNew( param::jitterX, "nan", "0.5" ) == 0 );
        REQUIRE( app.testSimNew( param::jitterRoll, "nan", "nan" ) == 0 );
        REQUIRE( app.testSimNew( param::jitterTau, "nan", "nan" ) == 0 );
        REQUIRE( app.testSimNew( param::settleTime, "nan", "nan" ) == 0 );
        REQUIRE( app.testSimNew( param::rollRate, "nan", "nan" ) == 0 );

        REQUIRE( app.jitterX() == Approx( 0.5 ).epsilon( 1e-12 ) );
        REQUIRE( app.jitterY() == Approx( 0.02 ).epsilon( 1e-12 ) );
        REQUIRE( app.jitterRoll() == Approx( 0.01 ).epsilon( 1e-12 ) );
        REQUIRE( app.jitterTau() == Approx( 0.05 ).epsilon( 1e-12 ) );
        REQUIRE( app.settleTime() == Approx( 0.5 ).epsilon( 1e-12 ) );
        REQUIRE( app.rollRate() == Approx( 1.0 ).epsilon( 1e-12 ) );

        // Each property is SET back from the member, so no nan is left on the server.
        for( param p : { param::jitterX, param::jitterRoll, param::jitterTau, param::settleTime, param::rollRate } )
        {
            REQUIRE( MagAOX::wcc::isFinite( telescopeSimTester::testEl( app.simProp( p ), "current" ) ) );
            REQUIRE( MagAOX::wcc::isFinite( telescopeSimTester::testEl( app.simProp( p ), "target" ) ) );
        }
        REQUIRE( telescopeSimTester::testEl( app.simProp( param::jitterRoll ), "current" ) ==
                 Approx( 0.01 ).epsilon( 1e-12 ) );

        for( int i = 0; i < 5000; ++i )
        {
            app.testAdvance( 1.0 / 5000.0 );
        }

        REQUIRE( app.st() == telSimState::tracking );
        REQUIRE( MagAOX::wcc::isFinite( app.reportRA() ) );
        REQUIRE( MagAOX::wcc::isFinite( app.reportDec() ) );
        REQUIRE( MagAOX::wcc::isFinite( app.reportPA() ) );
    }

    SECTION( "a NaN OU state is cleared so pointing.pa cannot stay NaN" )
    {
        app.testApplySim( keep, keep, 0.5, 0.5, 0.01, keep, keep, 0.05 );
        app.testCommand( 192.317, 26.84316038, 0.0 );
        app.forceState( telSimState::tracking );
        app.testPoisonOuState();

        REQUIRE_FALSE( MagAOX::wcc::isFinite( app.reportPA() ) );

        app.testAdvance( 0.001 );

        REQUIRE( MagAOX::wcc::isFinite( app.reportPA() ) );
        REQUIRE( MagAOX::wcc::isFinite( app.reportRA() ) );
        REQUIRE( MagAOX::wcc::isFinite( app.reportDec() ) );
        REQUIRE( app.jitterTau() == Approx( 0.0 ).epsilon( 1e-12 ) );
    }

    SECTION( "a live slew_rate change is used on the next advance" )
    {
        app.testApplySim( 5.0, keep, keep, keep, keep, keep, keep, keep );
        app.testCommand( 192.317 + 5.0, 26.84316038, 0.0 );
        app.testAdvance( 1.0 );

        REQUIRE( app.baseRA() == Approx( 192.317 + 5.0 ).margin( 1e-6 ) );
        REQUIRE( ( app.st() == telSimState::settling || app.st() == telSimState::tracking ) );
    }

    SECTION( "write_hz and history_s update the implied depth independently" )
    {
        REQUIRE( app.testApplyPointingBuffer( 2500.0, 4.0 ) == 0 );
        REQUIRE( app.writeHz() == Approx( 2500.0 ).epsilon( 1e-12 ) );
        REQUIRE( app.historyS() == Approx( 4.0 ).epsilon( 1e-12 ) );
        REQUIRE( app.pointingDepth() == 10000 );

        REQUIRE( app.testApplyPointingBuffer( keep, 8.0 ) == 0 );
        REQUIRE( app.writeHz() == Approx( 2500.0 ).epsilon( 1e-12 ) );
        REQUIRE( app.historyS() == Approx( 8.0 ).epsilon( 1e-12 ) );
        REQUIRE( app.pointingDepth() == 20000 );

        REQUIRE( app.testApplyPointingBuffer( -10.0, 0.0 ) == 0 );
        REQUIRE( app.writeHz() == Approx( 2500.0 ).epsilon( 1e-12 ) );
        REQUIRE( app.historyS() == Approx( 8.0 ).epsilon( 1e-12 ) );
    }

    std::remove( path.c_str() );
}

} // namespace telescopeSimTest

} // namespace libXWCTest
