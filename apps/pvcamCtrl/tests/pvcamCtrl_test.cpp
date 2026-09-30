/** \file pvcamCtrl_test.cpp
 * \brief Catch2 tests for the pvcamCtrl app.
 * \author Claude Code
 *
 * \ingroup pvcamCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <semaphore.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define protected public
#include "../pvcamCtrl.hpp"
#undef protected

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace
{

/// The storage type the stub uses when reading or writing a parameter value.
enum class pvStubType
{
    u16, ///< uns16 (also rs_bool)
    i16, ///< int16
    i32, ///< int32 (also PVCAM enums)
    u32, ///< uns32
    i64, ///< long64
    u64  ///< ulong64
};

/// Fake PVCAM SDK state shared by the stub functions below.
struct pvcamStubState
{
    std::vector<std::string> cameraNames; ///< Camera names, by index.  The handle of a camera is its index.

    std::vector<std::string> cameraSerials; ///< Camera serial numbers, by index.

    std::vector<bool> serialAvail; ///< Whether PARAM_HEAD_SER_NUM_ALPHA is available, by camera index.

    std::set<std::string> openFailNames; ///< Camera names for which pl_cam_open fails.

    std::map<std::pair<uns32, int>, long long> params; ///< Parameter values, keyed by (param ID, attribute).

    std::set<std::pair<uns32, int>> getFails; ///< (param ID, attribute) pairs for which pl_get_param fails.

    std::set<uns32> setFails; ///< Parameter IDs for which pl_set_param fails.

    std::vector<std::pair<uns32, long long>> setCalls; ///< Every (param ID, value) sent to pl_set_param.

    std::vector<std::string> portNames; ///< PARAM_READOUT_PORT entry descriptions.

    std::vector<int32> portValues; ///< PARAM_READOUT_PORT entry values.

    bool initReturn{ true }; ///< Return value of pl_pvcam_init.

    bool uninitReturn{ true }; ///< Return value of pl_pvcam_uninit.

    int16 uninitError{ 157 }; ///< Error code set when pl_pvcam_uninit fails.

    bool getTotalReturn{ true }; ///< Return value of pl_cam_get_total.

    bool closeReturn{ true }; ///< Return value of pl_cam_close.

    bool enumStrLengthReturn{ true }; ///< Return value of pl_enum_str_length.

    bool registerReturn{ true }; ///< Return value of pl_cam_register_callback_ex3.

    bool deregisterReturn{ true }; ///< Return value of pl_cam_deregister_callback.

    bool setupReturn{ true }; ///< Return value of pl_exp_setup_cont.

    bool startReturn{ true }; ///< Return value of pl_exp_start_cont.

    bool stopReturn{ true }; ///< Return value of pl_exp_stop_cont.

    int16 errorCode{ 0 }; ///< Value returned by pl_error_code, set by failing calls.

    int initCalls{ 0 }; ///< Number of pl_pvcam_init calls.

    int uninitCalls{ 0 }; ///< Number of pl_pvcam_uninit calls.

    int openCalls{ 0 }; ///< Number of pl_cam_open calls.

    std::vector<int16> closedHandles; ///< Handles passed to pl_cam_close, in order.

    int16 lastOpenMode{ -1 }; ///< Open mode passed to the last pl_cam_open.

    int getCalls{ 0 }; ///< Number of pl_get_param calls.

    int enumStrLengthCalls{ 0 }; ///< Number of pl_enum_str_length calls.

    int registerCalls{ 0 }; ///< Number of pl_cam_register_callback_ex3 calls.

    int deregisterCalls{ 0 }; ///< Number of pl_cam_deregister_callback calls.

    int32 lastCallbackEvent{ -1 }; ///< Event passed to the last pl_cam_register_callback_ex3.

    void *lastCallback{ nullptr }; ///< Callback passed to the last pl_cam_register_callback_ex3.

    void *lastContext{ nullptr }; ///< Context passed to the last pl_cam_register_callback_ex3.

    int setupCalls{ 0 }; ///< Number of pl_exp_setup_cont calls.

    rgn_type lastRgn{ 0, 0, 0, 0, 0, 0 }; ///< Region passed to the last pl_exp_setup_cont.

    uns16 lastRgnTotal{ 0 }; ///< Region count passed to the last pl_exp_setup_cont.

    uns32 lastExposure{ 0 }; ///< Exposure time passed to the last pl_exp_setup_cont.

    int16 lastExpMode{ -1 }; ///< Exposure mode passed to the last pl_exp_setup_cont.

    int16 lastBufferMode{ -1 }; ///< Buffer mode passed to the last pl_exp_setup_cont.

    uns32 frameBytes{ 512 * 512 * 2 }; ///< Frame size in bytes reported by pl_exp_setup_cont.

    int startCalls{ 0 }; ///< Number of pl_exp_start_cont calls.

    void *startBuffer{ nullptr }; ///< Buffer passed to the last pl_exp_start_cont.

    uns32 startSize{ 0 }; ///< Buffer size passed to the last pl_exp_start_cont.

    int stopCalls{ 0 }; ///< Number of pl_exp_stop_cont calls.

    int16 lastStopState{ -1 }; ///< Abort mode passed to the last pl_exp_stop_cont.

    std::vector<uns8> frame; ///< The frame returned by pl_exp_get_latest_frame.

    /// Set a parameter attribute value.
    void setParam( uns32     param, /**< [in] the parameter ID */
                   int       attr,  /**< [in] the attribute */
                   long long val    /**< [in] the value */
    )
    {
        params[{ param, attr }] = val;
    }

    /// Get a parameter attribute value, or 0 if it was never set.
    long long getParam( uns32 param, /**< [in] the parameter ID */
                        int   attr   /**< [in] the attribute */
    ) const
    {
        auto it = params.find( { param, attr } );
        if( it == params.end() )
        {
            return 0;
        }

        return it->second;
    }

    /// Get the last value sent to a parameter with pl_set_param, or -99999 if never set.
    long long lastSet( uns32 param /**< [in] the parameter ID */ ) const
    {
        for( auto it = setCalls.rbegin(); it != setCalls.rend(); ++it )
        {
            if( it->first == param )
            {
                return it->second;
            }
        }

        return -99999;
    }

    /// Get the number of pl_set_param calls for a parameter.
    int setCount( uns32 param /**< [in] the parameter ID */ ) const
    {
        int n = 0;
        for( const auto &c : setCalls )
        {
            if( c.first == param )
            {
                ++n;
            }
        }

        return n;
    }
};

/// The global fake PVCAM SDK state.
pvcamStubState g_pvStub;

/// Reset the fake PVCAM SDK state before a test.
void resetPvcamStub()
{
    g_pvStub = pvcamStubState();
}

/// The error code set by the stub functions when they are told to fail.
constexpr int16 c_pvStubError = 42;

/// Get the storage type of a parameter attribute, matching how pvcamCtrl declares its variables.
/** For PARAM_EXPOSURE_TIME the real SDK type is uns64 for every attribute, but pvcamCtrl reads ATTR_CURRENT
 * into an uns32, so the stub writes 32 bits for that attribute.  PARAM_SPDTAB_INDEX is set from an uns32.
 */
pvStubType pvParamType( uns32 param, /**< [in] the parameter ID */
                        int   attr   /**< [in] the attribute */
)
{
    if( attr == ATTR_AVAIL )
    {
        return pvStubType::u16;
    }

    if( attr == ATTR_COUNT )
    {
        return pvStubType::u32;
    }

    switch( param )
    {
    case PARAM_EXPOSURE_TIME:
        return ( attr == ATTR_CURRENT ) ? pvStubType::u32 : pvStubType::u64;
    case PARAM_READOUT_TIME:
    case PARAM_SPDTAB_INDEX:
        return pvStubType::u32;
    case PARAM_PRE_TRIGGER_DELAY:
    case PARAM_CLEARING_TIME:
    case PARAM_POST_TRIGGER_DELAY:
        return pvStubType::i64;
    case PARAM_EXP_RES_INDEX:
    case PARAM_PIX_TIME:
        return pvStubType::u16;
    case PARAM_GAIN_INDEX:
    case PARAM_BIT_DEPTH:
    case PARAM_TEMP:
    case PARAM_TEMP_SETPOINT:
        return pvStubType::i16;
    default:
        return pvStubType::i32;
    }
}

/// Write a value to a parameter destination with the given storage type.
void pvWrite( void      *dst, /**< [out] the destination */
              pvStubType t,   /**< [in] the storage type */
              long long  v    /**< [in] the value */
)
{
    switch( t )
    {
    case pvStubType::u16:
        *static_cast<uns16 *>( dst ) = static_cast<uns16>( v );
        break;
    case pvStubType::i16:
        *static_cast<int16 *>( dst ) = static_cast<int16>( v );
        break;
    case pvStubType::i32:
        *static_cast<int32 *>( dst ) = static_cast<int32>( v );
        break;
    case pvStubType::u32:
        *static_cast<uns32 *>( dst ) = static_cast<uns32>( v );
        break;
    case pvStubType::i64:
        *static_cast<long64 *>( dst ) = static_cast<long64>( v );
        break;
    case pvStubType::u64:
        *static_cast<ulong64 *>( dst ) = static_cast<ulong64>( v );
        break;
    }
}

