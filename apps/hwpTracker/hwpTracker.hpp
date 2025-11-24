/** \file hwpTracker.hpp
  * \brief The MagAO-X HWP rotation tracker header file
  *
  * \ingroup hwpTracker_files
  */

#ifndef hwpTracker_hpp
#define hwpTracker_hpp


#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include <mx/math/gslInterpolation.hpp>
#include <mx/ioutils/readColumns.hpp>

/** \defgroup hwpTracker
  * \brief The MagAO-X application to track pupil rotation with the HWP.
  *
  * <a href="../handbook/operating/software/apps/hwpTracker.html">Application Documentation</a>
  *
  * \ingroup apps
  *
  */

/** \defgroup hwpTracker_files
  * \ingroup hwpTracker
  */

namespace MagAOX
{
namespace app
{

/// The MagAO-X ADC Tracker
/**
  * \ingroup hwpTracker
  */
class hwpTracker : public MagAOXApp<true>
{

   //Give the test harness access.
   friend class hwpTracker_test;

protected:

   /** \name Configurable Parameters
     *@{
     */
   float m_altitude {90}; ///< Current altitude

   float m_parang {0}; ///< Current parallactic angle

   float m_hwpTrackingOffset {90}; ///< HWP tracking offset

   float m_zero {360}; ///< The zero point of the HWP stage.
   
   int m_sign {-1}; ///< The sign to apply to the calculated HWP angle.

   std::string m_devName {"stagehwprot"}; ///< The device name of the HWP stage.  Default is 'stagehwprot'

   std::string m_tcsDevName {"tcsi"}; ///< The device name of the TCS Interface providing 'teldata.altitude'.  Default is 'tcsi'

   float m_updateInterval {10}; ///< The interval at which to update positions, in seconds.  Default is 10 secs.

   bool m_tracking {false}; ///< Is the HWP in ADI synchronization mode?. Default is false.




public:
   /// Default c'tor.
   hwpTracker();

   /// D'tor, declared and defined for noexcept.
   ~hwpTracker() noexcept
   {}

   virtual void setupConfig();

   /// Implementation of loadConfig logic, separated for testing.
   /** This is called by loadConfig().
     */
   int loadConfigImpl( mx::app::appConfigurator & _config /**< [in] an application configuration from which to load values*/);

   virtual void loadConfig();

   /// Startup function
   /**
     *
     */
   virtual int appStartup();

   /// Implementation of the FSM for hwpTracker.
   /**
     * \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
   virtual int appLogic();

   /// Shutdown the app.
   /**
     *
     */
   virtual int appShutdown();


   /** @name INDI
     *
     * @{
     */
protected:

   pcf::IndiProperty m_indiP_tracking;

   pcf::IndiProperty m_indiP_teldata;

   pcf::IndiProperty m_indiP_hwpSetPos;
   
   pcf::IndiProperty m_indiP_hwpTrackingOffset;
   
   pcf::IndiProperty m_indiP_hwpActualPos;

   pcf::IndiProperty m_indiP_hwpStatus;

public:
   INDI_NEWCALLBACK_DECL(hwpTracker, m_indiP_tracking);

   INDI_NEWCALLBACK_DECL(hwpTracker, m_indiP_hwpSetPos);

   INDI_SETCALLBACK_DECL(hwpTracker, m_indiP_teldata);

