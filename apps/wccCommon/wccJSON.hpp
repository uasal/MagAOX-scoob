/** \file wccJSON.hpp
 * \brief A small dependency free JSON reader for WCC visit files.
 * \author Adam Schilperoort
 *
 * The MagAO-X tree has no C++ JSON library: FlatBuffers covers the telemetry
 * path and the Python applications use orjson. The WCC visit file is JSON, so
 * this header supplies just enough of a reader to consume one, rather than
 * vendoring a large third party dependency for a single file format.
 *
 * Scope is deliberately limited to reading:
 * - objects, arrays, strings, numbers, true, false, null
 * - the standard string escapes, with \\u decoded to UTF-8
 * - object member order is preserved
 *
 * There is no writer, no comment extension and no trailing comma extension.
 * Numbers are held as double, which is exact for the integers that appear in a
 * visit file.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccJSON_hpp
#define wccJSON_hpp

#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "wccNumeric.hpp"

namespace MagAOX
{
namespace wcc
{

/// A parsed JSON value.
/** Accessors never throw and never return a reference to a temporary: lookups
 * that miss return a shared static null value, so chained access such as
 * `root["GUIDE_STAR"][0]["SENSOR"].asString("")` is safe on malformed input.
 *
 * \ingroup wccCommon
 */
class jsonValue
{
  public:
    /// The JSON value kinds.
    enum valueType
    {
        Null,   ///< JSON null, and the kind of any failed lookup.
        Bool,   ///< JSON true or false.
        Number, ///< A JSON number, held as double.
        String, ///< A JSON string.
        Array,  ///< A JSON array.
        Object  ///< A JSON object.
    };

    /** \name Contents - Data
     *@{
     */
  protected:
    valueType m_type{ Null }; ///< Which kind of value this is.

    bool m_bool{ false }; ///< Payload when m_type is Bool.

    double m_number{ 0 }; ///< Payload when m_type is Number.

    std::string m_string; ///< Payload when m_type is String.

    std::vector<jsonValue> m_array; ///< Payload when m_type is Array.

    /// Payload when m_type is Object, in document order.
    std::vector<std::pair<std::string, jsonValue>> m_object;
    ///@}

  public:
    /// The kind of value this is.
    valueType type() const;

    /// True if this is JSON null, which includes every failed lookup.
    bool isNull() const;

    /// True if this is a JSON object.
    bool isObject() const;

    /// True if this is a JSON array.
    bool isArray() const;

    /// True if this is a JSON number.
    bool isNumber() const;

    /// True if this is a JSON string.
    bool isString() const;

    /// True if this is a JSON boolean.
    bool isBool() const;

    /// Number of array elements, or object members, or 0 for a scalar.
    size_t size() const;

    /// Look up an object member.
    /** \returns a reference to the member
     * \returns a reference to a shared null value if this is not an object or the key is absent
     */
    const jsonValue &operator[]( const std::string &key /**< [in] member name */ ) const;

    /// Index an array.
    /** \returns a reference to the element
     * \returns a reference to a shared null value if this is not an array or the index is out of range
     */
    const jsonValue &operator[]( size_t i /**< [in] element index */ ) const;

    /// Name of an object member by position, for iterating an object in document order.
    /** \returns the member name
     * \returns an empty string if this is not an object or the index is out of range
     */
    const std::string &key( size_t i /**< [in] member index */ ) const;

    /// Value of an object member by position.
    /** \returns a reference to the member value
     * \returns a reference to a shared null value if this is not an object or the index is out of range
     */
    const jsonValue &value( size_t i /**< [in] member index */ ) const;

    /// True if this is an object holding the named member.
    bool has( const std::string &key /**< [in] member name */ ) const;

    /// This value as a double.
    /** Strings are parsed, booleans map to 1 and 0.
     *
     * \returns the value, or def if it cannot be represented as a number
     */
    double asDouble( double def = 0 /**< [in] value to return if conversion is not possible */ ) const;

    /// This value as an int.
    /** \returns the value rounded to nearest, or def if conversion is not possible
     */
    int asInt( int def = 0 /**< [in] value to return if conversion is not possible */ ) const;

    /// This value as a bool.
    /** Numbers are true when non-zero.
     *
     * \returns the value, or def if conversion is not possible
     */
    bool asBool( bool def = false /**< [in] value to return if conversion is not possible */ ) const;

