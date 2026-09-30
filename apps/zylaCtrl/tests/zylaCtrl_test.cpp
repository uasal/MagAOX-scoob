/** \file zylaCtrl_test.cpp
 * \brief Catch2 tests for the zylaCtrl app.
 * \author Claude Code
 *
 * \ingroup zylaCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <utility>
#include <vector>

#define protected public
#include "../zylaCtrl.hpp"
#undef protected

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace
{

/// The handle AT_Open returns for camera index 0.  Index n gets c_zylaHandleBase + n.
constexpr AT_H c_zylaHandleBase = 100;

/// Arguments captured from the last AT_ConvertBuffer call.
struct zylaConvertCall
{
    AT_U8 *input{ nullptr }; ///< The raw input buffer.

    AT_U8 *output{ nullptr }; ///< The destination buffer.

    AT_64 width{ 0 }; ///< The image width.

    AT_64 height{ 0 }; ///< The image height.

    AT_64 stride{ 0 }; ///< The raw row stride in bytes.

    std::wstring inputEncoding; ///< The raw pixel encoding.

    std::wstring outputEncoding; ///< The requested output pixel encoding.
};

/// Fake Andor SDK3 state shared by the stub functions below.
struct zylaStubState
{
    int initLibReturn{ AT_SUCCESS }; ///< Return code of AT_InitialiseLibrary.

    int initUtilReturn{ AT_SUCCESS }; ///< Return code of AT_InitialiseUtilityLibrary.

    int finLibReturn{ AT_SUCCESS }; ///< Return code of AT_FinaliseLibrary.

    int finUtilReturn{ AT_SUCCESS }; ///< Return code of AT_FinaliseUtilityLibrary.

    int openReturn{ AT_SUCCESS }; ///< Return code of AT_Open.

    int closeReturn{ AT_SUCCESS }; ///< Return code of AT_Close.

    int flushReturn{ AT_SUCCESS }; ///< Return code of AT_Flush.

    int queueReturn{ AT_SUCCESS }; ///< Return code of AT_QueueBuffer.

    int waitReturn{ AT_SUCCESS }; ///< Return code of AT_WaitBuffer.

    int initLibCalls{ 0 }; ///< Number of AT_InitialiseLibrary calls.

    int initUtilCalls{ 0 }; ///< Number of AT_InitialiseUtilityLibrary calls.

    int finLibCalls{ 0 }; ///< Number of AT_FinaliseLibrary calls.

    int finUtilCalls{ 0 }; ///< Number of AT_FinaliseUtilityLibrary calls.

    int flushCalls{ 0 }; ///< Number of AT_Flush calls.

    int convertCalls{ 0 }; ///< Number of AT_ConvertBuffer calls.

    std::vector<int> opened; ///< Camera indices passed to AT_Open, in order.

    std::vector<AT_H> closed; ///< Handles passed to AT_Close, in order.

    std::vector<std::wstring> serials; ///< Serial number of each camera, by index.

    std::vector<std::wstring> models; ///< Model name of each camera, by index.

    std::map<std::wstring, int> featureErrors; ///< Return code of any call naming this feature (or command).

    std::map<std::wstring, AT_64> ints; ///< Values returned by AT_GetInt, by feature.

    std::map<std::wstring, AT_64> setInts; ///< Values sent with AT_SetInt, by feature.

    std::map<std::wstring, double> floats; ///< Values returned by AT_GetFloat, by feature.

    std::map<std::wstring, double> setFloats; ///< Values sent with AT_SetFloat, by feature.

    std::map<std::wstring, AT_BOOL> bools; ///< Values returned by AT_GetBool, by feature.

    std::map<std::wstring, AT_BOOL> setBools; ///< Values sent with AT_SetBool, by feature.

    std::map<std::wstring, int> enumIndex; ///< Values returned by AT_GetEnumIndex, by feature.

    std::map<std::wstring, std::vector<std::wstring>> enumStrings; ///< Enumerated strings, by feature.

    std::map<std::wstring, std::wstring> setEnumStrings; ///< Values sent with AT_SetEnumString, by feature.

    std::vector<std::wstring> commands; ///< Commands sent with AT_Command, in order.

    std::vector<std::pair<AT_U8 *, int>> queued; ///< Buffers and sizes passed to AT_QueueBuffer, in order.

    AT_U8 *waitPtr{ nullptr }; ///< Buffer returned by a successful AT_WaitBuffer.

    int waitSize{ 0 }; ///< Buffer size returned by a successful AT_WaitBuffer.

    unsigned int lastTimeout{ 0 }; ///< Timeout passed to the last AT_WaitBuffer.

    zylaConvertCall lastConvert; ///< Arguments of the last AT_ConvertBuffer call.

    uint16_t convertFill{ 0 }; ///< 16-bit value AT_ConvertBuffer writes to each output pixel.
};

/// The global fake Andor SDK3 state.
zylaStubState g_zylaStub;

/// Reset the fake Andor SDK3 state before a test.
void resetZylaStub()
{
    g_zylaStub = zylaStubState();
}

/// Get the forced return code for a feature, or AT_SUCCESS if none is set.
int featureError( const AT_WC *feature /**< [in] the feature name */ )
{
    auto it = g_zylaStub.featureErrors.find( feature );
    if( it == g_zylaStub.featureErrors.end() )
    {
        return AT_SUCCESS;
    }

    return it->second;
}

/// Copy a wide string into an SDK output buffer, writing at most len characters including the terminator.
void copyWide( AT_WC              *dest, /**< [out] the output buffer */
               int                 len,  /**< [in] the buffer length in characters */
               const std::wstring &src   /**< [in] the string to copy */
)
{
    if( len <= 0 )
    {
        return;
    }

    size_t n = src.size();
    if( n > static_cast<size_t>( len - 1 ) )
    {
        n = len - 1;
    }

    for( size_t i = 0; i < n; ++i )
    {
        dest[i] = src[i];
    }
    dest[n] = L'\0';
}

} // namespace

