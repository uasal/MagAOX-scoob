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
#include <fstream>
#include <iomanip>
#include <sstream>
#include <map>
#include <filesystem>
#include <set>
#include <thread>
#include <mutex>

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

   // Power monitoring variables
   float m_gmslVoltage {0.0};
   float m_gmslCurrent {0.0};
   std::string m_powerDevicePath;         // Direct device path (e.g., "/sys/bus/i2c/drivers/ina3221/8-0040/hwmon/hwmon6")
   int m_powerUpdateCounter = 0;          // Counter for rate limiting power updates
   int m_powerUpdateInterval = 10;        // Update power every N frames (10Hz at 100fps = every 10 frames)
   
   // High-frequency power monitoring thread
   std::thread m_powerThread;             // Separate thread for power monitoring
   bool m_powerThreadRunning = false;     // Control flag for power thread
   std::chrono::milliseconds m_powerUpdateRate{10}; // Update every 10ms (100Hz)
   std::mutex m_powerMutex;               // Separate mutex for power data
   
   // Comprehensive power monitoring structure
   struct PowerRail {
      std::string name;           // e.g., "12V_A_GMSL1"
      std::string devicePath;     // e.g., "/sys/bus/i2c/drivers/ina3221/8-0040/hwmon/hwmon6"
      int channel;                // e.g., 1, 2, 3, 7
      std::string voltageFile;    // e.g., "in1_input"
      std::string currentFile;    // e.g., "curr1_input"
      float voltage = 0.0;
      float current = 0.0;
      bool valid = false;
   };
   
   std::vector<PowerRail> m_powerRails;
   
   // Power logging variables
   bool m_powerLoggingEnabled = false;
   std::ofstream m_powerLogFile;
   std::mutex m_powerLogMutex;
   std::string m_powerLogPath;

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

   struct TimeSpec {
      long tv_sec;
      long tv_nsec;
  };

   // timing info for extra camera statistics
   TimeSpec prev_timestamp = {0,0};
   double running_mean = 0.0;
   int frame_count = 0;
   int buffer_discard = 0;
   bool has_prev = false;

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

   // Power monitoring thread function
   void powerMonitoringThread();
   
   // Power monitoring functions
   void initializePowerRails();
   void updateAllPowerRails();
   
   // Power logging functions
   int startPowerLogging();
   int stopPowerLogging();
   void logPowerData();

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

   void reset_cam_statistics();

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
   pcf::IndiProperty m_indiP_frame_timestamp_s;
   pcf::IndiProperty m_indiP_frame_timestamp_ns;
   pcf::IndiProperty m_indiP_mean_frame_time;
   pcf::IndiProperty m_indiP_power;
   pcf::IndiProperty m_indiP_power_status;
   pcf::IndiProperty m_indiP_gmsl_voltage;
   pcf::IndiProperty m_indiP_gmsl_current;
   pcf::IndiProperty m_indiP_power_logging;

public:

   INDI_NEWCALLBACK_DECL(nsvCtrl, m_indiP_vCrop);
   INDI_NEWCALLBACK_DECL(nsvCtrl, m_indiP_bitDepth);
   INDI_NEWCALLBACK_DECL(nsvCtrl, m_indiP_power_logging);
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

   // Initialize power monitoring
   m_gmslVoltage = 0.0;
   m_gmslCurrent = 0.0;

   return;
}

inline
nsvCtrl::~nsvCtrl() noexcept
{
   // Stop power monitoring thread
   if (m_powerThreadRunning) {
      m_powerThreadRunning = false;
      if (m_powerThread.joinable()) {
         m_powerThread.join();
      }
   }
   
   // Power monitoring uses direct file reading, no persistent streams to close
   return;
}

