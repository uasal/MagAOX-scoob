/** \file pixelinkCtrl.cpp
  * \brief The MagAO-X Pixelink camera controller main program (adapted from picamCtrl)
  *
  * \author Kyle Van Gorkom
  * 
  * \ingroup pixelinkCtrl_files
  */


#include "pixelinkCtrl.hpp"

int main(int argc, char ** argv)
{
   MagAOX::app::pixelinkCtrl pcam;

   return pcam.main(argc, argv);
}