extern "C"
{

    int AT_InitialiseLibrary()
    {
        ++g_zylaStub.initLibCalls;
        return g_zylaStub.initLibReturn;
    }

    int AT_FinaliseLibrary()
    {
        ++g_zylaStub.finLibCalls;
        return g_zylaStub.finLibReturn;
    }

    int AT_InitialiseUtilityLibrary()
    {
        ++g_zylaStub.initUtilCalls;
        return g_zylaStub.initUtilReturn;
    }

    int AT_FinaliseUtilityLibrary()
    {
        ++g_zylaStub.finUtilCalls;
        return g_zylaStub.finUtilReturn;
    }

    int AT_Open( int CameraIndex, AT_H *Hndl )
    {
        g_zylaStub.opened.push_back( CameraIndex );

        if( g_zylaStub.openReturn != AT_SUCCESS )
        {
            return g_zylaStub.openReturn;
        }

        *Hndl = c_zylaHandleBase + CameraIndex;
        return AT_SUCCESS;
    }

    int AT_Close( AT_H Hndl )
    {
        g_zylaStub.closed.push_back( Hndl );
        return g_zylaStub.closeReturn;
    }

    int AT_SetInt( AT_H Hndl, const AT_WC *Feature, AT_64 Value )
    {
        static_cast<void>( Hndl );

        int rv = featureError( Feature );
        if( rv != AT_SUCCESS )
        {
            return rv;
        }

        g_zylaStub.setInts[Feature] = Value;
        return AT_SUCCESS;
    }

    int AT_GetInt( AT_H Hndl, const AT_WC *Feature, AT_64 *Value )
    {
        static_cast<void>( Hndl );

        int rv = featureError( Feature );
        if( rv != AT_SUCCESS )
        {
            return rv;
        }

        *Value = g_zylaStub.ints[Feature];
        return AT_SUCCESS;
    }

    int AT_SetFloat( AT_H Hndl, const AT_WC *Feature, double Value )
    {
        static_cast<void>( Hndl );

        int rv = featureError( Feature );
        if( rv != AT_SUCCESS )
        {
            return rv;
        }

        g_zylaStub.setFloats[Feature] = Value;
        return AT_SUCCESS;
    }

    int AT_GetFloat( AT_H Hndl, const AT_WC *Feature, double *Value )
    {
        static_cast<void>( Hndl );

        int rv = featureError( Feature );
        if( rv != AT_SUCCESS )
        {
            return rv;
        }

        *Value = g_zylaStub.floats[Feature];
        return AT_SUCCESS;
    }

    int AT_SetBool( AT_H Hndl, const AT_WC *Feature, AT_BOOL Value )
    {
        static_cast<void>( Hndl );

        int rv = featureError( Feature );
        if( rv != AT_SUCCESS )
        {
            return rv;
        }

        g_zylaStub.setBools[Feature] = Value;
        return AT_SUCCESS;
    }

    int AT_GetBool( AT_H Hndl, const AT_WC *Feature, AT_BOOL *Value )
    {
        static_cast<void>( Hndl );

        int rv = featureError( Feature );
        if( rv != AT_SUCCESS )
        {
            return rv;
        }

        *Value = g_zylaStub.bools[Feature];
        return AT_SUCCESS;
    }

    int AT_Command( AT_H Hndl, const AT_WC *Feature )
    {
        static_cast<void>( Hndl );

        int rv = featureError( Feature );
        if( rv != AT_SUCCESS )
        {
            return rv;
        }

        g_zylaStub.commands.push_back( Feature );
        return AT_SUCCESS;
    }

    int AT_SetEnumString( AT_H Hndl, const AT_WC *Feature, const AT_WC *String )
    {
        static_cast<void>( Hndl );

        int rv = featureError( Feature );
        if( rv != AT_SUCCESS )
        {
            return rv;
        }

        g_zylaStub.setEnumStrings[Feature] = String;
        return AT_SUCCESS;
    }

    int AT_GetEnumIndex( AT_H Hndl, const AT_WC *Feature, int *Value )
    {
        static_cast<void>( Hndl );

        int rv = featureError( Feature );
        if( rv != AT_SUCCESS )
        {
            return rv;
        }

        *Value = g_zylaStub.enumIndex[Feature];
        return AT_SUCCESS;
    }

    int AT_GetEnumStringByIndex( AT_H Hndl, const AT_WC *Feature, int Index, AT_WC *String, int StringLength )
    {
        static_cast<void>( Hndl );

        int rv = featureError( Feature );
        if( rv != AT_SUCCESS )
        {
            return rv;
        }

        const std::vector<std::wstring> &strs = g_zylaStub.enumStrings[Feature];

        if( Index < 0 || Index >= static_cast<int>( strs.size() ) )
        {
            return AT_ERR_OUTOFRANGE;
        }

        copyWide( String, StringLength, strs[Index] );
        return AT_SUCCESS;
    }

    int AT_GetString( AT_H Hndl, const AT_WC *Feature, AT_WC *String, int StringLength )
    {
        int rv = featureError( Feature );
        if( rv != AT_SUCCESS )
        {
            return rv;
        }

        int idx = Hndl - c_zylaHandleBase;

        const std::vector<std::wstring> *strs = nullptr;
        if( std::wstring( Feature ) == L"SerialNumber" )
        {
            strs = &g_zylaStub.serials;
        }
        else if( std::wstring( Feature ) == L"Camera Model" )
        {
            strs = &g_zylaStub.models;
        }
        else
        {
            return AT_ERR_NOTIMPLEMENTED;
        }

        if( idx < 0 || idx >= static_cast<int>( strs->size() ) )
        {
            return AT_ERR_INVALIDHANDLE;
        }

        copyWide( String, StringLength, ( *strs )[idx] );
        return AT_SUCCESS;
    }

    int AT_QueueBuffer( AT_H Hndl, AT_U8 *Ptr, int PtrSize )
    {
        static_cast<void>( Hndl );

        if( g_zylaStub.queueReturn != AT_SUCCESS )
        {
            return g_zylaStub.queueReturn;
        }

        g_zylaStub.queued.push_back( std::make_pair( Ptr, PtrSize ) );
        return AT_SUCCESS;
    }

    int AT_WaitBuffer( AT_H Hndl, AT_U8 **Ptr, int *PtrSize, unsigned int Timeout )
    {
        static_cast<void>( Hndl );

        g_zylaStub.lastTimeout = Timeout;

        if( g_zylaStub.waitReturn != AT_SUCCESS )
        {
            return g_zylaStub.waitReturn;
        }

        *Ptr     = g_zylaStub.waitPtr;
        *PtrSize = g_zylaStub.waitSize;
        return AT_SUCCESS;
    }

    int AT_Flush( AT_H Hndl )
    {
        static_cast<void>( Hndl );

        ++g_zylaStub.flushCalls;
        return g_zylaStub.flushReturn;
    }

    int AT_ConvertBuffer( AT_U8       *inputBuffer,
                          AT_U8       *outputBuffer,
                          AT_64        width,
                          AT_64        height,
                          AT_64        stride,
                          const AT_WC *inputPixelEncoding,
                          const AT_WC *outputPixelEncoding )
    {
        ++g_zylaStub.convertCalls;

        g_zylaStub.lastConvert.input          = inputBuffer;
        g_zylaStub.lastConvert.output         = outputBuffer;
        g_zylaStub.lastConvert.width          = width;
        g_zylaStub.lastConvert.height         = height;
        g_zylaStub.lastConvert.stride         = stride;
        g_zylaStub.lastConvert.inputEncoding  = inputPixelEncoding;
        g_zylaStub.lastConvert.outputEncoding = outputPixelEncoding;

        for( AT_64 n = 0; n < width * height; ++n )
        {
            uint16_t v = g_zylaStub.convertFill;
            memcpy( outputBuffer + n * sizeof( uint16_t ), &v, sizeof( uint16_t ) );
        }

        return AT_SUCCESS;
    }
}
/// \endcond

