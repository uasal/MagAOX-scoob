/** \file pixelinkCtrl.hpp
  * \brief The MagAO-X Pixelink camera controller main program (adapted from picamCtrl)
  *
  * \author Kyle Van Gorkom
  *
  * \ingroup pixelinkCtrl_files
  */

#ifndef pixelinkCtrl_hpp
#define pixelinkCtrl_hpp

#define PIXELINK_LINUX
//#define MAX_IMAGE_SIZE (5000*5000*2)

//#include <ImageStruct.h>
#include <endian.h>
#include <ImageStreamIO/ImageStreamIO.h>

#include <PixeLINKApi.h>
//#include <PixeLINKCodes.h>
//#include <PixeLINKTypes.h>

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


/** \defgroup pixelinkCtrl camera
  * \brief Control of a Pixelink Camera.
  *
  * <a href="../handbook/operating/software/apps/pixelinkCtrl.html">Application Documentation</a>
  *
  * \ingroup apps
  *
  */

/** \defgroup pixelinkCtrl_files Camera Files
  * \ingroup pixelinkCtrl
  */

/** MagAO-X application to control a Pixelink camera
  *
  * \ingroup pixelinkCtrl
  *
  * \todo Config item for ImageStreamIO name filename
  * \todo implement ImageStreamIO circular buffer, with config setting
  */
class pixelinkCtrl : public MagAOXApp<>, public dev::stdCamera<pixelinkCtrl>, public dev::frameGrabber<pixelinkCtrl>, public dev::telemeter<pixelinkCtrl>
{

   friend class dev::stdCamera<pixelinkCtrl>;
   friend class dev::frameGrabber<pixelinkCtrl>;
   //friend class dev::dssShutter<pixelinkCtrl>;
   friend class dev::telemeter<pixelinkCtrl>;

   typedef MagAOXApp<> MagAOXAppT;

public:
   /** \name app::dev Configurations
     *@{
     */
   static constexpr bool c_stdCamera_tempControl = false; ///< app::dev config to tell stdCamera to expose temperature controls
   
   static constexpr bool c_stdCamera_temp = false; ///< app::dev config to tell stdCamera to expose temperature
   
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
   std::string m_serialNumber; ///< The camera's identifying serial number


   ///@}

   int m_depth {16};
   
   HANDLE m_cameraHandle {0};
   bool m_running;
   float m_bodyTemp;
   float m_sensorTemp;

   //AcquisitionBuffer m_acqBuff;
   FRAME_DESC m_frameDesc;
   //std::vector<U8> m_frameBuffer;
   U16* m_frameBuffer = nullptr;

   std::string m_cameraName;
   std::string m_cameraModel;

public:

   ///Default c'tor
   pixelinkCtrl();

   ///Destructor
   ~pixelinkCtrl() noexcept;

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

   int setTempControl();

   
   int setReadoutSpeed();
   int setEMGain();
   int setExpTime();
   //int capExpTime(piflt& exptime);
   int setFPS();

   /// Check the next ROI
   /** Checks if the target values are valid and adjusts them to the closest valid values if needed.
     *
     * \returns 0 if successful
     * \returns -1 otherwise
     */
   int checkNextROI();

   int setNextROI();
   
   //Framegrabber interface:
   int configureAcquisition();
   float fps();
   int startAcquisition();
   int acquireAndCheckValid();
   int loadImageIntoStream(void * dest);
   int reconfig();


   //INDI:
protected:

   pcf::IndiProperty m_indiP_readouttime;

public:
   INDI_NEWCALLBACK_DECL(pixelinkCtrl, m_indiP_adcquality);

   /** \name Telemeter Interface
     * 
     * @{
     */ 
   int checkRecordTimes();
   
   int recordTelem( const telem_stdcam * );
   
   
   ///@}
};

inline
pixelinkCtrl::pixelinkCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
   m_powerMgtEnabled = false;
   
   m_default_x = 512; 
   m_default_y = 512; 
   m_default_w = 512;  
   m_default_h = 512;  
   m_default_bin_x = 1;
   m_default_bin_y = 1;
   
   m_full_w = 1936; // fix me!!!
   m_full_h = 1464; // fix me!!!
   m_full_x = 0.5*((float) m_full_w-1.0);
   m_full_y = 0.5*((float) m_full_h-1.0);

   
   m_maxEMGain = 100;

   m_expTimeSet = 1e-3; // default value?
   
   return;
}

