/** \file dmCtrl_test.cpp
 * \brief Catch2 tests for the dmCtrl app.
 * \author Claude Code
 *
 * These tests exercise the command/telemetry structures in `dmCommands.hpp`.  The `dmCtrl` class in
 * `dmCtrl.hpp` is not included because that header does not currently compile (it refers to the
 * `summerDevice` serial helpers without their `dev::` namespace and to an undefined `PacketCallbacks`).
 *
 * \ingroup dmCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstring>
#include <iomanip>
#include <string>
#include <vector>

#include "../../../libMagAOX/libMagAOX.hpp"

/// The logging application type expected by `dmCommands.hpp` (normally declared by `dmCtrl.hpp`).
typedef MagAOX::app::MagAOXApp<true> MagAOXAppT;

#include "../dmCommands.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup dmCtrl_unit_test dmCtrl Unit Tests
 * \brief Unit tests for the dmCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `dmCtrl` unit tests.
/** \ingroup dmCtrl_unit_test
 */
namespace dmCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Build a telemetry payload whose fields are `base`, `base+1`, ..., `base+10`.
CGraphDMTelemetryPayload makePayload( double base /**< [in] the value of the first field */ )
{
    CGraphDMTelemetryPayload p;
    p.P1V2  = base + 0;
    p.P2V2  = base + 1;
    p.P28V  = base + 2;
    p.P2V5  = base + 3;
    p.P6V   = base + 4;
    p.P5V   = base + 5;
    p.P3V3D = base + 6;
    p.P4V3  = base + 7;
    p.P2I2  = base + 8;
    p.P4I3  = base + 9;
    p.P6I   = base + 10;
    return p;
}

/// Require that every field of a payload matches the `makePayload( base )` pattern.
void requirePayload( const CGraphDMTelemetryPayload &p,   /**< [in] the payload to check */
                     double                          base /**< [in] the expected value of the first field */
)
{
    REQUIRE( p.P1V2 == base + 0 );
    REQUIRE( p.P2V2 == base + 1 );
    REQUIRE( p.P28V == base + 2 );
    REQUIRE( p.P2V5 == base + 3 );
    REQUIRE( p.P6V == base + 4 );
    REQUIRE( p.P5V == base + 5 );
    REQUIRE( p.P3V3D == base + 6 );
    REQUIRE( p.P4V3 == base + 7 );
    REQUIRE( p.P2I2 == base + 8 );
    REQUIRE( p.P4I3 == base + 9 );
    REQUIRE( p.P6I == base + 10 );
}

/// Minimal concrete `PZTQuery` for testing the base-class payload handling.
class testQuery : public PZTQuery
{
  public:
    /// Construct with a default payload.
    testQuery( uint16_t type,        /**< [in] the payload type */
               void    *defaultData, /**< [in] the default payload data */
               size_t   defaultLen   /**< [in] the default payload length */
    )
    {
        PayloadType        = type;
        DefaultPayloadData = defaultData;
        DefaultPayloadLen  = defaultLen;
        PayloadData        = defaultData;
        PayloadLen         = defaultLen;
    }

    int m_errorCalls{ 0 };   ///< Number of calls to errorLogString().
    int m_processCalls{ 0 }; ///< Number of calls to processReply().
    int m_logCalls{ 0 };     ///< Number of calls to logReply().

    /// Count error log requests.
    void errorLogString( const size_t ParamsLen /**< [in] unused reply length */ ) override
    {
        static_cast<void>( ParamsLen );
        ++m_errorCalls;
    }

    /// Count processed replies.
    void processReply( char const  *Params,   /**< [in] unused reply buffer */
                       const size_t ParamsLen /**< [in] unused reply length */
                       ) override
    {
        static_cast<void>( Params );
        static_cast<void>( ParamsLen );
        ++m_processCalls;
    }

    /// Count log requests.
    void logReply() override
    {
        ++m_logCalls;
    }
};

