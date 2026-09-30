/** \file po4ao_test.cpp
 * \brief Catch2 tests for the po4ao app.
 * \author Claude Code
 *
 * The TensorRT and CUDA runtime headers are replaced by the stubs in `stubs/`, whose functions and fake engine
 * classes are defined here.
 */

#include "../../../tests/testXWC.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <mx/improc/eigenImage.hpp>

// po4ao.hpp uses `eigenImage` unqualified at global scope (in CircularBuffer) before anything brings
// mx::improc into scope, so make the alias template visible here before including it.
using mx::improc::eigenImage;

#include "../po4ao.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Fake TensorRT and CUDA state used by the stub definitions below.
namespace po4aoStubs
{

/// Controllable state and call records for the TensorRT and CUDA stubs.
struct po4aoStubState
{
    /// The shape reported for the input tensor ("input").
    nvinfer1::Dims inputDims;

    /// The shape reported for the output tensor ("output").
    nvinfer1::Dims outputDims;

    /// If true, deserializeCudaEngine() returns nullptr.
    bool failDeserialize{ false };

    /// Number of runtimes created by createInferRuntime().
    int runtimesCreated{ 0 };

    /// Number of runtimes destroyed.
    int runtimesDestroyed{ 0 };

    /// Number of deserializeCudaEngine() calls.
    int deserializeCalls{ 0 };

    /// Number of engines created, also the id of the last engine.
    int enginesCreated{ 0 };

    /// Number of engines destroyed.
    int enginesDestroyed{ 0 };

    /// Number of execution contexts created.
    int contextsCreated{ 0 };

    /// Number of execution contexts destroyed.
    int contextsDestroyed{ 0 };

    /// Number of executeV2() calls.
    int executeCalls{ 0 };

    /// The engine id of the context used by the last executeV2() call.
    int lastContextId{ 0 };

    /// The blob passed to the last deserializeCudaEngine() call.
    std::vector<char> lastBlob;

    /// The input tensor seen by the last executeV2() call.
    std::vector<float> lastInput;

    /// The values executeV2() writes to the output tensor.
    std::vector<float> nextOutput;

    /// Number of cudaMalloc() calls.
    int mallocCalls{ 0 };

    /// The sizes passed to cudaMalloc().
    std::vector<size_t> mallocSizes;

    /// Number of cudaFree() calls.
    int freeCalls{ 0 };

    /// The kinds passed to cudaMemcpy(), in order.
    std::vector<int> memcpyKinds;

    /// Set the tensor shapes to an NCHW input of C x H x W and an N-element output.
    void setShapes( int64_t C /**< [in] input channels */,
                    int64_t H /**< [in] input height */,
                    int64_t W /**< [in] input width */,
                    int64_t N /**< [in] output size */ )
    {
        inputDims.nbDims  = 4;
        inputDims.d[0]    = 1;
        inputDims.d[1]    = C;
        inputDims.d[2]    = H;
        inputDims.d[3]    = W;
        outputDims.nbDims = 2;
        outputDims.d[0]   = 1;
        outputDims.d[1]   = N;
    }

    /// The number of elements in the input tensor.
    size_t inputSize() const
    {
        return inputDims.d[1] * inputDims.d[2] * inputDims.d[3];
    }
};

/// The global stub state, reset at the start of each test.
po4aoStubState g_stub;

/// Fake execution context which copies the input and writes po4aoStubState::nextOutput.
class fakeContext : public nvinfer1::IExecutionContext
{
  public:
    /// The id of the engine that created this context.
    int m_id{ 0 };

    /// Construct a context for engine \p id.
    explicit fakeContext( int id /**< [in] the engine id */ ) : m_id( id )
    {
        ++g_stub.contextsCreated;
    }

    /// Count the destruction.
    ~fakeContext()
    {
        ++g_stub.contextsDestroyed;
    }

    /// Record the input and write the configured output.
    bool executeV2( void *const *bindings /**< [in] input and output buffers */ ) noexcept override
    {
        ++g_stub.executeCalls;
        g_stub.lastContextId = m_id;

        const float *in = static_cast<const float *>( bindings[0] );
        g_stub.lastInput.assign( in, in + g_stub.inputSize() );

        float *out = static_cast<float *>( bindings[1] );
        for( size_t n = 0; n < g_stub.nextOutput.size(); ++n )
        {
            out[n] = g_stub.nextOutput[n];
        }

        return true;
    }
};

/// Fake engine reporting the shapes in po4aoStubState.
class fakeEngine : public nvinfer1::ICudaEngine
{
  public:
    /// The id of this engine (its creation index, starting at 1).
    int m_id{ 0 };

    /// Construct engine \p id.
    explicit fakeEngine( int id /**< [in] the engine id */ ) : m_id( id )
    {
    }

    /// Count the destruction.
    ~fakeEngine()
    {
        ++g_stub.enginesDestroyed;
    }

    /// Create a fakeContext tagged with this engine's id.
    nvinfer1::IExecutionContext *createExecutionContext() noexcept override
    {
        return new fakeContext( m_id );
    }

    /// There is one input and one output.
    int32_t getNbIOTensors() const noexcept override
    {
        return 2;
    }

    /// Tensor 0 is "input", tensor 1 is "output".
    const char *getIOTensorName( int32_t index /**< [in] the tensor index */ ) const noexcept override
    {
        if( index == 0 )
        {
            return "input";
        }

        return "output";
    }

