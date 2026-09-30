/** \file tmcController.hpp
 * \brief Minimal tmcController stub declarations for the kcubeCtrl unit tests.
 * \author Claude Code
 *
 * Shadows `/opt/MagAOX/source/tmcController/tmcController.hpp` (the Thorlabs motion controller library, built on
 * libftdi1) so that `kcubeCtrl.hpp` compiles without the library, libftdi1 or K-Cube hardware.  Only the types and
 * members kcubeCtrl uses are declared.  The member function bodies, and the per-controller fake state they drive,
 * are defined in `kcubeCtrl_test.cpp`.
 *
 * \ingroup kcubeCtrl_files
 */

#ifndef kcubeCtrl_tests_stubs_tmcController_hpp
#define kcubeCtrl_tests_stubs_tmcController_hpp

#include <cstdint>
#include <ostream>
#include <string>

/// Opaque stand-in for the libftdi1 device context.
struct ftdi_context;

/// Stub of the libftdi1 error string lookup.
/**
 * \returns a description of the last libftdi1 error
 */
const char *ftdi_get_error_string( struct ftdi_context *ftdi /**< [in] the libftdi1 context */ );

/// Stand-in for the Thorlabs motion controller (APT protocol over libftdi1) interface.
class tmcController
{
  public:
    /// Channel enable states used with mod_set_chanenablestate().
    enum class EnableState : uint8_t
    {
        enabled  = 0x01, ///< The channel is enabled.
        disabled = 0x02  ///< The channel is disabled.
    };

    /// Piezo output voltage limits used in TPZIOSettings.
    enum class VoltLimit : uint16_t
    {
        V75  = 0x01, ///< 75 V limit.
        V100 = 0x02, ///< 100 V limit.
        V150 = 0x03  ///< 150 V limit.
    };

    /// Hardware information returned by hw_req_info().
    struct HWInfo
    {
        uint32_t serialNumber{ 0 }; ///< The controller serial number.

        std::string modelNumber; ///< The controller model number.

        /// Write the hardware information to a stream.
        void dump( std::ostream &os /**< [out] the stream to write to */ ) const;
    };

    /// K-Cube front panel (MMI) parameters.
    struct KMMIParams
    {
        int16_t DispBrightness{ 0 }; ///< The display brightness.

        /// Write the MMI parameters to a stream.
        void dump( std::ostream &os /**< [out] the stream to write to */ ) const;
    };

    /// Piezo I/O settings.
    struct TPZIOSettings
    {
        VoltLimit VoltageLimit{ VoltLimit::V75 }; ///< The output voltage limit.

        /// Write the I/O settings to a stream.
        void dump( std::ostream &os /**< [out] the stream to write to */ ) const;
    };

    /// Default c'tor.
    tmcController();

    /// D'tor.
    virtual ~tmcController();

    /// Get the USB serial number of the controller.
    /**
     * \returns the serial number
     */
    std::string serial() const;

    /// Set the USB serial number of the controller.
    void serial( const std::string &ser /**< [in] the new serial number */ );

    /// Get the USB vendor id.
    /**
     * \returns the vendor id
     */
    int vendor() const;

    /// Get the USB product id.
    /**
     * \returns the product id
     */
    int product() const;

    /// Find and open the USB device.
    /**
     * \returns 0 on success
     * \returns -3 if the device is not found
     * \returns another negative value on error
     */
    int open( bool quiet /**< [in] whether to suppress error messages */ );

    /// Connect to (configure) the opened device.
    /**
     * \returns 0 on success, < 0 on error
     */
    int connect();

    /// Request the hardware information.
    /**
     * \returns 0 on success, < 0 on error
     */
    int hw_req_info( HWInfo &hwi /**< [out] the hardware information */ );

    /// Stop the automatic status update messages.
    /**
     * \returns 0 on success, < 0 on error
     */
    int hw_stop_updatemsgs();

    /// Flash the front panel to identify the module.
    /**
     * \returns 0 on success, < 0 on error
     */
    int mod_identify();

    /// Set a channel enable state.
    /**
     * \returns 0 on success, < 0 on error
     */
    int mod_set_chanenablestate( uint8_t     chan, /**< [in] the channel identifier */
                                 EnableState es /**< [in] the new enable state */ );

    /// Request the front panel (MMI) parameters.
    /**
     * \returns 0 on success, < 0 on error
     */
    int kpz_req_kcubemmiparams( KMMIParams &par /**< [out] the MMI parameters */ );

    /// Set the front panel (MMI) parameters.
    /**
     * \returns 0 on success, < 0 on error
     */
    int kpz_set_kcubemmiparams( const KMMIParams &par /**< [in] the MMI parameters */ );

    /// Request the piezo I/O settings.
    /**
     * \returns 0 on success, < 0 on error
     */
    int pz_req_tpz_iosettings( TPZIOSettings &tios /**< [out] the I/O settings */ );

    /// Set the piezo I/O settings.
    /**
     * \returns 0 on success, < 0 on error
     */
    int pz_set_tpz_iosettings( const TPZIOSettings &tios /**< [in] the I/O settings */ );

    /// Request the output voltage, as a fraction of the voltage limit.
    /**
     * \returns 0 on success, < 0 on error
     */
    int pz_req_outputvolts( float &ov /**< [out] the output voltage fraction */ );

    /// Set the output voltage, as a fraction of the voltage limit.
    /**
     * \returns 0 on success, < 0 on error
     */
    int pz_set_outputvolts( float ov /**< [in] the output voltage fraction */ );

    /// Report an error from a libftdi1 function.
    virtual void ftdiErrmsg( const std::string &src,  ///< [in] The source of the error (the tmcController function)
                             const std::string &msg,  ///< [in] The message describing the error
                             int                rv,   ///< [in] The return value of the libftdi1 function
                             const std::string &file, ///< [in] The file name of this file
                             int                line  ///< [in] The line number at which the error was recorded
    );

    /// Report an error not from libftdi1.
    virtual void otherErrmsg( const std::string &src,  ///< [in] The source of the error (the tmcController function)
                              const std::string &msg,  ///< [in] The message describing the error
                              const std::string &file, ///< [in] The file name of this file
                              int                line  ///< [in] The line number at which the error was recorded
    );

  protected:
    struct ftdi_context *m_ftdi{ nullptr }; ///< The libftdi1 context (never allocated by the stub).

    std::string m_serial; ///< The USB serial number.
};

#endif // kcubeCtrl_tests_stubs_tmcController_hpp
