/** \file template_test.cpp
  * \brief Catch2 tests for the template app.
  *
  * History:
  */
#include "../../../tests/catch2/catch.hpp"

#include "../dmCtrl.hpp"
#include <fstream>
#include <cstdio>
#include <cmath>

using namespace MagAOX::app;

// namespace dmCtrl_test 
// {
   
/*! \brief Mock classes
*/

class MockMappingQuery : public MappingQuery {
   public:
   bool payloadSet = false;
   uint16_t startPixel;
   
   void setPayload(const void *MappingPayloads, uint16_t MappingPayloadsLen, uint16_t StartPixel) override {
      payloadSet = true;
      PayloadData = const_cast<void*>(MappingPayloads);
      PayloadLen = MappingPayloadsLen;
      startPixel = StartPixel;
   }
};

class MockShortPixelsQuery : public ShortPixelsQuery {
   public:
   bool payloadSet = false;
   uint16_t startPixel;
   
   void setPayload(const void *Setpoints, uint16_t SetpointsLen, uint16_t StartPixel) override {
      payloadSet = true;
      ShortPixelsQuery::setPayload(Setpoints, SetpointsLen, StartPixel);
   }
};

class MockLongPixelsQuery : public LongPixelsQuery {
   public:
   bool payloadSet = false;
   uint16_t startPixel;
   
   void setPayload(const void *Setpoints, uint16_t SetpointsLen, uint16_t StartPixel) override {
      payloadSet = true;
      PayloadData = const_cast<void*>(Setpoints);
      PayloadLen = SetpointsLen;
      startPixel = StartPixel;
   }
};

class MockBaseQuery : public dev::sdevQuery {
   public:
   bool payloadSet = false;
   
   void setPayload(void *Setpoints, uint16_t SetpointsLen) override {
      payloadSet = true;
      PayloadData = const_cast<void*>(Setpoints);
      PayloadLen = SetpointsLen;
   }

   void errorLogString(const size_t ParamsLen) override {
      // Empty; don't need it for testing
   }
   void processReply(char const *Params, const size_t ParamsLen) override {
      // Empty; don't need it for testing
   }
   void logReply() override {
      // Empty; don't need it for testing
   }
};

// Utilities
void createValidFITSFile(std::string& filename) {
   fitsfile* fptr;
   int status = 0;
   long naxes[2] = {2, 2}; // 2D image of size 2x2
   std::vector<long> data = {0, 3, 1, 0}; // Example data

   // Create a new FITS file
   fits_create_file(&fptr, filename.c_str(), &status);
   fits_create_img(fptr, LONG_IMG, 2, naxes, &status);
   fits_write_img(fptr, TLONG, 1, data.size(), data.data(), &status);
   fits_close_file(fptr, &status);
}

   
// Class for testing that exposes protected members and mocks methods.
class dmCtrl_test : public dmCtrl {
public:

   dmCtrl_test() {
      m_mode = "short";
      m_act_gain = 1.0;
      m_volume_factor = 1.0;
   }

   // Override the query method
   void query(dev::sdevQuery *Query) override {
   }

   // Override the receive method
   void receive() override {
   }

   // Expose protected methods for testing
   using dmCtrl::get_shmim_to_pixel_mapping;
   using dmCtrl::get_array_to_actuator_mapping;
   using dmCtrl::send_array;
   using dmCtrl::initDM;
   using dmCtrl::zeroDM;
   using dmCtrl::commandDM;
   using dmCtrl::releaseDM;
   using dmCtrl::mappingQuery;
   using dmCtrl::shortPixelQuery;
   using dmCtrl::longPixelQuery;
   using dmCtrl::m_nbAct;
   using dmCtrl::m_dm_map_filename;
   using dmCtrl::m_shmim_map_filename;
   using dmCtrl::m_mode;
   using dmCtrl::m_act_gain;
   using dmCtrl::m_volume_factor;
   using dmCtrl::m_dminputs;
   using dmCtrl::m_actuator_mapping;
   using dmCtrl::SHORT;
   using dmCtrl::LONG;
   using dmCtrl::DITHER;
};