inline
pixelinkCtrl::~pixelinkCtrl() noexcept
{
   // FIX ME
   //if(m_acqBuff.memory)
   //{
   //   free(m_acqBuff.memory);
   //}

   return;
}

inline
void pixelinkCtrl::setupConfig()
{
   //config.add("camera.serialNumber", "", "camera.serialNumber", argType::Required, "camera", "serialNumber", false, "int", "The identifying serial number of the camera.");

   dev::stdCamera<pixelinkCtrl>::setupConfig(config);
   dev::frameGrabber<pixelinkCtrl>::setupConfig(config);
   //dev::dssShutter<pixelinkCtrl>::setupConfig(config);
   dev::telemeter<pixelinkCtrl>::setupConfig(config);
}

inline
void pixelinkCtrl::loadConfig()
{

   //config(m_serialNumber, "camera.serialNumber");

   dev::stdCamera<pixelinkCtrl>::loadConfig(config);
   dev::frameGrabber<pixelinkCtrl>::loadConfig(config);
   //dev::dssShutter<pixelinkCtrl>::loadConfig(config);
   dev::telemeter<pixelinkCtrl>::loadConfig(config);
   

}

inline
int pixelinkCtrl::appStartup()
{


   powerOnDefaults();

   // DELETE ME
   //m_outfile = fopen("/home/xsup/test2.txt", "w");
   
   //createROIndiNumber( m_indiP_readouttime, "readout_time", "Readout Time (s)");
   //indi::addNumberElement<float>( m_indiP_readouttime, "value", 0.0, std::numeric_limits<float>::max(), 0.0,  "%0.1f", "readout time");
   //registerIndiPropertyReadOnly( m_indiP_readouttime );

   m_minROIx = 0;
   m_maxROIx = 1023;
   m_stepROIx = 0;
   
   m_minROIy = 0;
   m_maxROIy = 1023;
   m_stepROIy = 0;
   
   m_minROIWidth = 1;
   m_maxROIWidth = 1024;
   m_stepROIWidth = 4;
   
   m_minROIHeight = 1;
   m_maxROIHeight = 1024;
   m_stepROIHeight = 1;
   
   m_minROIBinning_x = 1;
   m_maxROIBinning_x = 32;
   m_stepROIBinning_x = 1;
   
   m_minROIBinning_y = 1;
   m_maxROIBinning_y = 1024;
   m_stepROIBinning_y = 1;
   
   if(dev::stdCamera<pixelinkCtrl>::appStartup() < 0)
   {
      return log<software_critical,-1>({__FILE__,__LINE__});
   }
   
   if(dev::frameGrabber<pixelinkCtrl>::appStartup() < 0)
   {
      return log<software_critical,-1>({__FILE__,__LINE__});
   }

   /*if(dev::dssShutter<pixelinkCtrl>::appStartup() < 0)
   {
      return log<software_critical,-1>({__FILE__,__LINE__});
   }*/

   if(dev::telemeter<pixelinkCtrl>::appStartup() < 0)
   {
      return log<software_error,-1>({__FILE__,__LINE__});
   }


   state(stateCodes::NOTCONNECTED);
   
   return 0;
}

inline
int pixelinkCtrl::appLogic()
{

   //and run stdCamera's appLogic
   if(dev::stdCamera<pixelinkCtrl>::appLogic() < 0)
   {
      return log<software_error, -1>({__FILE__, __LINE__});
   }
   
   //first run frameGrabber's appLogic to see if the f.g. thread has exited.
   if(dev::frameGrabber<pixelinkCtrl>::appLogic() < 0)
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
         //if(powerState() != 1 || powerStateTarget() != 1) return 0;
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
         //if(powerState() != 1 || powerStateTarget() != 1) return 0;
         return log<software_error,0>({__FILE__,__LINE__});
      }

      if(frameGrabber<pixelinkCtrl>::updateINDI() < 0)
      {
         return log<software_error,0>({__FILE__,__LINE__});
      }      
      
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

      if(stdCamera<pixelinkCtrl>::updateINDI() < 0)
      {
         return log<software_error,0>({__FILE__,__LINE__});
      }
      
      if(frameGrabber<pixelinkCtrl>::updateINDI() < 0)
      {
         return log<software_error,0>({__FILE__,__LINE__});
      }

      if(telemeter<pixelinkCtrl>::appLogic() < 0)
      {
         log<software_error>({__FILE__, __LINE__});
         return 0;
      }

   }

   //Fall through check?
   return 0;

}