/// Read a value from a parameter source with the given storage type.
long long pvRead( const void *src, /**< [in] the source */
                  pvStubType  t    /**< [in] the storage type */
)
{
    switch( t )
    {
    case pvStubType::u16:
        return *static_cast<const uns16 *>( src );
    case pvStubType::i16:
        return *static_cast<const int16 *>( src );
    case pvStubType::i32:
        return *static_cast<const int32 *>( src );
    case pvStubType::u32:
        return *static_cast<const uns32 *>( src );
    case pvStubType::i64:
        return *static_cast<const long64 *>( src );
    case pvStubType::u64:
        return static_cast<long long>( *static_cast<const ulong64 *>( src ) );
    }

    return 0;
}

/// Record a stub failure and return PV_FAIL.
rs_bool pvFail()
{
    g_pvStub.errorCode = c_pvStubError;
    return PV_FAIL;
}

} // namespace

extern "C"
{

    int16 pl_error_code( void )
    {
        return g_pvStub.errorCode;
    }

    rs_bool pl_error_message( int16 err_code, char *msg )
    {
        snprintf( msg, ERROR_MSG_LEN, "stub error %d", static_cast<int>( err_code ) );
        return PV_OK;
    }

    rs_bool pl_pvcam_init( void )
    {
        ++g_pvStub.initCalls;
        return g_pvStub.initReturn ? PV_OK : pvFail();
    }

    rs_bool pl_pvcam_uninit( void )
    {
        ++g_pvStub.uninitCalls;
        if( !g_pvStub.uninitReturn )
        {
            g_pvStub.errorCode = g_pvStub.uninitError;
            return PV_FAIL;
        }

        return PV_OK;
    }

    rs_bool pl_cam_get_total( int16 *totl_cams )
    {
        if( !g_pvStub.getTotalReturn )
        {
            return pvFail();
        }

        *totl_cams = static_cast<int16>( g_pvStub.cameraNames.size() );
        return PV_OK;
    }

    rs_bool pl_cam_get_name( int16 cam_num, char *cam_name )
    {
        if( cam_num < 0 || cam_num >= static_cast<int16>( g_pvStub.cameraNames.size() ) )
        {
            return pvFail();
        }

        snprintf( cam_name, CAM_NAME_LEN, "%s", g_pvStub.cameraNames[cam_num].c_str() );
        return PV_OK;
    }

    rs_bool pl_cam_open( char *cam_name, int16 *hcam, int16 o_mode )
    {
        ++g_pvStub.openCalls;
        g_pvStub.lastOpenMode = o_mode;

        std::string name( cam_name );
        if( g_pvStub.openFailNames.count( name ) > 0 )
        {
            return pvFail();
        }

        for( size_t n = 0; n < g_pvStub.cameraNames.size(); ++n )
        {
            if( g_pvStub.cameraNames[n] == name )
            {
                *hcam = static_cast<int16>( n );
                return PV_OK;
            }
        }

        return pvFail();
    }

    rs_bool pl_cam_close( int16 hcam )
    {
        g_pvStub.closedHandles.push_back( hcam );
        return g_pvStub.closeReturn ? PV_OK : pvFail();
    }

    rs_bool pl_get_param( int16 hcam, uns32 param_id, int16 param_attribute, void *param_value )
    {
        ++g_pvStub.getCalls;

        if( g_pvStub.getFails.count( { param_id, static_cast<int>( param_attribute ) } ) > 0 )
        {
            return pvFail();
        }

        if( param_id == PARAM_HEAD_SER_NUM_ALPHA )
        {
            if( hcam < 0 || hcam >= static_cast<int16>( g_pvStub.cameraSerials.size() ) )
            {
                return pvFail();
            }

            if( param_attribute == ATTR_AVAIL )
            {
                bool avail =
                    static_cast<size_t>( hcam ) < g_pvStub.serialAvail.size() ? g_pvStub.serialAvail[hcam] : true;
                *static_cast<rs_bool *>( param_value ) = avail ? PV_OK : PV_FAIL;
                return PV_OK;
            }

            snprintf(
                static_cast<char *>( param_value ), MAX_ALPHA_SER_NUM_LEN, "%s", g_pvStub.cameraSerials[hcam].c_str() );
            return PV_OK;
        }

        pvWrite(
            param_value, pvParamType( param_id, param_attribute ), g_pvStub.getParam( param_id, param_attribute ) );

        return PV_OK;
    }

    rs_bool pl_set_param( int16 hcam, uns32 param_id, void *param_value )
    {
        static_cast<void>( hcam );

        long long val = pvRead( param_value, pvParamType( param_id, ATTR_CURRENT ) );
        g_pvStub.setCalls.push_back( { param_id, val } );

        if( g_pvStub.setFails.count( param_id ) > 0 )
        {
            return pvFail();
        }

        g_pvStub.setParam( param_id, ATTR_CURRENT, val );

        return PV_OK;
    }

    rs_bool pl_get_enum_param( int16 hcam, uns32 param_id, uns32 index, int32 *value, char *desc, uns32 length )
    {
        static_cast<void>( hcam );
        static_cast<void>( param_id );

        if( index >= g_pvStub.portNames.size() || length == 0 )
        {
            return pvFail();
        }

        *value = index < g_pvStub.portValues.size() ? g_pvStub.portValues[index] : static_cast<int32>( index );
        snprintf( desc, length, "%s", g_pvStub.portNames[index].c_str() );

        return PV_OK;
    }

    rs_bool pl_enum_str_length( int16 hcam, uns32 param_id, uns32 index, uns32 *length )
    {
        static_cast<void>( hcam );
        static_cast<void>( param_id );

        ++g_pvStub.enumStrLengthCalls;

        if( !g_pvStub.enumStrLengthReturn || index >= g_pvStub.portNames.size() )
        {
            return pvFail();
        }

        *length = static_cast<uns32>( g_pvStub.portNames[index].size() + 1 );
        return PV_OK;
    }

    rs_bool pl_cam_register_callback_ex3( int16 hcam, int32 callback_event, void *callback, void *context )
    {
        static_cast<void>( hcam );

        ++g_pvStub.registerCalls;
        g_pvStub.lastCallbackEvent = callback_event;
        g_pvStub.lastCallback      = callback;
        g_pvStub.lastContext       = context;

        return g_pvStub.registerReturn ? PV_OK : pvFail();
    }

    rs_bool pl_cam_deregister_callback( int16 hcam, int32 callback_event )
    {
        static_cast<void>( hcam );
        static_cast<void>( callback_event );

        ++g_pvStub.deregisterCalls;

        return g_pvStub.deregisterReturn ? PV_OK : pvFail();
    }

    rs_bool pl_exp_setup_cont( int16           hcam,
                               uns16           rgn_total,
                               const rgn_type *rgn_array,
                               int16           exp_mode,
                               uns32           exposure_time,
                               uns32          *exp_bytes,
                               int16           buffer_mode )
    {
        static_cast<void>( hcam );

        ++g_pvStub.setupCalls;
        g_pvStub.lastRgnTotal   = rgn_total;
        g_pvStub.lastRgn        = rgn_array[0];
        g_pvStub.lastExpMode    = exp_mode;
        g_pvStub.lastExposure   = exposure_time;
        g_pvStub.lastBufferMode = buffer_mode;

        if( !g_pvStub.setupReturn )
        {
            return pvFail();
        }

        *exp_bytes = g_pvStub.frameBytes;

        // The camera accepts the requested exposure time
        g_pvStub.setParam( PARAM_EXPOSURE_TIME, ATTR_CURRENT, exposure_time );

        return PV_OK;
    }

    rs_bool pl_exp_start_cont( int16 hcam, void *pixel_stream, uns32 size )
    {
        static_cast<void>( hcam );

        ++g_pvStub.startCalls;
        g_pvStub.startBuffer = pixel_stream;
        g_pvStub.startSize   = size;

        return g_pvStub.startReturn ? PV_OK : pvFail();
    }

    rs_bool pl_exp_get_latest_frame( int16 hcam, void **frame )
    {
        static_cast<void>( hcam );

        *frame = static_cast<void *>( g_pvStub.frame.data() );
        return PV_OK;
    }

    rs_bool pl_exp_stop_cont( int16 hcam, int16 cam_state )
    {
        static_cast<void>( hcam );

        ++g_pvStub.stopCalls;
        g_pvStub.lastStopState = cam_state;

        return g_pvStub.stopReturn ? PV_OK : pvFail();
    }

} // extern "C"
/// \endcond

namespace libXWCTest
{

/** \defgroup pvcamCtrl_unit_test pvcamCtrl Unit Tests
 * \brief Unit tests for the pvcamCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `pvcamCtrl` unit tests.
/** \ingroup pvcamCtrl_unit_test
 */
namespace pvcamCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness which initializes the pvcamCtrl state normally set up by appStartup and the power system.
class pvcamCtrl_test : public pvcamCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit pvcamCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        // Power is on
        m_powerState       = 1;
        m_powerTargetState = 1;

