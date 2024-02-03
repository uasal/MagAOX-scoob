/** \file fsmCtrl.hpp
  * \brief The MagAO-X XXXXXX header file
  *
  * \ingroup fsmCtrl_files
  */

#pragma once

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include <string.h>
#include <stdio.h>

#include <iostream>
using namespace std;

#include <pthread.h>

typedef MagAOX::app::MagAOXApp<true> MagAOXAppT; //This needs to be before the other header files for logging to work

#include "fsmCommands.hpp"
#include "binaryUart.hpp"
#include "cGraphPacket.hpp"
#include "linux_pinout_client_socket.hpp"
#include "socket.hpp"

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
class fsmCtrl : public MagAOXApp<true>, public dev::telemeter<fsmCtrl>,  public dev::shmimMonitor<fsmCtrl>
{

  //Give the test harness access.
  friend class fsmCtrl_test;

  friend class dev::telemeter<fsmCtrl>;
  typedef dev::telemeter<fsmCtrl> telemeterT;

  friend class dev::shmimMonitor<fsmCtrl>;

protected:

   /** \name Configurable Parameters
     *@{
     */
  std::string type;
  std::string PortName;
  int nHostPort;
  int period_s;

  std::string m_shmimName; ///< The name of the shmim stream to write to.

  uint32_t m_fsmWidth {0}; ///< The width of the images in the stream
  uint32_t m_fsmHeight {0}; ///< The height of the images in the stream 

   //here add parameters which will be config-able at runtime
   ///@}

  // uint16_t CGraphPayloadType;
  char Buffer[4096];
  CGraphPacket SocketProtocol;
  linux_pinout_client_socket LocalPortPinout;
  BinaryUart UartParser;
  PZTQuery* statusQuery = new StatusQuery();
  PZTQuery* adcsQuery = new AdcsQuery();
  PZTQuery* dacsQuery = new DacsQuery();
  uint32_t targetSetpoints[3];

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

   /// Set fsm's DACs   
  //  void setDacs();
   void setDacs(uint32_t*);

   void query(PZTQuery*);

   /** \name Telemeter Interface
     *
     * @{
     */
   int checkRecordTimes();

   int recordTelem( const telem_fsm * );

   int recordFsm( bool force = false );
   ///@}

   /** \name shmim Monitor Interface
     *
     * @{
     */

   /// Called after shmimMonitor connects to the dmXXdisp stream.  Checks for proper size.
   /**
     * \returns 0 on success
     * \returns -1 if incorrect size or data type in stream.
     */    
   int allocate(const dev::shmimT & sp);

   /// Called by shmimMonitor when a new fsm command is available.
   int processImage( void * curr_src,
                     const dev::shmimT & sp
                   );

   int commandFSM(void * curr_src);
   ///@}

protected:
   //declare our properties
   pcf::IndiProperty m_dacs;

public:
   INDI_NEWCALLBACK_DECL(fsmCtrl, m_dacs);
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

  config.add("fsm.shmim_name", "", "fsm.shmim_name", argType::Required, "fsm", "shmim_name", false, "string", "The name of the ImageStreamIO shared memory image to monitor for FSM commands. Will be used as /tmp/<shmimName>.im.shm.");

  config.add("fsm.width", "", "fsm.width", argType::Required, "fsm", "width", false, "string", "The width of the FSM in actuators.");
  config.add("fsm.height", "", "fsm.height", argType::Required, "fsm", "height", false, "string", "The height of the FSM in actuators.");

  telemeterT::setupConfig(config);
}

