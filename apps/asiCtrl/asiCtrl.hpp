/** \file asiCtrl.hpp
  * \brief The MagAO-X ZWO ASI camera controller.
  *
  * \author Jared R. Males (jaredmales@gmail.com)
  *
  * \ingroup asiCtrl_files
  */

#ifndef asiCtrl_hpp
#define asiCtrl_hpp

#include <chrono>
#include <sstream>

#include <ImageStreamIO/ImageStreamIO.h>

#include "ASICamera2.h"

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

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
  * Verbose traces use `log<software_debug>` and `log<text_log>` at `logPrio::LOG_DEBUG`.
  * Those entries are recorded only when `logger.logLevel` is DBG or more verbose
  * (for example `--logLevel=DBG`). SDK rejections, ROI rounding, dropped frames, and
  * video timeouts are logged at warning or error without that flag.
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

   static constexpr bool c_stdCamera_blacklevel = true; ///< app::dev config to tell stdCamera to expose Blacklevel controls 

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

   std::string m_serialNumber; ///< The camera's identifying serial number (supports either 16-hex chars or decimal uint64 string)

   ASI_CAMERA_INFO m_camInfo {}; ///< SDK identity of the opened camera. Valid after a successful connect.

   // - m_camIndex is only for enumeration/logging (0..N-1)
   // - m_camID is the identifier to use for ALL ASI API calls (CameraID from ASI_CAMERA_INFO)
   int m_camID = -1; ///< SDK CameraID for ASI API calls. -1 when the camera is closed.

   int m_camIndex = -1; ///< Enumeration index passed to ASIGetCameraProperty. Not the CameraID.

   bool m_running {false}; ///< True while video capture is started.

   std::string m_camName; ///< Configured camera.cameraName, matched against ASI_CAMERA_INFO::Name.

   ///@}

   int m_bits {14}; ///< Bit depth implied by the current binning (12 at bin 1, otherwise 14).

   pixelT m_bfactor; ///< Scale from sensor bits into the uint16 image stream.

   /// Last ASIGetDroppedFrames count for this capture. The SDK resets it when capture stops.
   int m_lastDroppedFrames {0};

   /// ASIGetVideoData timeouts in a row. Cleared when a frame arrives.
   int m_consecutiveTimeouts {0};

   /// When the last dropped-frame warning was logged.
   std::chrono::steady_clock::time_point m_lastDropLog {};

   /// When ASIGetDroppedFrames was last sampled.
   std::chrono::steady_clock::time_point m_lastDropSample {};

   /// Shortest ASIGetVideoData wait [ms]. Covers USB delivery on short exposures.
   static constexpr int c_minVideoWait_ms = 2000;

   /// Longest ASIGetVideoData wait [ms]. Matches the previous fixed wait so a bad exposure cannot block longer than that.
   static constexpr int c_maxVideoWait_ms = 6000000;
   
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

   long m_imgSize = 0; ///< Byte length of m_imgBuff. RAW16 is width * height * 2.

   unsigned char* m_imgBuff = nullptr; ///< Frame buffer owned by this app. Sized from the ROI the driver accepted.
   //float m_blacklevel;
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

   /// Main FSM loop. Reconnects the camera and refreshes temperature, acquisition state, and telemetry.
   virtual int appLogic();

   /// Implementation of the on-power-off FSM logic
   virtual int onPowerOff();

   /// Implementation of the while-powered-off FSM
   virtual int whilePowerOff();

   /// Do any needed shutdown tasks.  Currently nothing in this app.
   virtual int appShutdown();

protected:

   /// Text name of an ASI_ERROR_CODE.
   static const char * asiErrorString( ASI_ERROR_CODE code /**< [in] SDK status */ );

   /// Text name of an ASI_CONTROL_TYPE this app sets or reads.
   static const char * asiControlName( ASI_CONTROL_TYPE control /**< [in] SDK control */ );

   /// Log an ASI command the driver rejected.
   /** The numeric SDK code is stored in the software_error `code_other` field.
     */
   void logASIRejection( ASI_ERROR_CODE code, /**< [in] SDK status */
                         const std::string & what /**< [in] command that failed */ );

   /// Verbose trace, recorded only at logger log level DBG or DEBUG2.
   void logASIDebug( const std::string & what /**< [in] trace text */ );

   /// Snap the next ROI onto the alignment ASISetROIFormat requires.
   /** Width must be a multiple of 8 and height a multiple of 2. The SDK has one binning value.
     */
   void alignNextROI();

   /// ASIGetVideoData wait for the current exposure, in milliseconds.
   int videoWaitMs();

   /// Sample ASIGetDroppedFrames and warn when USB transfer is falling behind.
   /** Success-path samples are limited to about 1 Hz. Timeouts sample immediately.
     */
   void noteDroppedFrames( const char * context, /**< [in] why this sample was taken */
                           bool force /**< [in] sample even if the 1 Hz interval has not elapsed */ );

   /// Delete the SDK frame buffer.
   void freeImageBuffer();

   /// Read one ASI control. Logs a software_error when the driver rejects the read.
   int getASIParameter( long & value, /**< [out] value returned by the SDK */
                        ASI_CONTROL_TYPE parameter /**< [in] control to read */ );

   /// Write one ASI control. Logs a software_error when the driver rejects the write.
   /** \returns 0 on ASI_SUCCESS
     * \returns -1 when the driver rejects the value
     */
   int setASIParameter( ASI_CONTROL_TYPE parameter, /**< [in] control to write */
                        long value, /**< [in] value to write */
                        bool commit /**< [in] unused; the SDK applies the value immediately */ );

   /// Open the configured camera by name and serial number.
   int connect();

   /// Publish OPERATING while capture is running, otherwise READY.
   int getAcquisitionState();

   /// Read ASI_TEMPERATURE into m_ccdTemp. Keeps the previous value if the read is rejected.
   int getTemp();

   // stdCamera interface:
   
   //This must set the power-on default values of
   /* -- m_ccdTempSetpt
    * -- m_currentROI 
    */
   /// Set the power-on ROI and EM gain defaults.
   int powerOnDefaults();

   /// Enable or disable the cooler from m_tempControlStatusSet.
   int setTempControl();

   /// Write m_ccdTempSetpt to ASI_TARGET_TEMP.
   int setTempSetPt();

   /// No readout-speed control on this camera.
   int setReadoutSpeed();

   //int setVShiftSpeed();
   /// Read ASI_GAIN into m_emGain.
   int getEMGain();

   /// Write m_emGainSet and warn if the driver rounds it.
   int setEMGain();

   /// Write m_expTimeSet, in microseconds, and warn if the driver rounds it.
   int setExpTime();

   /// Read ASI_OFFSET into m_blacklevel.
   int getBlacklevel();

   /// Write m_blacklevelSet and warn if the driver rounds it.
   int setBlacklevel();

   //int capExpTime(piflt& exptime);
   //int setFPS();

   /// Check the next ROI
   /** Checks if the target values are valid and adjusts them to the closest valid values if needed.
     *
     * \returns 0 if successful
     * \returns -1 otherwise
     */
   int checkNextROI();

   /// Request a frame-grabber reconfigure for m_nextROI. Trace is software_debug.
   int setNextROI();

   //Framegrabber interface:

   /// Stop capture, apply the ROI, and allocate the frame buffer from the driver's readback.
   int configureAcquisition();

   /// Frame rate reported to frameGrabber.
   float fps();

   /// Start video capture and reset dropped-frame tracking.
   int startAcquisition();

   /// Wait for one frame. 0 on a frame, 1 on timeout, -1 when capture has stopped.
   int acquireAndCheckValid();

   /// Copy the latest RAW16 frame into the image stream.
   int loadImageIntoStream( void * dest /**< [out] destination frame in the image stream */ );

   /// Stop video capture so the frame grabber can reconfigure.
   int reconfig();


   //INDI:
protected:

   //pcf::IndiProperty m_indiP_blacklevel;

   //pcf::IndiProperty m_indiP_readouttime;

public:
   //INDI_NEWCALLBACK_DECL(asiCtrl, m_indiP_adcquality);
   //INDI_NEWCALLBACK_DECL(asiCtrl, m_indiP_blacklevel);

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
     
   // these all need to be overwritten by actual sensor characteristics 
   m_full_x = 4143.5; 
   m_full_y = 2821.5; 
   m_full_w = 8288; 
   m_full_h = 5644; 

   //powerOnDefaults();
   
   m_maxEMGain = 450;
   m_blacklevel = 0;
   m_blacklevelSet = 0;
   m_maxBlacklevel = 65535;
   m_minBlacklevel = 0;

   return;
}

inline
asiCtrl::~asiCtrl() noexcept
{
   freeImageBuffer();

   return;
}

inline
void asiCtrl::setupConfig()
{
   config.add("camera.cameraName", "", "camera.cameraName", argType::Required, "camera", "cameraName", false, "str", "The identifying name of the camera.");
   config.add("camera.serialNum", "", "camera.serialNum", argType::Required, "camera", "serialNum", false, "str", "Unique serial number of the camera.");
   

   dev::stdCamera<asiCtrl>::setupConfig(config);
   dev::frameGrabber<asiCtrl>::setupConfig(config);
   //dev::dssShutter<asiCtrl>::setupConfig(config);
   dev::telemeter<asiCtrl>::setupConfig(config);
}

inline
void asiCtrl::loadConfig()
{

   config(m_camName, "camera.cameraName");
   config(m_serialNumber, "camera.serialNum");

   dev::stdCamera<asiCtrl>::loadConfig(config);
   dev::frameGrabber<asiCtrl>::loadConfig(config);
   //dev::dssShutter<picamCtrl>::loadConfig(config);
   dev::telemeter<asiCtrl>::loadConfig(config);

}

