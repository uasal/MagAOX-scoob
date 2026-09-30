/** \file nnReconstructor_test.cpp
 * \brief Catch2 tests for the nnReconstructor app.
 * \author Claude Code
 */

#include "../../../tests/testXWC.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "../nnReconstructor.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Fake CUDA runtime state: "device" memory is host memory from malloc.
struct cudaStubState
{
    /// Free any leftover allocations and reset the counters.
    void reset()
    {
        for( auto &it : allocations )
        {
            free( it.first );
        }
        allocations.clear();
        mallocSizes.clear();
        freeCalls = 0;
        memcpyKinds.clear();
        memcpyCounts.clear();
    }

    std::map<void *, size_t> allocations; ///< Live allocations and their sizes in bytes.

    std::vector<size_t> mallocSizes; ///< Sizes passed to cudaMalloc, in order.

    int freeCalls{ 0 }; ///< Number of cudaFree calls.

    std::vector<int> memcpyKinds; ///< Directions passed to cudaMemcpy, in order.

    std::vector<size_t> memcpyCounts; ///< Byte counts passed to cudaMemcpy, in order.
};

/// The global fake CUDA state.
cudaStubState g_cuda;

/// Fake TensorRT state driving the fake runtime, engine and execution context.
struct trtStubState
{
    /// Reset to a 4x6x6 input, 5 output engine with no calls recorded.
    void reset()
    {
        createRuntimeCalls = 0;
        lastLogger         = nullptr;
        deserializedData.clear();
        tensorNames = { "pupils", "modes" };
        shapeQueries.clear();
        inputDims.nbDims  = 4;
        inputDims.d[0]    = 1;
        inputDims.d[1]    = 4;
        inputDims.d[2]    = 6;
        inputDims.d[3]    = 6;
        outputDims.nbDims = 2;
        outputDims.d[0]   = 1;
        outputDims.d[1]   = 5;
        nBindings         = 2;
        executeCalls      = 0;
        lastInput.clear();
        lastInput2.clear();
        output.clear();
        runtimeDeletes = 0;
        engineDeletes  = 0;
        contextDeletes = 0;
    }

    int                      createRuntimeCalls{ 0 }; ///< Number of createInferRuntime calls.
    nvinfer1::ILogger       *lastLogger{ nullptr };   ///< The logger passed to createInferRuntime.
    std::vector<char>        deserializedData;        ///< The blob passed to deserializeCudaEngine.
    std::vector<std::string> tensorNames;             ///< IO tensor names, input first.
    std::vector<std::string> shapeQueries;            ///< Names passed to getTensorShape, in order.
    nvinfer1::Dims           inputDims;               ///< Shape of the input tensor (NCHW).
    nvinfer1::Dims           outputDims;              ///< Shape of the output tensor.
    int                      nBindings{ 2 };          ///< Number of bindings executeV2 expects; the last is the output.
    int                      executeCalls{ 0 };       ///< Number of executeV2 calls.
    std::vector<char>        lastInput;               ///< Contents of binding 0 at the last executeV2.
    std::vector<char>        lastInput2;              ///< Contents of binding 1 at the last executeV2, with 3 bindings.
    std::vector<char>        output;                  ///< Bytes written to the output binding by executeV2.
    int                      runtimeDeletes{ 0 };     ///< Number of fake runtimes destroyed.
    int                      engineDeletes{ 0 };      ///< Number of fake engines destroyed.
    int                      contextDeletes{ 0 };     ///< Number of fake contexts destroyed.
};

/// The global fake TensorRT state.
trtStubState g_trt;

/// Copy the contents of a fake device allocation.
/**
 * \returns the bytes of the allocation, or an empty vector if the pointer is not a live allocation
 */
std::vector<char> deviceContents( void *ptr /**< [in] the device pointer */ )
{
    auto it = g_cuda.allocations.find( ptr );
    if( it == g_cuda.allocations.end() )
    {
        return std::vector<char>();
    }
    char *p = static_cast<char *>( ptr );
    return std::vector<char>( p, p + it->second );
}

/// Fake execution context which records its inputs and writes the scripted output.
class fakeContext : public nvinfer1::IExecutionContext
{
  public:
    ~fakeContext() noexcept
    {
        ++g_trt.contextDeletes;
    }

    bool executeV2( void *const *bindings ) noexcept
    {
        ++g_trt.executeCalls;
        g_trt.lastInput = deviceContents( bindings[0] );
        if( g_trt.nBindings == 3 )
        {
            g_trt.lastInput2 = deviceContents( bindings[1] );
        }
        void *out = bindings[g_trt.nBindings - 1];
        auto  it  = g_cuda.allocations.find( out );
        if( it != g_cuda.allocations.end() )
        {
            memcpy( out, g_trt.output.data(), std::min( it->second, g_trt.output.size() ) );
        }
        return true;
    }
};

/// Fake engine reporting the shapes in g_trt.
class fakeEngine : public nvinfer1::ICudaEngine
{
  public:
    ~fakeEngine() noexcept
    {
        ++g_trt.engineDeletes;
    }

    nvinfer1::IExecutionContext *createExecutionContext() noexcept
    {
        return new fakeContext;
    }

    int32_t getNbIOTensors() const noexcept
    {
        return g_trt.tensorNames.size();
    }

    const char *getIOTensorName( int32_t index ) const noexcept
    {
        return g_trt.tensorNames[index].c_str();
    }

    nvinfer1::Dims getTensorShape( const char *tensorName ) const noexcept
    {
        g_trt.shapeQueries.push_back( tensorName );
        if( g_trt.tensorNames[0] == tensorName )
        {
            return g_trt.inputDims;
        }
        return g_trt.outputDims;
    }
};

