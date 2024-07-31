/** \file asiCtrl.hpp
  * \brief The MagAO-X Princeton Instruments EMCCD camera controller.
  *
  * \author Jared R. Males (jaredmales@gmail.com)
  *
  * \ingroup asiCtrl_files
  */

#ifndef asiCtrl_hpp
#define asiCtrl_hpp


//#include <ImageStruct.h>
#include <ImageStreamIO/ImageStreamIO.h>

#include "ASICamera2.h"

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#define DEBUG

#ifdef DEBUG
#define BREADCRUMB  std::cerr << __FILE__ << " " << __LINE__ << "\n";
#else
#define BREADCRUMB
#endif

typedef int16_t pixelT;

namespace MagAOX
{
namespace app
{

/** \defgroup asiCtrl ZWO ASI camera
  * \brief Control of a ZWO ASI camera
  *
  * <a href="../handbook/operating/software/apps/asiCtrl.html">Application Documentation</a>
  *
  * \ingroup apps
  *
  */

/** \defgroup asiCtrl_files ZWO ASI Camera Files
  * \ingroup asiCtrl
  */

/** MagAO-X/SCOOB application to control a ZWO ASI Camera
  *
  * \ingroup asiCtrl
  *
  * \todo Config item for ImageStreamIO name filename
  * \todo implement ImageStreamIO circular buffer, with config setting
  */
class asiCtrl : public MagAOXApp<>, public dev::stdCamera<asiCtrl>, public dev::frameGrabber<asiCtrl>, public dev::telemeter<asiCtrl>
{

   friend class dev::stdCamera<asiCtrl>;
   friend class dev::frameGrabber<asiCtrl>;
   friend class dev::telemeter<asiCtrl>;

   typedef MagAOXApp<> MagAOXAppT;

public:
   /** \name app::dev Configurations
     *@{
     */
   static constexpr bool c_stdCamera_tempControl = true; ///< app::dev config to tell stdCamera to expose temperature controls
   
   static constexpr bool c_stdCamera_temp = true; ///< app::dev config to tell stdCamera to expose temperature
   
   static constexpr bool c_stdCamera_readoutSpeed = false; ///< app::dev config to tell stdCamera to expose readout speed controls
   
   static constexpr bool c_stdCamera_vShiftSpeed = false; ///< app:dev config to tell stdCamera to expose vertical shift speed control

   static constexpr bool c_stdCamera_emGain = true; ///< app::dev config to tell stdCamera to expose EM gain controls 

   static constexpr bool c_stdCamera_exptimeCtrl = true; ///< app::dev config to tell stdCamera to expose exposure time controls
   
   static constexpr bool c_stdCamera_fpsCtrl = false; ///< app::dev config to tell stdCamera not to expose FPS controls

   static constexpr bool c_stdCamera_fps = false; ///< app::dev config to tell stdCamera not to expose FPS status

   static constexpr bool c_stdCamera_synchro = false; ///< app::dev config to tell stdCamera to not expose synchro mode controls
   
   static constexpr bool c_stdCamera_usesModes = false; ///< app:dev config to tell stdCamera not to expose mode controls
   
   static constexpr bool c_stdCamera_usesROI = true; ///< app:dev config to tell stdCamera to expose ROI controls

   static constexpr bool c_stdCamera_cropMode = false; ///< app:dev config to tell stdCamera to expose Crop Mode controls
   
   static constexpr bool c_stdCamera_hasShutter = false; ///< app:dev config to tell stdCamera to expose shutter controls
      
   static constexpr bool c_stdCamera_usesStateString = false; ///< app::dev confg to tell stdCamera to expose the state string property
   
   static constexpr bool c_frameGrabber_flippable = true; ///< app:dev config to tell framegrabber this camera can be flipped
   
   ///@}
   
protected:

   /** \name configurable parameters
     *@{
     */


   //std::string m_serialNumber; ///< The camera's identifying serial number

   ASI_CAMERA_INFO m_camInfo;
   int m_camNum;
   bool m_running;
   //std::string m_camName; // unique identifier???
   std::string m_camName;

   ///@}

   int m_bits {14};
   pixelT m_bfactor;
   
   //piint m_timeStampMask {PicamTimeStampsMask_ExposureStarted}; // time stamp at end of exposure
   //pi64s m_tsRes; // time stamp resolution
   //piint m_frameSize;
   //double m_camera_timestamp {0.0};
   //piflt m_FrameRateCalculation;
   //piflt m_ReadOutTimeCalculation;
   
