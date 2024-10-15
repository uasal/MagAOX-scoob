/** \file nsvCtrl.hpp
  * \brief The MagAO-X nsvCtrl controller header file
  *
  * \ingroup nsvCtrl_files
  */

#ifndef alpaoCtrl_hpp
#define alpaoCtrl_hpp

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include "v4l2lib.hpp"

#include <cstdlib>
#include <fcntl.h>

#include <unistd.h>

namespace MagAOX
{
namespace app
{

class nsvCtrl : public MagAOXApp<>, public dev::stdCamera<nsvCtrl>,
                                    public dev::frameGrabber<nsvCtrl>, public dev::telemeter<nsvCtrl>
{
    friend class dev::stdCamera<nsvCtrl>;
    friend class dev::frameGrabber<nsvCtrl>;
    friend class dev::telemeter<nsvCtrl>;

public:
     /** \name app::dev Configurations
     *@{
     */
   static constexpr bool c_stdCamera_tempControl = false; ///< app::dev config to tell stdCamera to expose temperature controls
   
   static constexpr bool c_stdCamera_temp = false; ///< app::dev config to tell stdCamera to expose temperature
   
   static constexpr bool c_stdCamera_readoutSpeed = false; ///< app::dev config to tell stdCamera to expose readout speed controls
   
   static constexpr bool c_stdCamera_vShiftSpeed = false; ///< app:dev config to tell stdCamera to expose vertical shift speed control
   
   static constexpr bool c_stdCamera_emGain = true; ///< app::dev config to tell stdCamera to expose EM gain controls 

   static constexpr bool c_stdCamera_blacklevel = true; ///< app::dev config to tell stdCamera to expose Blacklevel controls 

   static constexpr bool c_stdCamera_exptimeCtrl = true; ///< app::dev config to tell stdCamera to expose exposure time controls
   
   static constexpr bool c_stdCamera_fpsCtrl = false; ///< app::dev config to tell stdCamera to not expose FPS controls

   static constexpr bool c_stdCamera_fps = false; ///< app::dev config to tell stdCamera not to expose FPS status
   
   static constexpr bool c_stdCamera_synchro = false; ///< app::dev config to tell stdCamera to not expose synchro mode controls

   static constexpr bool c_stdCamera_usesModes = false; ///< app:dev config to tell stdCamera not to expose mode controls
   
   static constexpr bool c_stdCamera_usesROI = false; ///< app:dev config to tell stdCamera to expose ROI controls

   static constexpr bool c_stdCamera_cropMode = false; ///< app:dev config to tell stdCamera to expose Crop Mode controls
   
   static constexpr bool c_stdCamera_hasShutter = false; ///< app:dev config to tell stdCamera to expose shutter controls

   static constexpr bool c_stdCamera_usesStateString = false; ///< app::dev confg to tell stdCamera to expose the state string property

   static constexpr bool c_frameGrabber_flippable = false; ///< app:dev config to tell framegrabber this camera can not be flipped

   ///@}

protected:

   /** \name configurable parameters
     *@{
     */

   ///@}

   std::string m_configFile; ///< The path, relative to configDir, where to write and read the temporary config file.
   
   bool m_init {false}; ///< Whether or not the nsvCam is initialized.

   bool m_poweredOn {false};

   std::string camera_string = "/dev/video2"; // hard code to one camera for now. add cam select 

public:

   nsvCtrl();
   ~nsvCtrl() noexcept;

   std::string cmdRes(const char* cmd);

   /// Setup the configuration system (called by MagAOXApp::setup())
   virtual void setupConfig();

   /// load the configuration system results (called by MagAOXApp::setup())
   virtual void loadConfig();

   /// Startup functions
   /** Sets up the INDI vars.
     *
     */
   virtual int appStartup();

   /// Implementation of the FSM for the Siglent SDG
   virtual int appLogic();

   /// Implementation of the on-power-off FSM logic
   virtual int onPowerOff();

   /// Implementation of the while-powered-off FSM
   virtual int whilePowerOff();

   virtual int appShutdown();

   int cameraSelect();

   int getTemp();

   int setTempControl();

   int getFPS();
   
   int getEMGain();
   
   int setEMGain();

   int getBlacklevel();

   int setBlacklevel();

   int setCropMode();

   int setShutter( unsigned os);

   int setFPS();

   int getExpTime();

   //int getBitDepth(); //12, 14, 16
   
   //int setBitDepth(int bitDepth);

   int writeConfig();

   /** \name stdCamera Interface 
     * 
     * @{
     */
   
   /// Set defaults for a power on state.
   /** 
     * \returns 0 on success
     * \returns -1 on error
     */ 
   int powerOnDefaults();

   /// Required by stdCamera, but this does not do anything for this camera [stdCamera interface]
   /**
     * \returns 0 always
     */ 
   int setExpTime();
   
   /// Check the next ROI
   /** Checks if the target values are valid and adjusts them to the closest valid values if needed.
     *
     * \returns 0 if successfull
     * \returns -1 otherwise
     */
   int checkNextROI();

   /// Required by stdCamera, but this does not do anything for this camera [stdCamera interface]
   /**
     * \returns 0 always
     */
   int setNextROI();
   
   ///@}
   
   
   /** \name framegrabber Interface 
     * 
     * @{
     */
   
   int configureAcquisition();
   float fps();
   int startAcquisition();
   int acquireAndCheckValid();
   int loadImageIntoStream(void * dest);
   int reconfig();

   //INDI:
protected:
   
public:
   
   /** \name Telemeter Interface
     * 
     * @{
     */ 
   int checkRecordTimes();
   
   int recordTelem( const telem_stdcam * );
      
   ///@}
};



inline
nsvCtrl::nsvCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
   m_powerMgtEnabled = true;
   m_powerOnWait = 10;
   //m_startupTemp = -45;  
   
