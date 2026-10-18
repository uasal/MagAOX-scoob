/** \file fsmCtrl.cpp
  * \brief The MagAO-X xxxxx main program source file.
  *
  * \ingroup fsmCtrl_files
  */

#ifndef XWC_SIM_MODE
#include "fsmCtrl.hpp"
#else
#include "fsmCtrl_simulator.hpp"
#endif

int main(int argc, char **argv)
{
   MagAOX::app::fsmCtrl xapp;

   return xapp.main(argc, argv);

}