        // These are normally initialized in appStartup()
        sem_init( &m_frSemaphore, 0, 0 );
        sem_init( &m_frDoneSemaphore, 0, 0 );

        setupProp( m_indiP_exptime, "exptime" );
        setupProp( m_indiP_fps, "fps" );
    }

    /// Destructor, frees the circular buffer and the semaphores.
    ~pvcamCtrl_test() noexcept
    {
        delete[] m_circBuff; // pvcamCtrl never frees this
        m_circBuff      = nullptr;
        m_circBuffBytes = 0;

        sem_destroy( &m_frSemaphore );
        sem_destroy( &m_frDoneSemaphore );
    }

    /// Set the device and name of a property owned by this app.
    void setupProp( pcf::IndiProperty &prop, /**< [out] the property to set up */
                    const std::string &name  /**< [in] the INDI property name */
    )
    {
        prop.setDevice( m_configName );
        prop.setName( name );
    }

    /// Setup the configuration, read a config file, and load it.
    void loadTestConfig( const std::string &path /**< [in] path of the config file */ )
    {
        setupConfig();
        config.readConfig( path );
        loadConfig();
    }

    /// Prepare the app and the stub for configureAcquisition and the exposure-time hooks.
    /** Opens handle 0, uses a small (1 MB) circular buffer, and sets the stub exposure limits, readout time,
     * and fan availability.
     */
    void prepareAcq()
    {
        m_handle           = 0;
        m_circBuffMaxBytes = 1048576;

        g_pvStub.setParam( PARAM_EXPOSURE_TIME, ATTR_MIN, 10 );
        g_pvStub.setParam( PARAM_EXPOSURE_TIME, ATTR_MAX, 100000000 );
        g_pvStub.setParam( PARAM_READOUT_TIME, ATTR_CURRENT, 5000 );
        g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_AVAIL, 1 );
        g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_CURRENT, FAN_SPEED_HIGH );
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

/// Verify the pvcamCtrl constructor defaults and pvcamErrMessage formatting.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl constructor defaults and error message", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::pvcamCtrl();
    pvcamErrMessage( "", 0, "" );
    #endif
    // clang-format on

    resetPvcamStub();

    SECTION( "constructor defaults" )
    {
        pvcamCtrl_test app( "campv" );

        REQUIRE( app.m_powerMgtEnabled == true );
        REQUIRE( app.m_powerOnWait == 15u );
        REQUIRE( app.m_handle == -1 );
        REQUIRE( app.m_expTime == Approx( 0.01 ) );
        REQUIRE( app.m_expTimeSet == Approx( 0.01 ) );
        REQUIRE( app.m_tempTol == Approx( 0.1 ) );
        REQUIRE( app.m_circBuffMaxBytes == 536870912u );
        REQUIRE( app.m_acqSleep == 5000u );
        REQUIRE( app.m_circBuff == nullptr );

        REQUIRE( app.m_default_x == Approx( 1599.5 ) );
        REQUIRE( app.m_default_y == Approx( 1599.5 ) );
        REQUIRE( app.m_default_w == 512 );
        REQUIRE( app.m_default_h == 512 );
        REQUIRE( app.m_full_w == 3200 );
        REQUIRE( app.m_full_h == 3200 );

        REQUIRE( app.m_defaultReadoutSpeed == "dynamic_range" );
        REQUIRE( app.m_readoutSpeedNames ==
                 std::vector<std::string>( { "sensitivity", "speed", "dynamic_range", "sub_electron" } ) );
        REQUIRE( app.m_readoutSpeedNameLabels.size() == 4 );
        REQUIRE( app.m_readoutSpeedName == "dynamic_range" );
        REQUIRE( app.m_readoutSpeedNameSet == "dynamic_range" );

        REQUIRE( app.m_defaultFanSpeed == "high" );
        REQUIRE( app.m_fanSpeedNames == std::vector<std::string>( { "high", "medium", "low", "off" } ) );
        REQUIRE( app.m_fanSpeedNameLabels == std::vector<std::string>( { "High", "Medium", "Low", "Off" } ) );
        REQUIRE( app.m_fanSpeedName == "high" );
        REQUIRE( app.m_fanSpeedNameSet == "high" );

        REQUIRE( pvcamCtrl::c_stdCamera_readoutSpeed == true );
        REQUIRE( pvcamCtrl::c_stdCamera_fanSpeed == true );
        REQUIRE( pvcamCtrl::c_stdCamera_usesROI == true );
        REQUIRE( pvcamCtrl::c_stdCamera_tempControl == false );
        REQUIRE( pvcamCtrl::c_frameGrabber_flippable == false );
    }

    SECTION( "pvcamErrMessage formats the PVCAM message" )
    {
        REQUIRE( pvcamErrMessage( "pl_cam_open", 7, "retrying" ) == "pl_cam_open failed: stub error 7 retrying" );
        REQUIRE( pvcamErrMessage( "pl_cam_open", 7, "" ) == "pl_cam_open failed: stub error 7" );
    }
}

/// Verify configuration defaults, overrides, and fatal configuration errors.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl configuration", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::setupConfig();
    pvcamCtrl::loadConfigImpl( std::declval<mx::app::appConfigurator &>() );
    pvcamCtrl::loadConfig();
    #endif
    // clang-format on

    resetPvcamStub();

    SECTION( "defaults with only the serial number" )
    {
        pvcamCtrl_test app( "campv" );

        const std::string path = "/tmp/pvcamCtrl_test_defaults.conf";
        mx::app::writeConfigFile( path, { "camera" }, { "serialNumber" }, { "A1234" } );
        app.loadTestConfig( path );
        std::remove( path.c_str() );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_serialNumber == "A1234" );
        REQUIRE( app.m_circBuffMaxBytes == 536870912u );
        REQUIRE( app.m_acqSleep == 5000u );
        REQUIRE( app.m_defaultReadoutSpeed == "dynamic_range" );
        REQUIRE( app.m_fanSpeedControlEnabled == true );
        REQUIRE( app.m_defaultFanSpeed == "high" );
        REQUIRE( app.m_shmimName == "campv" );

        // stdCamera starts the current and next ROI at the defaults
        REQUIRE( app.m_currentROI.x == Approx( 1599.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 1599.5 ) );
        REQUIRE( app.m_currentROI.w == 512 );
        REQUIRE( app.m_currentROI.h == 512 );
        REQUIRE( app.m_nextROI.w == 512 );
        REQUIRE( app.m_nextROI.bin_x == 1 );
        REQUIRE( app.m_nextROI.bin_y == 1 );
    }

    SECTION( "overrides" )
    {
        pvcamCtrl_test app( "campv" );

        const std::string path = "/tmp/pvcamCtrl_test_overrides.conf";
        mx::app::writeConfigFile( path,
                                  { "camera",
                                    "camera",
                                    "camera",
                                    "camera",
                                    "camera",
                                    "camera",
                                    "camera",
                                    "framegrabber",
                                    "framegrabber",
                                    "shutter" },
                                  { "serialNumber",
                                    "circBuffMaxBytes",
                                    "defaultReadoutSpeed",
                                    "fanSpeedControl",
                                    "defaultFanSpeed",
                                    "default_w",
                                    "default_h",
                                    "acqSleep",
                                    "shmimName",
                                    "dioDevice" },
                                  { "B5678", "1048576", "speed", "0", "low", "256", "128", "2000", "pvtest", "dio" } );
        app.loadTestConfig( path );
        std::remove( path.c_str() );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_serialNumber == "B5678" );
        REQUIRE( app.m_circBuffMaxBytes == 1048576u );
        REQUIRE( app.m_defaultReadoutSpeed == "speed" );
        REQUIRE( app.m_fanSpeedControlEnabled == false );
        REQUIRE( app.m_defaultFanSpeed == "low" );
        REQUIRE( app.m_acqSleep == 2000u );
        REQUIRE( app.m_shmimName == "pvtest" );
        REQUIRE( app.m_dioDevice == "dio" );
        REQUIRE( app.m_nextROI.w == 256 );
        REQUIRE( app.m_nextROI.h == 128 );
        REQUIRE( app.m_currentROI.w == 256 );
    }

    SECTION( "missing serial number is fatal" )
    {
        pvcamCtrl_test app( "campv" );

        const std::string path = "/tmp/pvcamCtrl_test_noserial.conf";
        mx::app::writeConfigFile( path, { "none" }, { "nada" }, { "0" } );
        app.loadTestConfig( path );
        std::remove( path.c_str() );

        REQUIRE( app.m_shutdown != 0 );
        REQUIRE( app.m_serialNumber == "" );
    }

    SECTION( "invalid default fan speed is fatal" )
    {
        pvcamCtrl_test app( "campv" );

        const std::string path = "/tmp/pvcamCtrl_test_badfan.conf";
        mx::app::writeConfigFile(
            path, { "camera", "camera" }, { "serialNumber", "defaultFanSpeed" }, { "A1234", "turbo" } );
        app.loadTestConfig( path );
        std::remove( path.c_str() );

        REQUIRE( app.m_shutdown != 0 );
    }
}