   //PicamHandle m_cameraHandle {0};
   //PicamHandle m_modelHandle {0};

   //PicamAcquisitionBuffer m_acqBuff;
   //PicamAvailableData m_available;

   long m_imgSize;
   unsigned char* m_imgBuff;
   //std::string m_cameraName;
   //std::string m_cameraModel;

   //int m_gain;

public:

   ///Default c'tor
   asiCtrl();

   ///Destructor
   ~asiCtrl() noexcept;

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

protected:
   int getASIParameter(long & value,
                        ASI_CONTROL_TYPE parameter
                        );

   int setASIParameter(ASI_CONTROL_TYPE parameter,
                        long value,
                        bool commit);

   int connect();

   int getAcquisitionState();

   int getTemp();

   // stdCamera interface:
   
   //This must set the power-on default values of
   /* -- m_ccdTempSetpt
    * -- m_currentROI 
    */
   int powerOnDefaults();
   
   int setTempControl();
   int setTempSetPt();
   int setReadoutSpeed();
   //int setVShiftSpeed();
   int getEMGain();
   int setEMGain();
   int setExpTime();
   //int capExpTime(piflt& exptime);
   //int setFPS();

   /// Check the next ROI
   /** Checks if the target values are valid and adjusts them to the closest valid values if needed.
     *
     * \returns 0 if successful
     * \returns -1 otherwise
     */
   int checkNextROI();

   int setNextROI();
   //int setShutter(int sh);
   
   //Framegrabber interface:
   int configureAcquisition();
   float fps();
   int startAcquisition();
   int acquireAndCheckValid();
   int loadImageIntoStream(void * dest);
   int reconfig();


   //INDI:
protected:

   //pcf::IndiProperty m_indiP_readouttime;

public:
   //INDI_NEWCALLBACK_DECL(asiCtrl, m_indiP_adcquality);

   /** \name Telemeter Interface
     * 
     * @{
     */ 
   int checkRecordTimes();
   
   int recordTelem( const telem_stdcam * );
   
   
   ///@}
};

inline
asiCtrl::asiCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
   m_powerMgtEnabled = true;

   //m_acqBuff.memory_size = 0;
   //m_acqBuff.memory = 0;

   m_default_x = 4143.5; 
   m_default_y = 2821.5; 
   m_default_w = 2048;  
   m_default_h = 2048;  
   m_default_bin_x = 2;
   m_default_bin_y = 2;
      
   m_full_x = 4143.5; 
   m_full_y = 2821.5; 
   m_full_w = 8288; 
   m_full_h = 5644; 

   //powerOnDefaults();
   
   m_maxEMGain = 450;
   
