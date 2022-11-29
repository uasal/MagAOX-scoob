/** \file aguc8Ctrl.hpp
  * \brief The MagAO-X AGUC8 Controller header file
  *
  * \ingroup aguc8Ctrl_files
  */

#ifndef aguc8Ctrl_hpp
#define aguc8Ctrl_hpp

#include <map>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"


#define DEBUG

#ifdef DEBUG
#define BREADCRUMB  std::cerr << __FILE__ << " " << __LINE__ << "\n";
#else
#define BREADCRUMB
#endif

/** \defgroup aguc8Ctrl
  * \brief The AGUC8 Controller application.
  *
  * Controls a multi-channel Newport AGUC8 controller.  Each channel (axis?) gets its own thread.
  * 
  * <a href="../handbook/operating/software/apps/picoMotorCtrl.html">Application Documentation</a>
  *
  * \ingroup apps
  *
  */

/** \defgroup aguc8Ctrl_files
  * \ingroup aguc8Ctrl
  */



namespace MagAOX
{
namespace app
{

/** MagAO-X application to control a multi-channel Newport AGUC8 Controller.
  *
  * \todo replace telnet with serial comm (see smc100ccCtrl)
  * \todo generalize somehow to handle units on the same channel but different axes (save for later -- assume each on axis 1 for now)
  * 
  */
class aguc8Ctrl : public MagAOXApp<>, public tty::usbDevice, public dev::ioDevice, public dev::telemeter<aguc8Ctrl>
{
   
   friend class dev::telemeter<aguc8Ctrl>;
   
   typedef long posT;
   
   struct motorChannel
   {
      aguc8Ctrl * m_parent {nullptr}; ///< A pointer to this for thread starting.
      
      std::string m_name; ///< The name of this channel, from the config section
      
      std::vector<std::string> m_presetNames;
      std::vector<posT> m_presetPositions;
      
      int m_channel {-1}; ///< The number of this channel, where the motor is plugged in
      
      posT m_currCounts {0}; ///< The current counts, the cumulative position
      
      bool m_doMove {false}; ///< Flag indicating that a move is requested.
      bool m_moving {false}; ///< Flag to indicate that we are actually moving
      
      pcf::IndiProperty m_property;
      pcf::IndiProperty m_indiP_presetName;
      
      std::thread * m_thread {nullptr}; ///< Thread for managing this channel.  A pointer to allow copying, but must be deleted in d'tor of parent.
       
      bool m_threadInit {true}; ///< Thread initialization flag.  
      
      pid_t m_threadID {0}; ///< The ID of the thread.
   
      pcf::IndiProperty m_threadProp; ///< The property to hold the thread details.
   
      motorChannel( aguc8Ctrl * p /**< [in] The parent point to set */) : m_parent(p)
      {
         m_thread = new std::thread;
      }
      
      motorChannel( aguc8Ctrl * p,     ///< [in] The parent point to set
                    const std::string & n, ///< [in] The name of this channel
                    int ch                 ///< [in] The number of this channel
                  ) : m_parent(p), m_name(n), m_channel(ch)
      {
         m_thread = new std::thread;
      }
      
   };
   
   typedef std::map<std::string, motorChannel> channelMapT;
   
   /** \name Configurable Parameters
     * @{
     */


   //std::string m_deviceAddr; ///< The device address
   //std::string m_devicePort {"23"}; ///< The device port
   //std::string m_idVendor;
   //std::string m_idProduct;
   //std::string m_serial;
   
   int m_nChannels {4}; ///< The number of motor channels total on the hardware.  Number of attached motors inferred from config.

   int m_writeTimeout {1000}; // time out
   int m_readTimeout {1000};
   float m_sleep {1};
   
   ///@}
   
   channelMapT m_channels; ///< Map of motor names to channel.
   
   //tty::telnetConn m_telnetConn; ///< The telnet connection manager
   //tty::usbDevice m_usbDevice; ///< USB device manager

