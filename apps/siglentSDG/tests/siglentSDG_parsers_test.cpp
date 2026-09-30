/** \file siglentSDG_parsers_test.cpp
 * \brief Catch2 tests for the siglentSDG response parsers.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 */

#include "../../../tests/testXWC.hpp"

#include <string>
#include <vector>

#include "../siglentSDG_parsers.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \addtogroup siglentSDG_unit_test
 * \brief Unit tests for the siglentSDG application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `siglentSDG` unit tests.
/** \ingroup siglentSDG_unit_test
 */
namespace siglentSDGTest
{

/// Output values of parseBSWV, with a helper to run the parser.
struct bswvResult
{
    int         channel{ -10 }; ///< The parsed channel.
    std::string wvtp;           ///< The parsed waveform type.
    double      freq{ -10 };    ///< The parsed frequency [Hz].
    double      peri{ -10 };    ///< The parsed period [s].
    double      amp{ -10 };     ///< The parsed amplitude [V].
    double      ampvrms{ -10 }; ///< The parsed RMS amplitude [V].
    double      ofst{ -10 };    ///< The parsed offset [V].
    double      hlev{ -10 };    ///< The parsed high level [V].
    double      llev{ -10 };    ///< The parsed low level [V].
    double      phse{ -10 };    ///< The parsed phase [deg].
    double      wdth{ -10 };    ///< The parsed pulse width [s].