    /// Return the input or output shape.
    nvinfer1::Dims getTensorShape( const char *tensorName /**< [in] the tensor name */ ) const noexcept override
    {
        if( std::strcmp( tensorName, "input" ) == 0 )
        {
            return g_stub.inputDims;
        }

        return g_stub.outputDims;
    }
};

/// Fake runtime which records the blob and creates fakeEngine objects.
class fakeRuntime : public nvinfer1::IRuntime
{
  public:
    /// Count the destruction.
    ~fakeRuntime()
    {
        ++g_stub.runtimesDestroyed;
    }

    /// Record the blob and create a new engine, unless po4aoStubState::failDeserialize is set.
    nvinfer1::ICudaEngine *deserializeCudaEngine( const void *blob /**< [in] the serialized engine */,
                                                  std::size_t size /**< [in] the blob size */ ) noexcept override
    {
        ++g_stub.deserializeCalls;
        const char *b = static_cast<const char *>( blob );
        g_stub.lastBlob.assign( b, b + size );

        if( g_stub.failDeserialize )
        {
            return nullptr;
        }

        ++g_stub.enginesCreated;
        return new fakeEngine( g_stub.enginesCreated );
    }
};

} // namespace po4aoStubs

nvinfer1::IRuntime *nvinfer1::createInferRuntime( nvinfer1::ILogger &logger ) noexcept
{
    static_cast<void>( logger );
    ++po4aoStubs::g_stub.runtimesCreated;
    return new po4aoStubs::fakeRuntime;
}

cudaError_t cudaMalloc( void **devPtr, size_t size )
{
    ++po4aoStubs::g_stub.mallocCalls;
    po4aoStubs::g_stub.mallocSizes.push_back( size );
    *devPtr = std::malloc( size );
    return cudaSuccess;
}

cudaError_t cudaFree( void *devPtr )
{
    ++po4aoStubs::g_stub.freeCalls;
    std::free( devPtr );
    return cudaSuccess;
}

cudaError_t cudaMemcpy( void *dst, const void *src, size_t count, enum cudaMemcpyKind kind )
{
    po4aoStubs::g_stub.memcpyKinds.push_back( static_cast<int>( kind ) );
    std::memcpy( dst, src, count );
    return cudaSuccess;
}

/// \endcond

namespace libXWCTest
{

/** \defgroup po4ao_unit_test po4ao Unit Tests
 * \brief Unit tests for the po4ao application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `po4ao` unit tests.
/** \ingroup po4ao_unit_test
 */
namespace po4aoTest
{

using po4aoStubs::g_stub;

/// Directory used as `MILK_SHM_DIR` for the shared-memory stream tests.
constexpr const char *c_shmDir = "/tmp/po4ao_test_shm";

/// Name of the output stream opened by allocate().
constexpr const char *c_outName = "po4ao_test_out";

/// Name of the observation stream opened by allocate().
constexpr const char *c_obsName = "po4ao_test_obs";

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing po4ao internals.
class po4ao_test : public po4ao
{
  public:
    using po4ao::allocate;
    using po4ao::newCallBack_m_indiP_reloadToggle;
    using po4ao::processImage;

    using po4ao::command_buffer;
    using po4ao::context;
    using po4ao::context2;
    using po4ao::d_input;
    using po4ao::d_output;
    using po4ao::dataDirs;
    using po4ao::engine;
    using po4ao::engine2;
    using po4ao::engineData;
    using po4ao::engineDirs;
    using po4ao::engineName;
    using po4ao::engineReloaded;
    using po4ao::episode_counter;
    using po4ao::frame_counter;
    using po4ao::inputC;
    using po4ao::inputH;
    using po4ao::inputSize;
    using po4ao::inputState;
    using po4ao::inputW;
    using po4ao::integrator_commands;
    using po4ao::integrator_gain;
    using po4ao::iterations_per_ep;
    using po4ao::m_indiP_reloadToggle;
    using po4ao::m_output;
    using po4ao::m_outputChannel;
    using po4ao::m_outputDataType;
    using po4ao::m_outputHeight;
    using po4ao::m_outputOpened;
    using po4ao::m_outputTypeSize;
    using po4ao::m_outputWidth;
    using po4ao::max_sigma;
    using po4ao::Nact;
    using po4ao::Nact_across;
    using po4ao::Nfeatures;
    using po4ao::Nhist;
    using po4ao::outputSize;
    using po4ao::po4ao_act_Channel;
    using po4ao::po4ao_obs_Channel;
    using po4ao::po4ao_obs_DataType;
    using po4ao::po4ao_obs_Height;
    using po4ao::po4ao_obs_Opened;
    using po4ao::po4ao_obs_TypeSize;
    using po4ao::po4ao_obs_Width;
    using po4ao::reconstructed_buffer;
    using po4ao::reloadEngine;
    using po4ao::replay_buffer_size;
    using po4ao::runtime;
    using po4ao::warmup_episodes;

    /// True once shutdownForTest() has run.
    bool m_testShutdown{ false };

    /// Construct a harness with the given device name, creating the reload_engine toggle property.
    explicit po4ao_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
        createStandardIndiToggleSw( m_indiP_reloadToggle, "reload_engine" );
    }

