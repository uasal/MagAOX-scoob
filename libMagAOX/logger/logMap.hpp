/** \file logMap.hpp
 * \brief Declares and defines the logMap class and related classes.
 * \author Jared R. Males (jaredmales@gmail.com)
 *
 * \ingroup logger_files
 *
 * History:
 * - 2020-01-02 created by JRM
 */

#ifndef logger_logMap_hpp
#define logger_logMap_hpp

#include <mx/sys/timeUtils.hpp>
using namespace mx::sys::tscomp;

#include <mx/ioutils/fileUtils.hpp>

#include <vector>
#include <map>

#include <flatlogs/flatlogs.hpp>
#include "../file/stdFileName.hpp"

namespace MagAOX
{
namespace logger
{

/// Structure to hold a log file in memory, tracking when a new file needs to be opened.
struct logInMemory
{
    typedef mx::verbose::vvv verboseT;

    std::vector<char> m_memory; ///< The buffer holding the log data.

    flatlogs::timespecX m_startTime{ 0, 0 };
    flatlogs::timespecX m_endTime{ 0, 0 };

    int loadFile( file::stdFileName<verboseT> const &lfn );
};

/// Map of log entries by application name, mapping both to files and to loaded buffers.
struct logMap
{
    typedef mx::verbose::vvv verboseT;

    typedef file::stdSubDir<verboseT> stdSubDirT;
    typedef file::stdFileName<verboseT> stdFileNameT;

    /// The app-name to file-name map type, for sorting the input files by application
    typedef std::map<std::string, std::set<stdFileNameT, file::compStdFileName<stdFileNameT>>> appToFileMapT;

    /// The app-name to buffer map type, for looking up the currently loaded logs for a given app.
    typedef std::map<std::string, logInMemory> appToBufferMapT;

    appToFileMapT m_appToFileMap;

    appToBufferMapT m_appToBufferMap;

    /// Get log file names in a directory and distribute them into the map by app-name
    /** Finding no logs is not reported as an error (no exception is thrown).  You must check
     *  the size of m_appToFileMap to check if any files were found.
     *
     * \b Tests
     *     - Building the app-to-file map \ref tests_libMagAOX_logger_logMap_apptofile "[test doc]"
     */
    mx::error_t loadAppToFileMap( const std::string &dir,             /**< [in] the directory to search for files
                                                                        (contains the dev/YYYY_MM_DD subdirs)*/
                           const std::string       &dev,       ///< [in] the device name to search for logs of
                           const std::string       &ext,       ///< [in] the extension to search for
                           const stdFileNameT &firstFile, ///< [in] the first file that needs coverage
                           const stdFileNameT &lastFile   ///< [in] the last file that needs coverage
    );

    /// Get the log for an event code which is the first prior to the supplied time
    int getPriorLog( char                      *&logBefore, ///< [out] pointer to the first byte of the prior log entry
                     const std::string          &appName,   ///< [in] the name of the app specifying which log to search
                     const flatlogs::eventCodeT &ev,        ///< [in] the event code to search for
                     const flatlogs::timespecX  &ts,        ///< [in] the timestamp to be prior to
                     char                       *hint = 0   /**< [in] [optional] a hint specifying
                                                                      where to start searching.  If null
                                                                      search starts at beginning.*/
    );

    /// Get the next log with the same event code which is after the supplied time
    int getNextLog( char             *&logAfter,   ///< [out] pointer to the first byte of the prior log entry
                    char              *logCurrent, ///< [in] The log to start from
                    const std::string &appName     ///< [in] the name of the app specifying which log to search
    );

    int getNearestLogs( flatlogs::bufferPtrT &logBefore, flatlogs::bufferPtrT &logAfter, const std::string &appName );

    int loadFiles( const std::string         &appName,  ///< MagAO-X app name for which to load files
                   const flatlogs::timespecX &startTime ///<
    );
};

} // namespace logger
} // namespace MagAOX

#endif // logger_logMap_hpp
