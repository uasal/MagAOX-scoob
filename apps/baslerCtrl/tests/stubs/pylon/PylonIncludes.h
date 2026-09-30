/** \file PylonIncludes.h
 * \brief Stand-in for the Basler Pylon SDK headers, for the baslerCtrl unit tests.
 * \author Claude Code
 *
 * Declares only the Pylon, GenApi and Basler_UsbCameraParams classes, members, enums and functions that
 * baslerCtrl uses.  The other Pylon headers baslerCtrl includes (`pylon/PixelData.h`, `pylon/GrabResultData.h`,
 * `pylon/usb/BaslerUsbInstantCamera.h`, `pylon/usb/_BaslerUsbCameraParams.h`, `GenApi/IFloat.h`) include this
 * file.  Camera parameters are simple value holders with fault injection; the non-inline functions
 * (factory, camera construction, open/close, grabbing) are defined in baslerCtrl_test.cpp, which drives
 * them from a fake SDK state.
 *
 * \ingroup baslerCtrl_files
 */

#ifndef baslerCtrl_tests_stubs_pylon_PylonIncludes_h
#define baslerCtrl_tests_stubs_pylon_PylonIncludes_h

#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <vector>

/// Stand-in for the Pylon namespace.
namespace Pylon
{

/// Pylon string type.
typedef std::string String_t;

/// Exception thrown by the stub parameters and camera on injected faults.
class GenericException : public std::exception
{
  public:
    /// Construct with a description.
    explicit GenericException( const std::string &what /**< [in] the description */ ) : m_what( what )
    {
    }

    /// Get the description.
    const char *what() const noexcept override
    {
        return m_what.c_str();
    }

  protected:
    std::string m_what; ///< The description.
};

/// Base of the stub camera parameters, holding the access and fault-injection flags.
class CParameterBase
{
  public:
    bool m_writable{ true }; ///< Whether GenApi::IsWritable reports this parameter writable.

    bool m_throwOnSet{ false }; ///< If true SetValue throws GenericException.

    bool m_throwOnGet{ false }; ///< If true GetValue throws GenericException.

    int m_setCalls{ 0 }; ///< Number of successful SetValue calls.

  protected:
    /// Throw if a set fault is injected.
    void checkSet() const
    {
        if( m_throwOnSet )
        {
            throw GenericException( "SetValue fault" );
        }
    }

    /// Throw if a get fault is injected.
    void checkGet() const
    {
        if( m_throwOnGet )
        {
            throw GenericException( "GetValue fault" );
        }
    }
};

/// Stub integer camera parameter.
class CIntegerParameter : public CParameterBase
{
  public:
    int64_t m_value{ 0 }; ///< The current value.

    int64_t m_min{ 0 }; ///< The minimum value.

    int64_t m_max{ 0 }; ///< The maximum value.

    int64_t m_inc{ 1 }; ///< The increment.

    std::vector<int64_t> m_setHistory; ///< Every value set, in order.

    std::function<void( int64_t )> m_onSet; ///< Optional hook called after a successful SetValue.

    /// Get the value.
    int64_t GetValue() const
    {
        checkGet();
        return m_value;
    }

    /// Set the value.
    void SetValue( int64_t value /**< [in] the new value */ )
    {
        checkSet();
        m_value = value;
        m_setHistory.push_back( value );
        ++m_setCalls;
        if( m_onSet )
        {
            m_onSet( value );
        }
    }

    /// Get the minimum value.
    int64_t GetMin() const
    {
        checkGet();
        return m_min;
    }

    /// Get the maximum value.
    int64_t GetMax() const
    {
        checkGet();
        return m_max;
    }

    /// Get the increment.
    int64_t GetInc() const
    {
        checkGet();
        return m_inc;
    }
};

/// Stub float camera parameter.
class CFloatParameter : public CParameterBase
{
  public:
    double m_value{ 0 }; ///< The current value.

    /// Get the value.
    double GetValue() const
    {
        checkGet();
        return m_value;
    }

    /// Set the value.
    void SetValue( double value /**< [in] the new value */ )
    {
        checkSet();
        m_value = value;
        ++m_setCalls;
    }
};

/// Stub boolean camera parameter.
class CBooleanParameter : public CParameterBase
{
  public:
    bool m_value{ true }; ///< The current value.

