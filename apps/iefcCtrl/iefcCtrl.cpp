/** \file iefcCtrl.cpp
  * \brief MagAO-X IEFC controller main program.
  *
  * \ingroup iefcCtrl_files
  */

#include "iefcCtrl.hpp"

int main(int argc, char **argv)
{
   MagAOX::app::iefcCtrl xapp;
   return xapp.main(argc, argv);
}