   return;
}

inline
asiCtrl::~asiCtrl() noexcept
{
   /*if(m_imgBuff) // fix me
   {
      free(m_imgBuff);
   }*/

   return;
}

inline
void asiCtrl::setupConfig()
{
   config.add("camera.cameraName", "", "camera.cameraName", argType::Required, "camera", "cameraName", false, "str", "The identifying name of the camera.");

   dev::stdCamera<asiCtrl>::setupConfig(config);
   dev::frameGrabber<asiCtrl>::setupConfig(config);
   //dev::dssShutter<asiCtrl>::setupConfig(config);
   dev::telemeter<asiCtrl>::setupConfig(config);
}

inline
void asiCtrl::loadConfig()
{

   config(m_camName, "camera.cameraName");

   dev::stdCamera<asiCtrl>::loadConfig(config);
   dev::frameGrabber<asiCtrl>::loadConfig(config);
   //dev::dssShutter<picamCtrl>::loadConfig(config);
   dev::telemeter<asiCtrl>::loadConfig(config);

}

inline
int asiCtrl::appStartup()
{

   // DELETE ME
   //m_outfile = fopen("/home/xsup/test2.txt", "w");

   //createROIndiNumber( m_indiP_readouttime, "readout_time", "Readout Time (s)");
   //indi::addNumberElement<float>( m_indiP_readouttime, "value", 0.0, std::numeric_limits<float>::max(), 0.0,  "%0.1f", "readout time");
   //registerIndiPropertyReadOnly( m_indiP_readouttime );

   
   //m_minTemp = -55;
   //m_maxTemp = 25;
   //m_stepTemp = 0;
   
   m_minROIx = 0;
   m_maxROIx = 4144;
   m_stepROIx = 0;
   
   m_minROIy = 0;
   m_maxROIy = 2822;
   m_stepROIy = 0;
   
   m_minROIWidth = 1;
   m_maxROIWidth = 2048;
   m_stepROIWidth = 8;
   
   m_minROIHeight = 1;
   m_maxROIHeight = 2048;
   m_stepROIHeight = 2;
   
   m_minROIBinning_x = 1;
   m_maxROIBinning_x = 4; // not actually sure
   m_stepROIBinning_x = 1;

   m_minROIBinning_y = 1;
   m_maxROIBinning_y = 4; // not actually sure
   m_stepROIBinning_y = 1;
   
   
   if(dev::stdCamera<asiCtrl>::appStartup() < 0)
   {
      return log<software_critical,-1>({__FILE__,__LINE__});
   }
   
   if(dev::frameGrabber<asiCtrl>::appStartup() < 0)
   {
      return log<software_critical,-1>({__FILE__,__LINE__});
   }

   if(dev::telemeter<asiCtrl>::appStartup() < 0)
   {
      return log<software_error,-1>({__FILE__,__LINE__});
   }

   state(stateCodes::NOTCONNECTED);

   
   return 0;

}

inline
int asiCtrl::appLogic()
{

   //and run stdCamera's appLogic
   if(dev::stdCamera<asiCtrl>::appLogic() < 0)
   {
      return log<software_error, -1>({__FILE__, __LINE__});
   }
   
   //first run frameGrabber's appLogic to see if the f.g. thread has exited.
   if(dev::frameGrabber<asiCtrl>::appLogic() < 0)
   {
      return log<software_error, -1>({__FILE__, __LINE__});
   }

   //state(stateCodes::READY);

   if( state() == stateCodes::NOTCONNECTED || state() == stateCodes::NODEVICE || state() == stateCodes::ERROR)
   {
      m_reconfig = true; //Trigger a f.g. thread reconfig.

      //Might have gotten here because of a power off.
      //if(MagAOXAppT::m_powerState == 0) return 0;
      if(powerState() != 1 || powerStateTarget() != 1) return 0;

      std::unique_lock<std::mutex> lock(m_indiMutex);
      if(connect() < 0)
      {
         if(powerState() != 1 || powerStateTarget() != 1) return 0;
         log<software_error>({__FILE__, __LINE__});
      }

      if(state() != stateCodes::CONNECTED) return 0;
   }

   if( state() == stateCodes::CONNECTED )
   {
      //Get a lock
      std::unique_lock<std::mutex> lock(m_indiMutex);

      if( getAcquisitionState() < 0 )
      {
         return log<software_error,0>({__FILE__,__LINE__});
      }

      /*if( setTempSetPt() < 0 ) //m_ccdTempSetpt already set on power on
      {
         return log<software_error,0>({__FILE__,__LINE__});
      }*/

      
      if(frameGrabber<asiCtrl>::updateINDI() < 0)
      {
         return log<software_error,0>({__FILE__,__LINE__});
      }
      
      //setPicamParameter(m_modelHandle, PicamParameter_DisableCoolingFan, PicamCoolingFanStatus_On);
      
      
   }


   if( state() == stateCodes::READY || state() == stateCodes::OPERATING )
   {
      //Get a lock if we can
      std::unique_lock<std::mutex> lock(m_indiMutex, std::try_to_lock);


      //but don't wait for it, just go back around.
      if(!lock.owns_lock()) return 0;

      if(getAcquisitionState() < 0)
      {
         if(MagAOXAppT::m_powerState == 0) return 0;

         state(stateCodes::ERROR);
         return 0;
      }


      if(getTemp() < 0)
      {
         if(MagAOXAppT::m_powerState == 0) return 0;

         state(stateCodes::ERROR);
         return 0;
      }

      if(stdCamera<asiCtrl>::updateINDI() < 0)
      {
         return log<software_error,0>({__FILE__,__LINE__});
      }
      
      if(frameGrabber<asiCtrl>::updateINDI() < 0)
      {
         return log<software_error,0>({__FILE__,__LINE__});
      }

      if(telemeter<asiCtrl>::appLogic() < 0)
      {
         log<software_error>({__FILE__, __LINE__});
         return 0;
      }

   }

   //Fall through check?
   return 0;

}

inline
int asiCtrl::onPowerOff()
{
   std::lock_guard<std::mutex> lock(m_indiMutex);

   if(m_camNum >= 0)
   {
      ASIStopVideoCapture(m_camNum);
      ASICloseCamera(m_camNum);
      m_running = false;
      m_camNum = -1;
   }

   //asi_UninitializeLibrary();

   if(stdCamera<asiCtrl>::onPowerOff() < 0)
   {
      log<software_error>({__FILE__, __LINE__});
   }
   
   return 0;
}

inline
int asiCtrl::whilePowerOff()
{

   if(stdCamera<asiCtrl>::onPowerOff() < 0 )
   {
      log<software_error>({__FILE__, __LINE__});
   }
   
   return 0;
}

inline
int asiCtrl::appShutdown()
{
   dev::frameGrabber<asiCtrl>::appShutdown();

   if(m_camNum >= 0)
   {
      ASIStopVideoCapture(m_camNum);
      ASICloseCamera(m_camNum);
      m_camNum = -1;
   }

   //asi_UninitializeLibrary();

   ///\todo error check these base class fxns.
   dev::frameGrabber<asiCtrl>::appShutdown();
   //dev::dssShutter<asiCtrl>::appShutdown();

   return 0;
}

inline
int asiCtrl::getASIParameter( long & value,
                                  ASI_CONTROL_TYPE parameter
                                )
{
   ASI_BOOL bAuto;
   ASIGetControlValue(m_camNum, parameter, &value, &bAuto);

   if(MagAOXAppT::m_powerState == 0) return -1; //Flag error but don't log

   return 0;
}

inline
int asiCtrl::setASIParameter( ASI_CONTROL_TYPE parameter,
                                  long value,
                                  bool commit
                                )
{
   ASISetControlValue(m_camNum, parameter, value, ASI_FALSE);

   if(!commit) return 0; // what is this?

   return 0;
}

inline
int asiCtrl::connect()
{


   // default ROIs
   m_nextROI.x = m_default_x;
   m_nextROI.y = m_default_y;
   m_nextROI.w = m_default_w;
   m_nextROI.h = m_default_h;
   m_nextROI.bin_x = m_default_bin_x;
   m_nextROI.bin_y = m_default_bin_y;

   if(m_imgBuff)
   {
      std::cerr << "Clearing\n";
      free(m_imgBuff);
      m_imgBuff = NULL;
      //m_acqBuff.memory = NULL;
      //m_acqBuff.memory_size = 0;
   }


   if(m_camNum >= 0)
   {
      ASIStopVideoCapture(m_camNum);
      ASICloseCamera(m_camNum);
      m_running = false;
      m_camNum = -1;
   }

   ;

   std::cout << "Looking for cameras\n";

   int numDevices = ASIGetNumOfConnectedCameras();

   if(powerState() != 1 || powerStateTarget() != 1) return 0;

   if(numDevices <= 0)
   {

      state(stateCodes::NODEVICE);
      if(!stateLogged())
      {
         log<text_log>("no ASI Cameras available.");
      }
      return 0;
   }


   std::cout << "Looking for the camera\n";

   for(int i=0; i< numDevices; ++i)
   {

      ASIGetCameraProperty(&m_camInfo, i);
      std::cout << "Found camera name: " << m_camInfo.Name << "\n";
      std::cout << "But looking for name: " << m_camName << "\n";

      if( strcmp(m_camInfo.Name,m_camName.c_str()) == 0 )
      {
         std::cerr << "Camera was found.  Now connecting.\n";

         m_camNum = i;
         ASIOpenCamera(i);
         ASIInitCamera(i);

         state(stateCodes::CONNECTED);

         return 0;
      }
      else
      {
         if(powerState() != 1 || powerStateTarget() != 1) return 0;
      }
   }

   state(stateCodes::NODEVICE);
   if(!stateLogged())
   {
      log<text_log>("Camera not found in available ids.");
      m_camNum = -1;
   }

   return 0;
}

/*inline
int asiCtrl::setFPS()
{
   return 0;
}*/

inline 
int asiCtrl::powerOnDefaults()
{
   m_currentROI.x = 4143.5;
   m_currentROI.y = 2821.5;
   m_currentROI.w = 2048;
   m_currentROI.h = 2048;
   m_currentROI.bin_x = 2;
   m_currentROI.bin_y = 2;

   //m_gain = 1;
   m_emGainSet = 0;

   //m_width = 8288;
   //m_height = 5644;
   //std::cout << m_width << " " << m_height << "\n";

   return 0;
}

inline
int asiCtrl::setEMGain()
{
   // not EM gain, but this already exists, so...
   int rv = ASISetControlValue(m_camNum, ASI_GAIN, m_emGainSet, ASI_FALSE); 
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting gain"});
      return -1;
   }