inline
void nsvCtrl::setupConfig()
{
 
   config.add("camera.camID", "", "camera.camID", argType::Required, "camera","camID", false, "str", "v4l2 Card Type identifyer for camera.");
   config.add("camera.vcropoffset", "", "camera.vcropoffset", argType::Required, "camera", "vcropoffset", false, "int", "vertical crop offset for camera");
   config.add("camera.bitDepth", "", "camera.bitDepth", argType::Required, "camera", "bitDepth", false, "int", "pixel bit depth");
   config.add("camera.power", "", "camera.power", argType::Optional, "camera", "power", false, "bool", "camera power"); // TODO make toggle
   config.add("camera.power_device_path", "", "camera.power_device_path", argType::Optional, "camera", "power_device_path", false, "str", "Direct device path for power monitoring (e.g., /sys/bus/i2c/drivers/ina3221/8-0040/hwmon/hwmon6)");

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
   config(m_powerDevicePath, "camera.power_device_path");
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

   createROIndiNumber( m_indiP_frame_timestamp_s, "frame_timestamp_s", "Frame Timestamp (s)");
   indi::addNumberElement<uint>( m_indiP_frame_timestamp_s, "value", 0, std::numeric_limits<uint>::max(), 0,  "%d", "readout time");
   registerIndiPropertyReadOnly( m_indiP_frame_timestamp_s );

   createROIndiNumber( m_indiP_frame_timestamp_ns, "frame_timestamp_ns", "Frame Timestamp (ns)");
   indi::addNumberElement<uint>( m_indiP_frame_timestamp_ns, "value", 0, std::numeric_limits<uint>::max(), 0,  "%d", "readout time");
   registerIndiPropertyReadOnly( m_indiP_frame_timestamp_ns );

   createROIndiNumber( m_indiP_mean_frame_time, "m_indiP_mean_frame_time", "Mean Frame Time (s)");
   indi::addNumberElement<float>( m_indiP_mean_frame_time, "value", 0.0, std::numeric_limits<float>::max(), 0.0,  "%f", "readout time");
   registerIndiPropertyReadOnly( m_indiP_mean_frame_time );

   createROIndiNumber( m_indiP_gmsl_voltage, "gmsl_voltage", "GMSL Voltage (V)");
   indi::addNumberElement<float>( m_indiP_gmsl_voltage, "value", 0.0, 15.0, 0.0,  "%.3f", "GMSL interface voltage");
   registerIndiPropertyReadOnly( m_indiP_gmsl_voltage );

   createROIndiNumber( m_indiP_gmsl_current, "gmsl_current", "GMSL Current (A)");
   indi::addNumberElement<float>( m_indiP_gmsl_current, "value", 0.0, 10.0, 0.0,  "%.3f", "GMSL interface current");
   registerIndiPropertyReadOnly( m_indiP_gmsl_current );

   createStandardIndiToggleSw( m_indiP_power_logging, "power_logging", "Power Logging", "Enable/disable power data logging");
   m_indiP_power_logging["toggle"].set(0);
   registerIndiPropertyNew( m_indiP_power_logging, INDI_NEWCALLBACK(m_indiP_power_logging));

   /*
   createStandardIndiNumber<int>(m_indiP_frame_timestamp_s, "frame_timestamp_s", 0, 2147483647, 1, "%d");
   m_indiP_frame_timestamp_s["current"] = prev_timestamp.tv_sec;
   registerIndiPropertyReadOnly(m_indiP_frame_timestamp_s, INDI_NEWCALLBACK(m_indiP_frame_timestamp_s));
   */

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

   // Initialize power monitoring
   if (!m_powerDevicePath.empty()) {
      // Test if power monitoring files exist
      std::string currentFile = m_powerDevicePath + "/curr1_input";
      std::string voltageFile = m_powerDevicePath + "/in1_input";
      
      std::ifstream testCurrent(currentFile);
      std::ifstream testVoltage(voltageFile);
      
      if (testCurrent.is_open() && testVoltage.is_open()) {
         log<text_log>("Power monitoring initialized: " + m_powerDevicePath, logPrio::LOG_INFO);
         
         // Initialize all power rails
         initializePowerRails();
         
         // Start power monitoring thread
         m_powerThreadRunning = true;
         m_powerThread = std::thread(&nsvCtrl::powerMonitoringThread, this);
         log<text_log>("Power monitoring thread started at " + std::to_string(1000/m_powerUpdateRate.count()) + "Hz", logPrio::LOG_INFO);
      } else {
         log<text_log>("Power monitoring files not available: " + currentFile + ", " + voltageFile, logPrio::LOG_WARNING);
      }
   } else {
      log<text_log>("No power_device_path configured, power monitoring disabled", logPrio::LOG_INFO);
   }

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
      
      // Update power data inside INDI mutex to ensure it gets processed
      {
         std::lock_guard<std::mutex> powerLock(m_powerMutex);
         updateIfChanged(m_indiP_gmsl_voltage, "value", m_gmslVoltage, INDI_OK);
         updateIfChanged(m_indiP_gmsl_current, "value", m_gmslCurrent, INDI_OK);
      }
   
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

      // Power data already updated above

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

   reset_cam_statistics();

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
   m_ccdTemp = res / 1000.0;
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

void nsvCtrl::powerMonitoringThread()
{
   log<text_log>("Power monitoring thread started", logPrio::LOG_INFO);
   
   int loop_count = 0;
   while (m_powerThreadRunning && !shutdown()) {
      // Update all power rails
      updateAllPowerRails();
      
      // Update legacy GMSL variables for backward compatibility
      {
         std::lock_guard<std::mutex> lock(m_powerMutex);
         // Find any GMSL rail for legacy compatibility (prefer B over A)
         for (const auto& rail : m_powerRails) {
            if ((rail.name.find("GMSL") != std::string::npos) && rail.valid) {
               m_gmslVoltage = rail.voltage;
               m_gmslCurrent = rail.current;
               break; // Use first GMSL rail found
            }
         }
      }
      
      // Log power data if enabled
      logPowerData();
      
      // Debug logging every 100 loops (1 second at 100Hz)
      loop_count++;
      if (loop_count % 100 == 0) {
         log<text_log>("Power thread running, loop count: " + std::to_string(loop_count) + 
                      ", voltage: " + std::to_string(m_gmslVoltage) + 
                      "V, current: " + std::to_string(m_gmslCurrent) + "A", logPrio::LOG_INFO);
      }
      
      // Sleep for the specified update rate
      std::this_thread::sleep_for(m_powerUpdateRate);
   }
   
   log<text_log>("Power monitoring thread stopped", logPrio::LOG_INFO);
}

inline
void nsvCtrl::initializePowerRails()
{
   // Clear existing rails
   m_powerRails.clear();
   
   log<text_log>("Starting auto-discovery of INA3221 power sensors...", logPrio::LOG_INFO);
   
   // Auto-discover all INA3221 devices (follow symlinks robustly; avoid external find)
   std::vector<std::string> devicePaths;
   const std::string basePath = "/sys/bus/i2c/drivers/ina3221";
   try {
      namespace fs = std::filesystem;
      if (fs::exists(basePath) && fs::is_directory(basePath)) {
         for (const auto &dirEntry : fs::directory_iterator(basePath)) {
            const std::string entryName = dirEntry.path().filename().string();
            // Match entries like "0-0040", "1-0043", etc. and ensure it's a symlink
            if (entryName.find('-') != std::string::npos &&
                entryName.find('-') == entryName.rfind('-') &&
                fs::is_symlink(dirEntry.symlink_status())) {
               // Resolve the real device path
               fs::path realDevicePath;
               try {
                  realDevicePath = fs::read_symlink(dirEntry.path());
                  // If the symlink is relative, make it absolute based on parent
                  if (realDevicePath.is_relative()) {
                     realDevicePath = dirEntry.path().parent_path() / realDevicePath;
                  }
               } catch (...) {
                  // If we can't resolve symlink, skip
                  continue;
               }

               const fs::path hwmonRoot = realDevicePath / "hwmon";
               if (fs::exists(hwmonRoot) && fs::is_directory(hwmonRoot)) {
                  for (const auto &hwmonEntry : fs::directory_iterator(hwmonRoot)) {
                     const std::string hwmonName = hwmonEntry.path().filename().string();
                     if (fs::is_directory(hwmonEntry.status()) &&
                         hwmonName.rfind("hwmon", 0) == 0) {
                        const std::string hwmonDir = hwmonEntry.path().string();
                        devicePaths.push_back(hwmonDir);
                        log<text_log>("Found INA3221 device: " + hwmonDir, logPrio::LOG_INFO);
                     }
                  }
               }
            }
         }
      }
   } catch (const std::exception &e) {
      log<text_log>("Exception during auto-discovery: " + std::string(e.what()), logPrio::LOG_WARNING);
   }
   
   // If auto-discovery failed or found no devices, fall back to configured path
   if (devicePaths.empty()) {
      if (!m_powerDevicePath.empty()) {
         log<text_log>("Auto-discovery failed, using configured power device path: " + m_powerDevicePath, logPrio::LOG_WARNING);
         devicePaths.push_back(m_powerDevicePath);
      } else {
         log<text_log>("No INA3221 devices found via auto-discovery and no power_device_path configured", logPrio::LOG_WARNING);
         return;
      }
   }
   
   // Scan each device for power rails
   for (const auto& devicePath : devicePaths) {
      log<text_log>("Scanning device: " + devicePath, logPrio::LOG_INFO);
      try {
         namespace fs = std::filesystem;
         std::vector<std::string> labelFiles;
         for (const auto &entry : fs::directory_iterator(devicePath)) {
            if (!entry.is_regular_file()) continue;
            const std::string fname = entry.path().filename().string();
            if (fname.rfind("in", 0) == 0 && fname.size() > 7 &&
                fname.find("_label") == fname.size() - 6) {
               labelFiles.push_back(entry.path().string());
            }
         }

         if (labelFiles.empty()) {
            log<text_log>("No label files found in " + devicePath, logPrio::LOG_WARNING);
            continue;
         }

         for (const auto &labelFile : labelFiles) {
            const std::string filename = std::filesystem::path(labelFile).filename().string();
            const std::size_t posStart = 2; // after 'in'
            const std::size_t posEnd = filename.find("_label");
            if (posEnd == std::string::npos || posEnd <= posStart) continue;
            int channel = -1;
            try {
               channel = std::stoi(filename.substr(posStart, posEnd - posStart));
            } catch (...) {
               continue;
            }

            std::ifstream labelStream(labelFile);
            if (!labelStream.is_open()) {
               log<text_log>("Cannot read label file: " + labelFile + " (open failed)", logPrio::LOG_WARNING);
               continue;
            }

            std::string railName;
            std::getline(labelStream, railName);
            if (railName.empty()) continue;

            PowerRail rail;
            rail.name = railName;
            rail.devicePath = devicePath;
            rail.channel = channel;
            rail.voltageFile = devicePath + "/in" + std::to_string(channel) + "_input";
            rail.currentFile = devicePath + "/curr" + std::to_string(channel) + "_input";
            rail.valid = false;

            std::ifstream vFile(rail.voltageFile);
            std::ifstream cFile(rail.currentFile);
            if (vFile.good() && cFile.good()) {
               std::string testValue;
               std::getline(vFile, testValue);
               if (!testValue.empty()) {
                  rail.valid = true;
                  log<text_log>("Discovered power rail: " + railName + " (ch" + std::to_string(channel) + ") at " + devicePath, logPrio::LOG_INFO);
               } else {
                  log<text_log>("Power rail " + railName + " files exist but are not readable (empty)", logPrio::LOG_WARNING);
               }
            } else {
               log<text_log>("Power rail " + railName + " files not accessible", logPrio::LOG_WARNING);
            }
            m_powerRails.push_back(rail);
         }
      } catch (const std::exception &e) {
         log<text_log>("Exception scanning device '" + devicePath + "': " + std::string(e.what()), logPrio::LOG_WARNING);
      }
   }
   
   log<text_log>("Auto-discovery complete: Found " + std::to_string(m_powerRails.size()) + " power rails", logPrio::LOG_INFO);
   
   // Log summary of discovered rails
   for (const auto& rail : m_powerRails) {
      if (rail.valid) {
         log<text_log>("  ✓ " + rail.name + " (ch" + std::to_string(rail.channel) + ") - " + rail.devicePath, logPrio::LOG_INFO);
      } else {
         log<text_log>("  ✗ " + rail.name + " (ch" + std::to_string(rail.channel) + ") - INVALID", logPrio::LOG_WARNING);
      }
   }
}

inline
void nsvCtrl::updateAllPowerRails()
{
   std::lock_guard<std::mutex> lock(m_powerMutex);
   
   for (auto& rail : m_powerRails) {
      if (!rail.valid) continue;
      
      // Read voltage
      std::ifstream vFile(rail.voltageFile);
      if (vFile.is_open()) {
         std::string voltageStr;
         std::getline(vFile, voltageStr);
         if (!voltageStr.empty()) {
            try {
               rail.voltage = std::stof(voltageStr) / 1000.0f; // Convert mV to V
            } catch (const std::exception& e) {
               // Silent error handling
            }
         }
      } else {
         // Log permission error only once per rail to avoid spam
         static std::set<std::string> logged_voltage_errors;
         if (logged_voltage_errors.find(rail.name) == logged_voltage_errors.end()) {
            log<text_log>("Failed to open voltage file for rail '" + rail.name + "': " + rail.voltageFile + " (permission denied)", logPrio::LOG_WARNING);
            logged_voltage_errors.insert(rail.name);
         }
      }
      
      // Read current
      std::ifstream cFile(rail.currentFile);
      if (cFile.is_open()) {
         std::string currentStr;
         std::getline(cFile, currentStr);
         if (!currentStr.empty()) {
            try {
               rail.current = std::stof(currentStr) / 1000.0f; // Convert mA to A
            } catch (const std::exception& e) {
               // Silent error handling
            }
         }
      } else {
         // Log permission error only once per rail to avoid spam
         static std::set<std::string> logged_current_errors;
         if (logged_current_errors.find(rail.name) == logged_current_errors.end()) {
            log<text_log>("Failed to open current file for rail '" + rail.name + "': " + rail.currentFile + " (permission denied)", logPrio::LOG_WARNING);
            logged_current_errors.insert(rail.name);
         }
      }
   }
}

inline
int nsvCtrl::startPowerLogging()
{
   std::lock_guard<std::mutex> lock(m_powerLogMutex);
   
   if (m_powerLoggingEnabled) {
      return 0; // Already logging
   }
   
   // Generate log file path with timestamp in MagAOX telemetry directory
   auto now = std::chrono::system_clock::now();
   auto time_t = std::chrono::system_clock::to_time_t(now);
   auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
   
   // Ensure telemetry directory exists
   std::string telemDir = "/opt/MagAOX/telem";
   if (access(telemDir.c_str(), F_OK) != 0) {
      // Create directory if it doesn't exist
      std::string mkdirCmd = "mkdir -p " + telemDir;
      if (system(mkdirCmd.c_str()) != 0) {
         log<software_error>({__FILE__, __LINE__, "Failed to create telemetry directory: " + telemDir});
         return -1;
      }
   }
   
   std::stringstream ss;
   ss << telemDir << "/nsvCtrl_power_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
   ss << "_" << std::setfill('0') << std::setw(3) << ms.count() << ".csv";
   m_powerLogPath = ss.str();
   
   m_powerLogFile.open(m_powerLogPath, std::ios::out);
   if (!m_powerLogFile.is_open()) {
      log<software_error>({__FILE__, __LINE__, "Failed to open power log file: " + m_powerLogPath});
      return -1;
   }
   
   // Write CSV header
   m_powerLogFile << "system_time_ns,camera_timestamp_s,camera_timestamp_ns,voltage_v,current_a\n";
   m_powerLogFile.flush();
   
   m_powerLoggingEnabled = true;
   log<text_log>("Power logging started, file: " + m_powerLogPath, logPrio::LOG_INFO);
   
   return 0;
}

inline
int nsvCtrl::stopPowerLogging()
{
   std::lock_guard<std::mutex> lock(m_powerLogMutex);
   
   if (!m_powerLoggingEnabled) {
      return 0; // Not logging
   }
   
   m_powerLoggingEnabled = false;
   
   if (m_powerLogFile.is_open()) {
      m_powerLogFile.close();
      log<text_log>("Power logging stopped, file: " + m_powerLogPath, logPrio::LOG_INFO);
   }
   
   return 0;
}

inline
void nsvCtrl::logPowerData()
{
   if (!m_powerLoggingEnabled) return;
   
   std::lock_guard<std::mutex> lock(m_powerLogMutex);
   
   if (!m_powerLogFile.is_open()) return;
   
   // Get current system time in nanoseconds
   auto now = std::chrono::high_resolution_clock::now();
   auto duration = now.time_since_epoch();
   auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
   
   // Get camera timestamp (if available)
   long camera_timestamp_s = 0;
   long camera_timestamp_ns = 0;
   
   // Try to get camera timestamp from current image
   if (m_currImageTimestamp.tv_sec != 0 || m_currImageTimestamp.tv_nsec != 0) {
      camera_timestamp_s = m_currImageTimestamp.tv_sec;
      camera_timestamp_ns = m_currImageTimestamp.tv_nsec;
   }
   
   // Write header if this is the first write
   static bool headerWritten = false;
   if (!headerWritten) {
      m_powerLogFile << "system_time_ns,camera_timestamp_s,camera_timestamp_ns";
      for (const auto& rail : m_powerRails) {
         if (rail.valid) {
            m_powerLogFile << "," << rail.name << "_ch" << rail.channel << "_voltage_v," 
                          << rail.name << "_ch" << rail.channel << "_current_a";
         }
      }
      m_powerLogFile << "\n";
      headerWritten = true;
   }
   
   // Write data for all power rails
   m_powerLogFile << nanoseconds << "," 
                  << camera_timestamp_s << "," 
                  << camera_timestamp_ns;
   
   for (const auto& rail : m_powerRails) {
      if (rail.valid) {
         m_powerLogFile << "," << std::fixed << std::setprecision(6) << rail.voltage 
                        << "," << std::fixed << std::setprecision(6) << rail.current;
      } else {
         m_powerLogFile << ",0.000000,0.000000"; // Invalid rail
      }
   }
   
   m_powerLogFile << "\n";
   m_powerLogFile.flush();
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
   // need to dump camera buffers here when changing framerate otherwise screws up statistics...

   reset_cam_statistics();
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
   reset_cam_statistics();
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
void nsvCtrl::reset_cam_statistics()
{
   prev_timestamp = {0,0};
   running_mean = 0.0;
   frame_count = 0;
   buffer_discard = 0;
   has_prev = false;
   updateIfChanged( m_indiP_frame_timestamp_s, "value", prev_timestamp.tv_sec, INDI_OK); 
   updateIfChanged( m_indiP_frame_timestamp_ns, "value", prev_timestamp.tv_nsec, INDI_OK);
   updateIfChanged( m_indiP_mean_frame_time, "value", running_mean, INDI_OK); 
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

      // Power monitoring now handled by separate thread at 10Hz

      if (has_prev && buffer_discard == bufferCount) { // make sure we cycle through the buffer at least once before computing statistics.
         int64_t sec_diff = static_cast<int64_t>(m_currImageTimestamp.tv_sec) - static_cast<int64_t>(prev_timestamp.tv_sec);
         int64_t nsec_diff = static_cast<int64_t>(m_currImageTimestamp.tv_nsec) - static_cast<int64_t>(prev_timestamp.tv_nsec);
         int64_t total_nsec = sec_diff * 1'000'000'000 + nsec_diff;
         double delta_t = static_cast<double>(total_nsec) / 1e9;

         frame_count++;
         running_mean = ((frame_count - 1) * running_mean + delta_t) / (double)frame_count;

         updateIfChanged( m_indiP_frame_timestamp_s, "value", prev_timestamp.tv_sec, INDI_OK); 
         updateIfChanged( m_indiP_frame_timestamp_ns, "value", prev_timestamp.tv_nsec, INDI_OK);
         updateIfChanged( m_indiP_mean_frame_time, "value", running_mean, INDI_OK); 
     } else {
         has_prev = true;
     }
     prev_timestamp.tv_sec = m_currImageTimestamp.tv_sec;
     prev_timestamp.tv_nsec = m_currImageTimestamp.tv_nsec;
     if(buffer_discard < bufferCount)
        buffer_discard++;
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

INDI_NEWCALLBACK_DEFN(nsvCtrl, m_indiP_power_logging)(const pcf::IndiProperty &ipRecv)
{
   INDI_VALIDATE_CALLBACK_PROPS(m_indiP_power_logging, ipRecv);
   
   if(!ipRecv.find("toggle")) return 0;
   
   if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On)
   {
      updateSwitchIfChanged(m_indiP_power_logging, "toggle", pcf::IndiElement::On, INDI_OK);
      
      if(startPowerLogging() < 0)
      {
         log<software_error>({__FILE__, __LINE__, "Failed to start power logging"});
         updateSwitchIfChanged(m_indiP_power_logging, "toggle", pcf::IndiElement::Off, INDI_ALERT);
         return -1;
      }
      
      log<text_log>("Power logging started", logPrio::LOG_INFO);
   }
   else
   {
      updateSwitchIfChanged(m_indiP_power_logging, "toggle", pcf::IndiElement::Off, INDI_OK);
      
      if(stopPowerLogging() < 0)
      {
         log<software_error>({__FILE__, __LINE__, "Failed to stop power logging"});
         return -1;
      }
      
      log<text_log>("Power logging stopped", logPrio::LOG_INFO);
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
