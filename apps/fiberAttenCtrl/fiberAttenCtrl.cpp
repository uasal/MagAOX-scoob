/** \file fiberAttenCtrl.cpp
  * \brief The MagAO-X fiber attenuation controller app
  *
  * \ingroup fiberAttenCtrl_files
  */

#include "fiberAttenCtrl.hpp"


int main(int argc, char **argv)
{
   MagAOX::app::fiberAttenCtrl xapp;

   return xapp.main(argc, argv);

}
