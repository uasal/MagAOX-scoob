/** \file dlDataCollection_test.cpp
 * \brief Catch2 tests for the dlDataCollection app.
 * \author Claude Code
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../dlDataCollection.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup dlDataCollection_unit_test dlDataCollection Unit Tests
 * \brief Unit tests for the dlDataCollection application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `dlDataCollection` unit tests.
/** \ingroup dlDataCollection_unit_test
 */
namespace dlDataCollectionTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing dlDataCollection internals, with an in-memory modeval stream.
class dlDataCollection_test : public dlDataCollection
{
  public:
    /// Metadata for the in-memory modeval stream (no shared memory, no semaphores).
    IMAGE_METADATA m_testMd;

    /// Pixel buffer for the in-memory modeval stream.
    std::vector<float> m_testData;

    /// Construct a harness with the given device name.
    explicit dlDataCollection_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
        memset( &m_testMd, 0, sizeof( m_testMd ) );
        memset( &m_modevalStream, 0, sizeof( m_modevalStream ) );
    }

    /// Free the buffers the app allocates but never frees in its destructor.
    ~dlDataCollection_test() noexcept
    {
        delete[] pp_image;
        delete[] modeval;
        delete[] randomAmps;
        delete imagebuffer;
    }

    /// Run setupConfig(), read the given config file, and run loadConfig().
    void configure( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
    }

    /// Point the modeval stream at an in-memory float buffer of the given size.
    void attachStream( size_t n /**< [in] the number of floats in the stream */ )
    {
        m_testData.assign( n, -1.0f );
        m_testMd.size[0]          = n;
        m_testMd.size[1]          = 1;
        m_testMd.sem              = 0;
        m_modevalStream.md        = &m_testMd;
        m_modevalStream.array.raw = m_testData.data();
    }

    /// Forget the buffers freed by appShutdown(), which does not reset the pointers.
    void forgetFreedBuffers()
    {
        pp_image   = nullptr;
        modeval    = nullptr;
        randomAmps = nullptr;
    }

    using dlDataCollection::allocate;
    using dlDataCollection::processImage;

    using dlDataCollection::ampsDir;
    using dlDataCollection::dataDirs;
    using dlDataCollection::dataset_i;
    using dlDataCollection::frame_counter;
    using dlDataCollection::frame_saved;
    using dlDataCollection::frame_wait;
    using dlDataCollection::imagebuffer;
    using dlDataCollection::imageNorm;
    using dlDataCollection::m_modevalChannel;
    using dlDataCollection::m_modevalOpened;
    using dlDataCollection::m_modevalStream;
    using dlDataCollection::m_pupPix;
    using dlDataCollection::m_pwfsHeight;
    using dlDataCollection::m_pwfsWidth;
    using dlDataCollection::m_shaped_command;
    using dlDataCollection::modalNorm;
    using dlDataCollection::modeval;
    using dlDataCollection::Nact_across;
    using dlDataCollection::Nmodes;
    using dlDataCollection::Nperset;
    using dlDataCollection::Npup;
    using dlDataCollection::Nset;
    using dlDataCollection::NumFrameSkip;
    using dlDataCollection::pixels_per_quadrant;
    using dlDataCollection::pp_image;
    using dlDataCollection::pup_offset1_x;
    using dlDataCollection::pup_offset1_y;
    using dlDataCollection::pup_offset2_x;
    using dlDataCollection::pup_offset2_y;
    using dlDataCollection::randomAmps;
    using dlDataCollection::zeroPad;

    using dev::shmimMonitor<dlDataCollection>::m_height;
    using dev::shmimMonitor<dlDataCollection>::m_shmimName;
    using dev::shmimMonitor<dlDataCollection>::m_width;
};

/// \endcond

/// Write a text file with one value per line.
void writeLines( const std::string        &file, /**< [in] the file path */
                 const std::vector<float> &values /**< [in] the values to write, one per line */ )
{
    std::ofstream fout( file );
    for( size_t n = 0; n < values.size(); ++n )
    {
        fout << values[n] << "\n";
    }
}

/// Read a binary file of floats.
/**
 * \returns the contents of the file as floats
 */