/// Fake runtime which records the deserialized blob.
class fakeRuntime : public nvinfer1::IRuntime
{
  public:
    ~fakeRuntime() noexcept
    {
        ++g_trt.runtimeDeletes;
    }

    nvinfer1::ICudaEngine *deserializeCudaEngine( const void *blob, std::size_t size ) noexcept
    {
        const char *p = static_cast<const char *>( blob );
        g_trt.deserializedData.assign( p, p + size );
        return new fakeEngine;
    }
};

namespace nvinfer1
{

IRuntime *createInferRuntime( ILogger &logger ) noexcept
{
    ++g_trt.createRuntimeCalls;
    g_trt.lastLogger = &logger;
    return new fakeRuntime;
}

} // namespace nvinfer1

float __half2float( const __half a )
{
    uint32_t sign = static_cast<uint32_t>( a.__x & 0x8000 ) << 16;
    uint32_t exp  = ( a.__x >> 10 ) & 0x1F;
    uint32_t mant = a.__x & 0x3FF;
    uint32_t f;

    if( exp == 0 )
    {
        if( mant == 0 )
        {
            f = sign;
        }
        else // subnormal: normalize
        {
            int e = -1;
            do
            {
                ++e;
                mant <<= 1;
            } while( !( mant & 0x400 ) );
            mant &= 0x3FF;
            f = sign | ( static_cast<uint32_t>( 127 - 15 - e ) << 23 ) | ( mant << 13 );
        }
    }
    else if( exp == 31 )
    {
        f = sign | 0x7F800000 | ( mant << 13 );
    }
    else
    {
        f = sign | ( ( exp - 15 + 127 ) << 23 ) | ( mant << 13 );
    }

    float out;
    memcpy( &out, &f, sizeof( out ) );
    return out;
}

__half __float2half( const float a )
{
    uint32_t f;
    memcpy( &f, &a, sizeof( f ) );

    uint32_t sign = ( f >> 16 ) & 0x8000;
    uint32_t fexp = ( f >> 23 ) & 0xFF;
    int32_t  exp  = static_cast<int32_t>( fexp ) - 127 + 15;
    uint32_t mant = f & 0x7FFFFF;

    __half h;
    if( fexp == 0xFF ) // inf or nan
    {
        h.__x = sign | 0x7C00 | ( mant ? 0x200 : 0 );
        return h;
    }
    if( exp >= 31 ) // overflow to inf
    {
        h.__x = sign | 0x7C00;
        return h;
    }
    if( exp <= 0 ) // subnormal or zero
    {
        if( exp < -10 )
        {
            h.__x = sign;
            return h;
        }
        mant |= 0x800000;
        uint32_t shift = 14 - exp;
        uint32_t hm    = mant >> shift;
        uint32_t rem   = mant & ( ( 1u << shift ) - 1 );
        uint32_t halfw = 1u << ( shift - 1 );
        if( rem > halfw || ( rem == halfw && ( hm & 1 ) ) )
        {
            ++hm;
        }
        h.__x = sign | hm;
        return h;
    }

    uint32_t hm  = mant >> 13;
    uint32_t rem = mant & 0x1FFF;
    uint32_t out = sign | ( static_cast<uint32_t>( exp ) << 10 ) | hm;
    if( rem > 0x1000 || ( rem == 0x1000 && ( hm & 1 ) ) )
    {
        ++out; // a carry into the exponent is correct rounding
    }
    h.__x = out;
    return h;
}

extern "C"
{
    cudaError_t cudaMalloc( void **devPtr, size_t size )
    {
        *devPtr                     = malloc( size > 0 ? size : 1 );
        g_cuda.allocations[*devPtr] = size;
        g_cuda.mallocSizes.push_back( size );
        return cudaSuccess;
    }

    cudaError_t cudaFree( void *devPtr )
    {
        ++g_cuda.freeCalls;
        g_cuda.allocations.erase( devPtr );
        free( devPtr );
        return cudaSuccess;
    }

    cudaError_t cudaMemcpy( void *dst, const void *src, size_t count, enum cudaMemcpyKind kind )
    {
        g_cuda.memcpyKinds.push_back( kind );
        g_cuda.memcpyCounts.push_back( count );
        memcpy( dst, src, count );
        return cudaSuccess;
    }
}

/// \endcond

namespace libXWCTest
{

/** \defgroup nnReconstructor_unit_test nnReconstructor Unit Tests
 * \brief Unit tests for the nnReconstructor application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `nnReconstructor` unit tests.
/** \ingroup nnReconstructor_unit_test
 */
namespace nnReconstructorTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing nnReconstructor internals, with fake TensorRT and CUDA.
class nnReconstructor_test : public nnReconstructor
{
  public:
    /// Construct a harness with the given device name, resetting the stubs and initializing the semaphore.
    explicit nnReconstructor_test( const std::string &device /**< [in] INDI device name */ )
    {
        g_trt.reset();
        g_cuda.reset();
        m_configName = device;
        sem_init( &m_smSemaphore, 0, 0 );
    }

    /// Release the engine, device memory and host buffers, which the app only releases in appShutdown().
    ~nnReconstructor_test() noexcept
    {
        cleanup_engine_context();
        cleanup_engine_memory();
        delete[] pp_image;
        delete[] pp_image_half;
        delete[] pup_Is;
        delete[] pup_Is_half;
        delete[] modeval_half;
        sem_destroy( &m_smSemaphore );
    }

    /// Run setupConfig(), read the given config file, and run loadConfig().
    void configure( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();

        // Keep telemetry records out of the queue so nothing is written at destruction.
        m_tel.m_logLevel = logPrio::LOG_INFO;
    }

