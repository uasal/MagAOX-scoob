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

#include <chrono>
#include <ctime>

#include <unistd.h>

std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
auto duration = now.time_since_epoch();

typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<8>
>::type> Days; 

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

   static constexpr bool c_stdCamera_fps = true; ///< app::dev config to tell stdCamera not to expose FPS status
   
   static constexpr bool c_stdCamera_synchro = false; ///< app::dev config to tell stdCamera to not expose synchro mode controls

   static constexpr bool c_stdCamera_usesModes = true; ///< app:dev config to tell stdCamera not to expose mode controls
   
   static constexpr bool c_stdCamera_usesROI = true; ///< app:dev config to tell stdCamera to expose ROI controls

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

   std::string m_camPath; ///< /dev/video2 or similar, path to l4v2 cam, read from config

   int m_current_frame; ///< frame index, from 0 to bufsize for current frame to read out
   int m_oldest_frame; ///< the oldest camera frame in the buffer (must be dequeued first)

   int m_bitDepth; // <camera bit depth>

   int m_vCrop; ///< camera vcropoffset, used in sliced mode

   std::vector<void*> ROIbuffers;

public:

   nsvCtrl();
   ~nsvCtrl() noexcept;

   std::string cmdRes(const char* cmd);

   /// Setup the configuration system (called by MagAOXApp::setup())
   virtual void setupConfig();

   /// load the configuration system results (called by MagAOXApp::setup())
   virtual void loadConfig();

   /// Startup functions
   /** Sets up the INDI vars. */
   virtual int appStartup();

   virtual int appLogic();

   virtual int onPowerOff();

   virtual int whilePowerOff();

   virtual int appShutdown();

   int cameraSelect();

   int setReadoutMode();

   int getTemp();

   int setTempControl();

   int getFPS();
   
   int getEMGain();
   
   int setEMGain();

   int setVCrop(int offset);

   int getVCrop();

   int setBitDepth(int bitDepth);

   int getBlacklevel();

   int setBlacklevel();

   int setCropMode();

   int setShutter(unsigned os);

   int setFPS();

   int getExpTime();

   int writeROISubframe();

   int resizeROIbufs();

   //int getBitDepth(); //12, 14, 16

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

   /// Sets exposure
   /**
     * \returns 0 if successful 
     * \returns -1 otherwise
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

   pcf::IndiProperty m_indiP_vCrop; ///< Property for camera frame vertical crop offset
   pcf::IndiProperty m_indiP_bitDepth; ///< Property for camera bit depth
   
public:

   INDI_NEWCALLBACK_DECL(nsvCtrl, m_indiP_vCrop);
   INDI_NEWCALLBACK_DECL(nsvCtrl, m_indiP_bitDepth);

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
   m_current_frame = 0;
   m_oldest_frame = 0;

   m_powerMgtEnabled = false;
   m_powerOnWait = 1;
   std::string m_powerDevice;             ///< The INDI device name of the power controller
   std::string m_powerChannel;            ///< The INDI property name of the channel controlling this device's power.

   int m_powerState;       ///< Current power state, 1=On, 0=Off, -1=Unk.
   int m_powerTargetState; ///< Current target power state, 1=On, 0=Off, -1=Unk.

   //m_startupTemp = -45;  
   
   m_maxEMGain = 360;
   m_emGainSet = 100;
   m_blacklevelSet = 10;
   m_maxBlacklevel = 65535; // assuming 16-bit. Pair with bitdepth when implemented
   m_minBlacklevel = 0;
   m_maxExpTime = 3600000000;
   m_minExpTime = 69;

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
 
   config.add("camera.camPath", "", "camera.camPath", argType::Required, "camera","camPath", false, "str", "The path to the camera.");
   config.add("camera.vcropoffset", "", "camera.vcropoffset", argType::Required, "camera", "vcropoffset", false, "str", "vertical crop offset for camera");
   config.add("camera.bitDepth", "", "camera.bitDepth", argType::Required, "camera", "bitDepth", false, "str", "pixel bit depth");


   dev::stdCamera<nsvCtrl>::setupConfig(config);
   dev::frameGrabber<nsvCtrl>::setupConfig(config);
   dev::telemeter<nsvCtrl>::setupConfig(config);
   
}

inline
void nsvCtrl::loadConfig()
{

   config(m_camPath, "camera.camPath");
   config(m_vCrop, "camera.vcropoffset");
   config(m_bitDepth, "camera.bitDepth");
   dev::stdCamera<nsvCtrl>::loadConfig(config);

   m_configFile = "/tmp/nsv_";
   m_configFile += configName();
   m_configFile += ".cfg";

   m_modeName = m_startupMode;
   m_nextMode = m_modeName;

   //m_default_x = m_cameraModes[m_modeName].m_centerX;
   //m_default_y = m_cameraModes[m_modeName].m_centerY;
   //m_default_w = m_cameraModes[m_modeName].m_sizeX;
   //m_default_h = m_cameraModes[m_modeName].m_sizeY;

   m_full_x = m_cameraModes[m_modeName].m_centerX;
   m_full_y = m_cameraModes[m_modeName].m_centerY;
   m_full_w = m_cameraModes[m_modeName].m_sizeX;
   m_full_h = m_cameraModes[m_modeName].m_sizeY;
   
   m_maxFPS = m_cameraModes[m_modeName].m_maxFPS;
   m_minFPS = m_cameraModes[m_modeName].m_maxFPS;

   m_currentROI.x = m_default_x;
   m_currentROI.y = m_default_y;
   m_currentROI.w = m_default_w;
   m_currentROI.h = m_default_h;
   m_currentROI.bin_x = 1;
   m_currentROI.bin_y = 1;      

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
   // register new indi properties
   getVCrop();
   createStandardIndiNumber<int>(m_indiP_vCrop, "vcropoffset", 25, 3699, 1, "%d");
   m_indiP_vCrop["current"] = m_vCrop;
   m_indiP_vCrop["target"] = m_vCrop;
   registerIndiPropertyNew(m_indiP_vCrop, INDI_NEWCALLBACK(m_indiP_vCrop));

   createStandardIndiNumber<int>(m_indiP_bitDepth, "bitDepth", 10, 16, 2, "%d");
   m_indiP_bitDepth["current"] = m_bitDepth;
   m_indiP_bitDepth["target"] = m_bitDepth;
   registerIndiPropertyNew(m_indiP_bitDepth, INDI_NEWCALLBACK(m_indiP_bitDepth));

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

   //m_powerState = 1;  
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
         state(stateCodes::ERROR);
         return 0;
      }
      */
   
      //if(m_powerState == 0) return 0;


     if(getFPS() < 0 || 
        getEMGain() < 0 ||
        getBlacklevel() < 0 ||
        getExpTime() < 0 ||
        getVCrop() < 0)
      {   
         state(stateCodes::ERROR);
         return 0;
      }

      if(int ps = getPowerStatus() != 0){
         
         /*
         if(ps == 1){
            log<text_log>("Camera in 'No Power' state", logPrio::LOG_CRITICAL);   //refer to V4L2_IN_ST_NO_POWER in videodev2.h
         } else if(ps == 2){
            log<text_log>("Camera in 'No Signal' state", logPrio::LOG_CRITICAL); 
         } else{
            log<text_log>("Camera Bad Status", logPrio::LOG_CRITICAL);   
         }

         state(stateCodes::NODEVICE);
         return 0;
         */
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

   return 0;

}