/// \endcond

/// Verify the command payload type constants.
/**
 * \ingroup dmCtrl_unit_test
 */
TEST_CASE( "dmCtrl payload type constants", "[dmCtrl]" )
{
    // clang-format off
    #ifdef DMCTRL_TEST_DOXYGEN_REF
    MagAOX::app::CGraphPayloadTypeDMDac;
    MagAOX::app::CGraphPayloadTypeDMTelemetry;
    MagAOX::app::CGraphPayloadTypeDMHVSwitch;
    MagAOX::app::CGraphPayloadTypeDMDacConfig;
    #endif
    // clang-format on

    REQUIRE( CGraphPayloadTypeDMDac == 0x3002U );
    REQUIRE( CGraphPayloadTypeDMTelemetry == 0x3004U );
    REQUIRE( CGraphPayloadTypeDMHVSwitch == 0x3007U );
    REQUIRE( CGraphPayloadTypeDMDacConfig == 0x3009U );

    // All the payload types are distinct
    std::vector<uint16_t> types = { CGraphPayloadTypeDMDac,
                                    CGraphPayloadTypeDMTelemetry,
                                    CGraphPayloadTypeDMHVSwitch,
                                    CGraphPayloadTypeDMDacConfig };
    for( size_t i = 0; i < types.size(); ++i )
    {
        for( size_t j = i + 1; j < types.size(); ++j )
        {
            REQUIRE( types[i] != types[j] );
        }
    }
}

/// Verify `CGraphDMTelemetryPayload` equality operators.
/**
 * \ingroup dmCtrl_unit_test
 */
TEST_CASE( "dmCtrl telemetry payload comparison", "[dmCtrl]" )
{
    // clang-format off
    #ifdef DMCTRL_TEST_DOXYGEN_REF
    MagAOX::app::CGraphDMTelemetryPayload::operator==(const CGraphDMTelemetryPayload *);
    MagAOX::app::CGraphDMTelemetryPayload::operator==(const CGraphDMTelemetryPayload);
    #endif
    // clang-format on

    CGraphDMTelemetryPayload a = makePayload( 1.0 );
    CGraphDMTelemetryPayload b = makePayload( 1.0 );

    SECTION( "identical payloads compare equal by value and by pointer" )
    {
        // The operators are non-const members, so evaluate them outside the Catch2 decomposer.
        bool eqValue   = ( a == b );
        bool eqPointer = ( a == &b );
        bool eqReverse = ( b == a );
        bool eqSelf    = ( a == a );
        REQUIRE( eqValue );
        REQUIRE( eqPointer );
        REQUIRE( eqReverse );
        REQUIRE( eqSelf );
    }

    SECTION( "a difference in any single field makes the payloads unequal" )
    {
        for( int field = 0; field < 11; ++field )
        {
            CGraphDMTelemetryPayload c = makePayload( 1.0 );
            double                  *f = nullptr;
            switch( field )
            {
            case 0:
                f = &c.P1V2;
                break;
            case 1:
                f = &c.P2V2;
                break;
            case 2:
                f = &c.P28V;
                break;
            case 3:
                f = &c.P2V5;
                break;
            case 4:
                f = &c.P6V;
                break;
            case 5:
                f = &c.P5V;
                break;
            case 6:
                f = &c.P3V3D;
                break;
            case 7:
                f = &c.P4V3;
                break;
            case 8:
                f = &c.P2I2;
                break;
            case 9:
                f = &c.P4I3;
                break;
            default:
                f = &c.P6I;
                break;
            }
            *f += 0.5;

            INFO( "field " << field );
            bool eqValue   = ( a == c );
            bool eqPointer = ( a == &c );
            REQUIRE_FALSE( eqValue );
            REQUIRE_FALSE( eqPointer );
        }
    }
}

/// Verify `CGraphDMTelemetryPayload` assignment operators.
/**
 * \ingroup dmCtrl_unit_test
 */