    /// Call appShutdown() and forget the pointers it freed without resetting.
    /**
     * \returns the appShutdown() return value
     */
    int shutdown()
    {
        int rv = appShutdown();
        forgetEngine();
        forgetDeviceMemory();
        pp_image      = nullptr;
        pp_image_half = nullptr;
        pup_Is        = nullptr;
        pup_Is_half   = nullptr;
        modeval_half  = nullptr;
        return rv;
    }

    /// Forget the engine pointers after cleanup_engine_context(), which does not reset them.
    void forgetEngine()
    {
        context = nullptr;
        engine  = nullptr;
        runtime = nullptr;
    }

    /// Forget the device pointers after cleanup_engine_memory(), which does not reset them.
    void forgetDeviceMemory()
    {
        d_input  = nullptr;
        d_input2 = nullptr;
        d_output = nullptr;
    }

    /// Set up the fps source property as appStartup() does, without registering it.
    void setupFpsSourceProp()
    {
        m_indiP_fpsSource = pcf::IndiProperty( pcf::IndiProperty::Number );
        m_indiP_fpsSource.setDevice( m_fpsSource );
        m_indiP_fpsSource.setName( "fps" );
    }

    /// The shmimMonitor (input WFS) image width.
    uint32_t &smWidth()
    {
        return dev::shmimMonitor<nnReconstructor>::m_width;
    }

    /// The shmimMonitor (input WFS) image height.
    uint32_t &smHeight()
    {
        return dev::shmimMonitor<nnReconstructor>::m_height;
    }

    /// The shmimMonitor (input WFS) stream name.
    std::string &smShmimName()
    {
        return dev::shmimMonitor<nnReconstructor>::m_shmimName;
    }

    /// The frameGrabber (output modal) stream name.
    std::string &fgShmimName()
    {
        return dev::frameGrabber<nnReconstructor>::m_shmimName;
    }

    /// The frameGrabber (output modal) type size.
    size_t &fgTypeSize()
    {
        return dev::frameGrabber<nnReconstructor>::m_typeSize;
    }

    /// Whether the frameGrabber owns its stream.
    bool &fgOwnShmim()
    {
        return dev::frameGrabber<nnReconstructor>::m_ownShmim;
    }

    /// The frameGrabber circular buffer length.
    uint32_t &fgCircBuffLength()
    {
        return dev::frameGrabber<nnReconstructor>::m_circBuffLength;
    }

    /// The frameGrabber current image timestamp.
    timespec &fgTimestamp()
    {
        return dev::frameGrabber<nnReconstructor>::m_currImageTimestamp;
    }

    /// The telemeter maximum interval.
    double &telMaxInterval()
    {
        return dev::telemeter<nnReconstructor>::m_maxInterval;
    }

    using nnReconstructor::acquireAndCheckValid;
    using nnReconstructor::allocate;
    using nnReconstructor::checkRecordTimes;
    using nnReconstructor::configureAcquisition;
    using nnReconstructor::fps;
    using nnReconstructor::loadImageIntoStream;
    using nnReconstructor::processImage;
    using nnReconstructor::reconfig;
    using nnReconstructor::recordTelem;
    using nnReconstructor::setCallBack_m_indiP_fpsSource;
    using nnReconstructor::startAcquisition;

