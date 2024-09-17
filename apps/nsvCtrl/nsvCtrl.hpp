/** \file nsvCtrl.hpp
  * \brief The MagAO-X ALPAO DM controller header file
  *
  * \ingroup alpaoCtrl_files
  */

#ifndef alpaoCtrl_hpp
#define alpaoCtrl_hpp

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"


#include <linux/videodev2.h>


namespace MagAOX
{
namespace app
{
/*
 Data
    Images
    DarkFrame
    LightFrame?
*/ 

class nsvCtrl : public MagAOXApp<>, public dev::stdCamera<nsvCtrl>, public dev::edtCamera<nsvCtrl>,
                                    public dev::frameGrabber<nsvCtrls>, public dev::telemeter<nsvCtrl>
{
    friend class dev::stdCamera<nsvCtrl>;
    friend class dev::edtCamera<nsvCtrl>;
    friend class dev::frameGrabber<nsvCtrl>;
    friend class dev::telemeter<nsvCtrl>;

public:
     /** \name app::dev Configurations
     *@{
     */
   static constexpr bool c_stdCamera_tempControl = true; ///< app::dev config to tell stdCamera to expose temperature controls
   
   static constexpr bool c_stdCamera_temp = true; ///< app::dev config to tell stdCamera to expose temperature
   
   static constexpr bool c_stdCamera_readoutSpeed = true; ///< app::dev config to tell stdCamera to expose readout speed controls
   
   static constexpr bool c_stdCamera_vShiftSpeed = true; ///< app:dev config to tell stdCamera to expose vertical shift speed control
   
   static constexpr bool c_stdCamera_emGain = true; ///< app::dev config to tell stdCamera to expose EM gain controls 

   static constexpr bool c_stdCamera_exptimeCtrl = true; ///< app::dev config to tell stdCamera to expose exposure time controls
   
   static constexpr bool c_stdCamera_fpsCtrl = false; ///< app::dev config to tell stdCamera to not expose FPS controls

   static constexpr bool c_stdCamera_fps = true; ///< app::dev config to tell stdCamera not to expose FPS status
   
   static constexpr bool c_stdCamera_synchro = false; ///< app::dev config to tell stdCamera to not expose synchro mode controls

   static constexpr bool c_stdCamera_usesModes = false; ///< app:dev config to tell stdCamera not to expose mode controls
   
   static constexpr bool c_stdCamera_usesROI = true; ///< app:dev config to tell stdCamera to expose ROI controls

   static constexpr bool c_stdCamera_cropMode = true; ///< app:dev config to tell stdCamera to expose Crop Mode controls
   
   static constexpr bool c_stdCamera_hasShutter = true; ///< app:dev config to tell stdCamera to expose shutter controls

   static constexpr bool c_stdCamera_usesStateString = false; ///< app::dev confg to tell stdCamera to expose the state string property

   static constexpr bool c_edtCamera_relativeConfigPath = false; ///< app::dev config to tell edtCamera to use absolute path to camera config file
   
   static constexpr bool c_frameGrabber_flippable = false; ///< app:dev config to tell framegrabber this camera can not be flipped
   
   ///@}

protected:

   /** \name configurable parameters
     *@{
     */

   ///@}

   std::string m_configFile; ///< The path, relative to configDir, where to write and read the temporary config file.
   
   bool m_libInit {false}; ///< Whether or not the Andor SDK library is initialized.

   bool m_poweredOn {false};


public:

   ///Default constructortor
   nsvCtrl();

   ///Destructor
   ~nsvCtrl() noexcept;

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

   /// Do any needed shutdown tasks.  Currently nothing in this app.
   virtual int appShutdown();

   int cameraSelect();

   float getTemp();
   
   float getFPS();
   
   int setFPS(float fps);
   
   bool getPowerState();
   
   int setPowerState(bool on);
   
   float getGain();
   
   int setGain(float gain);
   
   int setExposure(float exposure);
   
   float getExposure();
   
   int getBitDepth(); //12, 14, 16
   
   int setBitDepth(int bitDepth);

   int setROI(int offsetX, int ofsetY, int widthX, int heightY);
   
   int getXOffset()
   
   int getYOffset();
   
   int getXWidth(); //max 6244
   
   int getYWidth(); //max 4188

   int setBinning(int x, int y); //1x1 nominal, 2x2, 4x4, etc (couple with width) 

   float getHeartBeat();

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
   
   /// Turn temperature control on or off.
   /** Sets temperature control on or off based on the current value of m_tempControlStatus
     * \returns 0 on success
     * \returns -1 on error
     */ 
   int setTempControl();
   
   /// Set the CCD temperature setpoint [stdCamera interface].
   /** Sets the temperature to m_ccdTempSetpt.
     * \returns 0 on success
     * \returns -1 on error
     */
   int setTempSetPt();
   
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
   
   m_startupTemp = -45;
   
   m_defaultReadoutSpeed  = "emccd_17MHz";
   m_readoutSpeedNames = {"ccd_00_08MHz", "ccd_01MHz", "ccd_03MHz", "emccd_01MHz", "emccd_05MHz", "emccd_10MHz", "emccd_17MHz"};
   m_readoutSpeedNameLabels = {"CCD 0.08 MHz", "CCD 1 MHz", "CCD 3 MHz", "EMCCD 1 MHz", "EMCCD 5 MHz", "EMCCD 10 MHz", "EMCCD 17 MHz"};
   
   m_defaultVShiftSpeed = "3_3us";
   m_vShiftSpeedNames = {"0_3us", "0_5us", "0_9us", "1_7us", "3_3us"};
   m_vShiftSpeedNameLabels = {"0.3 us", "0.5 us", "0.9 us", "1.7 us", "3.3 us"};
   
   m_maxEMGain = 300;

      
   m_default_x = 255.5; 
   m_default_y = 255.5; 
   m_default_w = 512;  
   m_default_h = 512;  
      
   m_nextROI.x = m_default_x;
   m_nextROI.y = m_default_y;
   m_nextROI.w = m_default_w;
   m_nextROI.h = m_default_h;
   m_nextROI.bin_x = 1;
   m_nextROI.bin_y = 1;
   
   m_full_x = 255.5; 
   m_full_y = 255.5; 
   m_full_w = 512; 
   m_full_h = 512; 
   
   
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
   dev::edtCamera<nsvCtrl>::setupConfig(config);
   
   
   
   dev::frameGrabber<nsvCtrl>::setupConfig(config);

   dev::telemeter<nsvCtrl>::setupConfig(config);
   

}

inline
void nsvCtrl::loadConfig()
{
   dev::stdCamera<nsvCtrl>::loadConfig(config);
   
   m_configFile = "/tmp/andor_";
   m_configFile += configName();
   m_configFile += ".cfg";
   m_cameraModes["onlymode"] = dev::cameraConfig({m_configFile, "", 255, 255, 512, 512, 1, 1, 1000});
   m_startupMode = "onlymode";
   
   if(writeConfig() < 0)
   {
      log<software_critical>({__FILE__,__LINE__});
      m_shutdown = true;
      return;
   }
   
   dev::edtCamera<nsvCtrl>::loadConfig(config);


   if(m_maxEMGain < 1)
   {
      m_maxEMGain = 1;
      log<text_log>("maxEMGain set to 1");
   }

   if(m_maxEMGain > 300)
   {
      m_maxEMGain = 300;
      log<text_log>("maxEMGain set to 300");
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

   if(dev::edtCamera<nsvCtrl>::appStartup() < 0)
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

   return 0;

}



inline
int nsvCtrl::appLogic()
{
   //run stdCamera's appLogic
   if(dev::stdCamera<nsvCtrl>::appLogic() < 0)
   {
      return log<software_error, -1>({__FILE__, __LINE__});
   }
   
   //run edtCamera's appLogic
   if(dev::edtCamera<nsvCtrl>::appLogic() < 0)
   {
      return log<software_error, -1>({__FILE__, __LINE__});
   }
   
   //run frameGrabber's appLogic to see if the f.g. thread has exited.
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
      sleep(30);
      writeConfig();
      m_shutterStatus = "READY";

      state(stateCodes::READY);

      if(m_poweredOn && m_ccdTempSetpt > -999)
      {
         m_poweredOn = false;
         if(setTempSetPt() < 0)
         {
            if(powerState() != 1 || powerStateTarget() != 1) return 0;
            return log<software_error,0>({__FILE__,__LINE__});
         }
      }
   }

   if( state() == stateCodes::READY || state() == stateCodes::OPERATING )
   {
      //Get a lock if we can
      std::unique_lock<std::mutex> lock(m_indiMutex, std::try_to_lock);

      //but don't wait for it, just go back around.
      if(!lock.owns_lock()) return 0;

      if(getTemp() < 0)
      {
         if(m_powerState == 0) return 0;

         state(stateCodes::ERROR);
         return 0;
      }

     if(getFPS() < 0)
      {
         if(m_powerState == 0) return 0;

         state(stateCodes::ERROR);
         return 0;
      }

      if(getEMGain () < 0)
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
      
      if(edtCamera<nsvCtrl>::updateINDI() < 0)
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
   if(m_libInit)
   {
      ShutDown();
      m_libInit = false;
   }
      
   m_powerOnCounter = 0;

   std::lock_guard<std::mutex> lock(m_indiMutex);

   m_shutterStatus = "POWEROFF";
   m_shutterState = 0;
   
   if(stdCamera<nsvCtrl>::onPowerOff() < 0)
   {
      log<software_error>({__FILE__, __LINE__});
   }
   
   if(edtCamera<nsvCtrl>::onPowerOff() < 0)
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
   
   if(edtCamera<nsvCtrl>::whilePowerOff() < 0)
   {
      log<software_error>({__FILE__, __LINE__});
   }
   
   return 0;
}


inline
int nsvCtrl::appShutdown()
{
   if(m_libInit)
   {
      ShutDown();
      m_libInit = false;
   }
      
   dev::frameGrabber<nsvCtrl>::appShutdown();

   dev::telemeter<nsvCtrl>::appShutdown();
   
   return 0;
}

 
inline
int nsvCtrl::cameraSelect()
{
   unsigned int error;
   
   if(!m_libInit)
   {
      char path[] = "/usr/local/etc/andor/";
      error = Initialize(path);

      if(error == DRV_USBERROR || error == DRV_ERROR_NOCAMERA || error == DRV_VXDNOTINSTALLED)
      {
         state(stateCodes::NODEVICE);
         if(!stateLogged())
         {
            log<text_log>("No Andor USB camera found", logPrio::LOG_WARNING);
         }

         ShutDown();

         //Not an error, appLogic should just go on.
         return 0;
      }
      else if(error!=DRV_SUCCESS)
      {
         log<software_critical>({__FILE__, __LINE__, "ANDOR SDK initialization failed: " + andorSDKErrorName(error)});
         ShutDown();
         return -1;
      }
      
      m_libInit = true;
   }
   
   at_32 lNumCameras = 0;
   error = GetAvailableCameras(&lNumCameras);

   if(error != DRV_SUCCESS)
   {
      log<software_critical>({__FILE__, __LINE__, "ANDOR SDK GetAvailableCameras failed: " + andorSDKErrorName(error)});
      return -1;
   }
   
   if(lNumCameras < 1)
   {
      if(!stateLogged())
      {
         log<text_log>("No Andor cameras found after initialization", logPrio::LOG_WARNING);
      }
      state(stateCodes::NODEVICE);
      return 0;
   }
   
   int iSelectedCamera = 0; //We're hard-coded for just one camera!

   int serialNumber = 0;
   error = GetCameraSerialNumber(&serialNumber);
   
   if(error != DRV_SUCCESS)
   {
      log<software_critical>({__FILE__, __LINE__, "ANDOR SDK GetCameraSerialNumber failed: " + andorSDKErrorName(error)});
      return -1;
   }
   
   log<text_log>(std::string("Found Andor USB Camera with serial number ") + std::to_string(serialNumber));
   
   at_32 lCameraHandle;
   error = GetCameraHandle(iSelectedCamera, &lCameraHandle);

   if(error != DRV_SUCCESS)
   {
      log<software_critical>({__FILE__, __LINE__, "ANDOR SDK GetCameraHandle failed: " + andorSDKErrorName(error)});
      return -1;
   }
   
   error = SetCurrentCamera(lCameraHandle);

   if(error != DRV_SUCCESS)
   {
      log<software_critical>({__FILE__, __LINE__, "ANDOR SDK SetCurrentCamera failed: "  + andorSDKErrorName(error)});
      return -1;
   }
   
   char name[MAX_PATH];
   
   error = GetHeadModel(name);
   if(error != DRV_SUCCESS)
   {
      log<software_critical>({__FILE__, __LINE__, "ANDOR SDK GetHeadModel failed: " + andorSDKErrorName(error)});
      return -1;
   }

   state(stateCodes::CONNECTED);
   log<text_log>(std::string("Connected to ") + name +  " with serial number " + std::to_string(serialNumber));
   
   unsigned int eprom;
   unsigned int cofFile;
   unsigned int vxdRev;
   unsigned int vxdVer;
   unsigned int dllRev;
   unsigned int dllVer;
   error = GetSoftwareVersion(&eprom, &cofFile, &vxdRev, &vxdVer, &dllRev, &dllVer);
   if(error != DRV_SUCCESS)
   {
      log<software_critical>({__FILE__, __LINE__, "ANDOR SDK GetSoftwareVersion failed: " + andorSDKErrorName(error)});
      return -1;
   }

   log<text_log>(std::string("eprom: ") + std::to_string(eprom));
   log<text_log>(std::string("cofFile: ") + std::to_string(cofFile));
   log<text_log>(std::string("vxd: ") + std::to_string(vxdVer) + "." + std::to_string(vxdRev));
   log<text_log>(std::string("dll: ") + std::to_string(dllVer) + "." + std::to_string(dllRev));
   
   unsigned int PCB;
   unsigned int Decode;
   unsigned int dummy1;
   unsigned int dummy2;
   unsigned int CameraFirmwareVersion;
   unsigned int CameraFirmwareBuild;
   error = GetHardwareVersion(&PCB, &Decode, &dummy1, &dummy2, &CameraFirmwareVersion, &CameraFirmwareBuild);
   if(error != DRV_SUCCESS)
   {
      log<software_critical>({__FILE__, __LINE__, "ANDOR SDK GetHardwareVersion failed: " + andorSDKErrorName(error)});
      return -1;
   }
   
   log<text_log>(std::string("PCB: ") + std::to_string(PCB));
   log<text_log>(std::string("Decode: ") + std::to_string(Decode));
   log<text_log>(std::string("f/w: ") + std::to_string(CameraFirmwareVersion) + "." + std::to_string(CameraFirmwareBuild));
   
#if 0 //We don't normally need to do this, but keep here in case we want to check in the future
   int em_speeds;
   error=GetNumberHSSpeeds(0,0, &em_speeds);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, std::string("Andor SDK Error from GetNumberHSSpeeds: ") + andorSDKErrorName(error)});
   }
   
