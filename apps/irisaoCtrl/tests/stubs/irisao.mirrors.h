/** \file irisao.mirrors.h
 * \brief Test stand-in for the IrisAO mirror SDK header used by irisaoCtrl.
 * \author Claude Code
 *
 * Declares only the IrisAO SDK types and functions that `irisaoCtrl.hpp` uses.  The function bodies,
 * with controllable fake state, are defined in `irisaoCtrl_test.cpp`.
 *
 * \ingroup irisaoCtrl_files
 */

#ifndef irisaoCtrl_tests_stubs_irisao_mirrors_h
#define irisaoCtrl_tests_stubs_irisao_mirrors_h

/// Stand-in for the opaque IrisAO mirror object.
struct irisaoMirrorStub
{
    int m_id; ///< Identifier of the fake mirror, unused by the app.
};

/// Handle to a connected IrisAO mirror.
typedef irisaoMirrorStub *MirrorHandle;

/// Index of a mirror segment.
typedef unsigned int SegmentNumber;

/// Position (piston/tip/tilt) state of a segment.
struct MirrorPosition
{
    float z;         ///< Piston.
    float xgrad;     ///< X gradient (tip).
    float ygrad;     ///< Y gradient (tilt).
    bool  reachable; ///< Whether the commanded position is reachable.
};

/// Commands that can be sent to the mirror.
enum MirrorCommandType
{
    MirrorSendSettings = 0 ///< Send the currently set segment positions to the driver.
};

/// Connect to the mirror with the given mirror and driver serial numbers.
MirrorHandle MirrorConnect( const char *mirrorSerial, /**< [in] the mirror serial number */
                            const char *driverSerial, /**< [in] the driver serial number */
                            bool        disableHW     /**< [in] if true, no commands are sent to hardware */
);

/// Check whether a segment number is valid for the mirror.
bool MirrorIterate( MirrorHandle  mirror, /**< [in] the mirror handle */
                    SegmentNumber segment /**< [in] the segment number */
);

/// Set the commanded position of a segment.
void SetMirrorPosition( MirrorHandle  mirror,  /**< [in] the mirror handle */
                        SegmentNumber segment, /**< [in] the segment number */
                        float         z,       /**< [in] piston */
                        float         xgrad,   /**< [in] x gradient */
                        float         ygrad    /**< [in] y gradient */
);

/// Get the position state of a segment.
void GetMirrorPosition( MirrorHandle    mirror,  /**< [in] the mirror handle */
                        SegmentNumber   segment, /**< [in] the segment number */
                        MirrorPosition *position /**< [out] the segment position state */
);

/// Send a command to the mirror.
void MirrorCommand( MirrorHandle      mirror, /**< [in] the mirror handle */
                    MirrorCommandType command /**< [in] the command */
);

/// Release the mirror connection.
void MirrorRelease( MirrorHandle mirror /**< [in] the mirror handle */ );

#endif // irisaoCtrl_tests_stubs_irisao_mirrors_h