TEST_CASE( "dmCtrl telemetry payload assignment", "[dmCtrl]" )
{
    // clang-format off
    #ifdef DMCTRL_TEST_DOXYGEN_REF
    MagAOX::app::CGraphDMTelemetryPayload::operator=(const CGraphDMTelemetryPayload *);
    MagAOX::app::CGraphDMTelemetryPayload::operator=(const CGraphDMTelemetryPayload &);
    #endif
    // clang-format on

    CGraphDMTelemetryPayload src = makePayload( 10.0 );

    SECTION( "assignment from a reference copies every field" )
    {
        CGraphDMTelemetryPayload  dst = makePayload( -5.0 );
        CGraphDMTelemetryPayload &r   = ( dst = src );
        requirePayload( dst, 10.0 );
        REQUIRE( &r == &dst );
    }

    SECTION( "assignment from a pointer copies every field" )
    {
        CGraphDMTelemetryPayload  dst = makePayload( -5.0 );
        CGraphDMTelemetryPayload &r   = ( dst = &src );
        requirePayload( dst, 10.0 );
        REQUIRE( &r == &dst );
    }

    SECTION( "the source is not modified" )
    {
        CGraphDMTelemetryPayload dst = makePayload( -5.0 );
        dst                          = src;
        requirePayload( src, 10.0 );
    }
}

/// Verify the `TelemetryQuery` constructor sets the payload type and log strings.
/**
 * \ingroup dmCtrl_unit_test
 */
TEST_CASE( "dmCtrl TelemetryQuery construction", "[dmCtrl]" )
{
    // clang-format off
    #ifdef DMCTRL_TEST_DOXYGEN_REF
    MagAOX::app::TelemetryQuery::TelemetryQuery();
    MagAOX::app::PZTQuery::getPayloadType();
    MagAOX::app::PZTQuery::getPayloadData();
    MagAOX::app::PZTQuery::getPayloadLen();
    #endif
    // clang-format on

    TelemetryQuery tq;

    REQUIRE( tq.getPayloadType() == CGraphPayloadTypeDMTelemetry );
    REQUIRE( tq.getPayloadData() == nullptr );
    REQUIRE( tq.getPayloadLen() == 0 );
    REQUIRE( tq.ParamsPtr == nullptr );
    REQUIRE( tq.startLog == "PZTTelemetry: Querying telemetry." );
    REQUIRE( tq.endLog == "PZTTelemetry: Finished querying telemetry." );
}

/// Verify `PZTQuery::setPayload()` and `PZTQuery::resetPayload()`.
/**
 * \ingroup dmCtrl_unit_test
 */
TEST_CASE( "dmCtrl PZTQuery payload set and reset", "[dmCtrl]" )
{
    // clang-format off
    #ifdef DMCTRL_TEST_DOXYGEN_REF
    MagAOX::app::PZTQuery::setPayload(void *, uint16_t);
    MagAOX::app::PZTQuery::resetPayload();
    #endif
    // clang-format on

    SECTION( "TelemetryQuery resets to a null, zero-length payload" )
    {
        TelemetryQuery tq;
        char           buf[16] = { 0 };

        tq.setPayload( buf, sizeof( buf ) );
        REQUIRE( tq.getPayloadData() == static_cast<void *>( buf ) );
        REQUIRE( tq.getPayloadLen() == sizeof( buf ) );

        tq.resetPayload();
        REQUIRE( tq.getPayloadData() == nullptr );
        REQUIRE( tq.getPayloadLen() == 0 );

        // Payload type is unchanged by payload manipulation
        REQUIRE( tq.getPayloadType() == CGraphPayloadTypeDMTelemetry );
    }

    SECTION( "a query with a non-null default payload resets to that default" )
    {
        char      defBuf[8]  = { 0 };
        char      newBuf[32] = { 0 };
        testQuery q( CGraphPayloadTypeDMDac, defBuf, sizeof( defBuf ) );

        REQUIRE( q.getPayloadType() == CGraphPayloadTypeDMDac );
        REQUIRE( q.getPayloadData() == static_cast<void *>( defBuf ) );
        REQUIRE( q.getPayloadLen() == sizeof( defBuf ) );

        q.setPayload( newBuf, sizeof( newBuf ) );
        REQUIRE( q.getPayloadData() == static_cast<void *>( newBuf ) );
        REQUIRE( q.getPayloadLen() == sizeof( newBuf ) );

        q.resetPayload();
        REQUIRE( q.getPayloadData() == static_cast<void *>( defBuf ) );
        REQUIRE( q.getPayloadLen() == sizeof( defBuf ) );
    }
}