   std::cerr << "Number of EM HS speeds: " << em_speeds << "\n";
   for(int i=0; i< em_speeds; ++i)
   {  
      float speed;
      error=GetHSSpeed(0,0,i, &speed);
      std::cerr << i << " " << speed << "\n";
   }
   
   int conv_speeds;
   error=GetNumberHSSpeeds(0,1, &conv_speeds);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, std::string("Andor SDK Error from GetNumberHSSpeeds: ") + andorSDKErrorName(error)});
   }
   
   std::cerr << "Number of Conventional HS speeds: " << conv_speeds << "\n";
   for(int i=0; i< conv_speeds; ++i)
   {  
      float speed;
      error=GetHSSpeed(0,1,i, &speed);
      std::cerr << i << " " << speed << "\n";
   }
#endif
#if 0 //We don't normally need to do this, but keep here in case we want to check in the future
   int v_speeds;
   error=GetNumberVSSpeeds(&v_speeds);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, std::string("Andor SDK Error from GetNumberVSSpeeds: ") + andorSDKErrorName(error)});
   }
   
   std::cerr << "Number of VS speeds: " << v_speeds << "\n";
   for(int i=0; i< v_speeds; ++i)
   {  
      float speed;
      error=GetVSSpeed(i, &speed);
      std::cerr << i << " " << speed << "\n";
   }
