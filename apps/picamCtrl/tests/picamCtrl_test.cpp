/** \file picamCtrl_test.cpp
 * \brief Catch2 tests for the picamCtrl app.
 * \author Claude Code
 *
 * \ingroup picamCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Exposes the protected state of the app, of the dev:: base classes, of MagAOXApp (power state), and of the
// telemetry logManager (so the telemetry thread can be marked as running without starting it).
#define protected public
#include "../picamCtrl.hpp"
#undef protected

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace
{

/// Storage whose address is the fake camera device handle.
int g_picamCameraToken{ 0 };

/// Storage whose address is the fake camera model handle.
int g_picamModelToken{ 0 };

/// The fake camera device handle.
PicamHandle cameraHandle()
{
    return &g_picamCameraToken;
}

/// The fake camera model handle.
PicamHandle modelHandle()
{
    return &g_picamModelToken;
}

/// The kinds of parameter set calls recorded by the stub.
enum picamSetKind
{
    setKindInt,       ///< Picam_SetParameterIntegerValue
    setKindFlt,       ///< Picam_SetParameterFloatingPointValue
    setKindLarge,     ///< Picam_SetParameterLargeIntegerValue
    setKindIntOnline, ///< Picam_SetParameterIntegerValueOnline
    setKindFltOnline  ///< Picam_SetParameterFloatingPointValueOnline
};

/// One recorded parameter set call.
struct picamSetCall
{
    PicamHandle handle{ nullptr }; ///< The handle the parameter was set on.

    PicamParameter parameter{ PicamParameter_ExposureTime }; ///< The parameter.

    double value{ 0 }; ///< The value, converted to double.

    picamSetKind kind{ setKindInt }; ///< Which set function was called.
};

/// Fake PICam SDK state shared by the stub functions below.
struct picamStubState
{
    std::vector<PicamCameraID> cameras; ///< Cameras returned by Picam_GetAvailableCameraIDs.

    int initCalls{ 0 }; ///< Number of Picam_InitializeLibrary calls.

    int uninitCalls{ 0 }; ///< Number of Picam_UninitializeLibrary calls.

    int getIDsCalls{ 0 }; ///< Number of Picam_GetAvailableCameraIDs calls.

    int destroyIDsCalls{ 0 }; ///< Number of Picam_DestroyCameraIDs calls.

    int closeCalls{ 0 }; ///< Number of Picam_CloseCamera calls.

    PicamHandle lastClosed{ nullptr }; ///< Handle passed to the last Picam_CloseCamera.

    PicamError openReturn{ PicamError_None }; ///< Value returned by PicamAdvanced_OpenCameraDevice.

    int openCalls{ 0 }; ///< Number of PicamAdvanced_OpenCameraDevice calls.

    std::string lastOpenSerial; ///< Serial number of the camera passed to the last open.

    PicamError modelReturn{ PicamError_None }; ///< Value returned by PicamAdvanced_GetCameraModel.

    std::map<int, piint> intValues; ///< Integer parameter values, by parameter.

    std::map<int, piflt> fltValues; ///< Floating point parameter values, by parameter.

    std::map<int, pi64s> largeValues; ///< Large integer parameter values, by parameter.

    std::map<int, PicamError> getErrors; ///< Forced errors for the Get*Value functions, by parameter.

    std::map<int, PicamError> setErrors; ///< Forced errors for the offline Set*Value functions, by parameter.

    std::map<int, PicamError> onlineErrors; ///< Forced errors for the Set*ValueOnline functions, by parameter.

    std::vector<picamSetCall> setCalls; ///< Every parameter set call, in order.

    PicamError commitReturn{ PicamError_None }; ///< Value returned by Picam_CommitParameters.

    std::vector<PicamParameter> failedParameters; ///< Parameters reported as failed by Picam_CommitParameters.

    int commitCalls{ 0 }; ///< Number of Picam_CommitParameters calls.

    int destroyParametersCalls{ 0 }; ///< Number of Picam_DestroyParameters calls.

    std::map<int, pibln> exists; ///< Values returned by Picam_DoesParameterExist, by parameter (default false).

    std::map<int, PicamError> existsErrors; ///< Forced errors for Picam_DoesParameterExist, by parameter.

    std::map<int, pibln> readable; ///< Values returned by Picam_CanReadParameter, by parameter (default false).

    std::map<int, PicamError> readableErrors; ///< Forced errors for Picam_CanReadParameter, by parameter.

    pibln running{ false }; ///< Value returned by Picam_IsAcquisitionRunning when runningSequence is empty.

    std::deque<pibln> runningSequence; ///< Values returned by successive Picam_IsAcquisitionRunning calls.

    PicamError runningReturn{ PicamError_None }; ///< Value returned by Picam_IsAcquisitionRunning.

    int runningCalls{ 0 }; ///< Number of Picam_IsAcquisitionRunning calls.

    PicamRoi roi{ 0, 1024, 1, 0, 1024, 1 }; ///< The camera ROI, returned by Picam_GetParameterRoisValue.

    PicamRois rois{ nullptr, 0 }; ///< The ROI set returned by Picam_GetParameterRoisValue.

    PicamRoi lastSetRoi{ 0, 0, 0, 0, 0, 0 }; ///< The ROI passed to the last Picam_SetParameterRoisValue.

    int setRoisCalls{ 0 }; ///< Number of Picam_SetParameterRoisValue calls.

    PicamError setRoisReturn{ PicamError_None }; ///< Value returned by Picam_SetParameterRoisValue.

    PicamError getRoisReturn{ PicamError_None }; ///< Value returned by Picam_GetParameterRoisValue.

    int destroyRoisCalls{ 0 }; ///< Number of Picam_DestroyRois calls.

    std::vector<PicamRangeConstraint> constraints; ///< Constraints returned by the range-constraint query.

    int startCalls{ 0 }; ///< Number of Picam_StartAcquisition calls.

    PicamError startReturn{ PicamError_None }; ///< Value returned by Picam_StartAcquisition.

    int stopCalls{ 0 }; ///< Number of Picam_StopAcquisition calls.

    PicamError stopReturn{ PicamError_None }; ///< Value returned by Picam_StopAcquisition.

    PicamError waitReturn{ PicamError_None }; ///< Value returned by Picam_WaitForAcquisitionUpdate.

    PicamAvailableData waitData{ nullptr, 0 }; ///< Data reported by Picam_WaitForAcquisitionUpdate.

    int waitCalls{ 0 }; ///< Number of Picam_WaitForAcquisitionUpdate calls.

    piint lastWaitTimeout{ 0 }; ///< Timeout passed to the last Picam_WaitForAcquisitionUpdate.

    int setBufferCalls{ 0 }; ///< Number of PicamAdvanced_SetAcquisitionBuffer calls.

    const PicamAcquisitionBuffer *lastBuffer{ nullptr }; ///< Buffer passed to the last set-buffer call.

    PicamError setBufferReturn{ PicamError_None }; ///< Value returned by PicamAdvanced_SetAcquisitionBuffer.

    int stringCalls{ 0 }; ///< Number of Picam_GetEnumerationString calls.

    int destroyStringCalls{ 0 }; ///< Number of Picam_DestroyString calls.

    /// Get the last set call for a parameter, or nullptr if it was never set.
    const picamSetCall *lastSet( PicamParameter p /**< [in] the parameter to look up */ ) const
    {
        for( auto it = setCalls.rbegin(); it != setCalls.rend(); ++it )
        {
            if( it->parameter == p )
            {
                return &( *it );
            }
        }

        return nullptr;
    }

    /// Get the number of set calls for a parameter.
    int setCount( PicamParameter p /**< [in] the parameter to count */ ) const
    {
        int n = 0;
        for( const auto &c : setCalls )
        {
            if( c.parameter == p )
            {
                ++n;
            }
        }

        return n;
    }
};

/// The global fake PICam SDK state.
picamStubState g_picamStub;

/// Reset the fake PICam SDK state before a test.
void resetPicamStub()
{
    g_picamStub = picamStubState();
}

/// Build a camera ID.
PicamCameraID makeCameraID( const std::string &serial, /**< [in] the serial number */
                            const std::string &sensor /**< [in] the sensor name */ )
{
    PicamCameraID id;
    memset( &id, 0, sizeof( id ) );
    id.model              = PicamModel_ProEMHS1024BExcelon;
    id.computer_interface = PicamComputerInterface_GigabitEthernet;
    strncpy( id.sensor_name, sensor.c_str(), sizeof( id.sensor_name ) - 1 );
    strncpy( id.serial_number, serial.c_str(), sizeof( id.serial_number ) - 1 );
    return id;
}

/// Record a set call and apply it unless an error is forced.
PicamError recordSet( std::map<int, PicamError> &errors,    /**< [in] the forced errors to check */
                      PicamHandle                handle,    /**< [in] the handle */
                      PicamParameter             parameter, /**< [in] the parameter */
                      double                     value,     /**< [in] the value */
                      picamSetKind               kind /**< [in] which set function was called */ )
{
    picamSetCall c;
    c.handle    = handle;
    c.parameter = parameter;
    c.value     = value;
    c.kind      = kind;
    g_picamStub.setCalls.push_back( c );

    if( errors.count( parameter ) > 0 )
    {
        return errors[parameter];
    }

    return PicamError_None;
}

} // namespace

PicamError Picam_InitializeLibrary()
{
    ++g_picamStub.initCalls;
    return PicamError_None;
}

PicamError Picam_UninitializeLibrary()
{
    ++g_picamStub.uninitCalls;
    return PicamError_None;
}

PicamError Picam_GetEnumerationString( PicamEnumeratedType type, piint value, const pichar **s )
{
    ++g_picamStub.stringCalls;
    std::string str = "E" + std::to_string( static_cast<int>( type ) ) + "_" + std::to_string( value );
    *s              = strdup( str.c_str() );
    return PicamError_None;
}

PicamError Picam_DestroyString( const pichar *s )
{
    ++g_picamStub.destroyStringCalls;
    free( const_cast<pichar *>( s ) );
    return PicamError_None;
}

PicamError Picam_GetAvailableCameraIDs( const PicamCameraID **id_array, piint *id_count )
{
    ++g_picamStub.getIDsCalls;
    *id_array = g_picamStub.cameras.empty() ? nullptr : g_picamStub.cameras.data();
    *id_count = static_cast<piint>( g_picamStub.cameras.size() );
    return PicamError_None;
}

PicamError Picam_DestroyCameraIDs( const PicamCameraID *id_array )
{
    static_cast<void>( id_array );
    ++g_picamStub.destroyIDsCalls;
    return PicamError_None;
}

PicamError Picam_CloseCamera( PicamHandle camera )
{
    ++g_picamStub.closeCalls;
    g_picamStub.lastClosed = camera;
    return PicamError_None;
}

PicamError Picam_GetParameterIntegerValue( PicamHandle camera, PicamParameter parameter, piint *value )
{
    static_cast<void>( camera );
    if( g_picamStub.getErrors.count( parameter ) > 0 )
    {
        return g_picamStub.getErrors[parameter];
    }

    *value = g_picamStub.intValues[parameter];
    return PicamError_None;
}

PicamError Picam_SetParameterIntegerValue( PicamHandle camera, PicamParameter parameter, piint value )
{
    PicamError err = recordSet( g_picamStub.setErrors, camera, parameter, value, setKindInt );
    if( err == PicamError_None )
    {
        g_picamStub.intValues[parameter] = value;
    }
    return err;
}

PicamError Picam_GetParameterLargeIntegerValue( PicamHandle camera, PicamParameter parameter, pi64s *value )
{
    static_cast<void>( camera );
    if( g_picamStub.getErrors.count( parameter ) > 0 )
    {
        return g_picamStub.getErrors[parameter];
    }

    *value = g_picamStub.largeValues[parameter];
    return PicamError_None;
}

PicamError Picam_SetParameterLargeIntegerValue( PicamHandle camera, PicamParameter parameter, pi64s value )
{
    PicamError err = recordSet( g_picamStub.setErrors, camera, parameter, static_cast<double>( value ), setKindLarge );
    if( err == PicamError_None )
    {
        g_picamStub.largeValues[parameter] = value;
    }
    return err;
}