    /// Release everything the app allocated, if shutdownForTest() was not called.
    ~po4ao_test() noexcept
    {
        if( !m_testShutdown )
        {
            shutdownForTest();
        }
    }

    /// Read a config file and run loadConfig().
    void loadConfigFromFile( const std::string &fname /**< [in] config file path */ )
    {
        config.readConfig( fname );
        loadConfig();
    }

    /// The configured input shmim name.
    std::string inputShmimName()
    {
        return shmimName();
    }

    /// Close the streams allocate() opened, run appShutdown(), and free what it leaves behind.
    /** appShutdown() does not close the output streams, does not free m_output, and leaves dangling pointers.
     */
    void shutdownForTest()
    {
        m_testShutdown = true;

        if( m_outputOpened )
        {
            ImageStreamIO_closeIm( &m_outputStream );
            m_outputOpened = false;
        }

        if( po4ao_obs_Opened )
        {
            ImageStreamIO_closeIm( &po4ao_obs_Stream );
            po4ao_obs_Opened = false;
        }

        appShutdown();

        if( m_output )
        {
            delete[] m_output;
            m_output = nullptr;
        }

        inputState           = nullptr;
        integrator_commands  = nullptr;
        command_buffer       = nullptr;
        reconstructed_buffer = nullptr;
        runtime              = nullptr;
        engine               = nullptr;
        engine2              = nullptr;
        context              = nullptr;
        context2             = nullptr;
        d_input              = nullptr;
        d_output             = nullptr;
    }
};

/// A float shared-memory stream created for a test and destroyed with the object.
class testStream
{
  public:
    /// The creator's image.
    IMAGE m_image{};

    /// True if creation succeeded.
    bool m_created{ false };

    /// Create a w x 1 float stream named \p name with \p nsem semaphores.
    testStream( const std::string &name /**< [in] stream name */,
                uint32_t           w /**< [in] stream width */,
                int                nsem /**< [in] number of semaphores */ )
    {
        uint32_t imsize[3] = { w, 1, 1 };
        m_created          = ( ImageStreamIO_createIm_gpu( &m_image,
                                                  name.c_str(),
                                                  3,
                                                  imsize,
                                                  _DATATYPE_FLOAT,
                                                  -1,
                                                  1,
                                                  nsem,
                                                  0,
                                                  CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                                  0 ) == IMAGESTREAMIO_SUCCESS );
    }

    /// Destroy the stream.
    ~testStream()
    {
        if( m_created )
        {
            ImageStreamIO_destroyIm( &m_image );
        }
    }
};
/// \endcond

/// Create the private shared-memory directory and point ImageStreamIO at it.
/** ImageStreamIO caches the directory on first use, so every test uses the same one.
 */
static void setupShmDir()
{
    std::filesystem::create_directories( c_shmDir );
    setenv( "MILK_SHM_DIR", c_shmDir, 1 );
}

/// Write \p contents to the binary file \p fname.
static void writeFile( const std::string &fname /**< [in] file path */,
                       const std::string &contents /**< [in] file contents */ )
{
    std::ofstream fout( fname, std::ios::binary );
    fout.write( contents.data(), contents.size() );
}

/// Write an engine file, set the stub shapes, and run load_engine(), create_engine_context() and
/// prepare_engine_memory() as appStartup() does.
static void setupEngine( po4ao_test        &app /**< [in] the harness */,
                         const std::string &engineFile /**< [in] engine file name, in /tmp */,
                         const std::string &contents /**< [in] engine file contents */,
                         int                C /**< [in] input channels */,
                         int                H /**< [in] input height and width */,
                         int                N /**< [in] output size */ )
{
    writeFile( "/tmp/" + engineFile, contents );
    g_stub.setShapes( C, H, H, N );

    app.engineDirs = "/tmp";
    app.engineName = engineFile;
    app.load_engine( app.engineDirs + "/" + app.engineName );
    app.create_engine_context();
    app.prepare_engine_memory();
}

/// Verify CircularBuffer::add() stores images in a ring and num_elements() saturates at the history size.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao CircularBuffer add and num_elements", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    CircularBuffer::add(float*);
    CircularBuffer::num_elements();
    CircularBuffer::getBuffer();
    #endif
    // clang-format on

    CircularBuffer cb( 1, 2, 3 );
    REQUIRE( cb.num_elements() == 0 );
    REQUIRE( cb.getBuffer() != nullptr );

    float im[2];
    for( int n = 0; n < 4; ++n )
    {
        im[0] = 10 * n;
        im[1] = 10 * n + 1;
        cb.add( im );
        CHECK( cb.num_elements() == ( n < 3 ? n + 1 : 3 ) );
    }

    float *buf = cb.getBuffer();

    // The 4th image overwrote slot 0, slots 1 and 2 hold images 1 and 2.
    CHECK( buf[0] == 30 );
    CHECK( buf[1] == 31 );
    CHECK( buf[2] == 10 );
    CHECK( buf[3] == 11 );
    CHECK( buf[4] == 20 );
    CHECK( buf[5] == 21 );
}