#endif
   
   
   //Initialize Shutter to SHUT
   int ss = 2;
   if(m_shutterState == 1) ss = 1;
   else m_shutterState = 0; //handles startup case
   error = SetShutter(1,ss,500,500);
   if(error != DRV_SUCCESS)
   {
      log<software_critical>({__FILE__, __LINE__, "ANDOR SDK SetShutter failed: " + andorSDKErrorName(error)});
      return -1;
   }
   
   // Set CameraLink
   error = SetCameraLinkMode(1);
   if(error != DRV_SUCCESS)
   {
      log<software_critical>({__FILE__, __LINE__, "ANDOR SDK SetCameraLinkMode failed: " + andorSDKErrorName(error)});
      return -1;
   }
   
   //Set Read Mode to --Image--
   error = SetReadMode(4);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, "Andor SDK Error from SetReadMode: " + andorSDKErrorName(error)});
   }
   
   //Set Acquisition mode to --Run Till Abort--
   error = SetAcquisitionMode(5);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, "Andor SDK Error from SetAcquisitionMode: " + andorSDKErrorName(error)});
   }

   //Set to frame transfer mode
   error = SetFrameTransferMode(1);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, "Andor SDK Error from SetFrameTransferMode: " + andorSDKErrorName(error)});
   }
   
   //Set to real gain mode 
   error = SetEMGainMode(3);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, "Andor SDK Error from SetEMGainMode: " + andorSDKErrorName(error)});
   }
   
   //Set default amplifier and speed
   m_readoutSpeedName = m_defaultReadoutSpeed;
   m_readoutSpeedNameSet = m_readoutSpeedName;

   int newa;
   int newhss;
   
   if(readoutParams(newa, newhss, m_readoutSpeedNameSet) < 0)
   {
      return log<text_log,-1>("invalid default readout speed: " + m_readoutSpeedNameSet, logPrio::LOG_ERROR);
   }
   
   // Set the HSSpeed to first index
   /* See page 284
    */
   error = SetHSSpeed(newa,newhss);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, std::string("Andor SDK Error from SetHSSpeed: ") + andorSDKErrorName(error)});
   }
   
   m_vShiftSpeedName = m_defaultVShiftSpeed;
   m_vShiftSpeedNameSet = m_vShiftSpeedName;
   
   int newvs;
   float vs;
   if(vshiftParams(newvs,m_vShiftSpeedNameSet, vs) < 0)
   {
      return log<text_log,-1>("invalid default vert. shift speed: " + m_vShiftSpeedNameSet, logPrio::LOG_ERROR);
   }
   
   // Set the VSSpeed to first index
   error = SetVSSpeed(newvs);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, std::string("Andor SDK Error from SetVSSpeed: ") + andorSDKErrorName(error)});
   }

   m_vshiftSpeed = vs;
   // Set the amplifier
   /* See page 298
    */
   error = SetOutputAmplifier(newa);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, std::string("Andor SDK Error from SetOutputAmplifier: ") + andorSDKErrorName(error)});
   }
   
   //Set initial exposure time
   error = SetExposureTime(0.1);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, "Andor SDK Error from SetExposureTime: " + andorSDKErrorName(error)});
   }
   
   //Turn cooling on:
   if(m_ccdTempSetpt > -999)
   {
      error = CoolerON();
      if(error != DRV_SUCCESS)
      {
         log<software_critical,-1>({__FILE__, __LINE__, "ANDOR SDK CoolerON failed: " + andorSDKErrorName(error)});
      }
      m_tempControlStatus = true;
      m_tempControlStatusStr = "COOLING";
      log<text_log>("enabled temperature control");
   }
   
   int nc;
   GetNumberADChannels(&nc);
   std::cout << "NumberADChannels: " << nc << "\n";
   
   GetNumberAmp(&nc);
   std::cout << "NumberAmp; " << nc << "\n";
   
   return 0;

}