    /// This value as a string.
    /** Numbers and booleans are formatted.
     *
     * \returns the value, or def if this is null, an array or an object
     */
    std::string asString(
        const std::string &def = std::string() /**< [in] value to return if conversion is not possible */ ) const;

    /// A member of this object as a double, in one call.
    double num( const std::string &key /**< [in] member name */,
                double def = 0 /**< [in] default if absent */ ) const;

    /// A member of this object as an int, in one call.
    int integer( const std::string &key /**< [in] member name */, int def = 0 /**< [in] default if absent */ ) const;

    /// A member of this object as a string, in one call.
    std::string str( const std::string &key /**< [in] member name */,
                     const std::string &def = std::string() /**< [in] default if absent */ ) const;

    /// A member of this object as a bool, in one call.
    bool boolean( const std::string &key /**< [in] member name */,
                  bool def = false /**< [in] default if absent */ ) const;

    /// Collect an array of strings, tolerating a bare string in place of a one element array.
    /** The visit file writes several single valued fields as one element arrays,
     * so both spellings are accepted.
     *
     * \returns the number of entries appended
     */
    size_t asStringArray( std::vector<std::string> &out /**< [out] strings, appended */ ) const;

    /// The shared null value returned by failed lookups.
    static const jsonValue &nullValue();

    friend class jsonParser;
};

/// Recursive descent JSON parser.
/** \ingroup wccCommon
 */
class jsonParser
{
    /** \name Parse State - Data
     *@{
     */
  protected:
    const char *m_p{ nullptr }; ///< Current read position.

    const char *m_end{ nullptr }; ///< One past the last character.

    std::string m_error; ///< Human readable description of the first failure.
    ///@}

  public:
    /// Parse a JSON document from memory.
    /** \returns 0 on success
     * \returns -1 on a syntax error, with error() describing it
     */
    int parse( const std::string &text /**< [in] the document */,
               jsonValue &out /**< [out] the parsed root value */ );

    /// Description of the first parse failure.
    const std::string &error() const;

  protected:
    /// Advance past whitespace.
    void skipWS();

    /// True if input remains.
    bool more() const;

    /// Record a parse error, if one is not already recorded.
    /** \returns -1 always, so callers can `return fail(...)`
     */
    int fail( const std::string &what /**< [in] description */ );

    /// Parse any value at the current position.
    int parseValue( jsonValue &out /**< [out] the value */ );

    /// Parse an object, with the opening brace already consumed.
    int parseObject( jsonValue &out /**< [out] the object */ );

    /// Parse an array, with the opening bracket already consumed.
    int parseArray( jsonValue &out /**< [out] the array */ );

    /// Parse a string, with the opening quote already consumed.
    int parseString( std::string &out /**< [out] the decoded string */ );

    /// Parse a number.
    int parseNumber( jsonValue &out /**< [out] the number */ );

    /// Append a code point to a string as UTF-8.
    static void appendUTF8( std::string &s /**< [in,out] target */, uint32_t cp /**< [in] code point */ );
};

/// Read and parse a JSON file.
/** \returns 0 on success
 * \returns -1 if the file cannot be read or does not parse, with err describing why
 *
 * \ingroup wccCommon
 */
inline int jsonParseFile( const std::string &path /**< [in] path to the file */,
                          jsonValue &out /**< [out] the parsed root value */,
                          std::string &err /**< [out] description of any failure */ )
{
    std::ifstream fin( path, std::ios::binary );

    if( !fin.good() )
    {
        err = "cannot open " + path;
        return -1;
    }

    std::ostringstream ss;
    ss << fin.rdbuf();

    jsonParser p;

    if( p.parse( ss.str(), out ) < 0 )
    {
        err = path + ": " + p.error();
        return -1;
    }

    return 0;
}

inline const jsonValue &jsonValue::nullValue()
{
    static const jsonValue nv;
    return nv;
}

inline jsonValue::valueType jsonValue::type() const
{
    return m_type;
}

inline bool jsonValue::isNull() const
{
    return m_type == Null;
}

inline bool jsonValue::isObject() const
{
    return m_type == Object;
}

inline bool jsonValue::isArray() const
{
    return m_type == Array;
}

inline bool jsonValue::isNumber() const
{
    return m_type == Number;
}

inline bool jsonValue::isString() const
{
    return m_type == String;
}

inline bool jsonValue::isBool() const
{
    return m_type == Bool;
}

