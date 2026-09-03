/** \file llowfscSimCpp.cpp
  * \brief MagAO-X C++ optical-sim host main program.
  */

#include "llowfscSimCpp.hpp"

int main( int argc, char **argv )
{
    MagAOX::app::llowfscSimCpp xapp;
    return xapp.main( argc, argv );
}
