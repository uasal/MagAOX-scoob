/** \file telescopeSim.cpp
 * \brief The MagAO-X WCC telescope simulator main program source file.
 *
 * \ingroup telescopeSim_files
 */

#include "telescopeSim.hpp"

int main( int argc, char **argv )
{
    MagAOX::app::telescopeSim xapp;

    return xapp.main( argc, argv );
}
