/** \file summerDevice.hpp
 * \brief The generic interface for Summer-made electronics.
 *
 * \author Summer Franks
 * \author Irina Stefan
 *
 * \ingroup app_files
 */

#ifndef summerDevice_hpp
#define summerDevice_hpp

#include <ImageStreamIO/ImageStruct.h>
#include <ImageStreamIO/ImageStreamIO.h>

#include "../../libMagAOX/common/paths.hpp"

#include <memory> // For std::unique_pointer 
#include <vector> // For std::vector
using namespace std;

// typedef MagAOX::app::MagAOXApp<true> MagAOXAppT; // This needs to be before the other header files for logging to work in other headers

#include "summerDeviceUtils/commands.hpp"
#include "summerDeviceUtils/binaryUart.hpp"
#include "summerDeviceUtils/cGraphPacket.hpp"
#include "summerDeviceUtils/linux_pinout_client_socket.hpp"
#include "summerDeviceUtils/linux_pinout_uart.hpp"
#include "summerDeviceUtils/socket.hpp"
#include "summerDeviceUtils/IUart.h"

namespace MagAOX
{
namespace app
{
namespace dev
{


/** Generic interface for Summer-made electronics.
  *
  *
  * The derived class `derivedT` has the following requirements:
  * 
  * - Must be derived from MagAOXApp<true>
  * 
  * - Must include the following friend declaration:
  *   \code
  *       friend class dev::summerDevice<DERIVEDNAME>; //replace DERIVEDNAME with derivedT class name
  *   \endcode
  * 
  * - Must include the following typedef:
  *   \code
  *       typedef dev::summerDevice<DERIVEDNAME> summerDeviceT; //replace DERIVEDNAME with derivedT class name
  *   \endcode
  *
  * - Must call this class's setupConfig(), loadConfig(), appStartup(), appLogic(), and appShutdown() 
  *   in the corresponding function of `derivedT`, with error checking. 
  *   For convenience the following macros are defined to provide error checking:
  *   \code  
  *       TELEMETER_SETUP_CONFIG( cfig )
  *       TELEMETER_LOAD_CONFIG( cfig )
  *       TELEMETER_APP_STARTUP
  *       TELEMETER_APP_LOGIC
  *       TELEMETER_APP_SHUTDOWN
  *   \endcode
  * 
  * - Must define its own queries of type sdevQuery
  * 
  * This interface defines the following functions with default implementation for use:
  *   connect()
  *   testConnection()
  *   query(sdevQuery *)
  *   receive()
  * Each can be re-implemented in derived classes as required.
  *
  * \ingroup appdev
  */
template <class derivedT>
class summerDevice
{

protected:
    /** \name Configurable Parameters
     * @{
     */
    std::string m_connectionType = "serial"; ///< The type of connection: serial or socket
    std::string m_portName = "/dev/ttyUSB0"; ///< For serial connections: the port on which the connection is made
    uint32_t m_baudRate = 115200; //< For serial connections: the port baud rate
    std::string m_ipName = "127.0.0.1"; ///< For socket connections: The ip address on which the connection is made
    int m_hostPort = 66873; ///< For socket connections: The port on which the connection is made; 65536 + 1337
    ///@}

public:
    char Buffer[4096];
    CGraphPacket PacketProtocol;
    SocketBinaryUartCallbacks PacketCallbacks;
    std::unique_ptr<IUart> LocalPortPinout;
    std::unique_ptr<BinaryUart> UartParser;

    const std::string &connectionType() const;

    const std::string &portName() const;

    const uint32_t &baudRate() const;

    const std::string &ipName() const;

    const int &hostPort() const;

    /// Setup the configuration system
    /**
      * This should be called in `derivedT::setupConfig` as
      * \code
        summerDevice<derivedT>::setupConfig(config);
        \endcode
      * with appropriate error checking.
      */
    int setupConfig(mx::app::appConfigurator &config /**< [out] the derived classes configurator*/);

    /// load the configuration system results
    /**
      * This should be called in `derivedT::loadConfig` as
      * \code
        summerDevice<derivedT>::loadConfig(config);
        \endcode
      * with appropriate error checking.
      */
    int loadConfig(mx::app::appConfigurator &config /**< [in] the derived classes configurator*/);

