/** \file wccStarCatalog.hpp
 * \brief Star catalog ingest and cone search for the WCC applications.
 * \author Adam Schilperoort
 *
 * The C++ equivalent of `load_gaia_subset()` in
 * `uasal_star_catalog_simulator_prysm.py`, which reads a GSC 3.1 style CSV with
 * `astropy.table.Table.read(..., format="ascii.csv", comment="#")` and keeps the
 * `GAIAdr3sourceID`, `ra`, `dec` and `mag` columns.
 *
 * Columns are resolved by name from the header line, so the many other columns
 * in a GSC 3.1 export are ignored and the magnitude column can be switched to
 * any of the per band columns without code changes.
 *
 * A declination sorted index supports the cone searches the simulator and the
 * astrometric solver both need, so neither has to scan the whole catalog per
 * frame.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccStarCatalog_hpp
#define wccStarCatalog_hpp

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "wccSkyWCS.hpp"
#include "wccUnits.hpp"

namespace MagAOX
{
namespace wcc
{

/// One catalog source.
/** \ingroup wccCommon
 */
struct starEntry
{
    std::string m_id; ///< Catalog identifier, from the configured id column.

    double m_ra{ 0 }; ///< Right ascension, ICRS [deg].

    double m_dec{ 0 }; ///< Declination, ICRS [deg].

    double m_mag{ 0 }; ///< Magnitude, from the configured magnitude column.
};

/// Trim ASCII whitespace and surrounding double quotes from a field.
/** \returns the trimmed field
 *
 * \ingroup wccCommon
 */
