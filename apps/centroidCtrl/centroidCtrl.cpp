/** \file centroidCtrl.cpp
 * \brief The MagAO-X centroidCtrl main program source file.
 *
 * \ingroup centroidCtrl_files
 */

#include "centroidCtrl.hpp"


int main( int argc, char **argv )
{
    MagAOX::app::centroidCtrl xapp;

    return xapp.main( argc, argv );
}
