/** \file darkCtrl.cpp
  * \brief MagAO-X dark-library builder main program.
  *
  * \ingroup darkCtrl_files
  */

#include "darkCtrl.hpp"

int main( int argc, char **argv )
{
    MagAOX::app::darkCtrl xapp;
    return xapp.main( argc, argv );
}
