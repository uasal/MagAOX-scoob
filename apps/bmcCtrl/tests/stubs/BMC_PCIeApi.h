/** \file BMC_PCIeApi.h
 * \brief Test stand-in for the Boston Micromachines PCIe SDK header used by bmcCtrl.
 * \author Claude Code
 *
 * Declares only the PCIe function that `bmcCtrl.hpp` uses.  The function body is defined in
 * `bmcCtrl_test.cpp`.
 *
 * \ingroup bmcCtrl_files
 */

#ifndef bmcCtrl_tests_stubs_BMC_PCIeApi_h
#define bmcCtrl_tests_stubs_BMC_PCIeApi_h

#include "BMCApi.h"

/// Enable or disable the high resolution (pseudo 16-bit dithering) mode.
BMCRC BMC_PCIeEnableHighRes( DM *dm,    /**< [in] the DM handle */
                             int enable /**< [in] 1 to enable, 0 to disable */
);

#endif // bmcCtrl_tests_stubs_BMC_PCIeApi_h