    using nnReconstructor::context;
    using nnReconstructor::d_input;
    using nnReconstructor::d_input2;
    using nnReconstructor::d_output;
    using nnReconstructor::dataDirs;
    using nnReconstructor::engine;
    using nnReconstructor::engineData;
    using nnReconstructor::engineDirs;
    using nnReconstructor::engineName;
    using nnReconstructor::explicit_tt;
    using nnReconstructor::imageNorm;
    using nnReconstructor::input2Size;
    using nnReconstructor::inputC;
    using nnReconstructor::inputH;
    using nnReconstructor::inputSize;
    using nnReconstructor::inputW;
    using nnReconstructor::logger;
    using nnReconstructor::m_fps;
    using nnReconstructor::m_fpsSource;
    using nnReconstructor::m_indiP_fps;
    using nnReconstructor::m_indiP_fpsSource;
    using nnReconstructor::m_indiSetCallBacks;
    using nnReconstructor::m_pupPix;
    using nnReconstructor::m_pwfsHeight;
    using nnReconstructor::m_pwfsWidth;
    using nnReconstructor::m_smSemaphore;
    using nnReconstructor::m_updated;
    using nnReconstructor::modalNorm;
    using nnReconstructor::modeval;
    using nnReconstructor::modeval_half;
    using nnReconstructor::Npup;
    using nnReconstructor::outputSize;
    using nnReconstructor::pixels_per_quadrant;
    using nnReconstructor::pp_image;
    using nnReconstructor::pp_image_half;
    using nnReconstructor::pup_Is;
    using nnReconstructor::pup_Is_half;
    using nnReconstructor::pup_offset1_x;
    using nnReconstructor::pup_offset1_y;
    using nnReconstructor::pup_offset2_x;
    using nnReconstructor::pup_offset2_y;
    using nnReconstructor::rebuildEngine;
    using nnReconstructor::runtime;
    using nnReconstructor::use_fp16;
    using nnReconstructor::zeroPad;
};

/// \endcond

/// Path of the fake engine file written by the tests.
const std::string engineFile = "/tmp/nnReconstructor_test_engine.trt";

/// Write the fake serialized engine file.
void writeEngineFile( const std::string &contents /**< [in] the file contents */ )
{
    std::ofstream fout( engineFile, std::ios::binary );
    fout.write( contents.data(), contents.size() );
}

/// Set the fake engine output to the given float values.
void setFloatOutput( const std::vector<float> &vals /**< [in] the output values */ )
{
    g_trt.output.resize( vals.size() * sizeof( float ) );
    memcpy( g_trt.output.data(), vals.data(), g_trt.output.size() );
}

/// Set the fake engine output to the given values converted to half precision.
void setHalfOutput( const std::vector<float> &vals /**< [in] the output values */ )
{
    std::vector<half> h( vals.size() );
    floatToHalfArray( h.data(), vals.data(), vals.size() );
    g_trt.output.resize( vals.size() * sizeof( half ) );
    memcpy( g_trt.output.data(), h.data(), g_trt.output.size() );
}

/// Load the fake engine, create the context, allocate device memory, and allocate() for a 16x16 WFS.
/** The fake engine has a 4x6x6 input and 5 outputs, which matches four 6x6 pupils.
 */
void setupPipeline( nnReconstructor_test &app,  /**< [in/out] the harness to set up */
                    bool                  fp16, /**< [in] use half precision */
                    bool                  tt /**< [in] use the explicit tip/tilt second input */ )
{
    writeEngineFile( "ENGINE" );
    app.load_engine( engineFile );
    app.create_engine_context();

    app.use_fp16    = fp16;
    app.explicit_tt = tt;
    g_trt.nBindings = tt ? 3 : 2;
    app.prepare_engine_memory();

    app.smWidth()     = 16;
    app.smHeight()    = 16;
    app.m_pupPix      = 6;
    app.zeroPad       = 2;
    app.pup_offset1_x = 1;
    app.pup_offset1_y = 2;
    app.pup_offset2_x = 9;
    app.pup_offset2_y = 10;
    app.imageNorm     = 0.5f;
    app.modalNorm     = 1.0f;
    app.fgTypeSize()  = sizeof( float );

    app.allocate( dev::shmimT() );
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

/// Verify the half-precision array conversion helpers.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor half precision conversion", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    halfToFloatArray(nullptr, nullptr, 0);
    floatToHalfArray(nullptr, nullptr, 0);
    #endif
    // clang-format on

    REQUIRE( sizeof( half ) == 2 );

    std::vector<float> in = { 0.0f, 1.0f, -2.25f, 1.5f, 65504.0f, 0.1f, 1000.25f, 3.0e-5f };
    std::vector<half>  h( in.size() );
    std::vector<float> out( in.size(), -99.0f );

    floatToHalfArray( h.data(), in.data(), in.size() );
    halfToFloatArray( out.data(), h.data(), in.size() );

    // exactly representable values
    REQUIRE( out[0] == 0.0f );
    REQUIRE( out[1] == 1.0f );
    REQUIRE( out[2] == -2.25f );
    REQUIRE( out[3] == 1.5f );
    REQUIRE( out[4] == 65504.0f );
    REQUIRE( h[1].__x == 0x3C00 );
    REQUIRE( h[2].__x == 0xC080 );

    // rounded to 11 significant bits
    REQUIRE( out[5] == Approx( 0.1f ).epsilon( 1e-3 ) );
    REQUIRE( out[6] == Approx( 1000.25f ).margin( 0.5 ) );
    REQUIRE( out[7] == Approx( 3.0e-5f ).epsilon( 1e-2 ) ); // subnormal in half

    // only num_elements are converted
    std::vector<float> partial( 3, -1.0f );
    halfToFloatArray( partial.data(), h.data(), 2 );
    REQUIRE( partial[0] == 0.0f );
    REQUIRE( partial[1] == 1.0f );
    REQUIRE( partial[2] == -1.0f );
}

/// Verify the TensorRT logger prints warnings and errors only.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor TensorRT Logger", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    Logger::log(ILogger::Severity::kWARNING, "");
    #endif
    // clang-format on

    Logger             lg;
    nvinfer1::ILogger &il = lg;

    std::stringstream capture;
    std::streambuf   *old = std::cout.rdbuf( capture.rdbuf() );

    il.log( nvinfer1::ILogger::Severity::kINTERNAL_ERROR, "internal" );
    il.log( nvinfer1::ILogger::Severity::kERROR, "error" );
    il.log( nvinfer1::ILogger::Severity::kWARNING, "warning" );
    il.log( nvinfer1::ILogger::Severity::kINFO, "info" );
    il.log( nvinfer1::ILogger::Severity::kVERBOSE, "verbose" );

    std::cout.rdbuf( old );

    REQUIRE( capture.str() == "internal\nerror\nwarning\n" );
}