inline
int pixelinkCtrl::onPowerOff()
{
   std::lock_guard<std::mutex> lock(m_indiMutex);

   if(m_cameraHandle)
   {
      PxLSetStreamState(m_cameraHandle, STOP_STREAM);
	   //ASSERT(API_SUCCESS(rc));
	   PxLUninitialize(m_cameraHandle);
	  // ASSERT(API_SUCCESS(rc));
   }

   if(stdCamera<pixelinkCtrl>::onPowerOff() < 0)
   {
      log<software_error>({__FILE__, __LINE__});
   }
   
   return 0;
}

inline
int pixelinkCtrl::whilePowerOff()
{
   /*if(dssShutter<pixelinkCtrl>::whilePowerOff() < 0)
   {
      log<software_error>({__FILE__, __LINE__});
   }*/

   if(stdCamera<pixelinkCtrl>::onPowerOff() < 0 )
   {
      log<software_error>({__FILE__, __LINE__});
   }
   
   return 0;
}

inline
int pixelinkCtrl::appShutdown()
{
   dev::frameGrabber<pixelinkCtrl>::appShutdown();

   log<software_error>({__FILE__, __LINE__, "beepboop"});

   if(m_cameraHandle)
   {
      log<software_error>({__FILE__, __LINE__, "SDHLFKSD"});
      PxLSetStreamState(m_cameraHandle, STOP_STREAM);
	   //ASSERT(API_SUCCESS(rc));
	   PxLUninitialize(m_cameraHandle);
	  // ASSERT(API_SUCCESS(rc));
   }

   ///\todo error check these base class fxns.
   dev::frameGrabber<pixelinkCtrl>::appShutdown();
   //dev::dssShutter<pixelinkCtrl>::appShutdown();

   return 0;
}

inline
int pixelinkCtrl::connect()
{


   // FIX ME!!!!!
   /*if(m_frameBuffer != nullptr){
      free(m_frameBuffer);
   }*/


   PXL_RETURN_CODE rc;
   if(m_cameraHandle)
   {
      PxLUninitialize(m_cameraHandle);
      m_cameraHandle = 0;
   }

   //if(powerState() != 1 || powerStateTarget() != 1) return 0;

   // for now, just connect to first camera found
   rc = PxLInitializeEx(0, &m_cameraHandle, 0);
   if (!API_SUCCESS(rc)) {
		log<software_error>({__FILE__,__LINE__, "Error connecting to camera or no camera found."});
      state(stateCodes::NODEVICE);
		return 0;
	}

   state(stateCodes::CONNECTED);


   return 0;
}


inline
int pixelinkCtrl::getAcquisitionState()
{


   if(MagAOXAppT::m_powerState == 0) return 0;

   if(m_running) state(stateCodes::OPERATING);
   else state(stateCodes::READY);

   return 0;

}

inline
int pixelinkCtrl::getTemps()
{

   U32 flags = 0;
	U32 numParams = 1;

   float sensorTemperature;
   if(PxLGetFeature(m_cameraHandle, FEATURE_SENSOR_TEMPERATURE, &flags, &numParams, &sensorTemperature) < -1)
   {
      log<software_error>({__FILE__, __LINE__});
      state(stateCodes::ERROR);
      return -1;
   }
   m_sensorTemp = sensorTemperature;

   float bodyTemperature;
   if(PxLGetFeature(m_cameraHandle, FEATURE_BODY_TEMPERATURE, &flags, &numParams, &bodyTemperature) < -1)
   {
      log<software_error>({__FILE__, __LINE__});
      state(stateCodes::ERROR);
      return -1;
   }
   m_bodyTemp = bodyTemperature;

   recordCamera();

   return 0;
}