inline size_t jsonValue::size() const
{
    if( m_type == Array )
    {
        return m_array.size();
    }

    if( m_type == Object )
    {
        return m_object.size();
    }

    return 0;
}

inline const jsonValue &jsonValue::operator[]( const std::string &key ) const
{
    if( m_type != Object )
    {
        return nullValue();
    }

    for( const auto &kv : m_object )
    {
        if( kv.first == key )
        {
            return kv.second;
        }
    }

    return nullValue();
}

inline const jsonValue &jsonValue::operator[]( size_t i ) const
{
    if( m_type != Array || i >= m_array.size() )
    {
        return nullValue();
    }

    return m_array[i];
}

inline const std::string &jsonValue::key( size_t i ) const
{
    static const std::string empty;

    if( m_type != Object || i >= m_object.size() )
    {
        return empty;
    }

    return m_object[i].first;
}

inline const jsonValue &jsonValue::value( size_t i ) const
{
    if( m_type != Object || i >= m_object.size() )
    {
        return nullValue();
    }

    return m_object[i].second;
}

inline bool jsonValue::has( const std::string &key ) const
{
    return !( *this )[key].isNull();
}

inline double jsonValue::asDouble( double def ) const
{
    if( m_type == Number )
    {
        return m_number;
    }

    if( m_type == Bool )
    {
        return m_bool ? 1.0 : 0.0;
    }

    if( m_type == String )
    {
        try
        {
            return std::stod( m_string );
        }
        catch( ... )
        {
            return def;
        }
    }

    return def;
}

inline int jsonValue::asInt( int def ) const
{
    if( m_type == Null || m_type == Array || m_type == Object )
    {
        return def;
    }

    const double v = asDouble( static_cast<double>( def ) );

    if( !isFinite( v ) )
    {
        return def;
    }

    return static_cast<int>( std::llround( v ) );
}

inline bool jsonValue::asBool( bool def ) const
{
    if( m_type == Bool )
    {
        return m_bool;
    }

    if( m_type == Number )
    {
        return m_number != 0;
    }

    if( m_type == String )
    {
        return m_string == "true" || m_string == "TRUE" || m_string == "yes" || m_string == "YES" ||
               m_string == "1" || m_string == "on" || m_string == "ON";
    }

    return def;
}

inline std::string jsonValue::asString( const std::string &def ) const
{
    if( m_type == String )
    {
        return m_string;
    }

    if( m_type == Bool )
    {
        return m_bool ? "true" : "false";
    }

    if( m_type == Number )
    {
        std::ostringstream ss;
        ss << m_number;
        return ss.str();
    }

    return def;
}

inline double jsonValue::num( const std::string &key, double def ) const
{
    const jsonValue &v = ( *this )[key];
    return v.isNull() ? def : v.asDouble( def );
}

inline int jsonValue::integer( const std::string &key, int def ) const
{
    const jsonValue &v = ( *this )[key];
    return v.isNull() ? def : v.asInt( def );
}

inline std::string jsonValue::str( const std::string &key, const std::string &def ) const
{
    const jsonValue &v = ( *this )[key];
    return v.isNull() ? def : v.asString( def );
}

inline bool jsonValue::boolean( const std::string &key, bool def ) const
{
    const jsonValue &v = ( *this )[key];
    return v.isNull() ? def : v.asBool( def );
}

inline size_t jsonValue::asStringArray( std::vector<std::string> &out ) const
{
    if( m_type == String )
    {
        out.push_back( m_string );
        return 1;
    }

    if( m_type != Array )
    {
        return 0;
    }

    size_t n = 0;
    for( const jsonValue &v : m_array )
    {
        if( v.isNull() || v.isArray() || v.isObject() )
        {
            continue;
        }

        out.push_back( v.asString() );
        ++n;
    }

    return n;
}

inline int jsonParser::parse( const std::string &text, jsonValue &out )
{
    m_p = text.c_str();
    m_end = m_p + text.size();
    m_error.clear();

    out = jsonValue();

    skipWS();

    if( parseValue( out ) < 0 )
    {
        return -1;
    }

    skipWS();

    if( more() )
    {
        return fail( "trailing content after the root value" );
    }

    return 0;
}

inline const std::string &jsonParser::error() const
{
    return m_error;
}

inline bool jsonParser::more() const
{
    return m_p < m_end;
}

inline void jsonParser::skipWS()
{
    while( m_p < m_end && ( *m_p == ' ' || *m_p == '\t' || *m_p == '\n' || *m_p == '\r' ) )
    {
        ++m_p;
    }
}

