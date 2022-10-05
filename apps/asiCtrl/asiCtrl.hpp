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

inline

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
   
   static constexpr bool c_stdCamera_readoutSpeed = true; ///< app::dev config to tell stdCamera to expose readout speed controls
   
   static constexpr bool c_stdCamera_vShiftSpeed = false; ///< app:dev config to tell stdCamera to expose vertical shift speed control

   static constexpr bool c_stdCamera_emGain = false; ///< app::dev config to tell stdCamera to expose EM gain controls 

   static constexpr bool c_stdCamera_exptimeCtrl = true; ///< app::dev config to tell stdCamera to expose exposure time controls
   
   static constexpr bool c_stdCamera_fpsCtrl = false; ///< app::dev config to tell stdCamera not to expose FPS controls

   static constexpr bool c_stdCamera_fps = false; ///< app::dev config to tell stdCamera not to expose FPS status
   
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
   char[64] m_camName; // unique identifier???



   ///@}

   int m_depth {0};
   
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
   unsigned char* m_imgBuf;
   std::string m_cameraName;
   std::string m_cameraModel;

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
   int connect();

   int getAcquisitionState();

   int getTemps();

   // stdCamera interface:
   
   //This must set the power-on default values of
   /* -- m_ccdTempSetpt
    * -- m_currentROI 
    */
   int powerOnDefaults();
   
   //int setTempControl();
   //int setTempSetPt();
   //int setReadoutSpeed();
   //int setVShiftSpeed();
   //int setEMGain();
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
   //float fps();
   int startAcquisition();
   int acquireAndCheckValid();
   int loadImageIntoStream(void * dest);
   int reconfig();


   //INDI:
protected:

   pcf::IndiProperty m_indiP_readouttime;

public:
   INDI_NEWCALLBACK_DECL(picamCtrl, m_indiP_adcquality);

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
   m_powerMgtEnabled = false;

   //m_acqBuff.memory_size = 0;
   //m_acqBuff.memory = 0;

   m_default_x = 4143.5; 
   m_default_y = 2821.5; 
   m_default_w = 2048;  
   m_default_h = 2048;  
      
   m_full_x = 4143.5; 
   m_full_y = 2821.5; 
   m_full_w = 8288; 
   m_full_h = 5644; 
   
   //m_maxEMGain = 100;
   
   return;
}

inline
asiCtrl::~asiCtrl() noexcept
{
   if(m_acqBuff) // fix me
   {
      free(m_acqBuff);
   }

   return;
}

inline
void asiCtrl::setupConfig()
{
   config.add("camera.serialNumber", "", "camera.serialNumber", argType::Required, "camera", "serialNumber", false, "int", "The identifying serial number of the camera.");

   dev::stdCamera<asiCtrl>::setupConfig(config);
   dev::frameGrabber<asiCtrl>::setupConfig(config);
   //dev::dssShutter<asiCtrl>::setupConfig(config);
   dev::telemeter<asiCtrl>::setupConfig(config);
}

