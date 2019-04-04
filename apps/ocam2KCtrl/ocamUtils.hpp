/** \file ocamUtils.hpp
  * \brief Utilities for the OCAM camera
  *
  * \ingroup ocam2kCtrl_files
  */

#ifndef ocamUtils_hpp
#define ocamUtils_hpp


#include <mx/ioutils/stringUtils.hpp>

namespace MagAOX
{
namespace app
{

///\todo document this
struct ocamTemps
{
   float CCD {0};
   float CPU {0};
   float POWER {0};
   float BIAS {0};
   float WATER {0};
   float LEFT {0};
   float RIGHT {0};
   float SET {0};
   float COOLING_POWER {0};
};

///\todo document this
int parseTemps( ocamTemps & temps,
                const std::string & tstr
              )
{
   std::vector<std::string> v;
   mx::ioutils::parseStringVector(v, tstr, "[]");

   if( v.size() < 18) return -1;
   
   temps.CCD = mx::ioutils::convertFromString<float>( v[1] );
   temps.CPU = mx::ioutils::convertFromString<float>( v[3] );
   temps.POWER = mx::ioutils::convertFromString<float>( v[5] );
   temps.BIAS = mx::ioutils::convertFromString<float>( v[7] );
   temps.WATER = mx::ioutils::convertFromString<float>( v[9] );
   temps.LEFT = mx::ioutils::convertFromString<float>( v[11] );
   temps.RIGHT = mx::ioutils::convertFromString<float>( v[13] );
   temps.SET = mx::ioutils::convertFromString<float>( v[15] )/10.0;
   temps.COOLING_POWER = mx::ioutils::convertFromString<float>( v[17] );
   
   return 0;
}

///\todo document this
///\todo add test for FPS
int parseFPS( float & fps,
              const std::string & fstr
            )
{
   std::vector<std::string> v;
   mx::ioutils::parseStringVector(v, fstr, "[]");

   if( v.size() < 3) return -1;

   fps = mx::ioutils::convertFromString<float>( v[1] );

   return 0;
}

/// Parse the EM gain response 
/** Example response: "Gain set to 2 \n\n", with the trailing space.
  * Expects gain >=1 and <= 600, otherwise returns an error.
  * 
  * \returns 0 on success, and emGain set to a value >= 1
  * \returns -1 on error, and emGain will be set to 0.
  */ 
int parseEMGain( unsigned & emGain,       ///< [out] the value of gain returned by the camera
                 const std::string & fstr ///< [in] the query response from the camera.
               )
{
   std::vector<std::string> v;
   mx::ioutils::parseStringVector(v, fstr, " ");
  
   if( v.size() != 5) 
   {
      emGain = 0;
      return -1;
   }
   
   emGain = mx::ioutils::convertFromString<unsigned>( v[3] );

   if(emGain < 1 || emGain > 600)
   {
      emGain = 0;
      return -1;
   }
   
   return 0;
}

} //namespace app
} //namespace MagAOX

#endif //ocamUtils_hpp
