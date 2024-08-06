/** \file fsmCtrl_test.cpp
  * \brief Catch2 tests for the template app.
  *
  * History:
  */
#include "../../../tests/catch2/catch.hpp"
#include "../../tests/testMacrosINDI.hpp"

#include "../fsmCtrl.hpp"

using namespace MagAOX::app;

namespace FSMTEST 
{

class fsmCtrl_test : public fsmCtrl 
{

public:
    fsmCtrl_test(const std::string device)
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP(val1);
        XWCTEST_SETUP_INDI_NEW_PROP(val2);
        XWCTEST_SETUP_INDI_NEW_PROP(val3);

        XWCTEST_SETUP_INDI_NEW_PROP(dac1);
        XWCTEST_SETUP_INDI_NEW_PROP(dac2);
        XWCTEST_SETUP_INDI_NEW_PROP(dac3);

        XWCTEST_SETUP_INDI_NEW_PROP(conversion_factors);
        XWCTEST_SETUP_INDI_NEW_PROP(input);
        XWCTEST_SETUP_INDI_NEW_PROP(query);
    }
};


SCENARIO( "INDI Callbacks", "[fsmCtrl]" )
{
    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, val1);
    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, val2);
    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, val3);

    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, dac1);
    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, dac2);
    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, dac3);

    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, conversion_factors);
    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, input);
    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, query);
}



// SCENARIO("")
// {
//    GIVEN("")
//    {
//       WHEN("")
//       {
//         REQUIRE();
//         REQUIRE();
//         REQUIRE();
//      }   
//    }
// }


} //namespace FSMTEST 