inline
int nsvCtrl::onPowerOff()
{
   stopStreaming();
   requestBuffers(0);
   closeCamera();
   
   if(m_init)
   {
      m_init = false;
   }

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
   
   m_poweredOn = false;
   m_powerState = 0;

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
   requestBuffers(0);
   closeCamera();
   
   if(m_init)
   {
      m_init = false;
   }
      
   dev::frameGrabber<nsvCtrl>::appShutdown();

   dev::telemeter<nsvCtrl>::appShutdown();
   
   return 0;
}

 
inline
int nsvCtrl::cameraSelect()  
{  
   const char *path = m_camPath.c_str();
   if(openCamera(path) == -1){
      log<text_log>("No nsv camera found on path", logPrio::LOG_CRITICAL);
      state(stateCodes::NODEVICE);
      return -1;
   }

   if(int ps = getPowerStatus() != 0){
      m_powerState = 0;
      m_poweredOn = false;
      /*
         if(ps == 1){
            log<text_log>("Camera in 'No Power' state", logPrio::LOG_CRITICAL);  
         } else if(ps == 2){
            log<text_log>("Camera in 'No Signal' state", logPrio::LOG_CRITICAL); 
         } else{
            log<text_log>("Camera Bad Status", logPrio::LOG_CRITICAL);   
         }

         state(stateCodes::NODEVICE);
         return 0;
         */
   } else {
      m_powerState = 1;
      m_poweredOn = true;
   }

   if(setReadoutMode() == -1){
      log<text_log>("Failed to set camera mode", logPrio::LOG_CRITICAL);
      state(stateCodes::NODEVICE);
      return -1;
   }
   
   if(initCamera(m_cameraModes[m_modeName].m_sizeX,m_cameraModes[m_modeName].m_sizeY,m_bitDepth) == -1){
      log<text_log>("Failed to initialize camera", logPrio::LOG_CRITICAL);
      state(stateCodes::NODEVICE);
      return -1;
   }

   // reload parameters affected by sensor mode
   getVCrop();
   updateIfChanged(m_indiP_vCrop, "current", m_vCrop);
   getExpTime();
   updateIfChanged(m_indiP_exptime, "current", m_expTime);

   CameraParams params = getCameraParams();
   std::cout << "Camera initialized to - Width: " << params.width
                  << ", Height: " << params.height
                  << ", Pixel Format: " << params.pixelFormat << std::endl;

   if(requestBuffers(m_circBuffLength)  == -1 || 
      queryBuffers()     == -1) {
      log<text_log>("Failed to initialize camera buffers", logPrio::LOG_CRITICAL);
      state(stateCodes::NODEVICE);
      return -1;
   }

   resizeROIbufs();

   if(startStreaming() == -1){
      log<text_log>("Failed to start camera stream", logPrio::LOG_CRITICAL);
      state(stateCodes::NODEVICE);
      return -1;
   }

   if(queueBuffers() == -1){ // load up initial images
      log<text_log>("Failed to start queueing images", logPrio::LOG_CRITICAL);
      state(stateCodes::NODEVICE);
      return -1;
   }

   m_current_frame = 0; 
   m_oldest_frame = 0;

   state(stateCodes::CONNECTED);
   log<text_log>(std::string("Connected to ") + m_camPath); //camera_string);

   m_init = true;

   return 0;
}