PicamError Picam_GetParameterFloatingPointValue( PicamHandle camera, PicamParameter parameter, piflt *value )
{
    static_cast<void>( camera );
    if( g_picamStub.getErrors.count( parameter ) > 0 )
    {
        return g_picamStub.getErrors[parameter];
    }

    *value = g_picamStub.fltValues[parameter];
    return PicamError_None;
}

PicamError Picam_SetParameterFloatingPointValue( PicamHandle camera, PicamParameter parameter, piflt value )
{
    PicamError err = recordSet( g_picamStub.setErrors, camera, parameter, value, setKindFlt );
    if( err == PicamError_None )
    {
        g_picamStub.fltValues[parameter] = value;
    }
    return err;
}

PicamError Picam_SetParameterIntegerValueOnline( PicamHandle camera, PicamParameter parameter, piint value )
{
    PicamError err = recordSet( g_picamStub.onlineErrors, camera, parameter, value, setKindIntOnline );
    if( err == PicamError_None )
    {
        g_picamStub.intValues[parameter] = value;
    }
    return err;
}

PicamError Picam_SetParameterFloatingPointValueOnline( PicamHandle camera, PicamParameter parameter, piflt value )
{
    PicamError err = recordSet( g_picamStub.onlineErrors, camera, parameter, value, setKindFltOnline );
    if( err == PicamError_None )
    {
        g_picamStub.fltValues[parameter] = value;
    }
    return err;
}

PicamError Picam_GetParameterRoisValue( PicamHandle camera, PicamParameter parameter, const PicamRois **value )
{
    static_cast<void>( camera );
    static_cast<void>( parameter );

    if( g_picamStub.getRoisReturn != PicamError_None )
    {
        return g_picamStub.getRoisReturn;
    }

    g_picamStub.rois.roi_array = &g_picamStub.roi;
    g_picamStub.rois.roi_count = 1;
    *value                     = &g_picamStub.rois;
    return PicamError_None;
}

PicamError Picam_SetParameterRoisValue( PicamHandle camera, PicamParameter parameter, const PicamRois *value )
{
    static_cast<void>( camera );
    static_cast<void>( parameter );

    ++g_picamStub.setRoisCalls;
    g_picamStub.lastSetRoi = value->roi_array[0];

    if( g_picamStub.setRoisReturn != PicamError_None )
    {
        return g_picamStub.setRoisReturn;
    }

    g_picamStub.roi = value->roi_array[0];
    return PicamError_None;
}

PicamError Picam_DestroyRois( const PicamRois *rois )
{
    static_cast<void>( rois );
    ++g_picamStub.destroyRoisCalls;
    return PicamError_None;
}

PicamError Picam_DoesParameterExist( PicamHandle camera, PicamParameter parameter, pibln *exists )
{
    static_cast<void>( camera );
    if( g_picamStub.existsErrors.count( parameter ) > 0 )
    {
        return g_picamStub.existsErrors[parameter];
    }

    *exists = g_picamStub.exists[parameter];
    return PicamError_None;
}

PicamError Picam_CanReadParameter( PicamHandle camera, PicamParameter parameter, pibln *readable )
{
    static_cast<void>( camera );
    if( g_picamStub.readableErrors.count( parameter ) > 0 )
    {
        return g_picamStub.readableErrors[parameter];
    }

    *readable = g_picamStub.readable[parameter];
    return PicamError_None;
}

PicamError Picam_CommitParameters( PicamHandle            camera,
                                   const PicamParameter **failed_parameters_array,
                                   piint                 *failed_parameters_count )
{
    static_cast<void>( camera );
    ++g_picamStub.commitCalls;

    if( g_picamStub.commitReturn != PicamError_None )
    {
        return g_picamStub.commitReturn;
    }

    *failed_parameters_array = g_picamStub.failedParameters.empty() ? nullptr : g_picamStub.failedParameters.data();
    *failed_parameters_count = static_cast<piint>( g_picamStub.failedParameters.size() );
    return PicamError_None;
}

PicamError Picam_DestroyParameters( const PicamParameter *parameter_array )
{
    static_cast<void>( parameter_array );
    ++g_picamStub.destroyParametersCalls;
    return PicamError_None;
}

PicamError Picam_IsAcquisitionRunning( PicamHandle camera, pibln *running )
{
    static_cast<void>( camera );
    ++g_picamStub.runningCalls;

    if( g_picamStub.runningReturn != PicamError_None )
    {
        return g_picamStub.runningReturn;
    }

    if( !g_picamStub.runningSequence.empty() )
    {
        *running = g_picamStub.runningSequence.front();
        g_picamStub.runningSequence.pop_front();
    }
    else
    {
        *running = g_picamStub.running;
    }

    return PicamError_None;
}

PicamError Picam_StartAcquisition( PicamHandle camera )
{
    static_cast<void>( camera );
    ++g_picamStub.startCalls;
    return g_picamStub.startReturn;
}

PicamError Picam_StopAcquisition( PicamHandle camera )
{
    static_cast<void>( camera );
    ++g_picamStub.stopCalls;
    return g_picamStub.stopReturn;
}

PicamError Picam_WaitForAcquisitionUpdate( PicamHandle             camera,
                                           piint                   readout_time_out,
                                           PicamAvailableData     *available,
                                           PicamAcquisitionStatus *status )
{
    static_cast<void>( camera );
    ++g_picamStub.waitCalls;
    g_picamStub.lastWaitTimeout = readout_time_out;

    *available           = g_picamStub.waitData;
    status->running      = true;
    status->errors       = PicamAcquisitionErrorsMask_None;
    status->readout_rate = 0;

    return g_picamStub.waitReturn;
}

PicamError PicamAdvanced_OpenCameraDevice( const PicamCameraID *id, PicamHandle *device )
{
    ++g_picamStub.openCalls;
    g_picamStub.lastOpenSerial = id->serial_number;

    if( g_picamStub.openReturn != PicamError_None )
    {
        return g_picamStub.openReturn;
    }

    *device = cameraHandle();
    return PicamError_None;
}

PicamError PicamAdvanced_GetCameraModel( PicamHandle device, PicamHandle *model )
{
    static_cast<void>( device );
    *model = modelHandle();
    return g_picamStub.modelReturn;
}

PicamError PicamAdvanced_GetParameterRangeConstraints( PicamHandle                  camera,
                                                       PicamParameter               parameter,
                                                       const PicamRangeConstraint **constraint_array,
                                                       piint                       *constraint_count )
{
    static_cast<void>( camera );
    static_cast<void>( parameter );
    *constraint_array = g_picamStub.constraints.empty() ? nullptr : g_picamStub.constraints.data();
    *constraint_count = static_cast<piint>( g_picamStub.constraints.size() );
    return PicamError_None;
}

PicamError PicamAdvanced_SetAcquisitionBuffer( PicamHandle device, const PicamAcquisitionBuffer *buffer )
{
    static_cast<void>( device );
    ++g_picamStub.setBufferCalls;
    g_picamStub.lastBuffer = buffer;
    return g_picamStub.setBufferReturn;
}

/// \endcond

namespace libXWCTest
{

/** \defgroup picamCtrl_unit_test picamCtrl Unit Tests
 * \brief Unit tests for the picamCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `picamCtrl` unit tests.
/** \ingroup picamCtrl_unit_test
 */
namespace picamCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Directory the test harness points the telemetry logger at.
const char *telemPath = "/tmp/picamCtrl_test_telem";

/// Removes the telemetry directory at program exit (telemetry is written when each app is destroyed).
struct telemCleanup
{
    /// Remove the telemetry directory.
    ~telemCleanup()
    {
        std::error_code ec;
        std::filesystem::remove_all( telemPath, ec );
    }
};

/// The cleanup object.
telemCleanup s_telemCleanup;

/// Test harness for picamCtrl.
class picamCtrl_test : public picamCtrl
{
  public:
    /// Construct a harness with the given device name.
    /** Resets the fake PICam state, points telemetry at /tmp, marks the telemetry thread as running, turns the power
     * on, and initializes the members the picamCtrl constructor leaves unset.
     */
    explicit picamCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        resetPicamStub();
        m_configName = device;
        static_cast<void>( m_tel.logPath( telemPath ) );
        m_tel.m_logThreadRunning = true;
        power( 1 );

        m_tsRes                  = 1;
        m_frameSize              = 0;
        m_FrameRateCalculation   = 0;
        m_ReadOutTimeCalculation = 0;
    }

    /// Run setupConfig(), read the given config file, and run loadConfig().
    void configure( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
        static_cast<void>( m_tel.logPath( telemPath ) );
    }

    /// Set the app power state and target.
    void power( int st /**< [in] the power state and target, 1 for on, 0 for off */ )
    {
        MagAOXApp<true>::m_powerState = st;
        m_powerTargetState            = st;
    }

    /// Set the dssShutter power state (independent of the camera power state).
    void shutterPower( int st /**< [in] the shutter power state, -1 unknown, 0 off, 1 on */ )
    {
        dev::dssShutter<picamCtrl>::m_powerState = st;
    }

    /// Create the INDI properties that appStartup() would create, without starting any threads.
    void startupProperties()
    {
        dev::stdCamera<picamCtrl>::appStartup();

        createStandardIndiNumber<float>(
            m_indiP_receiveExptime, "receiveExptime", m_minExpTime, m_maxExpTime, m_stepExpTime, "%0.3f" );
        createStandardIndiToggleSw( m_indiP_receiveSynchro, "receiveSynchro" );

        m_indiP_fxngensync_freq = pcf::IndiProperty( pcf::IndiProperty::Number );
        m_indiP_fxngensync_freq.setDevice( m_fxngenName );
        m_indiP_fxngensync_freq.setName( m_fxngenCh + "freq" );
        m_indiP_fxngensync_freq.add( pcf::IndiElement( "target" ) );

        m_indiP_fxngensync_output = pcf::IndiProperty( pcf::IndiProperty::Text );
        m_indiP_fxngensync_output.setDevice( m_fxngenName );
        m_indiP_fxngensync_output.setName( m_fxngenCh + "outp" );
        m_indiP_fxngensync_output.add( pcf::IndiElement( "value" ) );

        m_indiP_otherCamExptime = pcf::IndiProperty( pcf::IndiProperty::Number );
        m_indiP_otherCamExptime.setDevice( m_otherCamName );
        m_indiP_otherCamExptime.setName( "receiveExptime" );
        m_indiP_otherCamExptime.add( pcf::IndiElement( "target" ) );

        m_indiP_otherCamSynchro = pcf::IndiProperty( pcf::IndiProperty::Switch );
        m_indiP_otherCamSynchro.setDevice( m_otherCamName );
        m_indiP_otherCamSynchro.setName( "receiveSynchro" );
        m_indiP_otherCamSynchro.add( pcf::IndiElement( "toggle" ) );
    }

    /// Install the fake camera and model handles, as a successful connect() does.
    void openCamera()
    {
        m_cameraHandle = cameraHandle();
        m_modelHandle  = modelHandle();
    }

    /// Prime the fake camera and the app so that configureAcquisition() succeeds with a full-frame ROI.
    void primeConfigure()
    {
        openCamera();

        g_picamStub.intValues[PicamParameter_ReadoutControlMode] = PicamReadoutControlMode_FrameTransfer;
        g_picamStub.intValues[PicamParameter_ReadoutStride]      = 4096;
        g_picamStub.intValues[PicamParameter_FrameStride]        = 4096;
        g_picamStub.intValues[PicamParameter_FramesPerReadout]   = 1;
        g_picamStub.intValues[PicamParameter_FrameSize]          = 4088;
        g_picamStub.intValues[PicamParameter_PixelBitDepth]      = 16;
        g_picamStub.intValues[PicamParameter_AdcEMGain]          = 50;

        g_picamStub.fltValues[PicamParameter_ReadoutTimeCalculation] = 2.5;
        g_picamStub.fltValues[PicamParameter_FrameRateCalculation]   = 100.0;

        g_picamStub.largeValues[PicamParameter_TimeStampResolution] = 1000000;

        PicamRangeConstraint c;
        memset( &c, 0, sizeof( c ) );
        c.minimum               = 0.001;
        c.maximum               = 1000.0;
        c.increment             = 0.0001;
        g_picamStub.constraints = { c };

        m_nextROI.x     = 511.5;
        m_nextROI.y     = 511.5;
        m_nextROI.w     = 1024;
        m_nextROI.h     = 1024;
        m_nextROI.bin_x = 1;
        m_nextROI.bin_y = 1;

        m_readoutSpeedNameSet = "emccd_05MHz";
        m_vShiftSpeedNameSet  = "1_2us";
        m_fanSpeedNameSet     = "on";
        m_ccdTempSetpt        = -55;
        m_expTimeSet          = 0.01;
        m_synchroSet          = false;
    }
};

