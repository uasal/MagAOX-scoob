/** \file kim101Ctrl.cpp
  * \brief The MagAO-X KIM101 Inertial Motor Controller main program source file.
  *
  * \ingroup kim101Ctrl_files
  */

#include <csignal>

#include "kim101Ctrl.hpp"


int main(int argc, char **argv)
{
   // mxlib MagAOXApp uses resurrectee, which writes to a pipe for the parent
   // resurrector. When the binary is run alone (e.g. gdb), that write raises
   // SIGPIPE; ignore it so local debugging works (write fails with EPIPE instead).
   std::signal(SIGPIPE, SIG_IGN);

   MagAOX::app::kim101Ctrl xapp;

   return xapp.main(argc, argv);

}