std::vector<float> readFloats( const std::string &file /**< [in] the file path */ )
{
    std::ifstream      fin( file, std::ios::binary | std::ios::ate );
    std::vector<float> out;
    if( !fin )
    {
        return out;
    }
    std::streamsize sz = fin.tellg();
    fin.seekg( 0 );
    out.resize( sz / sizeof( float ) );
    fin.read( reinterpret_cast<char *>( out.data() ), out.size() * sizeof( float ) );
    return out;
}

/// Prefix of the random-amplitude files used by the pipeline tests.
const std::string ampsPrefix = "/tmp/dlDataCollection_test_amps_";

/// Prefix of the image dataset files written by the pipeline tests.
const std::string dataPrefix = "/tmp/dlDataCollection_test_data_";

/// Write the random-amplitude file for a dataset, with values `100*dataset + k`.
void writeAmps( int dataset, /**< [in] the dataset index */
                int n /**< [in] the number of values */ )
{
    std::vector<float> v;
    for( int k = 0; k < n; ++k )
    {
        v.push_back( 100.0f * dataset + k );
    }
    writeLines( ampsPrefix + "modeval_dataset_" + std::to_string( dataset ) + ".csv", v );
}

/// Configure a harness for a small 16x16 WFS, 6x6 padded pupils and 4 modes, and run allocate().
/** The modeval channel does not exist, so allocate() returns -1 after allocating the buffers.  The in-memory
 * modeval stream is then attached.  Random amplitudes for datasets 0 and 1 are written.
 *
 * \returns the allocate() return value
 */
int setupPipeline( dlDataCollection_test &app,     /**< [in/out] the harness to set up */
                   int                    nperset, /**< [in] images per dataset */
                   int                    nset,    /**< [in] number of datasets */
                   int                    skip /**< [in] NumFrameSkip */ )
{
    app.m_width          = 16;
    app.m_height         = 16;
    app.m_pupPix         = 6;
    app.zeroPad          = 2;
    app.pup_offset1_x    = 1;
    app.pup_offset1_y    = 2;
    app.pup_offset2_x    = 9;
    app.pup_offset2_y    = 10;
    app.imageNorm        = 0.5f;
    app.modalNorm        = 1.0f;
    app.Nmodes           = 4;
    app.Nact_across      = 2; // the shaped command must not read past the Nmodes modeval values
    app.Nperset          = nperset;
    app.Nset             = nset;
    app.NumFrameSkip     = skip;
    app.ampsDir          = ampsPrefix;
    app.dataDirs         = dataPrefix;
    app.m_modevalChannel = "dlDataCollection_test_no_such_stream";

    writeAmps( 0, nperset * 4 );
    writeAmps( 1, nperset * 4 );

    int rv = app.allocate( dev::shmimT() );
    app.attachStream( 4 );
    return rv;
}

/// Remove the files written by setupPipeline() and the pipeline.
void cleanupPipeline()
{
    for( int d = 0; d < 3; ++d )
    {
        remove( ( ampsPrefix + "modeval_dataset_" + std::to_string( d ) + ".csv" ).c_str() );
        remove( ( dataPrefix + "images_dataset_" + std::to_string( d ) + ".bin" ).c_str() );
    }
}

/// Make a 16x16 frame with value `i + offset` at linear index i.
/**
 * \returns the frame
 */
std::vector<float> makeFrame( float offset /**< [in] value added to every pixel */ )
{
    std::vector<float> f( 16 * 16 );
    for( size_t i = 0; i < f.size(); ++i )
    {
        f[i] = i + offset;
    }
    return f;
}

/// Compute the expected preprocessed image for a 16x16 frame from makeFrame(), matching setupPipeline().
/** The 6x6 pupils have a 2 pixel zero pad, so only a 2x2 region is copied from each pupil.  Within a pupil the
 * loop index is `ki = (col + 2) * 6 + (row + 2)`, and the frame is read as `frame[row + col * 16]`.
 *
 * \returns the 4 x 36 preprocessed image
 */