/// Build a switch property with the given elements.
pcf::IndiProperty switchProp( const std::string                               &device, /**< [in] the property device */
                              const std::string                               &name,   /**< [in] the property name */
                              const std::vector<std::pair<std::string, bool>> &els /**< [in] elements and states */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    for( const auto &e : els )
    {
        ip.add( pcf::IndiElement( e.first, e.second ? pcf::IndiElement::On : pcf::IndiElement::Off ) );
    }

    return ip;
}

/// Build a number property with a single element.
pcf::IndiProperty numberProp( const std::string &device, /**< [in] the property device */
                              const std::string &name,   /**< [in] the property name */
                              const std::string &el,     /**< [in] the element name */
                              double             value /**< [in] the element value */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, value ) );
    return ip;
}

/// \endcond

/// Verify picamCtrl construction defaults and the stdCamera/frameGrabber flags.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl construction defaults", "[picamCtrl]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl();
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );

    REQUIRE( app.m_powerMgtEnabled == true );
    REQUIRE( app.m_acqBuff.memory == nullptr );
    REQUIRE( app.m_acqBuff.memory_size == 0 );
    REQUIRE( app.m_cameraHandle == nullptr );
    REQUIRE( app.m_modelHandle == nullptr );
    REQUIRE( app.m_timeStampMask == PicamTimeStampsMask_ExposureStarted );
    REQUIRE( app.m_fxngenName == "fxngensync" );
    REQUIRE( app.m_fxngenCh == "C2" );
    REQUIRE( app.m_otherCamName.empty() );

    REQUIRE( app.m_defaultReadoutSpeed == "emccd_05MHz" );
    REQUIRE( app.m_readoutSpeedNames ==
             std::vector<std::string>{
                 "ccd_00_1MHz", "ccd_01MHz", "emccd_05MHz", "emccd_10MHz", "emccd_20MHz", "emccd_30MHz" } );
    REQUIRE( app.m_readoutSpeedNameLabels.size() == app.m_readoutSpeedNames.size() );

    REQUIRE( app.m_defaultVShiftSpeed == "1_2us" );
    REQUIRE( app.m_vShiftSpeedNames == std::vector<std::string>{ "0_7us", "1_2us", "2_0us", "5_0us" } );
    REQUIRE( app.m_vShiftSpeedNameLabels.size() == app.m_vShiftSpeedNames.size() );

    REQUIRE( app.m_defaultFanSpeed == "on" );
    REQUIRE( app.m_fanSpeedNames == std::vector<std::string>{ "on", "off" } );
    REQUIRE( app.m_fanSpeedNameLabels == std::vector<std::string>{ "On", "Off" } );
    REQUIRE( app.m_fanSpeedName == "on" );
    REQUIRE( app.m_fanSpeedNameSet == "on" );
    REQUIRE( app.m_fanSpeedControlEnabled == true );
    REQUIRE( app.m_fanControlSupported == false );
    REQUIRE( app.m_fanStatusSupported == false );
    REQUIRE( app.m_fanForcedOn == false );
    REQUIRE( app.m_fanSpeedLogPending == false );

    REQUIRE( app.m_full_x == Approx( 511.5 ) );
    REQUIRE( app.m_full_y == Approx( 511.5 ) );
    REQUIRE( app.m_full_w == 1024 );
    REQUIRE( app.m_full_h == 1024 );
    REQUIRE( app.m_maxEMGain == Approx( 1000 ) );

    REQUIRE( picamCtrl::c_stdCamera_tempControl == true );
    REQUIRE( picamCtrl::c_stdCamera_readoutSpeed == true );
    REQUIRE( picamCtrl::c_stdCamera_vShiftSpeed == true );
    REQUIRE( picamCtrl::c_stdCamera_fanSpeed == true );
    REQUIRE( picamCtrl::c_stdCamera_emGain == true );
    REQUIRE( picamCtrl::c_stdCamera_exptimeCtrl == true );
    REQUIRE( picamCtrl::c_stdCamera_fpsCtrl == false );
    REQUIRE( picamCtrl::c_stdCamera_usesModes == false );
    REQUIRE( picamCtrl::c_stdCamera_usesROI == true );
    REQUIRE( picamCtrl::c_stdCamera_hasShutter == true );
    REQUIRE( picamCtrl::c_stdCamera_blacklevel == false );
    REQUIRE( picamCtrl::c_frameGrabber_flippable == true );
}

/// Verify the readout-speed and vertical-shift-speed name parsers.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl readoutParams and vshiftParams", "[picamCtrl]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    readoutParams(adcQual, adcSpeed, "");
    vshiftParams(vss, "");
    #endif
    // clang-format on

    SECTION( "readout speeds" )
    {
        auto [name, qual, speed] = GENERATE(
            table<std::string, int, double>( { { "ccd_00_1MHz", PicamAdcQuality_LowNoise, 0.1 },
                                               { "ccd_01MHz", PicamAdcQuality_LowNoise, 1.0 },
                                               { "emccd_05MHz", PicamAdcQuality_ElectronMultiplied, 5.0 },
                                               { "emccd_10MHz", PicamAdcQuality_ElectronMultiplied, 10.0 },
                                               { "emccd_20MHz", PicamAdcQuality_ElectronMultiplied, 20.0 },
                                               { "emccd_30MHz", PicamAdcQuality_ElectronMultiplied, 30.0 } } ) );

        piint adcQual  = -1;
        piflt adcSpeed = -1;
        REQUIRE( readoutParams( adcQual, adcSpeed, name ) == 0 );
        REQUIRE( adcQual == qual );
        REQUIRE( adcSpeed == Approx( speed ) );
    }

    SECTION( "invalid readout speed" )
    {
        piint adcQual  = -1;
        piflt adcSpeed = -1;
        REQUIRE( readoutParams( adcQual, adcSpeed, "emccd_40MHz" ) == -1 );
        REQUIRE( adcQual == -1 );
        REQUIRE( adcSpeed == Approx( -1 ) );
    }

    SECTION( "vertical shift speeds" )
    {
        auto [name, vs] = GENERATE(
            table<std::string, double>( { { "0_7us", 0.7 }, { "1_2us", 1.2 }, { "2_0us", 2.0 }, { "5_0us", 5.0 } } ) );

        piflt vss = -1;
        REQUIRE( vshiftParams( vss, name ) == 0 );
        REQUIRE( vss == Approx( vs ) );
    }

    SECTION( "invalid vertical shift speed" )
    {
        piflt vss = -1;
        REQUIRE( vshiftParams( vss, "3_0us" ) == -1 );
        REQUIRE( vss == Approx( -1 ) );
    }
}

/// Verify picamCtrl configuration defaults, overrides, and an invalid default fan speed.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl configuration", "[picamCtrl][config]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::setupConfig();
    picamCtrl::loadConfig();
    #endif
    // clang-format on

    SECTION( "defaults" )
    {
        picamCtrl_test app( "picam" );

        mx::app::writeConfigFile( "/tmp/picamCtrl_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
        app.configure( "/tmp/picamCtrl_test_defaults.conf" );
        std::remove( "/tmp/picamCtrl_test_defaults.conf" );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_serialNumber.empty() );
        REQUIRE( app.m_fxngenName == "fxngensync" );
        REQUIRE( app.m_fxngenCh == "C2" );
        REQUIRE( app.m_otherCamName.empty() );
        REQUIRE( app.m_startupTemp == Approx( -999 ) );
        REQUIRE( app.m_defaultReadoutSpeed == "emccd_05MHz" );
        REQUIRE( app.m_defaultVShiftSpeed == "1_2us" );
        REQUIRE( app.m_fanSpeedControlEnabled == true );
        REQUIRE( app.m_defaultFanSpeed == "on" );
        REQUIRE( app.m_maxEMGain == Approx( 1000 ) );

        // The default ROI falls back to the full frame.
        REQUIRE( app.m_default_x == Approx( 511.5 ) );
        REQUIRE( app.m_default_y == Approx( 511.5 ) );
        REQUIRE( app.m_default_w == 1024 );
        REQUIRE( app.m_default_h == 1024 );
        REQUIRE( app.m_default_bin_x == 1 );
        REQUIRE( app.m_default_bin_y == 1 );
        REQUIRE( app.m_currentROI.x == Approx( 511.5 ) );
        REQUIRE( app.m_nextROI.w == 1024 );

        REQUIRE( app.m_shmimName == "picam" );
        REQUIRE( app.m_defaultFlip == picamCtrl::fgFlipNone );
        REQUIRE( app.m_maxInterval == Approx( 10.0 ) );
    }

    SECTION( "overrides" )
    {
        picamCtrl_test app( "picam" );

        mx::app::writeConfigFile(
            "/tmp/picamCtrl_test_override.conf",
            { "camera", "synchro", "synchro",      "synchro",      "camera",  "camera",   "camera",
              "camera", "camera",  "camera",       "camera",       "camera",  "camera",   "camera",
              "camera", "camera",  "framegrabber", "framegrabber", "shutter", "telemeter" },
            { "serialNumber",        "deviceName",         "channel",         "otherCamName",    "startupTemp",
              "defaultReadoutSpeed", "defaultVShiftSpeed", "fanSpeedControl", "defaultFanSpeed", "maxEMGain",
              "default_x",           "default_y",          "default_w",       "default_h",       "default_bin_x",
              "default_bin_y",       "shmimName",          "defaultFlip",     "powerDevice",     "maxInterval" },
            { "0123456789", "fxngen2", "C1",  "camsci2", "-40", "ccd_01MHz", "0_7us",   "false",  "off",  "500",
              "255.5",      "767.5",   "256", "128",     "2",   "1",         "camtest", "flipUD", "pdu1", "5.0" } );
        app.configure( "/tmp/picamCtrl_test_override.conf" );
        std::remove( "/tmp/picamCtrl_test_override.conf" );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_serialNumber == "0123456789" );
        REQUIRE( app.m_fxngenName == "fxngen2" );
        REQUIRE( app.m_fxngenCh == "C1" );
        REQUIRE( app.m_otherCamName == "camsci2" );
        REQUIRE( app.m_startupTemp == Approx( -40 ) );
        REQUIRE( app.m_defaultReadoutSpeed == "ccd_01MHz" );
        REQUIRE( app.m_defaultVShiftSpeed == "0_7us" );
        REQUIRE( app.m_fanSpeedControlEnabled == false );
        REQUIRE( app.m_defaultFanSpeed == "off" );
        REQUIRE( app.m_maxEMGain == Approx( 500 ) );
        REQUIRE( app.m_default_x == Approx( 255.5 ) );
        REQUIRE( app.m_default_y == Approx( 767.5 ) );
        REQUIRE( app.m_default_w == 256 );
        REQUIRE( app.m_default_h == 128 );
        REQUIRE( app.m_default_bin_x == 2 );
        REQUIRE( app.m_default_bin_y == 1 );
        REQUIRE( app.m_currentROI.x == Approx( 255.5 ) );
        REQUIRE( app.m_nextROI.h == 128 );
        REQUIRE( app.m_shmimName == "camtest" );
        REQUIRE( app.m_defaultFlip == picamCtrl::fgFlipUD );
        REQUIRE( app.m_powerDevice == "pdu1" );
        REQUIRE( app.m_maxInterval == Approx( 5.0 ) );
    }

    SECTION( "an invalid default fan speed stops stdCamera::loadConfig, which picamCtrl ignores" )
    {
        picamCtrl_test app( "picam" );

        mx::app::writeConfigFile( "/tmp/picamCtrl_test_badfan.conf",
                                  { "camera", "camera", "framegrabber" },
                                  { "defaultFanSpeed", "maxEMGain", "shmimName" },
                                  { "medium", "500", "camtest" } );
        app.configure( "/tmp/picamCtrl_test_badfan.conf" );
        std::remove( "/tmp/picamCtrl_test_badfan.conf" );

        REQUIRE( app.m_defaultFanSpeed == "medium" );
        REQUIRE( app.m_maxEMGain == Approx( 1000 ) ); // not reached
        REQUIRE( app.m_default_w == 0 );              // not reached
        REQUIRE( app.m_shmimName == "camtest" );      // the later base classes still load
        REQUIRE( app.m_shutdown == 0 );               // the stdCamera error is not propagated
    }
}