inline
int nsvCtrl::getTemp()
{
   //unsigned int error;
   //int temp_low {999}, temp_high {999};
   //error = GetTemperatureRange(&temp_low, &temp_high); 

   float temp = -999;
   unsigned int status = GetTemperatureF(&temp);
   
   std::string cooling;
   switch(status)
   {
      case DRV_TEMPERATURE_OFF: 
         m_tempControlStatusStr =  "OFF"; 
         m_tempControlStatus = false;
         m_tempControlOnTarget = false;
         break;
      case DRV_TEMPERATURE_STABILIZED: 
         m_tempControlStatusStr = "STABILIZED"; 
         m_tempControlStatus = true;
         m_tempControlOnTarget = true;
         break;
      case DRV_TEMPERATURE_NOT_REACHED: 
         m_tempControlStatusStr = "COOLING";
         m_tempControlStatus = true;
         m_tempControlOnTarget = false;
         break;
      case DRV_TEMPERATURE_NOT_STABILIZED: 
         m_tempControlStatusStr = "NOT STABILIZED";
         m_tempControlStatus = true;
         m_tempControlOnTarget = false;
         break;
      case DRV_TEMPERATURE_DRIFT: 
         m_tempControlStatusStr = "DRIFTING";
         m_tempControlStatus = true;
         m_tempControlOnTarget = false;
         break;
      default: 
         m_tempControlStatusStr =  "UNKOWN";
         m_tempControlStatus = false;
         m_tempControlOnTarget = false;
         m_ccdTemp = -999;
         log<software_error>({__FILE__, __LINE__, "ANDOR SDK GetTemperatureF:" + andorSDKErrorName(status)});
         return -1;
   }

   m_ccdTemp = temp;
   recordCamera();
      
   return 0;

}

