/** \file fsmCtrl.hpp
  * \brief The MagAO-X XXXXXX header file
  *
  * \ingroup fsmCtrl_files
  */

#ifndef fsmCtrl_hpp
#define fsmCtrl_hpp

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include <string.h>
#include <stdio.h>

#include <iostream>
using namespace std;

#include <pthread.h>

typedef MagAOX::app::MagAOXApp<true> MagAOXAppT; //This needs to be before the other header files for logging to work

#include "binaryUart.hpp"
#include "cGraphPacket.hpp"
#include "linux_pinout_client_socket.hpp"
#include "socket.hpp"
#include "fsmCommands.hpp"

/** \defgroup fsmCtrl
  * \brief The XXXXXX application to do YYYYYYY
  *
  * <a href="../handbook/operating/software/apps/XXXXXX.html">Application Documentation</a>
  *
  * \ingroup apps
  *
  */

/** \defgroup fsmCtrl_files
  * \ingroup fsmCtrl
  */

namespace MagAOX
{
namespace app
{

/// The MagAO-X xxxxxxxx
/**
  * \ingroup fsmCtrl
  */
class fsmCtrl : public MagAOXApp<true>, public dev::telemeter<fsmCtrl>
// class fsmCtrl : public MagAOXApp<true>
{

  //Give the test harness access.
  friend class fsmCtrl_test;

  friend class dev::telemeter<fsmCtrl>;
  typedef dev::telemeter<fsmCtrl> telemeterT;

protected:

   /** \name Configurable Parameters
     *@{
     */
  std::string type;
  std::string PortName;
  int nHostPort;
  int period_s;
   //here add parameters which will be config-able at runtime
   ///@}

  uint16_t CGraphPayloadType;
  char Buffer[4096];
  CGraphPacket SocketProtocol;
  linux_pinout_client_socket LocalPortPinout;
  BinaryUart UartParser;
  StatusQuery statusQuery;
  AdcsQuery adcsQuery;
  DacsQuery dacsQuery;

public:
   /// Default c'tor.
   fsmCtrl();

   /// D'tor, declared and defined for noexcept.
   ~fsmCtrl() noexcept
   {}

   virtual void setupConfig();

   /// Implementation of loadConfig logic, separated for testing.
   /** This is called by loadConfig().
     */
   int loadConfigImpl( mx::app::appConfigurator & _config /**< [in] an application configuration from which to load values*/);

   virtual void loadConfig();

   /// Startup function
   /**
     *
     */
   virtual int appStartup();

   /// Implementation of the FSM for fsmCtrl.
   /**
     * \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
   virtual int appLogic();

   /// Shutdown the app.
   /**
     *
     */
   virtual int appShutdown();

   /// TODO: Test the connection to the device
   int testConnection();

   /// Connect to fsm via Socket
   /**
     * 
     * \returns 0 if connection successful
     * \returns -1 on an error
     */ 
   int socketConnect();

   /// Query fsm status
   void queryStatus();
   
   /// Query fsm's ADCs   
   void queryAdcs();
   
   /// Query fsm's DACs   
   void queryDacs();

   void query(PZTQuery&);

   /** \name Telemeter Interface
     *
     * @{
     */
   int checkRecordTimes();

   int recordTelem( const telem_fsm * );

   int recordFsm( bool force = false );
   ///@}

};

fsmCtrl::fsmCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED), UartParser(LocalPortPinout, SocketProtocol, PacketCallbacks, false)
{
   m_powerMgtEnabled = true;  
   return;
}

void fsmCtrl::setupConfig()
{
  config.add("parameters.connection_type", "", "parameters.connection_type", argType::Required, "parameters", "connection_type", false, "string", "The type of connection: serial_port or socket.");
  config.add("parameters.period_s", "", "parameters.period_s", argType::Required, "parameters", "period_s", false, "int", "The period of status queries to the fsm.");

  config.add("socket.client_entrance_ip", "", "socket.client_entrance_ip", argType::Required, "socket", "client_entrance_ip", false, "string", "The IP address on the client machine that the tunnel is set up from.");
  config.add("socket.host_port", "", "socket.host_port", argType::Required, "socket", "host_port", false, "int", "The port at which the fsm driver is listening for connections.");

  telemeterT::setupConfig(config);
}

int fsmCtrl::loadConfigImpl( mx::app::appConfigurator & _config )
{
  _config(type, "parameters.connection_type");
  _config(period_s, "parameters.period_s");

  _config(PortName, "socket.client_entrance_ip");
	_config(nHostPort, "socket.host_port");

  return 0;
}

void fsmCtrl::loadConfig()
{
  if( loadConfigImpl(config) < 0)
  {
    log<text_log>("Error during config", logPrio::LOG_CRITICAL);
    m_shutdown = true;
  }

  if(telemeterT::loadConfig(config) < 0)
  {
    log<text_log>("Error during telemeter config", logPrio::LOG_CRITICAL);
    m_shutdown = true;
  }
}

int fsmCtrl::appStartup()
{
  if(telemeterT::appStartup() < 0)
  {
    return log<software_error, -1>({__FILE__,__LINE__});
  }

  return 0;
}

int fsmCtrl::appLogic()
{
  if(state() == stateCodes::POWERON)
  {
    if(!powerOnWaitElapsed()) 
    {
      return 0;
    }
    else
    {
      state(stateCodes::NOTCONNECTED);
    }  
  }

  if( state() == stateCodes::NOTCONNECTED)
  {
    int rv;
    rv = socketConnect();

    if (rv==0)
    {
      state(stateCodes::CONNECTED);
    }  
  }

  if( state() == stateCodes::CONNECTED)
  {
    queryStatus();

    queryAdcs();   

    queryDacs();   

    if(telemeterT::appLogic() < 0)
    {
        log<software_error>({__FILE__, __LINE__});
        return 0;
    }
  }

  return 0;
}

int fsmCtrl::appShutdown()
{
  telemeterT::appShutdown();

  return 0;
}

/// TODO: Test the connection to the device
int fsmCtrl::testConnection()
{
  return 0;
}

int fsmCtrl::socketConnect()
{
  //Tell C lib (stdio.h) not to buffer output, so we can ditch all the fflush(stdout) calls...
	setvbuf(stdout, NULL, _IONBF, 0);

  log<text_log>("Welcome to SerialPortBinaryCmdr!");
  log<text_log>("In order to tunnel to the lab use the following command before running this program: \"ssh -L 1337:localhost:1337 -N -f fsm\" (where fsm is the ssh alias of the remote server)!");

  int err = fsmCtrl::LocalPortPinout.init(nHostPort, PortName.c_str());
  if (IUart::IUartOK != err)
  {
    log<software_error, -1>({__FILE__, __LINE__, errno, "SerialPortBinaryCmdr: can't open socket (" + PortName + ":" + std::to_string(nHostPort) + "), exiting.\n"});
    return -1;
  }

  log<text_log>("Starting to do something");

	UartParser.Debug(false);
  return 0;
}


// Function to request fsm Status 
void fsmCtrl::queryStatus() {
  log<text_log>(statusQuery.startLog);  
  query(statusQuery);
  log<text_log>(statusQuery.endLog);
  recordFsm(false);

  // statusQuery.logReply();
}

// Function to request fsm ADCs
void fsmCtrl::queryAdcs() {
  log<text_log>(adcsQuery.startLog); 
  query(adcsQuery);  
  log<text_log>(adcsQuery.endLog);

  adcsQuery.logReply();
}

// Function to request fsm DACs
void fsmCtrl::queryDacs() {
  log<text_log>(dacsQuery.startLog); 
  query(dacsQuery);  
  log<text_log>(dacsQuery.endLog);

  dacsQuery.logReply();
}

void fsmCtrl::query(PZTQuery& pztQuery) 
{
  // Send command packet
  (&UartParser)->TxBinaryPacket(CGraphPayloadType, NULL, 0);

  // Allow time for fsm to respond, it's not instantaneous
  sleep(3);

  // The packet is read byte by byte, so keep going while there are bytes left
  bool Bored = false;
  while (!Bored) {
      Bored = true;
      if (UartParser.Process(pztQuery))
      {
        Bored = false;
      }

      if (false == LocalPortPinout.connected())
      {
          int err = LocalPortPinout.init(nHostPort, PortName.c_str());
          if (IUart::IUartOK != err)
          {
            log<software_error>({__FILE__, __LINE__, errno, "SerialPortBinaryCmdr: can't open socket (" + PortName + ":" + std::to_string(nHostPort)});            
          }
      }
  }
}

int fsmCtrl::checkRecordTimes()
{
  return telemeterT::checkRecordTimes(telem_fsm());
}

int fsmCtrl::recordTelem(const telem_fsm *)
{
  return recordFsm(true);
}

int fsmCtrl::recordFsm( bool force )
{
  static CGraphPZTStatusPayload LastStatus; ///< Structure holding the previous fsm voltage measurement.

  if( !(LastStatus == statusQuery.Status) || force )
  {
    LastStatus = statusQuery.Status;
    telem<telem_fsm>({LastStatus.P1V2, LastStatus.P2V2, LastStatus.P24V, LastStatus.P2V5, LastStatus.P3V3A, LastStatus.P6V, LastStatus.P5V, LastStatus.P3V3D, LastStatus.P4V3, LastStatus.N5V, LastStatus.N6V, LastStatus.P150V});
  }

  return 0;
}


} //namespace app
} //namespace MagAOX

#endif //fsmCtrl_hpp