inline
int asiCtrl::appStartup()
{

   //ASIInitLibrary();
   
   //createROIndiNumber( m_indiP_readouttime, "readout_time", "Readout Time (s)");
   //indi::addNumberElement<float>( m_indiP_readouttime, "value", 0.0, std::numeric_limits<float>::max(), 0.0,  "%0.1f", "readout time");
   //registerIndiPropertyReadOnly( m_indiP_readouttime );


   //createStandardIndiNumber<float>(m_indiP_blacklevel, "blacklevel", 0, 0, 1, "%f");
   //m_indiP_blacklevel["current"] = m_blacklevel;
   //m_indiP_blacklevel["target"] = m_blacklevel;
   //registerIndiPropertyNew(m_indiP_blacklevel, INDI_NEWCALLBACK(m_indiP_blacklevel));

   
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

   if(m_camID >= 0)
   {
      ASIStopVideoCapture(m_camID);
      ASICloseCamera(m_camID);
      m_running = false;
      m_camID = -1;
      m_camIndex = -1;
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

   if(m_camID >= 0)
   {
      ASIStopVideoCapture(m_camID);
      ASICloseCamera(m_camID);
      m_camID = -1;
      m_camIndex = -1;
   }

   dev::frameGrabber<asiCtrl>::appShutdown();
   //dev::dssShutter<asiCtrl>::appShutdown();

   return 0;
}

inline
const char * asiCtrl::asiErrorString( ASI_ERROR_CODE code )
{
   switch( code )
   {
      case ASI_SUCCESS:
         return "ASI_SUCCESS";
      case ASI_ERROR_INVALID_INDEX:
         return "ASI_ERROR_INVALID_INDEX";
      case ASI_ERROR_INVALID_ID:
         return "ASI_ERROR_INVALID_ID";
      case ASI_ERROR_INVALID_CONTROL_TYPE:
         return "ASI_ERROR_INVALID_CONTROL_TYPE";
      case ASI_ERROR_CAMERA_CLOSED:
         return "ASI_ERROR_CAMERA_CLOSED";
      case ASI_ERROR_CAMERA_REMOVED:
         return "ASI_ERROR_CAMERA_REMOVED";
      case ASI_ERROR_INVALID_PATH:
         return "ASI_ERROR_INVALID_PATH";
      case ASI_ERROR_INVALID_FILEFORMAT:
         return "ASI_ERROR_INVALID_FILEFORMAT";
      case ASI_ERROR_INVALID_SIZE:
         return "ASI_ERROR_INVALID_SIZE";
      case ASI_ERROR_INVALID_IMGTYPE:
         return "ASI_ERROR_INVALID_IMGTYPE";
      case ASI_ERROR_OUTOF_BOUNDARY:
         return "ASI_ERROR_OUTOF_BOUNDARY";
      case ASI_ERROR_TIMEOUT:
         return "ASI_ERROR_TIMEOUT";
      case ASI_ERROR_INVALID_SEQUENCE:
         return "ASI_ERROR_INVALID_SEQUENCE";
      case ASI_ERROR_BUFFER_TOO_SMALL:
         return "ASI_ERROR_BUFFER_TOO_SMALL";
      case ASI_ERROR_VIDEO_MODE_ACTIVE:
         return "ASI_ERROR_VIDEO_MODE_ACTIVE";
      case ASI_ERROR_EXPOSURE_IN_PROGRESS:
         return "ASI_ERROR_EXPOSURE_IN_PROGRESS";
      case ASI_ERROR_GENERAL_ERROR:
         return "ASI_ERROR_GENERAL_ERROR";
      case ASI_ERROR_INVALID_MODE:
         return "ASI_ERROR_INVALID_MODE";
      case ASI_ERROR_END:
         return "ASI_ERROR_END";
      default:
         return "ASI_ERROR_UNKNOWN";
   }
}

inline
const char * asiCtrl::asiControlName( ASI_CONTROL_TYPE control )
{
   switch( control )
   {
      case ASI_GAIN:
         return "ASI_GAIN";
      case ASI_EXPOSURE:
         return "ASI_EXPOSURE";
      case ASI_OFFSET:
         return "ASI_OFFSET";
      case ASI_BANDWIDTHOVERLOAD:
         return "ASI_BANDWIDTHOVERLOAD";
      case ASI_TEMPERATURE:
         return "ASI_TEMPERATURE";
      case ASI_HIGH_SPEED_MODE:
         return "ASI_HIGH_SPEED_MODE";
      case ASI_TARGET_TEMP:
         return "ASI_TARGET_TEMP";
      case ASI_COOLER_ON:
         return "ASI_COOLER_ON";
      default:
         return "ASI_CONTROL";
   }
}

inline
void asiCtrl::logASIRejection( ASI_ERROR_CODE code,
                               const std::string & what )
{
   std::ostringstream oss;
   oss << what << " failed: " << asiErrorString( code ) << " (" << static_cast<int>( code ) << ")";
   log<software_error>( { __FILE__, __LINE__, 0, static_cast<int>( code ), oss.str() } );
}

inline
void asiCtrl::logASIDebug( const std::string & what )
{
   log<software_debug>( { __FILE__, __LINE__, what } );
}

inline
void asiCtrl::freeImageBuffer()
{
   if( m_imgBuff )
   {
      delete[] m_imgBuff;
      m_imgBuff = nullptr;
   }

   m_imgSize = 0;
}

inline
void asiCtrl::alignNextROI()
{
   if( m_nextROI.bin_x < 1 )
   {
      log<text_log>( "ASI binning " + std::to_string( m_nextROI.bin_x ) + " is invalid; using 1",
                    logPrio::LOG_WARNING );
      m_nextROI.bin_x = 1;
      m_nextROI.bin_y = 1;
   }

   if( m_nextROI.bin_y != m_nextROI.bin_x )
   {
      log<text_log>( "ASI ROI binning is a single value; using bin_x=" + std::to_string( m_nextROI.bin_x ) +
                        " and ignoring bin_y=" + std::to_string( m_nextROI.bin_y ),
                    logPrio::LOG_WARNING );
      m_nextROI.bin_y = m_nextROI.bin_x;
   }

   int alignedW = m_nextROI.w;
   int alignedH = m_nextROI.h;

   if( alignedW < 8 )
   {
      alignedW = 8;
   }
   if( alignedH < 2 )
   {
      alignedH = 2;
   }

   alignedW -= alignedW % 8;
   alignedH -= alignedH % 2;

   if( alignedW != m_nextROI.w || alignedH != m_nextROI.h )
   {
      log<text_log>( "ASI ROI width must be a multiple of 8 and height a multiple of 2. Requested " +
                        std::to_string( m_nextROI.w ) + "x" + std::to_string( m_nextROI.h ) + ", using " +
                        std::to_string( alignedW ) + "x" + std::to_string( alignedH ),
                    logPrio::LOG_WARNING );
      m_nextROI.w = alignedW;
      m_nextROI.h = alignedH;
   }
}

inline
int asiCtrl::videoWaitMs()
{
   // ASIGetVideoData's wait is milliseconds. The SDK recommends exposure*2 + 500 ms.
   // A literal 6000000 is 100 minutes, so a stalled camera produced no log until then.
   double expSec = m_expTime;
   if( m_expTimeSet > expSec )
   {
      expSec = m_expTimeSet;
   }

   double waitMs = expSec * 2000.0 + 500.0;
   if( waitMs < c_minVideoWait_ms )
   {
      waitMs = c_minVideoWait_ms;
   }
   if( waitMs > c_maxVideoWait_ms )
   {
      waitMs = c_maxVideoWait_ms;
   }

   return static_cast<int>( waitMs );
}

inline
void asiCtrl::noteDroppedFrames( const char * context,
                                 bool force )
{
   auto now = std::chrono::steady_clock::now();
   if( !force && m_lastDropSample.time_since_epoch().count() != 0 &&
       now - m_lastDropSample < std::chrono::seconds( 1 ) )
   {
      return;
   }
   m_lastDropSample = now;

   int dropped = 0;
   ASI_ERROR_CODE rv = ASIGetDroppedFrames( m_camID, &dropped );
   if( rv != ASI_SUCCESS )
   {
      logASIDebug( std::string( "ASIGetDroppedFrames during " ) + context + " failed: " + asiErrorString( rv ) );
      return;
   }

   if( dropped <= m_lastDroppedFrames )
   {
      return;
   }

   if( m_lastDropLog.time_since_epoch().count() != 0 && now - m_lastDropLog < std::chrono::seconds( 5 ) )
   {
      return;
   }

   int delta = dropped - m_lastDroppedFrames;
   m_lastDroppedFrames = dropped;
   m_lastDropLog = now;

   long bandwidth = -1;
   ASI_BOOL bAuto = ASI_FALSE;
   ASIGetControlValue( m_camID, ASI_BANDWIDTHOVERLOAD, &bandwidth, &bAuto );

   std::ostringstream oss;
   oss << "ASI dropped " << delta << " frame(s) (" << dropped << " since capture start) during " << context
       << ". The SDK reports drops when USB transfer or the host cannot keep up."
       << " USB bandwidth setting (ASI_BANDWIDTHOVERLOAD)=" << bandwidth;
   log<text_log>( oss.str(), logPrio::LOG_WARNING );
}

inline
int asiCtrl::getASIParameter( long & value,
                              ASI_CONTROL_TYPE parameter )
{
   ASI_BOOL bAuto;
   ASI_ERROR_CODE rv = ASIGetControlValue( m_camID, parameter, &value, &bAuto );

   if( rv != ASI_SUCCESS )
   {
      if( MagAOXAppT::m_powerState == 0 )
      {
         return -1;
      }

      logASIRejection( rv, std::string( "ASIGetControlValue " ) + asiControlName( parameter ) );
      return -1;
   }

   if( MagAOXAppT::m_powerState == 0 )
   {
      return -1; // Flag error but don't log
   }

   logASIDebug( std::string( "ASIGetControlValue " ) + asiControlName( parameter ) + "=" + std::to_string( value ) );
   return 0;
}

inline
int asiCtrl::setASIParameter( ASI_CONTROL_TYPE parameter,
                              long value,
                              bool commit )
{
   static_cast<void>( commit ); // The SDK applies ASISetControlValue immediately.

   ASI_ERROR_CODE rv = ASISetControlValue( m_camID, parameter, value, ASI_FALSE );
   if( rv != ASI_SUCCESS )
   {
      logASIRejection( rv,
                       std::string( "ASISetControlValue " ) + asiControlName( parameter ) + "=" +
                           std::to_string( value ) );
      return -1;
   }

   logASIDebug( std::string( "ASISetControlValue " ) + asiControlName( parameter ) + "=" + std::to_string( value ) );
   return 0;
}

inline
int asiCtrl::connect()
{
    // Default ROIs
    m_nextROI.x = m_default_x;
    m_nextROI.y = m_default_y;
    m_nextROI.w = m_default_w;
    m_nextROI.h = m_default_h;
    m_nextROI.bin_x = m_default_bin_x;
    m_nextROI.bin_y = m_default_bin_y;

    if( m_imgBuff )
    {
        logASIDebug( "Clearing image buffer" );
        freeImageBuffer();
    }

    if( m_camID >= 0 )
    {
        ASI_ERROR_CODE stopRv = ASIStopVideoCapture( m_camID );
        if( stopRv != ASI_SUCCESS )
        {
            logASIRejection( stopRv, "ASIStopVideoCapture before reconnect" );
        }
        ASI_ERROR_CODE closeRv = ASICloseCamera( m_camID );
        if( closeRv != ASI_SUCCESS )
        {
            logASIRejection( closeRv, "ASICloseCamera before reconnect" );
        }
        m_running = false;
        m_camID = -1;
        m_camIndex = -1;
    }

    log<text_log>( "Looking for connected cameras" );

    int numDevices = ASIGetNumOfConnectedCameras();
    if( powerState() != 1 || powerStateTarget() != 1 ) return 0;

    if( numDevices <= 0 )
    {
        state( stateCodes::NODEVICE );
        if( !stateLogged() )
        {
            log<text_log>( "No ASI Cameras available." );
        }
        return 0;
    }

    log<text_log>( std::to_string( numDevices ) + " ASI cameras found. Searching for " + m_camName + " serial " +
                   m_serialNumber );

    int matchingCamID = -1;
    int matchingIndex = -1;

    // Helper lambdas: SN -> hex string; SN -> decimal string (uint64 interpreted big-endian)
    auto snToHex = [](const ASI_SN &sn)->std::string {
        char serialHex[17];
        for (int j = 0; j < 8; ++j) sprintf(serialHex + j * 2, "%02X", sn.id[j]);
        serialHex[16] = '\0';
        return std::string(serialHex);
    };

    auto snToDec = [](const ASI_SN &sn)->std::string {
        // Interpret the 8 bytes in the same displayed order as hex (big-endian fold)
        // This allows comparing against configs that store serial as a decimal uint64 string.
        unsigned long long v = 0;
        for(int j = 0; j < 8; ++j) {
            v = (v << 8) | static_cast<unsigned long long>(sn.id[j]);
        }
        return std::to_string(v);
    };

    // Determine whether the config looks like hex or decimal (accept either)
    auto looksHex16 = [](const std::string &s)->bool {
        if(s.size() != 16) return false;
        for(char c : s) {
            bool isHex = (c >= '0' && c <= '9') ||
                         (c >= 'a' && c <= 'f') ||
                         (c >= 'A' && c <= 'F');
            if(!isHex) return false;
        }
        return true;
    };

    const bool cfgIsHex = looksHex16(m_serialNumber);

    for( int i = 0; i < numDevices; ++i )
    {
        ASI_CAMERA_INFO camInfo{};
        ASI_ERROR_CODE propRv = ASIGetCameraProperty( &camInfo, i );
        if( propRv != ASI_SUCCESS )
        {
            logASIRejection( propRv, "ASIGetCameraProperty index " + std::to_string( i ) );
            continue;
        }

        const int camID = camInfo.CameraID;

        log<text_log>( "Camera index " + std::to_string( i ) + " name " + camInfo.Name + " CameraID " +
                          std::to_string( camID ),
                      logPrio::LOG_DEBUG );

        // Open only (avoid init during discovery)
        ASI_ERROR_CODE openRv = ASIOpenCamera( camID );
        if( openRv != ASI_SUCCESS )
        {
            logASIRejection( openRv, "ASIOpenCamera index " + std::to_string( i ) );
            continue;
        }

        ASI_SN sn{};
        ASI_ERROR_CODE snrv = ASIGetSerialNumber( camID, &sn );

        if( snrv == ASI_SUCCESS )
        {
            std::string serialHex = snToHex( sn );
            std::string serialDec = snToDec( sn );

            log<text_log>( "Index: " + std::to_string( i ) + " Name: " + std::string( camInfo.Name ) +
                           " SerialHex: " + serialHex + " SerialDec: " + serialDec );

            const bool nameMatch = ( strcmp( camInfo.Name, m_camName.c_str() ) == 0 );

            bool serialMatch = false;
            if( cfgIsHex )
            {
                serialMatch = ( serialHex == m_serialNumber );
            }
            else
            {
                serialMatch = ( serialDec == m_serialNumber );
            }

            if( nameMatch && serialMatch )
            {
                log<text_log>( "Camera name and serial matched at index " + std::to_string( i ) );
                matchingCamID = camID;
                matchingIndex = i;
                m_camInfo = camInfo;
                ASICloseCamera( camID );
                break;
            }
        }
        else
        {
            // If serial cannot be read without init, do NOT init here to avoid stepping on another app.
            log<text_log>( "Serial number not readable without init for index " + std::to_string( i ) + " (" +
                              asiErrorString( snrv ) + "); skipping so discovery stays non-invasive",
                          logPrio::LOG_DEBUG );
        }

        ASICloseCamera( camID );

        if( powerState() != 1 || powerStateTarget() != 1 ) return 0;
    }

    if( matchingCamID >= 0 )
    {
        log<text_log>( "Connecting to matched camera CameraID " + std::to_string( matchingCamID ) + " (index " +
                       std::to_string( matchingIndex ) + ")" );

        m_camID = matchingCamID;
        m_camIndex = matchingIndex;

        ASI_ERROR_CODE openRv = ASIOpenCamera( m_camID );
        if( openRv != ASI_SUCCESS )
        {
            state( stateCodes::ERROR );
            logASIRejection( openRv, "ASIOpenCamera on matched camera" );
            m_camID = -1;
            m_camIndex = -1;
            return -1;
        }

        // Init ONLY the matched camera.
        ASI_ERROR_CODE initRv = ASIInitCamera( m_camID );
        if( initRv != ASI_SUCCESS )
        {
            ASICloseCamera( m_camID );
            state( stateCodes::ERROR );
            logASIRejection( initRv, "ASIInitCamera on matched camera" );
            m_camID = -1;
            m_camIndex = -1;
            return -1;
        }

        logASIDebug( "Opened " + std::string( m_camInfo.Name ) + " max " + std::to_string( m_camInfo.MaxWidth ) + "x" +
                     std::to_string( m_camInfo.MaxHeight ) );
        state( stateCodes::CONNECTED );
    }
    else
    {
        state(stateCodes::NODEVICE);
        if (!stateLogged()) {
            log<text_log>("Camera not found in available devices (matched by name+serial).");
            m_camID = -1;
            m_camIndex = -1;
        }
    }

    return 0;
}

inline
int asiCtrl::powerOnDefaults()
{
   m_currentROI.x = 4143.5;
   m_currentROI.y = 2821.5;
   m_currentROI.w = 2048;
   m_currentROI.h = 2048;
   m_currentROI.bin_x = 2;
   m_currentROI.bin_y = 2;

   m_emGainSet = 0;

   return 0;
}

inline
int asiCtrl::setEMGain()
{
   long requested = static_cast<long>( m_emGainSet );
   if( setASIParameter( ASI_GAIN, requested, true ) < 0 )
   {
      return -1;
   }

   long gainReal = 0;
   if( getASIParameter( gainReal, ASI_GAIN ) < 0 )
   {
      return -1;
   }

   if( gainReal != requested )
   {
      log<text_log>( "ASI rounded gain from " + std::to_string( requested ) + " to " + std::to_string( gainReal ),
                    logPrio::LOG_WARNING );
   }

   m_emGainSet = gainReal;

   log<text_log>( "Set gain to: " + std::to_string( m_emGainSet ) );

   updateIfChanged( m_indiP_emGain, "current", m_emGainSet, INDI_IDLE );

   return 0;
}

inline
int asiCtrl::getEMGain()
{
   long gainReal = 0;
   if( getASIParameter( gainReal, ASI_GAIN ) < 0 )
   {
      return -1;
   }

   log<text_log>( "Got gain of: " + std::to_string( gainReal ) );

   m_emGain = gainReal;

   return 0;
}

inline
int asiCtrl::setExpTime()
{
   long requestedUs = static_cast<long>( m_expTimeSet * 1e6 );
   if( setASIParameter( ASI_EXPOSURE, requestedUs, true ) < 0 )
   {
      return -1;
   }

   sleep( 1 );

   long expTimeReal = 0;
   if( getASIParameter( expTimeReal, ASI_EXPOSURE ) < 0 )
   {
      return -1;
   }

   if( expTimeReal != requestedUs )
   {
      log<text_log>( "ASI rounded exposure from " + std::to_string( requestedUs ) + " us to " +
                        std::to_string( expTimeReal ) + " us",
                    logPrio::LOG_WARNING );
   }

   m_expTime = expTimeReal * 1e-6;

   log<text_log>( "Set exposure time to: " + std::to_string( m_expTime ) + " sec" );

   updateIfChanged( m_indiP_exptime, "current", m_expTime, INDI_IDLE );

   return 0;
}

inline
int asiCtrl::checkNextROI()
{
   alignNextROI();

   updateIfChanged( m_indiP_roi_x, "target", m_nextROI.x, INDI_OK );
   updateIfChanged( m_indiP_roi_y, "target", m_nextROI.y, INDI_OK );
   updateIfChanged( m_indiP_roi_w, "target", m_nextROI.w, INDI_OK );
   updateIfChanged( m_indiP_roi_h, "target", m_nextROI.h, INDI_OK );
   updateIfChanged( m_indiP_roi_bin_x, "target", m_nextROI.bin_x, INDI_OK );
   updateIfChanged( m_indiP_roi_bin_y, "target", m_nextROI.bin_y, INDI_OK );

   return 0;
}

//Set ROI property to busy if accepted, set toggle to Off and Idle either way.
//The frame grabber applies the ROI in configureAcquisition.
inline
int asiCtrl::setNextROI()
{
   std::ostringstream oss;
   oss << "setNextROI x=" << m_nextROI.x << " y=" << m_nextROI.y << " w=" << m_nextROI.w << " h=" << m_nextROI.h
       << " bin=" << m_nextROI.bin_x << "x" << m_nextROI.bin_y;
   logASIDebug( oss.str() );

   m_reconfig = true;

   updateSwitchIfChanged( m_indiP_roi_set, "request", pcf::IndiElement::Off, INDI_IDLE );
   updateSwitchIfChanged( m_indiP_roi_full, "request", pcf::IndiElement::Off, INDI_IDLE );
   updateSwitchIfChanged( m_indiP_roi_last, "request", pcf::IndiElement::Off, INDI_IDLE );
   updateSwitchIfChanged( m_indiP_roi_default, "request", pcf::IndiElement::Off, INDI_IDLE );
   return 0;
}

inline
int asiCtrl::configureAcquisition()
{
   recordCamera( true );

   ASI_ERROR_CODE stopRv = ASIStopVideoCapture( m_camID );
   if( stopRv != ASI_SUCCESS )
   {
      logASIRejection( stopRv, "ASIStopVideoCapture before ROI change" );
   }
   m_running = false;

   std::unique_lock<std::mutex> lock( m_indiMutex );

   alignNextROI();

   if( m_camInfo.SupportedBins[0] != 0 )
   {
      bool binSupported = false;
      for( int k = 0; k < 16 && m_camInfo.SupportedBins[k] != 0; ++k )
      {
         if( m_camInfo.SupportedBins[k] == m_nextROI.bin_x )
         {
            binSupported = true;
         }
      }

      if( !binSupported )
      {
         log<text_log>( "Requested ASI bin " + std::to_string( m_nextROI.bin_x ) +
                           " is not in the camera supported-bin list",
                       logPrio::LOG_WARNING );
      }
   }

   if( m_camInfo.MaxWidth > 0 &&
       static_cast<long>( m_nextROI.w ) * m_nextROI.bin_x > m_camInfo.MaxWidth )
   {
      state( stateCodes::ERROR );
      return log<software_error, -1>( { __FILE__,
                                        __LINE__,
                                        std::string( "ROI width " ) + std::to_string( m_nextROI.w ) + " at bin " +
                                            std::to_string( m_nextROI.bin_x ) + " exceeds sensor width " +
                                            std::to_string( m_camInfo.MaxWidth ) } );
   }

   if( m_camInfo.MaxHeight > 0 &&
       static_cast<long>( m_nextROI.h ) * m_nextROI.bin_y > m_camInfo.MaxHeight )
   {
      state( stateCodes::ERROR );
      return log<software_error, -1>( { __FILE__,
                                        __LINE__,
                                        std::string( "ROI height " ) + std::to_string( m_nextROI.h ) + " at bin " +
                                            std::to_string( m_nextROI.bin_y ) + " exceeds sensor height " +
                                            std::to_string( m_camInfo.MaxHeight ) } );
   }

   // Some bodies reject this control. Log it and still apply the ROI.
   if( setASIParameter( ASI_HIGH_SPEED_MODE, 0, true ) < 0 )
   {
      log<text_log>( "ASI_HIGH_SPEED_MODE was rejected; continuing with the current mode", logPrio::LOG_WARNING );
   }

   std::ostringstream requested;
   requested << "Applying ROI w=" << m_nextROI.w << " h=" << m_nextROI.h << " bin=" << m_nextROI.bin_x
             << " center=" << m_nextROI.x << "," << m_nextROI.y;
   logASIDebug( requested.str() );

   ASI_ERROR_CODE error = ASISetROIFormat( m_camID, m_nextROI.w, m_nextROI.h, m_nextROI.bin_x, ASI_IMG_RAW16 );
   if( error != ASI_SUCCESS )
   {
      logASIRejection( error,
                       "ASISetROIFormat " + std::to_string( m_nextROI.w ) + "x" + std::to_string( m_nextROI.h ) +
                           " bin " + std::to_string( m_nextROI.bin_x ) );
      state( stateCodes::ERROR );
      return -1;
   }

   // Width and height are already binned. The SDK start position is in that same binned frame.
   int x0 = m_nextROI.x / m_nextROI.bin_x - 0.5 * ( m_nextROI.w );
   int y0 = m_nextROI.y / m_nextROI.bin_y - 0.5 * ( m_nextROI.h );
   error = ASISetStartPos( m_camID, x0, y0 );
   if( error != ASI_SUCCESS )
   {
      logASIRejection( error,
                       "ASISetStartPos " + std::to_string( x0 ) + "," + std::to_string( y0 ) + " for ROI " +
                           std::to_string( m_nextROI.w ) + "x" + std::to_string( m_nextROI.h ) );
      state( stateCodes::ERROR );
      return -1;
   }

   int piWidth = 0;
   int piHeight = 0;
   int piBin = 0;
   int piStartX = 0;
   int piStartY = 0;
   ASI_IMG_TYPE pImg_type = ASI_IMG_END;
   error = ASIGetROIFormat( m_camID, &piWidth, &piHeight, &piBin, &pImg_type );
   if( error != ASI_SUCCESS )
   {
      logASIRejection( error, "ASIGetROIFormat" );
      state( stateCodes::ERROR );
      return -1;
   }

   error = ASIGetStartPos( m_camID, &piStartX, &piStartY );
   if( error != ASI_SUCCESS )
   {
      logASIRejection( error, "ASIGetStartPos" );
      state( stateCodes::ERROR );
      return -1;
   }

   if( piWidth != m_nextROI.w || piHeight != m_nextROI.h || piBin != m_nextROI.bin_x )
   {
      log<text_log>( "ASI ROI format differs from the request. Requested " + std::to_string( m_nextROI.w ) + "x" +
                        std::to_string( m_nextROI.h ) + " bin " + std::to_string( m_nextROI.bin_x ) + ", driver has " +
                        std::to_string( piWidth ) + "x" + std::to_string( piHeight ) + " bin " + std::to_string( piBin ),
                    logPrio::LOG_WARNING );
   }

   if( piStartX != x0 || piStartY != y0 )
   {
      log<text_log>( "ASI start position differs from the request. Requested " + std::to_string( x0 ) + "," +
                        std::to_string( y0 ) + ", driver has " + std::to_string( piStartX ) + "," +
                        std::to_string( piStartY ),
                    logPrio::LOG_WARNING );
   }

   if( pImg_type != ASI_IMG_RAW16 )
   {
      log<text_log>( "ASI image type is " + std::to_string( static_cast<int>( pImg_type ) ) + " instead of RAW16",
                    logPrio::LOG_WARNING );
   }

   m_currentROI.x = ( piStartX + 0.5 * piWidth ) * piBin;
   m_currentROI.y = ( piStartY + 0.5 * piHeight ) * piBin;
   m_currentROI.w = piWidth;
   m_currentROI.h = piHeight;
   m_currentROI.bin_x = piBin;
   m_currentROI.bin_y = piBin;

   m_width = piWidth;
   m_height = piHeight;

   if( piBin == 1 )
   {
      m_bits = 12;
   }
   else
   {
      m_bits = 14;
   }
   m_bfactor = pow( 2, 16 - m_bits );

   log<text_log>( "ASI ROI applied " + std::to_string( m_width ) + "x" + std::to_string( m_height ) + " bin " +
                  std::to_string( piBin ) + " start " + std::to_string( piStartX ) + "," + std::to_string( piStartY ) );

   updateIfChanged( m_indiP_roi_x, "current", m_currentROI.x, INDI_OK );
   updateIfChanged( m_indiP_roi_y, "current", m_currentROI.y, INDI_OK );
   updateIfChanged( m_indiP_roi_w, "current", m_currentROI.w, INDI_OK );
   updateIfChanged( m_indiP_roi_h, "current", m_currentROI.h, INDI_OK );
   updateIfChanged( m_indiP_roi_bin_x, "current", m_currentROI.bin_x, INDI_OK );
   updateIfChanged( m_indiP_roi_bin_y, "current", m_currentROI.bin_y, INDI_OK );

   if( piWidth <= 0 || piHeight <= 0 )
   {
      state( stateCodes::ERROR );
      return log<software_error, -1>( { __FILE__, __LINE__, "ASI ROI readback has a non-positive size" } );
   }

   freeImageBuffer();
   m_imgSize = static_cast<long>( piWidth ) * piHeight * 2;
   m_imgBuff = new unsigned char[m_imgSize];

   m_dataType = _DATATYPE_UINT16;

   sleep( 1 );
   if( setExpTime() < 0 || setEMGain() < 0 || setBlacklevel() < 0 )
   {
      state( stateCodes::ERROR );
      return -1;
   }

   recordCamera( true );

   return 0;
}

inline
float asiCtrl::fps()
{
   return m_fps;
}

inline
int asiCtrl::getBlacklevel()
{
   long blReal = 0;
   ASI_BOOL bAuto;
   sleep(1);
   ASI_ERROR_CODE rv = ASIGetControlValue( m_camID, ASI_BRIGHTNESS, &blReal, &bAuto );
   sleep( 1 );

   if( rv != ASI_SUCCESS )
   {
      logASIRejection( rv, "ASIGetControlValue ASI_OFFSET (blacklevel)" );
      return -1;
   }

   m_blacklevel = blReal;
   return 0;
}

inline
int asiCtrl::setBlacklevel()
{

   long blacklevel_to_set = static_cast<long>( m_blacklevelSet );

   if( blacklevel_to_set < m_minBlacklevel )
   {
      blacklevel_to_set = static_cast<long>( m_minBlacklevel );
      log<text_log>( "Blacklevel limited to " + std::to_string( blacklevel_to_set ), logPrio::LOG_WARNING );
   }

   if( blacklevel_to_set > m_maxBlacklevel )
   {
      blacklevel_to_set = static_cast<long>( m_maxBlacklevel );
      log<text_log>( "Blacklevel limited to maxBlacklevel = " + std::to_string( blacklevel_to_set ),
                    logPrio::LOG_WARNING );
   }

   if( setASIParameter( ASI_BRIGHTNESS, blacklevel_to_set, true ) < 0 )
   {
      return -1;
   }

   long blReal = 0;
   ASI_BOOL bAuto = ASI_FALSE;
   ASI_ERROR_CODE readRv = ASIGetControlValue( m_camID, ASI_BRIGHTNESS, &blReal, &bAuto );
   if( readRv == ASI_SUCCESS )
   {
      if( blReal != blacklevel_to_set )
      {
         log<text_log>( "ASI rounded blacklevel from " + std::to_string( blacklevel_to_set ) + " to " +
                           std::to_string( blReal ),
                       logPrio::LOG_WARNING );
      }

      m_blacklevel = blReal;
      m_blacklevelSet = blReal;
      blacklevel_to_set = blReal;
   }

   log<text_log>( "Set Blacklevel to: " + std::to_string( blacklevel_to_set ) );

   return 0;
}

inline
int asiCtrl::startAcquisition()
{
   ASI_ERROR_CODE rv = ASIStartVideoCapture( m_camID );
   if( rv != ASI_SUCCESS )
   {
      m_running = false;
      logASIRejection( rv, "ASIStartVideoCapture" );
      return -1;
   }

   m_running = true;
   m_consecutiveTimeouts = 0;
   m_lastDroppedFrames = 0;
   m_lastDropLog = std::chrono::steady_clock::time_point{};
   m_lastDropSample = std::chrono::steady_clock::time_point{};

   log<text_log>( "Starting video capture. Frame wait is " + std::to_string( videoWaitMs() ) + " ms." );
   return 0;
}

inline
int asiCtrl::getTemp()
{
   long asiTemp = 0;
   if( getASIParameter( asiTemp, ASI_TEMPERATURE ) < 0 )
   {
      // Keep the last temperature. appLogic treats a -1 from here as a lost camera.
      return 0;
   }

   m_ccdTemp = asiTemp / 10.0;
   return 0;
}

inline
int asiCtrl::setTempSetPt()
{
   long requested = static_cast<long>( m_ccdTempSetpt );
   if( setASIParameter( ASI_TARGET_TEMP, requested, true ) < 0 )
   {
      return -1;
   }

   return 0;
}

inline
int asiCtrl::setTempControl()
{
   if( m_tempControlStatusSet )
   {
      if( setASIParameter( ASI_COOLER_ON, ASI_TRUE, true ) < 0 )
      {
         return -1;
      }

      m_tempControlStatus = true;
      m_tempControlStatusStr = "COOLING";
      recordCamera();
      log<text_log>( "enabled temperature control" );
      return 0;
   }
   else
   {
      if( setASIParameter( ASI_COOLER_ON, ASI_FALSE, true ) < 0 )
      {
         return -1;
      }

      m_tempControlStatus = false;
      m_tempControlStatusStr = "OFF";
      recordCamera();
      log<text_log>( "disabled temperature control" );
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
   if( m_imgBuff == nullptr || m_imgSize <= 0 )
   {
      state( stateCodes::ERROR );
      return log<software_error, -1>( { __FILE__, __LINE__, "no ASI image buffer allocated" } );
   }

   const int waitMs = videoWaitMs();
   ASI_ERROR_CODE rv = ASIGetVideoData( m_camID, m_imgBuff, m_imgSize, waitMs );

   if( rv == ASI_SUCCESS )
   {
      if( m_consecutiveTimeouts > 0 )
      {
         log<text_log>( "ASI video capture resumed after " + std::to_string( m_consecutiveTimeouts ) +
                        " consecutive timeout(s)" );
         m_consecutiveTimeouts = 0;
      }

      noteDroppedFrames( "frame readout", false );
      clock_gettime( CLOCK_REALTIME, &m_currImageTimestamp );
      return 0;
   }

   if( rv == ASI_ERROR_TIMEOUT )
   {
      ++m_consecutiveTimeouts;
      noteDroppedFrames( "video timeout", true );

      std::ostringstream oss;
      oss << "ASIGetVideoData timed out after " << waitMs << " ms (exposure " << m_expTime
          << " s). No frame was delivered. Consecutive timeouts: " << m_consecutiveTimeouts
          << ". The SDK returns this when readout does not finish in time (USB transfer too slow, a stalled camera, "
             "or an exposure longer than the wait).";
      log<software_error>( { __FILE__, __LINE__, 0, static_cast<int>( rv ), oss.str() } );

      // >0 tells frameGrabber to keep capturing and skip this frame.
      return 1;
   }

   std::ostringstream oss;
   oss << "ASIGetVideoData failed (" << asiErrorString( rv ) << "). Capture stopped.";
   log<software_error>( { __FILE__, __LINE__, 0, static_cast<int>( rv ), oss.str() } );

   m_running = false;
   if( rv == ASI_ERROR_CAMERA_REMOVED || rv == ASI_ERROR_CAMERA_CLOSED || rv == ASI_ERROR_INVALID_ID )
   {
      state( stateCodes::NOTCONNECTED );
   }
   else
   {
      state( stateCodes::ERROR );
   }

   return -1;
}

inline
int asiCtrl::loadImageIntoStream( void * dest )
{
   if( m_imgBuff == nullptr )
   {
      return log<software_error, -1>( { __FILE__, __LINE__, "ASI image buffer is null" } );
   }

   pixelT * src = reinterpret_cast<pixelT *>( m_imgBuff );

   if( frameGrabber<asiCtrl>::loadImageIntoStreamCopy( dest, src, m_width, m_height, sizeof( pixelT ) ) == nullptr )
   {
      return -1;
   }

   return 0;
}

inline
int asiCtrl::reconfig()
{
   log<text_log>( "Stopping video capture." );
   ASI_ERROR_CODE rv = ASIStopVideoCapture( m_camID );
   if( rv != ASI_SUCCESS )
   {
      logASIRejection( rv, "ASIStopVideoCapture" );
   }
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

inline
int asiCtrl::checkRecordTimes()
{
   return telemeter<asiCtrl>::checkRecordTimes(telem_stdcam());
}
inline
int asiCtrl::recordTelem(const telem_stdcam *)
{
   return recordCamera(true);
}

/*
INDI_NEWCALLBACK_DEFN(asiCtrl, m_indiP_blacklevel)(const pcf::IndiProperty &ipRecv)
{
   if (ipRecv.getName() != m_indiP_blacklevel.getName())
   {
      log<software_error>({__FILE__, __LINE__, "wrong INDI property received."});
      return -1;
   }

   float bl = 0;

   if (ipRecv.find("current"))
   {
      bl = ipRecv["current"].get<float>();
   }

   if (ipRecv.find("target"))
   {
      bl = ipRecv["target"].get<float>();
   }

   // check for min/max values
   
   //ASI_CONTROL_CAPS blCtrl;
   //ASIGetControlCaps(m_camNum, ASI_BRIGHTNESS, &blCtrl);
   //long blmin = blCtrl.MinValue;
   //long blmax = blCtrl.MaxValue;
   //std::cerr << "Got a min/max for blacklevel of: " << blmin << " and " << blmax << "\n";

   std::unique_lock<std::mutex> lock(m_indiMutex);
   m_blacklevel = bl;
   int rv = ASISetControlValue(m_camNum, ASI_BRIGHTNESS, bl, ASI_FALSE);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting black level!"});
      return -1;
   }

   // check that we got the expected black level
   long blReal;
	ASI_BOOL bAuto;
   sleep(1);
   ASIGetControlValue(m_camNum, ASI_GAIN, &blReal, &bAuto); // this is always 0 *sigh*
   sleep(1);

   updateIfChanged(m_indiP_blacklevel, "target", bl);
   //m_blacklevel = blReal;
   //std::cerr << "Got a black level of " << m_blacklevel << "\n";
   updateIfChanged(m_indiP_blacklevel, "current", m_blacklevel);

   return 0;
}
*/



}//namespace app
} //namespace MagAOX
#endif
