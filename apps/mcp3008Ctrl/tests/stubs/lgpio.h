/** \file lgpio.h
 * \brief Minimal lgpio SPI stub declarations for the mcp3008Ctrl unit tests.
 * \author Claude Code
 *
 * Shadows the system `<lgpio.h>` so that `dependencies/MCP3008.cpp` can be compiled into the test
 * translation unit without the lgpio library or SPI hardware.  The function bodies, and the fake state
 * they drive, are defined in `mcp3008Ctrl_test.cpp`.
 *
 * \ingroup mcp3008Ctrl_files
 */

#ifndef mcp3008Ctrl_tests_stubs_lgpio_h
#define mcp3008Ctrl_tests_stubs_lgpio_h

/// Stub of the lgpio SPI open call.
/**
 * \returns a non-negative handle on success
 * \returns a negative error code on failure
 */
int lgSpiOpen( int spiDev /**< [in] the SPI device number */,
               int spiChan /**< [in] the SPI chip-select channel */,
               int spiBaud /**< [in] the SPI baud rate */,
               int spiFlags /**< [in] the SPI mode flags */ );

/// Stub of the lgpio SPI close call.
/**
 * \returns 0 on success
 * \returns a negative error code on failure
 */
int lgSpiClose( int handle /**< [in] the handle returned by lgSpiOpen */ );

/// Stub of the lgpio SPI full-duplex transfer call.
/**
 * \returns the number of bytes transferred on success
 * \returns a negative error code on failure
 */
int lgSpiXfer( int         handle /**< [in] the handle returned by lgSpiOpen */,
               const char *txBuf /**< [in] the bytes to transmit */,
               char       *rxBuf /**< [out] the bytes received */,
               int         count /**< [in] the number of bytes to transfer */ );

#endif // mcp3008Ctrl_tests_stubs_lgpio_h