inline int jsonParser::fail( const std::string &what )
{
    if( m_error.empty() )
    {
        m_error = what;
    }

    return -1;
}

inline int jsonParser::parseValue( jsonValue &out )
{
    skipWS();

    if( !more() )
    {
        return fail( "unexpected end of input" );
    }

    const char c = *m_p;

    if( c == '{' )
    {
        ++m_p;
        return parseObject( out );
    }

    if( c == '[' )
    {
        ++m_p;
        return parseArray( out );
    }

    if( c == '"' )
    {
        ++m_p;
        out.m_type = jsonValue::String;
        return parseString( out.m_string );
    }

    if( c == 't' )
    {
        if( m_end - m_p < 4 || std::string( m_p, 4 ) != "true" )
        {
            return fail( "malformed literal, expected true" );
        }
        m_p += 4;
        out.m_type = jsonValue::Bool;
        out.m_bool = true;
        return 0;
    }

    if( c == 'f' )
    {
        if( m_end - m_p < 5 || std::string( m_p, 5 ) != "false" )
        {
            return fail( "malformed literal, expected false" );
        }
        m_p += 5;
        out.m_type = jsonValue::Bool;
        out.m_bool = false;
        return 0;
    }

    if( c == 'n' )
    {
        if( m_end - m_p < 4 || std::string( m_p, 4 ) != "null" )
        {
            return fail( "malformed literal, expected null" );
        }
        m_p += 4;
        out.m_type = jsonValue::Null;
        return 0;
    }

    if( c == '-' || ( c >= '0' && c <= '9' ) )
    {
        return parseNumber( out );
    }

    return fail( std::string( "unexpected character '" ) + c + "'" );
}

inline int jsonParser::parseObject( jsonValue &out )
{
    out.m_type = jsonValue::Object;
    out.m_object.clear();

    skipWS();

    if( more() && *m_p == '}' )
    {
        ++m_p;
        return 0;
    }

    while( true )
    {
        skipWS();

        if( !more() || *m_p != '"' )
        {
            return fail( "expected a quoted member name in an object" );
        }
        ++m_p;

        std::string name;
        if( parseString( name ) < 0 )
        {
            return -1;
        }

        skipWS();

        if( !more() || *m_p != ':' )
        {
            return fail( "expected ':' after the member name '" + name + "'" );
        }
        ++m_p;

        jsonValue v;
        if( parseValue( v ) < 0 )
        {
            return -1;
        }

        out.m_object.emplace_back( name, std::move( v ) );

        skipWS();

        if( !more() )
        {
            return fail( "unterminated object" );
        }

        if( *m_p == ',' )
        {
            ++m_p;
            continue;
        }

        if( *m_p == '}' )
        {
            ++m_p;
            return 0;
        }

        return fail( "expected ',' or '}' in an object" );
    }
}

inline int jsonParser::parseArray( jsonValue &out )
{
    out.m_type = jsonValue::Array;
    out.m_array.clear();

    skipWS();

    if( more() && *m_p == ']' )
    {
        ++m_p;
        return 0;
    }

    while( true )
    {
        jsonValue v;
        if( parseValue( v ) < 0 )
        {
            return -1;
        }

        out.m_array.push_back( std::move( v ) );

        skipWS();

        if( !more() )
        {
            return fail( "unterminated array" );
        }

        if( *m_p == ',' )
        {
            ++m_p;
            continue;
        }

        if( *m_p == ']' )
        {
            ++m_p;
            return 0;
        }

        return fail( "expected ',' or ']' in an array" );
    }
}

inline void jsonParser::appendUTF8( std::string &s, uint32_t cp )
{
    if( cp < 0x80 )
    {
        s.push_back( static_cast<char>( cp ) );
    }
    else if( cp < 0x800 )
    {
        s.push_back( static_cast<char>( 0xC0 | ( cp >> 6 ) ) );
        s.push_back( static_cast<char>( 0x80 | ( cp & 0x3F ) ) );
    }
    else if( cp < 0x10000 )
    {
        s.push_back( static_cast<char>( 0xE0 | ( cp >> 12 ) ) );
        s.push_back( static_cast<char>( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) );
        s.push_back( static_cast<char>( 0x80 | ( cp & 0x3F ) ) );
    }
    else
    {
        s.push_back( static_cast<char>( 0xF0 | ( cp >> 18 ) ) );
        s.push_back( static_cast<char>( 0x80 | ( ( cp >> 12 ) & 0x3F ) ) );
        s.push_back( static_cast<char>( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) );
        s.push_back( static_cast<char>( 0x80 | ( cp & 0x3F ) ) );
    }
}