   // maybe report back the actual gain camera went to?
   long gainReal;
	ASI_BOOL bAuto;
   ASIGetControlValue(m_camNum, ASI_GAIN, &gainReal, &bAuto);

   m_emGainSet = gainReal;

   log<text_log>( "Set gain to: " + std::to_string(m_emGainSet));

   updateIfChanged(m_indiP_emGain, "current", m_emGainSet, INDI_IDLE);

   return 0;

}

inline
int asiCtrl::getEMGain()
{
   long gainReal;
	ASI_BOOL bAuto;
   ASIGetControlValue(m_camNum, ASI_GAIN, &gainReal, &bAuto);

   log<text_log>( "Got gain of: " + std::to_string(gainReal));

   m_emGain = gainReal;
   
   return 0;
}


inline
int asiCtrl::setExpTime()
{

   int rv = ASISetControlValue(m_camNum, ASI_EXPOSURE, m_expTimeSet * 1e6, ASI_FALSE);
   sleep(1);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting exposure time"});
      return -1;
   }

   // maybe report back the actual exposure time the camera went to?
   long expTimeReal;
	ASI_BOOL bAuto;
   ASIGetControlValue(m_camNum, ASI_EXPOSURE, &expTimeReal, &bAuto);

   m_expTime = expTimeReal*1e-6;

   //recordCamera();
   log<text_log>( "Set exposure time to: " + std::to_string(m_expTime) + " sec");

   updateIfChanged(m_indiP_exptime, "current", m_expTime, INDI_IDLE);
   
   return 0;
}

