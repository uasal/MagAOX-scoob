/** \file indiTSAccumulator_test.cpp
 * \brief Catch2 tests for the indiTSAccumulator app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup indiTSAccumulator_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "../indiTSAccumulator.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup indiTSAccumulator_unit_test indiTSAccumulator Unit Tests
 * \brief Unit tests for the indiTSAccumulator application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `indiTSAccumulator` unit tests.
/** \ingroup indiTSAccumulator_unit_test
 */
namespace indiTSAccumulatorTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// An in-process stand-in for a circular-buffer ImageStreamIO stream, with no shared memory or semaphores.
struct fakeStream
{
    IMAGE m_image; ///< The image structure handed to the app.

    IMAGE_METADATA m_md; ///< The metadata pointed to by `m_image.md`.

    std::vector<float> m_data; ///< Backing storage for `m_image.array.F`.

    std::vector<uint64_t> m_cnt; ///< Backing storage for `m_image.cntarray`.

    std::vector<timespec> m_atime; ///< Backing storage for `m_image.atimearray`.

    std::vector<timespec> m_wtime; ///< Backing storage for `m_image.writetimearray`.

    /// Build a stream with `depth` slices, initialized as `appStartup()` initializes a new stream.
    explicit fakeStream( uint32_t depth /**< [in] number of slices in the circular buffer */ )
        : m_data( depth, 0.0f ), m_cnt( depth, std::numeric_limits<uint64_t>::max() ), m_atime( depth, { 0, 0 } ),
          m_wtime( depth, { 0, 0 } )
    {
        std::memset( &m_image, 0, sizeof( m_image ) );
        std::memset( &m_md, 0, sizeof( m_md ) );

        m_md.size[0] = 1;
        m_md.size[1] = 1;
        m_md.size[2] = depth;
        m_md.cnt1    = depth - 1;
        m_md.sem     = 0; // no semaphores, so ImageStreamIO_sempost() is a no-op

        m_image.md             = &m_md;
        m_image.semlog         = nullptr;
        m_image.array.F        = m_data.data();
        m_image.cntarray       = m_cnt.data();
        m_image.atimearray     = m_atime.data();
        m_image.writetimearray = m_wtime.data();
    }

    fakeStream( const fakeStream & ) = delete;

    fakeStream &operator=( const fakeStream & ) = delete;
};