    /// Get the value.
    bool GetValue() const
    {
        checkGet();
        return m_value;
    }

    /// Set the value.
    void SetValue( bool value /**< [in] the new value */ )
    {
        checkSet();
        m_value = value;
        ++m_setCalls;
    }
};

/// Stub enumeration camera parameter.
template <typename enumT>
class CEnumParameterT : public CParameterBase
{
  public:
    enumT m_value{}; ///< The current value.

    /// Get the value.
    enumT GetValue() const
    {
        checkGet();
        return m_value;
    }

    /// Set the value.
    void SetValue( enumT value /**< [in] the new value */ )
    {
        checkSet();
        m_value = value;
        ++m_setCalls;
    }
};

/// Stub device information.
class CDeviceInfo
{
  public:
    /// Set the serial number.
    CDeviceInfo &SetSerialNumber( const String_t &serial /**< [in] the serial number */ )
    {
        m_serialNumber = serial;
        return *this;
    }

    /// Get the serial number.
    String_t GetSerialNumber() const
    {
        return m_serialNumber;
    }

    /// Set the model name.
    CDeviceInfo &SetModelName( const String_t &model /**< [in] the model name */ )
    {
        m_modelName = model;
        return *this;
    }

    /// Get the model name.
    String_t GetModelName() const
    {
        return m_modelName;
    }

  protected:
    String_t m_serialNumber; ///< The serial number.

    String_t m_modelName; ///< The model name.
};

/// Stub transport layer device.
class IPylonDevice
{
  public:
    CDeviceInfo m_info; ///< The device information.
};

/// Stub transport layer factory.
class CTlFactory
{
  public:
    /// Get the factory singleton.
    static CTlFactory &GetInstance();

    /// Create the first device matching the device info, throws if none is found.
    IPylonDevice *CreateFirstDevice( const CDeviceInfo &di = CDeviceInfo() /**< [in] the device to match */ );
};

/// Stub configuration event handler base.
class CConfigurationEventHandler
{
  public:
    /// Destructor.
    virtual ~CConfigurationEventHandler()
    {
    }
};

/// Stub continuous-acquisition configuration.
class CAcquireContinuousConfiguration : public CConfigurationEventHandler
{
};

/// Configuration registration mode.
enum ERegistrationMode
{
    RegistrationMode_Append,
    RegistrationMode_ReplaceAll
};

/// Ownership of registered objects.
enum ECleanup
{
    Cleanup_None,
    Cleanup_Delete
};

/// Grab strategy.
enum EGrabStrategy
{
    GrabStrategy_OneByOne,
    GrabStrategy_LatestImageOnly,
    GrabStrategy_LatestImages,
    GrabStrategy_UpcomingImage
};

/// Timeout handling of RetrieveResult.
enum ETimeoutHandling
{
    TimeoutHandling_Return,
    TimeoutHandling_ThrowException
};

/// Stub grab result data.
class CGrabResultData
{
  public:
    bool m_succeeded{ true }; ///< Whether the grab succeeded.

    std::vector<int16_t> m_buffer; ///< The image pixels.

    /// Whether the grab succeeded.
    bool GrabSucceeded() const
    {
        return m_succeeded;
    }

    /// Get the image buffer, or nullptr if empty.
    void *GetBuffer() const
    {
        if( m_buffer.size() == 0 )
        {
            return nullptr;
        }

        return const_cast<int16_t *>( m_buffer.data() );
    }
};

/// Stub smart pointer to a grab result.
class CGrabResultPtr
{
  public:
    /// Access the grab result, throws if the pointer is not valid.
    CGrabResultData *operator->() const
    {
        if( !m_ptr )
        {
            throw GenericException( "invalid grab result pointer" );
        }

        return m_ptr.get();
    }

    /// Whether the pointer holds a result.
    bool IsValid() const
    {
        return static_cast<bool>( m_ptr );
    }

    std::shared_ptr<CGrabResultData> m_ptr; ///< The held result.
};

} // namespace Pylon

