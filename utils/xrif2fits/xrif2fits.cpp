/** \file xrif2fits.cpp
 * \brief The xrif2fits main program.
 *
 * \ingroup xrif2fits_files
 */

#include "xrif2fits.hpp"

// Extracts the explanatory string of an exception, placing it in a vector.
// If the exception is nested, this recurses to extract the explanatory string of the
// next exception it holds.
void unwind_exceptions( std::vector<std::string> & whats, const std::exception &e )
{
    whats.push_back(e.what());

    try
    {
        std::rethrow_if_nested( e );
    }
    catch( const std::exception &nestedException )
    {
        unwind_exceptions( whats, nestedException);
    }
    catch( ... )
    {
    }
}

void print_exceptions(std::vector<std::string> & whats)
{
    std::cerr << "xrif2fits error. run with -h to seek help.\nexplanation:\n";
    std::cerr << "  " << whats.back() << '\n';
    for(size_t n = 1; n < whats.size(); ++n)
    {
        std::cerr << std::string(2, ' ') << std::string((n-1)*4, ' ') << "|-->" << whats[whats.size()-1-n] << '\n';
    }
}

int main( int argc, char **argv )
{
    try
    {
        xrif2fits * xs;
        try
        {
            xs = new xrif2fits;
        }
        catch( const std::exception &e )
        {
            std::throw_with_nested( MagAOX::xwcException( "error constructing xrif2fits" ) );
        }

        try
        {
            return xs->main( argc, argv );
        }
        catch( ... )
        {
            std::throw_with_nested( MagAOX::xwcException( "error from mx::app::main" ) );
        }

        try
        {
            delete xs;
        }
        catch( ... )
        {
            std::throw_with_nested( MagAOX::xwcException( "error during destruction" ) );
        }
    }
    catch( const std::exception &e )
    {
        std::vector<std::string> whats;
        unwind_exceptions(whats, e );
        print_exceptions(whats);

        return -1;
    }
}