/// Verify CircularBuffer::add_eigenimage() advances the write position.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao CircularBuffer add_eigenimage", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    CircularBuffer::add_eigenimage(Eigen::Map<eigenImage<float>>);
    #endif
    // clang-format on

    // add_eigenimage() copies from the address of the Map object rather than its data (see README), so only the
    // element count is checked.  The image is kept small so the copy stays within the Map object.
    CircularBuffer cb( 1, 4, 2 );

    float                         im[4] = { 1, 2, 3, 4 };
    Eigen::Map<eigenImage<float>> map( im, 1, 4 );

    cb.add_eigenimage( map );
    CHECK( cb.num_elements() == 1 );
    cb.add_eigenimage( map );
    CHECK( cb.num_elements() == 2 );
    cb.add_eigenimage( map );
    CHECK( cb.num_elements() == 2 );
}

/// Verify the TensorRT Logger prints warnings and errors but not info or verbose messages.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao Logger severity filter", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    Logger::log(Severity, const char*);
    #endif
    // clang-format on

    Logger             lg;
    nvinfer1::ILogger &il = lg;

    std::ostringstream captured;
    std::streambuf    *old = std::cout.rdbuf( captured.rdbuf() );

    il.log( nvinfer1::ILogger::Severity::kINTERNAL_ERROR, "po4ao-internal" );
    il.log( nvinfer1::ILogger::Severity::kERROR, "po4ao-error" );
    il.log( nvinfer1::ILogger::Severity::kWARNING, "po4ao-warning" );
    il.log( nvinfer1::ILogger::Severity::kINFO, "po4ao-info" );
    il.log( nvinfer1::ILogger::Severity::kVERBOSE, "po4ao-verbose" );

    std::cout.rdbuf( old );

    std::string out = captured.str();
    CHECK( out.find( "po4ao-internal\n" ) != std::string::npos );
    CHECK( out.find( "po4ao-error\n" ) != std::string::npos );
    CHECK( out.find( "po4ao-warning\n" ) != std::string::npos );
    CHECK( out.find( "po4ao-info" ) == std::string::npos );
    CHECK( out.find( "po4ao-verbose" ) == std::string::npos );
}

/// Verify the configuration defaults.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao configuration defaults", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::setupConfig();
    po4ao::loadConfig();
    po4ao::loadConfigImpl(mx::app::appConfigurator&);
    #endif
    // clang-format on

    po4ao_test app( "po4ao" );
    app.setupConfig();

    mx::app::writeConfigFile( "/tmp/po4ao_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.loadConfigFromFile( "/tmp/po4ao_test_defaults.conf" );

    CHECK( app.dataDirs == "" );
    CHECK( app.engineDirs == "" );
    CHECK( app.engineName == "" );
    CHECK( app.m_outputChannel == "" );
    CHECK( app.po4ao_obs_Channel == "" );
    CHECK( app.po4ao_act_Channel == "" );
    CHECK( app.Nhist == 0 );
    CHECK( app.iterations_per_ep == 0 );
    CHECK( app.warmup_episodes == 0 );
    CHECK( app.replay_buffer_size == 0 );
    CHECK( app.max_sigma == 0 );
    CHECK( app.integrator_gain == 0 );
    CHECK( app.reloadEngine == false );
    CHECK( app.inputShmimName() == "po4ao" );

    std::remove( "/tmp/po4ao_test_defaults.conf" );
}

/// Verify the configuration overrides.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao configuration overrides", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::setupConfig();
    po4ao::loadConfig();
    po4ao::loadConfigImpl(mx::app::appConfigurator&);
    #endif
    // clang-format on

    po4ao_test app( "po4ao" );
    app.setupConfig();

    mx::app::writeConfigFile( "/tmp/po4ao_test_overrides.conf",
                              { "parameters",
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
                                "shmimMonitor" },
                              { "dataDirs",
                                "engineDirs",
                                "engineName",
                                "channel",
                                "observation_channel",
                                "action_channel",
                                "Nhist",
                                "iterations_per_ep",
                                "warmup_episodes",
                                "replay_buffer_size",
                                "max_sigma",
                                "integrator_gain",
                                "reloadEngine",
                                "shmimName" },
                              { "/data/po4ao",
                                "/data/engines",
                                "policy.plan",
                                "aol1_outputs",
                                "po4ao_obs",
                                "po4ao_act",
                                "7",
                                "500",
                                "3",
                                "2000",
                                "0.25",
                                "0.4",
                                "true",
                                "aol1_modevals" } );
    app.loadConfigFromFile( "/tmp/po4ao_test_overrides.conf" );

    // dataDirs is registered but never read by loadConfigImpl().
    CHECK( app.dataDirs == "" );
    CHECK( app.engineDirs == "/data/engines" );
    CHECK( app.engineName == "policy.plan" );
    CHECK( app.m_outputChannel == "aol1_outputs" );
    CHECK( app.po4ao_obs_Channel == "po4ao_obs" );

    // Current behavior: po4ao_act_Channel is read from parameters.observation_channel, not action_channel.
    CHECK( app.po4ao_act_Channel == "po4ao_obs" );

    CHECK( app.Nhist == 7 );
    CHECK( app.iterations_per_ep == 500 );
    CHECK( app.warmup_episodes == 3 );
    CHECK( app.replay_buffer_size == 2000 );
    CHECK( app.max_sigma == Approx( 0.25 ) );
    CHECK( app.integrator_gain == Approx( 0.4 ) );
    CHECK( app.reloadEngine == true );
    CHECK( app.inputShmimName() == "aol1_modevals" );

    std::remove( "/tmp/po4ao_test_overrides.conf" );
}