    /// Startup function
    /** Starts the summerDevice thread
      * This should be called in `derivedT::appStartup` as
      * \code
        summerDevice<derivedT>::appStartup();
        \endcode
      * with appropriate error checking.
      *
      * \returns 0 on success
      * \returns -1 on error, which is logged.
      */
    int appStartup();

    /// Checks the summerDevice thread
    /** This should be called in `derivedT::appLogic` as
      * \code
        summerDevice<derivedT>::appLogic();
        \endcode
      * with appropriate error checking.
      *
      * \returns 0 on success
      * \returns -1 on error, which is logged.
      */
    int appLogic();

    /// Shuts down the summerDevice thread
    /** This should be called in `derivedT::appShutdown` as
      * \code
        summerDevice<derivedT>::appShutdown();
        \endcode
      * with appropriate error checking.
      *
      * \returns 0 on success
      * \returns -1 on error, which is logged.
      */
    int appShutdown();

    /// Return queries variable. Will need to be overriden in classes implementing the template
    /** This should be called in `derivedT::appShutdown` as
      * \returns std::vector<sdevQuery*>&
      */
    virtual const std::vector<sdevQuery*>& getQueries() const {
        return queries;
    }

protected:
    std::vector<sdevQuery*> queries = {};

    /// Initialize UartParser
    /**
     *
     * \returns 0 if UartParser successfully initialized
     * \returns -1 on an error
     */
    int initUartParser();

    /// Connect to fsm via Socket
    /**
     *
     * \returns 0 if connection successful
     * \returns -1 on an error
     */
    int socketConnect();

    /// Connect to fsm via Serial Port
    /**
     *
     * \returns 0 if connection successful
     * \returns -1 on an error
     */
    int serialPortConnect();

public:
    /**
     * @brief Connect to device
     *
     * \returns 0 if connection successful
     * \returns -1 on an error
     */
    virtual int connect();

    /// TODO:
    /**
     * @brief Test connection to the device
     *
     * \returns 0 if connected
     * \returns -1 if not connected
     */
    virtual int testConnection();

    /**
     * @brief Query interface for the device
     *
     * Function that sends a command packet to the device.
     *
     * @param pztQuery pointer to a class inheriting from PZTQuery (see fsmCommands.hpp)
     */
    virtual void query(sdevQuery *);

    /**
     * @brief Function that listens for responses from the fsm
     *
     * Function that checks for a response from the fsm and processes it.
     * If a response is received it processes the response as appropriate for the
     * command sent.
     *
     * @param pztQuery pointer to a class inheriting from PZTQuery (see fsmCommands.hpp)
     */
    virtual void receive();

    ///@}

    /** \name INDI
     *
     *@{
     */
protected:
    // declare our properties

    // pcf::IndiProperty m_indiP_shmimName; ///< Property used to report the shmim buffer name

    // pcf::IndiProperty m_indiP_frameSize; ///< Property used to report the current frame size

public:
    /// Update the INDI properties for this device controller
    /** You should call this once per main loop.
     * It is not called automatically.
     *
     * \returns 0 on success.
     * \returns -1 on error.
     */
    int updateINDI();