inline
int nsvCtrl::setReadoutMode()
{
   int result = 0;

   if(m_modeName == "sliced")
   {
      const std::string command = "v4l2-ctl --set-ctrl sensor_mode=1 -d " + m_camPath;
      result = std::system(command.c_str());
   }
   if(m_modeName == "fullframe")
   {
      const std::string command = "v4l2-ctl --set-ctrl sensor_mode=0 -d " + m_camPath;
      result = std::system(command.c_str());
   }

   if(result != 0)
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from setReadoutMode setting mode"}); 
      return -1;
   }

   log<text_log>("Set readout mode to " +  m_modeName);
   std::cout << "Set readout mode to " + m_modeName << std::endl;


   // new camera mode defaults. Should e able to specify an x,y and width, height for each mode rather than global to each
   /*
   m_default_x = m_cameraModes[m_modeName].m_default_x;
   m_default_y = m_cameraModes[m_modeName].m_default_y;
   m_default_w = m_cameraModes[m_modeName].m_default_w;
   m_default_h = m_cameraModes[m_modeName].m_default_w;
   */
   /*
      m_default_x = m_cameraModes[m_modeName].m_centerX;
      m_default_y = m_cameraModes[m_modeName].m_centerY;
      m_default_w = m_cameraModes[m_modeName].m_sizeX;
      m_default_h = m_cameraModes[m_modeName].m_sizeY;
  */ 

   m_full_x = m_cameraModes[m_modeName].m_centerX;
   m_full_y = m_cameraModes[m_modeName].m_centerY;
   m_full_w = m_cameraModes[m_modeName].m_sizeX;
   m_full_h = m_cameraModes[m_modeName].m_sizeY;
   
   m_maxFPS = m_cameraModes[m_modeName].m_maxFPS;
   m_minFPS = m_cameraModes[m_modeName].m_maxFPS;

   // after setting ReadoutMode reset ROI parameters
   m_nextROI.x = m_default_x;
   m_nextROI.y = m_default_y;
   m_nextROI.w = m_default_w;
   m_nextROI.h = m_default_h;
   m_nextROI.bin_x = 1;
   m_nextROI.bin_y = 1;

   m_currentROI.x = m_default_x;
   m_currentROI.y = m_default_y;
   m_currentROI.w = m_default_w;
   m_currentROI.h = m_default_h;
   m_currentROI.bin_x = 1;
   m_currentROI.bin_y = 1;

  // m_readoutSpeedName = m_readoutSpeedNameSet;
   //m_reconfig = true;

   return 0;
}