std::vector<float> expectedPP( const std::vector<float> &frame /**< [in] the 16x16 frame */ )
{
    std::vector<float> pp( 4 * 36, 0.0f );
    const int          offx[4] = { 1, 9, 1, 9 };
    const int          offy[4] = { 2, 2, 10, 10 };
    for( int q = 0; q < 4; ++q )
    {
        for( int col = 0; col < 2; ++col )
        {
            for( int row = 0; row < 2; ++row )
            {
                int ki          = ( col + 2 ) * 6 + ( row + 2 );
                pp[q * 36 + ki] = 0.5f * frame[( offy[q] + row ) + ( offx[q] + col ) * 16];
            }
        }
    }
    return pp;
}

/// Verify loadCSV reads one value per line and reports errors.
/**
 * \ingroup dlDataCollection_unit_test
 */
TEST_CASE( "dlDataCollection loadCSV", "[dlDataCollection]" )
{
    // clang-format off
    #ifdef DLDATACOLLECTION_TEST_DOXYGEN_REF
    loadCSV("", 0, dest);
    #endif
    // clang-format on

    std::string file = "/tmp/dlDataCollection_test_loadCSV.csv";
    {
        std::ofstream fout( file );
        fout << "1.5\n-2\n3e2\n42\n";
    }

    float  buf[4] = { 0, 0, 0, 0 };
    float *dest   = buf;

    SECTION( "reads the requested number of values" )
    {
        REQUIRE( loadCSV( file, 3, dest ) == true );
        REQUIRE( buf[0] == Approx( 1.5f ) );
        REQUIRE( buf[1] == Approx( -2.0f ) );
        REQUIRE( buf[2] == Approx( 300.0f ) );
        REQUIRE( buf[3] == 0.0f );
        REQUIRE( dest == buf );
    }

    SECTION( "a missing file returns false and leaves the destination unchanged" )
    {
        REQUIRE( loadCSV( "/tmp/dlDataCollection_test_no_such_file.csv", 3, dest ) == false );
        REQUIRE( buf[0] == 0.0f );
    }

    SECTION( "too few lines throws from std::stof" )
    {
        float  big[6];
        float *bigp = big;
        REQUIRE_THROWS_AS( loadCSV( file, 6, bigp ), std::invalid_argument );
    }

    remove( file.c_str() );
}

/// Verify the ImageBuffer ring of images and its binary save.
/**
 * \ingroup dlDataCollection_unit_test
 */
TEST_CASE( "dlDataCollection ImageBuffer", "[dlDataCollection]" )
{
    // clang-format off
    #ifdef DLDATACOLLECTION_TEST_DOXYGEN_REF
    ImageBuffer::ImageBuffer(0,0,0,0);
    ImageBuffer::add(nullptr);
    ImageBuffer::clear();
    ImageBuffer::save("");
    #endif
    // clang-format on

    std::string file = "/tmp/dlDataCollection_test_imagebuffer.bin";

    ImageBuffer ib( 2, 2, 3, 2 ); // 12 floats per image, 2 images

    std::vector<float> a( 12 ), b( 12 ), c( 12 );
    for( int n = 0; n < 12; ++n )
    {
        a[n] = n;
        b[n] = 100 + n;
        c[n] = -n - 1;
    }

    SECTION( "starts zeroed" )
    {
        ib.save( file );
        std::vector<float> out = readFloats( file );
        REQUIRE( out.size() == 24 );
        for( size_t n = 0; n < out.size(); ++n )
        {
            REQUIRE( out[n] == 0.0f );
        }
    }

    SECTION( "add fills slots in order, clear rewinds" )
    {
        ib.add( a.data() );
        ib.add( b.data() );
        ib.save( file );
        std::vector<float> out = readFloats( file );
        REQUIRE( out.size() == 24 );
        for( int n = 0; n < 12; ++n )
        {
            REQUIRE( out[n] == a[n] );
            REQUIRE( out[12 + n] == b[n] );
        }

        // clear does not zero the data, it only rewinds the write position
        ib.clear();
        ib.add( c.data() );
        ib.save( file );
        out = readFloats( file );
        REQUIRE( out.size() == 24 );
        for( int n = 0; n < 12; ++n )
        {
            REQUIRE( out[n] == c[n] );
            REQUIRE( out[12 + n] == b[n] );
        }
    }

    SECTION( "saving to an unwritable path does not throw" )
    {
        REQUIRE_NOTHROW( ib.save( "/nonexistent_dlDataCollection_dir/x.bin" ) );
    }

    remove( file.c_str() );
}

