/** \file scpiPowerCtrl.cpp
  * \brief The MagAO-X Tripp Lite Power Distribution Unit controller main program.
  *
  * \author Jared R. Males (jaredmales@gmail.com)
  * 
  * \ingroup scpiPowerCtrl_files
  */


#include "scpiPowerCtrl.hpp"

int main(int argc, char ** argv)
{
   MagAOX::app::scpiPowerCtrl pdu;

   return pdu.main(argc, argv);
}