    ///@}

private:
    derivedT &derived()
    {
        return *static_cast<derivedT *>(this);
    }
};

template <class derivedT>
const std::string & summerDevice<derivedT>::connectionType() const
{
    return m_connectionType;
}

template <class derivedT>
const std::string & summerDevice<derivedT>::portName() const
{
    return m_portName;
}

template <class derivedT>
const uint32_t & summerDevice<derivedT>::baudRate() const
{
    return m_baudRate;
}

template <class derivedT>
const std::string & summerDevice<derivedT>::ipName() const
{
    return m_ipName;
}

template <class derivedT>
const int & summerDevice<derivedT>::hostPort() const
{
    return m_hostPort;
}

template <class derivedT>
int summerDevice<derivedT>::setupConfig(mx::app::appConfigurator &config)
{
  config.add("sdev.connection_type", "", "sdev.connection_type", argType::Required, "sdev", "connection_type", false, "string", "The type of connection: serial_port or socket.");
  config.add("sdev.port_address", "", "sdev.port_address", argType::Optional, "sdev", "port_address", false, "string", "The address where the client machine is connected to.");
  config.add("sdev.baud_rate", "", "sdev.baud_rate", argType::Optional, "sdev", "baud_rate", false, "int", "The baud rate for the serial port.");
  config.add("sdev.client_entrance_ip", "", "sdev.client_entrance_ip", argType::Optional, "sdev", "client_entrance_ip", false, "string", "The IP address on the client machine that the tunnel is set up from.");
  config.add("sdev.host_port", "", "sdev.host_port", argType::Optional, "sdev", "host_port", false, "int", "The port at which the fsm driver is listening for connections.");

  return 0;
}

template <class derivedT>
int summerDevice<derivedT>::loadConfig(mx::app::appConfigurator &config)
{
  derivedT::template log<software_info>({__FILE__, __LINE__, "Sdev loading config"});
  config(m_connectionType, "sdev.connection_type");

  if (m_connectionType == "socket")
  {
    config(m_ipName, "sdev.client_entrance_ip");
    config(m_hostPort, "sdev.host_port");

    LocalPortPinout = std::make_unique<linux_pinout_client_socket>();
  }
  else if (m_connectionType == "serial_port")
  {
    config(m_portName, "sdev.port_address");
    config(m_baudRate, "sdev.baud_rate");

    LocalPortPinout = std::make_unique<linux_pinout_uart>();
  }

  // Since LocalPortPinout is now initialized, can also initialize UartParser
  return initUartParser();
}

template <class derivedT>
int summerDevice<derivedT>::appStartup()
{
    // // Register the shmimName INDI property
    // m_indiP_shmimName = pcf::IndiProperty(pcf::IndiProperty::Text);
    // m_indiP_shmimName.setDevice(derived().configName());
    // m_indiP_shmimName.setName(specificT::indiPrefix() + "_shmimName");
    // m_indiP_shmimName.setPerm(pcf::IndiProperty::ReadOnly);
    // m_indiP_shmimName.setState(pcf::IndiProperty::Idle);
    // m_indiP_shmimName.add(pcf::IndiElement("name"));
    // m_indiP_shmimName["name"] = m_shmimName;

    return 0;
}

template <class derivedT>
int summerDevice<derivedT>::appLogic()
{
    return 0;
}

template <class derivedT>
int summerDevice<derivedT>::appShutdown()
{
    return 0;
}

template <class derivedT>
int summerDevice<derivedT>::updateINDI()
{
    if (!derived().m_indiDriver)
        return 0;

    return 0;
}


//////////////
// CONNECTION
//////////////

template <class derivedT>
int summerDevice<derivedT>::initUartParser()
{
  try
  {
    UartParser = std::make_unique<BinaryUart>(*LocalPortPinout, PacketProtocol, PacketCallbacks, getQueries(), false);
    return 0;
  }
  catch (...)
  {
    derivedT::template log<software_error>({__FILE__, __LINE__});
    return -1;
  }

}

template <class derivedT>
int summerDevice<derivedT>::socketConnect()
{
  PinoutConfig pinoutConfig = PinoutConfig::CreateSocketConfig(m_hostPort, m_portName.c_str());
  int err = LocalPortPinout->init(pinoutConfig);
  if (IUart::IUartOK != err)
  {
    derivedT::template log<software_error, -1>({__FILE__, __LINE__, errno, "SerialPortBinaryCmdr: can't open socket (" + m_portName + ":" + std::to_string(m_hostPort) + "), exiting.\n"});
    return -1;
  }

  derivedT::template log<software_info>({__FILE__, __LINE__, "Connected to socket (" + m_portName + ":" + std::to_string(m_hostPort) + ")"});
  return 0;
}

template <class derivedT>
int summerDevice<derivedT>::serialPortConnect()
{
  PinoutConfig pinoutConfig = PinoutConfig::CreateSerialConfig(m_baudRate, m_portName.c_str());
  int err = LocalPortPinout->init(pinoutConfig);
  if (IUart::IUartOK != err)
  {
    derivedT::template log<software_error, -1>({__FILE__, __LINE__, errno, "SerialPortBinaryCmdr: can't open port (" + m_portName + ":" + std::to_string(m_baudRate) + "), exiting.\n"});
    return -1;
  }

  derivedT::template log<software_info>({__FILE__, __LINE__, "Connected to port (" + m_portName + ":" + std::to_string(m_baudRate) + ")"});
  return 0;
}


template <class derivedT>
int summerDevice<derivedT>::connect()
{
  int rv = -1;
  
  if (m_connectionType == "socket")
  {
    rv = socketConnect();
  } 
  else if (m_connectionType == "serial_port")
  {
    rv = serialPortConnect();
  }

  return rv;
}


/// TODO: Test the connection to the device
template <class derivedT>
int summerDevice<derivedT>::testConnection()
{
  return 0;
}

template <class derivedT>
void summerDevice<derivedT>::query(sdevQuery *Query)
{
  // derivedT::template log<text_log>(Query->startLog);
  // Send command packet
  UartParser->TxBinaryPacket(Query->getPayloadType(), Query->getPayloadData(), Query->getPayloadLen());
  // debug
  derivedT::template log<software_debug>({__FILE__, __LINE__, Query->endLog});
}

template <class derivedT>
void summerDevice<derivedT>::receive() {
  // The packet is read byte by byte, so keep going while there are bytes left
  bool Bored = false;
  while (!Bored)
  {
    Bored = true;
    if (UartParser->ProcessBulk())
    {
      Bored = false;
    }

    if (false == LocalPortPinout->isopen())
    {
      connect();
    }
  }
}


/// Call summerDeviceT::setupConfig with error checking for summerDevice
/**
  * \param cfig the application configurator 
  */
#define SUMMERDEVICE_SETUP_CONFIG( cfig )                                                   \
    if(summerDeviceT::setupConfig(cfig) < 0)                                                \
    {                                                                                       \
        log<software_error>({__FILE__, __LINE__, "Error from summerDeviceT::setupConfig"}); \
        m_shutdown = true;                                                                  \
        return;                                                                             \
    }

/// Call summerDeviceT::loadConfig with error checking for summerDevice
/** This must be inside a function that returns int, e.g. the standard loadConfigImpl.
  * \param cfig the application configurator 
  */
#define SUMMERDEVICE_LOAD_CONFIG( cfig )                                                             \
    if(summerDeviceT::loadConfig(cfig) < 0)                                                          \
    {                                                                                                \
        return log<software_error,-1>({__FILE__, __LINE__, "Error from summerDeviceT::loadConfig"}); \
    }

/// Call summerDeviceT::appStartup with error checking for summerDevice
#define SUMMERDEVICE_APP_STARTUP                                                                     \
    if(summerDeviceT::appStartup() < 0)                                                              \
    {                                                                                                \
        return log<software_error,-1>({__FILE__, __LINE__, "Error from summerDeviceT::appStartup"}); \
    }

/// Call summerDeviceT::appLogic with error checking for summerDevice
#define SUMMERDEVICE_APP_LOGIC                                                                     \
    if(summerDeviceT::appLogic() < 0)                                                              \
    {                                                                                              \
        return log<software_error,-1>({__FILE__, __LINE__, "Error from summerDeviceT::appLogic"}); \
    }

/// Call summerDeviceT::updateINDI with error checking for summerDevice
#define SUMMERDEVICE_UPDATE_INDI                                                                     \
    if(summerDeviceT::updateINDI() < 0)                                                              \
    {                                                                                                \
        return log<software_error,-1>({__FILE__, __LINE__, "Error from summerDeviceT::updateINDI"}); \
    }

/// Call summerDeviceT::appShutdown with error checking for summerDevice
#define SUMMERDEVICE_APP_SHUTDOWN                                                                     \
    if(summerDeviceT::appShutdown() < 0)                                                              \
    {                                                                                                 \
        return log<software_error,-1>({__FILE__, __LINE__, "Error from summerDeviceT::appShutdown"}); \
    }

} // namespace dev
} // namespace app
} // namespace MagAOX
#endif