/// Verify configuration defaults and overrides.
/**
 * \ingroup dlDataCollection_unit_test
 */
TEST_CASE( "dlDataCollection configuration", "[dlDataCollection]" )
{
    // clang-format off
    #ifdef DLDATACOLLECTION_TEST_DOXYGEN_REF
    dlDataCollection::dlDataCollection();
    dlDataCollection::setupConfig();
    dlDataCollection::loadConfig();
    dlDataCollection::loadConfigImpl(config);
    #endif
    // clang-format on

    SECTION( "defaults" )
    {
        std::string file = "/tmp/dlDataCollection_test_defaults.conf";
        mx::app::writeConfigFile( file, { "none" }, { "nada" }, { "0" } );

        dlDataCollection_test app( "dlcol" );
        app.configure( file );

        // imageNorm, modalNorm, m_pupPix and the pupil offsets have no default and are not checked.
        REQUIRE( app.dataDirs == "" );
        REQUIRE( app.ampsDir == "" );
        REQUIRE( app.Nperset == 0 );
        REQUIRE( app.Nset == 0 );
        REQUIRE( app.NumFrameSkip == 0 );
        REQUIRE( app.Nmodes == 0 );
        REQUIRE( app.m_modevalChannel == "" );
        REQUIRE( app.m_shmimName == "dlcol" );
        REQUIRE( app.Npup == 4 );
        REQUIRE( app.Nact_across == 50 );
        REQUIRE( app.zeroPad == 2 );
        REQUIRE( app.modeval == nullptr );
        REQUIRE( app.pp_image == nullptr );
        REQUIRE( app.randomAmps == nullptr );
        REQUIRE( app.imagebuffer == nullptr );

        remove( file.c_str() );
    }

    SECTION( "overrides" )
    {
        std::string file = "/tmp/dlDataCollection_test_overrides.conf";
        mx::app::writeConfigFile( file,
                                  { "shmimMonitor",
                                    "parameters",
                                    "parameters",
                                    "parameters",
                                    "parameters",
                                    "parameters",
                                    "parameters",
                                    "parameters",
                                    "parameters",
                                    "parameters",
                                    "parameters",
                                    "parameters",
                                    "parameters",
                                    "parameters",
                                    "parameters" },
                                  { "shmimName",
                                    "dataDirs",
                                    "ampsDir",
                                    "Nperset",
                                    "Nset",
                                    "NumFrameSkip",
                                    "imageNorm",
                                    "modalNorm",
                                    "channel",
                                    "Nmodes",
                                    "m_pupPix",
                                    "pup_offset1_x",
                                    "pup_offset1_y",
                                    "pup_offset2_x",
                                    "pup_offset2_y" },
                                  { "camwfs",
                                    "/data/",
                                    "/amps/",
                                    "100",
                                    "7",
                                    "3",
                                    "0.25",
                                    "2.5",
                                    "aol1_modevalDM",
                                    "2500",
                                    "60",
                                    "11",
                                    "12",
                                    "73",
                                    "74" } );

        dlDataCollection_test app( "dlcol" );
        app.configure( file );

        REQUIRE( app.m_shmimName == "camwfs" );
        REQUIRE( app.dataDirs == "/data/" );
        REQUIRE( app.ampsDir == "/amps/" );
        REQUIRE( app.Nperset == 100 );
        REQUIRE( app.Nset == 7 );
        REQUIRE( app.NumFrameSkip == 3 );
        REQUIRE( app.imageNorm == Approx( 0.25f ) );
        REQUIRE( app.modalNorm == Approx( 2.5f ) );
        REQUIRE( app.m_modevalChannel == "aol1_modevalDM" );
        REQUIRE( app.Nmodes == 2500 );
        REQUIRE( app.m_pupPix == 60 );
        REQUIRE( app.pup_offset1_x == 11 );
        REQUIRE( app.pup_offset1_y == 12 );
        REQUIRE( app.pup_offset2_x == 73 );
        REQUIRE( app.pup_offset2_y == 74 );

        remove( file.c_str() );
    }
}

