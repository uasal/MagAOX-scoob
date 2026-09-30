/** \file BMCApi.h
 * \brief Test stand-in for the Boston Micromachines SDK header used by bmcCtrl.
 * \author Claude Code
 *
 * Declares only the BMC SDK types and functions that `bmcCtrl.hpp` uses.  The function bodies,
 * with controllable fake state, are defined in `bmcCtrl_test.cpp`.
 *
 * \ingroup bmcCtrl_files
 */

#ifndef bmcCtrl_tests_stubs_BMCApi_h
#define bmcCtrl_tests_stubs_BMCApi_h

#include <cstdint>

/// The maximum number of actuators addressable by the SDK, sizes the map lookup table.
#define MAX_DM_SIZE 4096

/// BMC SDK return codes.
enum BMCRC
{
    NO_ERR      = 0, ///< The call succeeded.
    ERR_UNKNOWN = 1, ///< An unspecified error.
    ERR_NO_HW   = 2  ///< The hardware was not found.
};

/// Stand-in for the BMC DM handle structure.
struct DM
{
    uint32_t ActCount; ///< The number of actuators on the DM.
    int      m_id;     ///< Identifier of the fake DM, 0 when closed.
};

/// Get a human-readable string for a return code.
const char *BMCErrorString( BMCRC err /**< [in] the return code */ );

/// Open the DM with the given serial number.
BMCRC BMCOpen( DM         *dm, /**< [out] the DM handle */
               const char *serialNumber /**< [in] the DM serial number */ );

/// Load the actuator map lookup table.
BMCRC BMCLoadMap( DM         *dm,      /**< [in] the DM handle */
                  const char *mapPath, /**< [in] the map file path, nullptr for the default */
                  uint32_t   *mapLut   /**< [out] the map lookup table, MAX_DM_SIZE entries */
);

/// Send a command array (one value per actuator) to the DM.
BMCRC BMCSetArray( DM             *dm,    /**< [in] the DM handle */
                   const double   *array, /**< [in] the command array */
                   const uint32_t *mapLut /**< [in] the map lookup table, nullptr for none */
);

/// Set all actuators to zero.
BMCRC BMCClearArray( DM *dm /**< [in] the DM handle */ );

/// Close the DM.
BMCRC BMCClose( DM *dm /**< [in] the DM handle */ );

#endif // bmcCtrl_tests_stubs_BMCApi_h
