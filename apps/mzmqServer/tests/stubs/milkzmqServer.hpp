/** \file milkzmqServer.hpp
 * \brief Test stand-in for the milkzmq server header used by mzmqServer.
 * \author Claude Code
 *
 * The real `milkzmqServer.hpp` lives in `/opt/MagAOX/source/milkzmq`, which is not on the unit test include path,
 * and it needs ZeroMQ sockets and ImageStreamIO streams.  This stand-in declares only the parts of
 * `milkzmq::milkzmqServer` that the `mzmqServer` app and its tests use, with the same names and signatures.  The
 * member functions are defined in `mzmqServer_test.cpp` with controllable fake behavior.
 *
 * \ingroup mzmqServer_files
 */

#ifndef mzmqServer_tests_stubs_milkzmqServer_hpp
#define mzmqServer_tests_stubs_milkzmqServer_hpp

#include <pthread.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include <xrif/xrif.h>

/// Stand-in for the milkzmq namespace.
namespace milkzmq
{

/// Stand-in for the milkzmq ImageStreamIO server.
class milkzmqServer
{
  protected:
    std::string m_argv0{ "milkzmqServer" }; ///< The invoked name, used for error messages.

    int m_imagePort{ 5556 }; ///< The port number to use for the image server.

    int m_usecSleep{ 100 }; ///< The number of microseconds to sleep on each loop.

    float m_fpsTgt{ 10 }; ///< The max frames per second to transmit.

    float m_fpsGain{ 0.1 }; ///< Integrator gain on the fps trigger delta.

    int m_xrifDifferenceMethod{ XRIF_DIFFERENCE_NONE }; ///< The xrif difference method.

    int m_xrifReorderMethod{ XRIF_REORDER_NONE }; ///< The xrif reordering method.

    int m_xrifCompressMethod{ XRIF_COMPRESS_NONE }; ///< The xrif compression method.

    std::thread m_serverThread; ///< The server thread.

    /// Structure to manage the image threads.
    struct s_imageThread
    {
        std::thread *m_thread{ nullptr }; ///< Thread for publishing images, deleted by the parent's destructor.

        milkzmqServer *m_mzs{ nullptr }; ///< The server owning this thread.

        std::string m_imageName; ///< The name of the image stream.

        /// Create the (not yet started) thread object.
        s_imageThread()
        {
            m_thread = new std::thread;
        }
    };

    std::vector<s_imageThread> m_imageThreads; ///< The image threads, one per stream.

  public:
    /// Default c'tor.
    milkzmqServer();

    /// Destructor, stops and joins the threads and deletes the image thread objects.
    ~milkzmqServer();

    /// Get the image port number.
    /**
     * \returns the current value of m_imagePort
     */
    int imagePort();

    /// Add a shared memory image to serve.
    /**
     * \returns 0 on success
     */
    int shMemImName( const std::string &name /**< [in] the name of the image */ );

    /// Get the number of images being served.
    /**
     * \returns the size of m_imageThreads
     */
    size_t numImages();

    /// Get the name of an image.
    /**
     * \returns the name of image `n`
     */
    std::string shMemImName( size_t n /**< [in] the image number */ );

    /// Get the sleep time between semaphore checks.
    /**
     * \returns the current value of m_usecSleep
     */
    int usecSleep();

    /// Get the target maximum F.P.S.
    /**
     * \returns the current value of m_fpsTgt
     */
    float fpsTgt();

    /// Get the gain of the F.P.S. loop.
    /**
     * \returns the current value of m_fpsGain
     */
    float fpsGain();

    /// Disable compression.
    void noCompression();

    /// Enable the default compression (pixel differencing, bytepack-renibble reordering, LZ4).
    void defaultCompression();

    /// Get the xrif difference method.
    /**
     * \returns the current value of m_xrifDifferenceMethod
     */
    int xrifDifferenceMethod();

    /// Get the xrif reordering method.
    /**
     * \returns the current value of m_xrifReorderMethod
     */
    int xrifReorderMethod();

    /// Get the xrif compression method.
    /**
     * \returns the current value of m_xrifCompressMethod
     */
    int xrifCompressMethod();

    /// Start the server thread.
    /**
     * \returns 0 on success
     * \returns the injected return value otherwise
     */
    int serverThreadStart();

    /// Signal the server thread to exit.
    /**
     * \returns 0 on success
     */
    int serverThreadKill();

    /// Start an image thread.
    /**
     * \returns 0 on success
     * \returns the injected return value otherwise
     */
    int imageThreadStart( size_t thno /**< [in] the thread to start */ );

    /// Flag to control execution.  When true all threads exit.
    static std::atomic_bool m_timeToDie;

    /// Signal an image thread to exit.
    /**
     * \returns 0 on success
     */
    int imageThreadKill( size_t thno /**< [in] the thread to kill */ );

    /// Report status (LOG_INFO priority).
    virtual void reportInfo( const std::string &msg /**< [in] the status message */ );

    /// Report status (LOG_NOTICE priority).
    virtual void reportNotice( const std::string &msg /**< [in] the status message */ );

    /// Report a warning.
    virtual void reportWarning( const std::string &msg /**< [in] the warning message */ );

    /// Report an error.
    virtual void reportError( const std::string &msg,  /**< [in] the error message */
                              const std::string &file, /**< [in] the file where the error occurred */
                              int                line  /**< [in] the line number of the error */
    );
};

} // namespace milkzmq

#endif // mzmqServer_tests_stubs_milkzmqServer_hpp