   //m_defaultReadoutSpeed  = "emccd_17MHz";
   //m_readoutSpeedNames = {"ccd_00_08MHz", "ccd_01MHz", "ccd_03MHz", "emccd_01MHz", "emccd_05MHz", "emccd_10MHz", "emccd_17MHz"};
   //m_readoutSpeedNameLabels = {"CCD 0.08 MHz", "CCD 1 MHz", "CCD 3 MHz", "EMCCD 1 MHz", "EMCCD 5 MHz", "EMCCD 10 MHz", "EMCCD 17 MHz"};
   
   m_maxEMGain = 360;
   m_emGainSet = 100;
   m_blacklevelSet = 10;
   m_maxBlacklevel = 65535; // pair with bit depth mode?
   m_minBlacklevel = 0;
   m_maxExpTime = 3600000000;
   m_minExpTime = 69;
   m_maxFPS = 10000000;
   m_minFPS = 10000000;

   m_default_x = 3072; 
   m_default_y = 2105; 
   m_default_w = 6144;  
   m_default_h = 4210;  
      
   m_nextROI.x = m_default_x;
   m_nextROI.y = m_default_y;
   m_nextROI.w = m_default_w;
   m_nextROI.h = m_default_h;
   m_nextROI.bin_x = 1;
   m_nextROI.bin_y = 1;
   