SCENARIO("Testing send_array", "[dmCtrl]") {
   GIVEN("A pointer to the inputs vector, the number of inputs and a start pixel") {
      dmCtrl_test ctrl;
      std::vector<double> inputs = {1, 2, 3, 4, 5};
      uint16_t nbInputs = inputs.size();
      uint16_t startPixel = 0;
      
      WHEN("send_array is called with valid parameters and the mode is SHORT, but shortPixelQuery cast fails") {
         ctrl.m_mode = ctrl.SHORT;
         std::unique_ptr<dev::sdevQuery> mockBaseQuery = std::make_unique<MockBaseQuery>();
         MockBaseQuery* mockBaseQueryPtr = static_cast<MockBaseQuery*>(mockBaseQuery.get()); // Keep a pointer to the mock instance
         ctrl.shortPixelQuery = std::move(mockBaseQuery);

         THEN("It should return -1 and not set the payload") {
            int result = ctrl.send_array(inputs, nbInputs, startPixel);
            REQUIRE(result == -1);
            REQUIRE(!mockBaseQueryPtr->payloadSet);
         }
      }

      WHEN("send_array is called with valid parameters and the mode is SHORT") {
         ctrl.m_mode = ctrl.SHORT;
         std::unique_ptr<dev::sdevQuery> mockShortPixelsQuery = std::make_unique<MockShortPixelsQuery>();
         MockShortPixelsQuery* mockShortPixelsQueryPtr = static_cast<MockShortPixelsQuery*>(mockShortPixelsQuery.get()); // Keep a pointer to the mock instance
         ctrl.shortPixelQuery = std::move(mockShortPixelsQuery);

         THEN("It should return 0 and have set the payload") {
            int result = ctrl.send_array(inputs, nbInputs, startPixel);
            REQUIRE(result == 0);
            REQUIRE(mockShortPixelsQueryPtr->payloadSet);
            // TODO: Test that the payload data is set as expected for SHORT mode;
         }
      }
      
      WHEN("send_array is called with valid parameters and the mode is LONG, but longPixelQuery cast fails") {
         ctrl.m_mode = ctrl.LONG;
         std::unique_ptr<dev::sdevQuery> mockBaseQuery = std::make_unique<MockBaseQuery>();
         MockBaseQuery* mockBaseQueryPtr = static_cast<MockBaseQuery*>(mockBaseQuery.get()); // Keep a pointer to the mock instance
         ctrl.longPixelQuery = std::move(mockBaseQuery);

         THEN("It should return -1 and not set the payload") {
            int result = ctrl.send_array(inputs, nbInputs, startPixel);
            REQUIRE(result == -1);
            REQUIRE(!mockBaseQueryPtr->payloadSet);
         }
      }

      WHEN("send_array is called with valid parameters and the mode is LONG") {
         ctrl.m_mode = ctrl.LONG;
         std::unique_ptr<dev::sdevQuery> mockLongPixelsQuery = std::make_unique<MockLongPixelsQuery>();
         MockLongPixelsQuery* mockLongPixelsQueryPtr = static_cast<MockLongPixelsQuery*>(mockLongPixelsQuery.get()); // Keep a pointer to the mock instance
         ctrl.longPixelQuery = std::move(mockLongPixelsQuery);

         THEN("It should return 0 and have set the payload") {
            int result = ctrl.send_array(inputs, nbInputs, startPixel);
            REQUIRE(result == 0);
            REQUIRE(mockLongPixelsQueryPtr->payloadSet);
            // TODO: Test that the payload data is set as expected for LONG mode
         }
      }

      // TODO add dither tests when implemented

      WHEN("send_array is called with valid parameters unknown mode") {
         ctrl.m_mode = "";
         
         std::unique_ptr<dev::sdevQuery> mockShortPixelsQuery = std::make_unique<MockShortPixelsQuery>();
         MockShortPixelsQuery* mockShortPixelsQueryPtr = static_cast<MockShortPixelsQuery*>(mockShortPixelsQuery.get()); // Keep a pointer to the mock instance
         ctrl.shortPixelQuery = std::move(mockShortPixelsQuery);

         std::unique_ptr<dev::sdevQuery> mockLongPixelsQuery = std::make_unique<MockLongPixelsQuery>();
         MockLongPixelsQuery* mockLongPixelsQueryPtr = static_cast<MockLongPixelsQuery*>(mockLongPixelsQuery.get()); // Keep a pointer to the mock instance
         ctrl.longPixelQuery = std::move(mockLongPixelsQuery);

         THEN("It should return -1 and not set the payload") {
            int result = ctrl.send_array(inputs, nbInputs, startPixel);
            REQUIRE(result == -1);
            REQUIRE(!mockShortPixelsQueryPtr->payloadSet);
            REQUIRE(!mockLongPixelsQueryPtr->payloadSet);
         }
      }
   }
}