   ///@}
};

hwpTracker::hwpTracker() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{

   return;
}

void hwpTracker::setupConfig()
{
   config.add("hwp.zero", "", "hwp.zero", argType::Required, "hwp", "zero", false, "float", "The HWP zero position.  Default is 360.");

   config.add("hwp.sign", "", "hwp.sign", argType::Required, "hwp", "sign", false, "int", "The HWP rotation sign. Default is -1.");

   config.add("hwp.devName", "", "hwp.devName", argType::Required, "hwp", "devName", false, "string", "The device name of the HWPstage.  Default is 'stagehwprot'");

   config.add("tcs.devName", "", "tcs.devName", argType::Required, "tcs", "devName", false, "string", "The device name of the TCS Interface providing 'teldata.altitude'.  Default is 'tcsi'");

   config.add("tracking.updateInterval", "", "tracking.updateInterval", argType::Required, "tracking", "updateInterval", false, "float", "The interval at which to update positions, in seconds.  Default is 1 sec.");
}

int hwpTracker::loadConfigImpl( mx::app::appConfigurator & _config )
{
   _config(m_zero, "hwp.zero");
   _config(m_sign, "hwp.sign");
   _config(m_devName, "hwp.devName");
   _config(m_tcsDevName, "tcs.devName");
   _config(m_updateInterval, "tracking.updateInterval");

   return 0;
}

void hwpTracker::loadConfig()
{
   loadConfigImpl(config);
}

int hwpTracker::appStartup()
{


   createStandardIndiToggleSw( m_indiP_tracking, "tracking");
   registerIndiPropertyNew( m_indiP_tracking, INDI_NEWCALLBACK(m_indiP_tracking));


   REG_INDI_SETPROP(m_indiP_teldata, m_tcsDevName, "teldata");

   m_indiP_hwpSetPos = pcf::IndiProperty(pcf::IndiProperty::Number);
   m_indiP_hwpSetPos.setDevice(m_devName);
   m_indiP_hwpSetPos.setName("position");
   m_indiP_hwpSetPos.add(pcf::IndiElement("target"));

   m_indiP_hwpTrackingOffset = pcf::IndiProperty(pcf::IndiProperty::Number);
   m_indiP_hwpStatus = pcf::IndiProperty(pcf::IndiProperty::Number);
   m_indiP_hwpActualPos = pcf::IndiProperty(pcf::IndiProperty::Number);

   state(stateCodes::READY);

   return 0;
}

int hwpTracker::appLogic()
{

   static double lastupdate = 0;

   if(m_tracking && mx::sys::get_curr_time() - lastupdate > m_updateInterval)
   {
      // while on Nasmyth East, the sign convention for this offset angle is -1
      m_hwpTrackingOffset = -0.5 * m_parang + m_altitude;

      float hwpSetPos = m_indiP_hwpSetPos["target"];

      float hwpActualAngle = hwpSetPos + m_hwpTrackingOffset;

      float hwpStagePos = m_zero + m_sign * m_hwpTrackingOffset;
      
      std::cerr << "Sending HWP to: " << hwpStagePos << "\n";
      
      m_indiP_hwpTrackingOffset["current"] = m_hwpTrackingOffset
      sendNewProperty(m_indiP_hwpTrackingOffset);

      m_indiP_hwpActualPos["current"] = hwpActualAngle;
      sendNewProperty(m_indiP_hwpActualPos);

      m_indiP_hwpStatus["current"] = getHwpStatus(hwpSetPos);;
      sendNewProperty(m_indiP_hwpStatus);
      
      lastupdate = mx::sys::get_curr_time();


   }
   else if(!m_tracking) lastupdate = 0;

   

   return 0;
}

int hwpTracker::appShutdown()
{
   return 0;
}


INDI_NEWCALLBACK_DEFN(hwpTracker, m_indiP_hwpSetPos)(const pcf::IndiProperty &ipRecv)
{
   if(ipRecv.getName() != m_indiP_hwpSetPos.getName())
   {
      log<software_error>({__FILE__,__LINE__, "wrong INDI property received."});
      return -1;
   }

   if(!ipRecv.find("target")) return 0;

   // log<text_log>("stopped HWP rotation tracking");

   float hwpSetPos = ipRecv["target"];

   float hwpActualAngle = hwpSetPos + m_hwpTrackingOffset;

   float hwpStagePos = m_zero + m_sign * m_hwpTrackingOffset;
   
   std::cerr << "Sending HWP to: " << hwpStagePos << "\n";

   m_indiP_hwpSetPos["current"] = hwpSetPos

   m_indiP_hwpActualPos["current"] = hwpActualAngle;
   sendNewProperty(m_indiP_hwpActualPos);

   m_indiP_hwpStatus["current"] = getHwpStatus(hwpSetPos);;
   sendNewProperty(m_indiP_hwpStatus);

   return 0;
}



INDI_NEWCALLBACK_DEFN(hwpTracker, m_indiP_tracking)(const pcf::IndiProperty &ipRecv)
{
   if(ipRecv.getName() != m_indiP_tracking.getName())
   {
      log<software_error>({__FILE__,__LINE__, "wrong INDI property received."});
      return -1;
   }

   if(!ipRecv.find("toggle")) return 0;

   if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On)
   {
      updateSwitchIfChanged(m_indiP_tracking, "toggle", pcf::IndiElement::On, INDI_IDLE);

      m_tracking = true;

      log<text_log>("started HWP rotation tracking");
   }
   else
   {
      updateSwitchIfChanged(m_indiP_tracking, "toggle", pcf::IndiElement::Off, INDI_IDLE);

      m_tracking = false;

      m_hwpTrackingOffset = 0;

      log<text_log>("stopped HWP rotation tracking");
   }

   return 0;
}


INDI_SETCALLBACK_DEFN(hwpTracker, m_indiP_teldata)(const pcf::IndiProperty &ipRecv)
{
   if(ipRecv.getName() != m_indiP_teldata.getName())
   {
      log<software_error>({__FILE__,__LINE__,"wrong INDI property received"});

      return -1;
   }

   if(!ipRecv.find("altitude")) return 0;
   
   if(!ipRecv.find("parang")) return 0;

   m_altitude = ipRecv["altitude"].get<float>();

   m_parang = ipRecv["parang"].get<float>();

   return 0;
}

std::string getHwpStatus(float hwp_position)
{
   float tol = 0.5; // degrees
   if (fabs(hwp_position - 0) < tol) {
      std::string status = "Qplus";
   } else if (fabs(hwp_position - 45) < tol) {
      std::string status = "Qminus";
   } else if (fabs(hwp_position - 22.5) < tol) {
      std::string status = "Uplus";
   } else if (fabs(hwp_position - 67.5) < tol) {
      std::string status = "Uminus";
   }

   return status;
}

} //namespace app
} //namespace MagAOX

#endif //hwpTracker_hpp