/// Verify powerOnDefaults() resets the camera and fan state.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl powerOnDefaults", "[picamCtrl]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::powerOnDefaults();
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.m_fanForcedOn         = true;
    app.m_fanControlSupported = true;
    app.m_fanStatusSupported  = true;
    app.m_fanSpeedValid       = true;
    app.m_readoutSpeedName    = "ccd_01MHz";
    app.m_vShiftSpeedName     = "5_0us";

    SECTION( "with fan control" )
    {
        app.m_defaultFanSpeed = "off";
        REQUIRE( app.powerOnDefaults() == 0 );
        REQUIRE( app.m_ccdTempSetpt == Approx( -55 ) );
        REQUIRE( app.m_readoutSpeedName == "emccd_05MHz" );
        REQUIRE( app.m_vShiftSpeedName == "1_2us" );
        REQUIRE( app.m_fanSpeedName == "off" );
        REQUIRE( app.m_fanSpeedNameSet == "off" );
        REQUIRE( app.m_fanSpeedLogPending == true );
    }

    SECTION( "without fan control" )
    {
        app.m_fanSpeedControlEnabled = false;
        REQUIRE( app.powerOnDefaults() == 0 );
        REQUIRE( app.m_fanSpeedName.empty() );
        REQUIRE( app.m_fanSpeedNameSet.empty() );
        REQUIRE( app.m_fanSpeedLogPending == false );
    }

    REQUIRE( app.m_fanForcedOn == false );
    REQUIRE( app.m_fanControlSupported == false );
    REQUIRE( app.m_fanStatusSupported == false );
    REQUIRE( app.m_fanSpeedValid == false );
}

/// Verify connect() finds, opens, and probes the configured camera.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl connect", "[picamCtrl][connect]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::connect();
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.m_serialNumber = "12345";
    app.state( stateCodes::NOTCONNECTED );

    SECTION( "no cameras goes to NODEVICE" )
    {
        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( g_picamStub.initCalls == 1 );
        REQUIRE( g_picamStub.uninitCalls == 2 );
        REQUIRE( g_picamStub.destroyIDsCalls == 1 );
        REQUIRE( g_picamStub.openCalls == 0 );
    }

    SECTION( "power off returns before looking at the cameras" )
    {
        g_picamStub.cameras = { makeCameraID( "12345", "ProEM" ) };
        app.power( 0 );
        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
        REQUIRE( g_picamStub.getIDsCalls == 1 );
        REQUIRE( g_picamStub.openCalls == 0 );
    }

    SECTION( "the configured camera is not found" )
    {
        g_picamStub.cameras = { makeCameraID( "999", "Other" ) };
        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( g_picamStub.openCalls == 0 );
        REQUIRE( g_picamStub.destroyIDsCalls == 1 );
    }

    SECTION( "an open failure is an ERROR" )
    {
        g_picamStub.cameras    = { makeCameraID( "12345", "ProEM" ) };
        g_picamStub.openReturn = PicamError_CameraFaulted;
        REQUIRE( app.connect() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_picamStub.openCalls == 1 );
        REQUIRE( g_picamStub.destroyIDsCalls == 1 );
    }

    SECTION( "connect without fan control" )
    {
        app.m_fanSpeedControlEnabled = false;
        app.m_defaultReadoutSpeed    = "ccd_01MHz";
        app.m_defaultVShiftSpeed     = "2_0us";
        app.m_fanSpeedNameSet        = "unchanged";
        g_picamStub.cameras          = { makeCameraID( "999", "Other" ), makeCameraID( "12345", "ProEM" ) };

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( g_picamStub.openCalls == 1 );
        REQUIRE( g_picamStub.lastOpenSerial == "12345" );
        REQUIRE( app.m_cameraHandle == cameraHandle() );
        REQUIRE( app.m_modelHandle == modelHandle() );
        REQUIRE( app.m_cameraName == "ProEM" );
        REQUIRE( app.m_cameraModel == "E" + std::to_string( static_cast<int>( PicamEnumeratedType_Model ) ) + "_" +
                                          std::to_string( static_cast<int>( PicamModel_ProEMHS1024BExcelon ) ) );
        REQUIRE( g_picamStub.destroyStringCalls == g_picamStub.stringCalls );
        REQUIRE( g_picamStub.destroyIDsCalls == 1 );
        REQUIRE( app.m_readoutSpeedNameSet == "ccd_01MHz" );
        REQUIRE( app.m_vShiftSpeedNameSet == "2_0us" );
        REQUIRE( app.m_fanControlSupported == false );
        REQUIRE( app.m_fanStatusSupported == false );
        REQUIRE( app.m_fanSpeedNameSet == "unchanged" );
    }

    SECTION( "connect with fan control and readable fan status" )
    {
        app.m_defaultFanSpeed                                 = "off";
        g_picamStub.cameras                                   = { makeCameraID( "12345", "ProEM" ) };
        g_picamStub.exists[PicamParameter_DisableCoolingFan]  = true;
        g_picamStub.exists[PicamParameter_CoolingFanStatus]   = true;
        g_picamStub.readable[PicamParameter_CoolingFanStatus] = true;

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( app.m_fanControlSupported == true );
        REQUIRE( app.m_fanStatusSupported == true );
        REQUIRE( app.m_fanSpeedNameSet == "off" );
        REQUIRE( app.m_fanSpeedLogPending == true );
    }

    SECTION( "fan status that is not readable falls back to the commanded state" )
    {
        g_picamStub.cameras                                  = { makeCameraID( "12345", "ProEM" ) };
        g_picamStub.exists[PicamParameter_DisableCoolingFan] = true;
        g_picamStub.exists[PicamParameter_CoolingFanStatus]  = true;

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( app.m_fanControlSupported == true );
        REQUIRE( app.m_fanStatusSupported == false );
    }

    SECTION( "no fan status parameter falls back to the commanded state" )
    {
        g_picamStub.cameras                                  = { makeCameraID( "12345", "ProEM" ) };
        g_picamStub.exists[PicamParameter_DisableCoolingFan] = true;

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( app.m_fanControlSupported == true );
        REQUIRE( app.m_fanStatusSupported == false );
    }

    SECTION( "fan control enabled but not supported is an ERROR" )
    {
        g_picamStub.cameras = { makeCameraID( "12345", "ProEM" ) };

        REQUIRE( app.connect() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_picamStub.destroyIDsCalls == 1 );
    }

    SECTION( "errors while probing fan support are an ERROR" )
    {
        g_picamStub.cameras = { makeCameraID( "12345", "ProEM" ) };

        SECTION( "DisableCoolingFan existence" )
        {
            g_picamStub.existsErrors[PicamParameter_DisableCoolingFan] = PicamError_UnexpectedError;
        }

        SECTION( "CoolingFanStatus existence" )
        {
            g_picamStub.exists[PicamParameter_DisableCoolingFan]      = true;
            g_picamStub.existsErrors[PicamParameter_CoolingFanStatus] = PicamError_UnexpectedError;
        }

        SECTION( "CoolingFanStatus readability" )
        {
            g_picamStub.exists[PicamParameter_DisableCoolingFan]        = true;
            g_picamStub.exists[PicamParameter_CoolingFanStatus]         = true;
            g_picamStub.readableErrors[PicamParameter_CoolingFanStatus] = PicamError_UnexpectedError;
        }

        REQUIRE( app.connect() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_picamStub.destroyIDsCalls == 1 );
    }

    SECTION( "an open camera and acquisition buffer are released first" )
    {
        int otherToken               = 0;
        app.m_cameraHandle           = &otherToken;
        app.m_acqBuff.memory         = malloc( 64 );
        app.m_acqBuff.memory_size    = 64;
        app.m_fanSpeedControlEnabled = false;

        REQUIRE( app.connect() == 0 );
        REQUIRE( g_picamStub.closeCalls == 1 );
        REQUIRE( g_picamStub.lastClosed == &otherToken );
        REQUIRE( app.m_cameraHandle == nullptr );
        REQUIRE( app.m_acqBuff.memory == nullptr );
        REQUIRE( app.m_acqBuff.memory_size == 0 );
    }
}