/// Test harness exposing indiTSAccumulator internals.
class indiTSAccumulator_test : public indiTSAccumulator
{
  public:
    /// Construct a harness with the given device name.
    explicit indiTSAccumulator_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
    }

    using indiTSAccumulator::m_maxEntries;
    using indiTSAccumulator::m_properties;
    using indiTSAccumulator::st_setCallBack_all;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfigImpl()`.
    /**
     * \returns the value returned by `loadConfigImpl()`
     */
    int configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        return loadConfigImpl( config );
    }

    /// Run `setupConfig()`, read a configuration file, and run the virtual `loadConfig()`.
    void configureVirtual( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Attach a fake stream to an element, in place of the shared memory created by `appStartup()`.
    /**
     * \returns a pointer to the fake stream, owned by the harness
     */
    fakeStream *attach( const std::string &key, /**< [in] the `device.property` key */
                        size_t             n,   /**< [in] the element index within the property */
                        uint32_t           depth /**< [in] number of slices in the circular buffer */ )
    {
        m_streams.push_back( std::make_unique<fakeStream>( depth ) );
        m_properties.at( key ).m_elements.at( n ).m_imageStream = &m_streams.back()->m_image;
        return m_streams.back().get();
    }

    /// Get the name of an element of a configured property.
    /**
     * \returns the element name
     */
    std::string elementName( const std::string &key, /**< [in] the `device.property` key */
                             size_t             n /**< [in] the element index within the property */ )
    {
        return m_properties.at( key ).m_elements.at( n ).m_name;
    }

    /// Get the number of elements of a configured property.
    /**
     * \returns the number of elements
     */
    size_t elementCount( const std::string &key /**< [in] the `device.property` key */ )
    {
        return m_properties.at( key ).m_elements.size();
    }

  private:
    std::vector<std::unique_ptr<fakeStream>> m_streams; ///< Fake streams owned by the harness.
};

/// Build a Number property update with one element at a given timestamp.
/**
 * \returns the INDI property
 */
pcf::IndiProperty numberUpdate( const std::string &device, /**< [in] INDI device name */
                                const std::string &name,   /**< [in] INDI property name */
                                const std::string &el,     /**< [in] element name */
                                const std::string &value,  /**< [in] element value as text */
                                long               sec,    /**< [in] timestamp seconds */
                                long               usec    /**< [in] timestamp microseconds */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el ) );
    ip[el].setValue( value );

    timeval tv;
    tv.tv_sec  = sec;
    tv.tv_usec = usec;
    ip.setTimeStamp( pcf::TimeStamp( tv ) );

    return ip;
}
/// \endcond

/// Verify element lists are parsed into properties and elements.
/**
 * \ingroup indiTSAccumulator_unit_test
 */
TEST_CASE( "indiTSAccumulator parses the element list", "[indiTSAccumulator]" )
{
    // clang-format off
    #ifdef INDITSACCUMULATOR_TEST_DOXYGEN_REF
    indiTSAccumulator::setupConfig();
    indiTSAccumulator::loadConfigImpl( mx::app::appConfigurator() );
    indiTSAccumulator::loadConfig();
    #endif
    // clang-format on

    const std::string fname = "/tmp/indiTSAccumulator_test_parse.conf";
    mx::app::writeConfigFile(
        fname, { "" }, { "elements" }, { "tcsi.teldata.zd,tcsi.teldata.pa,dev2.temps.t1,dev3.prop.el.sub" } );

    indiTSAccumulator_test app( "tsacc" );
    REQUIRE( app.configure( fname ) == 0 );
    REQUIRE( app.shutdown() == 0 );

    REQUIRE( app.m_properties.size() == 3 );

    REQUIRE( app.m_properties.count( "tcsi.teldata" ) == 1 );
    REQUIRE( app.m_properties.at( "tcsi.teldata" ).m_property.getDevice() == "tcsi" );
    REQUIRE( app.m_properties.at( "tcsi.teldata" ).m_property.getName() == "teldata" );
    REQUIRE( app.elementCount( "tcsi.teldata" ) == 2 );
    REQUIRE( app.elementName( "tcsi.teldata", 0 ) == "zd" );
    REQUIRE( app.elementName( "tcsi.teldata", 1 ) == "pa" );

    REQUIRE( app.elementCount( "dev2.temps" ) == 1 );
    REQUIRE( app.elementName( "dev2.temps", 0 ) == "t1" );

    // Everything after the second '.' is the element name.
    REQUIRE( app.elementCount( "dev3.prop" ) == 1 );
    REQUIRE( app.elementName( "dev3.prop", 0 ) == "el.sub" );

    REQUIRE( app.m_properties.at( "dev2.temps" ).m_elements[0].m_imageStream == nullptr );
    REQUIRE( app.m_maxEntries == 36000 );

    std::remove( fname.c_str() );
}

/// Verify a configuration with no elements is rejected and requests shutdown.
/**
 * \ingroup indiTSAccumulator_unit_test
 */
TEST_CASE( "indiTSAccumulator rejects an empty element list", "[indiTSAccumulator]" )
{
    // clang-format off
    #ifdef INDITSACCUMULATOR_TEST_DOXYGEN_REF
    indiTSAccumulator::loadConfigImpl( mx::app::appConfigurator() );
    indiTSAccumulator::loadConfig();
    #endif
    // clang-format on

    const std::string fname = "/tmp/indiTSAccumulator_test_empty.conf";
    mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );

    SECTION( "loadConfigImpl returns an error" )
    {
        indiTSAccumulator_test app( "tsacc" );
        REQUIRE( app.configure( fname ) == -1 );
        REQUIRE( app.shutdown() == 1 );
        REQUIRE( app.m_properties.empty() );
    }

    SECTION( "loadConfig requests shutdown" )
    {
        indiTSAccumulator_test app( "tsacc" );
        app.configureVirtual( fname );
        REQUIRE( app.shutdown() == 1 );
    }

    std::remove( fname.c_str() );
}

/// Verify malformed element specifications are rejected.
/**
 * \ingroup indiTSAccumulator_unit_test
 */
TEST_CASE( "indiTSAccumulator rejects malformed element specifications", "[indiTSAccumulator]" )
{
    // clang-format off
    #ifdef INDITSACCUMULATOR_TEST_DOXYGEN_REF
    indiTSAccumulator::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/indiTSAccumulator_test_malformed.conf";

    SECTION( "no '.' at all" )
    {
        mx::app::writeConfigFile( fname, { "" }, { "elements" }, { "tcsi" } );

        indiTSAccumulator_test app( "tsacc" );
        REQUIRE( app.configure( fname ) == -1 );
        REQUIRE( app.shutdown() == 1 );
    }

    SECTION( "only one '.'" )
    {
        mx::app::writeConfigFile( fname, { "" }, { "elements" }, { "tcsi.teldata" } );

        indiTSAccumulator_test app( "tsacc" );
        REQUIRE( app.configure( fname ) == -1 );
        REQUIRE( app.shutdown() == 1 );
    }

    SECTION( "a bad entry after good ones stops parsing" )
    {
        mx::app::writeConfigFile( fname, { "" }, { "elements" }, { "tcsi.teldata.zd,bad" } );

        indiTSAccumulator_test app( "tsacc" );
        REQUIRE( app.configure( fname ) == -1 );
        REQUIRE( app.shutdown() == 1 );
        REQUIRE( app.elementCount( "tcsi.teldata" ) == 1 );
    }

    std::remove( fname.c_str() );
}

/// Verify a Number update is appended to the element's circular buffer.
/**
 * \ingroup indiTSAccumulator_unit_test
 */
TEST_CASE( "indiTSAccumulator setCallBack_all appends a new sample", "[indiTSAccumulator]" )
{
    // clang-format off
    #ifdef INDITSACCUMULATOR_TEST_DOXYGEN_REF
    indiTSAccumulator::setCallBack_all( pcf::IndiProperty() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/indiTSAccumulator_test_append.conf";
    mx::app::writeConfigFile( fname, { "" }, { "elements" }, { "tcsi.teldata.zd,tcsi.teldata.pa" } );

    indiTSAccumulator_test app( "tsacc" );
    REQUIRE( app.configure( fname ) == 0 );
    std::remove( fname.c_str() );

    fakeStream *zd = app.attach( "tcsi.teldata", 0, 4 );
    fakeStream *pa = app.attach( "tcsi.teldata", 1, 4 );

    REQUIRE( app.setCallBack_all( numberUpdate( "tcsi", "teldata", "zd", "12.5", 1000, 250 ) ) == 0 );

    REQUIRE( zd->m_md.cnt0 == 1 );
    REQUIRE( zd->m_md.cnt1 == 0 );
    REQUIRE( zd->m_data[0] == Approx( 12.5 ) );
    REQUIRE( zd->m_cnt[0] == 1 );
    REQUIRE( zd->m_md.atime.tv_sec == 1000 );
    REQUIRE( zd->m_md.atime.tv_nsec == 250000 );
    REQUIRE( zd->m_atime[0].tv_sec == 1000 );
    REQUIRE( zd->m_atime[0].tv_nsec == 250000 );
    REQUIRE( zd->m_wtime[0].tv_sec > 0 );
    REQUIRE( zd->m_md.write == 0 );

    // The element not present in the update is untouched.
    REQUIRE( pa->m_md.cnt0 == 0 );
    REQUIRE( pa->m_md.cnt1 == 3 );
}

/// Verify repeated updates with the same timestamp are ignored.
/**
 * \ingroup indiTSAccumulator_unit_test
 */
TEST_CASE( "indiTSAccumulator setCallBack_all ignores repeated timestamps", "[indiTSAccumulator]" )
{
    // clang-format off
    #ifdef INDITSACCUMULATOR_TEST_DOXYGEN_REF
    indiTSAccumulator::setCallBack_all( pcf::IndiProperty() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/indiTSAccumulator_test_repeat.conf";
    mx::app::writeConfigFile( fname, { "" }, { "elements" }, { "dev.prop.el" } );

    indiTSAccumulator_test app( "tsacc" );
    REQUIRE( app.configure( fname ) == 0 );
    std::remove( fname.c_str() );

    fakeStream *s = app.attach( "dev.prop", 0, 4 );

    REQUIRE( app.setCallBack_all( numberUpdate( "dev", "prop", "el", "1", 50, 0 ) ) == 0 );
    REQUIRE( app.setCallBack_all( numberUpdate( "dev", "prop", "el", "2", 50, 0 ) ) == 0 );

    REQUIRE( s->m_md.cnt0 == 1 );
    REQUIRE( s->m_data[0] == Approx( 1.0 ) );
    REQUIRE( s->m_data[1] == Approx( 0.0 ) );

    // A change in only the microseconds is a new sample.
    REQUIRE( app.setCallBack_all( numberUpdate( "dev", "prop", "el", "3", 50, 1 ) ) == 0 );
    REQUIRE( s->m_md.cnt0 == 2 );
    REQUIRE( s->m_md.cnt1 == 1 );
    REQUIRE( s->m_data[1] == Approx( 3.0 ) );
}

/// Verify the circular buffer rolls over at the stream depth.
/**
 * \ingroup indiTSAccumulator_unit_test
 */
TEST_CASE( "indiTSAccumulator setCallBack_all rolls over the circular buffer", "[indiTSAccumulator]" )
{
    // clang-format off
    #ifdef INDITSACCUMULATOR_TEST_DOXYGEN_REF
    indiTSAccumulator::setCallBack_all( pcf::IndiProperty() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/indiTSAccumulator_test_rollover.conf";
    mx::app::writeConfigFile( fname, { "" }, { "elements" }, { "dev.prop.el" } );

    indiTSAccumulator_test app( "tsacc" );
    REQUIRE( app.configure( fname ) == 0 );
    std::remove( fname.c_str() );

    fakeStream *s = app.attach( "dev.prop", 0, 3 );

    for( int k = 0; k < 4; ++k )
    {
        REQUIRE( app.setCallBack_all( numberUpdate( "dev", "prop", "el", std::to_string( 10 + k ), 100 + k, 0 ) ) ==
                 0 );
    }

    REQUIRE( s->m_md.cnt0 == 4 );
    REQUIRE( s->m_md.cnt1 == 0 );
    REQUIRE( s->m_data[0] == Approx( 13.0 ) );
    REQUIRE( s->m_data[1] == Approx( 11.0 ) );
    REQUIRE( s->m_data[2] == Approx( 12.0 ) );
    REQUIRE( s->m_cnt[0] == 4 );
    REQUIRE( s->m_cnt[1] == 2 );
    REQUIRE( s->m_cnt[2] == 3 );
    REQUIRE( s->m_atime[0].tv_sec == 103 );
}

/// Verify non-Number properties are rejected and unknown properties are ignored.
/**
 * \ingroup indiTSAccumulator_unit_test
 */
TEST_CASE( "indiTSAccumulator setCallBack_all rejects unsupported updates", "[indiTSAccumulator]" )
{
    // clang-format off
    #ifdef INDITSACCUMULATOR_TEST_DOXYGEN_REF
    indiTSAccumulator::setCallBack_all( pcf::IndiProperty() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/indiTSAccumulator_test_reject.conf";
    mx::app::writeConfigFile( fname, { "" }, { "elements" }, { "dev.prop.el" } );

    indiTSAccumulator_test app( "tsacc" );
    REQUIRE( app.configure( fname ) == 0 );
    std::remove( fname.c_str() );

    fakeStream *s = app.attach( "dev.prop", 0, 3 );

    SECTION( "a Text property is rejected" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Text );
        ip.setDevice( "dev" );
        ip.setName( "prop" );
        ip.add( pcf::IndiElement( "el" ) );
        ip["el"].setValue( std::string( "1" ) );

        REQUIRE( app.setCallBack_all( ip ) == -1 );
        REQUIRE( s->m_md.cnt0 == 0 );
    }

    SECTION( "an unconfigured property is ignored" )
    {
        REQUIRE( app.setCallBack_all( numberUpdate( "dev", "other", "el", "1", 5, 0 ) ) == 0 );
        REQUIRE( app.setCallBack_all( numberUpdate( "devx", "prop", "el", "1", 5, 0 ) ) == 0 );
        REQUIRE( s->m_md.cnt0 == 0 );
    }

    SECTION( "an update without a configured element is ignored" )
    {
        REQUIRE( app.setCallBack_all( numberUpdate( "dev", "prop", "notel", "1", 5, 0 ) ) == 0 );
        REQUIRE( s->m_md.cnt0 == 0 );
    }
}

/// Verify an element without a stream is skipped while other elements are still recorded.
/**
 * \ingroup indiTSAccumulator_unit_test
 */
TEST_CASE( "indiTSAccumulator setCallBack_all skips elements without a stream", "[indiTSAccumulator]" )
{
    // clang-format off
    #ifdef INDITSACCUMULATOR_TEST_DOXYGEN_REF
    indiTSAccumulator::setCallBack_all( pcf::IndiProperty() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/indiTSAccumulator_test_nullstream.conf";
    mx::app::writeConfigFile( fname, { "" }, { "elements" }, { "dev.prop.a,dev.prop.b" } );

    indiTSAccumulator_test app( "tsacc" );
    REQUIRE( app.configure( fname ) == 0 );
    std::remove( fname.c_str() );

    // Only element `b` gets a stream.
    fakeStream *b = app.attach( "dev.prop", 1, 3 );

    pcf::IndiProperty ip = numberUpdate( "dev", "prop", "a", "1", 7, 0 );
    ip.add( pcf::IndiElement( "b" ) );
    ip["b"].setValue( std::string( "2.25" ) );

    REQUIRE( app.setCallBack_all( ip ) == 0 );
    REQUIRE( b->m_md.cnt0 == 1 );
    REQUIRE( b->m_data[0] == Approx( 2.25 ) );
}

/// Verify the static callback wrapper forwards to `setCallBack_all()`.
/**
 * \ingroup indiTSAccumulator_unit_test
 */
TEST_CASE( "indiTSAccumulator static callback wrapper forwards updates", "[indiTSAccumulator]" )
{
    // clang-format off
    #ifdef INDITSACCUMULATOR_TEST_DOXYGEN_REF
    indiTSAccumulator::st_setCallBack_all( nullptr, pcf::IndiProperty() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/indiTSAccumulator_test_static.conf";
    mx::app::writeConfigFile( fname, { "" }, { "elements" }, { "dev.prop.el" } );

    indiTSAccumulator_test app( "tsacc" );
    REQUIRE( app.configure( fname ) == 0 );
    std::remove( fname.c_str() );

    fakeStream *s = app.attach( "dev.prop", 0, 3 );

    indiTSAccumulator *base = &app;
    REQUIRE( indiTSAccumulator_test::st_setCallBack_all( base, numberUpdate( "dev", "prop", "el", "4", 9, 0 ) ) == 0 );
    REQUIRE( s->m_md.cnt0 == 1 );
    REQUIRE( s->m_data[0] == Approx( 4.0 ) );
}

/// Verify `appLogic()` and `appShutdown()` are no-ops that succeed.
/**
 * \ingroup indiTSAccumulator_unit_test
 */
TEST_CASE( "indiTSAccumulator appLogic and appShutdown succeed", "[indiTSAccumulator]" )
{
    // clang-format off
    #ifdef INDITSACCUMULATOR_TEST_DOXYGEN_REF
    indiTSAccumulator::appLogic();
    indiTSAccumulator::appShutdown();
    #endif
    // clang-format on

    indiTSAccumulator_test app( "tsacc" );
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.appShutdown() == 0 );
}

} // namespace indiTSAccumulatorTest

} // namespace libXWCTest