/// Verify `TelemetryQuery::processReply()` decodes a full-length reply.
/**
 * \ingroup dmCtrl_unit_test
 */
TEST_CASE( "dmCtrl TelemetryQuery processReply with a valid reply", "[dmCtrl]" )
{
    // clang-format off
    #ifdef DMCTRL_TEST_DOXYGEN_REF
    MagAOX::app::TelemetryQuery::processReply(char const *, const size_t);
    #endif
    // clang-format on

    TelemetryQuery tq;

    CGraphDMTelemetryPayload src = makePayload( 3.25 );

    SECTION( "exactly sizeof(payload) bytes" )
    {
        alignas( CGraphDMTelemetryPayload ) char buf[sizeof( CGraphDMTelemetryPayload )];
        std::memcpy( buf, &src, sizeof( src ) );

        tq.processReply( buf, sizeof( buf ) );

        REQUIRE( tq.ParamsPtr == reinterpret_cast<const CGraphDMTelemetryPayload *>( buf ) );
        requirePayload( tq.Telemetry, 3.25 );
    }

    SECTION( "a longer reply is accepted and only the leading payload is decoded" )
    {
        alignas( CGraphDMTelemetryPayload ) char buf[sizeof( CGraphDMTelemetryPayload ) + 16];
        std::memset( buf, 0x7F, sizeof( buf ) );
        std::memcpy( buf, &src, sizeof( src ) );

        tq.processReply( buf, sizeof( buf ) );

        REQUIRE( tq.ParamsPtr != nullptr );
        requirePayload( tq.Telemetry, 3.25 );
    }

    SECTION( "the decoded telemetry is a copy, independent of the reply buffer" )
    {
        alignas( CGraphDMTelemetryPayload ) char buf[sizeof( CGraphDMTelemetryPayload )];
        std::memcpy( buf, &src, sizeof( src ) );

        tq.processReply( buf, sizeof( buf ) );

        std::memset( buf, 0, sizeof( buf ) );
        requirePayload( tq.Telemetry, 3.25 );
    }
}

/// Verify `TelemetryQuery::processReply()` rejects short or null replies.
/**
 * \ingroup dmCtrl_unit_test
 */
TEST_CASE( "dmCtrl TelemetryQuery processReply with an invalid reply", "[dmCtrl]" )
{
    // clang-format off
    #ifdef DMCTRL_TEST_DOXYGEN_REF
    MagAOX::app::TelemetryQuery::processReply(char const *, const size_t);
    MagAOX::app::TelemetryQuery::errorLogString(const size_t);
    #endif
    // clang-format on

    TelemetryQuery tq;
    tq.Telemetry = makePayload( 100.0 );

    CGraphDMTelemetryPayload src = makePayload( 1.0 );

    alignas( CGraphDMTelemetryPayload ) char buf[sizeof( CGraphDMTelemetryPayload )];
    std::memcpy( buf, &src, sizeof( src ) );

    SECTION( "a reply one byte short is rejected" )
    {
        REQUIRE_NOTHROW( tq.processReply( buf, sizeof( buf ) - 1 ) );
        REQUIRE( tq.ParamsPtr == nullptr );
        requirePayload( tq.Telemetry, 100.0 );
    }

    SECTION( "an empty reply is rejected" )
    {
        REQUIRE_NOTHROW( tq.processReply( buf, 0 ) );
        REQUIRE( tq.ParamsPtr == nullptr );
        requirePayload( tq.Telemetry, 100.0 );
    }

    SECTION( "a null reply buffer is rejected even with a valid length" )
    {
        REQUIRE_NOTHROW( tq.processReply( nullptr, sizeof( buf ) ) );
        REQUIRE( tq.ParamsPtr == nullptr );
        requirePayload( tq.Telemetry, 100.0 );
    }

    SECTION( "errorLogString can be called directly" )
    {
        REQUIRE_NOTHROW( tq.errorLogString( 3 ) );
    }
}