SCENARIO("Testing get_array_to_actuator_mapping", "[dmCtrl]") {
   GIVEN("A pointer to a CGraphDMMappings instance") {
      dmCtrl_test ctrl;
      std::string tmpFilename = "/tmp/test_dm_map.py";
      ctrl.m_dm_map_filename = tmpFilename;
      
      // Small, test-friendly number of actuators
      int nb_act = 4;
      ctrl.m_nbAct = nb_act;
      CGraphDMMappings map_lut(nb_act);

      WHEN("A file name not provided") {
         ctrl.m_dm_map_filename = "";

         int result = ctrl.get_array_to_actuator_mapping(map_lut);
         
         THEN("It should return -1 and not set the map_lut") {
            REQUIRE(result == -1);
         }
      }

      WHEN("A file name provided but file does not exist") {
         ctrl.m_dm_map_filename = "/non/existent/file.py";

         int result = ctrl.get_array_to_actuator_mapping(map_lut);
         
         THEN("It should return -1 and not set the map_lut") {
            REQUIRE(result == -1);
         }
      }
      
      WHEN("A valid map file provided") {

         // Make temporary map file
         std::ofstream ofs(tmpFilename);
         ofs << "actuatorMap = [\n";
         ofs << "[0, 0, 0],\n";
         ofs << "[0, 0, 1],\n";
         ofs << "[1, 1, 0],\n";
         ofs << "[1, 1, 1]]\n";
         ofs.close();

         int result = ctrl.get_array_to_actuator_mapping(map_lut);
         
         THEN("It should return 0 and the CGraphDMMappings instance should contain the file input data.") {
            REQUIRE(result == 0);
            
            REQUIRE(map_lut.length() == nb_act);
            REQUIRE(map_lut.Mappings[0].ControllerBoardIndex == 0);
            REQUIRE(map_lut.Mappings[0].DacIndex == 0);
            REQUIRE(map_lut.Mappings[0].DacChannel == 0);
            REQUIRE(map_lut.Mappings[1].ControllerBoardIndex == 0);
            REQUIRE(map_lut.Mappings[1].DacIndex == 0);
            REQUIRE(map_lut.Mappings[1].DacChannel == 1);
            REQUIRE(map_lut.Mappings[2].ControllerBoardIndex == 1);
            REQUIRE(map_lut.Mappings[2].DacIndex == 1);
            REQUIRE(map_lut.Mappings[2].DacChannel == 0);
            REQUIRE(map_lut.Mappings[3].ControllerBoardIndex == 1);
            REQUIRE(map_lut.Mappings[3].DacIndex == 1);
            REQUIRE(map_lut.Mappings[3].DacChannel == 1);
         }

         // Clean up temporary file.
         std::remove(tmpFilename.c_str());
      }

      WHEN("A map file provided with more actuator values than m_nbAct") {
         
         // Make temporary map file
         std::ofstream ofs(tmpFilename);
         ofs << "actuatorMap = [\n";
         ofs << "[0, 0, 0],\n";
         ofs << "[0, 0, 1],\n";
         ofs << "[1, 1, 0],\n";
         ofs << "[1, 1, 1],\n";
         ofs << "[2, 0, 0],\n";
         ofs << "[2, 0, 1],\n";
         ofs << "[2, 1, 0],\n";
         ofs << "[2, 1, 1]]\n";
         ofs.close();

         int result = ctrl.get_array_to_actuator_mapping(map_lut);
         
         THEN("It should return -1.") {
            REQUIRE(result == -1);
         }

         // Clean up temporary file.
         std::remove(tmpFilename.c_str());
      }

      WHEN("A map file provided with values outside of the configured ranges") {
         
         // Make temporary map file
         std::ofstream ofs(tmpFilename);
         ofs << "actuatorMap = [\n";
         ofs << "[0, 0, 0],\n";
         ofs << "[0, 0, 1],\n";
         ofs << "[1, 5, 0],\n";
         ofs << "[1, 5, 1]]\n";
         ofs.close();

         int result = ctrl.get_array_to_actuator_mapping(map_lut);
         
         THEN("It should return -1.") {
            REQUIRE(result == -1);
         }

         // Clean up temporary file.
         std::remove(tmpFilename.c_str());
      }

      WHEN("A map file provided with data line structure different to that expected by the parser ( [ControllerBoardIndex, DacIndex, DacChannel] )") {
         
         // Make temporary map file
         std::ofstream ofs(tmpFilename);
         ofs << "actuatorMap = [\n";
         ofs << "[0, 0,],\n";
         ofs << "[0, 0,],\n";
         ofs << "[1, 1,],\n";
         ofs << "[1, 1,]]\n";
         ofs.close();

         int result = ctrl.get_array_to_actuator_mapping(map_lut);
         
         THEN("It should return -1.") {
            REQUIRE(result == -1);
         }

         // Clean up temporary file.
         std::remove(tmpFilename.c_str());
      }

      WHEN("A map file provided with no valid data (i.e. no entries starting with '['])") {
         
         // Make temporary map file
         std::ofstream ofs(tmpFilename);
         ofs << "actuatorMap = [\n";
         ofs << "{a:1, b:2},\n";
         ofs.close();

         int result = ctrl.get_array_to_actuator_mapping(map_lut);
         
         THEN("It should return -1.") {
            REQUIRE(result == -1);
         }

         // Clean up temporary file.
         std::remove(tmpFilename.c_str());
      }
   }
}