inline
int asiCtrl::checkNextROI()
{
   updateIfChanged( m_indiP_roi_x, "target", m_nextROI.x, INDI_OK);
   updateIfChanged( m_indiP_roi_y, "target", m_nextROI.y, INDI_OK);
   updateIfChanged( m_indiP_roi_w, "target", m_nextROI.w, INDI_OK);
   updateIfChanged( m_indiP_roi_h, "target", m_nextROI.h, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_x, "target", m_nextROI.bin_x, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_y, "target", m_nextROI.bin_y, INDI_OK);

   return 0;
}

//Set ROI property to busy if accepted, set toggle to Off and Idlw either way.
//Set ROI actual 
//Update current values (including struct and indiP) and set to OK when done
inline 
int asiCtrl::setNextROI()
{
   //debug:
//    std::cerr << "setNextROI:\n";
//    std::cerr << "  m_nextROI.x = " << m_nextROI.x << "\n";
//    std::cerr << "  m_nextROI.y = " << m_nextROI.y << "\n";
//    std::cerr << "  m_nextROI.w = " << m_nextROI.w << "\n";
//    std::cerr << "  m_nextROI.h = " << m_nextROI.h << "\n";
//    std::cerr << "  m_nextROI.bin_x = " << m_nextROI.bin_x << "\n";
//    std::cerr << "  m_nextROI.bin_y = " << m_nextROI.bin_y << "\n";
   
   m_reconfig = true;

   updateSwitchIfChanged(m_indiP_roi_set, "request", pcf::IndiElement::Off, INDI_IDLE);
   updateSwitchIfChanged(m_indiP_roi_full, "request", pcf::IndiElement::Off, INDI_IDLE);
   updateSwitchIfChanged(m_indiP_roi_last, "request", pcf::IndiElement::Off, INDI_IDLE);
   updateSwitchIfChanged(m_indiP_roi_default, "request", pcf::IndiElement::Off, INDI_IDLE);
   return 0;
   
}