namespace libXWCTest
{

/** \defgroup zylaCtrl_unit_test zylaCtrl Unit Tests
 * \brief Unit tests for the zylaCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `zylaCtrl` unit tests.
/** \ingroup zylaCtrl_unit_test
 */
namespace zylaCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing zylaCtrl internals.
class zylaCtrl_test : public zylaCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit zylaCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        // Power is on
        m_powerState       = 1;
        m_powerTargetState = 1;

        // appStartup() normally sizes the buffer list
        m_inputBuffers.resize( 3, nullptr );
        m_nextBuffer = 0;

        // Not initialized by the zylaCtrl constructor
        m_stride = 0;
        std::memset( m_pixelEncoding, 0, sizeof( m_pixelEncoding ) );

        setupProp( m_indiP_fps, "fps" );
        setupProp( m_indiP_exptime, "exptime" );
        setupProp( m_indiP_temp, "temp_ccd" );
        setupProp( m_indiP_tempcont, "temp_controller" );
        setupProp( m_indiP_roi_x, "roi_region_x" );
        setupProp( m_indiP_roi_set, "roi_set" );
    }

    /// Set the device and name of a property owned by this app.
    void setupProp( pcf::IndiProperty &prop, /**< [out] the property to set up */
                    const std::string &name  /**< [in] the INDI property name */
    )
    {
        prop.setDevice( m_configName );
        prop.setName( name );
    }

    /// Mark the library as initialized and a camera as open, as after a successful cameraSelect().
    void setOpen( AT_H h = c_zylaHandleBase /**< [in] the camera handle */ )
    {
        m_libInit = true;
        m_handle  = h;
    }

    /// Allocate the input buffers with malloc (freed by the zylaCtrl destructor), as configureAcquisition() does.
    void allocBuffers( int size /**< [in] the size of each buffer in bytes */ )
    {
        for( size_t n = 0; n < m_inputBuffers.size(); ++n )
        {
            if( m_inputBuffers[n] )
            {
                free( m_inputBuffers[n] );
            }
            m_inputBuffers[n] = static_cast<unsigned char *>( malloc( size ) );
        }
        m_inputBufferSize = size;
    }

    /// Set up the configuration, read a config file, and load it.
    void loadTestConfig( const std::string &path /**< [in] the config file path */ )
    {
        setupConfig();
        config.readConfig( path );
        loadConfig();
    }
};
/// \endcond

/// Build a number property with a single element.
pcf::IndiProperty numberProp( const std::string &device, /**< [in] the device name */
                              const std::string &name,   /**< [in] the property name */
                              const std::string &el,     /**< [in] the element name */
                              float              val     /**< [in] the element value */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, val ) );
    return ip;
}

/// Build a switch property with a single element.
pcf::IndiProperty switchProp( const std::string                       &device, /**< [in] the device name */
                              const std::string                       &name,   /**< [in] the property name */
                              const std::string                       &el,     /**< [in] the element name */
                              const pcf::IndiElement::SwitchStateType &state   /**< [in] the switch state */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, state ) );
    return ip;
}

/// Check whether a handle was passed to AT_Close.
bool wasClosed( AT_H h /**< [in] the handle to look for */ )
{
    for( auto c : g_zylaStub.closed )
    {
        if( c == h )
        {
            return true;
        }
    }

    return false;
}

/// Set the AOI readback values used by configureAcquisition().
void setAOIReadback( AT_64 left,   /**< [in] AOI Left */
                     AT_64 top,    /**< [in] AOI Top */
                     AT_64 width,  /**< [in] AOI Width */
                     AT_64 height, /**< [in] AOI Height */
                     AT_64 stride, /**< [in] AOI Stride */
                     AT_64 bytes   /**< [in] ImageSizeBytes */
)
{
    g_zylaStub.ints[L"AOI Left"]       = left;
    g_zylaStub.ints[L"AOI Top"]        = top;
    g_zylaStub.ints[L"AOI Width"]      = width;
    g_zylaStub.ints[L"AOI Height"]     = height;
    g_zylaStub.ints[L"AOI Stride"]     = stride;
    g_zylaStub.ints[L"ImageSizeBytes"] = bytes;

    g_zylaStub.enumStrings[L"PixelEncoding"] = { L"Mono12", L"Mono12Packed", L"Mono16" };
    g_zylaStub.enumIndex[L"PixelEncoding"]   = 1;
}

/// Verify the zylaCtrl constructor defaults and base-class configuration flags.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl constructor defaults", "[zylaCtrl]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::zylaCtrl();
    #endif
    // clang-format on

    resetZylaStub();

    zylaCtrl_test app( "camzyla" );

    REQUIRE( app.m_powerMgtEnabled == true );
    REQUIRE( app.m_powerOnWait == 10 );

    REQUIRE( app.m_startupTemp == Approx( 20 ) );
    REQUIRE( app.m_expTimeSet == Approx( 0.05 ) );
    REQUIRE( app.m_fpsSet == Approx( 20 ) );

    // Power-on ROI (requires the m_startup_* -> m_default_* fix, see README.md)
    REQUIRE( app.m_default_x == Approx( 1075 ) );
    REQUIRE( app.m_default_y == Approx( 975 ) );
    REQUIRE( app.m_default_w == 128 );
    REQUIRE( app.m_default_h == 128 );
    REQUIRE( app.m_default_bin_x == 1 );
    REQUIRE( app.m_default_bin_y == 1 );

    REQUIRE( app.m_full_x == Approx( 1023.5 ) );
    REQUIRE( app.m_full_y == Approx( 1023.5 ) );
    REQUIRE( app.m_full_w == 2048 );
    REQUIRE( app.m_full_h == 2048 );

    REQUIRE( app.m_imageTimeout == 1000 );
    REQUIRE( app.m_libInit == false );
    REQUIRE( app.m_handle == AT_HANDLE_UNINITIALISED );
    REQUIRE( app.m_outputBuffer == nullptr );

    REQUIRE( zylaCtrl::c_stdCamera_tempControl == true );
    REQUIRE( zylaCtrl::c_stdCamera_temp == true );
    REQUIRE( zylaCtrl::c_stdCamera_exptimeCtrl == true );
    REQUIRE( zylaCtrl::c_stdCamera_fpsCtrl == true );
    REQUIRE( zylaCtrl::c_stdCamera_usesROI == true );
    REQUIRE( zylaCtrl::c_stdCamera_emGain == false );
    REQUIRE( zylaCtrl::c_stdCamera_readoutSpeed == false );
    REQUIRE( zylaCtrl::c_stdCamera_usesModes == false );
    REQUIRE( zylaCtrl::c_stdCamera_hasShutter == false );
    REQUIRE( zylaCtrl::c_frameGrabber_flippable == false );
}