/// Verify load_engine() reads the whole file, and throws for a missing file.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao load_engine", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::load_engine(const std::string);
    #endif
    // clang-format on

    g_stub = po4aoStubs::po4aoStubState();

    po4ao_test app( "po4ao" );

    SECTION( "existing file" )
    {
        std::string contents( "PO4AO-ENGINE\0\x01\x02", 15 );
        writeFile( "/tmp/po4ao_test_load.plan", contents );

        app.load_engine( "/tmp/po4ao_test_load.plan" );

        REQUIRE( app.engineData.size() == contents.size() );
        CHECK( std::string( app.engineData.data(), app.engineData.size() ) == contents );

        std::remove( "/tmp/po4ao_test_load.plan" );
    }

    SECTION( "missing file" )
    {
        // load_engine() only prints an error, then sizes the vector from tellg() == -1.
        REQUIRE_THROWS_AS( app.load_engine( "/tmp/po4ao_test_does_not_exist.plan" ), std::length_error );
    }
}

/// Verify create_engine_context() deserializes the loaded engine and derives the geometry from the tensor shapes.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao create_engine_context", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::create_engine_context();
    #endif
    // clang-format on

    g_stub = po4aoStubs::po4aoStubState();

    po4ao_test app( "po4ao" );

    writeFile( "/tmp/po4ao_test_ctx.plan", "ENGINE-CTX" );
    app.load_engine( "/tmp/po4ao_test_ctx.plan" );

    g_stub.setShapes( 3, 5, 5, 25 );
    app.create_engine_context();

    CHECK( g_stub.runtimesCreated == 1 );
    CHECK( g_stub.deserializeCalls == 1 );
    CHECK( std::string( g_stub.lastBlob.begin(), g_stub.lastBlob.end() ) == "ENGINE-CTX" );
    CHECK( g_stub.contextsCreated == 1 );

    REQUIRE( app.runtime != nullptr );
    REQUIRE( app.engine != nullptr );
    REQUIRE( app.context != nullptr );
    CHECK( app.engine2 == nullptr );
    CHECK( app.context2 == nullptr );

    CHECK( app.inputC == 3 );
    CHECK( app.inputH == 5 );
    CHECK( app.inputW == 5 );
    CHECK( app.inputSize == 75 );
    CHECK( app.outputSize == 25 );
    CHECK( app.Nact == 25 );
    CHECK( app.Nact_across == 5 );
    CHECK( app.Nfeatures == 3 );

    std::remove( "/tmp/po4ao_test_ctx.plan" );
}

/// Verify prepare_engine_memory() allocates the input and output buffers and cleanup_engine_memory() frees them.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao prepare and cleanup engine memory", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::prepare_engine_memory();
    po4ao::cleanup_engine_memory();
    #endif
    // clang-format on

    g_stub = po4aoStubs::po4aoStubState();

    po4ao_test app( "po4ao" );
    app.inputSize  = 12;
    app.outputSize = 5;

    app.prepare_engine_memory();

    REQUIRE( g_stub.mallocCalls == 2 );
    CHECK( g_stub.mallocSizes[0] == 12 * sizeof( float ) );
    CHECK( g_stub.mallocSizes[1] == 5 * sizeof( float ) );
    CHECK( app.d_input != nullptr );
    CHECK( app.d_output != nullptr );

    app.cleanup_engine_memory();
    CHECK( g_stub.freeCalls == 2 );

    app.d_input  = nullptr;
    app.d_output = nullptr;

    // With nothing allocated, nothing is freed.
    app.cleanup_engine_memory();
    CHECK( g_stub.freeCalls == 2 );
}

/// Verify reload_engine() deserializes a second engine and switch_engine() swaps it in.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao reload_engine and switch_engine", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::reload_engine();
    po4ao::switch_engine();
    #endif
    // clang-format on

    g_stub = po4aoStubs::po4aoStubState();

    po4ao_test app( "po4ao" );
    setupEngine( app, "po4ao_test_reload_a.plan", "ENGINE-A", 2, 2, 4 );

    nvinfer1::ICudaEngine       *first    = app.engine;
    nvinfer1::IExecutionContext *firstCtx = app.context;

    writeFile( "/tmp/po4ao_test_reload_b.plan", "ENGINE-B" );
    app.engineName = "po4ao_test_reload_b.plan";

    app.reload_engine();

    CHECK( g_stub.runtimesCreated == 1 ); // the existing runtime is reused
    CHECK( g_stub.deserializeCalls == 2 );
    CHECK( std::string( g_stub.lastBlob.begin(), g_stub.lastBlob.end() ) == "ENGINE-B" );
    REQUIRE( app.engine2 != nullptr );
    REQUIRE( app.context2 != nullptr );
    CHECK( app.engine == first );
    CHECK( app.context == firstCtx );
    CHECK( app.engineReloaded == true );

    nvinfer1::ICudaEngine       *second    = app.engine2;
    nvinfer1::IExecutionContext *secondCtx = app.context2;

    app.switch_engine();

    CHECK( app.engine == second );
    CHECK( app.context == secondCtx );
    CHECK( app.engine2 == first );
    CHECK( app.context2 == firstCtx );
    CHECK( app.engineReloaded == false );

    std::remove( "/tmp/po4ao_test_reload_a.plan" );
    std::remove( "/tmp/po4ao_test_reload_b.plan" );
}