inline
void asiCtrl::loadConfig()
{

   config(m_serialNumber, "camera.serialNumber");

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

   createROIndiNumber( m_indiP_readouttime, "readout_time", "Readout Time (s)");
   indi::addNumberElement<float>( m_indiP_readouttime, "value", 0.0, std::numeric_limits<float>::max(), 0.0,  "%0.1f", "readout time");
   registerIndiPropertyReadOnly( m_indiP_readouttime );
   
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
   m_maxROIWidth = 4144;
   m_stepROIWidth = 4;
   
   m_minROIHeight = 1;
   m_maxROIHeight = 2822;
   m_stepROIHeight = 1;
   
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


   if( state() == stateCodes::NOTCONNECTED || state() == stateCodes::NODEVICE || state() == stateCodes::ERROR)
   {
      m_reconfig = true; //Trigger a f.g. thread reconfig.

      //Might have gotten here because of a power off.
      if(MagAOXAppT::m_powerState == 0) return 0;

      std::unique_lock<std::mutex> lock(m_indiMutex);
      if(connect() < 0)
      {
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

      if(getTemps() < 0)
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

   if(m_camInfo)
   {
      ASIStopVideoCapture(m_camInfo.CameraID);
      ASICloseCamera(m_camInfo.CameraID);
      m_camInfo = 0;
   }

   asi_UninitializeLibrary();

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

   if(m_camInfo)
   {
      ASIStopVideoCapture(m_camInfo.CameraID);
      ASICloseCamera(m_camInfo.CameraID);
      m_camInfo = 0;
   }

   asi_UninitializeLibrary();

   ///\todo error check these base class fxns.
   dev::frameGrabber<asiCtrl>::appShutdown();
   dev::dssShutter<asiCtrl>::appShutdown();

   return 0;
}

inline
int asiCtrl::getASIParameter( long & value,
                                  ASI_CONTROL_TYPE parameter
                                )
{
   ASI_BOOL bAuto;
   ASIGetControlValue( m_camInfo.CameraID, parameter, &value, &bAuto);

   if(MagAOXAppT::m_powerState == 0) return -1; //Flag error but don't log

   return 0;
}

inline
int asiCtrl::setasiParameter( ASI_CONTROL_TYPE parameter,
                                  long value,
                                  bool commit
                                )
{
   ASISetControlValue( m_camInfo.CameraID, parameter, value, ASI_FALSE);

   if(!commit) return 0; // what is this?

   return 0;
}

inline
int asiCtrl::connect()
{

   if(m_imgBuff)
   {
      std::cerr << "Clearing\n";
      free(m_imgBuff);
      //m_acqBuff.memory = NULL;
      //m_acqBuff.memory_size = 0;
   }

   BREADCRUMB

   if(m_camInfo)
   {
      BREADCRUMB
      ASIStopVideoCapture(m_camInfo.CameraID);
      ASICloseCamera(m_camInfo.CameraID);
      m_camInfo = 0;
   }

   BREADCRUMB

   int numDevices = ASIGetNumOfConnectedCameras();

   BREADCRUMB

   if(numDevices <= 0)
   {

      state(stateCodes::NODEVICE);
      if(!stateLogged())
      {
         log<text_log>("no ASI Cameras available.");
      }
      return 0;
   }

   BREADCRUMB

   for(int i=0; i< numDevices; ++i)
   {
      BREADCRUMB

      ASIGetCameraProperty(&m_camInfo, i);

      if( m_camInfo.Name == m_camName )
      {
         BREADCRUMB
         std::cerr << "Camera was found.  Now connecting.\n";

         ASIOpenCamera(m_camInfo.CameraID);

         return 0;
      }
   }

   state(stateCodes::NODEVICE);
   if(!stateLogged())
   {
      log<text_log>("Camera not found in available ids.");
   }

   return 0;
}

inline
int asiCtrl::setFPS()
{
   return 0;
}

inline 
int asiCtrl::powerOnDefaults()
{
   m_currentROI.x = 4143.5;
   m_currentROI.y = 2821.5;
   m_currentROI.w = 8288;
   m_currentROI.h = 5644;
   m_currentROI.bin_x = 2;
   m_currentROI.bin_y = 2;

   return 0;
}


inline
int asiCtrl::setExpTime()
{

   int rv = ASISetControlValue(m_camInfo.CameraID, ASI_EXPOSURE, m_expTimeSet * 1000, ASI_FALSE);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting exposure time"});
      return -1;
   }

   // maybe report back the actual exposure time the camera went to?
   long expTimeReal;
	ASI_BOOL bAuto;
   ASIGetControlValue(m_camInfo.CameraID, ASI_EXPOSURE, &expTimeReal, &bAuto);

   m_expTime = expTimeReal/1000.0;

   //recordCamera();
   //log<text_log>( "Set exposure time " + mode + " to: " + std::to_string(exptime/1000.0) + " sec");

   updateIfChanged(m_indiP_exptime, "current", m_expTime, INDI_IDLE);
   
   return 0;
}

inline
int asiCtrl::checkNextROI()
{
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
   
   return 0;
   
}


inline
int asiCtrl::configureAcquisition()
{

   //piint frameSize;
   //piint pixelBitDepth;

   m_camera_timestamp = 0; // reset tracked timestamp

   std::unique_lock<std::mutex> lock(m_indiMutex);

   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   // Dimensions
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   
   //  error = SetIsolatedCropModeEx(1, m_nextROI.h, m_nextROI.w, m_nextROI.bin_y, m_nextROI.bin_x, x0, y0);
   ASI_ERROR_CODE error;
   error = ASISetROIFormat( m_camInfo.CameraID, m_nextROI.w, m_nextROI.h, m_nextROI.bin, ASI_IMG_RAW16);   
   if( error < 0 )
   {
      //std::cerr << PicamEnum2String(PicamEnumeratedType_Error, error) << "\n";
      //log<software_error>({__FILE__, __LINE__, 0, error, PicamEnum2String(PicamEnumeratedType_Error, error)});
      state(stateCodes::ERROR);
      return -1;
   }

   int x0 = (m_nextROI.x - 0.5*(m_nextROI.w - 1)) + 1;
   int y0 = (m_nextROI.y - 0.5*(m_nextROI.h - 1)) + 1;
   error = ASISetStartPos(m_camInfo.CameraID, x0, y0);

   if( error < 0 )
   {
      //std::cerr << PicamEnum2String(PicamEnumeratedType_Error, error) << "\n";
      //log<software_error>({__FILE__, __LINE__, 0, error, PicamEnum2String(PicamEnumeratedType_Error, error)});
      state(stateCodes::ERROR);
      return -1;
   }

   m_nextROI.x = m_currentROI.x;
   m_nextROI.y = m_currentROI.y;
   m_nextROI.w = m_currentROI.w;
   m_nextROI.h = m_currentROI.h;
   m_nextROI.bin = m_currentROI.bin;

   updateIfChanged( m_indiP_roi_x, "current", m_currentROI.x, INDI_OK);
   updateIfChanged( m_indiP_roi_y, "current", m_currentROI.y, INDI_OK);
   updateIfChanged( m_indiP_roi_w, "current", m_currentROI.w, INDI_OK);
   updateIfChanged( m_indiP_roi_h, "current", m_currentROI.h, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_x, "current", m_currentROI.bin_x, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_y, "current", m_currentROI.bin_y, INDI_OK);
   
   //We also update target to the settable values
   updateIfChanged( m_indiP_roi_x, "target", m_currentROI.x, INDI_OK);
   updateIfChanged( m_indiP_roi_y, "target", m_currentROI.y, INDI_OK);
   updateIfChanged( m_indiP_roi_w, "target", m_currentROI.w, INDI_OK);
   updateIfChanged( m_indiP_roi_h, "target", m_currentROI.h, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_x, "target", m_currentROI.bin_x, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_y, "target", m_currentROI.bin_y, INDI_OK);
   
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   // Exposure Time and Frame Rate
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
   //=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*

   int rv = ASISetControlValue(m_camInfo.CameraID, ASI_EXPOSURE, m_expTimeSet * 1000, ASI_FALSE);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting exposure time"});
      return -1;
   }

   // maybe report back the actual exposure time the camera went to?
   long expTimeReal;
	ASI_BOOL bAuto;
   ASIGetControlValue(m_camInfo.CameraID, ASI_EXPOSURE, &expTimeReal, &bAuto);

   m_expTime = expTimeReal/1000.0;

   //log<text_log>( "Set exposure time " + mode + " to: " + std::to_string(exptime/1000.0) + " sec");

   m_expTimeSet = m_expTime; //At this point it must be true.
   updateIfChanged(m_indiP_exptime, "current", m_expTime, INDI_IDLE);
   updateIfChanged(m_indiP_exptime, "target", m_expTimeSet, INDI_IDLE);

   // gain
   ASISetControlValue(m_camInfo.CameraID,ASI_GAIN, m_Gain, ASI_FALSE); 

   //Start continuous acquisition

   ASIStartVideoCapture(m_camInfo.CameraID);

   // allocate memory for image readout
   m_imgSize = m_nextROI.w*m_nextROI.h*(1 + (Image_type==ASI_IMG_RAW16));
   m_imgBuf = new unsigned char[imgSize];

inline
int asiCtrl::startAcquisition()
{
   return 0;
}

inline
int asiCtrl::acquireAndCheckValid()
{

   // these need to be set elsewhere
   //long imgSize = width*height*(1 + (Image_type==ASI_IMG_RAW16));
	//unsigned char* imgBuf = new unsigned char[imgSize];

   ASIGetVideoData(m_vamInfo.CameraID, m_imgBuf, m_imgSize, 500);

   // might need to convert m_imgBuf into something else for loadImageInstreamStream

   // timestamp???

   return 0;

}

inline
int asiCtrl::loadImageIntoStream(void * dest)
{
   if( frameGrabber<asiCtrl>::loadImageIntoStreamCopy(dest, m_imgBuf, m_width, m_height, m_typeSize) == nullptr) return -1;

   return 0;
}

inline
int asiCtrl::reconfig()
{
   ///\todo clean this up.  Just need to wait on acquisition update the first time probably.
   
   ASIStopVideoCapture(m_camInfo.CameraID);

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
