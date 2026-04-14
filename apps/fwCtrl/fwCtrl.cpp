/** \file fwCtrl.cpp
  * \brief The MagAO-X xxxxx main program source file.
  *
  * \ingroup fwCtrl_files
  */

#include "fwCtrl.hpp"


int main(int argc, char **argv)
{
   MagAOX::app::fwCtrl xapp;

   return xapp.main(argc, argv);

}