/// Verify getAcquisitionState() maps the acquisition state to the app state.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl getAcquisitionState", "[picamCtrl]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::getAcquisitionState();
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.openCamera();
    app.state( stateCodes::CONNECTED );
    app.m_reconfig = false;

    SECTION( "running is OPERATING" )
    {
        g_picamStub.running = true;
        REQUIRE( app.getAcquisitionState() == 0 );
        REQUIRE( app.state() == stateCodes::OPERATING );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "not running is READY and requests a reconfigure" )
    {
        g_picamStub.running = false;
        REQUIRE( app.getAcquisitionState() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "an SDK error is an ERROR" )
    {
        g_picamStub.runningReturn = PicamError_InvalidHandle;
        REQUIRE( app.getAcquisitionState() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "power off is ignored" )
    {
        g_picamStub.runningReturn = PicamError_InvalidHandle;
        app.power( 0 );
        REQUIRE( app.getAcquisitionState() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
    }
}

/// Verify getTemps() reads the sensor temperature and maps the temperature status.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl getTemps", "[picamCtrl][temp]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::getTemps();
    picamCtrl::getPicamParameter(value, PicamParameter_SensorTemperatureReading);
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.openCamera();
    app.state( stateCodes::READY );
    g_picamStub.fltValues[PicamParameter_SensorTemperatureReading] = -54.5;

    SECTION( "status mapping" )
    {
        auto [status, onStatus, onTarget, str] = GENERATE(
            table<int, bool, bool, std::string>( { { PicamSensorTemperatureStatus_Unlocked, true, false, "UNLOCKED" },
                                                   { PicamSensorTemperatureStatus_Locked, true, true, "LOCKED" },
                                                   { PicamSensorTemperatureStatus_Faulted, false, false, "FAULTED" },
                                                   { 99, false, false, "UNKNOWN" } } ) );

        g_picamStub.intValues[PicamParameter_SensorTemperatureStatus] = status;
        REQUIRE( app.getTemps() == 0 );
        REQUIRE( app.m_ccdTemp == Approx( -54.5 ) );
        REQUIRE( app.m_tempControlStatus == onStatus );
        REQUIRE( app.m_tempControlOnTarget == onTarget );
        REQUIRE( app.m_tempControlStatusStr == str );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "a temperature read error is an ERROR" )
    {
        g_picamStub.getErrors[PicamParameter_SensorTemperatureReading] = PicamError_UnexpectedError;
        REQUIRE( app.getTemps() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "a status read error is an ERROR" )
    {
        g_picamStub.getErrors[PicamParameter_SensorTemperatureStatus] = PicamError_UnexpectedError;
        REQUIRE( app.getTemps() == -1 );
        REQUIRE( app.m_ccdTemp == Approx( -54.5 ) );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "power off fails without changing state" )
    {
        app.power( 0 );
        REQUIRE( app.getTemps() == -1 );
        REQUIRE( app.state() == stateCodes::READY );
    }
}

/// Verify getFanSpeed() maps the PICam cooling-fan status to the stdCamera fan state.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl getFanSpeed", "[picamCtrl][fan]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::getFanSpeed();
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.openCamera();
    app.state( stateCodes::READY );
    app.m_fanStatusSupported = true;

    SECTION( "status mapping" )
    {
        auto [status, name, forced] =
            GENERATE( table<int, std::string, bool>( { { PicamCoolingFanStatus_Off, "off", false },
                                                       { PicamCoolingFanStatus_On, "on", false },
                                                       { PicamCoolingFanStatus_ForcedOn, "on", true } } ) );

        app.m_fanSpeedName                                     = "";
        g_picamStub.intValues[PicamParameter_CoolingFanStatus] = status;
        REQUIRE( app.getFanSpeed() == 0 );
        REQUIRE( app.m_fanSpeedName == name );
        REQUIRE( app.m_fanForcedOn == forced );
        REQUIRE( app.m_fanSpeedValid == true );
    }

    SECTION( "forced on is cleared when the camera reports on" )
    {
        g_picamStub.intValues[PicamParameter_CoolingFanStatus] = PicamCoolingFanStatus_ForcedOn;
        REQUIRE( app.getFanSpeed() == 0 );
        REQUIRE( app.m_fanForcedOn == true );

        g_picamStub.intValues[PicamParameter_CoolingFanStatus] = PicamCoolingFanStatus_On;
        REQUIRE( app.getFanSpeed() == 0 );
        REQUIRE( app.m_fanForcedOn == false );
    }

    SECTION( "unsupported status is not read" )
    {
        app.m_fanStatusSupported                               = false;
        app.m_fanSpeedName                                     = "on";
        g_picamStub.intValues[PicamParameter_CoolingFanStatus] = PicamCoolingFanStatus_Off;
        REQUIRE( app.getFanSpeed() == 0 );
        REQUIRE( app.m_fanSpeedName == "on" );
        REQUIRE( app.m_fanSpeedValid == false );
    }

    SECTION( "an unknown status is an error" )
    {
        app.m_fanSpeedName                                     = "on";
        g_picamStub.intValues[PicamParameter_CoolingFanStatus] = 99;
        REQUIRE( app.getFanSpeed() == -1 );
        REQUIRE( app.m_fanSpeedName == "on" );
        REQUIRE( app.m_fanSpeedValid == false );
    }

    SECTION( "a read error is an ERROR" )
    {
        g_picamStub.getErrors[PicamParameter_CoolingFanStatus] = PicamError_UnexpectedError;
        REQUIRE( app.getFanSpeed() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }
}

/// Verify the deferred stdCamera setters and the trivial framegrabber hooks.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl deferred setters", "[picamCtrl]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::setTempControl();
    picamCtrl::setTempSetPt();
    picamCtrl::setReadoutSpeed();
    picamCtrl::setVShiftSpeed();
    picamCtrl::setFanSpeed();
    picamCtrl::setFPS();
    picamCtrl::checkNextROI();
    picamCtrl::setNextROI();
    picamCtrl::fps();
    picamCtrl::startAcquisition();
    picamCtrl::checkFocus();
    picamCtrl::gotoFocus();
    picamCtrl::setShutter(0);
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.startupProperties();
    app.m_reconfig = false;

    SECTION( "setTempControl is always on" )
    {
        app.m_tempControlStatus    = false;
        app.m_tempControlStatusSet = false;
        REQUIRE( app.setTempControl() == 0 );
        REQUIRE( app.m_tempControlStatus == true );
        REQUIRE( app.m_tempControlStatusSet == true );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "setTempSetPt" )
    {
        REQUIRE( app.setTempSetPt() == 0 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "setReadoutSpeed" )
    {
        REQUIRE( app.setReadoutSpeed() == 0 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "setVShiftSpeed" )
    {
        REQUIRE( app.setVShiftSpeed() == 0 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "setFanSpeed" )
    {
        app.m_fanSpeedLogPending = false;
        REQUIRE( app.setFanSpeed() == 0 );
        REQUIRE( app.m_reconfig == true );
        REQUIRE( app.m_fanSpeedLogPending == true );
        REQUIRE( g_picamStub.setCalls.empty() );
    }

    SECTION( "setNextROI" )
    {
        REQUIRE( app.setNextROI() == 0 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "trivial hooks" )
    {
        app.m_fps = 42.5;
        REQUIRE( app.setFPS() == 0 );
        REQUIRE( app.checkNextROI() == 0 );
        REQUIRE( app.fps() == Approx( 42.5 ) );
        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( app.m_reconfig == false );
        REQUIRE( g_picamStub.setCalls.empty() );
    }

    SECTION( "focus helpers without configuration" )
    {
        REQUIRE( app.checkFocus() == false );
        REQUIRE( app.gotoFocus() == -1 );
    }

    SECTION( "an invalid shutter request is rejected" )
    {
        REQUIRE( app.setShutter( 5 ) == -1 );
    }
}

/// Verify setEMGain() for the EM and conventional amplifiers, including clamping and errors.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl setEMGain", "[picamCtrl][gain]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::setEMGain();
    picamCtrl::setPicamParameterOnline(m_modelHandle, PicamParameter_AdcEMGain, 0);
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.openCamera();
    app.state( stateCodes::OPERATING );
    app.m_readoutSpeedName = "emccd_10MHz";

    SECTION( "EM gain is set online on the model and read back" )
    {
        auto [requested, sent] = GENERATE( table<float, int>( { { 300, 300 }, { -5, 0 }, { 5000, 1000 } } ) );

        app.m_emGainSet = requested;
        REQUIRE( app.setEMGain() == 0 );

        const picamSetCall *c = g_picamStub.lastSet( PicamParameter_AdcEMGain );
        REQUIRE( c != nullptr );
        REQUIRE( c->kind == setKindIntOnline );
        REQUIRE( c->handle == modelHandle() );
        REQUIRE( c->value == Approx( sent ) );
        REQUIRE( app.m_emGain == Approx( sent ) );
        REQUIRE( app.m_adcSpeed == Approx( 10 ) );
    }

    SECTION( "the conventional amplifier forces unity gain" )
    {
        app.m_readoutSpeedName = "ccd_01MHz";
        app.m_emGainSet        = 300;
        app.m_emGain           = 20;
        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( app.m_emGain == Approx( 1 ) );
        REQUIRE( app.m_adcSpeed == Approx( 1 ) );
        REQUIRE( g_picamStub.setCount( PicamParameter_AdcEMGain ) == 0 );
    }

    SECTION( "an invalid readout speed is an ERROR" )
    {
        app.m_readoutSpeedName = "bogus";
        REQUIRE( app.setEMGain() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_picamStub.setCount( PicamParameter_AdcEMGain ) == 0 );
    }

    SECTION( "an online set error is reported" )
    {
        app.m_emGain                                       = 20;
        app.m_emGainSet                                    = 300;
        g_picamStub.onlineErrors[PicamParameter_AdcEMGain] = PicamError_UnexpectedError;
        REQUIRE( app.setEMGain() == -1 );
        REQUIRE( app.m_emGain == Approx( 20 ) );
    }

    SECTION( "a readback error is reported" )
    {
        app.m_emGain                                    = 20;
        app.m_emGainSet                                 = 300;
        g_picamStub.getErrors[PicamParameter_AdcEMGain] = PicamError_UnexpectedError;
        REQUIRE( app.setEMGain() == -1 );
        REQUIRE( app.m_emGain == Approx( 20 ) );
    }
}

/// Verify setExpTime() and capExpTime().
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl setExpTime", "[picamCtrl][exptime]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::setExpTime();
    picamCtrl::capExpTime(exptime);
    picamCtrl::updateFxnGenSync();
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.startupProperties();
    app.openCamera();
    app.m_ReadOutTimeCalculation                               = 2.5;
    g_picamStub.fltValues[PicamParameter_FrameRateCalculation] = 99.0;

    SECTION( "capExpTime caps at the readout time" )
    {
        piflt e = 1.0;
        REQUIRE( app.capExpTime( e ) == 0 );
        REQUIRE( e == Approx( 2.5 ) );

        e = 7.0;
        REQUIRE( app.capExpTime( e ) == 0 );
        REQUIRE( e == Approx( 7.0 ) );
    }

    SECTION( "offline set when not acquiring" )
    {
        app.state( stateCodes::READY );
        app.m_expTimeSet = 0.01;
        REQUIRE( app.setExpTime() == 0 );

        const picamSetCall *c = g_picamStub.lastSet( PicamParameter_ExposureTime );
        REQUIRE( c != nullptr );
        REQUIRE( c->kind == setKindFlt );
        REQUIRE( c->handle == modelHandle() );
        REQUIRE( c->value == Approx( 10.0 ) );
        REQUIRE( g_picamStub.commitCalls == 1 );
        REQUIRE( app.m_expTime == Approx( 0.01 ) );
        REQUIRE( app.m_fps == Approx( 99.0 ) );
    }

    SECTION( "online set while acquiring" )
    {
        app.state( stateCodes::OPERATING );
        app.m_expTimeSet = 0.02;
        REQUIRE( app.setExpTime() == 0 );

        const picamSetCall *c = g_picamStub.lastSet( PicamParameter_ExposureTime );
        REQUIRE( c != nullptr );
        REQUIRE( c->kind == setKindFltOnline );
        REQUIRE( c->value == Approx( 20.0 ) );
        REQUIRE( g_picamStub.commitCalls == 0 );
        REQUIRE( app.m_expTime == Approx( 0.02 ) );
    }

    SECTION( "requests below the readout time are capped" )
    {
        app.state( stateCodes::READY );
        app.m_expTimeSet = 0.0001;
        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( g_picamStub.lastSet( PicamParameter_ExposureTime )->value == Approx( 2.5 ) );
        REQUIRE( app.m_expTime == Approx( 0.0025 ) );
    }

    SECTION( "a parameter the camera does not commit is an error" )
    {
        app.state( stateCodes::READY );
        app.m_expTime                = 0.5;
        app.m_expTimeSet             = 0.01;
        g_picamStub.failedParameters = { PicamParameter_ExposureTime };
        REQUIRE( app.setExpTime() == -1 );
        REQUIRE( g_picamStub.destroyParametersCalls == 1 );
        REQUIRE( app.m_expTime == Approx( 0.5 ) );
    }

    SECTION( "an SDK error is reported" )
    {
        app.state( stateCodes::OPERATING );
        app.m_expTime                                         = 0.5;
        app.m_expTimeSet                                      = 0.01;
        g_picamStub.onlineErrors[PicamParameter_ExposureTime] = PicamError_UnexpectedError;
        REQUIRE( app.setExpTime() == -1 );
        REQUIRE( app.m_expTime == Approx( 0.5 ) );
    }

    SECTION( "synchro with another camera forwards the exposure time and the frame rate" )
    {
        app.state( stateCodes::READY );
        app.m_synchro      = true;
        app.m_otherCamName = "camsci2";
        app.m_expTimeSet   = 0.01;
        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( app.m_indiP_otherCamExptime["target"].get<double>() == Approx( 0.01 ) );
        REQUIRE( app.m_indiP_fxngensync_freq["target"].get<double>() == Approx( 99.0 ) );
        REQUIRE( app.m_indiP_fxngensync_output["value"].get() == "On" );
    }
}

/// Verify setSynchro() couples the other camera and requests a reconfigure.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl setSynchro", "[picamCtrl][synchro]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::setSynchro();
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.m_otherCamName = "camsci2";
    app.startupProperties();
    app.m_reconfig = false;
    app.m_expTime  = 0.25;

    SECTION( "no other camera" )
    {
        app.m_otherCamName = "";
        app.m_synchroSet   = true;
        REQUIRE( app.setSynchro() == 0 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "synchro on turns on the other camera and sends it the exposure time" )
    {
        app.m_synchroSet = true;
        REQUIRE( app.setSynchro() == 0 );
        REQUIRE( app.m_indiP_otherCamSynchro["toggle"].getSwitchState() == pcf::IndiElement::On );
        REQUIRE( app.m_indiP_otherCamExptime["target"].get<double>() == Approx( 0.25 ) );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "synchro off turns off the other camera" )
    {
        app.m_indiP_otherCamSynchro["toggle"] = pcf::IndiElement::On;
        app.m_synchroSet                      = false;
        REQUIRE( app.setSynchro() == 0 );
        REQUIRE( app.m_indiP_otherCamSynchro["toggle"].getSwitchState() == pcf::IndiElement::Off );
        REQUIRE( app.m_reconfig == true );
    }
}

/// Verify configureAcquisition() applies fan, temperature, readout, vertical shift, ROI, exposure and trigger settings.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl configureAcquisition", "[picamCtrl][config_acq]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::configureAcquisition();
    picamCtrl::setPicamParameter(m_modelHandle, PicamParameter_DisableCoolingFan, 0);
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.startupProperties();
    app.primeConfigure();
    app.state( stateCodes::CONNECTED );

    SECTION( "full-frame electron-multiplied configuration" )
    {
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );

        // time stamps
        const picamSetCall *c = g_picamStub.lastSet( PicamParameter_TimeStamps );
        REQUIRE( c != nullptr );
        REQUIRE( c->handle == modelHandle() );
        REQUIRE( c->value == Approx( PicamTimeStampsMask_ExposureStarted ) );
        REQUIRE( app.m_tsRes == 1000000 );

        // fan on means DisableCoolingFan = 0
        c = g_picamStub.lastSet( PicamParameter_DisableCoolingFan );
        REQUIRE( c != nullptr );
        REQUIRE( c->kind == setKindInt );
        REQUIRE( c->handle == modelHandle() );
        REQUIRE( c->value == Approx( 0 ) );
        REQUIRE( app.m_fanSpeedName == "on" );
        REQUIRE( app.m_fanSpeedValid == true );
        REQUIRE( app.m_fanSpeedLogPending == false );

        // temperature set point on the camera
        c = g_picamStub.lastSet( PicamParameter_SensorTemperatureSetPoint );
        REQUIRE( c != nullptr );
        REQUIRE( c->kind == setKindFlt );
        REQUIRE( c->handle == cameraHandle() );
        REQUIRE( c->value == Approx( -55 ) );

        // readout speed and quality
        REQUIRE( g_picamStub.lastSet( PicamParameter_AdcSpeed )->value == Approx( 5 ) );
        REQUIRE( g_picamStub.lastSet( PicamParameter_AdcQuality )->value ==
                 Approx( PicamAdcQuality_ElectronMultiplied ) );
        REQUIRE( app.m_adcSpeed == Approx( 5 ) );
        REQUIRE( app.m_readoutSpeedName == "emccd_05MHz" );
        REQUIRE( app.m_emGain == Approx( 50 ) );

        // vertical shift
        REQUIRE( g_picamStub.lastSet( PicamParameter_VerticalShiftRate )->value == Approx( 1.2 ) );
        REQUIRE( app.m_vShiftSpeedName == "1_2us" );
        REQUIRE( app.m_vshiftSpeed == Approx( 1.2 ) );

        // ROI
        REQUIRE( g_picamStub.setRoisCalls == 1 );
        REQUIRE( g_picamStub.lastSetRoi.x == 0 );
        REQUIRE( g_picamStub.lastSetRoi.y == 0 );
        REQUIRE( g_picamStub.lastSetRoi.width == 1024 );
        REQUIRE( g_picamStub.lastSetRoi.height == 1024 );
        REQUIRE( g_picamStub.lastSetRoi.x_binning == 1 );
        REQUIRE( g_picamStub.lastSetRoi.y_binning == 1 );
        REQUIRE( g_picamStub.destroyRoisCalls == 1 );
        REQUIRE( app.m_currentROI.x == Approx( 511.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 511.5 ) );
        REQUIRE( app.m_currentROI.w == 1024 );
        REQUIRE( app.m_currentROI.h == 1024 );
        REQUIRE( app.m_width == 1024 );
        REQUIRE( app.m_height == 1024 );
        REQUIRE( app.m_depth == 16 );
        REQUIRE( app.m_frameSize == 4088 );

        // exposure time and frame rate
        REQUIRE( app.m_ReadOutTimeCalculation == Approx( 2.5 ) );
        REQUIRE( app.m_minExpTime == Approx( 0.001 ) );
        REQUIRE( app.m_maxExpTime == Approx( 1000.0 ) );
        REQUIRE( app.m_stepExpTime == Approx( 0.0001 ) );
        REQUIRE( g_picamStub.lastSet( PicamParameter_ExposureTime )->value == Approx( 10.0 ) );
        REQUIRE( app.m_expTime == Approx( 0.01 ) );
        REQUIRE( app.m_expTimeSet == Approx( 0.01 ) );
        REQUIRE( app.m_fps == Approx( 100.0 ) );

        // acquisition buffer
        REQUIRE( app.m_acqBuff.memory != nullptr );
        REQUIRE( app.m_acqBuff.memory_size == 40960 );
        REQUIRE( g_picamStub.setBufferCalls == 1 );
        REQUIRE( g_picamStub.lastBuffer == &app.m_acqBuff );

        // no synchro
        c = g_picamStub.lastSet( PicamParameter_TriggerResponse );
        REQUIRE( c != nullptr );
        REQUIRE( c->handle == cameraHandle() );
        REQUIRE( c->value == Approx( PicamTriggerResponse_NoResponse ) );
        REQUIRE( g_picamStub.setCount( PicamParameter_TriggerDetermination ) == 0 );
        REQUIRE( app.m_synchro == false );

        // continuous acquisition
        c = g_picamStub.lastSet( PicamParameter_ReadoutCount );
        REQUIRE( c != nullptr );
        REQUIRE( c->kind == setKindLarge );
        REQUIRE( c->value == Approx( 0 ) );
        REQUIRE( g_picamStub.startCalls == 1 );
        REQUIRE( app.m_dataType == _DATATYPE_UINT16 );
    }

    SECTION( "time stamp errors are not detected" )
    {
        // configureAcquisition() tests these PicamError returns with `< 0`, which never holds for PICam errors.
        g_picamStub.setErrors[PicamParameter_TimeStamps]          = PicamError_UnexpectedError;
        g_picamStub.getErrors[PicamParameter_TimeStampResolution] = PicamError_UnexpectedError;
        app.m_tsRes                                               = 7;
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.m_tsRes == 7 );
        REQUIRE( g_picamStub.startCalls == 1 );
    }

    SECTION( "the acquisition buffer is reused when large enough" )
    {
        REQUIRE( app.configureAcquisition() == 0 );
        void *mem = app.m_acqBuff.memory;
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.m_acqBuff.memory == mem );
        REQUIRE( g_picamStub.setBufferCalls == 1 );
    }

    SECTION( "fan off means DisableCoolingFan = 1" )
    {
        app.m_fanSpeedName    = "on";
        app.m_fanSpeedNameSet = "off";
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_picamStub.lastSet( PicamParameter_DisableCoolingFan )->value == Approx( 1 ) );
        REQUIRE( app.m_fanSpeedName == "off" );
    }

    SECTION( "no fan write without fan control" )
    {
        app.m_fanSpeedControlEnabled = false;
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_picamStub.setCount( PicamParameter_DisableCoolingFan ) == 0 );
    }

    SECTION( "an invalid fan speed is an ERROR" )
    {
        app.m_fanSpeedNameSet = "medium";
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_picamStub.setCount( PicamParameter_DisableCoolingFan ) == 0 );
        REQUIRE( g_picamStub.startCalls == 0 );
    }

    SECTION( "a fan write error is an ERROR" )
    {
        g_picamStub.setErrors[PicamParameter_DisableCoolingFan] = PicamError_UnexpectedError;
        app.m_fanSpeedValid                                     = false;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.m_fanSpeedValid == false );
        REQUIRE( g_picamStub.startCalls == 0 );
    }

    SECTION( "the readout control mode must be frame transfer" )
    {
        g_picamStub.intValues[PicamParameter_ReadoutControlMode] = PicamReadoutControlMode_FullFrame;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( g_picamStub.setCount( PicamParameter_DisableCoolingFan ) == 0 );
        REQUIRE( g_picamStub.startCalls == 0 );
    }

    SECTION( "a temperature set point error is an ERROR" )
    {
        g_picamStub.setErrors[PicamParameter_SensorTemperatureSetPoint] = PicamError_UnexpectedError;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "the conventional amplifier resets the EM gain" )
    {
        app.m_readoutSpeedNameSet = "ccd_00_1MHz";
        app.m_emGain              = 50;
        app.m_emGainSet           = 50;
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_picamStub.lastSet( PicamParameter_AdcSpeed )->value == Approx( 0.1 ) );
        REQUIRE( g_picamStub.lastSet( PicamParameter_AdcQuality )->value == Approx( PicamAdcQuality_LowNoise ) );
        REQUIRE( app.m_readoutSpeedName == "ccd_00_1MHz" );
        REQUIRE( app.m_emGain == Approx( 1 ) );
        REQUIRE( app.m_emGainSet == Approx( 1 ) );
    }

    SECTION( "an invalid readout speed is an ERROR" )
    {
        app.m_readoutSpeedNameSet = "bogus";
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_picamStub.setCount( PicamParameter_AdcQuality ) == 0 );
    }

    SECTION( "an ADC speed error is not fatal" )
    {
        g_picamStub.setErrors[PicamParameter_AdcSpeed] = PicamError_UnexpectedError;
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_picamStub.startCalls == 1 );
    }

    SECTION( "an ADC quality error is an ERROR" )
    {
        g_picamStub.setErrors[PicamParameter_AdcQuality] = PicamError_UnexpectedError;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "an invalid vertical shift speed is an ERROR" )
    {
        app.m_vShiftSpeedNameSet = "3_0us";
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_picamStub.setCount( PicamParameter_VerticalShiftRate ) == 0 );
    }

    SECTION( "a vertical shift error is an ERROR" )
    {
        g_picamStub.setErrors[PicamParameter_VerticalShiftRate] = PicamError_UnexpectedError;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "a binned sub-ROI" )
    {
        app.m_nextROI.x     = 255.5;
        app.m_nextROI.y     = 767.5;
        app.m_nextROI.w     = 256;
        app.m_nextROI.h     = 128;
        app.m_nextROI.bin_x = 2;
        app.m_nextROI.bin_y = 1;

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_picamStub.lastSetRoi.x == 128 );
        REQUIRE( g_picamStub.lastSetRoi.y == 704 );
        REQUIRE( g_picamStub.lastSetRoi.width == 256 );
        REQUIRE( g_picamStub.lastSetRoi.height == 128 );
        REQUIRE( g_picamStub.lastSetRoi.x_binning == 2 );
        REQUIRE( g_picamStub.lastSetRoi.y_binning == 1 );
        REQUIRE( app.m_currentROI.x == Approx( 255.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 767.5 ) );
        REQUIRE( app.m_currentROI.bin_x == 2 );
        REQUIRE( app.m_xbinning == 2 );
        REQUIRE( app.m_width == 128 );
        REQUIRE( app.m_height == 128 );
        REQUIRE( app.m_nextROI.x == Approx( 255.5 ) );
        REQUIRE( app.m_nextROI.w == 256 );
    }

    SECTION( "a left-right flipped ROI" )
    {
        app.m_defaultFlip = picamCtrl::fgFlipLR;
        app.m_nextROI.x   = 255.5;
        app.m_nextROI.w   = 256;
        app.m_nextROI.y   = 511.5;
        app.m_nextROI.h   = 1024;

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_picamStub.lastSetRoi.x == 640 );
        REQUIRE( g_picamStub.lastSetRoi.y == 0 );
        REQUIRE( app.m_currentROI.x == Approx( 255.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 511.5 ) );
    }

    SECTION( "an up-down flipped ROI" )
    {
        app.m_defaultFlip = picamCtrl::fgFlipUD;
        app.m_nextROI.y   = 767.5;
        app.m_nextROI.h   = 128;

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_picamStub.lastSetRoi.x == 0 );
        REQUIRE( g_picamStub.lastSetRoi.y == 192 );
        REQUIRE( app.m_currentROI.y == Approx( 767.5 ) );
    }

    SECTION( "an ROI off the sensor is not sent, and the camera ROI is read back" )
    {
        app.m_nextROI.x = 10;
        app.m_nextROI.w = 100;

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_picamStub.setRoisCalls == 0 );
        REQUIRE( app.m_currentROI.x == Approx( 511.5 ) );
        REQUIRE( app.m_currentROI.w == 1024 );
        REQUIRE( app.m_nextROI.x == Approx( 511.5 ) );
    }

    SECTION( "an ROI set error is an ERROR" )
    {
        g_picamStub.setRoisReturn = PicamError_UnexpectedError;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_picamStub.startCalls == 0 );
    }

    SECTION( "an ROI readback error is an ERROR" )
    {
        g_picamStub.getRoisReturn = PicamError_UnexpectedError;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "frame geometry read errors are an ERROR" )
    {
        auto param = GENERATE( PicamParameter_ReadoutStride,
                               PicamParameter_FrameStride,
                               PicamParameter_FramesPerReadout,
                               PicamParameter_FrameSize,
                               PicamParameter_PixelBitDepth );

        g_picamStub.getErrors[param] = PicamError_UnexpectedError;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_picamStub.startCalls == 0 );
    }

    SECTION( "timing read errors fail without changing state" )
    {
        auto param = GENERATE(
            PicamParameter_ReadoutTimeCalculation, PicamParameter_ExposureTime, PicamParameter_FrameRateCalculation );

        g_picamStub.getErrors[param] = PicamError_UnexpectedError;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( g_picamStub.startCalls == 0 );
    }

    SECTION( "the exposure time is capped at the readout time" )
    {
        app.m_expTimeSet = 0.001;
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_picamStub.lastSet( PicamParameter_ExposureTime )->value == Approx( 2.5 ) );
        REQUIRE( app.m_expTime == Approx( 0.0025 ) );
        REQUIRE( app.m_expTimeSet == Approx( 0.0025 ) );
    }

    SECTION( "no exposure time request reads the camera value" )
    {
        app.m_expTimeSet                                   = 0;
        g_picamStub.fltValues[PicamParameter_ExposureTime] = 30.0;
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_picamStub.setCount( PicamParameter_ExposureTime ) == 0 );
        REQUIRE( app.m_expTime == Approx( 0.03 ) );
        REQUIRE( app.m_expTimeSet == Approx( 0.03 ) );
    }

    SECTION( "an exposure time set error fails" )
    {
        g_picamStub.setErrors[PicamParameter_ExposureTime] = PicamError_UnexpectedError;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( g_picamStub.startCalls == 0 );
    }

    SECTION( "more than one exposure constraint leaves the limits alone" )
    {
        app.m_minExpTime = 0;
        g_picamStub.constraints.push_back( g_picamStub.constraints[0] );
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.m_minExpTime == Approx( 0 ) );
    }

    SECTION( "synchro on sets the hardware trigger" )
    {
        app.m_synchroSet = true;
        REQUIRE( app.configureAcquisition() == 0 );

        const picamSetCall *c = g_picamStub.lastSet( PicamParameter_TriggerDetermination );
        REQUIRE( c != nullptr );
        REQUIRE( c->handle == cameraHandle() );
        REQUIRE( c->value == Approx( PicamTriggerDetermination_RisingEdge ) );
        REQUIRE( g_picamStub.lastSet( PicamParameter_TriggerResponse )->value ==
                 Approx( PicamTriggerResponse_ReadoutPerTrigger ) );
        REQUIRE( app.m_synchro == true );
        REQUIRE( app.m_indiP_fxngensync_freq["target"].get<double>() == Approx( 100.0 ) );
    }

    SECTION( "a readout count error is an ERROR" )
    {
        g_picamStub.setErrors[PicamParameter_ReadoutCount] = PicamError_UnexpectedError;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_picamStub.startCalls == 0 );
    }

    SECTION( "a start acquisition error is an ERROR" )
    {
        g_picamStub.startReturn = PicamError_UnexpectedError;
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_picamStub.startCalls == 1 );
    }

    SECTION( "power off during an error returns without logging or changing state" )
    {
        app.power( 0 );
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( g_picamStub.startCalls == 0 );
    }
}

