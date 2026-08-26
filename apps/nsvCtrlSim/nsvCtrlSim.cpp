/** \file nsvCtrlSim.cpp
  * \brief MagAO-X NSV camera simulator main program.
  *
  * \ingroup nsvCtrlSim_files
  */

#include "nsvCtrlSim.hpp"

int main( int argc, char **argv )
{
    MagAOX::app::nsvCtrlSim xapp;
    return xapp.main( argc, argv );
}