   m_full_x = 3072; //3121.5; 
   m_full_y = 2105; //2093.5; 
   m_full_w = 6144; //6244; 
   m_full_h = 4210; //4188; 
   
   
   return;
}

inline
nsvCtrl::~nsvCtrl() noexcept
{
   return;
}

inline
void nsvCtrl::setupConfig()
{
   dev::stdCamera<nsvCtrl>::setupConfig(config);
   dev::frameGrabber<nsvCtrl>::setupConfig(config);
   dev::telemeter<nsvCtrl>::setupConfig(config);
}

inline
void nsvCtrl::loadConfig()
{
   dev::stdCamera<nsvCtrl>::loadConfig(config);
   
   m_configFile = "/tmp/nsv_";
   m_configFile += configName();
   m_configFile += ".cfg";
   m_cameraModes["onlymode"] = dev::cameraConfig({m_configFile, "", 255, 255, 512, 512, 1, 1, 1, 1, 1000});
   m_startupMode = "onlymode";
   
   if(writeConfig() < 0)
   {
      log<software_critical>({__FILE__,__LINE__});
      m_shutdown = true;
      return;
   }

   if(m_maxEMGain < 1)
   {
      m_maxEMGain = 1;
      log<text_log>("maxGain set to 1");
   }

   if(m_maxEMGain > 360)
   {
      m_maxEMGain = 360;
      log<text_log>("maxGain set to 360");
   }

   dev::frameGrabber<nsvCtrl>::loadConfig(config);
   
   dev::telemeter<nsvCtrl>::loadConfig(config);

}

inline
int nsvCtrl::appStartup()
{
   if(dev::stdCamera<nsvCtrl>::appStartup() < 0)
   {
      return log<software_critical,-1>({__FILE__,__LINE__});
   }

   if(dev::frameGrabber<nsvCtrl>::appStartup() < 0)
   {
      return log<software_critical,-1>({__FILE__,__LINE__});
   }

   if(dev::telemeter<nsvCtrl>::appStartup() < 0)
   {
      return log<software_error,-1>({__FILE__,__LINE__});
   }
   
   state(stateCodes::NOTCONNECTED);

   m_powerState = 1;  //figure out power stuff
   m_powerTargetState = 1;

   return 0;

}

inline
int nsvCtrl::appLogic()
{

   if(dev::stdCamera<nsvCtrl>::appLogic() < 0)
   {
      return log<software_error, -1>({__FILE__, __LINE__});
   }
   
   if(dev::frameGrabber<nsvCtrl>::appLogic() < 0)
   {
      return log<software_error, -1>({__FILE__, __LINE__});
   }

   if( state() == stateCodes::POWERON) 
   {
      return 0;
   }

   if( state() == stateCodes::NOTCONNECTED || state() == stateCodes::NODEVICE || state() == stateCodes::ERROR)
   {
      //Might have gotten here because of a power off.
      if(m_powerState == 0) return 0;
      
      int ret = cameraSelect();
      m_powerState = 1;
      m_poweredOn = true;

      if( ret != 0) 
      {
         return log<software_critical,-1>({__FILE__, __LINE__});
      }
   }

   if( state() == stateCodes::CONNECTED )
   {
      printf("StateCode connected\n");

      writeConfig();
      m_shutterStatus = "READY";

      state(stateCodes::READY);
      if(m_poweredOn)
      {
         m_poweredOn = false;
         if(powerState() != 1 || powerStateTarget() != 1) return 0;
      }
   }

   if( state() == stateCodes::READY || state() == stateCodes::OPERATING )
   {
      //Get a lock if we can
      std::unique_lock<std::mutex> lock(m_indiMutex, std::try_to_lock);

      //but don't wait for it, just go back around.
      if(!lock.owns_lock()) return 0;

      /*
      if(getTemp() < 0)
      {
         if(m_powerState == 0) return 0;

         state(stateCodes::ERROR);
         return 0;
      }
      */

     if(getFPS() < 0)
      {
         if(m_powerState == 0) return 0;

         state(stateCodes::ERROR);
         return 0;
      }

      if(getEMGain() < 0)
      {
         if(m_powerState == 0) return 0;

         state(stateCodes::ERROR);
         return 0;
      }

      if(getBlacklevel() < 0)
      {
         if(m_powerState == 0) return 0;

         state(stateCodes::ERROR);
         return 0;
      }

      if(getExpTime() < 0)
      {
         if(m_powerState == 0) return 0;

         state(stateCodes::ERROR);
         return 0;
      }

      if(frameGrabber<nsvCtrl>::updateINDI() < 0)
      {
         log<software_error>({__FILE__, __LINE__});
         state(stateCodes::ERROR);
         return 0;
      }
      
      if(stdCamera<nsvCtrl>::updateINDI() < 0)
      {
         log<software_error>({__FILE__, __LINE__});
         state(stateCodes::ERROR);
         return 0;
      }
      
      if(telemeter<nsvCtrl>::appLogic() < 0)
      {
         log<software_error>({__FILE__, __LINE__});
         return 0;
      }
   }

   //Fall through check?

   return 0;

}

inline
int nsvCtrl::onPowerOff()
{
   if(m_init)
   {
      m_init = false;
   }

   stopStreaming();
      
   m_powerOnCounter = 0;

   std::lock_guard<std::mutex> lock(m_indiMutex);

   m_shutterStatus = "POWEROFF";
   m_shutterState = 0;
   
   if(stdCamera<nsvCtrl>::onPowerOff() < 0)
   {
      log<software_error>({__FILE__, __LINE__});
   }

   if(frameGrabber<nsvCtrl>::onPowerOff() < 0)
   {
      log<software_error>({__FILE__, __LINE__});
   }
   
   //Setting m_poweredOn
   m_poweredOn = true;

   return 0;
}


inline
int nsvCtrl::whilePowerOff()
{
   m_shutterStatus = "POWEROFF";
   m_shutterState = 0;
   
   if(stdCamera<nsvCtrl>::whilePowerOff() < 0)
   {
      log<software_error>({__FILE__, __LINE__});
   }
   
   return 0;
}


inline
int nsvCtrl::appShutdown()
{

   stopStreaming();
   if(m_init)
   {
      //stopStreaming();
      // clean up buffers?
      m_init = false;
   }
      
   dev::frameGrabber<nsvCtrl>::appShutdown();

   dev::telemeter<nsvCtrl>::appShutdown();
   
   return 0;
}

 
inline
int nsvCtrl::cameraSelect()  
{
   unsigned int error;
   
   char path[] = "/dev/video2";
   if(openCamera(path) == -1){
      log<text_log>("No nsv camera found on path", logPrio::LOG_WARNING);
      state(stateCodes::NODEVICE);
      return -1;
   }
   
   if(initCamera(6144,4210) == -1){
      log<text_log>("Failed to initialize camera", logPrio::LOG_WARNING);
      state(stateCodes::NODEVICE);
      return -1;
   }

   CameraParams params = getCameraParams();
   std::cout << "Camera params - Width: " << params.width
                  << ", Height: " << params.height
                  << ", Pixel Format: " << params.pixelFormat << std::endl;


   if(requestBuffers(1)  == -1 ||
      queryBuffers()     == -1) {
      log<text_log>("Failed to initialize camera buffers", logPrio::LOG_WARNING);
      state(stateCodes::NODEVICE);
      return -1;
   }

   if(startStreaming() == -1){
      log<text_log>("Failed to start camera stream", logPrio::LOG_WARNING);
      state(stateCodes::NODEVICE);
      return -1;
   }

   if(queueBuffer(0) == -1){
      log<text_log>("Failed to start queueing images", logPrio::LOG_WARNING);
      state(stateCodes::NODEVICE);
      return -1;
   }

    if(dequeueBuffer() == -1){
      log<text_log>("Failed to start queueing images", logPrio::LOG_WARNING);
      state(stateCodes::NODEVICE);
      return -1;
   }


   state(stateCodes::CONNECTED);
   log<text_log>(std::string("Connected to ") + camera_string);

   m_cropModeSet = false; // move this somewhere it makes sense

   return 0;
}

inline
int nsvCtrl::getTemp()
{
   float temp = -999;

   // get temperature parameter from nsv
   // in this dir? /sys/devices/virtual/thermal/thermal_zone?

   m_ccdTemp = temp;

   return 0;
}

inline
int nsvCtrl::getEMGain()
{
   const std::string command = "v4l2-ctl --get-ctrl gain -d " + camera_string; 
   std::string result = cmdRes(command.c_str());
   if( result == "error") 
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from getEMGain"});
      return -1;
   }