/// Verify acquireAndCheckValid() and loadImageIntoStream().
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl frame acquisition", "[picamCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::acquireAndCheckValid();
    picamCtrl::loadImageIntoStream(nullptr);
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.openCamera();
    app.state( stateCodes::OPERATING );

    SECTION( "a timeout returns 1" )
    {
        g_picamStub.waitReturn = PicamError_TimeOutOccurred;
        REQUIRE( app.acquireAndCheckValid() == 1 );
        REQUIRE( g_picamStub.lastWaitTimeout == 1000 );
        REQUIRE( app.state() == stateCodes::OPERATING );
    }

    SECTION( "an error is an ERROR" )
    {
        g_picamStub.waitReturn = PicamError_CameraFaulted;
        REQUIRE( app.acquireAndCheckValid() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "no readout returns 1" )
    {
        g_picamStub.waitData = { nullptr, 0 };
        REQUIRE( app.acquireAndCheckValid() == 1 );
    }

    SECTION( "a readout sets the camera time stamp from the frame metadata" )
    {
        std::vector<pi64s> buf( 4, 0 );
        buf[2]                     = 2500000; // metadata just after a 16 byte frame
        app.m_frameSize            = 16;
        app.m_tsRes                = 1000000;
        app.m_FrameRateCalculation = 100;
        app.m_camera_timestamp     = 2.49;
        g_picamStub.waitData       = { buf.data(), 1 };

        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( app.m_available.initial_readout == buf.data() );
        REQUIRE( app.m_available.readout_count == 1 );
        REQUIRE( app.m_camera_timestamp == Approx( 2.5 ) );
        REQUIRE( app.m_currImageTimestamp.tv_sec > 0 );
    }

    SECTION( "loadImageIntoStream copies with the configured flip" )
    {
        std::vector<uint16_t> src = { 1, 2, 3, 4 };
        std::vector<uint16_t> dest( 4, 0 );
        app.m_available.initial_readout = src.data();
        app.m_width                     = 2;
        app.m_height                    = 2;
        app.m_typeSize                  = sizeof( uint16_t );

        app.m_defaultFlip = picamCtrl::fgFlipNone;
        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
        REQUIRE( dest == std::vector<uint16_t>{ 1, 2, 3, 4 } );

        app.m_defaultFlip = picamCtrl::fgFlipUD;
        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
        REQUIRE( dest == std::vector<uint16_t>{ 3, 4, 1, 2 } );

        app.m_defaultFlip = 99;
        REQUIRE( app.loadImageIntoStream( dest.data() ) == -1 );
    }
}

/// Verify reconfig() stops the acquisition.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl reconfig", "[picamCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::reconfig();
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.openCamera();
    app.state( stateCodes::OPERATING );

    SECTION( "a stopped acquisition returns immediately" )
    {
        g_picamStub.running = false;
        REQUIRE( app.reconfig() == 0 );
        REQUIRE( g_picamStub.stopCalls == 1 );
        REQUIRE( g_picamStub.runningCalls == 1 );
        REQUIRE( g_picamStub.waitCalls == 0 );
    }

    SECTION( "a stop error is an ERROR" )
    {
        g_picamStub.stopReturn = PicamError_UnexpectedError;
        REQUIRE( app.reconfig() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "power off while running returns" )
    {
        g_picamStub.running = true;
        app.power( 0 );
        REQUIRE( app.reconfig() == 0 );
        REQUIRE( g_picamStub.stopCalls == 1 );
    }

    SECTION( "a running acquisition is stopped again and waited on" )
    {
        // This path sleeps for 1 s.
        g_picamStub.runningSequence = { true, false };
        REQUIRE( app.reconfig() == 0 );
        REQUIRE( g_picamStub.stopCalls == 2 );
        REQUIRE( g_picamStub.waitCalls == 1 );
        REQUIRE( g_picamStub.runningCalls == 2 );
    }
}

/// Verify the PICam parameter helpers, including commit failures.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl parameter helpers", "[picamCtrl]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::getPicamParameter(ivalue, PicamParameter_FrameSize);
    picamCtrl::getPicamParameter(fvalue, PicamParameter_ExposureTime);
    picamCtrl::setPicamParameter(PicamParameter_ReadoutCount, (pi64s) 0, true);
    picamCtrl::setPicamParameter(PicamParameter_AdcQuality, (piint) 0, true);
    picamCtrl::setPicamParameter(PicamParameter_ExposureTime, (piflt) 0, true);
    picamCtrl::setPicamParameterOnline(PicamParameter_ExposureTime, (piflt) 0);
    picamCtrl::setPicamParameterOnline(PicamParameter_AdcEMGain, (piint) 0);
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.openCamera();

    SECTION( "gets" )
    {
        g_picamStub.intValues[PicamParameter_FrameSize]    = 123;
        g_picamStub.fltValues[PicamParameter_ExposureTime] = 4.5;

        piint iv = 0;
        piflt fv = 0;
        REQUIRE( app.getPicamParameter( iv, PicamParameter_FrameSize ) == 0 );
        REQUIRE( iv == 123 );
        REQUIRE( app.getPicamParameter( fv, PicamParameter_ExposureTime ) == 0 );
        REQUIRE( fv == Approx( 4.5 ) );

        g_picamStub.getErrors[PicamParameter_FrameSize] = PicamError_ParameterDoesNotExist;
        REQUIRE( app.getPicamParameter( iv, PicamParameter_FrameSize ) == -1 );
    }

    SECTION( "gets fail silently while powered off" )
    {
        app.power( 0 );
        piint iv = 0;
        REQUIRE( app.getPicamParameter( iv, PicamParameter_FrameSize ) == -1 );
    }

    SECTION( "sets on the camera handle commit by default" )
    {
        REQUIRE( app.setPicamParameter( PicamParameter_ReadoutCount, static_cast<pi64s>( 7 ) ) == 0 );
        REQUIRE( app.setPicamParameter( PicamParameter_AdcQuality, static_cast<piint>( 3 ) ) == 0 );
        REQUIRE( app.setPicamParameter( PicamParameter_ExposureTime, static_cast<piflt>( 1.5 ) ) == 0 );
        REQUIRE( g_picamStub.commitCalls == 3 );
        REQUIRE( g_picamStub.destroyParametersCalls == 3 );
        REQUIRE( g_picamStub.lastSet( PicamParameter_ReadoutCount )->kind == setKindLarge );
        REQUIRE( g_picamStub.lastSet( PicamParameter_AdcQuality )->kind == setKindInt );
        REQUIRE( g_picamStub.lastSet( PicamParameter_ExposureTime )->kind == setKindFlt );
        REQUIRE( g_picamStub.lastSet( PicamParameter_ExposureTime )->handle == cameraHandle() );
    }

    SECTION( "sets without commit" )
    {
        REQUIRE( app.setPicamParameter( PicamParameter_ExposureTime, static_cast<piflt>( 1.5 ), false ) == 0 );
        REQUIRE( g_picamStub.commitCalls == 0 );
    }

    SECTION( "a commit error fails" )
    {
        g_picamStub.commitReturn = PicamError_UnexpectedError;
        REQUIRE( app.setPicamParameter( PicamParameter_AdcQuality, static_cast<piint>( 3 ) ) == -1 );
        REQUIRE( app.setPicamParameter( PicamParameter_ReadoutCount, static_cast<pi64s>( 0 ) ) == -1 );
    }

    SECTION( "another failed parameter does not fail the set" )
    {
        g_picamStub.failedParameters = { PicamParameter_AdcSpeed };
        REQUIRE( app.setPicamParameter( PicamParameter_AdcQuality, static_cast<piint>( 3 ) ) == 0 );
        g_picamStub.failedParameters = { PicamParameter_AdcQuality };
        REQUIRE( app.setPicamParameter( PicamParameter_AdcQuality, static_cast<piint>( 3 ) ) == -1 );
    }

    SECTION( "online sets" )
    {
        REQUIRE( app.setPicamParameterOnline( PicamParameter_ExposureTime, static_cast<piflt>( 2.0 ) ) == 0 );
        REQUIRE( app.setPicamParameterOnline( PicamParameter_AdcEMGain, static_cast<piint>( 10 ) ) == 0 );
        REQUIRE( g_picamStub.lastSet( PicamParameter_ExposureTime )->kind == setKindFltOnline );
        REQUIRE( g_picamStub.lastSet( PicamParameter_AdcEMGain )->kind == setKindIntOnline );
        REQUIRE( g_picamStub.commitCalls == 0 );

        g_picamStub.onlineErrors[PicamParameter_AdcEMGain] = PicamError_UnexpectedError;
        REQUIRE( app.setPicamParameterOnline( PicamParameter_AdcEMGain, static_cast<piint>( 10 ) ) == -1 );
    }
}