inline
int nsvCtrl::getEMGain()
{
   unsigned int error;
   int gain;

   error = GetEMCCDGain(&gain);
   if( error !=DRV_SUCCESS)
   {
      log<software_error>({__FILE__,__LINE__, "Andor SDK error from GetEMCCDGain: " + andorSDKErrorName(error)});
      return -1;
   }

   if(gain == 0) gain = 1;

   m_emGain = gain;
   
   return 0;
}

inline
int nsvCtrl::setReadoutSpeed()
{
   recordCamera(true);
   AbortAcquisition();
   state(stateCodes::CONFIGURING);

   int newa;
   int newhss;
   
   if( readoutParams(newa, newhss, m_readoutSpeedNameSet) < 0)
   {
      return log<text_log,-1>("invalid readout speed: " + m_readoutSpeedNameSet);
   }
   
   if(newa == 1 && m_cropMode)
   {
      log<text_log>("disabling crop mode for CCD readout", logPrio::LOG_NOTICE);
      m_cropModeSet = false;
   }
   // Set the HSSpeed to first index
   /* See page 284
    */
   unsigned int error = SetHSSpeed(newa,newhss);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, std::string("Andor SDK Error from SetHSSpeed: ") + andorSDKErrorName(error)});
   }
   
   // Set the amplifier
   /* See page 298
    */
   error = SetOutputAmplifier(newa);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, std::string("Andor SDK Error from SetOutputAmplifier: ") + andorSDKErrorName(error)});
   }

   log<text_log>("Set readout speed to " + m_readoutSpeedNameSet + " (" + std::to_string(newa) + "," + std::to_string(newhss) + ")");

      
   m_readoutSpeedName = m_readoutSpeedNameSet;
   
   m_nextMode = m_modeName;
   m_reconfig = true;

   return 0;
}