   m_emGain = std::stoi(result);
   return 0;
}

inline
int nsvCtrl::setEMGain()
{
   
   int gain_to_set = m_emGainSet;  

   if(gain_to_set < 0)
   {
      gain_to_set = 0;
      log<text_log>("Gain limited to 0", logPrio::LOG_WARNING);
   }
   
   if(gain_to_set > m_maxEMGain)
   {
      gain_to_set = m_maxEMGain;
      log<text_log>("Gain limited to maxGain = " + std::to_string(gain_to_set), logPrio::LOG_WARNING);
   }
   
   const std::string command = "v4l2-ctl --set-ctrl gain=" + std::to_string(gain_to_set) + " -d " + camera_string; 
   int result = std::system(command.c_str());
  
   if( result != 0)
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from setEMGain:"});
      return -1;
   }

   log<text_log>("Set Gain to: " + std::to_string(gain_to_set), logPrio::LOG_WARNING);
   
   return 0;
}


inline
int nsvCtrl::getBlacklevel()
{
   const std::string command = "v4l2-ctl --get-ctrl blacklevel -d " + camera_string; 
   std::string result = cmdRes(command.c_str());
   if( result == "error") 
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from blacklevel"});
      return -1;
   }

   m_blacklevel = std::stoi(result);
   return 0;
}

