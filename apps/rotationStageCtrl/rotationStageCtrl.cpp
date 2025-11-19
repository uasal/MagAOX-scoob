/** \file rotationStageCtrl.cpp
  * \brief The MagAO-X Rotation Stage Controller
  *
  * \ingroup rotationStageCtrl_files
  */


#include "rotationStageCtrl.hpp"

int main(int argc, char ** argv)
{
   MagAOX::app::rotationStageCtrl rsc;

   return rsc.main(argc, argv);
}

