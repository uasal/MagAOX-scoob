/** \file telescopeSim_test.cpp
 * \brief Catch2 tests for the telescopeSim app.
 * \author Adam Schilperoort
 *
 * \ingroup telescopeSim_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <fstream>

#include "../telescopeSim.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup telescopeSim_unit_test telescopeSim Unit Tests
 * \brief Unit tests for the telescopeSim application.
 *
 * Covers the mount model: that a commanded target is actually reached at the
 * configured rate rather than teleported to, that the state machine passes through
 * settling before reporting tracking, that jitter has the configured amplitude and
 * does not accumulate into a random walk, and that a relative offset moves the
 * boresight the way the shared field-angle convention says it should.
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
    /// Register the config targets, read a file, and load it.
    int testLoadConfigFile( const std::string &path )
    {
        setupConfig();
        config.readConfig( path );
        return loadConfigImpl( config );
    }

    /// Command a target, as start_visit or goto_target would.
    void testCommand( double ra, double dec, double pa )
    {
        commandTarget( ra, dec, pa, "test" );
    }

    /// Advance the mount model once.
    void testUpdate()
    {
        updateMount();
    }

    /// Pretend this many seconds have passed, so a step can be forced.
    /** Both the update baseline and the settle start move, because both are
     * measured against the same clock and only one of them advancing would let a
     * settle expire or stall spuriously.
     */
    void testRewind( double seconds )
    {
        m_lastUpdate -= seconds;
        m_settleStart -= seconds;
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

    /// The target.
    double targetRA()
    {
        return m_targetRA;
    }
    double targetDec()
    {
        return m_targetDec;
    }

    /// Set the jitter amplitude directly.
    void setJitter( double j, double jr )
    {
        m_jitter = j;
        m_jitterRoll = jr;
    }

    /// Set the settle time directly.
    void setSettle( double s )
    {
        m_settleTime = s;
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
    fout << "[sim]\nslew_rate=1.0\nroll_rate=1.0\njitter=0\njitter_roll=0\n";
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
    telescopeSim::appStartup;
    telescopeSim::appLogic;
    telescopeSim::appShutdown;
    telSimStateName;
    #endif
    // clang-format on

    SECTION( "default construction succeeds" )
    {
        telescopeSim app;

        REQUIRE( true );
    }

    SECTION( "startup pointing is applied" )
    {
        telescopeSimTester app;
        const std::string path = writeTelConfig( "/tmp/telescopeSim_test_config.conf" );

        REQUIRE( app.testLoadConfigFile( path ) == 0 );
        REQUIRE( app.baseRA() == Approx( 192.317 ).epsilon( 1e-12 ) );
        REQUIRE( app.baseDec() == Approx( 26.84316038 ).epsilon( 1e-12 ) );
        REQUIRE( app.reportRA() == Approx( 192.317 ).epsilon( 1e-12 ) );
        REQUIRE( app.st() == telSimState::idle );

        std::remove( path.c_str() );
    }

    SECTION( "a non-positive slew or roll rate is rejected" )
    {
        // A zero rate would mean the mount never arrives, which is worse than
        // refusing to start.
        for( const char *bad : { "slew_rate=0\n", "slew_rate=-1\n", "roll_rate=0\n" } )
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

        // Prime the elapsed-time baseline, then step one simulated second at a time.
        app.testUpdate();

        int steps = 0;
        while( app.st() == telSimState::slewing && steps < 20 )
        {
            app.testRewind( 1.0 );
            app.testUpdate();
            ++steps;
        }

        // It must not have arrived instantly, and must not have taken forever.
        REQUIRE( steps >= 4 );
        REQUIRE( steps <= 8 );

        // With settle_time 0 the next update promotes it to tracking.
        REQUIRE( ( app.st() == telSimState::settling || app.st() == telSimState::tracking ) );

        app.testRewind( 1.0 );
        app.testUpdate();
        REQUIRE( app.st() == telSimState::tracking );

        // And it arrived at the target, not past it.
        REQUIRE( app.baseRA() == Approx( 192.317 + 5.0 ).margin( 1e-6 ) );
        REQUIRE( app.baseDec() == Approx( 26.84316038 ).margin( 1e-6 ) );
    }

    SECTION( "a settle time is waited out before tracking is reported" )
    {
        app.setSettle( 5.0 );
        app.testCommand( 192.317 + 0.1, 26.84316038, 0.0 );

        app.testUpdate();
        app.testRewind( 1.0 );
        app.testUpdate();

        // Arrived, so settling; not yet tracking.
        REQUIRE( app.st() == telSimState::settling );

        app.testRewind( 1.0 );
        app.testUpdate();
        REQUIRE( app.st() == telSimState::settling );

        // Past the settle time it promotes.
        app.testRewind( 6.0 );
        app.testUpdate();
        REQUIRE( app.st() == telSimState::tracking );
    }

    SECTION( "roll slews at its own rate" )
    {
        app.testCommand( 192.317, 26.84316038, 3.0 );

        app.testUpdate();
        app.testRewind( 1.0 );
        app.testUpdate();

        // One second at 1 deg/s covers one degree of the three.
        REQUIRE( app.basePA() == Approx( 1.0 ).margin( 1e-6 ) );
        REQUIRE( app.st() == telSimState::slewing );
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

/// Verify jitter has the configured amplitude and does not accumulate.
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
        app.setJitter( 0.0, 0.0 );
        app.forceState( telSimState::tracking );

        for( int i = 0; i < 20; ++i )
        {
            app.testRewind( 1.0 );
            app.testUpdate();
            REQUIRE( app.reportRA() == Approx( app.baseRA() ).margin( 1e-12 ) );
            REQUIRE( app.reportDec() == Approx( app.baseDec() ).margin( 1e-12 ) );
        }
    }

    SECTION( "jitter has the configured rms and the base pointing stays put" )
    {
        const double jit = 2.0; // arcsec rms, large enough to measure quickly
        app.setJitter( jit, 0.0 );
        app.forceState( telSimState::tracking );

        const double ra0 = app.baseRA();
        const double dec0 = app.baseDec();

        double s = 0, s2 = 0;
        const int n = 4000;

        for( int i = 0; i < n; ++i )
        {
            app.testRewind( 1.0 );
            app.testUpdate();

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

    SECTION( "no jitter is applied while idle" )
    {
        app.setJitter( 5.0, 0.5 );
        app.forceState( telSimState::idle );

        app.testRewind( 1.0 );
        app.testUpdate();

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
        REQUIRE( std::isfinite( ra ) );
    }
}

} // namespace telescopeSimTest

} // namespace libXWCTest