/// Verify powerOnDefaults and the trivial stdCamera hooks.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl power-on defaults and simple stdCamera hooks", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::powerOnDefaults();
    pvcamCtrl::setTempControl();
    pvcamCtrl::setTempSetPt();
    pvcamCtrl::setVShiftSpeed();
    pvcamCtrl::setEMGain();
    pvcamCtrl::setReadoutSpeed();
    pvcamCtrl::setNextROI();
    pvcamCtrl::setShutter( 0 );
    pvcamCtrl::fps();
    #endif
    // clang-format on

    resetPvcamStub();

    SECTION( "powerOnDefaults with fan control enabled" )
    {
        pvcamCtrl_test app( "campv" );

        app.m_expTime                = 1.5;
        app.m_expTimeSet             = 2.5;
        app.m_defaultReadoutSpeed    = "speed";
        app.m_readoutSpeedName       = "sensitivity";
        app.m_readoutSpeedNameSet    = "sensitivity";
        app.m_defaultFanSpeed        = "medium";
        app.m_fanSpeedName           = "off";
        app.m_fanSpeedNameSet        = "off";
        app.m_fanSpeedValid          = true;
        app.m_fanSpeedControlEnabled = true;

        REQUIRE( app.powerOnDefaults() == 0 );

        REQUIRE( app.m_expTime == Approx( 0.01 ) );
        REQUIRE( app.m_expTimeSet == Approx( 0.01 ) );
        REQUIRE( app.m_readoutSpeedName == "speed" );
        REQUIRE( app.m_readoutSpeedNameSet == "speed" );
        REQUIRE( app.m_fanSpeedName == "medium" );
        REQUIRE( app.m_fanSpeedNameSet == "medium" );
        REQUIRE( app.m_fanSpeedValid == false );
    }

    SECTION( "powerOnDefaults with fan control disabled" )
    {
        pvcamCtrl_test app( "campv" );

        app.m_fanSpeedControlEnabled = false;
        app.m_fanSpeedValid          = true;

        REQUIRE( app.powerOnDefaults() == 0 );

        REQUIRE( app.m_fanSpeedName == "" );
        REQUIRE( app.m_fanSpeedNameSet == "" );
        REQUIRE( app.m_fanSpeedValid == false );
    }

    SECTION( "no-op hooks and reconfig requests" )
    {
        pvcamCtrl_test app( "campv" );

        REQUIRE( app.setTempControl() == 0 );
        REQUIRE( app.setTempSetPt() == 0 );
        REQUIRE( app.setVShiftSpeed() == 0 );
        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( g_pvStub.setCalls.empty() );

        app.m_reconfig = false;
        REQUIRE( app.setReadoutSpeed() == 0 );
        REQUIRE( app.m_reconfig == true );

        app.m_reconfig = false;
        REQUIRE( app.setNextROI() == 0 );
        REQUIRE( app.m_reconfig == true );

        app.m_fps = 123.5;
        REQUIRE( app.fps() == Approx( 123.5 ) );
    }

    SECTION( "invalid shutter request" )
    {
        pvcamCtrl_test app( "campv" );

        // Only an invalid request is tested: 0 or 1 signal the dssShutter threads, which are not running.
        REQUIRE( app.setShutter( 5 ) == -1 );
    }
}

/// Verify setFanSpeed maps the fan-speed names to PVCAM set points, and rejects invalid requests.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl setFanSpeed", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::setFanSpeed();
    #endif
    // clang-format on

    resetPvcamStub();

    SECTION( "each fan speed name" )
    {
        const std::vector<std::pair<std::string, int>> speeds = { { "high", FAN_SPEED_HIGH },
                                                                  { "medium", FAN_SPEED_MEDIUM },
                                                                  { "low", FAN_SPEED_LOW },
                                                                  { "off", FAN_SPEED_OFF } };

        for( const auto &sp : speeds )
        {
            resetPvcamStub();
            pvcamCtrl_test app( "campv" );
            app.m_handle        = 0;
            app.m_fanSpeedName  = "unknown";
            app.m_fanSpeedValid = false;

            app.m_fanSpeedNameSet = sp.first;
            REQUIRE( app.setFanSpeed() == 0 );

            REQUIRE( g_pvStub.setCount( PARAM_FAN_SPEED_SETPOINT ) == 1 );
            REQUIRE( g_pvStub.lastSet( PARAM_FAN_SPEED_SETPOINT ) == sp.second );
            REQUIRE( app.m_fanSpeedName == sp.first );
            REQUIRE( app.m_fanSpeedValid == true );
        }
    }

    SECTION( "setting the current speed again" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_handle          = 0;
        app.m_fanSpeedName    = "low";
        app.m_fanSpeedNameSet = "low";

        REQUIRE( app.setFanSpeed() == 0 );
        REQUIRE( g_pvStub.lastSet( PARAM_FAN_SPEED_SETPOINT ) == FAN_SPEED_LOW );
        REQUIRE( app.m_fanSpeedName == "low" );
    }

    SECTION( "invalid fan speed name" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_handle          = 0;
        app.m_fanSpeedName    = "high";
        app.m_fanSpeedNameSet = "turbo";
        app.m_fanSpeedValid   = false;

        REQUIRE( app.setFanSpeed() == -1 );
        REQUIRE( g_pvStub.setCalls.empty() );
        REQUIRE( app.m_fanSpeedName == "high" );
        REQUIRE( app.m_fanSpeedValid == false );
    }

    SECTION( "SDK error" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_handle          = 0;
        app.m_fanSpeedName    = "high";
        app.m_fanSpeedNameSet = "off";
        app.m_fanSpeedValid   = false;

        g_pvStub.setFails.insert( PARAM_FAN_SPEED_SETPOINT );

        REQUIRE( app.setFanSpeed() == -1 );
        REQUIRE( g_pvStub.setCount( PARAM_FAN_SPEED_SETPOINT ) == 1 );
        REQUIRE( app.m_fanSpeedName == "high" );
        REQUIRE( app.m_fanSpeedValid == false );
    }
}

/// Verify getFanSpeed reads the PVCAM set point and handles unavailable, unknown, and failed reads.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl getFanSpeed", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::getFanSpeed();
    #endif
    // clang-format on

    resetPvcamStub();

    SECTION( "each PVCAM fan speed value" )
    {
        const std::vector<std::pair<int, std::string>> speeds = { { FAN_SPEED_HIGH, "high" },
                                                                  { FAN_SPEED_MEDIUM, "medium" },
                                                                  { FAN_SPEED_LOW, "low" },
                                                                  { FAN_SPEED_OFF, "off" } };

        for( const auto &sp : speeds )
        {
            resetPvcamStub();
            pvcamCtrl_test app( "campv" );
            app.m_handle        = 0;
            app.m_fanSpeedName  = "";
            app.m_fanSpeedValid = false;

            g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_AVAIL, 1 );
            g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_CURRENT, sp.first );

            REQUIRE( app.getFanSpeed() == 0 );
            REQUIRE( app.m_fanSpeedName == sp.second );
            REQUIRE( app.m_fanSpeedValid == true );
        }
    }

    SECTION( "unknown PVCAM value" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_fanSpeedName  = "high";
        app.m_fanSpeedValid = false;

        g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_AVAIL, 1 );
        g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_CURRENT, 17 );

        REQUIRE( app.getFanSpeed() == -1 );
        REQUIRE( app.m_fanSpeedName == "high" );
        REQUIRE( app.m_fanSpeedValid == false );
    }

    SECTION( "parameter not available" )
    {
        pvcamCtrl_test app( "campv" );

        g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_AVAIL, 0 );

        REQUIRE( app.getFanSpeed() == -1 );
        REQUIRE( app.m_fanSpeedValid == false );
    }

    SECTION( "availability read fails with power on" )
    {
        pvcamCtrl_test app( "campv" );

        g_pvStub.getFails.insert( { PARAM_FAN_SPEED_SETPOINT, ATTR_AVAIL } );

        REQUIRE( app.getFanSpeed() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "current read fails with power on" )
    {
        pvcamCtrl_test app( "campv" );

        g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_AVAIL, 1 );
        g_pvStub.getFails.insert( { PARAM_FAN_SPEED_SETPOINT, ATTR_CURRENT } );

        REQUIRE( app.getFanSpeed() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "read fails with power off" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_powerState = 0;

        g_pvStub.getFails.insert( { PARAM_FAN_SPEED_SETPOINT, ATTR_AVAIL } );

        REQUIRE( app.getFanSpeed() == 0 );
        REQUIRE( app.state() != stateCodes::ERROR );
    }

    SECTION( "no SDK access while operating or with fan control disabled" )
    {
        pvcamCtrl_test app( "campv" );

        app.state( stateCodes::OPERATING );
        REQUIRE( app.getFanSpeed() == 0 );
        REQUIRE( g_pvStub.getCalls == 0 );

        app.state( stateCodes::READY );
        app.m_fanSpeedControlEnabled = false;
        REQUIRE( app.getFanSpeed() == 0 );
        REQUIRE( g_pvStub.getCalls == 0 );
    }
}