/// Verify configuration defaults and overrides, including the base-class configuration.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor configuration", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::nnReconstructor();
    nnReconstructor::setupConfig();
    nnReconstructor::loadConfig();
    nnReconstructor::loadConfigImpl(config);
    #endif
    // clang-format on

    SECTION( "defaults" )
    {
        std::string file = "/tmp/nnReconstructor_test_defaults.conf";
        mx::app::writeConfigFile( file, { "none" }, { "nada" }, { "0" } );

        nnReconstructor_test app( "nnrecon" );
        app.configure( file );

        // imageNorm, modalNorm, m_pupPix and the pupil offsets have no default and are not checked.
        REQUIRE( app.dataDirs == "" );
        REQUIRE( app.engineDirs == "" );
        REQUIRE( app.engineName == "" );
        REQUIRE( app.rebuildEngine == false );
        REQUIRE( app.use_fp16 == false );
        REQUIRE( app.explicit_tt == false );
        REQUIRE( app.m_fpsSource == "camwfs" );
        REQUIRE( app.smShmimName() == "nnrecon" );
        REQUIRE( app.fgShmimName() == "nnrecon" );
        REQUIRE( app.fgOwnShmim() == false ); // the output stream is not owned
        REQUIRE( app.fgCircBuffLength() == 1 );
        REQUIRE( app.telMaxInterval() == Approx( 10.0 ) );
        REQUIRE( app.Npup == 4 );
        REQUIRE( app.zeroPad == 2 );
        REQUIRE( app.input2Size == 4 );
        REQUIRE( app.runtime == nullptr );
        REQUIRE( app.engine == nullptr );
        REQUIRE( app.context == nullptr );

        remove( file.c_str() );
    }

    SECTION( "overrides" )
    {
        std::string file = "/tmp/nnReconstructor_test_overrides.conf";
        mx::app::writeConfigFile( file,
                                  { "shmimMonitor",
                                    "framegrabber",
                                    "framegrabber",
                                    "telemeter",
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
                                    "shmimName",
                                    "circBuffLength",
                                    "maxInterval",
                                    "dataDirs",
                                    "engineDirs",
                                    "engineName",
                                    "rebuildEngine",
                                    "imageNorm",
                                    "modalNorm",
                                    "use_fp16",
                                    "explicit_tt",
                                    "m_pupPix",
                                    "pup_offset1_x",
                                    "pup_offset1_y",
                                    "pup_offset2_x",
                                    "pup_offset2_y" },
                                  { "aol1_imWFS2",
                                    "aol1_modevalWFS",
                                    "3",
                                    "5.5",
                                    "/data",
                                    "/engines",
                                    "model.trt",
                                    "true",
                                    "0.125",
                                    "3.5",
                                    "true",
                                    "true",
                                    "56",
                                    "3",
                                    "4",
                                    "65",
                                    "66" } );

        nnReconstructor_test app( "nnrecon" );
        app.configure( file );

        REQUIRE( app.smShmimName() == "aol1_imWFS2" );
        REQUIRE( app.fgShmimName() == "aol1_modevalWFS" );
        REQUIRE( app.fgCircBuffLength() == 3 );
        REQUIRE( app.telMaxInterval() == Approx( 5.5 ) );
        REQUIRE( app.dataDirs == "/data" );
        REQUIRE( app.engineDirs == "/engines" );
        REQUIRE( app.engineName == "model.trt" );
        REQUIRE( app.rebuildEngine == true );
        REQUIRE( app.imageNorm == Approx( 0.125f ) );
        REQUIRE( app.modalNorm == Approx( 3.5f ) );
        REQUIRE( app.use_fp16 == true );
        REQUIRE( app.explicit_tt == true );
        REQUIRE( app.m_pupPix == 56 );
        REQUIRE( app.pup_offset1_x == 3 );
        REQUIRE( app.pup_offset1_y == 4 );
        REQUIRE( app.pup_offset2_x == 65 );
        REQUIRE( app.pup_offset2_y == 66 );

        remove( file.c_str() );
    }
}

/// Verify load_engine reads the engine file and fails for a missing file.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor load_engine", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::load_engine("");
    #endif
    // clang-format on

    nnReconstructor_test app( "nnrecon" );

    SECTION( "reads the whole file, including binary bytes" )
    {
        std::string contents( "TRT\0engine\xff\x01", 12 );
        writeEngineFile( contents );
        app.load_engine( engineFile );
        REQUIRE( app.engineData.size() == 12 );
        REQUIRE( std::string( app.engineData.data(), app.engineData.size() ) == contents );
    }

    SECTION( "an empty file gives empty engine data" )
    {
        writeEngineFile( "" );
        app.load_engine( engineFile );
        REQUIRE( app.engineData.size() == 0 );
    }

    SECTION( "a missing file throws" )
    {
        // The open error is only printed; tellg() then returns -1, which is too large a vector size.
        REQUIRE_THROWS( app.load_engine( "/tmp/nnReconstructor_test_no_such_engine.trt" ) );
    }

    remove( engineFile.c_str() );
}

/// Verify create_engine_context deserializes the engine and reads the tensor shapes.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor create_engine_context", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::create_engine_context();
    #endif
    // clang-format on

    nnReconstructor_test app( "nnrecon" );
    writeEngineFile( "serialized-engine" );
    app.load_engine( engineFile );

    SECTION( "default shapes" )
    {
        app.create_engine_context();

        REQUIRE( g_trt.createRuntimeCalls == 1 );
        REQUIRE( g_trt.lastLogger == static_cast<nvinfer1::ILogger *>( &app.logger ) );
        REQUIRE( std::string( g_trt.deserializedData.data(), g_trt.deserializedData.size() ) == "serialized-engine" );
        REQUIRE( app.runtime != nullptr );
        REQUIRE( app.engine != nullptr );
        REQUIRE( app.context != nullptr );

        // tensor 0 is the input, tensor 1 the output
        REQUIRE( g_trt.shapeQueries == std::vector<std::string>( { "pupils", "modes" } ) );
        REQUIRE( app.inputC == 4 );
        REQUIRE( app.inputH == 6 );
        REQUIRE( app.inputW == 6 );
        REQUIRE( app.inputSize == 144 );
        REQUIRE( app.outputSize == 5 );
    }

    SECTION( "other shapes" )
    {
        g_trt.inputDims.d[1]  = 2;
        g_trt.inputDims.d[2]  = 60;
        g_trt.inputDims.d[3]  = 62;
        g_trt.outputDims.d[1] = 2500;

        app.create_engine_context();

        REQUIRE( app.inputC == 2 );
        REQUIRE( app.inputH == 60 );
        REQUIRE( app.inputW == 62 );
        REQUIRE( app.inputSize == 2 * 60 * 62 );
        REQUIRE( app.outputSize == 2500 );
    }

    remove( engineFile.c_str() );
}

