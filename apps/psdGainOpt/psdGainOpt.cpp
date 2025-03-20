/** \file psdGainOpt.cpp
  * \brief The MagAO-X PSD-based gain optimizer main program source file.
  *
  * \ingroup template_files
  */

#include "psdGainOpt.hpp"


int main(int argc, char **argv)
{
   MagAOX::app::psdGainOpt xapp;

   return xapp.main(argc, argv);

}