/// Verify getTemp reads the detector temperature and set point and sets the lock status.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl getTemp", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::getTemp();
    #endif
    // clang-format on

    resetPvcamStub();

    SECTION( "on target" )
    {
        pvcamCtrl_test app( "campv" );

        g_pvStub.setParam( PARAM_TEMP_SETPOINT, ATTR_AVAIL, 1 );
        g_pvStub.setParam( PARAM_TEMP_SETPOINT, ATTR_CURRENT, -2500 );
        g_pvStub.setParam( PARAM_TEMP, ATTR_AVAIL, 1 );
        g_pvStub.setParam( PARAM_TEMP, ATTR_CURRENT, -2495 );

        REQUIRE( app.getTemp() == 0 );

        REQUIRE( app.m_ccdTempSetpt == Approx( -25.0 ) );
        REQUIRE( app.m_ccdTemp == Approx( -24.95 ) );
        REQUIRE( app.m_tempControlStatus == true );
        REQUIRE( app.m_tempControlOnTarget == true );
        REQUIRE( app.m_tempControlStatusStr == "LOCKED" );
    }

    SECTION( "off target" )
    {
        pvcamCtrl_test app( "campv" );

        g_pvStub.setParam( PARAM_TEMP_SETPOINT, ATTR_AVAIL, 1 );
        g_pvStub.setParam( PARAM_TEMP_SETPOINT, ATTR_CURRENT, -2500 );
        g_pvStub.setParam( PARAM_TEMP, ATTR_AVAIL, 1 );
        g_pvStub.setParam( PARAM_TEMP, ATTR_CURRENT, 1250 );

        REQUIRE( app.getTemp() == 0 );

        REQUIRE( app.m_ccdTempSetpt == Approx( -25.0 ) );
        REQUIRE( app.m_ccdTemp == Approx( 12.5 ) );
        REQUIRE( app.m_tempControlStatus == true );
        REQUIRE( app.m_tempControlOnTarget == false );
        REQUIRE( app.m_tempControlStatusStr == "UNLOCKED" );
    }

    SECTION( "parameters not available leave the values unchanged" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_ccdTemp      = 3.0;
        app.m_ccdTempSetpt = 5.0;

        g_pvStub.setParam( PARAM_TEMP_SETPOINT, ATTR_AVAIL, 0 );
        g_pvStub.setParam( PARAM_TEMP, ATTR_AVAIL, 0 );

        REQUIRE( app.getTemp() == 0 );

        REQUIRE( app.m_ccdTemp == Approx( 3.0 ) );
        REQUIRE( app.m_ccdTempSetpt == Approx( 5.0 ) );
        REQUIRE( app.m_tempControlStatusStr == "UNLOCKED" );
    }

    SECTION( "read fails with power off" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_powerState = 0;

        g_pvStub.getFails.insert( { PARAM_TEMP_SETPOINT, ATTR_AVAIL } );

        REQUIRE( app.getTemp() == 0 );
        REQUIRE( app.state() != stateCodes::ERROR );
    }

    SECTION( "no SDK access while operating" )
    {
        pvcamCtrl_test app( "campv" );
        app.state( stateCodes::OPERATING );

        REQUIRE( app.getTemp() == 0 );
        REQUIRE( g_pvStub.getCalls == 0 );
    }
}

/// Verify setExpTime and setFPS check the PVCAM exposure limits and request a reconfiguration.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl setExpTime and setFPS", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::setExpTime();
    pvcamCtrl::setFPS();
    #endif
    // clang-format on

    resetPvcamStub();

    SECTION( "within limits" )
    {
        pvcamCtrl_test app( "campv" );
        app.prepareAcq();
        app.m_reconfig   = false;
        app.m_expTimeSet = 0.5;

        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( app.m_expTimeSet == Approx( 0.5 ) );
        REQUIRE( app.m_reconfig == true );
        REQUIRE( app.m_fpsSetted == false );
    }

    SECTION( "below the minimum" )
    {
        pvcamCtrl_test app( "campv" );
        app.prepareAcq();
        g_pvStub.setParam( PARAM_EXPOSURE_TIME, ATTR_MIN, 2000000 ); // 2 s
        app.m_expTimeSet = 1.0;

        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( app.m_expTimeSet == Approx( 2.0 ) );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "above the maximum" )
    {
        pvcamCtrl_test app( "campv" );
        app.prepareAcq();
        g_pvStub.setParam( PARAM_EXPOSURE_TIME, ATTR_MAX, 10500000 ); // 10.5 s
        app.m_expTimeSet = 20.0;

        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( app.m_expTimeSet == Approx( 10.0 ) );
        REQUIRE( app.m_expTimeSet * 1e6 <= 10500000 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "minimum read fails" )
    {
        pvcamCtrl_test app( "campv" );
        app.prepareAcq();
        app.m_reconfig = false;
        g_pvStub.getFails.insert( { PARAM_EXPOSURE_TIME, ATTR_MIN } );

        REQUIRE( app.setExpTime() == -1 );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "maximum read fails" )
    {
        pvcamCtrl_test app( "campv" );
        app.prepareAcq();
        app.m_reconfig = false;
        g_pvStub.getFails.insert( { PARAM_EXPOSURE_TIME, ATTR_MAX } );

        REQUIRE( app.setExpTime() == -1 );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "setFPS converts to an exposure time" )
    {
        pvcamCtrl_test app( "campv" );
        app.prepareAcq();
        app.m_reconfig = false;
        app.m_fpsSet   = 4;

        REQUIRE( app.setFPS() == 0 );
        REQUIRE( app.m_expTimeSet == Approx( 0.25 ) );
        REQUIRE( app.m_fpsSetted == true );
        REQUIRE( app.m_reconfig == true );
    }
}

/// Verify checkNextROI keeps the requested ROI on the 3200x3200 detector.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl checkNextROI", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::checkNextROI();
    #endif
    // clang-format on

    resetPvcamStub();

    pvcamCtrl_test app( "campv" );

    SECTION( "valid ROI is unchanged" )
    {
        app.m_nextROI.x = 1599.5;
        app.m_nextROI.y = 1000;
        app.m_nextROI.w = 512;
        app.m_nextROI.h = 256;

        REQUIRE( app.checkNextROI() == 0 );

        REQUIRE( app.m_nextROI.x == Approx( 1599.5 ) );
        REQUIRE( app.m_nextROI.y == Approx( 1000 ) );
        REQUIRE( app.m_nextROI.w == 512 );
        REQUIRE( app.m_nextROI.h == 256 );
    }

    SECTION( "oversize ROI is clamped to the full frame" )
    {
        app.m_nextROI.x = 1599.5;
        app.m_nextROI.y = 1599.5;
        app.m_nextROI.w = 4000;
        app.m_nextROI.h = 5000;

        REQUIRE( app.checkNextROI() == 0 );

        REQUIRE( app.m_nextROI.w == 3200 );
        REQUIRE( app.m_nextROI.h == 3200 );
        REQUIRE( app.m_nextROI.x == Approx( 1599.5 ) );
        REQUIRE( app.m_nextROI.y == Approx( 1599.5 ) );
    }

    SECTION( "ROI past the high edges is trimmed" )
    {
        app.m_nextROI.x = 3100;
        app.m_nextROI.y = 3000;
        app.m_nextROI.w = 400;
        app.m_nextROI.h = 600;

        REQUIRE( app.checkNextROI() == 0 );

        // x0 = 2900, so w = 3199 - 2900 and x is recentered
        REQUIRE( app.m_nextROI.w == 299 );
        REQUIRE( app.m_nextROI.x == Approx( 3049.5 ) );

        // y0 = 2700, so h = 3199 - 2700 and y is recentered
        REQUIRE( app.m_nextROI.h == 499 );
        REQUIRE( app.m_nextROI.y == Approx( 2949.5 ) );
    }

    SECTION( "ROI past the low edges is trimmed" )
    {
        app.m_nextROI.x = 100;
        app.m_nextROI.y = 50;
        app.m_nextROI.w = 400;
        app.m_nextROI.h = 200;

        REQUIRE( app.checkNextROI() == 0 );

        // The part below 0 is removed from the width and height
        REQUIRE( app.m_nextROI.w == 300 );
        REQUIRE( app.m_nextROI.h == 150 );
    }
}

