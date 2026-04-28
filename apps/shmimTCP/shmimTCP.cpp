/** \file shmimTCP.cpp
 * \brief The MagAO-X shmimTCP main program.
 *
 * \ingroup shmimTCP_files
 */

#include "shmimTCP.hpp"

int main( int argc, char **argv )
{
    MagAOX::app::shmimTCP xapp;

    return xapp.main( argc, argv );
}

