/** \file asiCtrl.cpp
  * \brief The ASI ZWO camera controller main program.
  *
  * \author Kyle Van Gorkom (adapted from picamCtrl)
  * 
  * \ingroup asiCtrl_files
  */


#include "asiCtrl.hpp"

int main(int argc, char ** argv)
{
   MagAOX::app::asiCtrl asicam;

   return asicam.main(argc, argv);
}