/// Verify connect finds the camera by serial number, and handles SDK errors.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl connect", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::connect();
    #endif
    // clang-format on

    resetPvcamStub();

    g_pvStub.cameraNames   = { "pvcamUSB_0", "pvcamUSB_1" };
    g_pvStub.cameraSerials = { "X999", "A1234" };
    g_pvStub.serialAvail   = { true, true };
    g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_AVAIL, 1 );
    g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_CURRENT, FAN_SPEED_LOW );

    SECTION( "camera found at the second index" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "A1234";

        REQUIRE( app.connect() == 0 );

        REQUIRE( g_pvStub.uninitCalls == 1 );
        REQUIRE( g_pvStub.initCalls == 1 );
        REQUIRE( g_pvStub.openCalls == 2 );
        REQUIRE( g_pvStub.lastOpenMode == OPEN_EXCLUSIVE );
        REQUIRE( g_pvStub.closedHandles == std::vector<int16>( { 0 } ) );

        REQUIRE( app.m_handle == 1 );
        REQUIRE( app.m_camName == "pvcamUSB_1" );
        REQUIRE( app.state() == stateCodes::CONNECTED );

        // exposure resolution set to microseconds
        REQUIRE( g_pvStub.lastSet( PARAM_EXP_RES_INDEX ) == 1 );

        // the default fan speed is applied after connecting
        REQUIRE( app.m_fanSpeedNameSet == "high" );
        REQUIRE( g_pvStub.lastSet( PARAM_FAN_SPEED_SETPOINT ) == FAN_SPEED_HIGH );
        REQUIRE( app.m_fanSpeedName == "high" );
        REQUIRE( app.m_fanSpeedValid == true );
    }

    SECTION( "fan control disabled does not touch the fan" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_serialNumber           = "A1234";
        app.m_fanSpeedControlEnabled = false;

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( g_pvStub.setCount( PARAM_FAN_SPEED_SETPOINT ) == 0 );
    }

    SECTION( "no cameras" )
    {
        g_pvStub.cameraNames.clear();
        g_pvStub.cameraSerials.clear();

        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "A1234";

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_handle == -1 );
        REQUIRE( g_pvStub.openCalls == 0 );
    }

    SECTION( "serial number not found" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "Z0000";

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_handle == -1 );
        REQUIRE( g_pvStub.closedHandles == std::vector<int16>( { 0, 1 } ) );
    }

    SECTION( "serial number not available" )
    {
        g_pvStub.serialAvail = { false, false };

        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "A1234";

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_handle == -1 );
        REQUIRE( g_pvStub.closedHandles == std::vector<int16>( { 0, 1 } ) );
    }

    SECTION( "a camera that fails to open is skipped" )
    {
        g_pvStub.openFailNames = { "pvcamUSB_0" };

        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "A1234";

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.m_handle == 1 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( g_pvStub.closedHandles.empty() );
    }

    SECTION( "an open handle is closed first" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "A1234";
        app.m_handle       = 5;

        REQUIRE( app.connect() == 0 );
        REQUIRE( g_pvStub.closedHandles.size() == 2 );
        REQUIRE( g_pvStub.closedHandles[0] == 5 );
        REQUIRE( app.m_handle == 1 );
    }

    SECTION( "closing an open handle fails" )
    {
        g_pvStub.closeReturn = false;

        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "A1234";
        app.m_handle       = 5;

        REQUIRE( app.connect() == -1 );
        REQUIRE( app.m_handle == 5 );
        REQUIRE( g_pvStub.initCalls == 0 );
    }

    SECTION( "uninit reporting not-initialized is not an error" )
    {
        g_pvStub.uninitReturn = false;

        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "A1234";

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.m_handle == 1 );
    }

    SECTION( "init fails" )
    {
        g_pvStub.initReturn = false;

        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "A1234";

        REQUIRE( app.connect() == -1 );
        REQUIRE( app.m_handle == -1 );
        REQUIRE( g_pvStub.openCalls == 0 );
    }

    SECTION( "camera count fails" )
    {
        g_pvStub.getTotalReturn = false;

        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "A1234";

        REQUIRE( app.connect() == -1 );
        REQUIRE( g_pvStub.openCalls == 0 );
    }

    SECTION( "serial number read fails" )
    {
        g_pvStub.getFails.insert( { PARAM_HEAD_SER_NUM_ALPHA, ATTR_CURRENT } );

        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "A1234";

        REQUIRE( app.connect() == -1 );
        REQUIRE( app.m_handle == -1 );
        REQUIRE( g_pvStub.closedHandles == std::vector<int16>( { 0 } ) );
    }

    SECTION( "fan speed read fails after connecting" )
    {
        g_pvStub.getFails.insert( { PARAM_FAN_SPEED_SETPOINT, ATTR_AVAIL } );

        pvcamCtrl_test app( "campv" );
        app.m_serialNumber = "A1234";

        REQUIRE( app.connect() == -1 );
        REQUIRE( app.m_handle == 1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_pvStub.setCount( PARAM_FAN_SPEED_SETPOINT ) == 0 );
    }
}

/// Verify fillSpeedTable enumerates the readout ports, speeds and gains, and dumpEnum walks an enumeration.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl fillSpeedTable and dumpEnum", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::fillSpeedTable();
    pvcamCtrl::dumpEnum( 0, "" );
    #endif
    // clang-format on

    resetPvcamStub();

    g_pvStub.portNames  = { "Sensitivity", "Speed" };
    g_pvStub.portValues = { 0, 1 };
    g_pvStub.setParam( PARAM_READOUT_PORT, ATTR_COUNT, 2 );
    g_pvStub.setParam( PARAM_SPDTAB_INDEX, ATTR_COUNT, 2 );
    g_pvStub.setParam( PARAM_PIX_TIME, ATTR_CURRENT, 10 );
    g_pvStub.setParam( PARAM_GAIN_INDEX, ATTR_COUNT, 3 );
    g_pvStub.setParam( PARAM_GAIN_INDEX, ATTR_MIN, 1 );
    g_pvStub.setParam( PARAM_BIT_DEPTH, ATTR_CURRENT, 16 );

    SECTION( "table is filled when connected" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_handle = 0;
        app.state( stateCodes::CONNECTED );

        REQUIRE( app.fillSpeedTable() == 0 );

        REQUIRE( app.m_ports.size() == 2 );
        REQUIRE( app.m_ports[0].index == 0 );
        REQUIRE( app.m_ports[0].value == 0 );
        REQUIRE( app.m_ports[0].name == "Sensitivity" );
        REQUIRE( app.m_ports[1].index == 1 );
        REQUIRE( app.m_ports[1].value == 1 );
        REQUIRE( app.m_ports[1].name == "Speed" );

        for( size_t p = 0; p < app.m_ports.size(); ++p )
        {
            REQUIRE( app.m_ports[p].speeds.size() == 2 );
            for( size_t s = 0; s < app.m_ports[p].speeds.size(); ++s )
            {
                REQUIRE( app.m_ports[p].speeds[s].pixTime == 10 );
                REQUIRE( app.m_ports[p].speeds[s].minG == 1 );
                REQUIRE( app.m_ports[p].speeds[s].gains.size() == 3 );
            }
        }

        // each port selected, each speed selected on each port, and gains 1-3 on each speed
        REQUIRE( g_pvStub.setCount( PARAM_READOUT_PORT ) == 2 );
        REQUIRE( g_pvStub.setCount( PARAM_SPDTAB_INDEX ) == 4 );
        REQUIRE( g_pvStub.setCount( PARAM_GAIN_INDEX ) == 12 );
        REQUIRE( g_pvStub.lastSet( PARAM_GAIN_INDEX ) == 3 );
    }

    SECTION( "not connected" )
    {
        pvcamCtrl_test app( "campv" );

        REQUIRE( app.fillSpeedTable() == -1 );
        REQUIRE( g_pvStub.getCalls == 0 );
        REQUIRE( app.m_ports.empty() );
    }

    SECTION( "port count read fails" )
    {
        pvcamCtrl_test app( "campv" );
        app.state( stateCodes::CONNECTED );
        g_pvStub.getFails.insert( { PARAM_READOUT_PORT, ATTR_COUNT } );

        REQUIRE( app.fillSpeedTable() == -1 );
        REQUIRE( app.m_ports.empty() );
    }

    SECTION( "string length read fails" )
    {
        pvcamCtrl_test app( "campv" );
        app.state( stateCodes::CONNECTED );
        g_pvStub.enumStrLengthReturn = false;

        REQUIRE( app.fillSpeedTable() == -1 );
    }

    SECTION( "setting the speed index fails" )
    {
        pvcamCtrl_test app( "campv" );
        app.state( stateCodes::CONNECTED );
        g_pvStub.setFails.insert( PARAM_SPDTAB_INDEX );

        REQUIRE( app.fillSpeedTable() == -1 );
        REQUIRE( g_pvStub.setCount( PARAM_SPDTAB_INDEX ) == 1 );
    }

    SECTION( "dumpEnum walks every entry when connected" )
    {
        pvcamCtrl_test app( "campv" );
        app.state( stateCodes::CONNECTED );

        app.dumpEnum( PARAM_READOUT_PORT, "PARAM_READOUT_PORT" );
        REQUIRE( g_pvStub.enumStrLengthCalls == 2 );
    }

    SECTION( "dumpEnum does nothing when not connected" )
    {
        pvcamCtrl_test app( "campv" );

        app.dumpEnum( PARAM_READOUT_PORT, "PARAM_READOUT_PORT" );
        REQUIRE( g_pvStub.getCalls == 0 );
        REQUIRE( g_pvStub.enumStrLengthCalls == 0 );
    }
}

