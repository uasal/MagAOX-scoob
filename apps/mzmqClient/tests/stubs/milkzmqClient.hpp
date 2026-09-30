/** \file milkzmqClient.hpp
 * \brief Test stand-in for the milkzmq client header used by mzmqClient.
 * \author Claude Code
 *
 * The real `milkzmqClient.hpp` lives in `/opt/MagAOX/source/milkzmq`, which is not on the unit test include path,
 * and it needs a ZeroMQ connection.  This stand-in declares only the parts of `milkzmq::milkzmqClient` that the
 * `mzmqClient` app uses, with the same names and signatures.  The member functions are defined in
 * `mzmqClient_test.cpp` with controllable fake behavior.
 *
 * \ingroup mzmqClient_files
 */

#ifndef mzmqClient_tests_stubs_milkzmqClient_hpp
#define mzmqClient_tests_stubs_milkzmqClient_hpp

#include <pthread.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

/// Stand-in for the milkzmq namespace.
namespace milkzmq
{

/// Stand-in for the milkzmq ImageStreamIO client.
class milkzmqClient
{
  protected:
    std::string m_argv0{ "milkzmqClient" }; ///< The invoked name, used for error messages.

    std::string m_address{ "" }; ///< The address of the image server.

    int m_imagePort{ 5556 }; ///< The port number to use for the image server.

    /// Structure to manage the image threads.
    struct s_imageThread
    {
        std::thread *m_thread{ nullptr }; ///< Thread for receiving images, deleted by the parent's destructor.

        milkzmqClient *m_mzc{ nullptr }; ///< The client owning this thread.

        std::string m_imageName; ///< The name of the remote image stream.

        std::string m_localImageName; ///< The local name of the image stream.

        /// Create the (not yet started) thread object.
        s_imageThread()
        {
            m_thread = new std::thread;
        }
    };

    std::vector<s_imageThread> m_imageThreads; ///< The image threads, one per stream.

  public:
    /// Default c'tor.
    milkzmqClient();

    /// Destructor, stops and joins the image threads and deletes the thread objects.
    ~milkzmqClient();

    /// Add a shared memory image, with the same local name.
    /**
     * \returns 0 on success
     */
    int shMemImName( const std::string &name /**< [in] the remote name of the image */ );

    /// Add a shared memory image with a local name.
    /**
     * \returns 0 on success
     */
    int shMemImName( const std::string &name,     /**< [in] the remote name of the image */
                     const std::string &localName /**< [in] the local name of the image */
    );

    /// Get the remote name of an image.
    /**
     * \returns the name, or "" if `imno` is out of range
     */
    std::string shMemImName( size_t imno /**< [in] the image number */ );

    /// Get the local name of an image.
    /**
     * \returns the local name, or "" if `imno` is out of range
     */
    std::string localShMemImName( size_t imno /**< [in] the image number */ );

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

#endif // mzmqClient_tests_stubs_milkzmqClient_hpp