SCENARIO("Testing get_shmim_to_pixel_mapping", "[dmCtrl]") {
   GIVEN("A fits file name.") {
      dmCtrl_test ctrl;
      // ctrl.m_nbAct = 4; // Set a small, test-friendly number of actuators
      
      WHEN("The map file doesn't exist") {
         ctrl.m_shmim_map_filename = "invalid.fits"; // Non-existent file
         int result = ctrl.get_shmim_to_pixel_mapping();

         THEN("It should return -1") {
            REQUIRE(result == -1);
         }
      }

      WHEN("The map file has invalid image dimensions") {
         std::string filename = "invalid_dimensions.fits";
         fitsfile* fptr;
         int status = 0;
         long naxes[3] = {1, 1, 1}; // 3D image
         std::vector<long> data = {0, 3, 1};

         // Create a new FITS file
         fits_create_file(&fptr, filename.c_str(), &status);
         fits_create_img(fptr, LONG_IMG, 3, naxes, &status);
         fits_write_img(fptr, TLONG, 1, data.size(), data.data(), &status);
         fits_close_file(fptr, &status);
         
         ctrl.m_shmim_map_filename = filename;
         int result = ctrl.get_shmim_to_pixel_mapping();

         THEN("It should return -1") {
            REQUIRE(result == -1);
         }
      }

      WHEN("The mapping is valid") {
         std::string filename = "valid.fits";
         createValidFITSFile(filename);

         ctrl.m_shmim_map_filename = filename;
         ctrl.m_actuator_mapping.assign(ctrl.m_nbAct, -1); // This is done in initDM, so we do it here for the test
         
         int result = ctrl.get_shmim_to_pixel_mapping();

         THEN("It should return 0 and have the m_actuator_mapping populated") {
            REQUIRE(result == 0);

            // Check that the actuator mapping is populated correctly
            REQUIRE(ctrl.m_actuator_mapping[0] == 2);
            REQUIRE(ctrl.m_actuator_mapping[1] == -1);
            REQUIRE(ctrl.m_actuator_mapping[2] == 1);
            REQUIRE(ctrl.m_actuator_mapping[3] == -1);
         }
      }

      // Clean up generated files if necessary
      std::remove("valid.fits");
      std::remove("invalid_dimensions.fits");
   }
}