/// Verify the stdCamera INDI callbacks reach the picamCtrl hooks.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl stdCamera callbacks", "[picamCtrl][indi]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    dev::stdCamera<picamCtrl>::newCallBack_stdCamera(pcf::IndiProperty());
    dev::stdCamera<picamCtrl>::newCallBack_fanSpeed(pcf::IndiProperty());
    dev::stdCamera<picamCtrl>::newCallBack_readoutSpeed(pcf::IndiProperty());
    dev::stdCamera<picamCtrl>::newCallBack_vShiftSpeed(pcf::IndiProperty());
    dev::stdCamera<picamCtrl>::newCallBack_temp(pcf::IndiProperty());
    dev::stdCamera<picamCtrl>::newCallBack_temp_controller(pcf::IndiProperty());
    dev::stdCamera<picamCtrl>::newCallBack_emgain(pcf::IndiProperty());
    dev::stdCamera<picamCtrl>::newCallBack_exptime(pcf::IndiProperty());
    dev::stdCamera<picamCtrl>::newCallBack_synchro(pcf::IndiProperty());
    dev::stdCamera<picamCtrl>::newCallBack_roi_set(pcf::IndiProperty());
    picamCtrl::setFanSpeed();
    picamCtrl::setReadoutSpeed();
    picamCtrl::setVShiftSpeed();
    picamCtrl::setTempSetPt();
    picamCtrl::setTempControl();
    picamCtrl::setEMGain();
    picamCtrl::setExpTime();
    picamCtrl::setSynchro();
    picamCtrl::setNextROI();
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.startupProperties();
    app.openCamera();
    app.state( stateCodes::OPERATING );
    app.m_reconfig = false;

    SECTION( "fan_speed selects the fan state" )
    {
        app.m_fanSpeedName       = "on";
        app.m_fanSpeedLogPending = false;
        REQUIRE( app.newCallBack_stdCamera(
                     switchProp( "picam", "fan_speed", { { "on", false }, { "off", true } } ) ) == 0 );
        REQUIRE( app.m_fanSpeedNameSet == "off" );
        REQUIRE( app.m_fanSpeedLogPending == true );
        REQUIRE( app.m_reconfig == true );
        REQUIRE( g_picamStub.setCount( PicamParameter_DisableCoolingFan ) == 0 );
    }

    SECTION( "fan_speed with nothing selected resets to the current state" )
    {
        app.m_fanSpeedName    = "on";
        app.m_fanSpeedNameSet = "off";
        REQUIRE( app.newCallBack_stdCamera( switchProp( "picam", "fan_speed", { { "on", false } } ) ) == 0 );
        REQUIRE( app.m_fanSpeedNameSet == "on" );
    }

    SECTION( "fan_speed with both selected is rejected" )
    {
        app.m_fanSpeedNameSet = "on";
        REQUIRE( app.newCallBack_stdCamera( switchProp( "picam", "fan_speed", { { "on", true }, { "off", true } } ) ) ==
                 -1 );
        REQUIRE( app.m_fanSpeedNameSet == "on" );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "fan_speed is not dispatched without fan control" )
    {
        app.m_fanSpeedControlEnabled = false;
        REQUIRE( app.newCallBack_stdCamera( switchProp( "picam", "fan_speed", { { "off", true } } ) ) == -1 );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "a property for another device is rejected" )
    {
        REQUIRE( app.newCallBack_stdCamera( switchProp( "other", "fan_speed", { { "off", true } } ) ) == -1 );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "readout_speed" )
    {
        REQUIRE( app.newCallBack_stdCamera( switchProp( "picam", "readout_speed", { { "emccd_20MHz", true } } ) ) ==
                 0 );
        REQUIRE( app.m_readoutSpeedNameSet == "emccd_20MHz" );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "vshift_speed" )
    {
        REQUIRE( app.newCallBack_stdCamera( switchProp( "picam", "vshift_speed", { { "5_0us", true } } ) ) == 0 );
        REQUIRE( app.m_vShiftSpeedNameSet == "5_0us" );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "temp_ccd sets the set point and requests a reconfigure" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "picam", "temp_ccd", "target", -30 ) ) == 0 );
        REQUIRE( app.m_ccdTempSetpt == Approx( -30 ) );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "temp_controller off still leaves temperature control on" )
    {
        REQUIRE( app.newCallBack_stdCamera( switchProp( "picam", "temp_controller", { { "toggle", false } } ) ) == 0 );
        REQUIRE( app.m_tempControlStatus == true );
        REQUIRE( app.m_tempControlStatusSet == true );
    }

    SECTION( "emgain sets the EM gain online" )
    {
        app.m_readoutSpeedName = "emccd_05MHz";
        REQUIRE( app.newCallBack_stdCamera( numberProp( "picam", "emgain", "target", 200 ) ) == 0 );
        REQUIRE( app.m_emGainSet == Approx( 200 ) );
        REQUIRE( g_picamStub.lastSet( PicamParameter_AdcEMGain )->value == Approx( 200 ) );
        REQUIRE( app.m_emGain == Approx( 200 ) );
    }

    SECTION( "exptime sets the exposure time online while acquiring" )
    {
        app.m_ReadOutTimeCalculation = 1;
        REQUIRE( app.newCallBack_stdCamera( numberProp( "picam", "exptime", "target", 0.05 ) ) == 0 );
        REQUIRE( g_picamStub.lastSet( PicamParameter_ExposureTime )->kind == setKindFltOnline );
        REQUIRE( g_picamStub.lastSet( PicamParameter_ExposureTime )->value == Approx( 50.0 ) );
        REQUIRE( app.m_expTime == Approx( 0.05 ) );
    }

    SECTION( "synchro requests a reconfigure" )
    {
        REQUIRE( app.newCallBack_stdCamera( switchProp( "picam", "synchro", { { "toggle", true } } ) ) == 0 );
        REQUIRE( app.m_synchroSet == true );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "roi_set requests a reconfigure" )
    {
        app.m_currentROI.x = 100;
        REQUIRE( app.newCallBack_stdCamera( switchProp( "picam", "roi_set", { { "request", true } } ) ) == 0 );
        REQUIRE( app.m_lastROI.x == Approx( 100 ) );
        REQUIRE( app.m_reconfig == true );
    }
}