/// Verify prepare_engine_memory allocates device buffers of the right sizes for each precision and input mode.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor prepare_engine_memory and cleanup", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::prepare_engine_memory();
    nnReconstructor::cleanup_engine_memory();
    nnReconstructor::cleanup_engine_context();
    #endif
    // clang-format on

    nnReconstructor_test app( "nnrecon" );
    app.inputSize  = 144;
    app.outputSize = 5;

    SECTION( "fp32, one input" )
    {
        app.prepare_engine_memory();
        REQUIRE( g_cuda.mallocSizes == std::vector<size_t>( { 144 * 4, 5 * 4 } ) );
        REQUIRE( app.d_input != nullptr );
        REQUIRE( app.d_input2 == nullptr );
        REQUIRE( app.d_output != nullptr );

        app.cleanup_engine_memory();
        REQUIRE( g_cuda.freeCalls == 2 );
        REQUIRE( g_cuda.allocations.size() == 0 );
        app.forgetDeviceMemory();
    }

    SECTION( "fp32, explicit tip/tilt input" )
    {
        app.explicit_tt = true;
        app.prepare_engine_memory();
        REQUIRE( g_cuda.mallocSizes == std::vector<size_t>( { 144 * 4, 4 * 4, 5 * 4 } ) );
        REQUIRE( app.d_input2 != nullptr );

        app.cleanup_engine_memory();
        REQUIRE( g_cuda.freeCalls == 3 );
        REQUIRE( g_cuda.allocations.size() == 0 );
        app.forgetDeviceMemory();
    }

    SECTION( "fp16, one input" )
    {
        app.use_fp16 = true;
        app.prepare_engine_memory();
        REQUIRE( g_cuda.mallocSizes == std::vector<size_t>( { 144 * 2, 5 * 2 } ) );
    }

    SECTION( "fp16, explicit tip/tilt input" )
    {
        app.use_fp16    = true;
        app.explicit_tt = true;
        app.prepare_engine_memory();
        REQUIRE( g_cuda.mallocSizes == std::vector<size_t>( { 144 * 2, 4 * 2, 5 * 2 } ) );
    }

    SECTION( "cleanup_engine_context deletes the context, engine and runtime" )
    {
        writeEngineFile( "E" );
        app.load_engine( engineFile );
        app.create_engine_context();

        app.cleanup_engine_context();
        REQUIRE( g_trt.contextDeletes == 1 );
        REQUIRE( g_trt.engineDeletes == 1 );
        REQUIRE( g_trt.runtimeDeletes == 1 );
        app.forgetEngine();

        remove( engineFile.c_str() );
    }

    SECTION( "cleanup with nothing allocated is a no-op" )
    {
        app.cleanup_engine_memory();
        app.cleanup_engine_context();
        REQUIRE( g_cuda.freeCalls == 0 );
        REQUIRE( g_trt.contextDeletes == 0 );
    }
}

/// Verify allocate() sizes and zeroes the host buffers.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor allocate", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::allocate(dev::shmimT());
    #endif
    // clang-format on

    SECTION( "fp32" )
    {
        nnReconstructor_test app( "nnrecon" );
        setupPipeline( app, false, false );

        REQUIRE( app.m_pwfsWidth == 16 );
        REQUIRE( app.m_pwfsHeight == 16 );
        REQUIRE( app.pixels_per_quadrant == 36 );
        REQUIRE( app.pp_image != nullptr );
        REQUIRE( app.pup_Is != nullptr );
        REQUIRE( app.pp_image_half == nullptr );
        REQUIRE( app.modeval_half == nullptr );
        REQUIRE( app.pup_Is_half == nullptr );
        for( int n = 0; n < 4 * 36; ++n )
        {
            REQUIRE( app.pp_image[n] == 0.0f );
        }
        REQUIRE( app.modeval.rows() == 5 );
        REQUIRE( app.modeval.cols() == 1 );
        for( int n = 0; n < 5; ++n )
        {
            REQUIRE( app.modeval( n, 0 ) == 0.0f );
        }
    }

    SECTION( "fp16 with explicit tip/tilt" )
    {
        nnReconstructor_test app( "nnrecon" );
        setupPipeline( app, true, true );

        REQUIRE( app.pp_image_half != nullptr );
        REQUIRE( app.modeval_half != nullptr );
        REQUIRE( app.pup_Is_half != nullptr );
        for( int n = 0; n < 4 * 36; ++n )
        {
            REQUIRE( app.pp_image_half[n].__x == 0 );
        }
        for( int n = 0; n < 4; ++n )
        {
            REQUIRE( app.pup_Is[n] == 0.0f );
            REQUIRE( app.pup_Is_half[n].__x == 0 );
        }
        for( int n = 0; n < 5; ++n )
        {
            REQUIRE( app.modeval_half[n].__x == 0 );
        }
    }

    remove( engineFile.c_str() );
}

