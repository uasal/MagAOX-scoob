/** \file BrainStem-all.h
 * \brief Test stub for the Acroname BrainStem2 C++ API, declaring only what acronameUsbHub uses.
 * \author Claude Code
 *
 * This shadows `libs/BrainStem2/BrainStem2/BrainStem-all.h`, which the app includes as
 * `"BrainStem2/BrainStem-all.h"`.  The test build does not add `-I../../libs/BrainStem2/` and does not
 * link `libBrainStem2.a`, so this header is found on the `apps/acronameUsbHub/tests/stubs/` include path instead.
 *
 * The enum values match the real SDK.  The member function bodies, and the fake hub state they act on,
 * are defined in `acronameUsbHub_test.cpp`.
 *
 * \ingroup acronameUsbHub_files
 */

#ifndef acronameUsbHub_tests_stubs_BrainStem_all_h
#define acronameUsbHub_tests_stubs_BrainStem_all_h

#include <cstddef>
#include <cstdint>

/// Error codes returned by BrainStem library calls (subset, with the real SDK values).
typedef enum
{
    aErrNone       = 0,  ///< Success, no error.
    aErrParam      = 2,  ///< Invalid parameter.
    aErrNotFound   = 3,  ///< Not found.
    aErrBusy       = 5,  ///< Resource busy.
    aErrIO         = 6,  ///< Input/Output error.
    aErrTimeout    = 18, ///< Timeout occurred.
    aErrConnection = 25, ///< Connection error.
    aErrUnknown    = 33  ///< Unknown error.
} aErr;

/// The connection transport type.
typedef enum
{
    INVALID, ///< Undefined link type.
    USB,     ///< USB link type.
    TCPIP    ///< TCP/IP link type.
} linkType;

/// The module type number of the USBHub3p, as in `aProtocoldefs.h`.
#define aMODULE_TYPE_USBHub3p 19

/// The number of downstream USB ports on the USBHub3p.
#define aUSBHUB3P_NUM_USB_PORTS 8

extern "C"
{
    /// Get the model name for a module type number.
    /**
     * \returns a pointer to a static string with the model name
     */
    const char *aDefs_GetModelName( const int modelNum /**< [in] the module type number */ );

    /// Format a firmware build number as a version string.
    void aVersion_ParseString( uint32_t build,  /**< [in] the packed firmware version */
                               char    *string, /**< [out] the buffer to write the version string into */
                               size_t   len /**< [in] the size of the buffer */ );
}

namespace Acroname
{
namespace BrainStem
{

/// A BrainStem module, which holds the link to the device.
class Module
{
  public:
    /// Connect to a link module by transport type and serial number.
    /**
     * \returns aErrNone on success, or an error code
     */
    aErr connect( const linkType type, /**< [in] the transport to use */
                  const uint32_t serialNum /**< [in] the serial number of the module, 0 for the first found */ );

    /// Is the link connected to the BrainStem module.
    /**
     * \returns true if connected
     */
    bool isConnected( void );

    /// Disconnect from the BrainStem module.
    /**
     * \returns aErrNone on success, or an error code
     */
    aErr disconnect( void );
};

/// The system entity of a module (model, version, serial number).
class SystemClass
{
  public:
    /// Initialize the entity.
    void init( Module       *pModule, /**< [in] the module to which this entity belongs */
               const uint8_t index /**< [in] not used, always 0 */ );

    /// Get the module type number.
    /**
     * \returns aErrNone on success, or an error code
     */
    aErr getModel( uint8_t *model /**< [out] the module type number */ );

    /// Get the packed firmware version.
    /**
     * \returns aErrNone on success, or an error code
     */
    aErr getVersion( uint32_t *build /**< [out] the packed firmware version */ );

    /// Get the module serial number.
    /**
     * \returns aErrNone on success, or an error code
     */
    aErr getSerialNumber( uint32_t *serialNumber /**< [out] the serial number */ );
};

/// The USB entity of a hub, controlling the downstream ports.
class USBClass
{
  public:
    /// Enable both power and data lines of a port.
    /**
     * \returns aErrNone on success, or an error code
     */
    aErr setPortEnable( const uint8_t channel /**< [in] the port number */ );

    /// Disable both power and data lines of a port.
    /**
     * \returns aErrNone on success, or an error code
     */
    aErr setPortDisable( const uint8_t channel /**< [in] the port number */ );

    /// Get the port state bit field (bit 0 is VBUS enabled).
    /**
     * \returns aErrNone on success, or an error code
     */
    aErr getPortState( const uint8_t channel, /**< [in] the port number */
                       uint32_t     *state /**< [out] the port state bit field */ );
};

} // namespace BrainStem
} // namespace Acroname

using Acroname::BrainStem::Module;
using Acroname::BrainStem::SystemClass;
using Acroname::BrainStem::USBClass;

/// The Acroname USBHub3p module.
class aUSBHub3p : public Module
{
  public:
    SystemClass system; ///< The system entity.

    USBClass usb; ///< The USB entity, which controls the 8 downstream ports.
};

#endif // acronameUsbHub_tests_stubs_BrainStem_all_h
