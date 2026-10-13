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

        XWCTEST_SETUP_INDI_NEW_PROP(adcClkDivider);
        XWCTEST_SETUP_INDI_NEW_PROP(adcSamplesToAverage);
        XWCTEST_SETUP_INDI_NEW_PROP(adcMaxAccums);
        XWCTEST_SETUP_INDI_NEW_PROP(chop);
    }

    /// Test access to configFieldReplace for the samples-to-average setting (bits 16-31 of AdcConfig).
    uint32_t replaceSamplesToAverage(uint32_t reg, uint16_t value)
    {
        return configFieldReplace(m_adcSamplesToAverage, reg, value);
    }

    /// Test access to configFieldReplace for the clock divider setting (bits 0-15 of AdcConfig).
    uint32_t replaceClkDivider(uint32_t reg, uint16_t value)
    {
        return configFieldReplace(m_adcClkDivider, reg, value);
    }

    /// Test access to configFieldValue for the samples-to-average setting.
    uint16_t samplesToAverage(uint32_t reg)
    {
        return configFieldValue(m_adcSamplesToAverage, reg);
    }

    /// Test access to configFieldValue for the clock divider setting.
    uint16_t clkDivider(uint32_t reg)
    {
        return configFieldValue(m_adcClkDivider, reg);
    }

    /// Test access to configFieldReplace for the chop bit (bit 15 of the hardware control register).
    uint32_t replaceChop(uint32_t reg, uint16_t value)
    {
        return configFieldReplace(m_chop, reg, value);
    }

    /// Test access to configFieldValue for the chop bit.
    uint16_t chop(uint32_t reg)
    {
        return configFieldValue(m_chop, reg);
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

    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, adcClkDivider);
    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, adcSamplesToAverage);
    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, adcMaxAccums);
    XWCTEST_INDI_NEW_CALLBACK( fsmCtrl, chop);
}

/// Reading and replacing one 16-bit A/D setting in a 32-bit configuration register
/** Changing one half must leave the other half unchanged (e.g. MonitorAdcMaxAccums when writing ControlAdcMaxAccums).
 */
SCENARIO( "A/D configuration register halves", "[fsmCtrl]" )
{
    GIVEN("The fsm's default AdcConfig register: clock divider 255 (low), samples to average 65535 (high)")
    {
        fsmCtrl_test fc("fsmCtrl");
        uint32_t reg = 0xFFFF00FF;

        WHEN("Reading each setting")
        {
            REQUIRE(fc.clkDivider(reg) == 255);
            REQUIRE(fc.samplesToAverage(reg) == 65535);
        }

        WHEN("Changing samples to average")
        {
            uint32_t newReg = fc.replaceSamplesToAverage(reg, 256);
            REQUIRE(fc.samplesToAverage(newReg) == 256);
            REQUIRE(fc.clkDivider(newReg) == 255);
            REQUIRE(newReg == 0x010000FF);
        }

        WHEN("Changing the clock divider")
        {
            uint32_t newReg = fc.replaceClkDivider(reg, 203);
            REQUIRE(fc.clkDivider(newReg) == 203);
            REQUIRE(fc.samplesToAverage(newReg) == 65535);
            REQUIRE(newReg == 0xFFFF00CB);
        }
    }
}

/// Reading and replacing the single-bit chop setting in the hardware control register
/** Only bit 15 may change; every other bit (power, HV, UART outputs, faults) must be written back as read.
 */
SCENARIO( "Hardware control register chop bit", "[fsmCtrl]" )
{
    GIVEN("A hardware control register with chop on and other bits set")
    {
        fsmCtrl_test fc("fsmCtrl");
        uint32_t reg = 0xA50F8F0F; // bit 15 set

        WHEN("Reading chop")
        {
            REQUIRE(fc.chop(reg) == 1);
        }

        WHEN("Turning chop off")
        {
            uint32_t newReg = fc.replaceChop(reg, 0);
            REQUIRE(fc.chop(newReg) == 0);
            REQUIRE(newReg == (reg & ~0x8000u));
        }

        WHEN("Turning chop on when it is already on")
        {
            REQUIRE(fc.replaceChop(reg, 1) == reg);
        }
    }
}

/// ConfigRegisterQuery stores the register value from a reply and counts replies
SCENARIO( "Configuration register replies", "[fsmCtrl]" )
{
    GIVEN("A ConfigRegisterQuery")
    {
        ConfigRegisterQuery q(CGraphPayloadTypeFSMAdcConfigRegister, "AdcConfigRegister");

        WHEN("A 4-byte reply arrives")
        {
            uint32_t reply = 0xFFFF00FF;
            q.processReply(reinterpret_cast<const char *>(&reply), sizeof(reply));

            REQUIRE(q.Value == 0xFFFF00FF);
            REQUIRE(q.m_replyCount.load() == 1);
        }

        WHEN("The query is used to write")
        {
            uint32_t value = 0x010000FF;
            q.setPayload(&value, sizeof(value));
            REQUIRE(q.getPayloadLen() == sizeof(uint32_t));
            REQUIRE(q.getPayloadData() == &value);

            q.resetPayload();
            REQUIRE(q.getPayloadLen() == 0);
        }
    }
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