inline std::string trimField( const std::string &s /**< [in] raw field text */ )
{
    size_t b = 0;
    size_t e = s.size();

    while( b < e && ( s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n' ) )
    {
        ++b;
    }

    while( e > b && ( s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n' ) )
    {
        --e;
    }

    if( e - b >= 2 && s[b] == '"' && s[e - 1] == '"' )
    {
        ++b;
        --e;
    }

    return s.substr( b, e - b );
}

/// Split a CSV line on commas, honouring double quoted fields.
/** \ingroup wccCommon
 */
inline void splitCSV( const std::string &line /**< [in] one CSV record */,
                      std::vector<std::string> &fields /**< [out] the split and trimmed fields */ )
{
    fields.clear();

    std::string cur;
    bool inQuote = false;

    for( char c : line )
    {
        if( c == '"' )
        {
            inQuote = !inQuote;
            cur.push_back( c );
        }
        else if( c == ',' && !inQuote )
        {
            fields.push_back( trimField( cur ) );
            cur.clear();
        }
        else
        {
            cur.push_back( c );
        }
    }

    fields.push_back( trimField( cur ) );
}

/// A star catalog with a declination sorted cone search index.
/** \ingroup wccCommon
 */
class starCatalog
{

    /** \name Catalog Contents - Data
     *@{
     */
  protected:
    /// Sources, sorted by declination so that coneSearch() can bracket by binary search.
    std::vector<starEntry> m_stars;

    std::string m_path; ///< File the catalog was loaded from.

    std::string m_magColumn; ///< Header name of the magnitude column actually used.

    size_t m_skipped{ 0 }; ///< Rows rejected for missing or unparseable ra, dec or mag.

    double m_magMin{ 0 }; ///< Brightest magnitude present.

    double m_magMax{ 0 }; ///< Faintest magnitude present.
    ///@}

  public:
    /// Read a catalog from a CSV file.
    /** Lines beginning with '#' are skipped. The first remaining line is the
     * header and supplies the column names. Rows whose ra, dec or magnitude is
     * blank or unparseable are counted in skipped() and dropped.
     *
     * \returns 0 on success
     * \returns -1 if the file cannot be opened or a required column is missing
     */
    int load( const std::string &path /**< [in] path to the CSV catalog */,
              const std::string &magColumn = "mag" /**< [in] header name of the magnitude column */,
              const std::string &idColumn = "gsc2ID" /**< [in] header name of the identifier column */,
              double magLimit = 99.0 /**< [in] drop sources fainter than this */,
              size_t maxRows = 0 /**< [in] stop after this many accepted rows, 0 means no limit */ );

    /// Replace the catalog contents directly, then rebuild the index.
    /** Provided for tests and for callers that obtain sources another way.
     */
    void setStars( const std::vector<starEntry> &stars /**< [in] sources to hold */ );

    /// Number of sources held.
    size_t size() const;

    /// True if no sources are held.
    bool empty() const;

    /// Access a source by index.
    const starEntry &operator[]( size_t i /**< [in] index, must be less than size() */ ) const;

    /// The sources, in declination sorted order.
    const std::vector<starEntry> &stars() const;

    /// File the catalog was loaded from.
    const std::string &path() const;

    /// Header name of the magnitude column used.
    const std::string &magColumn() const;

    /// Rows rejected during load().
    size_t skipped() const;

    /// Brightest magnitude present.
    double magMin() const;

    /// Faintest magnitude present.
    double magMax() const;

    /// Find every source within an angular radius of a sky position.
    /** Brackets the declination band by binary search, then rejects on true
     * angular separation, so it is correct across the RA wrap and near the poles.
     *
     * \returns the number of sources appended to out
     */
    size_t coneSearch( double ra /**< [in] search center right ascension [deg] */,
                       double dec /**< [in] search center declination [deg] */,
                       double radius /**< [in] search radius [deg] */,
                       std::vector<size_t> &out /**< [out] indices of matching sources, appended */,
                       double magLimit = 99.0 /**< [in] ignore sources fainter than this */ ) const;

  protected:
    /// Sort by declination and refresh the magnitude range.
    void buildIndex();
};

inline int starCatalog::load( const std::string &path,
                              const std::string &magColumn,
                              const std::string &idColumn,
                              double magLimit,
                              size_t maxRows )
{
    std::ifstream fin( path );

    if( !fin.good() )
    {
        return -1;
    }

    m_path = path;
    m_magColumn = magColumn;
    m_skipped = 0;
    m_stars.clear();

    std::string line;
    std::vector<std::string> fields;

    // Find the header: the first line that is neither blank nor a '#' comment.
    int raCol = -1;
    int decCol = -1;
    int magCol = -1;
    int idCol = -1;
    size_t ncols = 0;

    while( std::getline( fin, line ) )
    {
        const std::string t = trimField( line );

        if( t.empty() || t[0] == '#' )
        {
            continue;
        }

        splitCSV( line, fields );
        ncols = fields.size();

        for( size_t i = 0; i < ncols; ++i )
        {
            if( fields[i] == "ra" )
            {
                raCol = static_cast<int>( i );
            }
            else if( fields[i] == "dec" )
            {
                decCol = static_cast<int>( i );
            }
            else if( fields[i] == magColumn )
            {
                magCol = static_cast<int>( i );
            }
            else if( fields[i] == idColumn )
            {
                idCol = static_cast<int>( i );
            }
        }

        break;
    }

    if( raCol < 0 || decCol < 0 || magCol < 0 )
    {
        return -1;
    }

    const size_t need = static_cast<size_t>( std::max( std::max( raCol, decCol ), magCol ) ) + 1;

    while( std::getline( fin, line ) )
    {
        if( line.empty() || line[0] == '#' )
        {
            continue;
        }

        splitCSV( line, fields );

        if( fields.size() < need )
        {
            ++m_skipped;
            continue;
        }

        starEntry s;

        try
        {
            const std::string &raS = fields[static_cast<size_t>( raCol )];
            const std::string &decS = fields[static_cast<size_t>( decCol )];
            const std::string &magS = fields[static_cast<size_t>( magCol )];

            if( raS.empty() || decS.empty() || magS.empty() )
            {
                ++m_skipped;
                continue;
            }

            s.m_ra = std::stod( raS );
            s.m_dec = std::stod( decS );
            s.m_mag = std::stod( magS );
        }
        catch( ... )
        {
            ++m_skipped;
            continue;
        }

        if( !std::isfinite( s.m_ra ) || !std::isfinite( s.m_dec ) || !std::isfinite( s.m_mag ) )
        {
            ++m_skipped;
            continue;
        }

        if( s.m_mag > magLimit )
        {
            ++m_skipped;
            continue;
        }

        if( idCol >= 0 && static_cast<size_t>( idCol ) < fields.size() )
        {
            s.m_id = fields[static_cast<size_t>( idCol )];
        }

        m_stars.push_back( s );

        if( maxRows > 0 && m_stars.size() >= maxRows )
        {
            break;
        }
    }

    buildIndex();

    return 0;
}

inline void starCatalog::setStars( const std::vector<starEntry> &stars )
{
    m_stars = stars;
    buildIndex();
}

inline void starCatalog::buildIndex()
{
    std::sort( m_stars.begin(),
               m_stars.end(),
               []( const starEntry &a, const starEntry &b ) { return a.m_dec < b.m_dec; } );

    m_magMin = 0;
    m_magMax = 0;

    bool first = true;
    for( const starEntry &s : m_stars )
    {
        if( first )
        {
            m_magMin = s.m_mag;
            m_magMax = s.m_mag;
            first = false;
            continue;
        }

        m_magMin = std::min( m_magMin, s.m_mag );
        m_magMax = std::max( m_magMax, s.m_mag );
    }
}

inline size_t starCatalog::size() const
{
    return m_stars.size();
}

inline bool starCatalog::empty() const
{
    return m_stars.empty();
}

inline const starEntry &starCatalog::operator[]( size_t i ) const
{
    return m_stars[i];
}

inline const std::vector<starEntry> &starCatalog::stars() const
{
    return m_stars;
}

inline const std::string &starCatalog::path() const
{
    return m_path;
}

inline const std::string &starCatalog::magColumn() const
{
    return m_magColumn;
}

inline size_t starCatalog::skipped() const
{
    return m_skipped;
}

inline double starCatalog::magMin() const
{
    return m_magMin;
}

inline double starCatalog::magMax() const
{
    return m_magMax;
}

inline size_t starCatalog::coneSearch( double ra,
                                       double dec,
                                       double radius,
                                       std::vector<size_t> &out,
                                       double magLimit ) const
{
    if( m_stars.empty() || radius <= 0 )
    {
        return 0;
    }

    const double decLo = dec - radius;
    const double decHi = dec + radius;

    // Bracket the declination band. Sources outside it cannot be within radius.
    auto lo = std::lower_bound( m_stars.begin(),
                                m_stars.end(),
                                decLo,
                                []( const starEntry &s, double v ) { return s.m_dec < v; } );

    auto hi = std::upper_bound( m_stars.begin(),
                                m_stars.end(),
                                decHi,
                                []( double v, const starEntry &s ) { return v < s.m_dec; } );

    size_t n = 0;

    for( auto it = lo; it != hi; ++it )
    {
        if( it->m_mag > magLimit )
        {
            continue;
        }

        if( angularSeparation( ra, dec, it->m_ra, it->m_dec ) > radius )
        {
            continue;
        }

        out.push_back( static_cast<size_t>( it - m_stars.begin() ) );
        ++n;
    }

    return n;
}

} // namespace wcc
} // namespace MagAOX

#endif // wccStarCatalog_hpp
