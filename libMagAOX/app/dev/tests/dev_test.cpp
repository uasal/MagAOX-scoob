/** \file template_test.cpp
  * \brief Catch2 tests for the template app.
  *
  * History:
  */
#include "../../../../tests/catch2/catch.hpp"

#include "../dm.hpp"
#include "../dmPokeWFS.hpp"
#include "../dssShutter.hpp"
#include "../edtCamera.hpp"
#include "../frameGrabber.hpp"
#include "../ioDevice.hpp"
#include "../semUtilsDerived.hpp"
#include "../shmimMonitor.hpp"
#include "../stdCamera.hpp"
#include "../stdMotionStage.hpp"
#include "../summerDevice.hpp"
#include "../telemeter.hpp"
#include "../summerDeviceUtils/binaryUart.hpp"
#include "../summerDeviceUtils/cGraphPacket.hpp"
#include "../summerDeviceUtils/commands.hpp"
#include "../summerDeviceUtils/iPacket.hpp"
#include "../summerDeviceUtils/IUart.hpp"
#include "../summerDeviceUtils/linux_pinout_client_socket.hpp"
#include "../summerDeviceUtils/linux_pinout_uart.hpp"
#include "../summerDeviceUtils/socket.hpp"


using namespace MagAOX::app;

namespace template_test 
{

SCENARIO( "xxxx", "[template]" )
{
   GIVEN("xxxxx")
   {
      int rv;

      WHEN("xxxx")
      {
         rv = [];

         REQUIRE(rv == 0);
      }
   }
}
} //namespace template_test 