int fsmCtrl::loadConfigImpl( mx::app::appConfigurator & _config )
{
  _config(type, "parameters.connection_type");
  _config(period_s, "parameters.period_s");

  _config(PortName, "socket.client_entrance_ip");
	_config(nHostPort, "socket.host_port");

  _config(m_shmimName, "fsm.shmim_name");

  _config(m_fsmWidth, "fsm.width");
  _config(m_fsmHeight, "fsm.height");

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

   shmimMonitor<fsmCtrl>::appStartup();

   // set up the  INDI properties
   REG_INDI_NEWPROP(m_dacs, "dacs", pcf::IndiProperty::Number);
   m_dacs.add (pcf::IndiElement("one"));
   m_dacs["one"] = 0;
   m_dacs.add (pcf::IndiElement("two"));
   m_dacs["two"] = 0;
   m_dacs.add (pcf::IndiElement("three"));
   m_dacs["three"] = 0;

  return 0;
}

int fsmCtrl::appLogic()
{

   shmimMonitor<fsmCtrl>::appLogic();

  if(state() == stateCodes::POWERON)
  {

    std::ostringstream oss;
    oss << "State is: " << state();
    log<text_log>(oss.str());
    
    if(!powerOnWaitElapsed()) 
    {
      return 0;
    }
    state(stateCodes::NOTCONNECTED);
  }

  if( state() == stateCodes::NOTCONNECTED)
  {

    std::ostringstream oss;
    oss << "State is: " << state();
    log<text_log>(oss.str());

    int rv;
    rv = socketConnect();
    // rv = 0;

    if (rv==0)
    {
      state(stateCodes::CONNECTED);
    }  
  }

  if( state() == stateCodes::CONNECTED)
  {
    std::ostringstream oss;
    oss << "State is: " << state();
    log<text_log>(oss.str());

    // queryStatus();

    // queryAdcs();   

    // queryDacs();   

    log<text_log>("Doing something");

    sleep(10);

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
  shmimMonitor<fsmCtrl>::appShutdown();  

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
  log<text_log>("In order to tunnel to the lab use the following command before running this program:\n ssh -L 1337:localhost:1337 -N -f fsm \n(where fsm is the ssh alias of the remote server)!");

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
  log<text_log>(statusQuery->startLog);  
  query(statusQuery);
  log<text_log>(statusQuery->endLog);
  recordFsm(false);

  // statusQuery->logReply();
}

// Function to request fsm ADCs
void fsmCtrl::queryAdcs() {
  log<text_log>(adcsQuery->startLog); 
  query(adcsQuery);  
  log<text_log>(adcsQuery->endLog);

  adcsQuery->logReply();
}

// Function to request fsm DACs
void fsmCtrl::queryDacs() {
  log<text_log>(dacsQuery->startLog); 
  query(dacsQuery);  
  log<text_log>(dacsQuery->endLog);

  dacsQuery->logReply();
}

// Function to set fsm DACs
void fsmCtrl::setDacs(uint32_t* Setpoints) {
// void fsmCtrl::setDacs() {  
  // uint32_t Setpoints[3];

  // Setpoints[0] = 60000;
  // Setpoints[1] = 60000;
  // Setpoints[2] = 60000;    

  std::ostringstream oss;
  oss << "SETDACS: " << Setpoints[0] << " | " << Setpoints[1] << " | " << Setpoints[2];
  log<text_log>(oss.str());

  DacsQuery* castDacsQuery = dynamic_cast<DacsQuery*>(dacsQuery);

  log<text_log>(dacsQuery->startLog);
  castDacsQuery->setPayload(Setpoints, 3 * sizeof(uint32_t));
  query(castDacsQuery);
  log<text_log>(castDacsQuery->endLog);

  castDacsQuery->logReply();
  castDacsQuery->resetPayload();
}

void fsmCtrl::query(PZTQuery* pztQuery) 
{
  // Send command packet
  (&UartParser)->TxBinaryPacket(pztQuery->getPayloadType(), pztQuery->getPayloadData(), pztQuery->getPayloadLen());

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
  StatusQuery* statusQueryPtr = dynamic_cast<StatusQuery*>(statusQuery);

  if( !(LastStatus == statusQueryPtr->Status) || force )
  {
    LastStatus = statusQueryPtr->Status;
    telem<telem_fsm>({LastStatus.P1V2, LastStatus.P2V2, LastStatus.P24V, LastStatus.P2V5, LastStatus.P3V3A, LastStatus.P6V, LastStatus.P5V, LastStatus.P3V3D, LastStatus.P4V3, LastStatus.N5V, LastStatus.N6V, LastStatus.P150V});
  }

  return 0;
}

int fsmCtrl::allocate( const dev::shmimT & sp)
{
   static_cast<void>(sp); //be unused
   
   int err = 0;
   
   if(fsmCtrl::m_width != m_fsmWidth)
   {
      log<software_critical>({__FILE__,__LINE__, "shmim width does not match configured FSM width"});
      ++err;
   }
   
   if(fsmCtrl::m_height != m_fsmHeight)
   {
      log<software_critical>({__FILE__,__LINE__, "shmim height does not match configured FSM height"});
      ++err;
   }
   
   if(err) return -1;
   
  //  m_instSatMap.resize(m_dmWidth,m_dmHeight);
  //  m_instSatMap.setZero();
   
  //  m_accumSatMap.resize(m_dmWidth,m_dmHeight);
  //  m_accumSatMap.setZero();
   
  //  m_satPercMap.resize(m_dmWidth,m_dmHeight);
  //  m_satPercMap.setZero();
   
  //  if(findDMChannels() < 0) 
  //  {
  //     log<software_critical>({__FILE__,__LINE__, "error finding DM channels"});
      
  //     return -1;
  //  }
   
   return 0;
}


int fsmCtrl::processImage( void * curr_src,
                                      const dev::shmimT & sp
                                    )
{
   static_cast<void>(sp); //be unused
   
   int rv = commandFSM( curr_src );
   
   if(rv < 0)
   {
      log<software_critical>({__FILE__, __LINE__, errno, rv, "Error from commandFSM"});
      return rv;
   }
  //  //Tell the sat thread to get going
  //  if(sem_post(&m_satSemaphore) < 0)
  //  {
  //     log<software_critical>({__FILE__, __LINE__, errno, 0, "Error posting to semaphore"});
  //     return -1;
  //  }
   
   return rv;
}


int fsmCtrl::commandFSM(void * curr_src)
{
  //  if(state() != stateCodes::OPERATING) return 0;
  //  float pos1 = ((float *) curr_src)[0];
  //  float pos2 = ((float *) curr_src)[1];
   
  //  float pos3 = 0;
  //  if(m_naxes == 3) pos3 = ((float *) curr_src)[2];
   
  //  std::unique_lock<std::mutex> lock(m_indiMutex);
   
  //  int rv;
  //  if( (rv = move_1(pos1)) < 0) return rv;
    
  //  if( (rv = move_2(pos2)) < 0) return rv;
    
  //  if(m_naxes == 3) if( (rv = move_3(pos3)) < 0) return rv;
   
   return 0;
   
}


INDI_NEWCALLBACK_DEFN(fsmCtrl, m_dacs)(const pcf::IndiProperty &ipRecv)
{
  if (ipRecv.createUniqueKey() == m_dacs.createUniqueKey())
  {  
    uint32_t dacs[3] = {0, 0, 0};

    if(ipRecv.find("one"))
    {
      dacs[0] = ipRecv["one"].get<uint32_t>();
    }    

    if(ipRecv.find("two"))
    {
      dacs[1] = ipRecv["two"].get<uint32_t>();
    }    

    if(ipRecv.find("three"))
    {
      dacs[2] = ipRecv["three"].get<uint32_t>();
    }    

    std::ostringstream oss;
    oss << "INDI callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
    log<text_log>(oss.str());

    setDacs(dacs);
    // setDacs();

  }

  log<text_log>("INDI callback.");
  return 0;
}


} //namespace app
} //namespace MagAOX