inline
int nsvCtrl::getTemp()
{
   float temp = -999;

   m_ccdTemp = temp;

   return 0;
}

inline
int nsvCtrl::getEMGain()
{
   const std::string command = "v4l2-ctl --get-ctrl gain -d " + m_camPath;
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
   
   const std::string command = "v4l2-ctl --set-ctrl gain=" + std::to_string(gain_to_set) + " -d " + m_camPath; 
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
int nsvCtrl::setVCrop(int offset)
{
   int vCrop_to_set = offset;  

   if(vCrop_to_set < 25)
   {
      vCrop_to_set = 25;
      log<text_log>("vCrop limited to 25", logPrio::LOG_WARNING);
   }
   
   if(vCrop_to_set > 3699) //TODO make this m_maxvCrop?
   {
      vCrop_to_set = 3699;
      log<text_log>("vCrop limited to 3699", logPrio::LOG_WARNING);
   }
   
   const std::string command = "v4l2-ctl --set-ctrl vcropoffset=" + std::to_string(vCrop_to_set) + " -d " + m_camPath; 
   int result = std::system(command.c_str());
  
   if( result != 0)
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from setvCrop:"});
      return -1;
   }

   log<text_log>("Set vcropoffset to: " + std::to_string(vCrop_to_set), logPrio::LOG_WARNING);
   
   return 0;
}

inline
int nsvCtrl::getVCrop()
{
   const std::string command = "v4l2-ctl --get-ctrl vcropoffset -d " + m_camPath; 
   std::string result = cmdRes(command.c_str());
   if( result == "error") 
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from getvCrop"});
      return -1;
   }

   m_vCrop = std::stoi(result);
   return 0;
}

inline
int nsvCtrl::setBitDepth(int bitDepth)
{
   if(bitDepth != 8 || bitDepth != 10 || bitDepth != 16)
   {
      log<text_log>("invalid input bitDepth. Resettting...", logPrio::LOG_WARNING);
      return -1;
   }
   
   const std::string command = "v4l2-ctl --set-fmt-video=pixelformat=RG" + std::to_string(bitDepth) + " -d " + m_camPath; 
   int result = std::system(command.c_str());
  
   if( result != 0)
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from setBitDepth:"});
      return -1;
   }

   log<text_log>("Set bitDepth to: " + std::to_string(bitDepth), logPrio::LOG_WARNING);
   m_bitDepth = bitDepth;
   
   return 0;
}