/// Verify `TelemetryQuery::logReply()` formats the telemetry without error.
/**
 * \ingroup dmCtrl_unit_test
 */
TEST_CASE( "dmCtrl TelemetryQuery logReply", "[dmCtrl]" )
{
    // clang-format off
    #ifdef DMCTRL_TEST_DOXYGEN_REF
    MagAOX::app::TelemetryQuery::logReply();
    #endif
    // clang-format on

    TelemetryQuery tq;
    tq.Telemetry = makePayload( 0.5 );

    REQUIRE_NOTHROW( tq.logReply() );

    // Logging does not modify the telemetry
    requirePayload( tq.Telemetry, 0.5 );
}

/// Verify `PZTQuery` virtual dispatch as used by the `dmCtrl` query list.
/**
 * \ingroup dmCtrl_unit_test
 */
TEST_CASE( "dmCtrl PZTQuery polymorphic dispatch", "[dmCtrl]" )
{
    // clang-format off
    #ifdef DMCTRL_TEST_DOXYGEN_REF
    MagAOX::app::PZTQuery::processReply(char const *, const size_t);
    MagAOX::app::PZTQuery::logReply();
    MagAOX::app::PZTQuery::errorLogString(const size_t);
    #endif
    // clang-format on

    SECTION( "TelemetryQuery through a base pointer" )
    {
        PZTQuery *q = new TelemetryQuery();

        REQUIRE( q->getPayloadType() == CGraphPayloadTypeDMTelemetry );

        CGraphDMTelemetryPayload src = makePayload( 7.0 );

        alignas( CGraphDMTelemetryPayload ) char buf[sizeof( CGraphDMTelemetryPayload )];
        std::memcpy( buf, &src, sizeof( src ) );

        q->processReply( buf, sizeof( buf ) );

        TelemetryQuery *tq = dynamic_cast<TelemetryQuery *>( q );
        REQUIRE( tq != nullptr );
        requirePayload( tq->Telemetry, 7.0 );

        delete q; // exercises the virtual destructor
    }

    SECTION( "a list of queries dispatches to each derived class" )
    {
        testQuery               tq1( CGraphPayloadTypeDMDac, nullptr, 0 );
        testQuery               tq2( CGraphPayloadTypeDMHVSwitch, nullptr, 0 );
        std::vector<PZTQuery *> queries = { &tq1, &tq2 };

        for( PZTQuery *q : queries )
        {
            q->processReply( nullptr, 0 );
            q->logReply();
            q->errorLogString( 0 );
        }

        REQUIRE( tq1.m_processCalls == 1 );
        REQUIRE( tq1.m_logCalls == 1 );
        REQUIRE( tq1.m_errorCalls == 1 );
        REQUIRE( tq2.m_processCalls == 1 );
        REQUIRE( tq2.m_logCalls == 1 );
        REQUIRE( tq2.m_errorCalls == 1 );

        REQUIRE( queries[0]->getPayloadType() == CGraphPayloadTypeDMDac );
        REQUIRE( queries[1]->getPayloadType() == CGraphPayloadTypeDMHVSwitch );
    }
}

} // namespace dmCtrlTest

} // namespace libXWCTest