/// Verify loadRandomAmps reads the dataset file named from ampsDir and the index.
/**
 * \ingroup dlDataCollection_unit_test
 */
TEST_CASE( "dlDataCollection loadRandomAmps", "[dlDataCollection]" )
{
    // clang-format off
    #ifdef DLDATACOLLECTION_TEST_DOXYGEN_REF
    dlDataCollection::loadRandomAmps(0);
    #endif
    // clang-format on

    dlDataCollection_test app( "dlcol" );
    app.ampsDir    = ampsPrefix;
    app.Nperset    = 2;
    app.Nmodes     = 3;
    app.randomAmps = new float[6];
    for( int n = 0; n < 6; ++n )
    {
        app.randomAmps[n] = -1;
    }

    writeLines( ampsPrefix + "modeval_dataset_3.csv", { 1, 2, 3, 4, 5, 6, 7 } );

    SECTION( "reads Nperset*Nmodes values from modeval_dataset_<n>.csv" )
    {
        app.loadRandomAmps( 3 );
        for( int n = 0; n < 6; ++n )
        {
            REQUIRE( app.randomAmps[n] == Approx( n + 1 ) );
        }
    }

    SECTION( "a missing dataset file leaves the amplitudes unchanged" )
    {
        app.loadRandomAmps( 4 );
        for( int n = 0; n < 6; ++n )
        {
            REQUIRE( app.randomAmps[n] == -1 );
        }
    }

    remove( ( ampsPrefix + "modeval_dataset_3.csv" ).c_str() );
}

/// Verify allocate() sizes the buffers from the stream and config, and fails when the modeval channel is missing.
/**
 * \ingroup dlDataCollection_unit_test
 */
TEST_CASE( "dlDataCollection allocate", "[dlDataCollection]" )
{
    // clang-format off
    #ifdef DLDATACOLLECTION_TEST_DOXYGEN_REF
    dlDataCollection::allocate(dev::shmimT());
    #endif
    // clang-format on

    dlDataCollection_test app( "dlcol" );
    REQUIRE( setupPipeline( app, 2, 5, 1 ) == -1 ); // modeval channel does not exist

    REQUIRE( app.m_modevalOpened == false );
    REQUIRE( app.m_pwfsWidth == 16 );
    REQUIRE( app.m_pwfsHeight == 16 );
    REQUIRE( app.pixels_per_quadrant == 36 );
    REQUIRE( app.pp_image != nullptr );
    REQUIRE( app.modeval != nullptr );
    REQUIRE( app.randomAmps != nullptr );
    REQUIRE( app.imagebuffer != nullptr );
    REQUIRE( app.m_shaped_command.rows() == 2 );
    REQUIRE( app.m_shaped_command.cols() == 2 );

    for( int n = 0; n < 4 * 36; ++n )
    {
        REQUIRE( app.pp_image[n] == 0.0f );
    }
    for( int n = 0; n < 4; ++n )
    {
        REQUIRE( app.modeval[n] == 0.0f );
    }

    // dataset 0 amplitudes were loaded
    for( int n = 0; n < 8; ++n )
    {
        REQUIRE( app.randomAmps[n] == Approx( n ) );
    }

    cleanupPipeline();
}

/// Verify send_to_shmim copies the modal values into the modeval stream and updates its counters.
/**
 * \ingroup dlDataCollection_unit_test
 */