inline
int nsvCtrl::getBlacklevel()
{
   const std::string command = "v4l2-ctl --get-ctrl blacklevel -d " + m_camPath;
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

   if(blacklevel_to_set < m_minBlacklevel)
   {
      blacklevel_to_set = m_minBlacklevel;
      log<text_log>("Blacklevel limited to " + std::to_string(blacklevel_to_set), logPrio::LOG_WARNING);
   }
   
   if(blacklevel_to_set > m_maxBlacklevel)
   {
      blacklevel_to_set = m_maxBlacklevel;
      log<text_log>("Blacklevel limited to maxBlacklevel = " + std::to_string(blacklevel_to_set), logPrio::LOG_WARNING);
   }
   
   const std::string command = "v4l2-ctl --set-ctrl blacklevel=" + std::to_string(blacklevel_to_set) + " -d " + m_camPath; 
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

   int w = m_currentROI.w;
   int h = m_currentROI.h;
   
   fout << "camera_class:                  \"nsvCam\"\n";
   fout << "width:                         " << w << "\n";
   fout << "height:                        " << h << "\n";
   fout << "depth:                         " << m_bitDepth << "\n";
   fout << "mode:                          " << m_modeName << "\n";
   fout << "blacklevel:                    " << m_blacklevel << "\n";
   fout << "gain:                          " << m_emGain << "\n";
   fout << "v crop:                        " << m_vCrop << "\n";
   fout << "ROI.w:                         " << m_currentROI.w << "\n";
   fout << "ROI.h:                         " << m_currentROI.h << "\n";
   fout << "ROI.x:                         " << m_currentROI.x << "\n";
   fout << "ROI.y:                         " << m_currentROI.y  << "\n";
   
   fout.close();
   
   return 0;

}
//------------------------------------------------------------------------
//-----------------------  stdCamera interface ---------------------------
//------------------------------------------------------------------------

inline
int nsvCtrl::powerOnDefaults()
{
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
   
  // since 'power off' nukes the camera buffers & stream, set reconfig here?

   return 0;
}

inline
int nsvCtrl::setTempControl()
{
   return 0;   // todo
}

inline
int nsvCtrl::getFPS()
{
   const std::string command = "v4l2-ctl --get-ctrl frame_rate -d " + m_camPath; 
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
   
   const std::string command = "v4l2-ctl --set-ctrl frame_rate=" + std::to_string(fr_to_set) + " -d " + m_camPath; 
   int result = std::system(command.c_str());
  
   //getFPS(); pull framerate to see what it set

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
   const std::string command = "v4l2-ctl --get-ctrl exposure -d " + m_camPath; 
   std::string result = cmdRes(command.c_str());

   if( result == "error") 
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from getExpTime"});
      return -1;
   }

   m_expTime = std::stoi(result) / 1000000.0;  // convert us to s

   return 0;
}

inline 
int nsvCtrl::setExpTime()
{
   
   int exp_to_set = m_expTimeSet * 1000000;  // convert s to us, use indi var

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
   
   const std::string command = "v4l2-ctl --set-ctrl exposure=" + std::to_string(exp_to_set) + " -d " + m_camPath; 
   int result = std::system(command.c_str());

   if( result != 0)
   {
      log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from setExpTime setting Exp to: " + std::to_string(exp_to_set)});
      return -1;
   }

   log<text_log>("Set Exp to: " + std::to_string(exp_to_set / 1000000.0), logPrio::LOG_WARNING);
   
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
   m_reconfig = true; 

   updateSwitchIfChanged(m_indiP_roi_set, "request", pcf::IndiElement::Off, INDI_IDLE);

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

   int x0 = (m_nextROI.x - 0.5*(m_nextROI.w - 1)) + 1;
   int y0 = (m_nextROI.y - 0.5*(m_nextROI.h - 1)) + 1;

   m_currentROI.bin_x = m_nextROI.bin_x;
   m_currentROI.bin_y = m_nextROI.bin_y;
   m_currentROI.x = x0 - 1.0 +  0.5*(m_nextROI.w - 1);
   m_currentROI.y = y0 - 1.0 +  0.5*(m_nextROI.h - 1);
   m_currentROI.w = m_nextROI.w;
   m_currentROI.h = m_nextROI.h;
   
   updateIfChanged( m_indiP_roi_x, "current", m_currentROI.x, INDI_OK);
   updateIfChanged( m_indiP_roi_y, "current", m_currentROI.y, INDI_OK);
   updateIfChanged( m_indiP_roi_w, "current", m_currentROI.w, INDI_OK);
   updateIfChanged( m_indiP_roi_h, "current", m_currentROI.h, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_x, "current", m_currentROI.bin_x, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_y, "current", m_currentROI.bin_y, INDI_OK);
  
   m_nextROI.x = m_currentROI.x;
   m_nextROI.y = m_currentROI.y;
   m_nextROI.w = m_currentROI.w;
   m_nextROI.h = m_currentROI.h;
   m_nextROI.bin_x = m_currentROI.bin_x;
   m_nextROI.bin_y = m_currentROI.bin_y;

   updateIfChanged( m_indiP_roi_x, "target", m_currentROI.x, INDI_OK);
   updateIfChanged( m_indiP_roi_y, "target", m_currentROI.y, INDI_OK);
   updateIfChanged( m_indiP_roi_w, "target", m_currentROI.w, INDI_OK);
   updateIfChanged( m_indiP_roi_h, "target", m_currentROI.h, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_x, "target", m_currentROI.bin_x, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_y, "target", m_currentROI.bin_y, INDI_OK);


   m_width = m_currentROI.w/m_currentROI.bin_x; //m_default_w
   m_height = m_currentROI.h/m_currentROI.bin_y; //m_default_h
   //m_dataType = _DATATYPE_INT16;  // depends on bitdepth of camera output. assume 16-bit 
   m_dataType = IMAGESTRUCT_UINT16;
   m_typeSize = imageStructDataType<IMAGESTRUCT_UINT16>::size;

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
   
   //sleep(1); //make sure camera is rully running before we try to synch with it.

   return 0;
}

