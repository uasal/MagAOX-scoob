/** \file psfRefCtrl.cpp
  * \brief MagAO-X PSF reference controller main program.
  *
  * \ingroup psfRefCtrl_files
  */

#include "psfRefCtrl.hpp"

int main( int argc, char **argv )
{
    MagAOX::app::psfRefCtrl xapp;
    return xapp.main( argc, argv );
}