inline 
int pixelinkCtrl::powerOnDefaults()
{

   m_nextROI.x = m_default_x;
   m_nextROI.y = m_default_y;
   m_nextROI.w = m_default_w;
   m_nextROI.h = m_default_h;
   m_nextROI.bin_x = m_default_bin_x;
   m_nextROI.bin_y = m_default_bin_y;

   m_currentROI.x = m_default_x;
   m_currentROI.y = m_default_y;
   m_currentROI.w = m_default_w;
   m_currentROI.h = m_default_h;
   m_currentROI.bin_x = m_default_bin_x;
   m_currentROI.bin_y = m_default_bin_y;

   //m_readoutSpeedName = "emccd_05MHz";
  // m_vShiftSpeedName = "1_2us";
   return 0;
}

inline
int pixelinkCtrl::setEMGain()
{
   // Not EM gain, but I'm re-using this function for CMOS gain

   U32 flags = 0;
	U32 numParams = 1;

   if(PxLSetFeature(m_cameraHandle, FEATURE_GAIN, FEATURE_FLAG_MANUAL, numParams, &m_emGainSet) < 0){
      log<software_error>({__FILE__, __LINE__, "Error setting gain"});
      return -1;
   }

   // check what gain actually went to
	numParams = 1;
   float gain;
   if(PxLGetFeature(m_cameraHandle, FEATURE_GAIN, &flags, &numParams, &gain) < 0)
   {
      log<software_error>({__FILE__, __LINE__});
      state(stateCodes::ERROR);
      return -1;
   }
   m_emGain = gain;

   updateIfChanged(m_indiP_emGain, "current", m_emGain, INDI_IDLE);

   recordCamera(true); // what does this do?
   return 0;
}

inline
int pixelinkCtrl::setExpTime()
{

   // send exposure time in seconds
   U32 flags = 0;
   U32 numParams = 1;
   if(PxLSetFeature(m_cameraHandle, FEATURE_EXPOSURE, FEATURE_FLAG_MANUAL, numParams, &m_expTimeSet) < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting exposure time"});
      state(stateCodes::ERROR);
      return -1;
   }

   // get actual exposure time camera went to
   float expTimeReal;
   if(PxLGetFeature(m_cameraHandle, FEATURE_SHUTTER, &flags, &numParams, &expTimeReal) < 0)
   {
      log<software_error>({__FILE__, __LINE__});
      state(stateCodes::ERROR);
      return -1;
   }

   m_expTime = expTimeReal;

   //recordCamera();
   //log<text_log>( "Set exposure time to: " + std::to_string(m_expTime) + " sec");

   updateIfChanged(m_indiP_exptime, "current", m_expTime, INDI_IDLE);

   return 0;
}