/// Stand-in for the Basler USB camera parameter enumerations.
namespace Basler_UsbCameraParams
{

/// Exposure auto modes.
enum ExposureAutoEnums
{
    ExposureAuto_Off,
    ExposureAuto_Once,
    ExposureAuto_Continuous
};

/// Pixel formats.
enum PixelFormatEnums
{
    PixelFormat_Mono8,
    PixelFormat_Mono10,
    PixelFormat_Mono12
};

/// Horizontal binning modes.
enum BinningHorizontalModeEnums
{
    BinningHorizontalMode_Sum,
    BinningHorizontalMode_Average
};

/// Vertical binning modes.
enum BinningVerticalModeEnums
{
    BinningVerticalMode_Sum,
    BinningVerticalMode_Average
};

} // namespace Basler_UsbCameraParams

namespace Pylon
{

/// Stub Basler USB instant camera, with only the parameters and methods baslerCtrl uses.
class CBaslerUsbInstantCamera
{
  public:
    /// Construct attached to a device.
    explicit CBaslerUsbInstantCamera( IPylonDevice *pDevice, /**< [in] the device */
                                      ECleanup      cleanupProcedure = Cleanup_Delete /**< [in] device ownership */ );

    /// Destructor.
    ~CBaslerUsbInstantCamera();

    /// Get the device information.
    const CDeviceInfo &GetDeviceInfo() const
    {
        return m_deviceInfo;
    }

    /// Register a configuration event handler.
    void RegisterConfiguration( CConfigurationEventHandler *pConfigurator,   /**< [in] the handler */
                                ERegistrationMode           mode,            /**< [in] the registration mode */
                                ECleanup                    cleanupProcedure /**< [in] handler ownership */
    );

    /// Open the camera.
    void Open();

    /// Close the camera.
    void Close();

    /// Start grabbing.
    void StartGrabbing( EGrabStrategy strategy /**< [in] the grab strategy */ );

    /// Stop grabbing.
    void StopGrabbing();

    /// Retrieve the next grab result.
    bool RetrieveResult( unsigned int     timeoutMs,      /**< [in] the timeout in ms */
                         CGrabResultPtr  &grabResult,     /**< [out] the grab result */
                         ETimeoutHandling timeoutHandling /**< [in] timeout handling */
    );

    CDeviceInfo m_deviceInfo; ///< The device information.

    CEnumParameterT<Basler_UsbCameraParams::ExposureAutoEnums> ExposureAuto; ///< Exposure auto mode.

    CEnumParameterT<Basler_UsbCameraParams::PixelFormatEnums> PixelFormat; ///< Pixel format.

    CEnumParameterT<Basler_UsbCameraParams::BinningHorizontalModeEnums>
        BinningHorizontalMode; ///< Horizontal binning mode.

    CEnumParameterT<Basler_UsbCameraParams::BinningVerticalModeEnums> BinningVerticalMode; ///< Vertical binning mode.

    CIntegerParameter BinningHorizontal; ///< Horizontal binning.

    CIntegerParameter BinningVertical; ///< Vertical binning.

    CIntegerParameter OffsetX; ///< ROI x offset.

    CIntegerParameter OffsetY; ///< ROI y offset.

    CIntegerParameter Width; ///< ROI width.

    CIntegerParameter Height; ///< ROI height.

    CIntegerParameter SensorWidth; ///< Sensor width.

    CIntegerParameter SensorHeight; ///< Sensor height.

    CBooleanParameter CenterX; ///< Auto-center the ROI in x.

    CBooleanParameter CenterY; ///< Auto-center the ROI in y.

    CFloatParameter DeviceTemperature; ///< Device temperature, in C.

    CFloatParameter ExposureTime; ///< Exposure time, in microseconds.

    CFloatParameter ResultingFrameRate; ///< Resulting frame rate, in Hz.

    CBooleanParameter AcquisitionFrameRateEnable; ///< Enable the acquisition frame rate limit.

    CFloatParameter AcquisitionFrameRate; ///< Acquisition frame rate limit, in Hz.
};

/// Initialize the Pylon runtime.
void PylonInitialize();

/// Terminate the Pylon runtime.
void PylonTerminate();

} // namespace Pylon

/// Stand-in for the GenApi namespace.
namespace GenApi
{

/// Whether a camera parameter is writable.
inline bool IsWritable( const Pylon::CParameterBase &p /**< [in] the parameter */ )
{
    return p.m_writable;
}

} // namespace GenApi

#endif // baslerCtrl_tests_stubs_pylon_PylonIncludes_h