TEST_CASE( "dlDataCollection send_to_shmim", "[dlDataCollection]" )
{
    // clang-format off
    #ifdef DLDATACOLLECTION_TEST_DOXYGEN_REF
    dlDataCollection::send_to_shmim();
    #endif
    // clang-format on

    dlDataCollection_test app( "dlcol" );
    setupPipeline( app, 2, 5, 1 );

    for( int n = 0; n < 4; ++n )
    {
        app.modeval[n] = 1.5f * ( n + 1 );
    }

    REQUIRE( app.send_to_shmim() == 0 );
    for( int n = 0; n < 4; ++n )
    {
        REQUIRE( app.m_testData[n] == Approx( 1.5f * ( n + 1 ) ) );
    }
    REQUIRE( app.m_testMd.cnt0 == 1 );
    REQUIRE( app.m_testMd.write == 0 );

    // The shaped command is filled column-major, which keeps the modal order in memory.
    REQUIRE( app.m_shaped_command( 0, 0 ) == Approx( 1.5f ) );
    REQUIRE( app.m_shaped_command( 1, 0 ) == Approx( 3.0f ) );
    REQUIRE( app.m_shaped_command( 0, 1 ) == Approx( 4.5f ) );
    REQUIRE( app.m_shaped_command( 1, 1 ) == Approx( 6.0f ) );

    REQUIRE( app.send_to_shmim() == 0 );
    REQUIRE( app.m_testMd.cnt0 == 2 );

    cleanupPipeline();
}

/// Verify processImage extracts the four padded pupils, drives the modal amplitudes, and saves datasets.
/**
 * \ingroup dlDataCollection_unit_test
 */
