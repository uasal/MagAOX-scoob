/** \file asdkWrapper.h
 * \brief Test stand-in for the ALPAO SDK C/C++ wrapper header used by alpaoCtrl.
 * \author Claude Code
 *
 * Declares only the ALPAO SDK types and functions that `alpaoCtrl.hpp` uses.  The function bodies,
 * with controllable fake state, are defined in `alpaoCtrl_test.cpp`.
 *
 * \ingroup alpaoCtrl_files
 */

#ifndef alpaoCtrl_tests_stubs_asdkWrapper_h
#define alpaoCtrl_tests_stubs_asdkWrapper_h

#include <cstddef>
#include <cstdint>

/// Stand-in for the ALPAO SDK namespace.
namespace acs
{

/// The ALPAO floating point type used for DM commands.
typedef double Scalar;

/// The ALPAO unsigned integer type.
typedef uint32_t UInt;

/// The ALPAO character type.
typedef char Char;

/// The ALPAO constant C-string type.
typedef const Char *CStrConst;

/// The ALPAO completion status returned by most SDK calls.
enum COMPL_STAT
{
    SUCCESS = 0, ///< The call succeeded.
    FAILURE = -1 ///< The call failed.
};

} // namespace acs

using acs::COMPL_STAT;
using acs::Scalar;
using acs::UInt;

/// Stand-in for the opaque ALPAO DM handle.
struct asdkDM
{
    int m_handle; ///< Identifier of the fake DM, unused by the app.
};

/// Initialize the DM with the given serial number, returning a handle (or nullptr on failure).
asdkDM *asdkInit( acs::CStrConst serialName /**< [in] the DM serial number */ );

/// Release the DM handle.
COMPL_STAT asdkRelease( asdkDM *pDm /**< [in] the DM handle */ );

/// Send a command vector (one value per actuator) to the DM.
COMPL_STAT asdkSend( asdkDM       *pDm,  /**< [in] the DM handle */
                     const Scalar *value /**< [in] the command vector */
);

/// Reset the DM.
COMPL_STAT asdkReset( asdkDM *pDm /**< [in] the DM handle */ );

/// Get a DM parameter.
COMPL_STAT asdkGet( asdkDM        *pDm,     /**< [in] the DM handle */
                    acs::CStrConst command, /**< [in] the parameter name */
                    Scalar        *value    /**< [out] the parameter value */
);

/// Get the last SDK error code and, if a buffer is given, its message.
COMPL_STAT asdkGetLastError( UInt      *errorNo, /**< [out] the last error code, 0 for no error */
                             acs::Char *errMsg,  /**< [out] buffer for the error message, may be nullptr */
                             size_t     len      /**< [in] length of errMsg */
);

#endif // alpaoCtrl_tests_stubs_asdkWrapper_h