inline
int nsvCtrl::setVShiftSpeed()
{
   recordCamera(true);
   AbortAcquisition();
   state(stateCodes::CONFIGURING);

   int newvs;
   float vs;
   if( vshiftParams(newvs, m_vShiftSpeedNameSet, vs) < 0)
   {
      return log<text_log,-1>("invalid vertical shift speed: " + m_vShiftSpeedNameSet);
   }
   
   // Set the VSSpeed
   unsigned int error = SetVSSpeed(newvs);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, std::string("Andor SDK Error from SetVSSpeed: ") + andorSDKErrorName(error)});
   }
   

   log<text_log>("Set vertical shift speed to " + m_vShiftSpeedNameSet + " (" + std::to_string(newvs) + ")");

      
   m_vShiftSpeedName = m_vShiftSpeedNameSet;
   m_vshiftSpeed = vs;

   m_nextMode = m_modeName;
   m_reconfig = true;

   return 0;
}

inline
int nsvCtrl::setEMGain()
{
   int amp;
   int hss;
      
   readoutParams(amp,hss, m_readoutSpeedName);
      
   if(amp != 0)
   {
      log<text_log>("Attempt to set EM gain while in conventional amplifier.", logPrio::LOG_NOTICE);
      return 0;
   }
   
   int emg = m_emGainSet;

   if(emg == 1) emg = 0;

   if(emg < 0)
   {
      emg = 0;
      log<text_log>("EM gain limited to 0", logPrio::LOG_WARNING);
   }
   
   if(emg > m_maxEMGain)
   {
      emg = m_maxEMGain;
      log<text_log>("EM gain limited to maxEMGain = " + std::to_string(emg), logPrio::LOG_WARNING);
   }
   
   unsigned int error = SetEMCCDGain(emg);
   if( error !=DRV_SUCCESS)
   {
      log<software_error>({__FILE__,__LINE__, "Andor SDK error from SetEMCCDGain: " + andorSDKErrorName(error)});
      return -1;
   }

   log<text_log>("Set EM Gain to: " + std::to_string(emg), logPrio::LOG_WARNING);
   
   return 0;
}

inline
int nsvCtrl::setCropMode()
{
   recordCamera(true);
   AbortAcquisition();
   state(stateCodes::CONFIGURING);
   
   //Check if we're in the EMCCD amplifier
   if(m_cropModeSet == true)
   {
      int amp;
      int hss;
      
      readoutParams(amp,hss, m_readoutSpeedName);
   
      if(amp == 1)
      {
         m_cropModeSet = false;
         log<text_log>("Can not set crop mode in CCD mode", logPrio::LOG_ERROR);
      }
   }
   
   m_nextMode = m_modeName;
   m_reconfig = true;
   
   return 0;
}

inline
int nsvCtrl::setShutter( unsigned os )
{
   recordCamera(true);
   AbortAcquisition();
   state(stateCodes::CONFIGURING);
    
   if(os == 0) //Shut
   {
      int error = SetShutter(1,2,500,500);
      if(error != DRV_SUCCESS)
      {
         return log<software_error, -1>({__FILE__, __LINE__, "ANDOR SDK SetShutter failed: " + andorSDKErrorName(error)});
      }

      m_shutterState = 0;
   }
   else //Open
   {
      int error = SetShutter(1,1,500,500);
      if(error != DRV_SUCCESS)
      {
         return log<software_error, -1>({__FILE__, __LINE__, "ANDOR SDK SetShutter failed: " + andorSDKErrorName(error)});
      }

      m_shutterState = 1;
   }

   m_nextMode = m_modeName;
   m_reconfig = true;

   return 0;
}






inline 
int nsvCtrl::writeConfig()
{
   std::string configFile = "/tmp/andor_";
   configFile += configName();
   configFile += ".cfg";
   
   std::ofstream fout;
   fout.open(configFile);
   
   if(fout.fail())
   {
      log<software_error>({__FILE__, __LINE__, "error opening config file for writing"});
      return -1;
   }

   int w = m_nextROI.w / m_nextROI.bin_x;
   int h = m_nextROI.h / m_nextROI.bin_y;
   
   fout << "camera_class:                  \"Andor\"\n";
   fout << "camera_model:                  \"iXon Ultra 897\"\n";
   fout << "camera_info:                   \"512x512 (1-tap, freerun)\"\n";
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
   if(m_tempControlStatusSet)
   {
      unsigned int error = CoolerON();
      if(error != DRV_SUCCESS)
      {
         log<software_critical>({__FILE__, __LINE__, "ANDOR SDK CoolerON failed: " + andorSDKErrorName(error)});
         return -1;
      }
      m_tempControlStatus = true;
      m_tempControlStatusStr = "COOLING";
      recordCamera();
      log<text_log>("enabled temperature control");
      return 0;
   }
   else
   {
      unsigned int error = CoolerOFF();
      if(error != DRV_SUCCESS)
      {
         log<software_critical>({__FILE__, __LINE__, "ANDOR SDK CoolerOFF failed: " + andorSDKErrorName(error)});
         return -1;
      }
      m_tempControlStatus = false;
      m_tempControlStatusStr = "OFF";
      recordCamera();
      log<text_log>("disabled temperature control");
      return 0;
   }
}