inline
int nsvCtrl::acquireAndCheckValid()
{
   if(m_init)
   {
      uint dmaTimeStamp[2];

      m_current_frame = dequeueBuffer(m_oldest_frame);  // cam forces you to read oldest frame in the buffer first
      if(m_current_frame == -1){
         state(stateCodes::ERROR);
         return log<software_error,-1>({__FILE__, __LINE__, "nsvCam failed to dequeue frame"});
      }
      queueBuffer(m_current_frame); // queue into index just read from

      writeROISubframe(); 

      m_oldest_frame++;
      if(m_oldest_frame > bufferCount - 1){
         m_oldest_frame = 0; 
      }

      time_t seconds = time(0); 
      auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);

      dmaTimeStamp[0] = seconds;  // timing info for cam
      dmaTimeStamp[1] = nanoseconds.count();

      m_currImageTimestamp.tv_sec = dmaTimeStamp[0];
      m_currImageTimestamp.tv_nsec = dmaTimeStamp[1];

   }

   return 0;
}

inline
int nsvCtrl::loadImageIntoStream(void * dest)
{
   //if( frameGrabber<nsvCtrl>::loadImageIntoStreamCopy(dest, buffers[m_current_frame], m_width, m_height, m_typeSize) == nullptr) return -1;
   if( frameGrabber<nsvCtrl>::loadImageIntoStreamCopy(dest, ROIbuffers[m_current_frame], m_currentROI.w, m_currentROI.h, m_typeSize) == nullptr) return -1;
   return 0;
}

inline
int nsvCtrl::writeROISubframe()
{
   
   uint16_t* imagePtr = static_cast<uint16_t*>(buffers[m_current_frame]);
   uint16_t* roiPtr = static_cast<uint16_t*>(ROIbuffers[m_current_frame]);

   // shouldn't have to threshold these here. Should have some indi blocker for invalid x and y...?
   if(m_currentROI.x - (m_currentROI.w / 2) < 0){
      m_currentROI.x = (m_currentROI.w / 2);
   } else if (m_currentROI.x + (m_currentROI.w / 2) > m_full_w - 1){
      m_currentROI.x = (m_full_w - (m_currentROI.w / 2)) - 1;
   }
   if(m_currentROI.y - (m_currentROI.h / 2) < 0){
      m_currentROI.y = (m_currentROI.h / 2);
   } else if (m_currentROI.y + (m_currentROI.h / 2) > m_full_h - 1){
      m_currentROI.y = (m_full_h - (m_currentROI.h / 2)) - 1;
   }

   int startX = m_currentROI.x - (m_currentROI.w / 2) + 0.5;
   int startY = m_currentROI.y - (m_currentROI.h / 2) + 0.5;

   int ROIIndex = 0;
   for(int i = 0; i < m_currentROI.h; i++){
      for(int j = 0; j < m_currentROI.w; j++){
         roiPtr[ROIIndex] = static_cast<uint16_t*>(imagePtr)[((startY + i) * m_full_w + (startX + j))];
         ROIIndex++;
      }
   }

   // cleanup roiPtr or imagePtr?

   return 0;
}