inline
int asiCtrl::configureAcquisition()
{
   recordCamera(true);
   ASIStopVideoCapture(m_camNum);
   m_running = false;

   //piint frameSize;
   //piint pixelBitDepth;

   //m_camera_timestamp = 0; // reset tracked timestamp

   std::unique_lock<std::mutex> lock(m_indiMutex);

   ASISetControlValue(m_camNum, ASI_HIGH_SPEED_MODE, 0, ASI_FALSE);
   //ASISetControlValue(m_camNum, ASI_FLIP, ASI_FLIP_VERT, ASI_FALSE);

   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   // Dimensions
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   
   //  error = SetIsolatedCropModeEx(1, m_nextROI.h, m_nextROI.w, m_nextROI.bin_y, m_nextROI.bin_x, x0, y0);
   ASI_ERROR_CODE error;
   error = ASISetROIFormat( m_camNum, m_nextROI.w, m_nextROI.h, m_nextROI.bin_x, ASI_IMG_RAW16);   
   if( error < 0 )
   {
      //std::cerr << PicamEnum2String(PicamEnumeratedType_Error, error) << "\n";
      //log<software_error>({__FILE__, __LINE__, 0, error, PicamEnum2String(PicamEnumeratedType_Error, error)});
      state(stateCodes::ERROR);
      return -1;
   }

   int x0 = m_nextROI.x /  m_nextROI.bin_x - 0.5*(m_nextROI.w);
   int y0 = m_nextROI.y /  m_nextROI.bin_x - 0.5*(m_nextROI.h);
   error = ASISetStartPos(m_camNum, x0, y0);
   if( error < 0 )
   {
      //std::cerr << PicamEnum2String(PicamEnumeratedType_Error, error) << "\n";
      //log<software_error>({__FILE__, __LINE__, 0, error, PicamEnum2String(PicamEnumeratedType_Error, error)});
      state(stateCodes::ERROR);
      return -1;
   }

   // query the camera for the actual ROI
   int piWidth;
   int piHeight;
   int piBin;
   int piStartX;
   int piStartY;
   ASI_IMG_TYPE pImg_type;
   ASIGetROIFormat(m_camNum, &piWidth, &piHeight, &piBin, &pImg_type);
   ASIGetStartPos(m_camNum, &piStartX, &piStartY);

   m_currentROI.x = (piStartX + 0.5*piWidth) * piBin;
   m_currentROI.y = (piStartY + 0.5*piHeight) * piBin;
   m_currentROI.w = piWidth;
   m_currentROI.h = piHeight;
   m_currentROI.bin_x = piBin;
   m_currentROI.bin_y = piBin;

   m_width  = piWidth;//m_currentROI.w; // /  m_nextROI.bin_x;
   m_height = piHeight;//m_currentROI.h; // /  m_nextROI.bin_x;

   if (piBin == 1){
      m_bits = 12;
   } else {
      m_bits = 14;
   }
   m_bfactor = pow(2, 16-m_bits);

   std::cout << m_width << " " << m_height << "\n";
   std::cout << m_nextROI.w << " " << m_nextROI.h << "\n";

   updateIfChanged( m_indiP_roi_x, "current", m_currentROI.x, INDI_OK);
   updateIfChanged( m_indiP_roi_y, "current", m_currentROI.y, INDI_OK);
   updateIfChanged( m_indiP_roi_w, "current", m_currentROI.w, INDI_OK);
   updateIfChanged( m_indiP_roi_h, "current", m_currentROI.h, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_x, "current", m_currentROI.bin_x, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_y, "current", m_currentROI.bin_y, INDI_OK);
   
   //We also update target to the settable values
   /*updateIfChanged( m_indiP_roi_x, "target", m_currentROI.x, INDI_OK);
   updateIfChanged( m_indiP_roi_y, "target", m_currentROI.y, INDI_OK);
   updateIfChanged( m_indiP_roi_w, "target", m_currentROI.w, INDI_OK);
   updateIfChanged( m_indiP_roi_h, "target", m_currentROI.h, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_x, "target", m_currentROI.bin_x, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_y, "target", m_currentROI.bin_y, INDI_OK);*/
   
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   // Exposure Time and Frame Rate
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*

   int rv = ASISetControlValue(m_camNum, ASI_EXPOSURE, m_expTimeSet * 1000, ASI_FALSE);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting exposure time"});
      return -1;
   }

   // maybe report back the actual exposure time the camera went to?
   long expTimeReal;
	ASI_BOOL bAuto;
   ASIGetControlValue(m_camNum, ASI_EXPOSURE, &expTimeReal, &bAuto);

   m_expTime = expTimeReal/1000.0;

   //log<text_log>( "Set exposure time " + mode + " to: " + std::to_string(exptime/1000.0) + " sec");

   m_expTimeSet = m_expTime; //At this point it must be true.
   updateIfChanged(m_indiP_exptime, "current", m_expTime, INDI_IDLE);
   updateIfChanged(m_indiP_exptime, "target", m_expTimeSet, INDI_IDLE);


   // gain
   //rv = ASISetControlValue(m_camNum,ASI_GAIN, m_gain, ASI_FALSE); 
   //if(rv < 0)
   //{
   //   log<software_error>({__FILE__, __LINE__, "Error setting gain"});
   //   return -1;
   //}
   setEMGain();

   rv = ASISetControlValue(m_camNum, ASI_BRIGHTNESS, 1, ASI_FALSE); // hard-coded for now, but should be an INDI property
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting brightness!"});
      return -1;
   }


   // TEMPORARY: HARD CODE HARDWARE BINNING !!!!
   //rv = ASISetControlValue(m_camNum, ASI_HARDWARE_BIN, 2, ASI_FALSE);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting brightness!"});
      return -1;
   }

   //Start continuous acquisition
   //ASIStartVideoCapture(m_camNum);

   // allocate memory for image readout
   m_imgSize = m_nextROI.w*m_nextROI.h*2;
   m_imgBuff = new unsigned char[m_imgSize];

   m_dataType = _DATATYPE_UINT16; 

   recordCamera(true); 

   return 0;
}