/// Verify zylaCtrl configuration defaults load from an empty config file.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl configuration defaults", "[zylaCtrl][config]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::setupConfig();
    zylaCtrl::loadConfig();
    #endif
    // clang-format on

    resetZylaStub();

    zylaCtrl_test app( "camzyla" );

    const std::string path = "/tmp/zylaCtrl_test_defaults.conf";
    mx::app::writeConfigFile( path, { "none" }, { "nada" }, { "0" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_serial == "" );
    REQUIRE( app.m_startupTemp == Approx( 20 ) );

    // stdCamera starts the current and next ROI at the power-on defaults
    REQUIRE( app.m_currentROI.x == Approx( 1075 ) );
    REQUIRE( app.m_currentROI.y == Approx( 975 ) );
    REQUIRE( app.m_currentROI.w == 128 );
    REQUIRE( app.m_currentROI.h == 128 );
    REQUIRE( app.m_currentROI.bin_x == 1 );
    REQUIRE( app.m_currentROI.bin_y == 1 );
    REQUIRE( app.m_nextROI.x == Approx( 1075 ) );
    REQUIRE( app.m_nextROI.w == 128 );

    // frameGrabber defaults
    REQUIRE( app.m_shmimName == "camzyla" );
    REQUIRE( app.m_circBuffLength == 1 );
}

/// Verify zylaCtrl configuration overrides are loaded.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl configuration overrides", "[zylaCtrl][config]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::setupConfig();
    zylaCtrl::loadConfig();
    #endif
    // clang-format on

    resetZylaStub();

    zylaCtrl_test app( "camzyla" );

    const std::string path = "/tmp/zylaCtrl_test_overrides.conf";
    mx::app::writeConfigFile(
        path,
        { "camera", "camera", "camera", "camera", "camera", "camera", "framegrabber", "framegrabber" },
        { "serial", "startupTemp", "default_x", "default_y", "default_w", "default_h", "shmimName", "circBuffLength" },
        { "VSC-01234", "-10", "500.5", "600.5", "256", "64", "zylatest", "5" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_serial == "VSC-01234" );
    REQUIRE( app.m_startupTemp == Approx( -10 ) );

    REQUIRE( app.m_default_x == Approx( 500.5 ) );
    REQUIRE( app.m_default_y == Approx( 600.5 ) );
    REQUIRE( app.m_default_w == 256 );
    REQUIRE( app.m_default_h == 64 );

    REQUIRE( app.m_currentROI.x == Approx( 500.5 ) );
    REQUIRE( app.m_currentROI.h == 64 );
    REQUIRE( app.m_nextROI.y == Approx( 600.5 ) );
    REQUIRE( app.m_nextROI.w == 256 );

    REQUIRE( app.m_shmimName == "zylatest" );
    REQUIRE( app.m_circBuffLength == 5 );
}

/// Verify powerOnDefaults() resets temperature control and the ROI.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl powerOnDefaults", "[zylaCtrl]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::powerOnDefaults();
    #endif
    // clang-format on

    resetZylaStub();

    zylaCtrl_test app( "camzyla" );

    app.m_tempControlStatusSet = true;
    app.m_tempControlStatus    = true;
    app.m_ccdTempSetpt         = -15;
    app.m_currentROI.x         = 10;
    app.m_currentROI.w         = 10;
    app.m_currentROI.bin_x     = 4;

    REQUIRE( app.powerOnDefaults() == 0 );

    REQUIRE( app.m_tempControlStatusSet == false );
    REQUIRE( app.m_tempControlStatus == false );
    REQUIRE( app.m_ccdTempSetpt == Approx( 0 ) );

    REQUIRE( app.m_currentROI.x == Approx( app.m_default_x ) );
    REQUIRE( app.m_currentROI.y == Approx( app.m_default_y ) );
    REQUIRE( app.m_currentROI.w == app.m_default_w );
    REQUIRE( app.m_currentROI.h == app.m_default_h );
    REQUIRE( app.m_currentROI.bin_x == app.m_default_bin_x );
    REQUIRE( app.m_currentROI.bin_y == app.m_default_bin_y );
}

/// Verify cameraSelect() opens the camera with the configured serial number and handles SDK errors.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl cameraSelect", "[zylaCtrl][connect]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::cameraSelect();
    #endif
    // clang-format on

    resetZylaStub();

    g_zylaStub.serials               = { L"VSC-00001", L"VSC-01234" };
    g_zylaStub.models                = { L"Zyla 5.5", L"Zyla 4.2" };
    g_zylaStub.ints[L"Device Count"] = 2;

    zylaCtrl_test app( "camzyla" );
    app.m_serial = "VSC-01234";

    SECTION( "camera found at the second index" )
    {
        REQUIRE( app.cameraSelect() == 0 );

        REQUIRE( app.m_handle == c_zylaHandleBase + 1 );
        REQUIRE( app.m_libInit == true );
        REQUIRE( g_zylaStub.initLibCalls == 1 );
        REQUIRE( g_zylaStub.initUtilCalls == 1 );
        REQUIRE( g_zylaStub.finLibCalls == 0 );

        REQUIRE( g_zylaStub.opened.size() == 2 );
        REQUIRE( g_zylaStub.opened[0] == 0 );
        REQUIRE( g_zylaStub.opened[1] == 1 );

        // the non-matching camera is closed, the matching one stays open
        REQUIRE( g_zylaStub.closed.size() == 1 );
        REQUIRE( wasClosed( c_zylaHandleBase ) );
    }

    SECTION( "camera not found" )
    {
        app.m_serial = "VSC-99999";

        REQUIRE( app.cameraSelect() == -1 );

        REQUIRE( app.m_handle == AT_HANDLE_UNINITIALISED );
        REQUIRE( app.m_libInit == false );
        REQUIRE( g_zylaStub.closed.size() == 2 );
        REQUIRE( wasClosed( c_zylaHandleBase ) );
        REQUIRE( wasClosed( c_zylaHandleBase + 1 ) );
        REQUIRE( g_zylaStub.finLibCalls == 1 );
        REQUIRE( g_zylaStub.finUtilCalls == 1 );
    }

    SECTION( "no cameras" )
    {
        g_zylaStub.ints[L"Device Count"] = 0;

        REQUIRE( app.cameraSelect() == -1 );

        REQUIRE( app.m_handle == AT_HANDLE_UNINITIALISED );
        REQUIRE( app.m_libInit == false );
        REQUIRE( g_zylaStub.opened.empty() );
        REQUIRE( g_zylaStub.finLibCalls == 1 );
        REQUIRE( g_zylaStub.finUtilCalls == 1 );
    }

    SECTION( "an open camera and library are closed and reinitialized first" )
    {
        app.setOpen( 55 );

        REQUIRE( app.cameraSelect() == 0 );

        REQUIRE( wasClosed( 55 ) );
        REQUIRE( g_zylaStub.finLibCalls == 1 );
        REQUIRE( g_zylaStub.finUtilCalls == 1 );
        REQUIRE( g_zylaStub.initLibCalls == 1 );
        REQUIRE( app.m_handle == c_zylaHandleBase + 1 );
        REQUIRE( app.m_libInit == true );
    }

    SECTION( "AT_FinaliseLibrary error on re-select" )
    {
        app.m_libInit           = true;
        g_zylaStub.finLibReturn = AT_ERR_NOTINITIALISED;

        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( g_zylaStub.initLibCalls == 0 );
    }

    SECTION( "AT_FinaliseUtilityLibrary error on re-select" )
    {
        app.m_libInit            = true;
        g_zylaStub.finUtilReturn = AT_ERR_NOTINITIALISED;

        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( g_zylaStub.initLibCalls == 0 );
    }

    SECTION( "AT_InitialiseLibrary error" )
    {
        g_zylaStub.initLibReturn = AT_ERR_NOTINITIALISED;

        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( app.m_libInit == false );
        REQUIRE( g_zylaStub.initUtilCalls == 0 );
    }

    SECTION( "AT_InitialiseUtilityLibrary error" )
    {
        g_zylaStub.initUtilReturn = AT_ERR_NOTINITIALISED;

        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( app.m_libInit == false );
        REQUIRE( g_zylaStub.opened.empty() );
    }

    SECTION( "Device Count error" )
    {
        g_zylaStub.featureErrors[L"Device Count"] = AT_ERR_NOTIMPLEMENTED;

        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( g_zylaStub.opened.empty() );
    }

    SECTION( "AT_Open error" )
    {
        g_zylaStub.openReturn = AT_ERR_INVALIDHANDLE;

        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( app.m_handle == AT_HANDLE_UNINITIALISED );
    }

    SECTION( "SerialNumber error" )
    {
        g_zylaStub.featureErrors[L"SerialNumber"] = AT_ERR_NOTIMPLEMENTED;

        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( app.m_handle == AT_HANDLE_UNINITIALISED );
    }

    SECTION( "Camera Model error" )
    {
        g_zylaStub.featureErrors[L"Camera Model"] = AT_ERR_NOTIMPLEMENTED;

        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( app.m_handle == AT_HANDLE_UNINITIALISED );
    }
}

