/** \file logFileRaw.cpp
 * \brief Manage a raw log file.
 * \author Jared R. Males (jaredmales@gmail.com)
 *
 * \ingroup logger_files
 *
 */

#include <cstring>
#include <filesystem>

#include "logFileRaw.hpp"
#include "../sys/fileTimes.hpp"

namespace MagAOX
{
namespace logger
{

logFileRaw::logFileRaw()
{
}

logFileRaw::~logFileRaw()
{
    close();
}

int logFileRaw::logPath( const std::string &newPath )
{
    m_logPath = newPath;
    return 0;
}

std::string logFileRaw::logPath()
{
    return m_logPath;
}

int logFileRaw::logName( const std::string &newName )
{
    m_logName = newName;
    return 0;
}

std::string logFileRaw::logName()
{
    return m_logName;
}

int logFileRaw::logExt( const std::string &newExt )
{
    m_logExt = newExt;
    return 0;
}

std::string logFileRaw::logExt()
{
    return m_logExt;
}

int logFileRaw::maxLogSize( size_t newMaxFileSize )
{
    m_maxLogSize = newMaxFileSize;
    return 0;
}

size_t logFileRaw::maxLogSize()
{
    return m_maxLogSize;
}

int logFileRaw::writeLog( flatlogs::bufferPtrT &data )
{
    size_t N = flatlogs::logHeader::totalSize( data );

    // Check if we need a new file
    if( m_currFileSize + N > m_maxLogSize || m_fout == 0 )
    {
        flatlogs::timespecX ts = flatlogs::logHeader::timespec( data );

        if( createFile( ts ) < 0 )
        {
            std::cerr << "logFileRaw::writeLog: Error by createFile.  At: " << __FILE__ << " " << __LINE__ << '\n';
            return -1;
        }
    }

    size_t nwr = fwrite( data.get(), sizeof( char ), N, m_fout );

    if( nwr != N * sizeof( char ) )
    {
        std::cerr << "logFileRaw::writeLog: Error by fwrite.  At: " << __FILE__ << " " << __LINE__ << '\n';
        std::cerr << "logFileRaw::writeLog: errno says: " << strerror( errno ) << '\n';
        return -1;
    }

    m_currFileSize += N;

    return 0;
}

int logFileRaw::flush()
{
    ///\todo this probably should be fsync, with appropriate error handling (see fsyncgate) [issue #192]

    if( m_fout )
    {
        if( fflush( m_fout ) != 0 )
        {
            std::cerr << "logFileRaw::flush: Error from fflush. At: " << __FILE__ << " " << __LINE__ << "\n";
            std::cerr << "logFileRaw::flush: errno says: " << strerror( errno ) << "\n";

            return -1;
        }
    }
    return 0;
}

int logFileRaw::close()
{
    if( m_fout )
    {
        errno = 0;

        if( fclose( m_fout ) != 0 )
        {
            std::cerr << "logFileRaw::close: Error from fclose. At: " << __FILE__ << " " << __LINE__ << "\n";
            std::cerr << "logFileRaw::close: errno says: " << strerror( errno ) << "\n";

            m_fout = nullptr;

            return -1;
        }

        m_fout = nullptr;
    }
    return 0;
}

int logFileRaw::createFile( flatlogs::timespecX &ts )
{
    std::string fileName;
    std::string relPath;

    int rv = sys::fileTimeRelPath( fileName, relPath, m_logName, m_logExt, ts.time_s, ts.time_ns );
    if( rv < 0 )
    {
        std::cerr << "logFileRaw::createFile: Error from fileTimePath. code: " << rv << ". ";
        std::cerr << __FILE__ << ' ' << __LINE__ << '\n';
        return -1;
    }

    std::string fullPath = m_logPath + '/' + relPath + '/';

    // Create directory
    try
    {
        std::filesystem::create_directories( fullPath ); // this does nothing if fname already exists
    }
    catch( const std::filesystem::filesystem_error &e )
    {
        std::cerr << "logFileRaw::createFile: filesystem_error from std::create_directories.\n";
        std::cerr << "    what: " << e.what() << '\n';
        std::cerr << "    code: " << e.code() << '\n';
        std::cerr << __FILE__ << ' ' << __LINE__ << '\n';
        return -1;
    }
    catch( const std::exception &e )
    {
        std::cerr << "logFileRaw::createFile: exception from std::create_directories.\n";
        std::cerr << "    what: " << e.what() << '\n';
        std::cerr << __FILE__ << ' ' << __LINE__ << '\n';
        return -1;
    }

    fullPath += fileName;

    // Check if file exists
    try
    {
        // This will be true only if another instance with same name (essentially impossible) tries
        // at same ns, which is pathological
        if( std::filesystem::exists( fullPath ) )
        {
            std::cerr << "logFileRaw::createFile: file " << fullPath << " exists. ";
            std::cerr << __FILE__ << ' ' << __LINE__ << '\n';
            return -1;
        }
    }
    catch( const std::filesystem::filesystem_error &e )
    {
        std::cerr << "logFileRaw::createFile: filesystem_error from std::filesystem::exists.\n";
        std::cerr << "    what: " << e.what() << '\n';
        std::cerr << "    code: " << e.code() << '\n';
        std::cerr << __FILE__ << ' ' << __LINE__ << '\n';
        return -1;
    }
    catch( const std::exception &e )
    {
        std::cerr << "logFileRaw::createFile: exception from std::filesystem::exists.\n";
        std::cerr << "    what: " << e.what() << '\n';
        std::cerr << __FILE__ << ' ' << __LINE__ << '\n';
        return -1;
    }

    // Close current file if it's open
    if( close() < 0 )
    {
        std::cerr << "logFileRaw::createFile: Error from close, attempting to continue. At: ";
        std::cerr << __FILE__ << " " << __LINE__ << '\n';
    }

    errno = 0;

    m_fout = fopen( fullPath.c_str(), "wb" );

    if( m_fout == 0 )
    {
        std::cerr << "logFileRaw::createFile: Error by fopen. At: " << __FILE__ << " " << __LINE__ << '\n';
        std::cerr << "logFileRaw::createFile: errno says: " << strerror( errno ) << '\n';
        std::cerr << "logFileRaw::createFile: fname = " << fullPath << '\n';
        return -1;
    }

    // Reset counters.
    m_currFileSize = 0;

    return 0;
}

} // namespace logger
} // namespace MagAOX
