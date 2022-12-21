/** \file aguc8Ctrl.cpp
  * \brief aguc8Ctrl source file, adapted from smc100ccCtrl.
  * \author Kyle Van Gorkom
  *
  * \ingroup aguc8Ctrl_files
  *
  */
#include "aguc8Ctrl.hpp"

int main(int argc, char ** argv)
{
   MagAOX::app::aguc8Ctrl xapp;

   return xapp.main(argc, argv);
}