inline
int nsvCtrl::setTempSetPt()
{
   int temp = m_ccdTempSetpt + 0.5;
   
   unsigned int error = SetTemperature(temp);
   
   if(error != DRV_SUCCESS)
   {
      log<software_critical>({__FILE__, __LINE__, "ANDOR SDK setTemperature failed: " + andorSDKErrorName(error)});
      return -1;
   }
   
  return 0;

}

inline
int nsvCtrl::getFPS()
{
   float exptime;
   float accumCycletime;
   float kinCycletime;

   unsigned int error = GetAcquisitionTimings(&exptime, &accumCycletime, &kinCycletime);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, "ANDOR SDK error from GetAcquisitionTimings: " + andorSDKErrorName(error)});
   }

   m_expTime = exptime;
   
   float readoutTime;
   error = GetReadOutTime(&readoutTime);
   if(error != DRV_SUCCESS)
   {
      return log<software_error,-1>({__FILE__, __LINE__, "ANDOR SDK error from GetReadOutTime: " + andorSDKErrorName(error)});
   }
   
   m_fps = 1.0/accumCycletime;
   
   return 0;

}

inline 
int nsvCtrl::setExpTime()
{
   recordCamera(true);
   AbortAcquisition();
   state(stateCodes::CONFIGURING);
   
   unsigned int error = SetExposureTime(m_expTimeSet);
   if(error != DRV_SUCCESS)
   {
      log<software_critical>({__FILE__, __LINE__, "ANDOR SDK SetExposureTime failed: " + andorSDKErrorName(error)});
      return -1;
   }
   m_nextMode = m_modeName;
   m_reconfig = true;
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
   recordCamera(true);
   AbortAcquisition();
   state(stateCodes::CONFIGURING);
   
   m_nextMode = m_modeName;
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

   unsigned int error;

   int status;
   error = GetStatus(&status);
   if(error != DRV_SUCCESS)
   {
      state(stateCodes::ERROR);
      return log<software_error,-1>({__FILE__, __LINE__, "Andor SDK Error from GetStatus: " + andorSDKErrorName(error)});
   }
   
   if(status != DRV_IDLE) 
   {
      return 0;
   }

   int x0 = (m_nextROI.x - 0.5*(m_nextROI.w - 1)) + 1;
   int y0 = (m_nextROI.y - 0.5*(m_nextROI.h - 1)) + 1;
    
   if(m_cropModeSet)
   {
      m_cropMode = m_cropModeSet;
      
      error = SetIsolatedCropModeEx(1, m_nextROI.h, m_nextROI.w, m_nextROI.bin_y, m_nextROI.bin_x, x0, y0);
      
      if(error != DRV_SUCCESS)
      {
         if(error == DRV_P2INVALID)
         {
            log<text_log>(std::string("crop mode invalid height: ") + std::to_string(m_nextROI.h), logPrio::LOG_ERROR);
         }
         else if(error == DRV_P3INVALID)
         {
            log<text_log>(std::string("crop mode invalid width: ") + std::to_string(m_nextROI.w), logPrio::LOG_ERROR);
         }
         else if(error == DRV_P4INVALID)
         {
            log<text_log>(std::string("crop mode invalid y binning: ") + std::to_string(m_nextROI.bin_y), logPrio::LOG_ERROR);
         }
         else if(error == DRV_P5INVALID)
         {
            log<text_log>(std::string("crop mode invalid x binning: ") + std::to_string(m_nextROI.bin_x), logPrio::LOG_ERROR);
         }
         else if(error == DRV_P6INVALID)
         {
            log<text_log>(std::string("crop mode invalid x center: ") + std::to_string(m_nextROI.x) + "/" + std::to_string(x0), logPrio::LOG_ERROR);
         }
         else if(error == DRV_P7INVALID)
         {
            log<text_log>(std::string("crop mode invalid y center: ") + std::to_string(m_nextROI.y) + "/" + std::to_string(y0), logPrio::LOG_ERROR);
         }
         else
         {
            log<software_error>({__FILE__, __LINE__, "Andor SDK Error from SetIsolatedCropModeEx: " + andorSDKErrorName(error)});
         }
         
         m_nextROI.x = m_currentROI.x;
         m_nextROI.y = m_currentROI.y;
         m_nextROI.w = m_currentROI.w;
         m_nextROI.h = m_currentROI.h;
         m_nextROI.bin_x = m_currentROI.bin_x;
         m_nextROI.bin_y = m_currentROI.bin_y;
               
         m_nextMode = m_modeName;
      
         state(stateCodes::ERROR);
         return -1;
      }
      
      //Set low-latency crop mode
      error = SetIsolatedCropModeType(1);
      if(error != DRV_SUCCESS)
      {
         log<software_error>({__FILE__, __LINE__, "SetIsolatedCropModeType: " + andorSDKErrorName(error)});
      }
   }
   else
   {
      m_cropMode = m_cropModeSet;
      
      error = SetIsolatedCropModeEx(0, m_nextROI.h, m_nextROI.w, m_nextROI.bin_y, m_nextROI.bin_x, x0, y0);
      if(error != DRV_SUCCESS)
      {
         log<software_error>({__FILE__, __LINE__, "SetIsolatedCropModeEx(0,): " + andorSDKErrorName(error)});
      }
            
      //Setup Image dimensions
      /* SetImage(int hbin, int vbin, int hstart, int hend, int vstart, int vend)
       * hbin: number of pixels to bin horizontally
       * vbin: number of pixels to bin vertically
       * hstart: Starting Column (inclusive)
       * hend: End column (inclusive)
       * vstart: Start row (inclusive)
       * vend: End row (inclusive)
       */
      error = SetImage(m_nextROI.bin_x, m_nextROI.bin_y, x0, x0 + m_nextROI.w - 1, y0, y0 + m_nextROI.h - 1);
      if(error != DRV_SUCCESS)
      {
         if(error == DRV_P1INVALID)
         {
            log<text_log>(std::string("invalid x-binning: ") + std::to_string(m_nextROI.bin_x), logPrio::LOG_ERROR);
         }
         else if(error == DRV_P2INVALID)
         {
            log<text_log>(std::string("invalid y-binning: ") + std::to_string(m_nextROI.bin_y), logPrio::LOG_ERROR);
         }
         else if(error == DRV_P3INVALID)
         {
            log<text_log>(std::string("invalid x-center: ") + std::to_string(m_nextROI.x) + "/" + std::to_string(x0), logPrio::LOG_ERROR);
         }
         else if(error == DRV_P4INVALID)
         {
            log<text_log>(std::string("invalid width: ") + std::to_string(m_nextROI.w), logPrio::LOG_ERROR);
         }
         else if(error == DRV_P5INVALID)
         {
            log<text_log>(std::string("invalid y-center: ") + std::to_string(m_nextROI.y) + "/" + std::to_string(y0), logPrio::LOG_ERROR);
         }
         else if(error == DRV_P6INVALID)
         {
            log<text_log>(std::string("invalid height: ") + std::to_string(m_nextROI.h), logPrio::LOG_ERROR);
         }
         else
         {
            log<software_error>({__FILE__, __LINE__, "Andor SDK Error from SetImage: " + andorSDKErrorName(error)});
         }
      
         m_nextROI.x = m_currentROI.x;
         m_nextROI.y = m_currentROI.y;
         m_nextROI.w = m_currentROI.w;
         m_nextROI.h = m_currentROI.h;
         m_nextROI.bin_x = m_currentROI.bin_x;
         m_nextROI.bin_y = m_currentROI.bin_y;
               
         m_nextMode = m_modeName;
      
         state(stateCodes::ERROR);
         return -1;
      }
      
      
   }
   
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

   //We also update target to the settable values
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


   ///\todo This should check whether we have a match between EDT and the camera right?
   m_width = m_currentROI.w/m_currentROI.bin_x;
   m_height = m_currentROI.h/m_currentROI.bin_y;
   m_dataType = _DATATYPE_INT16;

   
   // Print Detector Frame Size
   //std::cout << "Detector Frame is: " << width << "x" << height << "\n";

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
   unsigned int error;
   int status;
   error = GetStatus(&status);
   if(error != DRV_SUCCESS)
   {
      state(stateCodes::ERROR);
      return log<software_error,-1>({__FILE__, __LINE__, "Andor SDK Error from GetStatus: " + andorSDKErrorName(error)});
   }
   
   if(status != DRV_IDLE) 
   {
      state(stateCodes::OPERATING);
      return 0;
   }
   
   error = StartAcquisition();
   if(error != DRV_SUCCESS)
   {
      state(stateCodes::ERROR);
      return log<software_error,-1>({__FILE__, __LINE__, "Andor SDK Error from StartAcquisition: " + andorSDKErrorName(error)});
   }
   
   state(stateCodes::OPERATING);
   recordCamera();
   
   sleep(1); //make sure camera is rully running before we try to synch with it.

   return edtCamera<nsvCtrl>::pdvStartAcquisition();
}

inline
int nsvCtrl::acquireAndCheckValid()
{
   return edtCamera<nsvCtrl>::pdvAcquire( m_currImageTimestamp );

}

inline
int nsvCtrl::loadImageIntoStream(void * dest)
{
   if( frameGrabber<nsvCtrl>::loadImageIntoStreamCopy(dest, m_image_p, m_width, m_height, m_typeSize) == nullptr) return -1;

   return 0;
 }

inline
int nsvCtrl::reconfig()
{
   //lock mutex
   //std::unique_lock<std::mutex> lock(m_indiMutex);
   recordCamera(true);
   AbortAcquisition();
   state(stateCodes::CONFIGURING);

   writeConfig();
   
   int rv = edtCamera<nsvCtrl>::pdvReconfig();
   if(rv < 0) return rv;
   
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
