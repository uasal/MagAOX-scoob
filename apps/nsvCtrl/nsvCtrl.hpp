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
   
   static constexpr bool c_stdCamera_temp = true; ///< app::dev config to tell stdCamera to expose temperature
   
   static constexpr bool c_stdCamera_readoutSpeed = false; ///< app::dev config to tell stdCamera to expose readout speed controls
   
   static constexpr bool c_stdCamera_vShiftSpeed = false; ///< app:dev config to tell stdCamera to expose vertical shift speed control
   
   static constexpr bool c_stdCamera_emGain = true; ///< app::dev config to tell stdCamera to expose EM gain controls 

   static constexpr bool c_stdCamera_blacklevel = true; ///< app::dev config to tell stdCamera to expose Blacklevel controls 

   static constexpr bool c_stdCamera_exptimeCtrl = true; ///< app::dev config to tell stdCamera to expose exposure time controls
   
   static constexpr bool c_stdCamera_fpsCtrl = true; ///< app::dev config to tell stdCamera to not expose FPS controls

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

   std::chrono::duration<double> m_powerOnDuration;
   std::chrono::time_point<std::chrono::high_resolution_clock> m_powerOnTime;
   std::chrono::time_point<std::chrono::high_resolution_clock> m_powerOffTime;
   int m_powerCycles;

   std::string m_powerOnTS;
   std::string m_powerOffTS;
   std::string m_poweredOnDuration;

   std::string m_camID; // ID encoded in the camera (necessary to pair with path)
   std::string m_camPath; // dev/videoX

   int m_current_frame; ///< frame index, from 0 to bufsize for current frame to read out
   int m_oldest_frame; ///< the oldest camera frame in the buffer (must be dequeued first)

   int m_bitDepth; // <camera bit depth>

   bool m_power {false}; 

   int m_vCrop; ///< camera vcropoffset, used in sliced mode

   int m_xStartPos;
   int m_yStartPos;

   float m_minEMGain; // no min defined in stdCamera. Can assume 0?
   int m_minVCrop;
   int m_maxVCrop;
   int m_maxYStartPos;
   int m_minYStartPos;
   int m_maxXStartPos;
   int m_minXStartPos;

   std::vector<void*> ROIbuffers;

   // booleans for potential config values in camera... could do away with these but would be uglier
   bool uses_vCrop = false; 
   bool uses_fpgaPower = false;

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

   int getPowerStatus();

   int setTempControl();

   int getFPS();
   
   int getEMGain();
   
   int setEMGain();

   int setVCrop(int offset);

   int getVCrop();

   int getXStartPos();

   int setXStartPos(int pos);
   
   int getYStartPos();
   
   int setYStartPos(int pos);

   int setBitDepth(int bitDepth);

   int getBlacklevel();

   int setBlacklevel();

   int setCropMode();

   int set_preferred_stride(int stride);

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
   pcf::IndiProperty m_indiP_power;
   pcf::IndiProperty m_indiP_power_status;

public:

   INDI_NEWCALLBACK_DECL(nsvCtrl, m_indiP_vCrop);
   INDI_NEWCALLBACK_DECL(nsvCtrl, m_indiP_bitDepth);
   INDI_NEWCALLBACK_DECL(nsvCtrl, m_indiP_power);

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
   m_powerOnWait = 0;
   std::string m_powerDevice;             ///< The INDI device name of the power controller
   std::string m_powerChannel;            ///< The INDI property name of the channel controlling this device's power.

   m_powerState = -1;       ///< Current power state, 1=On, 0=Off, -1=Unk.
   //int m_powerTargetState = -1; ///< Current target power state, 1=On, 0=Off, -1=Unk.

   //m_startupTemp = -45;  
   
   // overwrite these once camera powered on, set a mode, & loaded camera params
   m_maxEMGain = 360;
   m_minEMGain = 0;
   m_emGainSet = 100; //default

   m_blacklevelSet = 10;
   m_maxBlacklevel = 65535; // assuming 16-bit. Pair with bitdepth when implemented
   m_minBlacklevel = 0;
   
   m_maxExpTime = 3600000000;
   m_minExpTime = 69;

   m_minFPS = 0;
   m_maxFPS = 99999999999;

   m_minVCrop = 0;
   m_maxVCrop = 99999999;

   m_maxYStartPos = 9999999;
   m_maxXStartPos = 99999999;
   m_xStartPos = 0;

   m_minYStartPos = 0;
   m_minXStartPos = 0;
   m_yStartPos = 0;

   // fps, expsoure, black level, gain, 
   // roi start pos, ver start pos, end pos, etc.
   m_powerCycles = 0;

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
 
   config.add("camera.camID", "", "camera.camID", argType::Required, "camera","camID", false, "str", "v4l2 Card Type identifyer for camera.");
   config.add("camera.vcropoffset", "", "camera.vcropoffset", argType::Required, "camera", "vcropoffset", false, "int", "vertical crop offset for camera");
   config.add("camera.bitDepth", "", "camera.bitDepth", argType::Required, "camera", "bitDepth", false, "int", "pixel bit depth");
   config.add("camera.power", "", "camera.power", argType::Optional, "camera", "power", false, "bool", "camera power"); // TODO make toggle

   dev::stdCamera<nsvCtrl>::setupConfig(config);
   dev::frameGrabber<nsvCtrl>::setupConfig(config);
   dev::telemeter<nsvCtrl>::setupConfig(config);
   
}