inline
int nsvCtrl::resizeROIbufs()
{  
   for (void* ptr : ROIbuffers) {
        delete[] static_cast<uint16_t*>(ptr);  
    }
   ROIbuffers.clear();
   ROIbuffers.resize(m_circBuffLength);

   int bufSize = m_currentROI.w * m_currentROI.h;

   for(int i = 0; i < (int)m_circBuffLength; i++){

      void* buffer = malloc(bufSize * sizeof(uint16_t));  // assuming 16-bit. verify w/ bufferSize in queryBuffer
      if (buffer == nullptr) {
         std::cerr << "Memory allocation failed!" << std::endl;
        return -1;
      }

      ROIbuffers[i] = buffer;
   }

   return 0;
}

inline
int nsvCtrl::reconfig()
{

   //lock mutex
   std::unique_lock<std::mutex> lock(m_indiMutex);
   recordCamera(true);
   state(stateCodes::CONFIGURING);
   printf("Reconfiguring camera . . .\n");

   // check for new mode
   if(m_nextMode != m_modeName)
   {
      if(m_init)
      {
         m_init = false;
      }

      if(stopStreaming() < 0){
         log<text_log>("Camera in 'No Power' state", logPrio::LOG_CRITICAL);
         state(stateCodes::NODEVICE);
         return -1;
      }
      requestBuffers(0);
      m_modeName = m_nextMode;  // set mode before camera reinit
      cameraSelect();   // camera initialization & stream init
      m_init = true;
   }

   state(stateCodes::CONNECTED);

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

INDI_NEWCALLBACK_DEFN(nsvCtrl, m_indiP_vCrop)(const pcf::IndiProperty &ipRecv)
{
   if (ipRecv.getName() != m_indiP_vCrop.getName())
   {
      log<software_error>({__FILE__, __LINE__, "wrong INDI property received."});
      return -1;
   }

   int vc = 0;

   if (ipRecv.find("current"))
   {
      vc = ipRecv["current"].get<int>();
   }

   if (ipRecv.find("target"))
   {
      vc = ipRecv["target"].get<int>();
   }

   // check for min/max values? set those on cam startup when querying params

   std::unique_lock<std::mutex> lock(m_indiMutex);
   m_vCrop = vc;
   int rv = setVCrop(vc);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting vcropoffset!"});
      return -1;
   }

   getVCrop();

   updateIfChanged(m_indiP_vCrop, "target", vc);
   updateIfChanged(m_indiP_vCrop, "current", m_vCrop);

   return 0;
}

INDI_NEWCALLBACK_DEFN(nsvCtrl, m_indiP_bitDepth)(const pcf::IndiProperty &ipRecv)
{
   if (ipRecv.getName() != m_indiP_bitDepth.getName())
   {
      log<software_error>({__FILE__, __LINE__, "wrong INDI property received."});
      return -1;
   }

   int vc = 0;

   if (ipRecv.find("current"))
   {
      vc = ipRecv["current"].get<int>();
   }

   if (ipRecv.find("target"))
   {
      vc = ipRecv["target"].get<int>();
   }

   std::unique_lock<std::mutex> lock(m_indiMutex);
   m_bitDepth = vc;
   int rv = setBitDepth(vc);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting bitDepth!"});
      return -1;
   }

   updateIfChanged(m_indiP_bitDepth, "target", vc);
   updateIfChanged(m_indiP_bitDepth, "current", m_bitDepth);

   return 0;
}

// piping through shell commands. Todo convert to ioctl v4l2 calls per parameter
std::string nsvCtrl::cmdRes(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Check for errors, for example:
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

}//namespace app
} //namespace MagAOX
#endif
