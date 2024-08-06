/** \file fsmCtrl_test.cpp
  * \brief Catch2 tests for the template app.
  *
  * History:
  */
#include "../../../tests/catch2/catch.hpp"

#include "../fsmCtrl.hpp"
#include "../binaryUart.hpp"
#include "../iPacket.hpp"
#include "../fsmCommands.hpp"

using namespace MagAOX::app;

namespace binaryUart_test 
{

/*! \brief Mock classes
 */
class MockIUart : public IUart
{
public:
    std::string sentData = ""; // Stream to capture sent data

    // Implement IUart interface methods
    bool dataready() const override { return false; }
    char getcqq() override { return '\0'; }
    char putcqq(char c) override { 
        // // Convert the byte to its hexadecimal representation
        // std::stringstream ss;
        // ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(c);
        // std::string hexByte = ss.str();

        // // Append the hexadecimal representation to sentData
        // sentData += hexByte;
        sentData += c;
        return c;
    }
    void flushoutput() override { }
    void purgeinput() override { }
    bool isopen() const override { return false; }
};

class MockIPacket : public IPacket
{
public:
    bool packetStartFound = false;
    bool packetEndFound = false;
    size_t payloadLen = 0;
    size_t maxPayloadLength = 0;
    size_t offsetChange = 0;
    bool packetIsValid = false;

    // Implement IPacket virtual methods
    bool FindPacketStart(const uint8_t* Buffer, const size_t BufferLen, size_t& Offset) const override { return packetStartFound; }
    bool FindPacketEnd(const uint8_t* Buffer, const size_t BufferLen, size_t& Offset) const override { 
        Offset += offsetChange;
        return packetEndFound; 
    }
    size_t HeaderLen() const override { return 8; }
    size_t FooterLen() const override { return 8; }
    size_t PayloadOffset() const override { return 0; }
    size_t MaxPayloadLength() const override { return maxPayloadLength; }
    size_t PayloadLen(const uint8_t* Buffer, const size_t BufferCount, const size_t PacketStartPos) const override { return payloadLen; }
    bool IsValid(const uint8_t* Buffer, const size_t BufferCount, const size_t PacketStartPos) const override { return packetIsValid; }
    uint64_t SerialNum() const override { return 0; }
	uint64_t PayloadType(const uint8_t* Buffer, const size_t PacketStartPos, const size_t PacketEndPos) const override { return 0; }
	bool DoesPayloadTypeMatch(const uint8_t* Buffer, const size_t BufferCount, const size_t PacketStartPos, const uint32_t CmdType) const override { return false; }
	size_t MakePacket(uint8_t* Buffer, const size_t BufferCount, const void* Payload, const uint16_t PayloadType, const size_t PayloadLen) const override {
        if (NULL != Payload) { memcpy(Buffer, Payload, PayloadLen); } // if Payload is not null, copy Payload into Buffer
        return PayloadLen; 
    }
};

class MockBinaryUartCallbacks : public BinaryUartCallbacks
{
public:
    bool BufferOverflowCalled = false;
    bool InvalidPacketCalled = false;
    bool UnHandledPacketCalled = false;
    bool EveryPacketCalled = false;

    // Implement BinaryUartCallbacks virtual methods
    void InvalidPacket(const uint8_t* Buffer, const size_t& BufferLen) override { InvalidPacketCalled = true; }
    void UnHandledPacket(const IPacket* Packet, const size_t& PacketLen) override { UnHandledPacketCalled = true; }
    void EveryPacket(const IPacket* Packet, const size_t& PacketLen) override { EveryPacketCalled = true; }
    void BufferOverflow(const size_t& BufferLen) override { BufferOverflowCalled = true; }
};

class MockQuery : public PZTQuery
{
public:
    bool processReplyCalled = false;