/// Verify getTemp() maps each TemperatureStatus string and reads the sensor and target temperatures.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl getTemp", "[zylaCtrl][temp]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::getTemp();
    #endif
    // clang-format on

    resetZylaStub();

    g_zylaStub.enumStrings[L"TemperatureStatus"] = {
        L"Cooler Off", L"Stabilised", L"Cooling", L"Drift", L"Not Stabilised", L"Fault", L"Something Else" };
    g_zylaStub.floats[L"SensorTemperature"]       = -12.25;
    g_zylaStub.floats[L"TargetSensorTemperature"] = -15.0;

    zylaCtrl_test app( "camzyla" );
    app.setOpen();

    SECTION( "status strings" )
    {
        /// Expected results for each TemperatureStatus index.
        struct expected
        {
            std::string str;      ///< Expected m_tempControlStatusStr.
            bool        status;   ///< Expected m_tempControlStatus.
            bool        onTarget; ///< Expected m_tempControlOnTarget.
        };

        std::vector<expected> cases = { { "Cooler Off", false, false },
                                        { "Stabilised", true, true },
                                        { "Cooling", true, false },
                                        { "Drift", true, false },
                                        { "Not Stabilised", true, false },
                                        { "Fault", false, false },
                                        { "Unknown", false, false } };

        for( size_t n = 0; n < cases.size(); ++n )
        {
            g_zylaStub.enumIndex[L"TemperatureStatus"] = static_cast<int>( n );

            // start from the opposite state so every field must be written
            app.m_tempControlStatusStr = "";
            app.m_tempControlStatus    = !cases[n].status;
            app.m_tempControlOnTarget  = !cases[n].onTarget;

            REQUIRE( app.getTemp() == 0 );

            CHECK( app.m_tempControlStatusStr == cases[n].str );
            CHECK( app.m_tempControlStatus == cases[n].status );
            CHECK( app.m_tempControlOnTarget == cases[n].onTarget );
        }

        REQUIRE( app.m_ccdTemp == Approx( -12.25 ) );
        REQUIRE( app.m_ccdTempSetpt == Approx( -15.0 ) );
    }

    SECTION( "TemperatureStatus error" )
    {
        g_zylaStub.featureErrors[L"TemperatureStatus"] = AT_ERR_NOTIMPLEMENTED;

        REQUIRE( app.getTemp() == -1 );
        REQUIRE( app.m_ccdTemp == Approx( -999 ) );
    }

    SECTION( "TemperatureStatus string error" )
    {
        g_zylaStub.enumIndex[L"TemperatureStatus"] = 99; // stub returns AT_ERR_OUTOFRANGE

        REQUIRE( app.getTemp() == -1 );
        REQUIRE( app.m_ccdTemp == Approx( -999 ) );
    }

    SECTION( "SensorTemperature error" )
    {
        g_zylaStub.featureErrors[L"SensorTemperature"] = AT_ERR_NOTIMPLEMENTED;

        REQUIRE( app.getTemp() == -1 );
        REQUIRE( app.m_ccdTemp == Approx( -999 ) );
    }

    SECTION( "TargetSensorTemperature error" )
    {
        g_zylaStub.featureErrors[L"TargetSensorTemperature"] = AT_ERR_NOTIMPLEMENTED;

        REQUIRE( app.getTemp() == -1 );
        REQUIRE( app.m_ccdTemp == Approx( -12.25 ) );
        REQUIRE( app.m_ccdTempSetpt == Approx( -999 ) );
    }
}

/// Verify setTempControl() turns SensorCooling on and off, and reports SDK errors.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl setTempControl", "[zylaCtrl][temp]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::setTempControl();
    #endif
    // clang-format on

    resetZylaStub();

    zylaCtrl_test app( "camzyla" );
    app.setOpen();

    SECTION( "cooling on" )
    {
        app.m_tempControlStatusSet = true;
        REQUIRE( app.setTempControl() == 0 );
        REQUIRE( g_zylaStub.setBools.count( L"SensorCooling" ) == 1 );
        REQUIRE( g_zylaStub.setBools[L"SensorCooling"] == AT_TRUE );
    }

    SECTION( "cooling off" )
    {
        app.m_tempControlStatusSet = false;
        REQUIRE( app.setTempControl() == 0 );
        REQUIRE( g_zylaStub.setBools.count( L"SensorCooling" ) == 1 );
        REQUIRE( g_zylaStub.setBools[L"SensorCooling"] == AT_FALSE );
    }

    SECTION( "SDK error turning cooling on" )
    {
        g_zylaStub.featureErrors[L"SensorCooling"] = AT_ERR_READONLY;
        app.m_tempControlStatusSet                 = true;
        REQUIRE( app.setTempControl() == -1 );
    }

    SECTION( "SDK error turning cooling off" )
    {
        g_zylaStub.featureErrors[L"SensorCooling"] = AT_ERR_READONLY;
        app.m_tempControlStatusSet                 = false;
        REQUIRE( app.setTempControl() == -1 );
    }
}