inline
int nsvCtrl::setBlacklevel()
{
   
   int blacklevel_to_set = m_blacklevelSet;  

   if(blacklevel_to_set < 0)
   {
      blacklevel_to_set = 0;
      log<text_log>("Blacklevel limited to 0", logPrio::LOG_WARNING);
   }
   
   if(blacklevel_to_set > m_maxBlacklevel)
   {
      blacklevel_to_set = m_maxBlacklevel;
      log<text_log>("Blacklevel limited to maxBlacklevel = " + std::to_string(blacklevel_to_set), logPrio::LOG_WARNING);
   }
   
   const std::string command = "v4l2-ctl --set-ctrl blacklevel=" + std::to_string(blacklevel_to_set) + " -d " + camera_string; 
   int result = std::system(command.c_str());
  
   if( result != 0)
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from setBlacklevel:"});
      return -1;
   }

   log<text_log>("Set Blacklevel to: " + std::to_string(blacklevel_to_set), logPrio::LOG_WARNING);
   
   return 0;
}

inline
int nsvCtrl::setCropMode()
{
   return 0;
}

inline
int nsvCtrl::setShutter( unsigned os )
{
   return 0;
}


inline 
int nsvCtrl::writeConfig()
{
   std::string configFile = "/tmp/nsvCam_";
   configFile += configName();
   configFile += ".cfg";
   
   std::ofstream fout;
   fout.open(configFile);
   
   if(fout.fail())
   {
      log<software_error>({__FILE__, __LINE__, " error opening config file " + configFile + " for writing"});
      return -1;
   }

   int w = m_nextROI.w / m_nextROI.bin_x;
   int h = m_nextROI.h / m_nextROI.bin_y;
   
   fout << "camera_class:                  \"nsvCam\"\n";
   //fout << "camera_model:                  \"iXon Ultra 897\"\n";
   //fout << "camera_info:                   \"512x512 (1-tap, freerun)\"\n";
   fout << "width:                         " << w << "\n";
   fout << "height:                        " << h << "\n";
   fout << "depth:                         16\n";
   fout << "extdepth:                      16\n";
   fout << "CL_DATA_PATH_NORM:             0f       # single tap\n";
   fout << "CL_CFG_NORM:                   02\n";
   
   fout.close();
   
   return 0;

}
//------------------------------------------------------------------------
//-----------------------  stdCamera interface ---------------------------
//------------------------------------------------------------------------

inline
int nsvCtrl::powerOnDefaults()
{
   //Camera boots up with this true in most cases.
   m_tempControlStatus = false;
   m_tempControlStatusSet = false;
   m_tempControlStatusStr =  "OFF"; 
   m_tempControlOnTarget = false;
      
   m_currentROI.x = m_default_x;
   m_currentROI.y = m_default_y;
   m_currentROI.w = m_default_w;
   m_currentROI.h = m_default_h;
   m_currentROI.bin_x = m_default_bin_x;
   m_currentROI.bin_y = m_default_bin_y;
   
   m_nextROI.x = m_default_x;
   m_nextROI.y = m_default_y;
   m_nextROI.w = m_default_w;
   m_nextROI.h = m_default_h;
   m_nextROI.bin_x = m_default_bin_x;
   m_nextROI.bin_y = m_default_bin_y;
   
   return 0;
}

inline
int nsvCtrl::setTempControl()
{
   return 0;   // these cameras don't implement this
}

std::string nsvCtrl::cmdRes(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Check for errors messages such as:
      //    Cannot open device /dev/video5, exiting.
      //    unknown control 'some_invalid_param_name'
    std::string str(cmd);
    if (result.find("Cannot open device") == 0 || result.find("unknown control") == 0) {
        log<software_error>({__FILE__,__LINE__, "v4l2-ctl cmdRes error executing " + str + ": " + result});
        return "error"; 
    }

    // Return substring after the space, ex: "frame_rate: 1000" becomes "1000"
    size_t space_pos = result.find(' ');
    if (space_pos != std::string::npos) {
        return result.substr(space_pos + 1); 
    }

    return "error"; 
}


inline
int nsvCtrl::getFPS()
{
   const std::string command = "v4l2-ctl --get-ctrl frame_rate -d " + camera_string; 
   std::string result = cmdRes(command.c_str());

   if( result == "error") 
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from getFPS"});
      return -1;
   }

   m_fps = std::stoi(result);

   return 0;
}