inline int jsonParser::parseString( std::string &out )
{
    out.clear();

    while( more() )
    {
        const char c = *m_p++;

        if( c == '"' )
        {
            return 0;
        }

        if( c != '\\' )
        {
            out.push_back( c );
            continue;
        }

        if( !more() )
        {
            return fail( "unterminated escape in a string" );
        }

        const char e = *m_p++;

        switch( e )
        {
        case '"':
            out.push_back( '"' );
            break;
        case '\\':
            out.push_back( '\\' );
            break;
        case '/':
            out.push_back( '/' );
            break;
        case 'b':
            out.push_back( '\b' );
            break;
        case 'f':
            out.push_back( '\f' );
            break;
        case 'n':
            out.push_back( '\n' );
            break;
        case 'r':
            out.push_back( '\r' );
            break;
        case 't':
            out.push_back( '\t' );
            break;
        case 'u':
        {
            if( m_end - m_p < 4 )
            {
                return fail( "truncated \\u escape" );
            }

            uint32_t cp = 0;
            for( int i = 0; i < 4; ++i )
            {
                const char h = *m_p++;
                cp <<= 4;

                if( h >= '0' && h <= '9' )
                {
                    cp |= static_cast<uint32_t>( h - '0' );
                }
                else if( h >= 'a' && h <= 'f' )
                {
                    cp |= static_cast<uint32_t>( h - 'a' + 10 );
                }
                else if( h >= 'A' && h <= 'F' )
                {
                    cp |= static_cast<uint32_t>( h - 'A' + 10 );
                }
                else
                {
                    return fail( "non-hex digit in a \\u escape" );
                }
            }

            // Combine a surrogate pair if one follows.
            if( cp >= 0xD800 && cp <= 0xDBFF && m_end - m_p >= 6 && m_p[0] == '\\' && m_p[1] == 'u' )
            {
                uint32_t lo = 0;
                bool ok = true;

                for( int i = 0; i < 4; ++i )
                {
                    const char h = m_p[2 + i];
                    lo <<= 4;

                    if( h >= '0' && h <= '9' )
                    {
                        lo |= static_cast<uint32_t>( h - '0' );
                    }
                    else if( h >= 'a' && h <= 'f' )
                    {
                        lo |= static_cast<uint32_t>( h - 'a' + 10 );
                    }
                    else if( h >= 'A' && h <= 'F' )
                    {
                        lo |= static_cast<uint32_t>( h - 'A' + 10 );
                    }
                    else
                    {
                        ok = false;
                        break;
                    }
                }

                if( ok && lo >= 0xDC00 && lo <= 0xDFFF )
                {
                    cp = 0x10000 + ( ( cp - 0xD800 ) << 10 ) + ( lo - 0xDC00 );
                    m_p += 6;
                }
            }

            appendUTF8( out, cp );
            break;
        }
        default:
            return fail( std::string( "unrecognized escape '\\" ) + e + "'" );
        }
    }

    return fail( "unterminated string" );
}

inline int jsonParser::parseNumber( jsonValue &out )
{
    const char *start = m_p;

    if( more() && *m_p == '-' )
    {
        ++m_p;
    }

    while( more() && *m_p >= '0' && *m_p <= '9' )
    {
        ++m_p;
    }

    if( more() && *m_p == '.' )
    {
        ++m_p;
        while( more() && *m_p >= '0' && *m_p <= '9' )
        {
            ++m_p;
        }
    }

    if( more() && ( *m_p == 'e' || *m_p == 'E' ) )
    {
        ++m_p;
        if( more() && ( *m_p == '+' || *m_p == '-' ) )
        {
            ++m_p;
        }
        while( more() && *m_p >= '0' && *m_p <= '9' )
        {
            ++m_p;
        }
    }

    const std::string text( start, static_cast<size_t>( m_p - start ) );

    try
    {
        out.m_number = std::stod( text );
    }
    catch( ... )
    {
        return fail( "malformed number '" + text + "'" );
    }

    out.m_type = jsonValue::Number;

    return 0;
}

} // namespace wcc
} // namespace MagAOX

#endif // wccJSON_hpp