/// Verify the stdCamera hooks that only flag a reconfiguration, and the other trivial hooks.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl simple stdCamera hooks", "[zylaCtrl]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::setExpTime();
    zylaCtrl::setFPS();
    zylaCtrl::setNextROI();
    zylaCtrl::checkNextROI();
    zylaCtrl::setTempSetPt();
    zylaCtrl::setShutter(0);
    zylaCtrl::getExpTime();
    zylaCtrl::getFPS();
    zylaCtrl::fps();
    #endif
    // clang-format on

    resetZylaStub();

    zylaCtrl_test app( "camzyla" );

    SECTION( "setExpTime flags a reconfiguration" )
    {
        app.m_reconfig = false;
        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "setFPS flags a reconfiguration" )
    {
        app.m_reconfig = false;
        REQUIRE( app.setFPS() == 0 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "setNextROI flags a reconfiguration" )
    {
        app.m_reconfig = false;
        REQUIRE( app.setNextROI() == 0 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "no-op hooks" )
    {
        app.m_reconfig = false;
        REQUIRE( app.checkNextROI() == 0 );
        REQUIRE( app.setTempSetPt() == 0 );
        REQUIRE( app.setShutter( 1 ) == 0 );
        REQUIRE( app.getExpTime() == 0 );
        REQUIRE( app.getFPS() == 0 );
        REQUIRE( app.m_reconfig == false );
        REQUIRE( g_zylaStub.setBools.empty() );
        REQUIRE( g_zylaStub.setFloats.empty() );
    }

    SECTION( "fps() reports m_fps" )
    {
        app.m_fps = 42.5;
        REQUIRE( app.fps() == Approx( 42.5 ) );
    }
}

/// Verify configureAcquisition() sets the AOI, reads it back, allocates and queues buffers, and sets timing.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl configureAcquisition", "[zylaCtrl][acquisition]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::configureAcquisition();
    #endif
    // clang-format on

    resetZylaStub();

    SECTION( "success" )
    {
        // AOI Left/Top readback as the camera would report after setting AOILeft = 1001, AOITop = 481
        setAOIReadback( 1001, 481, 128, 64, 256, 16384 );

        zylaCtrl_test app( "camzyla" );
        app.setOpen();

        app.m_nextROI.x     = 1063.5;
        app.m_nextROI.y     = 511.5;
        app.m_nextROI.w     = 128;
        app.m_nextROI.h     = 64;
        app.m_nextROI.bin_x = 2;
        app.m_nextROI.bin_y = 3;
        app.m_expTimeSet    = 0.01;
        app.m_fpsSet        = 50;
        app.m_nextBuffer    = 2;

        REQUIRE( app.configureAcquisition() == 0 );

        // AOI settings: 1-based left/top corner from the center
        REQUIRE( g_zylaStub.setInts[L"AOIHBin"] == 2 );
        REQUIRE( g_zylaStub.setInts[L"AOIVBin"] == 3 );
        REQUIRE( g_zylaStub.setInts[L"AOIWidth"] == 128 );
        REQUIRE( g_zylaStub.setInts[L"AOIHeight"] == 64 );
        REQUIRE( g_zylaStub.setInts[L"AOILeft"] == 1001 );
        REQUIRE( g_zylaStub.setInts[L"AOITop"] == 481 );

        // current ROI center is computed from the read back corner and size
        REQUIRE( app.m_currentROI.x == Approx( 1001 + 63.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 481 + 31.5 ) );
        REQUIRE( app.m_currentROI.w == 128 );
        REQUIRE( app.m_currentROI.h == 64 );

        REQUIRE( app.m_width == 128 );
        REQUIRE( app.m_height == 64 );
        REQUIRE( app.m_stride == 256 );
        REQUIRE( app.m_dataType == _DATATYPE_UINT16 );

        // buffers allocated and handed to the SDK
        REQUIRE( app.m_inputBufferSize == 16384 );
        REQUIRE( g_zylaStub.flushCalls == 1 );
        REQUIRE( g_zylaStub.queued.size() == app.m_inputBuffers.size() );
        for( size_t n = 0; n < app.m_inputBuffers.size(); ++n )
        {
            REQUIRE( app.m_inputBuffers[n] != nullptr );
            REQUIRE( g_zylaStub.queued[n].first == app.m_inputBuffers[n] );
            REQUIRE( g_zylaStub.queued[n].second == 16384 );
        }
        REQUIRE( app.m_nextBuffer == 0 );

        // timing
        REQUIRE( g_zylaStub.setFloats[L"ExposureTime"] == Approx( 0.01 ) );
        REQUIRE( g_zylaStub.setFloats[L"FrameRate"] == Approx( 50 ) );
        REQUIRE( app.m_expTime == Approx( 0.01 ) );
        REQUIRE( app.m_fps == Approx( 50 ) );

        // pixel encoding and cycle mode
        REQUIRE( std::wcscmp( app.m_pixelEncoding, L"Mono12Packed" ) == 0 );
        REQUIRE( g_zylaStub.setEnumStrings[L"CycleMode"] == std::wstring( L"Continuous" ) );
    }

    SECTION( "reconfiguring replaces the buffers" )
    {
        setAOIReadback( 1, 1, 64, 64, 128, 8192 );

        zylaCtrl_test app( "camzyla" );
        app.setOpen();
        app.m_nextROI.w     = 64;
        app.m_nextROI.h     = 64;
        app.m_nextROI.bin_x = 1;
        app.m_nextROI.bin_y = 1;

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.m_inputBufferSize == 8192 );

        g_zylaStub.ints[L"ImageSizeBytes"] = 4096;
        g_zylaStub.queued.clear();

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.m_inputBufferSize == 4096 );
        REQUIRE( g_zylaStub.queued.size() == 3 );
        REQUIRE( g_zylaStub.queued[0].second == 4096 );
    }

    SECTION( "camera not open" )
    {
        zylaCtrl_test app( "camzyla" );

        app.m_libInit = true;
        app.m_handle  = AT_HANDLE_UNINITIALISED;
        REQUIRE( app.configureAcquisition() == -1 );

        app.m_libInit = false;
        app.m_handle  = c_zylaHandleBase;
        REQUIRE( app.configureAcquisition() == -1 );

        REQUIRE( g_zylaStub.setInts.empty() );
    }

    SECTION( "SDK feature errors" )
    {
        std::vector<std::wstring> features = { L"AOIHBin",
                                               L"AOIVBin",
                                               L"AOIWidth",
                                               L"AOILeft",
                                               L"AOIHeight",
                                               L"AOITop",
                                               L"AOI Left",
                                               L"AOI Top",
                                               L"AOI Width",
                                               L"AOI Height",
                                               L"AOI Stride",
                                               L"ImageSizeBytes",
                                               L"CycleMode" };

        for( const auto &f : features )
        {
            resetZylaStub();
            setAOIReadback( 1, 1, 64, 64, 128, 8192 );
            g_zylaStub.featureErrors[f] = AT_ERR_OUTOFRANGE;

            zylaCtrl_test app( "camzyla" );
            app.setOpen();
            app.m_nextROI.w     = 64;
            app.m_nextROI.h     = 64;
            app.m_nextROI.bin_x = 1;
            app.m_nextROI.bin_y = 1;

            CHECK( app.configureAcquisition() == -1 );
        }
    }

    SECTION( "AT_Flush error" )
    {
        setAOIReadback( 1, 1, 64, 64, 128, 8192 );
        g_zylaStub.flushReturn = AT_ERR_NOTINITIALISED;

        zylaCtrl_test app( "camzyla" );
        app.setOpen();

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( g_zylaStub.queued.empty() );
    }

    SECTION( "AT_QueueBuffer error" )
    {
        setAOIReadback( 1, 1, 64, 64, 128, 8192 );
        g_zylaStub.queueReturn = AT_ERR_NOTINITIALISED;

        zylaCtrl_test app( "camzyla" );
        app.setOpen();

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( g_zylaStub.setEnumStrings.empty() );
    }
}