SCENARIO("Testing zeroDM", "[dmCtrl]") {
   GIVEN("A dmCtrl object instance with mode set to SHORT") {
      dmCtrl_test ctrl;
      ctrl.m_nbAct = 4; // Set a small, test-friendly number of actuators
      
      WHEN("zeroDM's call to send_array succeeds") {
         ctrl.m_mode = ctrl.SHORT;

         std::unique_ptr<dev::sdevQuery> mockShortPixelsQuery = std::make_unique<MockShortPixelsQuery>();
         MockShortPixelsQuery* mockShortPixelsQueryPtr = static_cast<MockShortPixelsQuery*>(mockShortPixelsQuery.get()); // Keep a pointer to the mock instance
         ctrl.shortPixelQuery = std::move(mockShortPixelsQuery);
         
         int result = ctrl.zeroDM();

         THEN("It should return 0 and have set the payload to zero") {
            REQUIRE(result == 0);
            REQUIRE(mockShortPixelsQueryPtr->payloadSet);
            // TODO: Test that the payload data is set to zero;
         }
      }

      WHEN("zeroDM's call to send_array fails") {
         ctrl.m_mode = "";

         std::unique_ptr<dev::sdevQuery> mockShortPixelsQuery = std::make_unique<MockShortPixelsQuery>();
         MockShortPixelsQuery* mockShortPixelsQueryPtr = static_cast<MockShortPixelsQuery*>(mockShortPixelsQuery.get()); // Keep a pointer to the mock instance
         ctrl.shortPixelQuery = std::move(mockShortPixelsQuery);
         
         int result = ctrl.zeroDM();

         THEN("It should return 0 and have set the payload to zero") {
            REQUIRE(result == -1);
            REQUIRE(!mockShortPixelsQueryPtr->payloadSet);
         }
      }
   }
}