/// Verify the receiveSynchro and receiveExptime callbacks used by the other camera.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl receive callbacks", "[picamCtrl][indi]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::newCallBack_m_indiP_receiveSynchro(pcf::IndiProperty());
    picamCtrl::newCallBack_m_indiP_receiveExptime(pcf::IndiProperty());
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );
    app.startupProperties();
    app.m_reconfig   = false;
    app.m_synchroSet = false;

    SECTION( "receiveSynchro on and off" )
    {
        REQUIRE( app.newCallBack_m_indiP_receiveSynchro(
                     switchProp( "picam", "receiveSynchro", { { "toggle", true } } ) ) == 0 );
        REQUIRE( app.m_synchroSet == true );
        REQUIRE( app.m_reconfig == true );

        app.m_reconfig = false;
        REQUIRE( app.newCallBack_m_indiP_receiveSynchro(
                     switchProp( "picam", "receiveSynchro", { { "toggle", false } } ) ) == 0 );
        REQUIRE( app.m_synchroSet == false );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "receiveSynchro without a toggle is a no-op" )
    {
        REQUIRE( app.newCallBack_m_indiP_receiveSynchro(
                     switchProp( "picam", "receiveSynchro", { { "other", true } } ) ) == 0 );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "receiveSynchro with the wrong device or name is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_receiveSynchro(
                     switchProp( "other", "receiveSynchro", { { "toggle", true } } ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_receiveSynchro( switchProp( "picam", "synchro", { { "toggle", true } } ) ) ==
                 -1 );
        REQUIRE( app.m_synchroSet == false );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "receiveExptime sets the requested exposure time" )
    {
        REQUIRE( app.newCallBack_m_indiP_receiveExptime( numberProp( "picam", "receiveExptime", "target", 0.5 ) ) ==
                 0 );
        REQUIRE( app.m_expTimeSet == Approx( 0.5 ) );
        REQUIRE( app.m_reconfig == true );
        REQUIRE( g_picamStub.setCalls.empty() );
    }

    SECTION( "receiveExptime without a target is a no-op" )
    {
        app.m_expTimeSet = 0.1;
        REQUIRE( app.newCallBack_m_indiP_receiveExptime( numberProp( "picam", "receiveExptime", "current", 0.5 ) ) ==
                 0 );
        REQUIRE( app.m_expTimeSet == Approx( 0.1 ) );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "receiveExptime with the wrong device is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_receiveExptime( numberProp( "other", "receiveExptime", "target", 0.5 ) ) ==
                 -1 );
        REQUIRE( app.m_reconfig == false );
    }
}

/// Verify the power-off hooks and appShutdown() release the camera.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl power off and shutdown", "[picamCtrl][power]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::onPowerOff();
    picamCtrl::whilePowerOff();
    picamCtrl::appShutdown();
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );

    SECTION( "onPowerOff closes the camera and updates the shutter status" )
    {
        app.openCamera();
        app.shutterPower( 0 );
        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_picamStub.closeCalls == 1 );
        REQUIRE( g_picamStub.lastClosed == cameraHandle() );
        REQUIRE( app.m_cameraHandle == nullptr );
        REQUIRE( g_picamStub.uninitCalls == 1 );
        REQUIRE( app.m_shutterStatus == "POWEROFF" );
        REQUIRE( app.m_shutterState == -1 );
    }

    SECTION( "onPowerOff with no camera only uninitializes the library" )
    {
        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_picamStub.closeCalls == 0 );
        REQUIRE( g_picamStub.uninitCalls == 1 );
        REQUIRE( app.m_shutterStatus == "UNKNOWN" );
    }

    SECTION( "whilePowerOff tracks the shutter without touching the camera" )
    {
        app.openCamera();
        app.shutterPower( 1 );
        app.m_sensorState = 1;
        REQUIRE( app.whilePowerOff() == 0 );
        REQUIRE( g_picamStub.closeCalls == 0 );
        REQUIRE( g_picamStub.uninitCalls == 0 );
        REQUIRE( app.m_shutterStatus == "READY" );
        REQUIRE( app.m_shutterState == 1 );
    }

    SECTION( "appShutdown closes the camera" )
    {
        app.openCamera();
        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_picamStub.closeCalls == 1 );
        REQUIRE( app.m_cameraHandle == nullptr );
        REQUIRE( g_picamStub.uninitCalls == 1 );
    }
}

/// Verify the telemetry interface records the camera state.
/**
 * \ingroup picamCtrl_unit_test
 */
TEST_CASE( "picamCtrl telemetry", "[picamCtrl][telem]" )
{
    // clang-format off
    #ifdef PICAMCTRL_TEST_DOXYGEN_REF
    picamCtrl::checkRecordTimes();
    picamCtrl::recordTelem(nullptr);
    #endif
    // clang-format on

    picamCtrl_test app( "picam" );

    SECTION( "recordTelem forces a record" )
    {
        telem_stdcam::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( nullptr ) == 0 );
        REQUIRE( telem_stdcam::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records when the interval has elapsed" )
    {
        telem_stdcam::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_stdcam::lastRecord.tv_sec > 0 );
    }
}

} // namespace picamCtrlTest

} // namespace libXWCTest
