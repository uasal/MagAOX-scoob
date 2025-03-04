/** \file fsmCommands.hpp
 * \brief Utility file for the fsmCtrl app with structre and class definitions for commands to be sent to the fsm
 *
 * \ingroup fsmCtrl_files
 */

#pragma once

#include <iostream>
#include <sstream> // for stringstreams
#include <cstddef> // for nullptr
using namespace std;

namespace MagAOX
{
    namespace app
    {
        namespace dev
        {
            /**
             * @brief Base class for all the fsm queries
             *
             * sdevQuery is the class from which all the query classes inherit.
             * It ensures that the all implement a minimal interfaces that includes
             * processReply, logReply and errorLogString.
             */
            class sdevQuery
            {
            public:
                std::string startLog = "";
                std::string endLog = "";

                virtual ~sdevQuery() = default;
                virtual void errorLogString(const size_t ParamsLen) = 0;
                virtual void processReply(char const *Params, const size_t ParamsLen) = 0;
                virtual void logReply() = 0;
                virtual uint16_t getPayloadType() const
                {
                    return PayloadType;
                }
                virtual void *getPayloadData() const
                {
                    return PayloadData;
                }
                virtual uint16_t getPayloadLen() const
                {
                    return PayloadLen;
                }
                virtual void setPayload(void *newPayloadData, uint16_t newPayloadLen)
                {
                    PayloadData = newPayloadData;
                    PayloadLen = newPayloadLen;
                }
                virtual void resetPayload()
                {
                    PayloadData = DefaultPayloadData;
                    PayloadLen = DefaultPayloadLen;
                }

            protected:
                uint16_t PayloadType = -1;
                void *DefaultPayloadData = NULL;
                size_t DefaultPayloadLen = 0;
                void *PayloadData = DefaultPayloadData;
                size_t PayloadLen = DefaultPayloadLen;
            };
        } // namespace dev
    } // namespace app
} // namespace MagAOX