/// Verify startAcquisition() and reconfig() send the acquisition commands and report SDK errors.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl startAcquisition and reconfig", "[zylaCtrl][acquisition]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::startAcquisition();
    zylaCtrl::reconfig();
    #endif
    // clang-format on

    resetZylaStub();

    zylaCtrl_test app( "camzyla" );
    app.setOpen();

    SECTION( "start" )
    {
        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( g_zylaStub.commands.size() == 1 );
        REQUIRE( g_zylaStub.commands[0] == std::wstring( L"AcquisitionStart" ) );
    }

    SECTION( "start error" )
    {
        g_zylaStub.featureErrors[L"AcquisitionStart"] = AT_ERR_NOTINITIALISED;
        REQUIRE( app.startAcquisition() == -1 );
    }

    SECTION( "reconfig stops and flushes" )
    {
        REQUIRE( app.reconfig() == 0 );
        REQUIRE( g_zylaStub.commands.size() == 1 );
        REQUIRE( g_zylaStub.commands[0] == std::wstring( L"AcquisitionStop" ) );
        REQUIRE( g_zylaStub.flushCalls == 1 );
    }

    SECTION( "reconfig stop error" )
    {
        g_zylaStub.featureErrors[L"AcquisitionStop"] = AT_ERR_NOTINITIALISED;
        REQUIRE( app.reconfig() == -1 );
        REQUIRE( g_zylaStub.flushCalls == 0 );
    }

    SECTION( "reconfig flush error" )
    {
        g_zylaStub.flushReturn = AT_ERR_NOTINITIALISED;
        REQUIRE( app.reconfig() == -1 );
        REQUIRE( g_zylaStub.flushCalls == 1 );
    }
}

/// Verify acquireAndCheckValid() handles success, timeout, SDK errors, and a wrong buffer size.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl acquireAndCheckValid", "[zylaCtrl][acquisition]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::acquireAndCheckValid();
    #endif
    // clang-format on

    resetZylaStub();

    zylaCtrl_test app( "camzyla" );
    app.setOpen();
    app.allocBuffers( 64 );

    g_zylaStub.waitPtr  = app.m_inputBuffers[1];
    g_zylaStub.waitSize = 64;

    SECTION( "valid frame" )
    {
        app.m_currImageTimestamp = { 0, 0 };

        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( g_zylaStub.lastTimeout == app.m_imageTimeout );
        REQUIRE( app.m_outputBuffer == app.m_inputBuffers[1] );
        REQUIRE( app.m_outputBufferSize == 64 );
        REQUIRE( app.m_currImageTimestamp.tv_sec > 0 );
    }

    SECTION( "timeout" )
    {
        g_zylaStub.waitReturn = AT_ERR_TIMEDOUT;
        REQUIRE( app.acquireAndCheckValid() == 1 );
    }

    SECTION( "SDK error" )
    {
        g_zylaStub.waitReturn = AT_ERR_NOTINITIALISED;
        REQUIRE( app.acquireAndCheckValid() == -1 );
    }

    SECTION( "wrong buffer size" )
    {
        g_zylaStub.waitSize = 32;
        REQUIRE( app.acquireAndCheckValid() == -1 );
    }
}

/// Verify loadImageIntoStream() converts to Mono16 and requeues buffers in order, including after a skip.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl loadImageIntoStream", "[zylaCtrl][acquisition]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::loadImageIntoStream(nullptr);
    #endif
    // clang-format on

    resetZylaStub();
    g_zylaStub.convertFill = 1234;

    zylaCtrl_test app( "camzyla" );
    app.setOpen();
    app.allocBuffers( 64 );
    app.m_width  = 4;
    app.m_height = 2;
    app.m_stride = 8;
    std::wcsncpy( app.m_pixelEncoding, L"Mono12Packed", 255 );

    std::vector<uint16_t> dest( 8, 0 );

    SECTION( "no output buffer" )
    {
        app.m_outputBuffer = nullptr;
        REQUIRE( app.loadImageIntoStream( dest.data() ) == -1 );
        REQUIRE( g_zylaStub.convertCalls == 0 );
    }

    SECTION( "next buffer in order" )
    {
        app.m_nextBuffer   = 0;
        app.m_outputBuffer = app.m_inputBuffers[0];

        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );

        REQUIRE( g_zylaStub.convertCalls == 1 );
        REQUIRE( g_zylaStub.lastConvert.input == app.m_inputBuffers[0] );
        REQUIRE( g_zylaStub.lastConvert.output == reinterpret_cast<AT_U8 *>( dest.data() ) );
        REQUIRE( g_zylaStub.lastConvert.width == 4 );
        REQUIRE( g_zylaStub.lastConvert.height == 2 );
        REQUIRE( g_zylaStub.lastConvert.stride == 8 );
        REQUIRE( g_zylaStub.lastConvert.inputEncoding == std::wstring( L"Mono12Packed" ) );
        REQUIRE( g_zylaStub.lastConvert.outputEncoding == std::wstring( L"Mono16" ) );
        for( auto v : dest )
        {
            REQUIRE( v == 1234 );
        }

        REQUIRE( g_zylaStub.queued.size() == 1 );
        REQUIRE( g_zylaStub.queued[0].first == app.m_inputBuffers[0] );
        REQUIRE( g_zylaStub.queued[0].second == 64 );
        REQUIRE( app.m_nextBuffer == 1 );
    }

    SECTION( "last buffer wraps to the first" )
    {
        app.m_nextBuffer   = 2;
        app.m_outputBuffer = app.m_inputBuffers[2];

        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
        REQUIRE( g_zylaStub.queued[0].first == app.m_inputBuffers[2] );
        REQUIRE( app.m_nextBuffer == 0 );
    }

    SECTION( "skipped buffer" )
    {
        app.m_nextBuffer   = 0;
        app.m_outputBuffer = app.m_inputBuffers[2];

        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );

        // the returned buffer, not the expected one, is requeued
        REQUIRE( g_zylaStub.queued.size() == 1 );
        REQUIRE( g_zylaStub.queued[0].first == app.m_inputBuffers[2] );
        REQUIRE( app.m_nextBuffer == 0 );
    }

    SECTION( "AT_QueueBuffer error" )
    {
        app.m_nextBuffer       = 1;
        app.m_outputBuffer     = app.m_inputBuffers[1];
        g_zylaStub.queueReturn = AT_ERR_NOTINITIALISED;

        REQUIRE( app.loadImageIntoStream( dest.data() ) == -1 );
        REQUIRE( app.m_nextBuffer == 1 );
    }
}