TEST_CASE( "dlDataCollection processImage", "[dlDataCollection]" )
{
    // clang-format off
    #ifdef DLDATACOLLECTION_TEST_DOXYGEN_REF
    dlDataCollection::processImage(nullptr, dev::shmimT());
    #endif
    // clang-format on

    dlDataCollection_test app( "dlcol" );
    setupPipeline( app, 2, 5, 1 );

    std::vector<float> f1 = makeFrame( 0 );
    std::vector<float> f2 = makeFrame( 1000 );
    std::vector<float> f3 = makeFrame( 2000 );
    std::vector<float> f4 = makeFrame( 3000 );

    SECTION( "pupil extraction with zero padding and normalization" )
    {
        REQUIRE( app.processImage( f1.data(), dev::shmimT() ) == 0 );

        std::vector<float> pp = expectedPP( f1 );
        for( int n = 0; n < 4 * 36; ++n )
        {
            REQUIRE( app.pp_image[n] == Approx( pp[n] ) );
        }
        // spot checks: pupil 0 first valid pixel is frame(2,1) = 2 + 16 = 18, times 0.5
        REQUIRE( app.pp_image[14] == Approx( 9.0f ) );
        REQUIRE( app.pp_image[0] == 0.0f );
        REQUIRE( app.pp_image[35] == 0.0f );
    }

    SECTION( "frame skipping, modal amplitudes and dataset save" )
    {
        // frame 1: amplitudes from row 0 of dataset 0, not stored (frame_wait != NumFrameSkip)
        REQUIRE( app.processImage( f1.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.frame_saved == 0 );
        REQUIRE( app.frame_wait == 1 );
        REQUIRE( app.frame_counter == 1 );
        for( int n = 0; n < 4; ++n )
        {
            REQUIRE( app.m_testData[n] == Approx( n ) );
        }
        REQUIRE( app.m_testMd.cnt0 == 1 );

        // frame 2: same amplitudes (nothing stored yet), stored in slot 0
        REQUIRE( app.processImage( f2.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.frame_saved == 1 );
        REQUIRE( app.frame_wait == 1 );
        for( int n = 0; n < 4; ++n )
        {
            REQUIRE( app.m_testData[n] == Approx( n ) );
        }

        // frame 3: amplitudes from row 1, stored in slot 1, which completes and saves dataset 0
        REQUIRE( app.processImage( f3.data(), dev::shmimT() ) == 0 );
        for( int n = 0; n < 4; ++n )
        {
            REQUIRE( app.m_testData[n] == Approx( 4 + n ) );
        }
        REQUIRE( app.dataset_i == 1 );
        REQUIRE( app.frame_saved == 0 );
        REQUIRE( app.frame_counter == 3 );

        std::vector<float> saved = readFloats( dataPrefix + "images_dataset_0.bin" );
        REQUIRE( saved.size() == 2 * 4 * 36 );
        std::vector<float> pp2 = expectedPP( f2 );
        std::vector<float> pp3 = expectedPP( f3 );
        for( int n = 0; n < 4 * 36; ++n )
        {
            REQUIRE( saved[n] == Approx( pp2[n] ) );
            REQUIRE( saved[4 * 36 + n] == Approx( pp3[n] ) );
        }

        // dataset 1 amplitudes are now loaded
        REQUIRE( app.randomAmps[0] == Approx( 100 ) );

        // frame 4: amplitudes from row 0 of dataset 1
        REQUIRE( app.processImage( f4.data(), dev::shmimT() ) == 0 );
        for( int n = 0; n < 4; ++n )
        {
            REQUIRE( app.m_testData[n] == Approx( 100 + n ) );
        }
        REQUIRE( app.frame_saved == 1 );
        REQUIRE( app.m_testMd.cnt0 == 4 );
    }

    cleanupPipeline();
}

/// Verify NumFrameSkip of 2 stores every second frame.
/**
 * \ingroup dlDataCollection_unit_test
 */
TEST_CASE( "dlDataCollection processImage frame skip", "[dlDataCollection]" )
{
    // clang-format off
    #ifdef DLDATACOLLECTION_TEST_DOXYGEN_REF
    dlDataCollection::processImage(nullptr, dev::shmimT());
    #endif
    // clang-format on

    dlDataCollection_test app( "dlcol" );
    setupPipeline( app, 10, 5, 2 );

    std::vector<float> f = makeFrame( 0 );

    std::vector<int> saved;
    for( int k = 0; k < 8; ++k )
    {
        REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
        saved.push_back( app.frame_saved );
    }

    // frames 3, 5 and 7 (1-based) are stored
    REQUIRE( saved == std::vector<int>( { 0, 0, 1, 1, 2, 2, 3, 3 } ) );
    REQUIRE( app.frame_counter == 8 );

    cleanupPipeline();
}

/// Verify that completing the last dataset calls appShutdown, which zeros the modal command.
/**
 * \ingroup dlDataCollection_unit_test
 */
TEST_CASE( "dlDataCollection last dataset shuts down", "[dlDataCollection]" )
{
    // clang-format off
    #ifdef DLDATACOLLECTION_TEST_DOXYGEN_REF
    dlDataCollection::processImage(nullptr, dev::shmimT());
    dlDataCollection::appShutdown();
    #endif
    // clang-format on

    dlDataCollection_test app( "dlcol" );
    setupPipeline( app, 1, 0, 1 ); // one image per dataset, Nset = 0

    // Remove dataset 1 so the reload after shutdown fails before touching the freed amplitudes.
    remove( ( ampsPrefix + "modeval_dataset_1.csv" ).c_str() );

    std::vector<float> f = makeFrame( 0 );

    REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
    REQUIRE( app.dataset_i == 0 );

    REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
    app.forgetFreedBuffers(); // appShutdown freed them without resetting the pointers

    REQUIRE( app.dataset_i == 1 );
    REQUIRE( readFloats( dataPrefix + "images_dataset_0.bin" ).size() == 4 * 36 );

    // appShutdown sent a zero command
    for( int n = 0; n < 4; ++n )
    {
        REQUIRE( app.m_testData[n] == 0.0f );
    }
    REQUIRE( app.m_testMd.cnt0 == 3 );

    cleanupPipeline();
}

/// Verify appShutdown sends a zero modal command and frees the buffers.
/**
 * \ingroup dlDataCollection_unit_test
 */
TEST_CASE( "dlDataCollection appShutdown", "[dlDataCollection]" )
{
    // clang-format off
    #ifdef DLDATACOLLECTION_TEST_DOXYGEN_REF
    dlDataCollection::appShutdown();
    #endif
    // clang-format on

    dlDataCollection_test app( "dlcol" );
    setupPipeline( app, 2, 5, 1 );

    for( int n = 0; n < 4; ++n )
    {
        app.modeval[n] = 7.0f;
    }

    REQUIRE( app.appShutdown() == 0 );
    app.forgetFreedBuffers();

    for( int n = 0; n < 4; ++n )
    {
        REQUIRE( app.m_testData[n] == 0.0f );
    }
    REQUIRE( app.m_testMd.cnt0 == 1 );
    REQUIRE( app.m_testMd.write == 0 );

    cleanupPipeline();
}

} // namespace dlDataCollectionTest

} // namespace libXWCTest
