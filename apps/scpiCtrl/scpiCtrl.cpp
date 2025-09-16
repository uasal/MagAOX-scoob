/** \file scpiCtrl.cpp
  * \brief The MagAO-X SCPI controller main program.
  *
  * \author Jared R. Males (jaredmales@gmail.com)
  * 
  * \ingroup scpiCtrl_files
  */

#define MAGAOX_SCPICTRL_HEADER_ONLY_IMPL
#include "scpiCtrl.hpp"

int main(int argc, char ** argv)
{
   MagAOX::app::scpiCtrl pdu;

   return pdu.main(argc, argv);
}