/// Verify cleanup_engine_context() deletes the runtime and both engines and contexts.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao cleanup_engine_context", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::cleanup_engine_context();
    #endif
    // clang-format on

    g_stub = po4aoStubs::po4aoStubState();

    po4ao_test app( "po4ao" );
    setupEngine( app, "po4ao_test_cleanup.plan", "ENGINE-C", 2, 2, 4 );
    app.reload_engine();

    app.cleanup_engine_context();

    CHECK( g_stub.runtimesDestroyed == 1 );
    CHECK( g_stub.enginesDestroyed == 2 );
    CHECK( g_stub.contextsDestroyed == 2 );

    // cleanup_engine_context() does not reset the pointers.
    app.runtime  = nullptr;
    app.engine   = nullptr;
    app.engine2  = nullptr;
    app.context  = nullptr;
    app.context2 = nullptr;

    std::remove( "/tmp/po4ao_test_cleanup.plan" );
}

/// Verify the reload_engine INDI callback.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao reload_engine INDI callback", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::newCallBack_m_indiP_reloadToggle(const pcf::IndiProperty&);
    #endif
    // clang-format on

    g_stub = po4aoStubs::po4aoStubState();

    po4ao_test app( "po4ao" );

    REQUIRE( app.m_indiP_reloadToggle.getName() == "reload_engine" );
    REQUIRE( app.m_indiP_reloadToggle.getDevice() == "po4ao" );

    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( "po4ao" );

    SECTION( "wrong name" )
    {
        ip.setName( "wrong" );
        ip.add( pcf::IndiElement( "toggle", pcf::IndiElement::On ) );

        CHECK( app.newCallBack_m_indiP_reloadToggle( ip ) == -1 );
        CHECK( app.engineReloaded == false );
        CHECK( g_stub.deserializeCalls == 0 );
    }

    SECTION( "missing toggle element" )
    {
        ip.setName( "reload_engine" );

        REQUIRE_THROWS_AS( app.newCallBack_m_indiP_reloadToggle( ip ), std::runtime_error );
    }

    SECTION( "toggle off" )
    {
        ip.setName( "reload_engine" );
        ip.add( pcf::IndiElement( "toggle", pcf::IndiElement::Off ) );

        CHECK( app.newCallBack_m_indiP_reloadToggle( ip ) == 0 );
        CHECK( app.engineReloaded == false );
        CHECK( g_stub.deserializeCalls == 0 );
    }

    SECTION( "toggle on reloads the engine" )
    {
        setupEngine( app, "po4ao_test_indi_a.plan", "ENGINE-A", 2, 2, 4 );
        writeFile( "/tmp/po4ao_test_indi_b.plan", "ENGINE-B" );
        app.engineName = "po4ao_test_indi_b.plan";

        ip.setName( "reload_engine" );
        ip.add( pcf::IndiElement( "toggle", pcf::IndiElement::On ) );

        CHECK( app.newCallBack_m_indiP_reloadToggle( ip ) == 0 );
        CHECK( app.engineReloaded == true );
        CHECK( g_stub.deserializeCalls == 2 );
        CHECK( std::string( g_stub.lastBlob.begin(), g_stub.lastBlob.end() ) == "ENGINE-B" );
        CHECK( app.engine2 != nullptr );
        CHECK( app.context2 != nullptr );

        std::remove( "/tmp/po4ao_test_indi_a.plan" );
        std::remove( "/tmp/po4ao_test_indi_b.plan" );
    }
}

/// Verify allocate() creates the host buffers and opens the output and observation streams.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao allocate opens the output streams", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::allocate(const dev::shmimT&);
    #endif
    // clang-format on

    g_stub = po4aoStubs::po4aoStubState();
    setupShmDir();

    testStream out( c_outName, 4, 10 );
    testStream obs( c_obsName, 4, 10 );
    REQUIRE( out.m_created );
    REQUIRE( obs.m_created );

    {
        po4ao_test app( "po4ao" );
        setupEngine( app, "po4ao_test_alloc.plan", "ENGINE", 2, 2, 4 );
        app.m_outputChannel    = c_outName;
        app.po4ao_obs_Channel  = c_obsName;
        app.replay_buffer_size = 3;

        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        CHECK( app.m_outputOpened == true );
        CHECK( app.m_outputWidth == 4 );
        CHECK( app.m_outputHeight == 1 );
        CHECK( app.m_outputDataType == _DATATYPE_FLOAT );
        CHECK( app.m_outputTypeSize == sizeof( float ) );

        CHECK( app.po4ao_obs_Opened == true );
        CHECK( app.po4ao_obs_Width == 4 );
        CHECK( app.po4ao_obs_Height == 1 );
        CHECK( app.po4ao_obs_DataType == _DATATYPE_FLOAT );
        CHECK( app.po4ao_obs_TypeSize == sizeof( float ) );

        REQUIRE( app.inputState != nullptr );
        REQUIRE( app.integrator_commands != nullptr );
        REQUIRE( app.m_output != nullptr );
        REQUIRE( app.command_buffer != nullptr );
        REQUIRE( app.reconstructed_buffer != nullptr );

        for( int n = 0; n < app.Nfeatures * app.Nact; ++n )
        {
            CHECK( app.inputState[n] == 0 );
        }

        for( int n = 0; n < app.Nact; ++n )
        {
            CHECK( app.integrator_commands[n] == 0 );
            CHECK( app.m_output[n] == 0 );
        }

        CHECK( app.command_buffer->num_elements() == 0 );
        CHECK( app.reconstructed_buffer->num_elements() == 0 );

        app.shutdownForTest();
    }

    std::remove( "/tmp/po4ao_test_alloc.plan" );
    std::filesystem::remove_all( c_shmDir );
}