inline
void nsvCtrl::loadConfig()
{
   config(m_camID, "camera.camID");
   config(m_vCrop, "camera.vcropoffset");
   config(m_bitDepth, "camera.bitDepth");
   config(m_power, "camera.power");
   dev::stdCamera<nsvCtrl>::loadConfig(config);

   m_configFile = "/tmp/nsv_";
   m_configFile += configName();
   m_configFile += ".cfg";

   m_modeName = m_startupMode;
   m_nextMode = m_modeName;

   m_full_x = m_cameraModes[m_modeName].m_centerX;
   m_full_y = m_cameraModes[m_modeName].m_centerY;
   m_full_w = m_cameraModes[m_modeName].m_sizeX;
   m_full_h = m_cameraModes[m_modeName].m_sizeY;
   
   m_maxFPS = m_cameraModes[m_modeName].m_maxFPS; // config defaults. actual camera will be different
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
   
   m_camPath = findCameraByID(m_camID);
   
   dev::frameGrabber<nsvCtrl>::loadConfig(config);
   
   dev::telemeter<nsvCtrl>::loadConfig(config);

}

inline
int nsvCtrl::appStartup()
{
   // register new indi properties
   if(config.isSet("camera.vcropoffset"))
   {
      getVCrop();
   }
   createStandardIndiNumber<int>(m_indiP_vCrop, "vcropoffset", 25, 3699, 1, "%d");
   m_indiP_vCrop["current"] = m_vCrop;
   m_indiP_vCrop["target"] = m_vCrop;
   registerIndiPropertyNew(m_indiP_vCrop, INDI_NEWCALLBACK(m_indiP_vCrop));

   createStandardIndiNumber<int>(m_indiP_bitDepth, "bitDepth", 10, 16, 2, "%d");
   m_indiP_bitDepth["current"] = m_bitDepth;
   m_indiP_bitDepth["target"] = m_bitDepth;
   registerIndiPropertyNew(m_indiP_bitDepth, INDI_NEWCALLBACK(m_indiP_bitDepth));

   createStandardIndiToggleSw( m_indiP_power, "power");
   registerIndiPropertyNew( m_indiP_power, INDI_NEWCALLBACK(m_indiP_power));

   createROIndiText(m_indiP_power_status, "power_status", "power_status", "power_status", "power_status", "power_status");
   indi::addTextElement(m_indiP_power_status, "last_power_on", "Power On TS:");
   indi::addTextElement(m_indiP_power_status, "last_power_off", "Power Off TS:");
   indi::addTextElement(m_indiP_power_status, "on_duration", "Total Time On");
   indi::addTextElement(m_indiP_power_status, "power_cycles", "# Power Cycles");
   m_indiP_power_status["last_power_on"] = m_powerOnTS;
   m_indiP_power_status["last_power_off"] = m_powerOffTS;
   m_indiP_power_status["on_duration"] = m_poweredOnDuration;
   m_indiP_power_status["power_cycles"] = std::to_string(m_powerCycles);
   registerIndiPropertyReadOnly(m_indiP_power_status);

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

   //state(stateCodes::NOTCONNECTED);

   state(stateCodes::POWEROFF); //haven't powered on yet

   m_powerState = 0;  
   //m_powerTargetState = 1;

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

   if( state() == stateCodes::POWEROFF) return 0;

   if( state() == stateCodes::POWERON)  // nothing is happening in here... why??
   {
      //turn_on_power();
      //sleep(10);
      printf("turning on camera\n");
      log<text_log>("Powering on camera", logPrio::LOG_NOTICE);
      state(stateCodes::NOTCONNECTED);
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
     /* if(m_poweredOn)
      {
         m_poweredOn = false;
         if(powerState() != 1 || powerStateTarget() != 1) return 0;
      } */
   }

   if( state() == stateCodes::READY || state() == stateCodes::OPERATING )
   {
      //Get a lock if we can
      std::unique_lock<std::mutex> lock(m_indiMutex, std::try_to_lock);

      //but don't wait for it, just go back around.
      if(!lock.owns_lock()) return 0;
   
      if(m_powerState == 0) return 0;

      /* Update power data */
      m_powerOnDuration = std::chrono::high_resolution_clock::now() - m_powerOnTime;
   
      m_poweredOnDuration = std::to_string(m_powerOnDuration.count());
      updateIfChanged(m_indiP_power_status, "on_duration", m_poweredOnDuration);
      
      //updateIfChanged(m_indiP_power_data, std::vector<std::string>({"last_power_on", "last_power_off", "on_duration", "power_cycles"}), 
      //                              std::vector<std::string>({m_powerOnTS, m_powerOffTS, m_poweredOnDuration, std::to_string(m_powerCycles)}));

      if(getFPS() < 0 || 
        getEMGain() < 0 ||
        getBlacklevel() < 0 || // currently returning 1 hard-coded when value not found in camera... so return val is not useful
        getExpTime() < 0 ||
        uses_vCrop ? getVCrop() < 0 : 0 ||
        c_stdCamera_temp ? getTemp() < 0 : 0)
      {   
         if(m_powerState == 0) return 0;

         state(stateCodes::ERROR);
         return 0;
      }

      if(getPowerStatus() < 0 && getPowerStatus() != PARAM_NOT_FOUND){
         log<text_log>("Camera in 'No Power' state", logPrio::LOG_CRITICAL);  
         state(stateCodes::NODEVICE);
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

   return 0;

}

inline
int nsvCtrl::onPowerOff()
{
   printf("onPowerOff called \n");

   m_powerOnCounter = 0;
   m_poweredOn = false;
   m_powerState = 0;
   m_shutterStatus = "POWEROFF";
   m_shutterState = 0;

   std::lock_guard<std::mutex> lock(m_indiMutex);

   state(stateCodes::POWEROFF);
   stopStreaming();
   requestBuffers(0);
   closeCamera();
   turn_off_power();
   sleep(3);

   if(stdCamera<nsvCtrl>::onPowerOff() < 0)
   {
      log<software_error>({__FILE__, __LINE__});
   }

   if(frameGrabber<nsvCtrl>::onPowerOff() < 0)
   {
      log<software_error>({__FILE__, __LINE__});
   }

   //m_init = false;
   if(m_init)
   {
      m_init = false;
   }

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
   printf("appShutdown\n");
   if(m_init)
   {
      stopStreaming();
      requestBuffers(0);
      closeCamera();
      turn_off_power();
      m_init = false;
   }
      
   dev::frameGrabber<nsvCtrl>::appShutdown();

   dev::telemeter<nsvCtrl>::appShutdown();
   
   return 0;
}

 
inline
int nsvCtrl::cameraSelect()  
{  
   if(openCamera(m_camPath.c_str()) == -1){
      log<text_log>("No nsv camera found matching id", logPrio::LOG_CRITICAL);
      state(stateCodes::NODEVICE);
      return -1;
   }

   // after connecting to the camera, we have polled all values and need to reconfigure INDI
   // Could connect to the camera and poll values with the fd prior to streaming, however this could be dangerous? Wait for power on for now

   getPowerStatus();
   if(!m_powerState) {
      state(stateCodes::NODEVICE);
      return log<text_log,-1>("Camera in 'No Power' state", logPrio::LOG_CRITICAL);
   } 

   // need to get valid modes first before setting mode

   if(setReadoutMode() == -1){
      log<text_log>("Failed to set camera mode", logPrio::LOG_CRITICAL);
      state(stateCodes::NODEVICE);
      return -1;
   }

   // before initializing camera, need to pull settings for new mode
   
   /* including this in the setReadoutMode call, verify x's, y's, after setting. After true ROI is implemented might rework this
   if(setCamImageFormat(m_cameraModes[m_modeName].m_sizeX,m_cameraModes[m_modeName].m_sizeY,m_bitDepth) == -1){
      log<text_log>("Failed to initialize camera", logPrio::LOG_CRITICAL);
      state(stateCodes::NODEVICE);
      return -1;
   }
   */

   // reload parameters affected by sensor mode
   if(uses_vCrop){
      getVCrop();
      updateIfChanged(m_indiP_vCrop, "current", m_vCrop);
   }
   getExpTime();
   updateIfChanged(m_indiP_exptime, "current", m_expTime);

   printf("Camera initialized to - Width: %d,", camera_modes[0].resolutions[camera_modes[0].current_resolution].first);
   printf(" Height: %d,", camera_modes[0].resolutions[camera_modes[0].current_resolution].second);
   printf(" Bit depth: %d\n", camera_modes[0].bitDepth);

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
   log<text_log>(std::string("Connected to ") + m_camID);

   m_init = true;

   return 0;
}

/*
   Old firmware for IMX 571 requires setting a sensor_mode parameter to switch between a certain width/height 

   Newer firmware allows --set-fmt-video=width=1280,height=720 type call and will automatically change modes,
   although there is some vagueness when it comes to width/height priority. CameraMode struct has list of resolutions
   w/ indexer current_resolution and can call updateCurrentMode() to update current_resolution index. 
   
   Assumes resolutions doesn't change. When switching cameras, use getCameraModes() for list of valid resolutions

   if using "Modes" and m_modeName as an indexer, need to match valid modes on camera in a MagAOX config file.  
   Could generically support setting width/height from INDI and report back what sensor mode the camera set,
   however this would not be backwards compatible with old firmware.  
   
   Want to preserve camera mode/resolution settings for experiments, so better to contain in MagAOX conf file
   with appropriate 'startupMode' value and categories for each mode. Not ideal/ requires extra work since each
   camera config has to be rewritten whenever there's a firmware update.
   
   There may be a better solution to dynamically create modes and maintain reproducibilty/consistency
*/

// maybe should accept a width/height input from user and programatically decide what mode to set (obscure from user)
// and just report the new max framerate when switching ROIs. as well as report new h/w limits within mode
inline
int nsvCtrl::setReadoutMode()
{
   int result = 0;

   auto it = camera_controls.find("sensor_mode");
   if(it != camera_controls.end()){ 
      if(m_modeName == "sliced")  // keep sliced and fullframe config to indicate mode for old firmware for now
      {
         const std::string command = "v4l2-ctl --set-ctrl sensor_mode=1 -d " + m_camID;
         result = std::system(command.c_str());
      }
      if(m_modeName == "fullframe")
      {
         const std::string command = "v4l2-ctl --set-ctrl sensor_mode=0 -d " + m_camID;
         result = std::system(command.c_str());
      }

      if(result != 0)
      {
         log<software_error>({__FILE__,__LINE__, "v4l2-ctl error from setReadoutMode setting mode"}); 
         return -1;
      }
   }

   log<text_log>("Setting readout mode to " +  m_modeName);

   // each mode has defined width & height so call set-fmt to configure. Could make this more flexible.
   // after set-fmt confirm the output values for width/height match input config, otherwise warn user,
   //    then update max width and height using output from camera. 


   // to diagnose issues with setting format- use "v4l2-ctl --set-fmt-video=width=1280,height=720 --verbose" optional bitDepth
   //    then check outputs -> VIDIOC_QUERYCAP: ok   VIDIOC_G_FMT: ok   VIDIOC_S_FMT: ok
   
   // not generically supporting setting width and height. Will need to change when Neutralino adds true ROI support. Still doing pseudo-ROI
   printf("width: %d height: %d\n", m_cameraModes[m_modeName].m_sizeX, m_cameraModes[m_modeName].m_sizeY);
   setCamImageFormat(m_cameraModes[m_modeName].m_sizeX, m_cameraModes[m_modeName].m_sizeY, m_bitDepth); 
   sleep(2);
   updateCameraControls();  // update controls after updating image format
   updateCurrentMode(); 

   // update sensor parameters after camera image format
   // bypass_mode
   // override_enable
   // height_align
   // size_align
   // preferred_stride 
   set_preferred_stride(32);

   // now update local variables.  some redundancy with CameraControl struct
   int width = camera_modes[0].resolutions[camera_modes[0].current_resolution].first;
   int height = camera_modes[0].resolutions[camera_modes[0].current_resolution].second;

   m_full_w = width;
   m_full_h = height;

   printf("width :%d, height: %d\n", width, height);

   m_maxFPS = (1 / camera_modes[0].intervals[camera_modes[0].current_resolution]);  // would get from CameraControl frame_rate, but 571 doesn't report correctly

   // could directly reference these via camera_controls["frame_rate"].maximum but
   auto fr = camera_controls.find("frame_rate"); // exposure time and framerate are tied, so when you update one you must update the other
   if(fr != camera_controls.end()){
      m_minFPS = fr->second.minimum / 1000000;  
      m_maxFPS = fr->second.maximum / 1000000;
   } 

   auto bl = camera_controls.find("blacklevel"); 
   if(bl != camera_controls.end()){
      m_minBlacklevel = bl->second.minimum; 
      m_maxBlacklevel = bl->second.maximum;
   }

   auto blv = camera_controls.find("black_level");
   if(blv != camera_controls.end()){
      m_minBlacklevel = blv->second.minimum; 
      m_maxBlacklevel = blv->second.maximum;
   }

   auto gn = camera_controls.find("gain");
   if(gn != camera_controls.end()){
      m_minEMGain = gn->second.minimum;  
      m_maxEMGain = gn->second.maximum;
   }

   auto xp = camera_controls.find("exposure");
   if(xp != camera_controls.end()){
      m_minExpTime = xp->second.minimum;  
      m_maxExpTime = xp->second.maximum;
   } 

   auto vc = camera_controls.find("vcropoffset");
   if(vc != camera_controls.end()){
      uses_vCrop = true;
      m_minVCrop = vc->second.minimum;  
      m_maxVCrop = vc->second.maximum;
      m_vCrop = vc->second.current_value;
      // TODO check if it's already been added to indi../
      createStandardIndiNumber<int>(m_indiP_vCrop, "vcropoffset", m_minVCrop, m_maxVCrop, vc->second.step, "%d");  // need to check if already defined
      m_indiP_vCrop["current"] = m_vCrop;
      m_indiP_vCrop["target"] = m_vCrop;
      registerIndiPropertyNew(m_indiP_vCrop, INDI_NEWCALLBACK(m_indiP_vCrop)); // only create if already created
   } else {
      uses_vCrop = false;
   }

   auto hs = camera_controls.find("roi_hor_start_pos");
   if(hs != camera_controls.end()){
      m_minXStartPos = hs->second.minimum;  
      m_maxXStartPos = hs->second.maximum;
   } 

   auto vs = camera_controls.find("roi_ver_start_pos");
   if(vs != camera_controls.end()){
      m_minYStartPos = vs->second.minimum;  
      m_maxYStartPos = vs->second.maximum;
   } 

   m_full_x = width / 2; // get from sensor not from config
   m_full_y = height / 2;

   // after setting ReadoutMode reset ROI parameters
   
   /* TODO these defaults will segfault the program if they're out of bounds for one of the modes. 
      Want to preserve ROI across modes, however still need to check valid x,y w,h when changing modes.
      What would be preferable is 'global' defaults in addition to mode-based defaults. 
      To truly preserve ROI, need to account for vcropoffset parameter as well for y... */

   m_currentROI.x = m_nextROI.x = m_default_x;
   m_currentROI.y = m_nextROI.y = m_default_y;
   m_currentROI.w = m_nextROI.w = m_default_w;
   m_currentROI.h = m_nextROI.h = m_default_h;
   m_currentROI.bin_x = m_nextROI.bin_x = 1;
   m_currentROI.bin_y = m_nextROI.bin_y = 1;

   if(m_default_x + (m_default_w / 2) > m_full_w || m_default_x - (m_default_w / 2) < 0)
   {
      m_currentROI.x = m_nextROI.x = m_full_x;
      log<text_log>("Invalid default x with current width and height in mode. Setting to center x", logPrio::LOG_WARNING);
   }
   if(m_default_y + (m_default_h / 2) > m_full_h || m_default_y - (m_default_y / 2) < 0)
   {
      m_currentROI.y = m_nextROI.y = m_full_y;
      log<text_log>("Invalid default y with current width and height in mode. Setting to center y", logPrio::LOG_WARNING);
   }
   if(m_default_w > m_full_w)
   {
      m_currentROI.w = m_nextROI.w = m_full_w;
      log<text_log>("Invalid default w with current mode. Setting to max width", logPrio::LOG_WARNING);
   }
   if(m_default_h > m_full_h)
   {
      m_currentROI.h = m_nextROI.h = m_full_h;
      log<text_log>("Invalid default h with current mode. Setting to max height", logPrio::LOG_WARNING);
   }

   printf("Set readout mode to: %s\n", m_modeName.c_str());

   return 0;
}

inline
int nsvCtrl::getTemp()
{
   int res = getAndUpdateSingleControlVal("get_fpga_temperature");  // values range from -1 to 15
   if(res == PARAM_NOT_FOUND){
      // handle when we can't see power explicitly.  Assume turned on..
      //m_poweredOn = true;
      //m_powerState = 1;
      return PARAM_NOT_FOUND;
   }
   m_ccdTemp = res;
   return res < 0 ? log<software_error,-1>({__FILE__, __LINE__, "failed to get fpga temperature"}) : m_ccdTemp;
}

inline
int nsvCtrl::getPowerStatus()
{
   int res = getAndUpdateSingleControlVal("get_fpga_power_status");  // values range from -1 to 15
   if(res == PARAM_NOT_FOUND){
      // handle when we can't see power explicitly.  Assume turned on..
      m_poweredOn = true;
      m_powerState = 1;
      return PARAM_NOT_FOUND;
   }

   // TODO delete this block once they actually make the fpga report correctly
   m_poweredOn = true;
   m_powerState = 1;
   return 1;

   if(res > 0){
      m_poweredOn = true;
      m_powerState = 1;
   } else if(res == 0){
      m_poweredOn = false;
      m_powerState = 0;
   } else {
      m_poweredOn = false;
      m_powerState = -1;
   }
   return res < 0 ? log<software_error,-1>({__FILE__, __LINE__, "failed to get power state"}) : m_powerState;
}

inline
int nsvCtrl::getFPS()
{
   m_fps = getAndUpdateSingleControlVal("frame_rate") / 1000000.0; 
   if(m_fps == PARAM_NOT_FOUND){ 
      return 1;
   }
   return m_fps < 0 ? log<software_error,-1>({__FILE__, __LINE__, "nsvCam failed to dequeue frame"}) : m_fps;
}

inline
int nsvCtrl::setFPS()
{
   if(m_fpsSet > m_maxFPS || m_fpsSet < m_minFPS)
      log<text_log>(std::to_string(m_fpsSet) + " fps out of bounds", logPrio::LOG_WARNING);
   int ret = writeSingleControlVal("frame_rate", int(m_fpsSet * 1000000));
   if(ret == PARAM_NOT_FOUND){ 
      return 1;
   }
   return ret < 0 ? log<software_error>({__FILE__,__LINE__, "error setting framerate: " + std::to_string(m_fpsSet)}) : log<text_log,1>({"set framerate: " + std::to_string(m_fpsSet)});
}

inline 
int nsvCtrl::getExpTime()
{
   m_expTime = getAndUpdateSingleControlVal("exposure") / 1000000.0;
   if(m_expTime == PARAM_NOT_FOUND){ 
      return 1;
   }
   return m_expTime < 0 ? log<software_error,-1>({__FILE__, __LINE__, "failed to get exposure time"}) : m_expTime;
}

inline 
int nsvCtrl::setExpTime()
{
   if((m_expTimeSet * 1000000) > m_maxExpTime || (m_expTimeSet * 1000000) < m_minExpTime)
      log<text_log>(std::to_string(m_expTimeSet) + " exp out of bounds", logPrio::LOG_WARNING);
   int ret = writeSingleControlVal("exposure", int(m_expTimeSet * 1000000)); // convert s to us, use indi var
   if(ret == PARAM_NOT_FOUND){ 
      return 1;
   }
   return ret < 0 ? log<software_error>({__FILE__,__LINE__, "error setting exposure to" + std::to_string(m_expTimeSet)}) : log<text_log,1>({"set exposure: " + std::to_string(m_expTimeSet)});
}

inline
int nsvCtrl::getEMGain()
{
   m_emGain = getAndUpdateSingleControlVal("gain");
   if(m_emGain == PARAM_NOT_FOUND){ 
      return 1;
   }
   return m_emGain < 0 ? log<software_error,-1>({__FILE__, __LINE__, "failed to get gain"}) : m_emGain;
}

inline
int nsvCtrl::setEMGain()
{
   if(m_emGainSet > m_maxEMGain || m_emGainSet < m_minEMGain)
      log<text_log>(std::to_string(m_emGainSet) + " gain out of bounds", logPrio::LOG_WARNING);
   int ret = writeSingleControlVal("gain", int(m_emGainSet));
   if(ret == PARAM_NOT_FOUND){ 
      return 1;
   }
   return ret < 0 ? log<software_error>({__FILE__,__LINE__, "error setting exposure to" + std::to_string(m_emGainSet)}) : log<text_log,1>({"set gain: " + std::to_string(m_emGainSet)});
}

// IMX 455 uses blacklevel while IMX571 uses black_level
inline
int nsvCtrl::getBlacklevel()
{
   m_blacklevel = getAndUpdateSingleControlVal("blacklevel");
   if(m_blacklevel < 0)
     m_blacklevel = getAndUpdateSingleControlVal("black_level");
   if(m_blacklevel == PARAM_NOT_FOUND){ 
      return 1;
   }
   return m_blacklevel < 0 ? log<software_error,-1>({__FILE__, __LINE__, "failed to get gain black level"}) : m_blacklevel;
}

inline
int nsvCtrl::setBlacklevel()
{ 
   if(m_blacklevelSet > m_maxBlacklevel || m_blacklevelSet < m_minBlacklevel)
      log<text_log>(std::to_string(m_blacklevelSet) + " black level out of bounds", logPrio::LOG_WARNING);
   int ret = writeSingleControlVal("blacklevel", int(m_blacklevelSet));
   if(ret < 0)
      ret = writeSingleControlVal("black_level", int(m_blacklevelSet));
   if(ret == PARAM_NOT_FOUND){ 
      return 1;
   }
   return ret < 0 ? log<software_error>({__FILE__,__LINE__, "error setting black level to" + std::to_string(m_blacklevelSet)}) : log<text_log,1>({"set black level: " + std::to_string(m_blacklevelSet)});
}

inline
int nsvCtrl::getVCrop()
{
   m_vCrop = getAndUpdateSingleControlVal("vcropoffset");
   if(m_vCrop == PARAM_NOT_FOUND){ 
      return 1;
   }
   return m_vCrop < 0 ? log<software_error,-1>({__FILE__, __LINE__, "failed to get vcropoffset"}) : m_vCrop;
}

inline
int nsvCtrl::setVCrop(int offset)
{ 
   if(offset > m_maxVCrop || offset < m_minVCrop)
      log<text_log>(std::to_string(offset) + " vropoffset out of bounds", logPrio::LOG_WARNING);
   int ret = writeSingleControlVal("vcropoffset", offset);
   if(ret == PARAM_NOT_FOUND){ 
      return 1;
   }
   return ret < 0 ? log<software_error>({__FILE__,__LINE__, "error setting vcrop to" + std::to_string(offset)}) : log<text_log,1>({"set vcrop to: " + std::to_string(ret)});
}

// ROI controls
inline
int nsvCtrl::getXStartPos()
{
   m_xStartPos = getAndUpdateSingleControlVal("roi_hor_start_pos");
   if(m_xStartPos == PARAM_NOT_FOUND){ 
      return 1;
   }
   return m_xStartPos < 0 ? log<software_error,-1>({__FILE__, __LINE__, "failed to get get roi x start pos"}) : m_xStartPos;
}

inline
int nsvCtrl::setXStartPos(int pos)
{ 
   if(pos > m_maxXStartPos || pos < m_minXStartPos)
      log<text_log>(std::to_string(pos) + " ROI start x pos out of bounds", logPrio::LOG_WARNING);
   int ret = writeSingleControlVal("roi_hor_start_pos", pos);
   if(ret == PARAM_NOT_FOUND){ 
      return 1;
   }
   return ret < 0 ? log<software_error>({__FILE__,__LINE__, "error setting roi start x to" + std::to_string(pos)}) : log<text_log,1>({"set roi start x to: " + std::to_string(ret)});
}

inline
int nsvCtrl::getYStartPos()
{
   m_yStartPos = getAndUpdateSingleControlVal("roi_ver_start_pos");
   if(m_yStartPos == PARAM_NOT_FOUND){ 
      return 1;
   }
   return m_xStartPos < 0 ? log<software_error,-1>({__FILE__, __LINE__, "failed to get get roi y start pos"}) : m_yStartPos;
}

inline
int nsvCtrl::setYStartPos(int pos)
{ 
   if(pos > m_maxYStartPos || pos < m_minYStartPos)
      log<text_log>(std::to_string(pos) + " ROI start y pos out of bounds", logPrio::LOG_WARNING);
   int ret = writeSingleControlVal("roi_ver_start_pos", pos);
   if(ret == PARAM_NOT_FOUND){ 
      return 1;
   }
   return ret < 0 ? log<software_error>({__FILE__,__LINE__, "error setting roi start y to" + std::to_string(pos)}) : log<text_log,1>({"set roi start y to: " + std::to_string(pos)});
}

inline
int nsvCtrl::setBitDepth(int bitDepth)
{
   m_bitDepth = bitDepth;
   m_reconfig = true; // have to redo all the way from stopping stream, cameraSelect, etc when image format changes
   return 0;
}

inline
int nsvCtrl::setCropMode()
{
   return 0;
}

inline
int nsvCtrl::set_preferred_stride(int stride)
{
   int res = writeSingleControlVal("preferred_stride", stride);  // 8-bit, 16-bit, 32-bit, 64-bit
   if(res == PARAM_NOT_FOUND){
      return PARAM_NOT_FOUND;
   }
   return res < 0 ? log<software_error,-1>({__FILE__, __LINE__, "failed to set stride to" + std::to_string(stride)}) : log<text_log,1>({"set preferred_stride to: " + std::to_string(res)});
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
   printf("powerOnDefaults\n");

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

   if(m_default_x + (m_default_w / 2) > m_full_w || m_default_x - (m_default_w / 2) < 0)
   {
      m_currentROI.x = m_nextROI.x = m_full_x;
      log<text_log>("Invalid default x with current width and height in mode. Setting to center x", logPrio::LOG_WARNING);
   }
   if(m_default_y + (m_default_h / 2) > m_full_h || m_default_y - (m_default_y / 2) < 0)
   {
      m_currentROI.y = m_nextROI.y = m_full_y;
      log<text_log>("Invalid default y with current width and height in mode. Setting to center y", logPrio::LOG_WARNING);
   }
   if(m_default_w > m_full_w)
   {
      m_currentROI.w = m_nextROI.w = m_full_w;
      log<text_log>("Invalid default w with current mode. Setting to max width", logPrio::LOG_WARNING);
   }
   if(m_default_h > m_full_h)
   {
      m_currentROI.h = m_nextROI.h = m_full_h;
      log<text_log>("Invalid default h with current mode. Setting to max height", logPrio::LOG_WARNING);
   }
   
   m_reconfig = true; 
   
  // since 'power off' nukes the camera buffers & stream, set reconfig here?

   return 0;
}

inline
int nsvCtrl::setTempControl()
{
   return 0;   // todo
}

inline 
int nsvCtrl::checkNextROI()
{
   return 0;
}

inline 
int nsvCtrl::setNextROI()
{ 
   if(m_poweredOn)
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
   m_dataType = _DATATYPE_UINT16;
   //m_typeSize = imageStructDataType<IMAGESTRUCT_UINT16>::size;

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
   resizeROIbufs(); // ensure new ROI is always up to date before starting stream

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
      
      m_current_frame = dequeueBuffer(m_oldest_frame);  // cam forces you to read oldest frame in the buffer first
      if(m_current_frame == -1){ //fd is gone once powered off, so dequeue will fail
         /*
         if(!m_poweredOn || m_powerState != 1){ // changed power status of camera after dequeing but not before receiving result
            return log<text_log>("Tried to dequeue camera frame while off", logPrio::LOG_WARNING);
         }
         */
         // ABOVE CODE WILL SEGFAULT. IN FACT... ANYTHING OTHER THAN THESE NEXT TWO LINES WILL SEGFAULT. 
         // I suspect this is because of the frameGrabber "derived().powerState()" which I am not setting because I have my own power impelmentation...
         state(stateCodes::ERROR);
         return log<software_error,-1>({__FILE__, __LINE__, "nsvCam failed to dequeue frame"}); 
      }
      queueBuffer(m_current_frame); // queue into index just read from

      writeROISubframe(); 

      m_oldest_frame++;
      if(m_oldest_frame > bufferCount - 1){
         m_oldest_frame = 0; 
      }

      // get timing information stored for the camera frame that was just dequeued
      m_currImageTimestamp.tv_sec = cameraTimestamp.seconds;
      m_currImageTimestamp.tv_nsec = cameraTimestamp.nanoseconds;

      // add reporting out timestamp from camera for frame start.  Relate camera frame to PC frame?
      // generate statistics for mean frame time.
      // allocate an array of these - timespec m_currImageTimestamp {0,0};

   }

   return 0;
}

inline
int nsvCtrl::loadImageIntoStream(void * dest)
{
   //if( frameGrabber<nsvCtrl>::loadImageIntoStreamCopy(dest, buffers[m_current_frame], m_width, m_height, m_typeSize) == nullptr) return -1;
   if( frameGrabber<nsvCtrl>::loadImageIntoStreamCopy(dest, ROIbuffers[m_current_frame], m_currentROI.w, m_currentROI.h, m_typeSize) == nullptr) return log<software_error,-1>({__FILE__, __LINE__, "grabbing subframe failed"});
   return 0;
}

inline
int nsvCtrl::writeROISubframe()
{
   
   uint16_t* imagePtr = static_cast<uint16_t*>(buffers[m_current_frame]); // need to static cast for appriate camera bitdepth
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
         roiPtr[ROIIndex] = static_cast<uint16_t*>(imagePtr)[((startY + i) * m_full_w + (startX + j))]; //m_full_w or should it be bytes_per_line?
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

   printf("roi w: %d h: %d, bufsize: %d\n", m_currentROI.w, m_currentROI.h, bufSize);

   for(int i = 0; i < (int)m_circBuffLength; i++){

      void* buffer = malloc(bufSize * sizeof(uint16_t));  // assuming 16-bit. verify w/ bufferSize in queryBuffer
      if (buffer == nullptr) {
         state(stateCodes::ERROR);
         return log<software_error,-1>({__FILE__, __LINE__, "resizeROIbufs memory allocation failed"});
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
   // if deprecating modes/ dynamically creating modes, should trigger this some other way
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
      //m_init = true;
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
   int rv = uses_vCrop ? setVCrop(m_vCrop) : -999;
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting vcropoffset!"});
      return -1;
   }
   if(uses_vCrop)
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


INDI_NEWCALLBACK_DEFN(nsvCtrl, m_indiP_power)(const pcf::IndiProperty &ipRecv)
{
   INDI_VALIDATE_CALLBACK_PROPS(m_indiP_power, ipRecv);
   
   if(!ipRecv.find("toggle")) return 0;
   
   if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On)
   {
      updateSwitchIfChanged(m_indiP_power, "toggle", pcf::IndiElement::On, INDI_IDLE);
      
      turn_on_power();
      sleep(6);
      m_powerCycles += 1;
      m_power = true;
      m_poweredOn = true;
      m_powerState = 1;
      m_powerOnTime = std::chrono::high_resolution_clock::now(); 
      //updateIfChanged(m_indiP_power_data, "powered_on", m_powerOnTime);
      //updateIfChanged(m_indiP_power_data, "power_cycles", m_powerCycles);

      std::time_t timePointOnT = std::chrono::system_clock::to_time_t(m_powerOnTime);
      char bufferOn[100];
      std::strftime(bufferOn, sizeof(bufferOn), "%I:%M %p %m/%d/%y", std::localtime(&timePointOnT));
      std::string m_powerOnTS(bufferOn);

      //m_powerOnTS = std::to_string(m_powerOnTime);
      //updateIfChanged(m_indiP_power_data, std::vector<std::string>({"last_power_on", "last_power_off", "on_duration", "power_cycles"}), 
      //                              std::vector<std::string>({m_powerOnTS, m_powerOffTS, m_poweredOnDuration, std::to_string(m_powerCycles)}));

      updateIfChanged(m_indiP_power_status, "power_cycles", m_powerCycles);
      updateIfChanged(m_indiP_power_status, "last_power_on", m_powerOnTS);

      state(stateCodes::POWERON);
      
      log<text_log>("powered on from INDI");
   }
   else
   {
      updateSwitchIfChanged(m_indiP_power, "toggle", pcf::IndiElement::Off, INDI_IDLE);
      
      m_power = false;
      m_poweredOn = false;
      m_powerState = 0;
      m_powerOffTime = std::chrono::high_resolution_clock::now();
      onPowerOff();
      //updateIfChanged(m_indiP_power_data, "powered_off", m_powerOffTime);

      std::time_t timePointOffT = std::chrono::system_clock::to_time_t(m_powerOffTime);
      char bufferOff[100];
      std::strftime(bufferOff, sizeof(bufferOff), "%I:%M %p %m/%d/%y", std::localtime(&timePointOffT));
      std::string m_powerOffTS(bufferOff);

      //m_powerOffTS = std::to_string(m_powerOffTime);
     // updateIfChanged(m_indiP_power_status, std::vector<std::string>({"last_power_on", "last_power_off", "on_duration", "power_cycles"}), 
     //                               std::vector<std::string>({m_powerOnTS, m_powerOffTS, m_poweredOnDuration, std::to_string(m_powerCycles)}));

      updateIfChanged(m_indiP_power_status, "last_power_off", m_powerOffTS);

      state(stateCodes::POWEROFF);
      printf("powered off, state changed to POWEROFF\n");
      
      log<text_log>("powered off from INDI");
   }
   
   return 0;
}

std::string nsvCtrl::cmdRes(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

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
