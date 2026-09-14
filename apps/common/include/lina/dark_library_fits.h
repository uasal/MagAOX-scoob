#pragma once

/** \file dark_library_fits.h
  * \brief Stamp and scan FITS headers for the shared dark library.
  */

#include <lina/dark_library.h>

#include <algorithm>
#include <dirent.h>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <mx/ioutils/fits/fitsFile.hpp>
#include <mx/ioutils/fits/fitsHeader.hpp>

namespace lina {

/// True for `*.fits` / `*.fit` (any case), excluding `.` / `..`.
inline bool is_dark_fits_filename( const std::string &name )
{
    if( name.empty() || name[0] == '.' )
        return false;
    const std::string lower = darklib_detail::lower( name );
    const auto has_ext = [&]( const char *ext ) -> bool {
        const std::size_t n = std::char_traits<char>::length( ext );
        return lower.size() >= n && lower.compare( lower.size() - n, n, ext ) == 0;
    };
    return has_ext( ".fits" ) || has_ext( ".fit" );
}

/// Stamp library metadata onto a FITS header (HIERARCH for long keyword names).
inline void append_dark_entry_header( mx::fits::fitsHeader &head /**< [in,out] header to append to */,
                                      const DarkLibraryEntry &e /**< [in] values to stamp */ )
{
    head.append( "EXPTIME", e.exptime, "exposure time [s]" );
    head.append( "NDARK", e.ndark, "frames averaged" );
    if( !e.shm_cam_input.empty() )
        head.append( "SHM_CAM_INPUT", e.shm_cam_input, "ImageStreamIO name" );
    if( !e.cam_name.empty() )
        head.append( "CAM_NAME", e.cam_name, "INDI camera device" );
    if( e.width > 0 )
        head.append( "WIDTH", e.width, "image width [pix]" );
    if( e.height > 0 )
        head.append( "HEIGHT", e.height, "image height [pix]" );
    if( e.bitdepth > 0 )
        head.append( "BITDEPTH", e.bitdepth, "camera bit depth" );
    head.append( "ROI_X", e.roi_x, "ROI origin x [pix]" );
    head.append( "ROI_Y", e.roi_y, "ROI origin y [pix]" );
    head.append( "ROI_WIDTH", e.roi_width, "ROI width [pix]" );
    head.append( "ROI_HEIGHT", e.roi_height, "ROI height [pix]" );
    if( std::isfinite( e.gain ) )
        head.append( "EMGAIN", e.gain, "EM gain" );
    if( std::isfinite( e.blacklevel ) )
        head.append( "BLACKLEVEL", e.blacklevel, "black level" );
}

/// Parse a FITS header already loaded by mxlib into a library entry.
inline DarkLibraryEntry entry_from_fits_header( mx::fits::fitsHeader &head /**< [in] loaded header */,
                                                const std::string &relpath /**< [in] filename in lib dir */ )
{
    std::map<std::string, std::string> raw;
    for( auto it = head.begin(); it != head.end(); ++it )
    {
        const std::string key = it->keyword();
        if( key.empty() || key == "COMMENT" || key == "HISTORY" )
            continue;
        try
        {
            raw[key] = it->valueStr();
        }
        catch( ... )
        {
            continue;
        }
    }
    return darklib_detail::entry_from_header_fields( raw, relpath );
}

/// Read one FITS file's primary header into a library entry.
inline DarkLibraryEntry entry_from_dark_fits_file( const std::string &path /**< [in] absolute FITS path */,
                                                   const std::string &relpath /**< [in] filename in lib dir */ )
{
    mx::fits::fitsFile<float> ff;
    mx::fits::fitsHeader head;
    if( ff.readHeader( head, path ) < 0 )
        throw std::runtime_error( "failed to read FITS header: " + path );
    return entry_from_fits_header( head, relpath );
}

/// Scan `lib_dir` for FITS files and return entries with a valid EXPTIME.
/** Files that cannot be parsed or lack a positive EXPTIME are omitted.
  * If `skipped` is non-null, it receives `filename: reason` strings.
  */
inline std::vector<DarkLibraryEntry>
scan_dark_fits_directory( const std::string &lib_dir /**< [in] dark library directory */,
                          std::vector<std::string> *skipped = nullptr /**< [out] optional skip log */ )
{
    if( lib_dir.empty() )
        throw std::runtime_error( "dark library directory is empty" );

    DIR *dir = opendir( lib_dir.c_str() );
    if( dir == nullptr )
        throw std::runtime_error( "cannot open dark library directory: " + lib_dir );

    std::vector<std::string> names;
    while( dirent *de = readdir( dir ) )
    {
        if( de->d_name[0] == '\0' )
            continue;
        const std::string name = de->d_name;
        if( is_dark_fits_filename( name ) )
            names.push_back( name );
    }
    closedir( dir );
    std::sort( names.begin(), names.end() );

    std::vector<DarkLibraryEntry> entries;
    entries.reserve( names.size() );
    for( const auto &name : names )
    {
        try
        {
            auto e = entry_from_dark_fits_file( darklib_detail::join_path( lib_dir, name ), name );
            if( !std::isfinite( e.exptime ) || e.exptime <= 0.0 )
            {
                if( skipped )
                    skipped->push_back( name + ": missing EXPTIME" );
                continue;
            }
            if( e.relpath.empty() )
                e.relpath = name;
            entries.push_back( std::move( e ) );
        }
        catch( const std::exception &ex )
        {
            if( skipped )
                skipped->push_back( name + ": " + ex.what() );
        }
    }
    return entries;
}

/// Rebuild `dark_metadata.txt` from FITS headers in `lib_dir`.
inline std::vector<DarkLibraryEntry>
generate_dark_library_manifest_from_fits( const std::string &lib_dir /**< [in] dark library directory */,
                                          std::vector<std::string> *skipped = nullptr /**< [out] optional skip log */ )
{
    auto entries = scan_dark_fits_directory( lib_dir, skipped );
    if( entries.empty() )
        throw std::runtime_error( "no FITS files with EXPTIME in " + lib_dir );
    write_dark_library_manifest( lib_dir, entries );
    return entries;
}

} // namespace lina
