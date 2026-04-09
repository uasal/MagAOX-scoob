/** \file kim101Ctrl.cpp
  * \brief The MagAO-X KIM101 Inertial Motor Controller main program source file.
  *
  * \ingroup kim101Ctrl_files
  */

#include "kim101Ctrl.hpp"


int main(int argc, char **argv)
{
   MagAOX::app::kim101Ctrl xapp;

   return xapp.main(argc, argv);

}