inline
int nsvCtrl::setFPS()
{
   int fr_to_set = m_fpsSet; 

   if(fr_to_set < m_minFPS)
   {
      fr_to_set = m_minFPS;
      log<text_log>("FPS limited to min of: " + std::to_string(m_minFPS), logPrio::LOG_WARNING);
   }
   
   if(fr_to_set > m_maxFPS)
   {
      fr_to_set = m_maxFPS;
      log<text_log>("FPS limited to max of = " + std::to_string(m_maxFPS), logPrio::LOG_WARNING);
   }
   
   const std::string command = "v4l2-ctl --set-ctrl frame_rate=" + std::to_string(fr_to_set) + " -d " + camera_string; 
   int result = std::system(command.c_str());
  
   // really should do a v4l2-ctl --get-ctrl=frame_rate (getFPS call) to confirm camera took the new fr
   //getFPS();

   if( result != 0)
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from setFPS setting FPS to: " + std::to_string(fr_to_set)});
      return -1;
   }

   log<text_log>("Set FPS to: " + std::to_string(fr_to_set), logPrio::LOG_WARNING);
   
   return 0;
}


inline 
int nsvCtrl::getExpTime()
{
   const std::string command = "v4l2-ctl --get-ctrl exposure -d " + camera_string; 
   std::string result = cmdRes(command.c_str());

   if( result == "error") 
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from getExpTime"});
      return -1;
   }

   m_expTime = std::stoi(result);

   return 0;
}

inline 
int nsvCtrl::setExpTime()
{
   
   int exp_to_set = m_expTimeSet;  //instead of passing in a parameter, we're going off of a member variable that gets set somewhere else

   if(exp_to_set < m_minExpTime)
   {
      exp_to_set = m_minExpTime;
      log<text_log>("Exp limited to min of: " + std::to_string(m_minExpTime), logPrio::LOG_WARNING);
   }
   
   if(exp_to_set > m_maxExpTime)
   {
      exp_to_set = m_maxExpTime;
      log<text_log>("Exp limited to max of = " + std::to_string(m_maxExpTime), logPrio::LOG_WARNING);
   }
   
   const std::string command = "v4l2-ctl --set-ctrl exposure=" + std::to_string(exp_to_set) + " -d " + camera_string; 
   int result = std::system(command.c_str());

   if( result != 0)
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from setExpTime setting Exp to: " + std::to_string(exp_to_set)});
      return -1;
   }

   log<text_log>("Set Exp to: " + std::to_string(exp_to_set), logPrio::LOG_WARNING);
   
   return 0;
}


inline 
int nsvCtrl::checkNextROI()
{
   return 0;
}

inline 
int nsvCtrl::setNextROI()
{ 
   return 0;
}

//------------------------------------------------------------------------
//-------------------   framegrabber interface ---------------------------
//------------------------------------------------------------------------

