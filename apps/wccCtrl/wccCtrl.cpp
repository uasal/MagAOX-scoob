/** \file wccCtrl.cpp
 * \brief The MagAO-X WCC acquisition controller main program source file.
 *
 * \ingroup wccCtrl_files
 */

#include "wccCtrl.hpp"

int main( int argc, char **argv )
{
    MagAOX::app::wccCtrl xapp;

    return xapp.main( argc, argv );
}