inline
int pixelinkCtrl::checkNextROI()
{
   // FIX ME

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
int pixelinkCtrl::setNextROI()
{   
   // FIX ME
   m_reconfig = true;

   updateSwitchIfChanged(m_indiP_roi_set, "request", pcf::IndiElement::Off, INDI_IDLE);
   updateSwitchIfChanged(m_indiP_roi_full, "request", pcf::IndiElement::Off, INDI_IDLE);
   updateSwitchIfChanged(m_indiP_roi_last, "request", pcf::IndiElement::Off, INDI_IDLE);
   updateSwitchIfChanged(m_indiP_roi_default, "request", pcf::IndiElement::Off, INDI_IDLE);
   return 0;
   
}

inline
int pixelinkCtrl::configureAcquisition()
{

   // not sure how to check if already stopped, so we force it to start so we can stop it
   PxLSetStreamState(m_cameraHandle, START_STREAM);

   recordCamera(true);
   if(PXL_RETURN_CODE rc = PxLSetStreamState(m_cameraHandle, STOP_STREAM) < 0){
      log<software_error>({__FILE__, __LINE__, rc, "Error stopping continuous acquisition"});
      state(stateCodes::ERROR);
      return -1;
   }
   m_running = false;

   //m_camera_timestamp = 0; // reset tracked timestamp

   std::unique_lock<std::mutex> lock(m_indiMutex);

   // special camera mode
   F32 paramMode = FEATURE_SPECIAL_CAMERA_MODE_FIXED_FRAME_RATE;
   if(PxLSetFeature(m_cameraHandle, FEATURE_SPECIAL_CAMERA_MODE, FEATURE_FLAG_MANUAL, 1, &paramMode) < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting special camera mode"});
      state(stateCodes::ERROR);
      return -1;
   }

   // FIX ME: exposure time
   setExpTime();

   // FIX ME: gain
   setEMGain();

   // set bit depth
   F32 paramsDepth = PIXEL_FORMAT_MONO16;
   PXL_RETURN_CODE rc = PxLSetFeature(m_cameraHandle, FEATURE_PIXEL_FORMAT, FEATURE_FLAG_MANUAL, 1, &paramsDepth);
   if(rc < 0)
   {
      log<software_error>({__FILE__, __LINE__, rc, "Error setting bit depth"});
      state(stateCodes::ERROR);
      return -1;
   }

   // FIX ME: ROI
   U32 numParamsROI = FEATURE_ROI_NUM_PARAMS;
   F32 paramsROI[FEATURE_ROI_NUM_PARAMS] = {};

   paramsROI[FEATURE_ROI_PARAM_LEFT] =  m_nextROI.x;//m_nextROI.bin_x*(m_nextROI.x - 0.5*m_nextROI.w);
   paramsROI[FEATURE_ROI_PARAM_TOP] = m_nextROI.y;// m_nextROI.bin_y*(m_nextROI.y - 0.5*m_nextROI.h);
   paramsROI[FEATURE_ROI_PARAM_WIDTH] = m_nextROI.w;//m_nextROI.bin_x*m_nextROI.w;
   paramsROI[FEATURE_ROI_PARAM_HEIGHT] =  m_nextROI.h;//m_nextROI.bin_y*m_nextROI.h;

   std::cout<<paramsROI[0]<<","<<paramsROI[1]<<","<<paramsROI[2]<<","<<paramsROI[3]<<"\n";
   if(rc = PxLSetFeature(m_cameraHandle, FEATURE_ROI, FEATURE_FLAG_MANUAL, numParamsROI, paramsROI) < 0)
   {
      log<software_error>({__FILE__, __LINE__, rc, "Error setting ROI"});
      state(stateCodes::ERROR);
      return -1;
   }

   // FIX ME: binning
   U32 numParamsBin = FEATURE_PIXEL_ADDRESSING_NUM_PARAMS;
   F32 paramsBin[FEATURE_PIXEL_ADDRESSING_NUM_PARAMS] = {};
   //paramsBin = (int *)malloc(sizeof(int)*numParamsBin);

   paramsBin[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE] = m_nextROI.bin_x;
   paramsBin[FEATURE_PIXEL_ADDRESSING_PARAM_MODE] = PIXEL_ADDRESSING_MODE_BIN;
   paramsBin[FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE] = m_nextROI.bin_x;
   paramsBin[FEATURE_PIXEL_ADDRESSING_PARAM_Y_VALUE] = m_nextROI.bin_y;
   if(PxLSetFeature(m_cameraHandle, FEATURE_PIXEL_ADDRESSING, FEATURE_FLAG_MANUAL, numParamsBin, paramsBin) < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting binning"});
      state(stateCodes::ERROR);
      return -1;
   }

   // get the actual ROI the camera went to 
   U32 flags = 0;
   if(PxLGetFeature(m_cameraHandle, FEATURE_ROI, &flags, &numParamsROI, &paramsROI[0]) < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error querying ROI"});
      state(stateCodes::ERROR);
      return -1;
   }

   // get the actual binning the camera went to
   if(PxLGetFeature(m_cameraHandle, FEATURE_PIXEL_ADDRESSING, &flags, &numParamsROI, &paramsBin[0]) < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error querying binning"});
      state(stateCodes::ERROR);
      return -1;
   }

   // Update ROI/bin values with the real values from the camera
   m_currentROI.x = paramsROI[FEATURE_ROI_PARAM_LEFT];///paramsBin[FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE] + 0.5*paramsROI[FEATURE_ROI_PARAM_WIDTH]; //(piStartX + 0.5*piWidth) * piBin;
   m_currentROI.y = paramsROI[FEATURE_ROI_PARAM_TOP];///paramsBin[FEATURE_PIXEL_ADDRESSING_PARAM_Y_VALUE] + 0.5*paramsROI[FEATURE_ROI_PARAM_HEIGHT];
   m_currentROI.w = paramsROI[FEATURE_ROI_PARAM_WIDTH]; /// paramsBin[FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE];
   m_currentROI.h = paramsROI[FEATURE_ROI_PARAM_HEIGHT]; /// paramsBin[FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE];
   m_currentROI.bin_x = paramsBin[FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE];
   m_currentROI.bin_y = paramsBin[FEATURE_PIXEL_ADDRESSING_PARAM_Y_VALUE];

   updateIfChanged( m_indiP_roi_x, "current", m_currentROI.x, INDI_OK);
   updateIfChanged( m_indiP_roi_y, "current", m_currentROI.y, INDI_OK);
   updateIfChanged( m_indiP_roi_w, "current", m_currentROI.w, INDI_OK);
   updateIfChanged( m_indiP_roi_h, "current", m_currentROI.h, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_x, "current", m_currentROI.bin_x, INDI_OK);
   updateIfChanged( m_indiP_roi_bin_y, "current", m_currentROI.bin_y, INDI_OK);

   // allocate frame buffer
   m_width = m_currentROI.w / m_currentROI.bin_x;
   m_height =  m_currentROI.h / m_currentROI.bin_y;
   m_frameBuffer = (U16*)malloc(m_width*m_height*2);

   //Start continuous acquisition
   recordCamera();
   if(PxLSetStreamState (m_cameraHandle, START_STREAM) < 0){
      log<software_error>({__FILE__, __LINE__, "Error starting continuous acquisition"});
      state(stateCodes::ERROR);
      return -1;
   }
   m_dataType = _DATATYPE_UINT16; //Where does this go?

   return 0;

}

inline
float pixelinkCtrl::fps()
{
   return m_fps;
}

inline
int pixelinkCtrl::startAcquisition()
{
   if(PxLSetStreamState (m_cameraHandle, START_STREAM) < 0){
      log<software_error>({__FILE__, __LINE__, "Error starting continuous acquisition"});
      state(stateCodes::ERROR);
      return -1;
   }

   state(stateCodes::OPERATING);
   m_running = true;
   return 0;
}

inline
int pixelinkCtrl::setReadoutSpeed()
{
   return 0;
}

inline
int pixelinkCtrl::acquireAndCheckValid()
{

   m_frameDesc.uSize = sizeof(m_frameDesc);
   //if(PxLGetNextFrame(m_cameraHandle, (U32)m_frameBuffer.size(), &m_frameBuffer[0], &m_frameDesc) < 0)
   PXL_RETURN_CODE rc = PxLGetNextFrame(m_cameraHandle, 512*512*2, (LPVOID*)m_frameBuffer, &m_frameDesc);
	if(rc < 0)
   {
      log<software_error>({__FILE__, __LINE__, rc, "Error getting next frame"});
      return -1;
   }

   // timestamp
   clock_gettime(CLOCK_REALTIME, &m_currImageTimestamp);


   return 0;
}

inline
int pixelinkCtrl::loadImageIntoStream(void * dest)
{
   // FIX ME
   
   pixelT * src = nullptr;
   src = (pixelT *) m_frameBuffer;

   // frames are big endian straight from the camera, so flip the bits
   for(int i=0; i<(m_width*m_height); i++){
      src[i] = __builtin_bswap16(src[i]);
   }

   if( frameGrabber<pixelinkCtrl>::loadImageIntoStreamCopy(dest, src, m_width, m_height, m_typeSize) == nullptr) return -1;
   return 0;
}

inline
int pixelinkCtrl::reconfig()
{
   // FIX ME: stop streaming, then????

   if(PxLSetStreamState(m_cameraHandle, STOP_STREAM) < 0)
   {
      log<software_error>({__FILE__, __LINE__, 0, "Error stopping continuous acquisition"});
      state(stateCodes::ERROR);

      return -1;
   }

   m_running = false;
   return 0;
}

int pixelinkCtrl::checkRecordTimes()
{
   return telemeter<pixelinkCtrl>::checkRecordTimes(telem_stdcam());
}
   
int pixelinkCtrl::recordTelem(const telem_stdcam *)
{
   return recordCamera(true);
}


}//namespace app
} //namespace MagAOX
#endif