inline
int nsvCtrl::configureAcquisition()
{
   //lock mutex
   std::unique_lock<std::mutex> lock(m_indiMutex);

   unsigned int error;

   int x0 = (m_nextROI.x - 0.5*(m_nextROI.w - 1)) + 1;
   int y0 = (m_nextROI.y - 0.5*(m_nextROI.h - 1)) + 1;

   m_cropMode = m_cropModeSet;

   if(m_cropModeSet)
   { 
      //error = SetIsolatedCropModeEx(1, m_nextROI.h, m_nextROI.w, m_nextROI.bin_y, m_nextROI.bin_x, x0, y0);
      // set some ROI or binning parameters

   }
   else
   {          
      //Setup Image dimensions
      /* SetImage(int hbin, int vbin, int hstart, int hend, int vstart, int vend)
       * hbin: number of pixels to bin horizontally
       * vbin: number of pixels to bin vertically
       * hstart: Starting Column (inclusive)
       * hend: End column (inclusive)
       * vstart: Start row (inclusive)
       * vend: End row (inclusive)
       */
      //error = SetImage(m_nextROI.bin_x, m_nextROI.bin_y, x0, x0 + m_nextROI.w - 1, y0, y0 + m_nextROI.h - 1);

      // SetImageROI(int xbin, int ybin, int xstart, int xend, int ystart, int yend)
      
      
   }
   
   // nsv not implementing changing ROI for now. Only 6144x4210 and 6144x512

   m_currentROI.bin_x = m_nextROI.bin_x;
   m_currentROI.bin_y = m_nextROI.bin_y;
   m_currentROI.x = x0 - 1.0 +  0.5*(m_nextROI.w - 1);
   m_currentROI.y = y0 - 1.0 +  0.5*(m_nextROI.h - 1);
   m_currentROI.w = m_nextROI.w;
   m_currentROI.h = m_nextROI.h;
   
   //updateIfChanged( m_indiP_roi_x, "current", m_currentROI.x, INDI_OK);
   //updateIfChanged( m_indiP_roi_y, "current", m_currentROI.y, INDI_OK);
   //updateIfChanged( m_indiP_roi_w, "current", m_currentROI.w, INDI_OK);
   //updateIfChanged( m_indiP_roi_h, "current", m_currentROI.h, INDI_OK);
   //updateIfChanged( m_indiP_roi_bin_x, "current", m_currentROI.bin_x, INDI_OK);
   //updateIfChanged( m_indiP_roi_bin_y, "current", m_currentROI.bin_y, INDI_OK);

   //We also update target to the settable values
   m_nextROI.x = m_currentROI.x;
   m_nextROI.y = m_currentROI.y;
   m_nextROI.w = m_currentROI.w;
   m_nextROI.h = m_currentROI.h;
   m_nextROI.bin_x = m_currentROI.bin_x;
   m_nextROI.bin_y = m_currentROI.bin_y;

   //updateIfChanged( m_indiP_roi_x, "target", m_currentROI.x, INDI_OK);
   //updateIfChanged( m_indiP_roi_y, "target", m_currentROI.y, INDI_OK);
   //updateIfChanged( m_indiP_roi_w, "target", m_currentROI.w, INDI_OK);
   //updateIfChanged( m_indiP_roi_h, "target", m_currentROI.h, INDI_OK);
   //updateIfChanged( m_indiP_roi_bin_x, "target", m_currentROI.bin_x, INDI_OK);
   //updateIfChanged( m_indiP_roi_bin_y, "target", m_currentROI.bin_y, INDI_OK);


   ///\todo This should check whether we have a match between EDT and the camera right?
   m_width = m_currentROI.w/m_currentROI.bin_x;
   m_height = m_currentROI.h/m_currentROI.bin_y;
   m_dataType = _DATATYPE_INT16;  // depends on bitdepth of camera output?

   recordCamera(true);

   return 0;
}

inline
float nsvCtrl::fps()
{
   return m_fps;
}

inline
int nsvCtrl::startAcquisition()
{

   if(startStreaming() == -1){
      state(stateCodes::ERROR);
      return log<software_error,-1>({__FILE__, __LINE__, "nsvCam failed to start acquisition"});
   }
   state(stateCodes::OPERATING);
   recordCamera();
   
   sleep(1); //make sure camera is rully running before we try to synch with it.

   return 0;
}

inline
int nsvCtrl::acquireAndCheckValid()
{
   uint dmaTimeStamp[2];
   int bufferIndex;
   queueBuffer(0);
   waitForFrame();
   bufferIndex = dequeueBuffer();
   
   dmaTimeStamp[0] = 0;  // TODO timing info for cam
   dmaTimeStamp[1] = 1;

   m_currImageTimestamp.tv_sec = dmaTimeStamp[0];
   m_currImageTimestamp.tv_nsec = dmaTimeStamp[1];

   return 0;
}

inline
int nsvCtrl::loadImageIntoStream(void * dest)
{
   if( frameGrabber<nsvCtrl>::loadImageIntoStreamCopy(dest, buffers[0], m_width, m_height, m_typeSize) == nullptr) return -1;

   return 0;
 }

inline
int nsvCtrl::reconfig()
{
   //lock mutex
   //std::unique_lock<std::mutex> lock(m_indiMutex);
   recordCamera(true);
   stopStreaming();
   state(stateCodes::CONFIGURING);

   // add some reconfig stuff here
   startStreaming();
   writeConfig();
   
   state(stateCodes::READY);

   m_nextMode = m_modeName;
   return 0;
}

inline
int nsvCtrl::checkRecordTimes()
{
   return telemeter<nsvCtrl>::checkRecordTimes(telem_stdcam());
}
  
inline
int nsvCtrl::recordTelem( const telem_stdcam * )
{
   return recordCamera(true);
}

}//namespace app
} //namespace MagAOX
#endif