/// Verify processImage in fp32: pupil extraction, device copies, inference, and the frameGrabber hand-off.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor processImage fp32", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::processImage(nullptr, dev::shmimT());
    nnReconstructor::acquireAndCheckValid();
    nnReconstructor::loadImageIntoStream(nullptr);
    #endif
    // clang-format on

    nnReconstructor_test app( "nnrecon" );
    setupPipeline( app, false, false );
    setFloatOutput( { 0.5f, -1.0f, 2.0f, 3.25f, 4.0f } );

    std::vector<float> frame = makeFrame( 7 );

    REQUIRE( app.m_updated == false );
    REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );

    // preprocessed image
    std::vector<float> pp = expectedPP( frame );
    for( int n = 0; n < 4 * 36; ++n )
    {
        REQUIRE( app.pp_image[n] == Approx( pp[n] ) );
    }
    REQUIRE( app.pp_image[14] == Approx( 0.5f * ( 18 + 7 ) ) ); // pupil 0 first valid pixel is frame(2,1)
    REQUIRE( app.pp_image[0] == 0.0f );                         // zero padding

    // the whole preprocessed image was sent to the device and used as the input binding
    REQUIRE( g_cuda.memcpyKinds == std::vector<int>( { cudaMemcpyHostToDevice, cudaMemcpyDeviceToHost } ) );
    REQUIRE( g_cuda.memcpyCounts == std::vector<size_t>( { 144 * 4, 5 * 4 } ) );
    REQUIRE( g_trt.executeCalls == 1 );
    REQUIRE( g_trt.lastInput.size() == 144 * 4 );
    REQUIRE( memcmp( g_trt.lastInput.data(), app.pp_image, 144 * 4 ) == 0 );

    // the output came back into modeval
    REQUIRE( app.modeval( 0, 0 ) == Approx( 0.5f ) );
    REQUIRE( app.modeval( 1, 0 ) == Approx( -1.0f ) );
    REQUIRE( app.modeval( 2, 0 ) == Approx( 2.0f ) );
    REQUIRE( app.modeval( 3, 0 ) == Approx( 3.25f ) );
    REQUIRE( app.modeval( 4, 0 ) == Approx( 4.0f ) );

    REQUIRE( app.m_updated == true );
    REQUIRE( app.fgTimestamp().tv_sec > 0 );

    // the semaphore was posted, so the framegrabber side does not wait
    REQUIRE( app.acquireAndCheckValid() == 0 );

    std::vector<float> dest( 6, -9.0f );
    REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
    REQUIRE( app.m_updated == false );
    REQUIRE( dest[0] == Approx( 0.5f ) );
    REQUIRE( dest[4] == Approx( 4.0f ) );
    REQUIRE( dest[5] == -9.0f ); // only outputSize values are copied

    remove( engineFile.c_str() );
}

/// Verify processImage with the explicit tip/tilt second input.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor processImage explicit tip/tilt", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::processImage(nullptr, dev::shmimT());
    #endif
    // clang-format on

    nnReconstructor_test app( "nnrecon" );
    setupPipeline( app, false, true );
    setFloatOutput( { 1, 2, 3, 4, 5 } );

    std::vector<float> frame = makeFrame( 0 );
    REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );

    REQUIRE( g_cuda.memcpyKinds ==
             std::vector<int>( { cudaMemcpyHostToDevice, cudaMemcpyHostToDevice, cudaMemcpyDeviceToHost } ) );
    REQUIRE( g_cuda.memcpyCounts == std::vector<size_t>( { 144 * 4, 4 * 4, 5 * 4 } ) );

    // the pupil intensities input is sent, but processImage never computes it, so it stays zero
    REQUIRE( g_trt.lastInput2.size() == 4 * 4 );
    for( size_t n = 0; n < 4; ++n )
    {
        float v;
        memcpy( &v, g_trt.lastInput2.data() + n * sizeof( float ), sizeof( float ) );
        REQUIRE( v == 0.0f );
    }

    for( int n = 0; n < 5; ++n )
    {
        REQUIRE( app.modeval( n, 0 ) == Approx( n + 1 ) );
    }

    remove( engineFile.c_str() );
}

/// Verify processImage in half precision.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor processImage fp16", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::processImage(nullptr, dev::shmimT());
    #endif
    // clang-format on

    nnReconstructor_test app( "nnrecon" );
    setupPipeline( app, true, false );
    setHalfOutput( { 0.5f, -1.0f, 2.0f, 3.25f, 4.0f } );

    std::vector<float> frame = makeFrame( 0 );
    REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );

    REQUIRE( g_cuda.memcpyCounts == std::vector<size_t>( { 144 * 2, 5 * 2 } ) );

    // the device input holds the half-precision preprocessed image
    std::vector<float> pp = expectedPP( frame );
    REQUIRE( g_trt.lastInput.size() == 144 * 2 );
    for( int n = 0; n < 4 * 36; ++n )
    {
        half h;
        memcpy( &h, g_trt.lastInput.data() + n * sizeof( half ), sizeof( half ) );
        REQUIRE( __half2float( h ) == Approx( pp[n] ) ); // values are small integers or halves, exact in fp16
    }

    // the half-precision output was converted back
    REQUIRE( app.modeval( 0, 0 ) == 0.5f );
    REQUIRE( app.modeval( 1, 0 ) == -1.0f );
    REQUIRE( app.modeval( 3, 0 ) == 3.25f );
    REQUIRE( app.modeval( 4, 0 ) == 4.0f );
    REQUIRE( app.m_updated == true );

    remove( engineFile.c_str() );
}

/// Verify acquireAndCheckValid returns 1 when the semaphore is posted without an update, or times out.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor acquireAndCheckValid", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::acquireAndCheckValid();
    #endif
    // clang-format on

    nnReconstructor_test app( "nnrecon" );

    SECTION( "posted and updated" )
    {
        app.m_updated = true;
        REQUIRE( sem_post( &app.m_smSemaphore ) == 0 );
        REQUIRE( app.acquireAndCheckValid() == 0 );
    }

    SECTION( "posted but not updated" )
    {
        app.m_updated = false;
        REQUIRE( sem_post( &app.m_smSemaphore ) == 0 );
        REQUIRE( app.acquireAndCheckValid() == 1 );
    }

    SECTION( "not posted: times out after 1 second" )
    {
        app.m_updated = true;
        REQUIRE( app.acquireAndCheckValid() == 1 );
    }
}