/// Verify configureAcquisition sets the readout port, ROI, exposure, frame rate, and circular buffer.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl configureAcquisition", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::configureAcquisition();
    #endif
    // clang-format on

    resetPvcamStub();

    pvcamCtrl_test app( "campv" );
    app.prepareAcq();

    app.m_nextROI.x     = 1599.5;
    app.m_nextROI.y     = 1599.5;
    app.m_nextROI.w     = 512;
    app.m_nextROI.h     = 512;
    app.m_nextROI.bin_x = 1;
    app.m_nextROI.bin_y = 1;

    SECTION( "default settings" )
    {
        REQUIRE( app.configureAcquisition() == 0 );

        // the end-of-frame callback is re-registered with this app as context
        REQUIRE( g_pvStub.deregisterCalls == 1 );
        REQUIRE( g_pvStub.registerCalls == 1 );
        REQUIRE( g_pvStub.lastCallbackEvent == PL_CALLBACK_EOF );
        REQUIRE( g_pvStub.lastCallback == reinterpret_cast<void *>( &pvcamCtrl::st_endOfFrameCallback ) );
        REQUIRE( g_pvStub.lastContext == static_cast<void *>( static_cast<pvcamCtrl *>( &app ) ) );

        // dynamic_range is readout port 2, 16 bit
        REQUIRE( g_pvStub.lastSet( PARAM_READOUT_PORT ) == 2 );
        REQUIRE( app.m_8bit == false );
        REQUIRE( app.m_readoutSpeedName == "dynamic_range" );

        // ROI
        REQUIRE( g_pvStub.setupCalls == 1 );
        REQUIRE( g_pvStub.lastRgnTotal == 1 );
        REQUIRE( g_pvStub.lastRgn.s1 == 1343 );
        REQUIRE( g_pvStub.lastRgn.s2 == 1854 );
        REQUIRE( g_pvStub.lastRgn.sbin == 1 );
        REQUIRE( g_pvStub.lastRgn.p1 == 1343 );
        REQUIRE( g_pvStub.lastRgn.p2 == 1854 );
        REQUIRE( g_pvStub.lastRgn.pbin == 1 );
        REQUIRE( g_pvStub.lastExpMode == TIMED_MODE );
        REQUIRE( g_pvStub.lastBufferMode == CIRC_OVERWRITE );
        REQUIRE( app.m_width == 512u );
        REQUIRE( app.m_height == 512u );
        REQUIRE( app.m_dataType == _DATATYPE_UINT16 );
        REQUIRE( app.m_currentROI.w == 512 );
        REQUIRE( app.m_currentROI.x == Approx( 1599.5 ) );

        // exposure in microseconds (0.01 s, allowing for float truncation)
        REQUIRE( g_pvStub.lastExposure >= 9999u );
        REQUIRE( g_pvStub.lastExposure <= 10000u );
        REQUIRE( app.m_expTime == Approx( 0.01 ).margin( 2e-6 ) );
        REQUIRE( app.m_expTimeSet == app.m_expTime );

        // exposure-limited frame rate
        REQUIRE( app.m_fps == Approx( 100 ).epsilon( 0.001 ) );
        REQUIRE( app.m_fpsSet == app.m_fps );
        REQUIRE( app.fps() == app.m_fps );

        // circular buffer holds a whole number of frames
        REQUIRE( app.m_circBuffBytes == 1048576u );
        REQUIRE( app.m_circBuff != nullptr );

        // fan already at the requested speed
        REQUIRE( g_pvStub.setCount( PARAM_FAN_SPEED_SETPOINT ) == 0 );
        REQUIRE( app.m_fanSpeedName == "high" );
    }

    SECTION( "readout speeds" )
    {
        const std::vector<std::pair<std::string, int>> speeds = {
            { "sensitivity", 0 }, { "speed", 1 }, { "dynamic_range", 2 }, { "sub_electron", 3 } };

        for( const auto &sp : speeds )
        {
            app.m_readoutSpeedNameSet = sp.first;
            REQUIRE( app.configureAcquisition() == 0 );
            REQUIRE( g_pvStub.lastSet( PARAM_READOUT_PORT ) == sp.second );
            REQUIRE( app.m_readoutSpeedName == sp.first );
            REQUIRE( app.m_8bit == ( sp.first == "speed" ) );
        }
    }

    SECTION( "unknown readout speed falls back to dynamic_range" )
    {
        app.m_readoutSpeedNameSet = "bogus";

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_pvStub.lastSet( PARAM_READOUT_PORT ) == 2 );
        REQUIRE( app.m_readoutSpeedNameSet == "dynamic_range" );
        REQUIRE( app.m_readoutSpeedName == "dynamic_range" );
        REQUIRE( app.m_8bit == false );
    }

    SECTION( "binned ROI" )
    {
        app.m_nextROI.h     = 256;
        app.m_nextROI.bin_x = 2;
        app.m_nextROI.bin_y = 2;

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( g_pvStub.lastRgn.s1 == 1343 );
        REQUIRE( g_pvStub.lastRgn.s2 == 1854 );
        REQUIRE( g_pvStub.lastRgn.sbin == 2 );
        REQUIRE( g_pvStub.lastRgn.p1 == 1471 );
        REQUIRE( g_pvStub.lastRgn.p2 == 1726 );
        REQUIRE( g_pvStub.lastRgn.pbin == 2 );
        REQUIRE( app.m_width == 256u );
        REQUIRE( app.m_height == 128u );
        REQUIRE( app.m_currentROI.bin_x == 2 );
    }

    SECTION( "readout-limited frame rate" )
    {
        g_pvStub.setParam( PARAM_READOUT_TIME, ATTR_CURRENT, 20000 );

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.m_fps == Approx( 50 ).epsilon( 0.001 ) );
    }

    SECTION( "frame rate requested with setFPS" )
    {
        g_pvStub.setParam( PARAM_POST_TRIGGER_DELAY, ATTR_CURRENT, 1000 ); // ns
        app.m_fpsSet    = 20;
        app.m_fpsSetted = true;

        REQUIRE( app.configureAcquisition() == 0 );

        // set up twice, the second time with 1/fps less twice the post-trigger delay
        REQUIRE( g_pvStub.setupCalls == 2 );
        REQUIRE( static_cast<double>( g_pvStub.lastExposure ) == Approx( 49998 ).margin( 2 ) );
        REQUIRE( app.m_expTime == Approx( 0.049998 ).margin( 3e-6 ) );
        REQUIRE( app.m_fpsSetted == false );
        REQUIRE( app.m_fps == Approx( 20 ).epsilon( 0.001 ) );
    }

    SECTION( "fan speed restored if it differs from the request" )
    {
        g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_CURRENT, FAN_SPEED_LOW );
        app.m_fanSpeedNameSet = "high";

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_pvStub.setCount( PARAM_FAN_SPEED_SETPOINT ) == 1 );
        REQUIRE( g_pvStub.lastSet( PARAM_FAN_SPEED_SETPOINT ) == FAN_SPEED_HIGH );
        REQUIRE( app.m_fanSpeedName == "high" );
    }

    SECTION( "fan control disabled" )
    {
        g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_AVAIL, 0 );
        app.m_fanSpeedControlEnabled = false;

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_pvStub.setCount( PARAM_FAN_SPEED_SETPOINT ) == 0 );
    }

    SECTION( "fan speed not available" )
    {
        g_pvStub.setParam( PARAM_FAN_SPEED_SETPOINT, ATTR_AVAIL, 0 );

        REQUIRE( app.configureAcquisition() == -1 );
    }

    SECTION( "deregister failure is not fatal" )
    {
        g_pvStub.deregisterReturn = false;

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_pvStub.registerCalls == 1 );
    }

    SECTION( "register failure" )
    {
        g_pvStub.registerReturn = false;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( g_pvStub.setCount( PARAM_READOUT_PORT ) == 0 );
        REQUIRE( g_pvStub.setupCalls == 0 );
    }

    SECTION( "readout port failure" )
    {
        g_pvStub.setFails.insert( PARAM_READOUT_PORT );

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( g_pvStub.setupCalls == 0 );
    }

    SECTION( "setup failure" )
    {
        g_pvStub.setupReturn = false;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.m_shutdown != 0 );
        REQUIRE( app.m_circBuff == nullptr );
    }
}