    // Implement PZTQuery virtual methods
    void errorLogString(const size_t ParamsLen) override { };
    void processReply(char const *Params, const size_t ParamsLen) override { processReplyCalled = true; };
    void logReply() override { };
    uint16_t getPayloadType() const override { return 0; }
    uint16_t getPayloadLen() const override { return 0; }
    void setPayload(void *newPayloadData, uint16_t newPayloadLen) override { }
    void resetPayload() override { }
};


SCENARIO("Creating BinaryUart instance", "[BinaryUart]")
{
    GIVEN("A new BinaryUart instance")
    {
        // Create mock instances to pass to BinaryUart
        MockIUart* pinout = new MockIUart();   
        MockIPacket* packet = new MockIPacket();
        MockBinaryUartCallbacks* callbacks = new MockBinaryUartCallbacks();

        const uint64_t rxCountInit = BinaryUart::RxCountInit;  
        const bool inPacketInit = BinaryUart::InPacketInit;  
        const uint64_t packetStartInit = BinaryUart::PacketStartInit;  
        const uint64_t packetLenInit = BinaryUart::PacketLenInit;  
        const size_t rxBufferLenBytes = BinaryUart::RxBufferLenBytes;  
        const char emptyBufferChar = BinaryUart::EmptyBufferChar;  
        const uint64_t invalidSerialNumber = BinaryUart::InvalidSerialNumber;  

        WHEN("Instance created with an IUart, IPacket, callbacks and a given serialnum.")
        {
            uint64_t serialnum = 0xAAA;
            BinaryUart uart = BinaryUart(*pinout, *packet, *callbacks, serialnum);

            THEN("Instance creation is successful")
            {
                // Verify RxCount state
                REQUIRE(uart.RxCount == rxCountInit);
                // Verify InPacket flag
                REQUIRE(uart.InPacket == inPacketInit);
                // Verify serialnum
                REQUIRE(uart.SerialNum == serialnum);
                // Verify PacketStart
                REQUIRE(uart.PacketStart == packetStartInit);
                // Verify PacketLen
                REQUIRE(uart.PacketLen == packetLenInit);

                // Check if the RxBuffer is empty
                bool isEmpty = true;
                for (size_t i = 0; i < rxBufferLenBytes; ++i)
                {
                    if (uart.RxBuffer[i] != emptyBufferChar)
                    {
                        isEmpty = false;
                        break;
                    }
                }
                // Assert that the RxBuffer is empty
                REQUIRE(isEmpty);                
            }
        }

        WHEN("Instance created with an IUart, IPacket, callbacks and no given serialnum.")
        {
            BinaryUart uart = BinaryUart(*pinout, *packet, *callbacks);

            THEN("Instance creation is successful")
            {
                // Verify RxCount state
                REQUIRE(uart.RxCount == rxCountInit);
                // Verify InPacket flag
                REQUIRE(uart.InPacket == inPacketInit);
                // Verify serialnum
                REQUIRE(uart.SerialNum == invalidSerialNumber);
                // Verify PacketStart
                REQUIRE(uart.PacketStart == packetStartInit);
                // Verify PacketLen
                REQUIRE(uart.PacketLen == packetLenInit);

                // Check if the RxBuffer is empty
                bool isEmpty = true;
                for (size_t i = 0; i < rxBufferLenBytes; ++i)
                {
                    if (uart.RxBuffer[i] != emptyBufferChar)
                    {
                        isEmpty = false;
                        break;
                    }
                }
                // Assert that the RxBuffer is empty
                REQUIRE(isEmpty);                       
            }
        }

    }
}

/// TODO: Would really need to test Init on its own, not only when called in constructor


// Test cases for BinaryUart::ProcessByte method
SCENARIO("Testing ProcessByte", "[BinaryUart]")
{
    GIVEN("An instance of BinaryUart and a new constant character")
    {
        // Create mock instances to pass to BinaryUart
        MockIUart* pinout = new MockIUart();   
        MockIPacket* packet = new MockIPacket();
        MockBinaryUartCallbacks* callbacks = new MockBinaryUartCallbacks();
        
        const uint64_t rxCountInit = BinaryUart::RxCountInit;  
        const bool inPacketInit = BinaryUart::InPacketInit;  
        const uint64_t packetStartInit = BinaryUart::PacketStartInit;  
        const uint64_t packetLenInit = BinaryUart::PacketLenInit;  
        const size_t rxBufferLenBytes = BinaryUart::RxBufferLenBytes;  
        const char emptyBufferChar = BinaryUart::EmptyBufferChar;  

        BinaryUart uart = BinaryUart(*pinout, *packet, *callbacks);

        WHEN("The buffer is empty")
        {
            const char testChar = 'a';

            uart.ProcessByte(testChar);

            THEN("The new character is added to the buffer on the first position")
            {
                // Verify RxCount state
                REQUIRE(uart.RxCount == (rxCountInit + 1));                
                // Verify Buffer has the new character
                REQUIRE(uart.RxBuffer[rxCountInit] == testChar);                
                // Verify InPacket flag
                REQUIRE(uart.InPacket == inPacketInit);              
            }
        }
        WHEN("The buffer is neither empty nor full")
        {
            uint64_t start = uart.RxCount = 20;
            const char testChar = 'a';

            uart.ProcessByte(testChar);
            
            THEN("The new character is added to the buffer on the next available position")
            {
                // We're not testing the case where we're out of buffer bounds
                REQUIRE(start < sizeof(uart.RxBuffer));    

                // Verify RxCount state
                REQUIRE(uart.RxCount == (start + 1));
                // Verify Buffer has the new character
                REQUIRE(uart.RxBuffer[start] == testChar);
                // Verify InPacket flag
                REQUIRE(uart.InPacket == inPacketInit);                 
            }
        }
        WHEN("The buffer is full")
        {
            uint64_t start = uart.RxCount = sizeof(uart.RxBuffer);
            const uint64_t serialnum = uart.SerialNum;
            const char testChar = 'a';

            // Fill buffer
            for (size_t i = 0; i < rxBufferLenBytes; ++i) { uart.RxBuffer[i] = 'a'; }

            uart.ProcessByte(testChar);

            THEN("BufferOverflow is called and the buffer is reset")
            {
                // We're testing the case where we're out of buffer bounds
                REQUIRE(start >= sizeof(uart.RxBuffer));    
                 
                // Verify BufferOverflow called
                REQUIRE(callbacks->BufferOverflowCalled);      

                // Verify serialnum
                REQUIRE(uart.SerialNum == serialnum);
                // Verify RxCount state
                REQUIRE(uart.RxCount == rxCountInit);
                // Verify PacketStart
                REQUIRE(uart.PacketStart == packetStartInit);
                // Verify PacketLen
                REQUIRE(uart.PacketLen == packetLenInit);         
                // Verify InPacket flag
                REQUIRE(uart.InPacket == inPacketInit);                             

                // Check if the RxBuffer is empty
                bool isEmpty = true;
                for (size_t i = 0; i < rxBufferLenBytes; ++i)
                {
                    if (uart.RxBuffer[i] != emptyBufferChar)
                    {
                        isEmpty = false;
                        break;
                    }
                }
                // Assert that the RxBuffer is empty
                REQUIRE(isEmpty);                                     
            }
        }
    }
}


// Test cases for BinaryUart::CheckPacketStart method
SCENARIO("Testing CheckPacketStart", "[BinaryUart]")
{
    GIVEN("An instance of BinaryUart")
    {    
        // Create mock instances to pass to BinaryUart
        MockIUart* pinout = new MockIUart();   
        MockIPacket* packet = new MockIPacket();
        MockBinaryUartCallbacks* callbacks = new MockBinaryUartCallbacks();

        BinaryUart uart = BinaryUart(*pinout, *packet, *callbacks);

        WHEN("Not in packet and in header position")
        {
            uart.InPacket = false;
            uart.RxCount = (packet->HeaderLen() - 1);

            uart.CheckPacketStart();

            THEN("Packet start not found")
            {
                REQUIRE(uart.InPacket == false);
            }
        }

        WHEN("In packet and after header")
        {
            uart.InPacket = true;
            uart.RxCount = (packet->HeaderLen() + 1);

            uart.CheckPacketStart();

            THEN("Packet start not here (found before)")
            {
                REQUIRE(uart.InPacket == true);
            }
        }

        WHEN("Not in packet and after header, but packet start not marked as expected")
        {
            uart.InPacket = false;
            uart.RxCount = (packet->HeaderLen() + 1);

            uart.CheckPacketStart();

            THEN("Packet start not found")
            {
                REQUIRE(uart.InPacket == false);
            }
        }

        WHEN("Not in packet and after header, and packet start marked as expected")
        {
            uart.InPacket = false;
            uart.RxCount = (packet->HeaderLen() + 1);
            packet->packetStartFound = true;

            uart.CheckPacketStart();

            THEN("Packet start found")
            {
                REQUIRE(uart.InPacket == true);
            }
        }        
    }
}


// Test cases for BinaryUart::CheckPacketEnd method
SCENARIO("Testing CheckPacketEnd", "[BinaryUart]")
{
    GIVEN("An instance of BinaryUart")
    {    
        // Create mock instances
        MockIUart* pinout = new MockIUart();   
        MockIPacket* packet = new MockIPacket();
        MockBinaryUartCallbacks* callbacks = new MockBinaryUartCallbacks();
        MockQuery* query = new MockQuery();

        const uint64_t rxCountInit = BinaryUart::RxCountInit;  
        const bool inPacketInit = BinaryUart::InPacketInit;  
        const uint64_t packetStartInit = BinaryUart::PacketStartInit;  
        const uint64_t packetLenInit = BinaryUart::PacketLenInit;  
        const size_t rxBufferLenBytes = BinaryUart::RxBufferLenBytes;  
        const char emptyBufferChar = BinaryUart::EmptyBufferChar;  
        const uint64_t invalidSerialNumber = BinaryUart::InvalidSerialNumber;  

        BinaryUart uart = BinaryUart(*pinout, *packet, *callbacks);

        WHEN("Not in packet, but RxCount larger or equal than header + footer lengths")
        {
            uart.RxCount = packet->HeaderLen() + packet->FooterLen() + 1;

            THEN("Packet end not found. Keep looking")            
            {
                REQUIRE_FALSE(uart.CheckPacketEnd(query));
            }
        }

        WHEN("In packet, but RxCount less than header + footer lengths")
        {
            uart.InPacket = true;
            uart.RxCount = packet->HeaderLen() + packet->FooterLen() - 1;

            THEN("Packet end not found. Keep looking")
            {
                REQUIRE_FALSE(uart.CheckPacketEnd(query));
            }
        }

        WHEN("In packet and RxCount larger or equal than header + footer lengths, but packet end not marked as expected")
        {
            uart.InPacket = true;
            uart.RxCount = packet->HeaderLen() + packet->FooterLen() + 1; 
            packet->packetEndFound = false;                   

            THEN("Packet end not found. Keep looking")
            {
                REQUIRE_FALSE(uart.CheckPacketEnd(query));
            }
        }

        WHEN("In packet and RxCount larger or equal than header + footer lengths; packet end marked as expected, but packet shorter than expected."
            "\nUnrealistic payload len")
        {
            uart.InPacket = true; 
            packet->packetEndFound = true;

            packet->payloadLen = rxBufferLenBytes + 1;
            uart.RxCount = packet->HeaderLen() + packet->FooterLen() + packet->PayloadLen(uart.RxBuffer, uart.RxCount, uart.PacketStart) - 1;

            // Fill buffer
            for (size_t i = 0; i < uart.RxBufferLenBytes; ++i) { uart.RxBuffer[i] = 'a'; }

            THEN("Invalid packet callback. Buffer reset.")
            {
                REQUIRE_FALSE(uart.CheckPacketEnd(query));

                // Verify InvalidPacket called
                REQUIRE(callbacks->InvalidPacketCalled);

                // Verify serialnum
                REQUIRE(uart.SerialNum == invalidSerialNumber);
                // Verify RxCount state
                REQUIRE(uart.RxCount == rxCountInit);
                // Verify PacketStart
                REQUIRE(uart.PacketStart == packetStartInit);
                // Verify PacketLen
                REQUIRE(uart.PacketLen == packetLenInit);         
                // Verify InPacket flag
                REQUIRE(uart.InPacket == inPacketInit);                             

                // Check if the RxBuffer is empty
                bool isEmpty = true;
                for (size_t i = 0; i < rxBufferLenBytes; ++i)
                {
                    if (uart.RxBuffer[i] != emptyBufferChar)
                    {
                        isEmpty = false;
                        break;
                    }
                }
                // Assert that the RxBuffer is empty
                REQUIRE(isEmpty);                   
            }
        }        

        WHEN("In packet and RxCount larger or equal than header + footer lengths; packet end marked as expected, but packet shorter than expected."
            "\nPayload mis-identified as footer")
        {
            uart.InPacket = true; 
            packet->packetEndFound = true;

            packet->maxPayloadLength = 0;
            packet->payloadLen = rxBufferLenBytes - 1;
            uart.RxCount = packet->HeaderLen() + packet->FooterLen() + packet->PayloadLen(uart.RxBuffer, uart.RxCount, uart.PacketStart) - 1;            
            
            THEN("Packet end not found. Keep looking")
            {
                REQUIRE_FALSE(uart.CheckPacketEnd(query));
            }
        }        

        WHEN("Packet end found (all's good) but packet is invalid.")
        {
            uart.InPacket = true; 
            packet->packetEndFound = true;

            uart.RxCount = packet->HeaderLen() + packet->FooterLen() + packet->PayloadLen(uart.RxBuffer, uart.RxCount, uart.PacketStart);   

            THEN("Invalid packet callback.")
            {
                REQUIRE_FALSE(uart.CheckPacketEnd(query));

                // Verify InvalidPacket called
                REQUIRE(callbacks->InvalidPacketCalled);
            }
        }

        WHEN("Packet end found (all's good), packet is valid and serial num matches.")
        {
            uart.InPacket = true; 
            packet->packetEndFound = true;
            packet->packetIsValid = true;

            uart.RxCount = packet->HeaderLen() + packet->FooterLen() + packet->PayloadLen(uart.RxBuffer, uart.RxCount, uart.PacketStart);

            THEN("Processing reply.")
            {
                REQUIRE(uart.CheckPacketEnd(query));  

                // Verify processReply called
                REQUIRE(query->processReplyCalled);

                // Verify everyPacket called
                REQUIRE(callbacks->EveryPacketCalled);
            }
        }

        WHEN("Packet end found (all's good), packet is valid, but serial num doesn't match.")
        {

            uint64_t serialnum = 0xAAA;
            BinaryUart uart = BinaryUart(*pinout, *packet, *callbacks, serialnum);            

            uart.InPacket = true; 
            packet->packetEndFound = true;
            packet->packetIsValid = true;

            uart.RxCount = packet->HeaderLen() + packet->FooterLen() + packet->PayloadLen(uart.RxBuffer, uart.RxCount, uart.PacketStart);     

            THEN("Unhandled packet called.")
            {
                REQUIRE_FALSE(uart.CheckPacketEnd(query));  

                // Verify unhandled packet called
                REQUIRE(callbacks->UnHandledPacketCalled);

                // Verify everyPacket called
                REQUIRE(callbacks->EveryPacketCalled);                
            }
        }

        WHEN("Packet end found (all's good), packet validity irrelevant. More data in buffer after packet end.")
        {
            uart.InPacket = true; 
            packet->packetEndFound = true;

            uart.RxCount = packet->HeaderLen() + packet->FooterLen() + packet->PayloadLen(uart.RxBuffer, uart.RxCount, uart.PacketStart);   
            uint16_t rxCount = uart.RxCount;
            
            const char testChar = 'a';
            // Set buffer to 00000000aaaaaaaaa0000....
            for (size_t i = 8; i < uart.RxCount; i++) { uart.RxBuffer[i] = testChar; }

            uart.CheckPacketEnd(query);      

            THEN("Data moved to beginning of buffer")
            {
                REQUIRE_FALSE(uart.InPacket);

                REQUIRE(uart.RxCount == (rxCount - 4));

                // Check if the RxBuffer matches 0000aaaaaaaa000....
                bool asExpected = true;
                for (size_t i = 0; i < rxBufferLenBytes; ++i)
                {
                    if ((i>=4) && i < uart.RxCount){
                        if (uart.RxBuffer[i] != testChar)
                        {
                            asExpected = false;
                            break;
                        }
                    } else {
                        if (uart.RxBuffer[i] != emptyBufferChar)
                        {
                            asExpected = false;
                            break;
                        }                        
                    }
                }
                // Assert that the RxBuffer is empty
                REQUIRE(asExpected);
            }
        }

        WHEN("Packet end found (all's good), packet validity irrelevant. No more data in buffer.")
        {
            uart.InPacket = true; 
            packet->packetEndFound = true;
            packet->offsetChange = 2 * (packet->HeaderLen() + packet->FooterLen() + packet->PayloadLen(uart.RxBuffer, uart.RxCount, uart.PacketStart));

            uart.RxCount = packet->HeaderLen() + packet->FooterLen() + packet->PayloadLen(uart.RxBuffer, uart.RxCount, uart.PacketStart);

            const char testChar = 'a';
            // Set buffer to 00000000aaaaaaaaa0000....
            for (size_t i = 8; i < uart.RxCount; i++) { uart.RxBuffer[i] = testChar; }

            uart.CheckPacketEnd(query);

            THEN("Buffer reset.")
            {
                // Verify serialnum
                REQUIRE(uart.SerialNum == invalidSerialNumber);
                // Verify RxCount state
                REQUIRE(uart.RxCount == rxCountInit);
                // Verify PacketStart
                REQUIRE(uart.PacketStart == packetStartInit);
                // Verify PacketLen
                REQUIRE(uart.PacketLen == packetLenInit);         
                // Verify InPacket flag
                REQUIRE(uart.InPacket == inPacketInit);                             

                // Check if the RxBuffer is empty
                bool isEmpty = true;
                for (size_t i = 0; i < rxBufferLenBytes; ++i)
                {
                    if (uart.RxBuffer[i] != emptyBufferChar)
                    {
                        isEmpty = false;
                        break;
                    }
                }
                // Assert that the RxBuffer is empty
                REQUIRE(isEmpty);  
            }
        }

    }
}


// Test cases for BinaryUart::CheckPacketEnd method
SCENARIO("Testing TxBinaryPacket", "[BinaryUart]")
{
    GIVEN("An instance of BinaryUart")
    {    
        // Create mock instances
        MockIUart* pinout = new MockIUart();   
        MockIPacket* packet = new MockIPacket();
        MockBinaryUartCallbacks* callbacks = new MockBinaryUartCallbacks();
        MockQuery* query = new MockQuery();

        const uint64_t rxCountInit = BinaryUart::RxCountInit;  
        const bool inPacketInit = BinaryUart::InPacketInit;  
        const uint64_t packetStartInit = BinaryUart::PacketStartInit;  
        const uint64_t packetLenInit = BinaryUart::PacketLenInit;  
        const size_t rxBufferLenBytes = BinaryUart::RxBufferLenBytes;  
        const char emptyBufferChar = BinaryUart::EmptyBufferChar;  
        const uint64_t invalidSerialNumber = BinaryUart::InvalidSerialNumber;  

        BinaryUart uart = BinaryUart(*pinout, *packet, *callbacks);

        WHEN("Called with given PayloadType, pointer to start of PayloadData and PayloadLen")
        {
            const uint16_t payloadType = 0x00000;
            const char testChar = 'a';
            uint8_t payloadData[3] = {static_cast<uint8_t>(testChar), static_cast<uint8_t>(testChar), static_cast<uint8_t>(testChar)};
            size_t payloadLen = 3 * sizeof(uint8_t);

            uart.TxBinaryPacket(payloadType, reinterpret_cast<void *>(payloadData), payloadLen);

            THEN("Builds and sends non-empty packet")            
            {
                // Convert payloadData to a string representation for comparison
                ostringstream oss("");
                for (int temp = 0; temp < 3; temp++)
                    oss << static_cast<char>(payloadData[temp]);
                std::string expectedData = oss.str();

                REQUIRE(expectedData == pinout->sentData);

            }
        }
    }
}

} //namespace binaryUart_test 