/// Verify allocate() fails when a stream is missing or has too few semaphores.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao allocate stream failures", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::allocate(const dev::shmimT&);
    #endif
    // clang-format on

    g_stub = po4aoStubs::po4aoStubState();
    setupShmDir();
    writeFile( "/tmp/po4ao_test_allocfail.plan", "ENGINE" );

    SECTION( "output stream missing" )
    {
        po4ao_test app( "po4ao" );
        setupEngine( app, "po4ao_test_allocfail.plan", "ENGINE", 2, 2, 4 );
        app.m_outputChannel    = "po4ao_test_missing_out";
        app.po4ao_obs_Channel  = "po4ao_test_missing_obs";
        app.replay_buffer_size = 3;

        CHECK( app.allocate( dev::shmimT() ) == -1 );
        CHECK( app.m_outputOpened == false );
        CHECK( app.po4ao_obs_Opened == false );

        app.shutdownForTest();
    }

    SECTION( "output stream has too few semaphores" )
    {
        testStream out( c_outName, 4, 5 );
        REQUIRE( out.m_created );

        {
            po4ao_test app( "po4ao" );
            setupEngine( app, "po4ao_test_allocfail.plan", "ENGINE", 2, 2, 4 );
            app.m_outputChannel    = c_outName;
            app.po4ao_obs_Channel  = "po4ao_test_missing_obs";
            app.replay_buffer_size = 3;

            CHECK( app.allocate( dev::shmimT() ) == -1 );
            CHECK( app.m_outputOpened == false );
            CHECK( app.po4ao_obs_Opened == false );

            app.shutdownForTest();
        }
    }

    SECTION( "observation stream missing" )
    {
        testStream out( c_outName, 4, 10 );
        REQUIRE( out.m_created );

        {
            po4ao_test app( "po4ao" );
            setupEngine( app, "po4ao_test_allocfail.plan", "ENGINE", 2, 2, 4 );
            app.m_outputChannel    = c_outName;
            app.po4ao_obs_Channel  = "po4ao_test_missing_obs";
            app.replay_buffer_size = 3;

            CHECK( app.allocate( dev::shmimT() ) == -1 );
            CHECK( app.m_outputOpened == true );
            CHECK( app.po4ao_obs_Opened == false );

            app.shutdownForTest();
        }
    }

    SECTION( "observation stream has too few semaphores" )
    {
        testStream out( c_outName, 4, 10 );
        testStream obs( c_obsName, 4, 2 );
        REQUIRE( out.m_created );
        REQUIRE( obs.m_created );

        {
            po4ao_test app( "po4ao" );
            setupEngine( app, "po4ao_test_allocfail.plan", "ENGINE", 2, 2, 4 );
            app.m_outputChannel    = c_outName;
            app.po4ao_obs_Channel  = c_obsName;
            app.replay_buffer_size = 3;

            CHECK( app.allocate( dev::shmimT() ) == -1 );
            CHECK( app.m_outputOpened == true );
            CHECK( app.po4ao_obs_Opened == false );

            app.shutdownForTest();
        }
    }

    std::remove( "/tmp/po4ao_test_allocfail.plan" );
    std::filesystem::remove_all( c_shmDir );
}

/// Verify processImage() during warmup applies the integrator gain and counts episodes.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao processImage warmup integrator", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::processImage(void*, const dev::shmimT&);
    po4ao::send_obs_to_shmim();
    #endif
    // clang-format on

    g_stub = po4aoStubs::po4aoStubState();
    setupShmDir();

    testStream out( c_outName, 4, 10 );
    testStream obs( c_obsName, 4, 10 );
    REQUIRE( out.m_created );
    REQUIRE( obs.m_created );

    {
        po4ao_test app( "po4ao" );
        setupEngine( app, "po4ao_test_warmup.plan", "ENGINE", 2, 2, 4 );
        app.m_outputChannel    = c_outName;
        app.po4ao_obs_Channel  = c_obsName;
        app.replay_buffer_size = 3;
        app.iterations_per_ep  = 2;
        app.warmup_episodes    = 5;
        app.integrator_gain    = 0.5;

        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        uint64_t cnt0 = obs.m_image.md[0].cnt0;

        std::vector<float> frame = { 1, 2, 3, 4 };

        // Frame 0 starts an episode, and publishes the observation.
        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );

        for( int n = 0; n < 4; ++n )
        {
            CHECK( app.integrator_commands[n] == Approx( 0.5 * frame[n] ) );
            CHECK( app.m_output[n] == Approx( 0.5 * frame[n] ) );
        }

        CHECK( app.frame_counter == 1 );
        CHECK( app.episode_counter == 1 );
        CHECK( obs.m_image.md[0].cnt0 == cnt0 + 1 );
        CHECK( obs.m_image.md[0].write == 0 );
        CHECK( app.reconstructed_buffer->num_elements() == 1 );

        // Frame 1 is within the episode.
        frame = { -2, 4, -6, 8 };
        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );

        for( int n = 0; n < 4; ++n )
        {
            CHECK( app.m_output[n] == Approx( 0.5 * frame[n] ) );
        }

        CHECK( app.frame_counter == 2 );
        CHECK( app.episode_counter == 1 );
        CHECK( obs.m_image.md[0].cnt0 == cnt0 + 1 );

        // Frames 2 and 3 complete a second episode.
        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );

        CHECK( app.frame_counter == 4 );
        CHECK( app.episode_counter == 2 );
        CHECK( obs.m_image.md[0].cnt0 == cnt0 + 2 );
        CHECK( app.reconstructed_buffer->num_elements() == 3 );

        // The integrator path never runs inference.
        CHECK( g_stub.executeCalls == 0 );

        app.shutdownForTest();
    }

    std::remove( "/tmp/po4ao_test_warmup.plan" );
    std::filesystem::remove_all( c_shmDir );
}