inline
float asiCtrl::fps()
{
   return m_fps;
}


inline
int asiCtrl::startAcquisition()
{
   log<text_log>("Starting video capture.");
   ASIStartVideoCapture(m_camNum);
   m_running = true;
   return 0;
}

inline
int asiCtrl::getTemp()
{
   long asiTemp;
   getASIParameter(asiTemp, ASI_TEMPERATURE);
   m_ccdTemp = asiTemp / 10.0;
   return 0;
}

inline
int asiCtrl::setTempSetPt()
{
   int rv = ASISetControlValue(m_camNum, ASI_TARGET_TEMP, m_ccdTempSetpt , ASI_FALSE);
   return 0;
}

inline
int asiCtrl::setTempControl()
{  
   if(m_tempControlStatusSet)
   {
      int rv = ASISetControlValue(m_camNum, ASI_COOLER_ON, ASI_TRUE , ASI_FALSE);
      m_tempControlStatus = true;
      m_tempControlStatusStr = "COOLING";
      recordCamera();
      log<text_log>("enabled temperature control");
      return 0;
   }
   else
   {
      int rv = ASISetControlValue(m_camNum, ASI_COOLER_ON, ASI_FALSE , ASI_FALSE);
      m_tempControlStatus = false;
      m_tempControlStatusStr = "OFF";
      recordCamera();
      log<text_log>("disabled temperature control");
      return 0;
   }
}
inline
int asiCtrl::setReadoutSpeed()
{
   return 0;
}

inline
int asiCtrl::acquireAndCheckValid()
{

   // these need to be set elsewhere
   //long imgSize = width*height*(1 + (Image_type==ASI_IMG_RAW16));
	//unsigned char* imgBuf = new unsigned char[imgSize];

   ASIGetVideoData(m_camNum, m_imgBuff, m_imgSize, 6000000); // really long time-out (100 minutes)

   // timestamp
   clock_gettime(CLOCK_REALTIME, &m_currImageTimestamp);

   return 0;

}

inline
int asiCtrl::loadImageIntoStream(void * dest)
{
   //std::cout << m_width << " " << m_height << "\n";

   pixelT * src = nullptr;
   src = (pixelT *) m_imgBuff;


   //std::cout << src[300] << "\n";

   if( frameGrabber<asiCtrl>::loadImageIntoStreamCopy(dest, src, m_width, m_height,  sizeof(pixelT)) == nullptr) return -1;


   return 0;
}

inline
int asiCtrl::reconfig()
{
   ///\todo clean this up.  Just need to wait on acquisition update the first time probably.
   
   log<text_log>("Stopping video capture.");
   ASIStopVideoCapture(m_camNum);
   m_running = false;

   return 0;
}

inline
int asiCtrl::getAcquisitionState()
{

   if(MagAOXAppT::m_powerState == 0) return 0;

   if(powerState() != 1 || powerStateTarget() != 1) return -1;

   if(m_running) state(stateCodes::OPERATING);
   else state(stateCodes::READY);

   return 0;

}



int asiCtrl::checkRecordTimes()
{
   return telemeter<asiCtrl>::checkRecordTimes(telem_stdcam());
}
   
int asiCtrl::recordTelem(const telem_stdcam *)
{
   return recordCamera(true);
}



}//namespace app
} //namespace MagAOX
#endif