/// Verify the simple frameGrabber hooks and configureAcquisition with a missing stream.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor frameGrabber hooks", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::configureAcquisition();
    nnReconstructor::fps();
    nnReconstructor::startAcquisition();
    nnReconstructor::reconfig();
    #endif
    // clang-format on

    nnReconstructor_test app( "nnrecon" );

    REQUIRE( app.fps() == 0.0f );
    app.m_fps = 1500.0f;
    REQUIRE( app.fps() == Approx( 1500.0f ) );

    REQUIRE( app.startAcquisition() == 0 );
    REQUIRE( app.reconfig() == 0 );

    // The output stream is not owned, so it must exist; a missing stream means try again later.
    app.fgShmimName() = "nnReconstructor_test_no_such_stream";
    REQUIRE( app.configureAcquisition() == 1 );
}

/// Verify the fps source set-property callback.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor setCallBack_m_indiP_fpsSource", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::setCallBack_m_indiP_fpsSource(pcf::IndiProperty());
    #endif
    // clang-format on

    nnReconstructor_test app( "nnrecon" );
    app.setupFpsSourceProp();

    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "camwfs" );
    ip.setName( "fps" );

    SECTION( "wrong device or name is rejected" )
    {
        pcf::IndiProperty bad( pcf::IndiProperty::Number );
        bad.setDevice( "camwfs2" );
        bad.setName( "fps" );
        bad.add( pcf::IndiElement( "current", 100.0 ) );
        REQUIRE( app.setCallBack_m_indiP_fpsSource( bad ) == -1 );

        bad.setDevice( "camwfs" );
        bad.setName( "exptime" );
        REQUIRE( app.setCallBack_m_indiP_fpsSource( bad ) == -1 );
        REQUIRE( app.m_fps == 0.0f );
    }

    SECTION( "missing current is rejected" )
    {
        ip.add( pcf::IndiElement( "target", 100.0 ) );
        REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == -1 );
        REQUIRE( app.m_fps == 0.0f );
    }

    SECTION( "current sets the fps" )
    {
        ip.add( pcf::IndiElement( "current", 1234.5 ) );
        REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
        REQUIRE( app.m_fps == Approx( 1234.5f ) );
        REQUIRE( app.fps() == Approx( 1234.5f ) );

        // unchanged value is accepted too
        REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
        REQUIRE( app.m_fps == Approx( 1234.5f ) );
    }

    std::unique_lock<std::mutex> lock( app.m_indiMutex, std::try_to_lock );
    REQUIRE( lock.owns_lock() );
}

/// Verify appStartup registers the fps properties and fails (by throwing) when the engine file is missing.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor appStartup with a missing engine", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::appStartup();
    #endif
    // clang-format on

    std::string file = "/tmp/nnReconstructor_test_startup.conf";
    mx::app::writeConfigFile( file,
                              { "parameters", "parameters" },
                              { "engineDirs", "engineName" },
                              { "/tmp/nnReconstructor_test_no_such_dir", "none.trt" } );

    nnReconstructor_test app( "nnrecon" );
    app.configure( file );

    REQUIRE_THROWS( app.appStartup() );

    // the INDI properties set up before the engine load
    REQUIRE( app.m_indiSetCallBacks.count( "camwfs.fps" ) == 1 );
    REQUIRE( app.m_indiP_fpsSource.getDevice() == "camwfs" );
    REQUIRE( app.m_indiP_fpsSource.getName() == "fps" );
    REQUIRE( app.m_indiP_fps.getName() == "fps" );
    REQUIRE( app.m_indiP_fps.find( "current" ) );

    // no runtime was created and the state was not changed
    REQUIRE( g_trt.createRuntimeCalls == 0 );
    REQUIRE( app.state() == stateCodes::UNINITIALIZED );

    remove( file.c_str() );
}

/// Verify appShutdown releases the engine, device memory and host buffers.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor appShutdown", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::appShutdown();
    #endif
    // clang-format on

    nnReconstructor_test app( "nnrecon" );
    setupPipeline( app, true, true );
    REQUIRE( g_cuda.allocations.size() == 3 );

    REQUIRE( app.shutdown() == 0 );

    REQUIRE( g_trt.contextDeletes == 1 );
    REQUIRE( g_trt.engineDeletes == 1 );
    REQUIRE( g_trt.runtimeDeletes == 1 );
    REQUIRE( g_cuda.freeCalls == 3 );
    REQUIRE( g_cuda.allocations.size() == 0 );

    remove( engineFile.c_str() );
}

/// Verify the telemetry hooks.
/**
 * \ingroup nnReconstructor_unit_test
 */
TEST_CASE( "nnReconstructor telemetry", "[nnReconstructor]" )
{
    // clang-format off
    #ifdef NNRECONSTRUCTOR_TEST_DOXYGEN_REF
    nnReconstructor::checkRecordTimes();
    nnReconstructor::recordTelem(nullptr);
    #endif
    // clang-format on

    std::string file = "/tmp/nnReconstructor_test_telem.conf";
    mx::app::writeConfigFile( file, { "none" }, { "nada" }, { "0" } );

    nnReconstructor_test app( "nnrecon" );
    app.configure( file );

    REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_fgtimings *>( nullptr ) ) == 0 );
    REQUIRE( app.checkRecordTimes() == 0 );

    remove( file.c_str() );
}

} // namespace nnReconstructorTest

} // namespace libXWCTest