/// Verify processImage() after warmup runs inference, and swaps in a reloaded engine.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao processImage inference and engine switch", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::processImage(void*, const dev::shmimT&);
    po4ao::switch_engine();
    #endif
    // clang-format on

    g_stub = po4aoStubs::po4aoStubState();
    setupShmDir();

    testStream out( c_outName, 4, 10 );
    testStream obs( c_obsName, 4, 10 );
    REQUIRE( out.m_created );
    REQUIRE( obs.m_created );

    writeFile( "/tmp/po4ao_test_infer_b.plan", "ENGINE-B" );

    {
        po4ao_test app( "po4ao" );
        setupEngine( app, "po4ao_test_infer_a.plan", "ENGINE-A", 2, 2, 4 );
        app.m_outputChannel    = c_outName;
        app.po4ao_obs_Channel  = c_obsName;
        app.replay_buffer_size = 3;
        app.iterations_per_ep  = 10;
        app.warmup_episodes    = 0;
        app.integrator_gain    = 0.5;

        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        // Past warmup.
        app.episode_counter = 1;
        g_stub.nextOutput   = { 10, 11, 12, 13 };

        std::vector<float> frame = { 1, 2, 3, 4 };
        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );

        CHECK( g_stub.executeCalls == 1 );
        CHECK( g_stub.lastContextId == 1 );

        // The state vector is currently all zeros (its history assembly is commented out in the app).
        REQUIRE( g_stub.lastInput.size() == 8 );
        for( size_t n = 0; n < g_stub.lastInput.size(); ++n )
        {
            CHECK( g_stub.lastInput[n] == 0 );
        }

        REQUIRE( g_stub.memcpyKinds.size() == 2 );
        CHECK( g_stub.memcpyKinds[0] == cudaMemcpyHostToDevice );
        CHECK( g_stub.memcpyKinds[1] == cudaMemcpyDeviceToHost );

        for( int n = 0; n < 4; ++n )
        {
            CHECK( app.m_output[n] == g_stub.nextOutput[n] );
            CHECK( app.integrator_commands[n] == 0 ); // the integrator is not used
        }

        CHECK( app.frame_counter == 1 );
        CHECK( app.episode_counter == 2 );

        // Reload, then the next frame still uses the old engine and swaps in the new one afterwards.
        app.engineName = "po4ao_test_infer_b.plan";
        app.reload_engine();
        nvinfer1::ICudaEngine *second = app.engine2;

        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
        CHECK( g_stub.lastContextId == 1 );
        CHECK( app.engine == second );
        CHECK( app.engineReloaded == false );

        g_stub.nextOutput = { -1, -2, -3, -4 };
        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
        CHECK( g_stub.executeCalls == 3 );
        CHECK( g_stub.lastContextId == 2 );

        for( int n = 0; n < 4; ++n )
        {
            CHECK( app.m_output[n] == g_stub.nextOutput[n] );
        }

        app.shutdownForTest();
    }

    std::remove( "/tmp/po4ao_test_infer_a.plan" );
    std::remove( "/tmp/po4ao_test_infer_b.plan" );
    std::filesystem::remove_all( c_shmDir );
}

/// Verify appShutdown() releases the engines and device memory.
/**
 * \ingroup po4ao_unit_test
 */
TEST_CASE( "po4ao appShutdown releases the engines", "[po4ao]" )
{
    // clang-format off
    #ifdef PO4AO_TEST_DOXYGEN_REF
    po4ao::appShutdown();
    #endif
    // clang-format on

    g_stub = po4aoStubs::po4aoStubState();

    po4ao_test app( "po4ao" );
    setupEngine( app, "po4ao_test_shutdown.plan", "ENGINE", 2, 2, 4 );
    app.reload_engine();

    app.shutdownForTest();

    CHECK( g_stub.runtimesDestroyed == 1 );
    CHECK( g_stub.enginesDestroyed == 2 );
    CHECK( g_stub.contextsDestroyed == 2 );
    CHECK( g_stub.mallocCalls == 2 );
    CHECK( g_stub.freeCalls == 2 );

    std::remove( "/tmp/po4ao_test_shutdown.plan" );
}

} // namespace po4aoTest

} // namespace libXWCTest
