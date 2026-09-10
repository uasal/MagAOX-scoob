/** \file wccSim.cpp
 * \brief The MagAO-X WCC all-sky sensor array simulator main program source file.
 *
 * \ingroup wccSim_files
 */

#include "wccSim.hpp"

int main( int argc, char **argv )
{
    MagAOX::app::wccSim xapp;

    return xapp.main( argc, argv );
}