/// Verify the acquisition hooks: start, frame-ready wait, image copy, end-of-frame callback, and reconfig.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl acquisition hooks", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::startAcquisition();
    pvcamCtrl::acquireAndCheckValid();
    pvcamCtrl::loadImageIntoStream( nullptr );
    pvcamCtrl::st_endOfFrameCallback( nullptr, nullptr );
    pvcamCtrl::endOfFrameCallback( nullptr );
    pvcamCtrl::reconfig();
    #endif
    // clang-format on

    resetPvcamStub();

    pvcamCtrl_test app( "campv" );
    app.m_handle = 0;

    SECTION( "startAcquisition" )
    {
        app.m_circBuff      = new uns8[16];
        app.m_circBuffBytes = 16;

        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( g_pvStub.startCalls == 1 );
        REQUIRE( g_pvStub.startBuffer == static_cast<void *>( app.m_circBuff ) );
        REQUIRE( g_pvStub.startSize == 16u );

        g_pvStub.startReturn = false;
        REQUIRE( app.startAcquisition() == -1 );
    }

    SECTION( "acquireAndCheckValid" )
    {
        // no frame ready
        REQUIRE( app.acquireAndCheckValid() == 1 );

        // frame ready
        app.m_currImageTimestamp = { 0, 0 };
        sem_post( &app.m_frSemaphore );
        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( app.m_currImageTimestamp.tv_sec > 0 );

        // and consumed
        REQUIRE( app.acquireAndCheckValid() == 1 );
    }

    SECTION( "loadImageIntoStream 16 bit" )
    {
        app.m_width  = 4;
        app.m_height = 2;
        app.m_8bit   = false;

        std::vector<uint16_t> src = { 1, 2, 300, 4000, 5, 60000, 7, 8 };
        g_pvStub.frame.resize( src.size() * sizeof( uint16_t ) );
        memcpy( g_pvStub.frame.data(), src.data(), g_pvStub.frame.size() );

        std::vector<uint16_t> dest( src.size(), 0 );
        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
        REQUIRE( dest == src );

        // the callback is released
        int semval = -1;
        sem_getvalue( &app.m_frDoneSemaphore, &semval );
        REQUIRE( semval == 1 );
    }

    SECTION( "loadImageIntoStream 8 bit" )
    {
        app.m_width  = 3;
        app.m_height = 2;
        app.m_8bit   = true;

        g_pvStub.frame = { 1, 2, 3, 200, 254, 255 };

        std::vector<uint16_t> dest( 6, 0 );
        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
        REQUIRE( dest == std::vector<uint16_t>( { 1, 2, 3, 200, 254, 255 } ) );
    }

    SECTION( "end-of-frame callback hands the frame to the framegrabber" )
    {
        // stale frame-done signals which the callback must clear
        sem_post( &app.m_frDoneSemaphore );
        sem_post( &app.m_frDoneSemaphore );

        FRAME_INFO fi{};
        fi.FrameNr   = 42;
        fi.TimeStamp = 1234;

        void *ctx = static_cast<void *>( static_cast<pvcamCtrl *>( &app ) );

        std::thread cbThread( [&fi, ctx]() { pvcamCtrl::st_endOfFrameCallback( &fi, ctx ); } );

        // wait for the frame-ready signal
        int rv = 1;
        for( int n = 0; n < 400000 && rv != 0; ++n )
        {
            rv = app.acquireAndCheckValid();
        }

        REQUIRE( rv == 0 );

        // release the callback
        sem_post( &app.m_frDoneSemaphore );
        cbThread.join();

        REQUIRE( app.m_frameInfo.FrameNr == 42 );
        REQUIRE( app.m_frameInfo.TimeStamp == 1234 );

        int semval = -1;
        sem_getvalue( &app.m_frDoneSemaphore, &semval );
        REQUIRE( semval == 0 );
    }

    SECTION( "reconfig stops acquisition" )
    {
        REQUIRE( app.reconfig() == 0 );
        REQUIRE( g_pvStub.stopCalls == 1 );
        REQUIRE( g_pvStub.lastStopState == CCS_HALT );

        g_pvStub.stopReturn = false;
        REQUIRE( app.reconfig() == 0 );
        REQUIRE( g_pvStub.stopCalls == 2 );
    }
}

/// Verify the stdCamera INDI callbacks reach the pvcamCtrl hooks.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl INDI callbacks", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    dev::stdCamera<pvcamCtrl>::newCallBack_stdCamera( std::declval<const pcf::IndiProperty &>() );
    dev::stdCamera<pvcamCtrl>::newCallBack_fanSpeed( std::declval<const pcf::IndiProperty &>() );
    dev::stdCamera<pvcamCtrl>::newCallBack_readoutSpeed( std::declval<const pcf::IndiProperty &>() );
    dev::stdCamera<pvcamCtrl>::newCallBack_exptime( std::declval<const pcf::IndiProperty &>() );
    dev::stdCamera<pvcamCtrl>::newCallBack_fps( std::declval<const pcf::IndiProperty &>() );
    pvcamCtrl::setFanSpeed();
    pvcamCtrl::setReadoutSpeed();
    pvcamCtrl::setExpTime();
    pvcamCtrl::setFPS();
    #endif
    // clang-format on

    resetPvcamStub();

    pvcamCtrl_test app( "campv" );
    app.prepareAcq();

    SECTION( "wrong device" )
    {
        pcf::IndiProperty ip = switchProp( "other", "fan_speed", "low", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_stdCamera( ip ) == -1 );
        REQUIRE( g_pvStub.setCalls.empty() );
    }

    SECTION( "unknown property" )
    {
        pcf::IndiProperty ip = switchProp( "campv", "emgain", "target", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_stdCamera( ip ) == -1 );
    }

    SECTION( "fan_speed" )
    {
        pcf::IndiProperty ip = switchProp( "campv", "fan_speed", "medium", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_stdCamera( ip ) == 0 );

        REQUIRE( app.m_fanSpeedNameSet == "medium" );
        REQUIRE( g_pvStub.lastSet( PARAM_FAN_SPEED_SETPOINT ) == FAN_SPEED_MEDIUM );
        REQUIRE( app.m_fanSpeedName == "medium" );
    }

    SECTION( "fan_speed with two speeds selected is rejected" )
    {
        pcf::IndiProperty ip = switchProp( "campv", "fan_speed", "low", pcf::IndiElement::On );
        ip.add( pcf::IndiElement( "off", pcf::IndiElement::On ) );

        REQUIRE( app.newCallBack_stdCamera( ip ) == -1 );
        REQUIRE( g_pvStub.setCalls.empty() );
    }

    SECTION( "fan_speed is ignored when fan control is disabled" )
    {
        app.m_fanSpeedControlEnabled = false;

        pcf::IndiProperty ip = switchProp( "campv", "fan_speed", "low", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_stdCamera( ip ) == -1 );
        REQUIRE( g_pvStub.setCalls.empty() );
    }

    SECTION( "readout_speed" )
    {
        app.m_reconfig = false;

        pcf::IndiProperty ip = switchProp( "campv", "readout_speed", "speed", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_stdCamera( ip ) == 0 );

        REQUIRE( app.m_readoutSpeedNameSet == "speed" );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "exptime" )
    {
        app.m_reconfig = false;

        pcf::IndiProperty ip = numberProp( "campv", "exptime", "target", 2.0 );
        REQUIRE( app.newCallBack_stdCamera( ip ) == 0 );

        REQUIRE( app.m_expTimeSet == Approx( 2.0 ) );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "fps" )
    {
        app.m_reconfig = false;

        pcf::IndiProperty ip = numberProp( "campv", "fps", "target", 20 );
        REQUIRE( app.newCallBack_stdCamera( ip ) == 0 );

        REQUIRE( app.m_fpsSet == Approx( 20 ) );
        REQUIRE( app.m_expTimeSet == Approx( 0.05 ) );
        REQUIRE( app.m_fpsSetted == true );
        REQUIRE( app.m_reconfig == true );
    }
}

/// Verify appLogic waits out the power-on delay, appShutdown closes the camera, and the telemetry hooks.
/**
 * \ingroup pvcamCtrl_unit_test
 */
TEST_CASE( "pvcamCtrl power-on wait, shutdown, and telemetry", "[pvcamCtrl]" )
{
    // clang-format off
    #ifdef PVCAMCTRL_TEST_DOXYGEN_REF
    pvcamCtrl::appLogic();
    pvcamCtrl::appShutdown();
    pvcamCtrl::checkRecordTimes();
    pvcamCtrl::recordTelem( static_cast<const MagAOX::logger::telem_stdcam *>( nullptr ) );
    pvcamCtrl::recordTelem( static_cast<const MagAOX::logger::telem_fgtimings *>( nullptr ) );
    #endif
    // clang-format on

    resetPvcamStub();

    SECTION( "appLogic during the power-on wait" )
    {
        pvcamCtrl_test app( "campv" );
        app.state( stateCodes::POWERON );
        app.m_powerOnCounter = 0;

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::POWERON );
        REQUIRE( g_pvStub.initCalls == 0 );
        REQUIRE( g_pvStub.getCalls == 0 );
    }

    SECTION( "appShutdown closes an open camera" )
    {
        pvcamCtrl_test app( "campv" );
        app.m_handle = 3;

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_pvStub.closedHandles == std::vector<int16>( { 3 } ) );
        REQUIRE( app.m_handle == -1 );
        REQUIRE( g_pvStub.uninitCalls == 1 );
    }

    SECTION( "appShutdown with no camera open" )
    {
        g_pvStub.uninitReturn = false;

        pvcamCtrl_test app( "campv" );

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_pvStub.closedHandles.empty() );
        REQUIRE( g_pvStub.uninitCalls == 1 );
    }

    SECTION( "telemetry" )
    {
        pvcamCtrl_test app( "campv" );

        REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_stdcam *>( nullptr ) ) == 0 );
        REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_fgtimings *>( nullptr ) ) == 0 );
        REQUIRE( app.checkRecordTimes() == 0 );
    }
}

} // namespace pvcamCtrlTest

} // namespace libXWCTest