/// Verify onPowerOff(), whilePowerOff() and appShutdown() close the camera and finalize the SDK.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl power off and shutdown", "[zylaCtrl]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::onPowerOff();
    zylaCtrl::whilePowerOff();
    zylaCtrl::appShutdown();
    #endif
    // clang-format on

    resetZylaStub();

    zylaCtrl_test app( "camzyla" );

    SECTION( "onPowerOff with an open camera" )
    {
        app.setOpen();
        app.m_powerOnCounter = 5;

        REQUIRE( app.onPowerOff() == 0 );

        REQUIRE( app.m_powerOnCounter == 0 );
        REQUIRE( wasClosed( c_zylaHandleBase ) );
        REQUIRE( app.m_handle == AT_HANDLE_UNINITIALISED );
        REQUIRE( app.m_libInit == false );
        REQUIRE( g_zylaStub.finLibCalls == 1 );
        REQUIRE( g_zylaStub.finUtilCalls == 1 );
    }

    SECTION( "onPowerOff with nothing open" )
    {
        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_zylaStub.closed.empty() );
        REQUIRE( g_zylaStub.finLibCalls == 0 );
    }

    SECTION( "whilePowerOff" )
    {
        REQUIRE( app.whilePowerOff() == 0 );
        REQUIRE( g_zylaStub.closed.empty() );
    }

    SECTION( "appShutdown with an open camera" )
    {
        app.setOpen();

        REQUIRE( app.appShutdown() == 0 );

        REQUIRE( wasClosed( c_zylaHandleBase ) );
        REQUIRE( app.m_handle == AT_HANDLE_UNINITIALISED );
        REQUIRE( app.m_libInit == false );
        REQUIRE( g_zylaStub.finLibCalls == 1 );
        REQUIRE( g_zylaStub.finUtilCalls == 1 );
    }

    SECTION( "appShutdown with nothing open" )
    {
        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_zylaStub.closed.empty() );
        REQUIRE( g_zylaStub.finUtilCalls == 0 );
    }
}

/// Verify the stdCamera INDI callbacks reach the zylaCtrl hooks and update the requested settings.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl stdCamera INDI callbacks", "[zylaCtrl][indi]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    dev::stdCamera<zylaCtrl>::newCallBack_stdCamera(pcf::IndiProperty());
    zylaCtrl::setTempControl();
    zylaCtrl::setTempSetPt();
    zylaCtrl::setFPS();
    zylaCtrl::setExpTime();
    zylaCtrl::setNextROI();
    #endif
    // clang-format on

    resetZylaStub();

    zylaCtrl_test app( "camzyla" );
    app.setOpen();

    SECTION( "wrong device" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "other", "fps", "target", 50 ) ) == -1 );
        REQUIRE( app.m_fpsSet == Approx( 20 ) );
    }

    SECTION( "unhandled property" )
    {
        // blacklevel is not exposed by zylaCtrl
        REQUIRE( app.newCallBack_stdCamera( numberProp( "camzyla", "blacklevel", "target", 5 ) ) == -1 );
    }

    SECTION( "temp_controller on" )
    {
        REQUIRE( app.newCallBack_stdCamera(
                     switchProp( "camzyla", "temp_controller", "toggle", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_tempControlStatusSet == true );
        REQUIRE( g_zylaStub.setBools[L"SensorCooling"] == AT_TRUE );
    }

    SECTION( "temp_controller off" )
    {
        app.m_tempControlStatusSet = true;
        REQUIRE( app.newCallBack_stdCamera(
                     switchProp( "camzyla", "temp_controller", "toggle", pcf::IndiElement::Off ) ) == 0 );
        REQUIRE( app.m_tempControlStatusSet == false );
        REQUIRE( g_zylaStub.setBools[L"SensorCooling"] == AT_FALSE );
    }

    SECTION( "temp_ccd target" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "camzyla", "temp_ccd", "target", -20 ) ) == 0 );
        REQUIRE( app.m_ccdTempSetpt == Approx( -20 ) );
    }

    SECTION( "fps target" )
    {
        app.m_reconfig = false;
        REQUIRE( app.newCallBack_stdCamera( numberProp( "camzyla", "fps", "target", 50 ) ) == 0 );
        REQUIRE( app.m_fpsSet == Approx( 50 ) );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "exptime target" )
    {
        app.m_reconfig = false;
        REQUIRE( app.newCallBack_stdCamera( numberProp( "camzyla", "exptime", "target", 0.01 ) ) == 0 );
        REQUIRE( app.m_expTimeSet == Approx( 0.01 ) );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "roi_region_x target" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "camzyla", "roi_region_x", "target", 1000 ) ) == 0 );
        REQUIRE( app.m_nextROI.x == Approx( 1000 ) );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "roi_set request" )
    {
        app.m_reconfig     = false;
        app.m_currentROI.x = 321;
        REQUIRE( app.newCallBack_stdCamera( switchProp( "camzyla", "roi_set", "request", pcf::IndiElement::On ) ) ==
                 0 );
        REQUIRE( app.m_reconfig == true );
        REQUIRE( app.m_lastROI.x == Approx( 321 ) );
    }
}

/// Verify the telemetry interface records the stdcam telemetry.
/**
 * \ingroup zylaCtrl_unit_test
 */
TEST_CASE( "zylaCtrl telemetry", "[zylaCtrl][telem]" )
{
    // clang-format off
    #ifdef ZYLACTRL_TEST_DOXYGEN_REF
    zylaCtrl::recordTelem(nullptr);
    zylaCtrl::checkRecordTimes();
    #endif
    // clang-format on

    resetZylaStub();

    zylaCtrl_test app( "camzyla" );

    SECTION( "recordTelem" )
    {
        MagAOX::logger::telem_stdcam::lastRecord = { 0, 0 };

        REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_stdcam *>( nullptr ) ) == 0 );
        REQUIRE( MagAOX::logger::telem_stdcam::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records when overdue" )
    {
        MagAOX::logger::telem_stdcam::lastRecord = { 0, 0 };

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_stdcam::lastRecord.tv_sec > 0 );
    }
}

} // namespace zylaCtrlTest

} // namespace libXWCTest