   ///Mutex for locking telnet communications.
   std::mutex m_usbMutex;
   
   public:

   /// Default c'tor.
   aguc8Ctrl();

   /// D'tor, declared and defined for noexcept.
   ~aguc8Ctrl() noexcept;

   /// Setup the configuration system (called by MagAOXApp::setup())
   virtual void setupConfig();

   /// Implementation of loadConfig logic, separated for testing.
   /** This is called by loadConfig().
     */
   int loadConfigImpl( mx::app::appConfigurator & _config /**< [in] an application configuration from which to load values*/);

   /// load the configuration system results (called by MagAOXApp::setup())
   virtual void loadConfig();

   /// Startup functions
   /** Setsup the INDI vars.
     *
     */
   virtual int appStartup();

   /// Implementation of the FSM
   /** 
     * \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
   virtual int appLogic();

   /// Implementation of the on-power-off FSM logic
   virtual int onPowerOff();

   /// Implementation of the while-powered-off FSM
   virtual int whilePowerOff();

   /// Do any needed shutdown tasks. 
   virtual int appShutdown();
   
   /// Read the current channel counts from disk at startup
   /** Reads the counts from the file with the specified name in this apps sys directory.
     * Returns the file contents as a posT.
     */ 
   posT readChannelCounts(const std::string & chName);
   
   int writeChannelCounts( const std::string & chName,
                           posT counts 
                         );

   int writeReadError(std::string comm, std::string & resp);
   int writeQuery(std::string comm, std::string & resp);
   int Read(std::string & comm);
   
   /// Channel thread starter function
   static void channelThreadStart( motorChannel * mc /**< [in] the channel to start controlling */);
   
   /// Channel thread execution function
   /** Runs until m_shutdown is true.
     */
   void channelThreadExec( motorChannel * mc );
   
/** \name INDI
     * @{
     */ 
protected:

   //declare our properties
   std::vector<pcf::IndiProperty> m_indiP_counts;
   //pcf::IndiProperty m_indiP_stepSize;
   
   
public:
   /// The static callback function to be registered for relative position requests
   /** Dispatches to the handler, which then signals the relavent thread.
     * 
     * \returns 0 on success.
     * \returns -1 on error.
     */

   //INDI_NEWCALLBACK_DECL(aguc8Ctrl, m_indiP_stepSize);

   static int st_newCallBack_pos( void * app, ///< [in] a pointer to this, will be static_cast-ed to this
                                      const pcf::IndiProperty &ipRecv ///< [in] the INDI property sent with the the new property request.
                                    );
   
   /// The handler function for relative position requests, called by the static callback
   /** Signals the relavent thread.
     * 
     * \returns 0 on success.
     * \returns -1 on error.
     */
   int newCallBack_pos( const pcf::IndiProperty &ipRecv /**< [in] the INDI property sent with the the new property request.*/);
   
   /// The static callback function to be registered for position presets
   /** Dispatches to the handler, which then signals the relavent thread.
     * 
     * \returns 0 on success.
     * \returns -1 on error.
     */
   static int st_newCallBack_presetName( void * app, ///< [in] a pointer to this, will be static_cast-ed to this
                                      const pcf::IndiProperty &ipRecv ///< [in] the INDI property sent with the the new property request.
                                    );
   
   /// The handler function for position presets, called by the static callback
   /** Signals the relavent thread.
     * 
     * \returns 0 on success.
     * \returns -1 on error.
     */
   int newCallBack_presetName( const pcf::IndiProperty &ipRecv /**< [in] the INDI property sent with the the new property request.*/);
   ///@}
   
   /** \name Telemeter Interface
     * 
     * @{
     */ 
   int checkRecordTimes();
   
   int recordTelem( const telem_pico * );
   