    /// Run parseBSWV on a response, storing the outputs in this structure.
    /**
     * \returns the return value of parseBSWV
     */
    int parse( const std::string &resp /**< [in] the device response */ )
    {
        return parseBSWV( channel, wvtp, freq, peri, amp, ampvrms, ofst, hlev, llev, phse, wdth, resp );
    }
};

/// Build a BSWV response from a channel prefix and key/value pairs.
/**
 * \returns a string of the form `<prefix>:BSWV k0,v0,k1,v1,...`
 */
std::string makeBSWV( const std::string              &prefix, /**< [in] the channel prefix, e.g. C1 */
                      const std::vector<std::string> &kv /**< [in] alternating keys and values */ )
{
    std::string s = prefix + ":BSWV ";
    for( size_t n = 0; n < kv.size(); ++n )
    {
        if( n > 0 )
        {
            s += ",";
        }
        s += kv[n];
    }
    return s;
}

/// The key/value pairs of a valid SINE BSWV response.
/**
 * \returns the alternating keys and values
 */
std::vector<std::string> sineKV()
{
    return { "WVTP",
             "SINE",
             "FRQ",
             "1000.5HZ",
             "PERI",
             "0.0009995S",
             "AMP",
             "0.5V",
             "AMPVRMS",
             "0.1768Vrms",
             "OFST",
             "-0.25V",
             "HLEV",
             "0V",
             "LLEV",
             "-0.5V",
             "PHSE",
             "90" };
}

/// The key/value pairs of a valid PULSE BSWV response.
/**
 * \returns the alternating keys and values
 */
std::vector<std::string> pulseKV()
{
    return { "WVTP", "PULSE", "FRQ", "2000HZ", "PERI", "0.0005S", "AMP", "4V",    "AMPVRMS",  "2Vrms", "OFST",
             "2V",   "HLEV",  "4V",  "LLEV",   "0V",   "DUTY",    "50",  "WIDTH", "0.00025S", "RISE",  "8.4e-09S" };
}

/// Signature shared by the STATE parsers (MDWV, SWWV, BTWV).
typedef int ( *stateParserT )( int &, std::string &, const std::string & );

/// Verify parseOUTP on valid responses.
/**
 * \ingroup siglentSDG_unit_test
 */
TEST_CASE( "siglentSDG parseOUTP valid responses", "[siglentSDG][parsers]" )
{
    // clang-format off
    #ifdef SIGLENTSDG_TEST_DOXYGEN_REF
    parseOUTP( *(int *)nullptr, *(int *)nullptr, "" );
    #endif
    // clang-format on

    int channel = -10;
    int output  = -10;

    SECTION( "off" )
    {
        REQUIRE( parseOUTP( channel, output, "C1:OUTP OFF,LOAD,HZ,PLRT,NOR" ) == 0 );
        REQUIRE( channel == 1 );
        REQUIRE( output == 0 );
    }

    SECTION( "on, with a numeric load" )
    {
        REQUIRE( parseOUTP( channel, output, "C2:OUTP ON,LOAD,50,PLRT,INVT" ) == 0 );
        REQUIRE( channel == 2 );
        REQUIRE( output == 1 );
    }

    SECTION( "minimal response" )
    {
        REQUIRE( parseOUTP( channel, output, "C1:OUTP ON" ) == 0 );
        REQUIRE( channel == 1 );
        REQUIRE( output == 1 );
    }

    SECTION( "trailing newline" )
    {
        REQUIRE( parseOUTP( channel, output, "C2:OUTP OFF,LOAD,HZ,PLRT,NOR\n" ) == 0 );
        REQUIRE( channel == 2 );
        REQUIRE( output == 0 );
    }

    SECTION( "multi-digit channel" )
    {
        REQUIRE( parseOUTP( channel, output, "C12:OUTP ON,LOAD,HZ" ) == 0 );
        REQUIRE( channel == 12 );
        REQUIRE( output == 1 );
    }
}

/// Verify the parseOUTP error codes and output resets.
/**
 * \ingroup siglentSDG_unit_test
 */
TEST_CASE( "siglentSDG parseOUTP errors", "[siglentSDG][parsers]" )
{
    // clang-format off
    #ifdef SIGLENTSDG_TEST_DOXYGEN_REF
    parseOUTP( *(int *)nullptr, *(int *)nullptr, "" );
    #endif
    // clang-format on

    int channel = -10;
    int output  = -10;

    // too few tokens; outputs are reset to -1
    REQUIRE( parseOUTP( channel, output, "" ) == -1 );
    REQUIRE( channel == -1 );
    REQUIRE( output == -1 );
    REQUIRE( parseOUTP( channel, output, "C1" ) == -1 );

    // wrong command
    REQUIRE( parseOUTP( channel, output, "C1:BSWV ON" ) == -2 );
    REQUIRE( parseOUTP( channel, output, "C1:OUTPON" ) == -2 );
    REQUIRE( parseOUTP( channel, output, "C1:outp ON" ) == -2 );

    // bad channel prefix
    REQUIRE( parseOUTP( channel, output, "X1:OUTP ON" ) == -3 );
    REQUIRE( parseOUTP( channel, output, ":OUTP ON" ) == -3 );
    REQUIRE( channel == -1 );

    // no channel number
    REQUIRE( parseOUTP( channel, output, "C:OUTP ON" ) == -4 );
    REQUIRE( channel == -1 );

    // no state; the channel is already parsed
    REQUIRE( parseOUTP( channel, output, "C2:OUTP" ) == -5 );
    REQUIRE( channel == 2 );
    REQUIRE( output == -1 );

    // bad state
    REQUIRE( parseOUTP( channel, output, "C1:OUTP MAYBE,LOAD,HZ" ) == -6 );
    REQUIRE( channel == 1 );
    REQUIRE( output == -1 );
    REQUIRE( parseOUTP( channel, output, "C1:OUTP on" ) == -6 );
    REQUIRE( parseOUTP( channel, output, "C1:OUTP  ON" ) == -6 ); // double space gives an empty token
}

/// Verify parseBSWV for SINE waveforms.
/**
 * \ingroup siglentSDG_unit_test
 */
TEST_CASE( "siglentSDG parseBSWV sine", "[siglentSDG][parsers]" )
{
    // clang-format off
    #ifdef SIGLENTSDG_TEST_DOXYGEN_REF
    parseBSWV( *(int *)nullptr, *(std::string *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, "" );
    #endif
    // clang-format on

    SECTION( "manual example" )
    {
        bswvResult r;
        REQUIRE(
            r.parse(
                "C1:BSWV WVTP,SINE,FRQ,10HZ,PERI,0.1S,AMP,2V,AMPVRMS,0.707Vrms,OFST,0V,HLEV,1V,LLEV,-1V,PHSE,0" ) ==
            0 );
        REQUIRE( r.channel == 1 );
        REQUIRE( r.wvtp == "SINE" );
        REQUIRE( r.freq == Approx( 10 ) );
        REQUIRE( r.peri == Approx( 0.1 ) );
        REQUIRE( r.amp == Approx( 2 ) );
        REQUIRE( r.ampvrms == Approx( 0.707 ) );
        REQUIRE( r.ofst == Approx( 0 ) );
        REQUIRE( r.hlev == Approx( 1 ) );
        REQUIRE( r.llev == Approx( -1 ) );
        REQUIRE( r.phse == Approx( 0 ) );
        REQUIRE( r.wdth == Approx( 0 ) );
    }

    SECTION( "channel 2, negative offset, phase, unit suffixes stripped" )
    {
        bswvResult r;
        REQUIRE( r.parse( makeBSWV( "C2", sineKV() ) ) == 0 );
        REQUIRE( r.channel == 2 );
        REQUIRE( r.wvtp == "SINE" );
        REQUIRE( r.freq == Approx( 1000.5 ) );
        REQUIRE( r.peri == Approx( 0.0009995 ) );
        REQUIRE( r.amp == Approx( 0.5 ) );
        REQUIRE( r.ampvrms == Approx( 0.1768 ) );
        REQUIRE( r.ofst == Approx( -0.25 ) );
        REQUIRE( r.hlev == Approx( 0 ) );
        REQUIRE( r.llev == Approx( -0.5 ) );
        REQUIRE( r.phse == Approx( 90 ) );
        REQUIRE( r.wdth == Approx( 0 ) ); // no width for SINE
    }

    SECTION( "exponent notation" )
    {
        std::vector<std::string> kv = sineKV();
        kv[3]                       = "1.5e+06HZ";
        kv[5]                       = "6.66667e-07S";
        bswvResult r;
        REQUIRE( r.parse( makeBSWV( "C1", kv ) ) == 0 );
        REQUIRE( r.freq == Approx( 1.5e6 ) );
        REQUIRE( r.peri == Approx( 6.66667e-7 ) );
    }

    SECTION( "trailing newline" )
    {
        bswvResult r;
        REQUIRE( r.parse( makeBSWV( "C1", sineKV() ) + "\n" ) == 0 );
        REQUIRE( r.phse == Approx( 90 ) );
    }
}

/// Verify parseBSWV for PULSE and DC waveforms.
/**
 * \ingroup siglentSDG_unit_test
 */
TEST_CASE( "siglentSDG parseBSWV pulse and DC", "[siglentSDG][parsers]" )
{
    // clang-format off
    #ifdef SIGLENTSDG_TEST_DOXYGEN_REF
    parseBSWV( *(int *)nullptr, *(std::string *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, "" );
    #endif
    // clang-format on

    SECTION( "pulse with width" )
    {
        bswvResult r;
        REQUIRE( r.parse( makeBSWV( "C1", pulseKV() ) ) == 0 );
        REQUIRE( r.channel == 1 );
        REQUIRE( r.wvtp == "PULSE" );
        REQUIRE( r.freq == Approx( 2000 ) );
        REQUIRE( r.peri == Approx( 0.0005 ) );
        REQUIRE( r.amp == Approx( 4 ) );
        REQUIRE( r.ampvrms == Approx( 2 ) );
        REQUIRE( r.ofst == Approx( 2 ) );
        REQUIRE( r.hlev == Approx( 4 ) );
        REQUIRE( r.llev == Approx( 0 ) );
        REQUIRE( r.wdth == Approx( 0.00025 ) );
        REQUIRE( r.phse == Approx( 0 ) ); // no phase for PULSE
    }

    SECTION( "pulse without WIDTH at the expected position" )
    {
        std::vector<std::string> kv = pulseKV();
        kv[18]                      = "WDTH";
        bswvResult r;
        REQUIRE( r.parse( makeBSWV( "C2", kv ) ) == -18 );
        REQUIRE( r.channel == 2 );
        REQUIRE( r.freq == Approx( 2000 ) ); // fields before WIDTH are parsed
        REQUIRE( r.wdth == Approx( 0 ) );
    }

    SECTION( "DC" )
    {
        bswvResult r;
        REQUIRE( r.parse( "C1:BSWV WVTP,DC,OFST,1.5V" ) == 0 );
        REQUIRE( r.channel == 1 );
        REQUIRE( r.wvtp == "DC" );
        REQUIRE( r.ofst == Approx( 1.5 ) );

        // everything else is reset to 0
        REQUIRE( r.freq == 0 );
        REQUIRE( r.peri == 0 );
        REQUIRE( r.amp == 0 );
        REQUIRE( r.ampvrms == 0 );
        REQUIRE( r.hlev == 0 );
        REQUIRE( r.llev == 0 );
        REQUIRE( r.phse == 0 );
        REQUIRE( r.wdth == 0 );
    }

    SECTION( "DC with extra fields" )
    {
        bswvResult r;
        REQUIRE( r.parse( "C2:BSWV WVTP,DC,OFST,-3V,AMP,0V\n" ) == 0 );
        REQUIRE( r.channel == 2 );
        REQUIRE( r.ofst == Approx( -3 ) );
        REQUIRE( r.amp == 0 );
    }

    SECTION( "DC errors" )
    {
        bswvResult r;
        REQUIRE( r.parse( "C1:BSWV WVTP,DC,OFST" ) == -7 );
        REQUIRE( r.parse( "C1:BSWV WVTP,DC" ) == -7 );
        REQUIRE( r.parse( "C1:BSWV WVTP,DC,AMP,1V" ) == -8 );
        REQUIRE( r.ofst == 0 );
    }
}

/// Verify the parseBSWV error codes.
/**
 * \ingroup siglentSDG_unit_test
 */
TEST_CASE( "siglentSDG parseBSWV errors", "[siglentSDG][parsers]" )
{
    // clang-format off
    #ifdef SIGLENTSDG_TEST_DOXYGEN_REF
    parseBSWV( *(int *)nullptr, *(std::string *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, *(double *)nullptr, "" );
    #endif
    // clang-format on

    SECTION( "header errors" )
    {
        bswvResult r;
        REQUIRE( r.parse( "" ) == -1 );
        REQUIRE( r.channel == 0 ); // outputs are reset first
        REQUIRE( r.freq == 0 );

        REQUIRE( r.parse( "C1:BSWV WVTP" ) == -1 );
        REQUIRE( r.parse( "C1:OUTP WVTP,SINE" ) == -2 );
        REQUIRE( r.parse( "X1:BSWV WVTP,SINE" ) == -3 );
        REQUIRE( r.parse( "C:BSWV WVTP,SINE" ) == -4 );
        REQUIRE( r.parse( "C1:BSWV WVTX,SINE" ) == -5 );
        REQUIRE( r.channel == 1 );
    }

    SECTION( "unsupported waveform types" )
    {
        std::vector<std::string> types = { "SQUARE", "RAMP", "NOISE", "ARB", "sine" };
        for( size_t n = 0; n < types.size(); ++n )
        {
            std::vector<std::string> kv = sineKV();
            kv[1]                       = types[n];
            bswvResult r;
            REQUIRE( r.parse( makeBSWV( "C1", kv ) ) == SDG_PARSEERR_WVTP );
            REQUIRE( r.wvtp == types[n] );
        }
        REQUIRE( SDG_PARSEERR_WVTP == -6 );
    }

    SECTION( "too few fields for SINE or PULSE" )
    {
        bswvResult r;
        REQUIRE( r.parse( "C1:BSWV WVTP,SINE,FRQ,10HZ" ) == -9 );
        REQUIRE( r.parse( "C1:BSWV WVTP,PULSE,FRQ,10HZ,PERI,0.1S" ) == -9 );

        std::vector<std::string> kv = sineKV();
        kv.pop_back(); // 19 tokens
        REQUIRE( r.parse( makeBSWV( "C1", kv ) ) == -9 );
        REQUIRE( r.freq == 0 );
    }

    SECTION( "wrong field names" )
    {
        // index into sineKV() of each key, and the expected error code
        std::vector<size_t> keyIdx = { 2, 4, 6, 8, 10, 12, 14, 16 };
        std::vector<int>    codes  = { -10, -11, -12, -13, -14, -15, -16, -17 };

        for( size_t n = 0; n < keyIdx.size(); ++n )
        {
            std::vector<std::string> kv = sineKV();
            kv[keyIdx[n]]               = "XXXX";
            bswvResult r;
            REQUIRE( r.parse( makeBSWV( "C1", kv ) ) == codes[n] );

            // fields before the bad one are parsed
            if( n > 0 )
            {
                REQUIRE( r.freq == Approx( 1000.5 ) );
            }
            else
            {
                REQUIRE( r.freq == 0 );
            }
        }
    }

    SECTION( "values after a bad field are not parsed" )
    {
        std::vector<std::string> kv = sineKV();
        kv[12]                      = "HLVL"; // the HLEV key
        bswvResult r;
        REQUIRE( r.parse( makeBSWV( "C1", kv ) ) == -15 );
        REQUIRE( r.ampvrms == Approx( 0.1768 ) );
        REQUIRE( r.ofst == Approx( -0.25 ) );
        REQUIRE( r.hlev == 0 );
        REQUIRE( r.llev == 0 );
    }
}

/// Verify the STATE parsers (parseMDWV, parseSWWV, parseBTWV) on valid and invalid responses.
/**
 * \ingroup siglentSDG_unit_test
 */
TEST_CASE( "siglentSDG STATE parsers", "[siglentSDG][parsers]" )
{
    // clang-format off
    #ifdef SIGLENTSDG_TEST_DOXYGEN_REF
    parseMDWV( *(int *)nullptr, *(std::string *)nullptr, "" );
    parseSWWV( *(int *)nullptr, *(std::string *)nullptr, "" );
    parseBTWV( *(int *)nullptr, *(std::string *)nullptr, "" );
    #endif
    // clang-format on

    std::vector<std::string>  cmds    = { "MDWV", "SWWV", "BTWV" };
    std::vector<stateParserT> parsers = { parseMDWV, parseSWWV, parseBTWV };

    for( size_t n = 0; n < cmds.size(); ++n )
    {
        INFO( cmds[n] );
        stateParserT parse = parsers[n];
        std::string  cmd   = cmds[n];
        std::string  other = cmds[( n + 1 ) % cmds.size()];

        int         channel = -10;
        std::string state;

        // valid
        REQUIRE( parse( channel, state, "C1:" + cmd + " STATE,OFF" ) == 0 );
        REQUIRE( channel == 1 );
        REQUIRE( state == "OFF" );

        REQUIRE( parse( channel, state, "C2:" + cmd + " STATE,ON,TIME,1S,START,100HZ\n" ) == 0 );
        REQUIRE( channel == 2 );
        REQUIRE( state == "ON" ); // the rest is ignored

        // the state value is not validated
        REQUIRE( parse( channel, state, "C1:" + cmd + " STATE,MAYBE" ) == 0 );
        REQUIRE( state == "MAYBE" );

        // no channel number parses as channel 0
        REQUIRE( parse( channel, state, "C:" + cmd + " STATE,ON" ) == 0 );
        REQUIRE( channel == 0 );

        // errors; the state is left unchanged and the channel is reset to 0
        state   = "prev";
        channel = -10;
        REQUIRE( parse( channel, state, "" ) == -1 );
        REQUIRE( channel == 0 );
        REQUIRE( parse( channel, state, "C1:" + cmd + " STATE" ) == -1 );
        REQUIRE( parse( channel, state, "C1:" + other + " STATE,OFF" ) == -2 );
        REQUIRE( parse( channel, state, "X1:" + cmd + " STATE,OFF" ) == -3 );
        REQUIRE( parse( channel, state, "C1:" + cmd + " STAT,OFF" ) == -4 );
        REQUIRE( channel == 1 );
        REQUIRE( state == "prev" );
    }
}

/// Verify parseARWV on valid and invalid responses.
/**
 * \ingroup siglentSDG_unit_test
 */
TEST_CASE( "siglentSDG parseARWV", "[siglentSDG][parsers]" )
{
    // clang-format off
    #ifdef SIGLENTSDG_TEST_DOXYGEN_REF
    parseARWV( *(int *)nullptr, *(int *)nullptr, "" );
    #endif
    // clang-format on

    int channel = -10;
    int index   = -10;

    SECTION( "valid" )
    {
        REQUIRE( parseARWV( channel, index, "C1:ARWV INDEX,0,NAME," ) == 0 );
        REQUIRE( channel == 1 );
        REQUIRE( index == 0 );

        REQUIRE( parseARWV( channel, index, "C2:ARWV INDEX,3,NAME,wave3\n" ) == 0 );
        REQUIRE( channel == 2 );
        REQUIRE( index == 3 );

        REQUIRE( parseARWV( channel, index, "C1:ARWV INDEX,12" ) == 0 );
        REQUIRE( index == 12 );
    }

    SECTION( "errors reset the index to -1" )
    {
        REQUIRE( parseARWV( channel, index, "" ) == -1 );
        REQUIRE( channel == 0 );
        REQUIRE( index == -1 );

        REQUIRE( parseARWV( channel, index, "C1:ARWV INDEX" ) == -1 );
        REQUIRE( parseARWV( channel, index, "C1:BTWV INDEX,0" ) == -2 );
        REQUIRE( parseARWV( channel, index, "X1:ARWV INDEX,0" ) == -3 );
        REQUIRE( parseARWV( channel, index, "C2:ARWV NAME,0" ) == -4 );
        REQUIRE( channel == 2 );
        REQUIRE( index == -1 );
    }
}

/// Verify parseSYNC on valid and invalid responses.
/**
 * \ingroup siglentSDG_unit_test
 */
TEST_CASE( "siglentSDG parseSYNC", "[siglentSDG][parsers]" )
{
    // clang-format off
    #ifdef SIGLENTSDG_TEST_DOXYGEN_REF
    parseSYNC( *(int *)nullptr, *(bool *)nullptr, "" );
    #endif
    // clang-format on

    int  channel = -10;
    bool sync    = false;

    SECTION( "valid" )
    {
        REQUIRE( parseSYNC( channel, sync, "C1:SYNC ON" ) == 0 );
        REQUIRE( channel == 1 );
        REQUIRE( sync == true );

        REQUIRE( parseSYNC( channel, sync, "C2:SYNC OFF" ) == 0 );
        REQUIRE( channel == 2 );
        REQUIRE( sync == false );

        REQUIRE( parseSYNC( channel, sync, "C1:SYNC ON,TYPE,CH1\n" ) == 0 );
        REQUIRE( channel == 1 );
        REQUIRE( sync == true );
    }

    SECTION( "errors reset sync to false" )
    {
        sync = true;
        REQUIRE( parseSYNC( channel, sync, "" ) == -1 );
        REQUIRE( sync == false );
        REQUIRE( channel == 0 );

        sync = true;
        REQUIRE( parseSYNC( channel, sync, "C1:SYNC" ) == -1 );
        REQUIRE( sync == false );

        REQUIRE( parseSYNC( channel, sync, "C1:OUTP ON" ) == -2 );
        REQUIRE( parseSYNC( channel, sync, "X1:SYNC ON" ) == -3 );
        REQUIRE( channel == 0 );

        sync = true;
        REQUIRE( parseSYNC( channel, sync, "C2:SYNC MAYBE" ) == -4 );
        REQUIRE( channel == 2 );
        REQUIRE( sync == false );

        REQUIRE( parseSYNC( channel, sync, "C1:SYNC on" ) == -4 );
    }
}

} // namespace siglentSDGTest

} // namespace libXWCTest
