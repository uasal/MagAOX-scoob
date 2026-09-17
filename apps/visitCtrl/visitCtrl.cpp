/** \file visitCtrl.cpp
 * \brief The MagAO-X WCC visit file loader main program source file.
 *
 * \ingroup visitCtrl_files
 */

#include "visitCtrl.hpp"

int main( int argc, char **argv )
{
    MagAOX::app::visitCtrl xapp;

    return xapp.main( argc, argv );
}