   int recordAGUC8( bool force = false );
   ///@}
   
};

aguc8Ctrl::aguc8Ctrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{   
   m_powerMgtEnabled = false;
   return;
}

aguc8Ctrl::~aguc8Ctrl() noexcept
{
   //Wait for each channel thread to exit, then delete it.
   for(channelMapT::iterator it = m_channels.begin(); it != m_channels.end(); ++ it)
   {
      if(it->second.m_thread != nullptr)
      {
         if(it->second.m_thread->joinable()) it->second.m_thread->join();
         delete it->second.m_thread;
      }
   }
}


void aguc8Ctrl::setupConfig()
{
   //config.add("device.address", "", "device.address", argType::Required, "device", "address", false, "string", "The controller IP address.");
   config.add("device.nChannels", "", "device.nChannels", argType::Required, "device", "nChannels", false, "int", "Number of motoro channels.  Default is 4.");
   
   tty::usbDevice::setupConfig(config);
   dev::ioDevice::setupConfig(config);
   
   dev::telemeter<aguc8Ctrl>::setupConfig(config);
}
#define AGUC8CTRL_E_NOMOTORS   (-5)
#define AGUC8CTRL_E_BADCHANNEL (-6)
#define AGUC8CTRL_E_DUPMOTOR   (-7)
#define AGUC8CTRL_E_INDIREG    (-20)

int aguc8Ctrl::loadConfigImpl( mx::app::appConfigurator & _config )
{
   //Standard config parsing
   //_config(m_deviceAddr, "device.address");
   _config(m_nChannels, "device.nChannels");

   //int rv = tty::usbDevice::loadConfig(_config);
 

   // Parse the unused config options to look for motors
   std::vector<std::string> sections;

   _config.unusedSections(sections);

   if( sections.size() == 0 )
   {
      log<text_log>("No motors found in config.", logPrio::LOG_CRITICAL);

      return AGUC8CTRL_E_NOMOTORS;
   }

   //Now see if any unused sections have a channel keyword
   for(size_t i=0;i<sections.size(); ++i)
   {
      int channel = -1;
      _config.configUnused(channel, mx::app::iniFile::makeKey(sections[i], "channel" ) );
      if( channel == -1 )
      {
         //not a channel
         continue;
      }
      
      if(channel < 1 || channel > m_nChannels)
      {
         log<text_log>("Bad channel specificiation: " + sections[i] + " " + std::to_string(channel), logPrio::LOG_CRITICAL);

         return AGUC8CTRL_E_BADCHANNEL;
      }

      //Ok, valid channel.  Insert into map and check for duplicates.
      std::pair<channelMapT::iterator, bool> insert = m_channels.insert(std::pair<std::string, motorChannel>(sections[i], motorChannel(this,sections[i],channel)));
      
      if(insert.second == false)
      {
         log<text_log>("Duplicate motor specification: " + sections[i] + " " + std::to_string(channel), logPrio::LOG_CRITICAL);
         return AGUC8CTRL_E_DUPMOTOR;
      }
      else
      {
         _config.configUnused(insert.first->second.m_presetNames, mx::app::iniFile::makeKey(sections[i], "names" ));
         _config.configUnused(insert.first->second.m_presetPositions, mx::app::iniFile::makeKey(sections[i], "positions" ));
      }
      
      log<pico_channel>({sections[i], (uint8_t) channel});
   }
   
   return 0;
}
  
void aguc8Ctrl::loadConfig()
{

   int rv = tty::usbDevice::loadConfig(config);
   //BREADCRUMB
   rv = tty::usbDevice::connect();
   //BREADCRUMB
   log<text_log>("device name is " + std::to_string(tty::usbDevice::getDeviceName()));

   if(rv != 0 && rv != TTY_E_NODEVNAMES && rv != TTY_E_DEVNOTFOUND) //Ignore error if not plugged in
   {
      log<software_error>({ __FILE__, __LINE__, "error loading USB device configs"});
      log<software_error>( {__FILE__, __LINE__, rv, tty::ttyErrorString(rv)});
      m_shutdown = 1;
   }

   if( loadConfigImpl(config) < 0)
   {
      log<text_log>("Error during config", logPrio::LOG_CRITICAL);
      m_shutdown = true;
   }
   
   if(dev::ioDevice::loadConfig(config) < 0)
   {
      log<text_log>("Error during ioDevice config", logPrio::LOG_CRITICAL);
      m_shutdown = true;
   }
   
   if(dev::telemeter<aguc8Ctrl>::loadConfig(config) < 0)
   {
      log<text_log>("Error during ioDevice config", logPrio::LOG_CRITICAL);
      m_shutdown = true;
   }
}

int aguc8Ctrl::appStartup()
{
   ///\todo read state from disk to get current counts.

   //BREADCRUMB
   
   for(channelMapT::iterator it = m_channels.begin(); it != m_channels.end(); ++ it)
   {
      it->second.m_currCounts = readChannelCounts(it->second.m_name);
      
      
      createStandardIndiNumber( it->second.m_property, it->first+"_pos", std::numeric_limits<posT>::lowest(), std::numeric_limits<posT>::max(), static_cast<posT>(1), "%d", "Position", it->first);
      it->second.m_property["current"].set(it->second.m_currCounts);
      it->second.m_property["target"].set(it->second.m_currCounts);
      it->second.m_property.setState(INDI_IDLE);
      
      if( registerIndiPropertyNew( it->second.m_property, st_newCallBack_pos) < 0)
      {
         #ifndef AGUC8CTRL_TEST_NOLOG
         log<software_error>({__FILE__,__LINE__});
         #endif
         return AGUC8CTRL_E_INDIREG;
      }
      
      if(it->second.m_presetNames.size() > 0)
      {
         if(createStandardIndiSelectionSw( it->second.m_indiP_presetName, it->first, it->second.m_presetNames) < 0)
         {
            log<software_critical>({__FILE__, __LINE__});
            return -1;
         }
         if( registerIndiPropertyNew( it->second.m_indiP_presetName, st_newCallBack_presetName) < 0)
         {
            log<software_error>({__FILE__,__LINE__});
            return -1;
         }
      }
      
      //Here we start each channel thread, with 0 R/T prio.
      threadStart( *it->second.m_thread, it->second.m_threadInit, it->second.m_threadID, it->second.m_threadProp, 0, "", it->second.m_name, &it->second, channelThreadStart);
   }
   
   //Install empty signal handler for USR1, which is used to interrupt sleeps in the channel threads.
   struct sigaction act;
   sigset_t set;

   act.sa_sigaction = &sigUsr1Handler;
   act.sa_flags = SA_SIGINFO;
   sigemptyset(&set);
   act.sa_mask = set;

   errno = 0;
   if( sigaction(SIGUSR1, &act, 0) < 0 )
   {
      std::string logss = "Setting handler for SIGUSR1 failed. Errno says: ";
      logss += strerror(errno);

      log<software_error>({__FILE__, __LINE__, errno, 0, logss});

      return -1;
   }

   // register INDI properties
   //REG_INDI_NEWPROP(m_indiP_currStep, "current_stepSize", pcf::IndiProperty::Number);
   //REG_INDI_NEWPROP(m_indiP_tgtStep, "target_stepSize", pcf::IndiProperty::Number);
   
   if(dev::telemeter<aguc8Ctrl>::appStartup() < 0)
   {
      return log<software_error,-1>({__FILE__,__LINE__});
   }
   
   return 0;
}

int aguc8Ctrl::appLogic()
{

   // TEMPORARILY ASSUME POWER IS ON
   if(state()==stateCodes::INITIALIZED){
      state(stateCodes::POWERON);
   }


   if( state() == stateCodes::POWERON)
   {
      if(!powerOnWaitElapsed()) return 0;
      
      state(stateCodes::NOTCONNECTED);
   }
   
   if(state() == stateCodes::NOTCONNECTED || state() == stateCodes::ERROR)
   {
      //BREADCRUMB
      //int rv = m_usbDevice.connect();
      int rv = 0; // uuhhhhhhh
      
      if(rv == 0)
      {
         state(stateCodes::CONNECTED);
      }
      else
      {
         if(!stateLogged())
         {
            log<text_log>("Failed to connect to USB device with serial " + m_serial);
            log<text_log>(tty::ttyErrorString(rv));
            //log<text_log>(m_usbDevice.getDeviceName());
         }
         
         return 0;
      }
      
   }
   
   if(state() == stateCodes::CONNECTED)
   {
         
      //std::unique_lock<std::mutex> lock(m_usbMutex);
      //BREADCRUMB
      std::string comm = "MR";
      std::string qresp;
      int resp = writeReadError(comm, qresp);
      //int rv = m_usbDevice.ttyWriteRead(resp, "MR", "\r\n", false, m_fileDescrip, m_readTimeout, m_writeTimeout); // set remote mode
      //BREADCRUMB
            
      if(resp == 0)
      {
         log<text_log>("Connected to the AGUC8 controller in remote mode");
      }
      else
      {
         if(m_powerState == 0) return 0;
         
         log<software_error>({__FILE__, __LINE__, "wrong response to MR request"});
         log<text_log>(std::to_string(resp));
         state(stateCodes::ERROR);
         return 0;
      }

      // temporary! check error codes after MR
      /*std::string qresp;
      comm = "TE";
      resp = writeRead(comm);
      resp = Read(qresp);
      std::cout << "got error code " << qresp << " to MR\n";*/

      state(stateCodes::READY);
      
      
      return 0;
   }
   
   if(state() == stateCodes::READY || state() == stateCodes::OPERATING)
   {
      //check connection      
      /*{
        //std::unique_lock<std::mutex> lock(m_usbMutex);
        //std::string comm = "MR";
        BREADCRUMB
        int rv = writeRead(comm);//(resp, "MR", "\r\n", false, m_fileDescrip, m_readTimeout, m_writeTimeout); // set remote mode
        BREADCRUMB
                
        if(rv == 0)
        {
            log<text_log>("Connected to the AGUC8 controller in remote mode");
        }
        else
        {
            if(m_powerState == 0) return 0;
            
            log<software_error>({__FILE__, __LINE__, "wrong response to MR request"});
            state(stateCodes::ERROR);
            return 0;
        }
      }*/
      
      //Now check state of motors
      bool anymoving = false;
      
      //This is where we'd check for moving
      for(channelMapT::iterator it = m_channels.begin(); it != m_channels.end(); ++ it)
      {
         //std::unique_lock<std::mutex> lock(m_telnetMutex);
      
         // change channel and then check status 
         //std::string comm = "CC" + std::to_string(it->second.m_channel);
         //std::string eresp;
         //int resp = writeReadError(comm, eresp);

         // check if moving
         // for now, just switch between axes rather than channels
         std::string query = std::to_string(it->second.m_channel) + "TS"; //"1TS"; // always assume devices are on axis 1 for now
         std::string qresp;
         int resp = writeQuery(query, qresp);
         
         int q = (int)qresp[3] - '0';

         //The check for moving here. With power off detection
         if( q != 0) 
         {
            anymoving = true;
            it->second.m_moving = true;
         }
         else
         {
            it->second.m_moving = false;
         }
         
         if(it->second.m_moving == false && it->second.m_doMove == true)
         {
            it->second.m_currCounts = it->second.m_property["target"].get<long>();
            log<text_log>("moved " + it->second.m_name + " to " + std::to_string(it->second.m_currCounts) + " counts");
            it->second.m_doMove = false;
            recordAGUC8(true);
         }
      }
   
      if(anymoving == false) state(stateCodes::READY);
      else state(stateCodes::OPERATING);
      
      for(channelMapT::iterator it = m_channels.begin(); it != m_channels.end(); ++ it)
      {
         std::unique_lock<std::mutex> lock(m_indiMutex);
         if(it->second.m_moving) updateIfChanged(it->second.m_property, "current", it->second.m_currCounts, INDI_BUSY);
         else updateIfChanged(it->second.m_property, "current", it->second.m_currCounts, INDI_IDLE);
         
         for(size_t n=0; n < it->second.m_presetNames.size(); ++n)
         {
            bool changed = false;
            if( it->second.m_currCounts == it->second.m_presetPositions[n])
            {
               if(it->second.m_indiP_presetName[it->second.m_presetNames[n]] == pcf::IndiElement::Off) changed = true;
               it->second.m_indiP_presetName[it->second.m_presetNames[n]] = pcf::IndiElement::On;
            }
            else
            {
               if(it->second.m_indiP_presetName[it->second.m_presetNames[n]] == pcf::IndiElement::On) changed = true;
               it->second.m_indiP_presetName[it->second.m_presetNames[n]] = pcf::IndiElement::Off;
            }
            
            if(changed) m_indiDriver->sendSetProperty(it->second.m_indiP_presetName);
         }
         
         if(writeChannelCounts(it->second.m_name, it->second.m_currCounts) < 0)
         {
            log<software_error>({__FILE__, __LINE__});
         }
      }
      
      if(telemeter<aguc8Ctrl>::appLogic() < 0)
      {
         log<software_error>({__FILE__, __LINE__});
         return 0;
      }
      
      return 0;
   }
   
   
   return 0;
}

int aguc8Ctrl::onPowerOff()
{
   return 0;
}

int aguc8Ctrl::whilePowerOff()
{
   return 0;
}

int aguc8Ctrl::appShutdown()
{
   //Shutdown and join the threads
   for(channelMapT::iterator it = m_channels.begin(); it != m_channels.end(); ++ it)
   {
      if(it->second.m_thread->joinable())
      {
         pthread_kill(it->second.m_thread->native_handle(), SIGUSR1);
         try
         {
            it->second.m_thread->join(); //this will throw if it was already joined
         }
         catch(...)
         {
         }
      }
   }
   
   dev::telemeter<aguc8Ctrl>::appShutdown();

   return 0;
}

aguc8Ctrl::posT aguc8Ctrl::readChannelCounts(const std::string & chName)
{
   std::string statusDir = sysPath;
   statusDir += "/";
   statusDir += m_configName;

   std::string fileName = statusDir + "/" + chName;
   
   std::ifstream posIn;
   posIn.open( fileName );
   
   if(!posIn.good())
   {
      log<text_log>("no position file for " + chName + " found.  initializing to 0.");
      return 0;
   }
   
   long pos;
   posIn >> pos;
   
   posIn.close();
   
   log<text_log>("initializing " + chName + " to " + std::to_string(pos));
   
   return pos;
}

int aguc8Ctrl::writeChannelCounts( const std::string & chName,
                                       posT counts 
                                 )
{
   std::string statusDir = sysPath;
   statusDir += "/";
   statusDir += m_configName;

   std::string fileName = statusDir + "/" + chName;
   
   elevatedPrivileges ep(this);
   
   std::ofstream posOut;
   posOut.open( fileName );
   
   if(!posOut.good())
   {
      log<text_log>("could not open counts file for " + chName + " -- can not store position.", logPrio::LOG_ERROR);
      return -1;
   }
   
   posOut << counts;
   
   posOut.close();
   
   return 0;
}

int aguc8Ctrl::writeReadError( std::string comm, std::string & resp)
{
    std::unique_lock<std::mutex> lock(m_usbMutex);
    //BREADCRUMB
    //std::cout << "Sending command " << comm << "\n";

    int rv = MagAOX::tty::ttyWrite( (comm+"\r\n").c_str(), m_fileDescrip, m_writeTimeout);
    sleep(m_sleep);

    // get error code of last command after reading
    std::string tecomm = "TE";
    std::string le = "\r\n";
    //rv = MagAOX::tty::ttyWriteRead(resp, tecomm.c_str() , le.c_str(), false, m_fileDescrip, m_writeTimeout, m_readTimeout);
    rv = MagAOX::tty::ttyWrite( (tecomm+"\r\n").c_str(), m_fileDescrip, m_writeTimeout);
    rv = MagAOX::tty::ttyRead(resp, "\r\n", m_fileDescrip, m_readTimeout);
    //std::cout << "Got error code " << resp << "from command " << comm << "\n";
    /*if (rv != TTY_E_NOERROR){
      return rv;
    }
    return std::stoi(resp);*/
    return rv;
}

int aguc8Ctrl::writeQuery( std::string comm, std::string & resp)
{
    std::unique_lock<std::mutex> lock(m_usbMutex);
    //BREADCRUMB
    //std::cout << "Sending query " << comm << "\n";

    int rv = MagAOX::tty::ttyWrite( (comm+"\r\n").c_str(), m_fileDescrip, m_writeTimeout);
    rv = MagAOX::tty::ttyRead(resp, "\r\n", m_fileDescrip, m_readTimeout);
    //std::cout << "Got response " << resp << "to query " << comm << "\n";
    return rv;
}

int aguc8Ctrl::Read( std::string & resp)
{
    std::unique_lock<std::mutex> lock(m_usbMutex);
    //BREADCRUMB
    int rv = MagAOX::tty::ttyRead(resp, "\r\n", m_fileDescrip, m_readTimeout);
    sleep(m_sleep);
    //std::cout << resp << "\n";
    return rv;
}

void aguc8Ctrl::channelThreadStart( motorChannel * mc )
{
   mc->m_parent->channelThreadExec(mc);
}
   
void aguc8Ctrl::channelThreadExec( motorChannel * mc)
{
   //Get the thread PID immediately so the caller can return.
   mc->m_threadID = syscall(SYS_gettid);
   
   //Wait for initialization to complete.
   while( mc->m_threadInit == true && m_shutdown == 0)
   {
      sleep(1);
   }
   
   //Now begin checking for state change request.
   while(!m_shutdown)
   {
      //If told to move and not moving, start a move
      if(mc->m_doMove && !mc->m_moving && (state() == stateCodes::READY || state() == stateCodes::OPERATING))
      {
         long dr = mc->m_property["target"].get<long>() - mc->m_currCounts;
         
         recordAGUC8(true);
         //std::unique_lock<std::mutex> lock(m_usbMutex);
         state(stateCodes::OPERATING);
         mc->m_moving = true;
         log<text_log>("moving " + mc->m_name + " by " + std::to_string(dr) + " counts");

         // need to change channel and then request a relative move
         //std::string comm = "CC" + std::to_string(mc->m_channel);
         std::string qresp;
         //log<text_log>("changing to channel  " + comm);
         //writeReadError(comm, qresp);
         
         std::string comm2 = std::to_string(mc->m_channel) + "PR" + std::to_string(dr); // this always commands axis 1 -- generalize me in the future!!!
         log<text_log>("sending move command  " + comm2);
         writeReadError(comm2, qresp);
      }
      else if( !(state() == stateCodes::READY || state() == stateCodes::OPERATING))
      {
         mc->m_doMove = false; //In case a move is requested when not able to move
      }
      
      sleep(1);
   }
   
   
}


int aguc8Ctrl::st_newCallBack_pos( void * app,
                                           const pcf::IndiProperty &ipRecv
                                         )
{
   return static_cast<aguc8Ctrl*>(app)->newCallBack_pos(ipRecv);
}

int aguc8Ctrl::newCallBack_pos( const pcf::IndiProperty &ipRecv )
{
   
   //Search for the channel
   std::string propName = ipRecv.getName();
   size_t nend = propName.rfind("_pos");
   
   if(nend == std::string::npos)
   {
      log<software_error>({__FILE__, __LINE__, "Channel without _pos received"});
      return -1;
   }
   
   std::string chName = propName.substr(0, nend);
   channelMapT::iterator it = m_channels.find(chName);

   if(it == m_channels.end())
   {
      log<software_error>({__FILE__, __LINE__, "Unknown channel name received"});
      return -1;
   }

   if(it->second.m_doMove == true)
   {
      log<text_log>("channel " + it->second.m_name + " is already moving", logPrio::LOG_WARNING);
      return 0;
   }
   
   //Set the target element, and the doMove flag, and then signal the thread.
   {//scope for mutex
      std::unique_lock<std::mutex> lock(m_indiMutex);
      
      long counts; //not actually used
      if(indiTargetUpdate( it->second.m_property, counts, ipRecv, true) < 0)
      {
         return log<software_error,-1>({__FILE__,__LINE__});
      }
   }
   
   it->second.m_doMove= true;
   
   pthread_kill(it->second.m_thread->native_handle(), SIGUSR1);
   
   return 0;
}

int aguc8Ctrl::st_newCallBack_presetName( void * app,
                                             const pcf::IndiProperty &ipRecv
                                            )
{
   return static_cast<aguc8Ctrl*>(app)->newCallBack_presetName (ipRecv);
}

int aguc8Ctrl::newCallBack_presetName( const pcf::IndiProperty &ipRecv )
{
   channelMapT::iterator it = m_channels.find(ipRecv.getName());

   if(it == m_channels.end())
   {
      log<software_error>({__FILE__, __LINE__, "Unknown channel name received"});
      return -1;
   }

   if(it->second.m_doMove == true)
   {
      log<text_log>("channel " + it->second.m_name + " is already moving", logPrio::LOG_WARNING);
      return 0;
   }
   
   long counts = -1e10;
   
   size_t i;
   for(i=0; i< it->second.m_presetNames.size(); ++i) 
   {
      if(!ipRecv.find(it->second.m_presetNames[i])) continue;
      
      if(ipRecv[it->second.m_presetNames[i]].getSwitchState() == pcf::IndiElement::On)
      {
         if(counts != -1e10)
         {
            log<text_log>("More than one preset selected", logPrio::LOG_ERROR);
            return -1;
         }
         
         counts = it->second.m_presetPositions[i];
         std::cerr << "selected: " << it->second.m_presetNames[i] << " " << counts << "\n";
      }
   }
   
   //Set the target element, and the doMove flag, and then signal the thread.
   {//scope for mutex
      std::unique_lock<std::mutex> lock(m_indiMutex);
      
      it->second.m_property["target"].set(counts);
   }
   
   it->second.m_doMove= true;
   
   pthread_kill(it->second.m_thread->native_handle(), SIGUSR1);
   
   return 0;
}

int aguc8Ctrl::checkRecordTimes()
{
   return telemeter<aguc8Ctrl>::checkRecordTimes(telem_pico());
}
   
int aguc8Ctrl::recordTelem( const telem_pico * )
{
   return recordAGUC8(true);
}

int aguc8Ctrl::recordAGUC8( bool force )
{
   static std::vector<int64_t> lastpos(m_nChannels, std::numeric_limits<long>::max());
   
   bool changed = false;
   for(channelMapT::iterator it = m_channels.begin(); it != m_channels.end(); ++ it)
   {
      if(it->second.m_currCounts != lastpos[it->second.m_channel-1]) changed = true;
   }
   
   if( changed || force )
   {
      for(channelMapT::iterator it = m_channels.begin(); it != m_channels.end(); ++ it)
      {
         lastpos[it->second.m_channel-1] = it->second.m_currCounts;
      }
   
      telem<telem_pico>(lastpos);
   }

   return 0;
}

} //namespace app
} //namespace MagAOX

#endif //aguc8Ctrl_hpp
