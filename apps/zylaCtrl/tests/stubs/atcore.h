/** \file atcore.h
 * \brief Test stub for the Andor SDK3 core header, declaring only what zylaCtrl uses.
 * \author Claude Code
 *
 * The types and constants match the real SDK3 `atcore.h` (`AT_H` and `AT_BOOL` are `int`, `AT_64` is
 * `long long`, `AT_U8` is `unsigned char`, and `AT_WC` is `wchar_t`).  The function bodies, and the fake
 * camera state they act on, are defined in `zylaCtrl_test.cpp`.
 *
 * \ingroup zylaCtrl_files
 */

#ifndef zylaCtrl_tests_stubs_atcore_h
#define zylaCtrl_tests_stubs_atcore_h

/// Boolean true for AT_BOOL values.
#define AT_TRUE 1

/// Boolean false for AT_BOOL values.
#define AT_FALSE 0

/// Return code for success.
#define AT_SUCCESS 0

/// Return code when the library is not initialized.
#define AT_ERR_NOTINITIALISED 1

/// Return code when a feature is not implemented.
#define AT_ERR_NOTIMPLEMENTED 2

/// Return code when a feature is read-only.
#define AT_ERR_READONLY 3

/// Return code when a value is out of range.
#define AT_ERR_OUTOFRANGE 6

/// Return code when a wait timed out.
#define AT_ERR_TIMEDOUT 13

/// Return code for an invalid handle.
#define AT_ERR_INVALIDHANDLE 12

/// Handle value for an unopened camera.
#define AT_HANDLE_UNINITIALISED -1

/// Handle used to query system-wide features, such as "Device Count".
#define AT_HANDLE_SYSTEM 1

/// Andor SDK3 handle type.
typedef int AT_H;

/// Andor SDK3 boolean type.
typedef int AT_BOOL;

/// Andor SDK3 64-bit integer type.
typedef long long AT_64;

/// Andor SDK3 byte type, used for image buffers.
typedef unsigned char AT_U8;

/// Andor SDK3 wide character type, used for feature names and strings.
typedef wchar_t AT_WC;

#ifdef __cplusplus
extern "C"
{
#endif

    /// Initialize the SDK3 library.
    int AT_InitialiseLibrary();

    /// Finalize the SDK3 library.
    int AT_FinaliseLibrary();

    /// Open a camera by index.
    int AT_Open( int   CameraIndex, /**< [in] the camera index */
                 AT_H *Hndl         /**< [out] the handle of the opened camera */
    );

    /// Close a camera.
    int AT_Close( AT_H Hndl /**< [in] the camera handle */ );

    /// Set an integer feature.
    int AT_SetInt( AT_H         Hndl,    /**< [in] the camera handle */
                   const AT_WC *Feature, /**< [in] the feature name */
                   AT_64        Value    /**< [in] the new value */
    );

    /// Get an integer feature.
    int AT_GetInt( AT_H         Hndl,    /**< [in] the camera handle */
                   const AT_WC *Feature, /**< [in] the feature name */
                   AT_64       *Value    /**< [out] the value */
    );

    /// Set a floating point feature.
    int AT_SetFloat( AT_H         Hndl,    /**< [in] the camera handle */
                     const AT_WC *Feature, /**< [in] the feature name */
                     double       Value    /**< [in] the new value */
    );

    /// Get a floating point feature.
    int AT_GetFloat( AT_H         Hndl,    /**< [in] the camera handle */
                     const AT_WC *Feature, /**< [in] the feature name */
                     double      *Value    /**< [out] the value */
    );

    /// Set a boolean feature.
    int AT_SetBool( AT_H         Hndl,    /**< [in] the camera handle */
                    const AT_WC *Feature, /**< [in] the feature name */
                    AT_BOOL      Value    /**< [in] the new value */
    );

    /// Get a boolean feature.
    int AT_GetBool( AT_H         Hndl,    /**< [in] the camera handle */
                    const AT_WC *Feature, /**< [in] the feature name */
                    AT_BOOL     *Value    /**< [out] the value */
    );

    /// Execute a command feature.
    int AT_Command( AT_H         Hndl,   /**< [in] the camera handle */
                    const AT_WC *Feature /**< [in] the command name */
    );

    /// Set an enumerated feature by its string value.
    int AT_SetEnumString( AT_H         Hndl,    /**< [in] the camera handle */
                          const AT_WC *Feature, /**< [in] the feature name */
                          const AT_WC *String   /**< [in] the new value */
    );

    /// Get the current index of an enumerated feature.
    int AT_GetEnumIndex( AT_H         Hndl,    /**< [in] the camera handle */
                         const AT_WC *Feature, /**< [in] the feature name */
                         int         *Value    /**< [out] the index */
    );

    /// Get the string value of an enumerated feature at an index.
    int AT_GetEnumStringByIndex( AT_H         Hndl,        /**< [in] the camera handle */
                                 const AT_WC *Feature,     /**< [in] the feature name */
                                 int          Index,       /**< [in] the index */
                                 AT_WC       *String,      /**< [out] the string value */
                                 int          StringLength /**< [in] the length of String */
    );

    /// Get a string feature.
    int AT_GetString( AT_H         Hndl,        /**< [in] the camera handle */
                      const AT_WC *Feature,     /**< [in] the feature name */
                      AT_WC       *String,      /**< [out] the value */
                      int          StringLength /**< [in] the length of String */
    );

    /// Queue an image buffer for acquisition.
    int AT_QueueBuffer( AT_H   Hndl,   /**< [in] the camera handle */
                        AT_U8 *Ptr,    /**< [in] the buffer */
                        int    PtrSize /**< [in] the buffer size in bytes */
    );

    /// Wait for the next filled image buffer.
    int AT_WaitBuffer( AT_H         Hndl,    /**< [in] the camera handle */
                       AT_U8      **Ptr,     /**< [out] the filled buffer */
                       int         *PtrSize, /**< [out] the size of the filled buffer */
                       unsigned int Timeout  /**< [in] the timeout in msec */
    );

    /// Flush the queued image buffers.
    int AT_Flush( AT_H Hndl /**< [in] the camera handle */ );

#ifdef __cplusplus
}
#endif

#endif // zylaCtrl_tests_stubs_atcore_h