SCENARIO("Testing initDM", "[dmCtrl]") {
   GIVEN("A dmCtrl object in a ready state") {
      dmCtrl_test ctrl;

      WHEN("Mapping query casting fails") {
         ctrl.mappingQuery.reset(); // Set mappingQuery to nullptr
         int result = ctrl.initDM();
         THEN("It should return -1 and not progress to assigning m_dminputs") {
            REQUIRE(result == -1);

            // Check the state of m_dminputs, since the error should occur before it is assigned.
            size_t initialSize = ctrl.m_dminputs.size();
            bool isEmpty = ctrl.m_dminputs.empty();

            // Then the size should be 0 and it should be empty
            REQUIRE(initialSize == 0);
            REQUIRE(isEmpty == true);     
         }
      }

      WHEN("Map file not provided") {
         ctrl.m_dm_map_filename = "";

         // Replace the mappingQuery with a mock
         std::unique_ptr<dev::sdevQuery> mockMappingQuery = std::make_unique<MockMappingQuery>();
         MockMappingQuery* mockMappingQueryPtr = static_cast<MockMappingQuery*>(mockMappingQuery.get()); // Keep a pointer to the mock instance
         ctrl.mappingQuery = std::move(mockMappingQuery);

         // Make zeroDM succeed
         ctrl.m_mode = ctrl.SHORT;
         std::unique_ptr<dev::sdevQuery> mockShortPixelsQuery = std::make_unique<MockShortPixelsQuery>();
         ctrl.shortPixelQuery = std::move(mockShortPixelsQuery);

         // Make get_shmim_to_pixel_mapping succeed
         std::string filename = "valid.fits";
         createValidFITSFile(filename);
         ctrl.m_shmim_map_filename = filename;

         int result = ctrl.initDM();
         
         THEN("It should return 0 and have an empty payload.") {
            // Then the result should be 0 (success)
            REQUIRE(result == 0);
            
            // And the payload should be set
            REQUIRE(mockMappingQueryPtr->payloadSet);
            REQUIRE(mockMappingQueryPtr->getPayloadLen() == 0);
            // TODO: Test that the payload data is empty;
         }

         // Clean up temporary file.
         std::remove("valid.fits");
      }
      
      WHEN("Map file provided") {
         // Small, test-friendly number of actuators
         int nb_act = 3;
         ctrl.m_nbAct = nb_act;

         // Make temporary map file
         std::string tmpFilename = "/tmp/test_dm_map.py";
         std::ofstream ofs(tmpFilename);
         ofs << "actuatorMap = [\n";
         ofs << "[1, 1, 1],\n";
         ofs << "[2, 1, 2]]";
         ofs << "[3, 2, 2]]";
         ofs.close();
         ctrl.m_dm_map_filename = tmpFilename;

         // Replace the mappingQuery with a mock
         std::unique_ptr<dev::sdevQuery> mockMappingQuery = std::make_unique<MockMappingQuery>();
         MockMappingQuery* mockMappingQueryPtr = static_cast<MockMappingQuery*>(mockMappingQuery.get()); // Keep a pointer to the mock instance
         ctrl.mappingQuery = std::move(mockMappingQuery);

         // Make zeroDM succeed
         ctrl.m_mode = ctrl.SHORT;
         std::unique_ptr<dev::sdevQuery> mockShortPixelsQuery = std::make_unique<MockShortPixelsQuery>();
         ctrl.shortPixelQuery = std::move(mockShortPixelsQuery);

         // Make get_shmim_to_pixel_mapping succeed
         std::string filename = "valid.fits";
         createValidFITSFile(filename);
         ctrl.m_shmim_map_filename = filename;

         int result = ctrl.initDM();
         uint16_t payloadLen = static_cast<uint16_t>(nb_act * sizeof(CGraphDMMappingPayload));
         
         THEN("It should return 0 and have a payload of payloadLen length.") {
            // Then the result should be 0 (success)
            REQUIRE(result == 0);
            
            // And the payload should be set correctly
            REQUIRE(mockMappingQueryPtr->payloadSet);
            REQUIRE(mockMappingQueryPtr->getPayloadLen() == payloadLen);
            // TODO: Test that the payload data is set correctly;
         }

         // Clean up temporary file.
         std::remove(tmpFilename.c_str());
         std::remove("valid.fits");
      }
      
      WHEN("Map retrieved successfully, but zeroDM fails") {
         ctrl.m_dm_map_filename = "";

         // Replace the mappingQuery with a mock
         std::unique_ptr<dev::sdevQuery> mockMappingQuery = std::make_unique<MockMappingQuery>();
         MockMappingQuery* mockMappingQueryPtr = static_cast<MockMappingQuery*>(mockMappingQuery.get()); // Keep a pointer to the mock instance
         ctrl.mappingQuery = std::move(mockMappingQuery);

         // Make zeroDM fail
         ctrl.m_mode = "";

         int result = ctrl.initDM();

         THEN("It should return -1 and not progress to assigning m_actuator_mapping") {
            REQUIRE(result == -1);

            // The mappingQuery payload should have been set
            REQUIRE(mockMappingQueryPtr->payloadSet);

            // Check the state of m_actuator_mapping, since the error should occur before it is assigned.
            size_t initialSize = ctrl.m_actuator_mapping.size();
            bool isEmpty = ctrl.m_actuator_mapping.empty();

            // Then the size should be 0 and it should be empty
            REQUIRE(initialSize == 0);
            REQUIRE(isEmpty == true);     
         }
      }

      WHEN("Map retrieved successfully, zeroDM succeeds, but get_shmim_to_pixel_mapping fails") {
         // Small, test-friendly number of actuators
         int nb_act = 3;
         ctrl.m_nbAct = nb_act;
         ctrl.m_dm_map_filename = "";

         // Replace the mappingQuery with a mock
         std::unique_ptr<dev::sdevQuery> mockMappingQuery = std::make_unique<MockMappingQuery>();
         MockMappingQuery* mockMappingQueryPtr = static_cast<MockMappingQuery*>(mockMappingQuery.get()); // Keep a pointer to the mock instance
         ctrl.mappingQuery = std::move(mockMappingQuery);

         // Make zeroDM succeed
         ctrl.m_mode = ctrl.SHORT;
         std::unique_ptr<dev::sdevQuery> mockShortPixelsQuery = std::make_unique<MockShortPixelsQuery>();
         ctrl.shortPixelQuery = std::move(mockShortPixelsQuery);

         // Make get_shmim_to_pixel_mapping fail
         ctrl.m_shmim_map_filename = "invalid.fits"; // Non-existent file

         int result = ctrl.initDM();

         THEN("It should return -1 but m_actuator_mapping should be assigned") {
            REQUIRE(result == -1);

            // The mappingQuery payload should have been set
            REQUIRE(mockMappingQueryPtr->payloadSet);

            // Check the state of m_actuator_mapping, since the error occurs after it is assigned.
            size_t initialSize = ctrl.m_actuator_mapping.size();
            bool isEmpty = ctrl.m_actuator_mapping.empty();

            // Then the size should be nb_act and it should not be empty
            REQUIRE(initialSize == nb_act);
            REQUIRE(isEmpty == false); 
         }
      }
   }
}


// } //namespace dmCtrl_test 
