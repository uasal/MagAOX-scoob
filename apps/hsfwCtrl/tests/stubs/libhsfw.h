/** \file libhsfw.h
 * \brief Test stub for the Optec HSFW library header, declaring only what hsfwCtrl uses.
 * \author Claude Code
 *
 * The function bodies, and the fake filter wheel state they act on, are defined in `hsfwCtrl_test.cpp`.
 *
 * \ingroup hsfwCtrl_files
 */

#ifndef hsfwCtrl_tests_stubs_libhsfw_h
#define hsfwCtrl_tests_stubs_libhsfw_h

#include <wchar.h>

/// Handle to an open filter wheel.
/** Opaque in the real library.  The stub gives it an identifier so tests can tell handles apart.
 */
typedef struct hsfw_wheel
{
    int id; ///< Identifier of the fake wheel
} hsfw_wheel;

/// One entry in the linked list of attached wheels returned by enumerate_wheels().
typedef struct hsfw_wheel_info
{
    unsigned short vendor_id; ///< USB vendor ID

    unsigned short product_id; ///< USB product ID

    wchar_t *serial_number; ///< The wheel serial number

    struct hsfw_wheel_info *next; ///< The next wheel in the list, or NULL at the end
} hsfw_wheel_info;

/// Status of a wheel, filled in by get_hsfw_status().
typedef struct wheel_status
{
    unsigned short position; ///< The current filter position, starting from 1

    int is_homed; ///< Non-zero if the wheel has been homed

    int is_homing; ///< Non-zero while the wheel is homing

    int is_moving; ///< Non-zero while the wheel is moving

    int error_state; ///< Non-zero if the wheel reports an error
} wheel_status;

/// Enumerate the attached wheels.
/**
 * \returns the head of a linked list of wheels, to be released with wheels_free_enumeration()
 * \returns NULL if no wheels are attached
 */
hsfw_wheel_info *enumerate_wheels();

/// Release a list returned by enumerate_wheels().
void wheels_free_enumeration( hsfw_wheel_info *devs /**< [in] the list to release */ );

/// Open a wheel.
/**
 * \returns a handle to the open wheel
 * \returns NULL on error
 */
hsfw_wheel *open_hsfw( unsigned short vendor_id,  /**< [in] the USB vendor ID */
                       unsigned short product_id, /**< [in] the USB product ID */
                       const wchar_t *serial_number /**< [in] the wheel serial number */ );

/// Close a wheel.
/**
 * \returns 0 on success
 */
int close_hsfw( hsfw_wheel *wheel /**< [in] the wheel to close */ );

/// Get the status of a wheel.
/**
 * \returns 0 on success
 * \returns < 0 on error
 */
int get_hsfw_status( hsfw_wheel   *wheel, /**< [in] the wheel to query */
                     wheel_status *status /**< [out] the wheel status */ );

/// Clear the error state of a wheel.
/**
 * \returns 0 on success
 */
int clear_error_hsfw( hsfw_wheel *wheel /**< [in] the wheel to clear */ );

/// Start homing a wheel.
/**
 * \returns 0 on success
 * \returns non-zero on error
 */
int home_hsfw( hsfw_wheel *wheel /**< [in] the wheel to home */ );

/// Move a wheel to a filter position.
/**
 * \returns 0 on success
 * \returns < 0 on error
 */
int move_hsfw( hsfw_wheel    *wheel, /**< [in] the wheel to move */
               unsigned short position /**< [in] the target filter position, starting from 1 */ );

/// Shut down the library.
/**
 * \returns 0 on success
 */
int exit_hsfw();

#endif // hsfwCtrl_tests_stubs_libhsfw_h
