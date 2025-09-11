/** \file scpiPowerCtrl.cpp
  * \brief The MagAO-X Tripp Lite Power Distribution Unit controller main program.
  *
  * \author Jared R. Males (jaredmales@gmail.com)
  * 
  * \ingroup scpiPowerCtrl_files
  */


#include "scpiPowerCtrl.hpp"
int MagAOX::app::scpiPowerCtrl::startTelemetryLogging()
{
    if(!m_telemetryFile.is_open())
    {
        m_telemetryFilename = generateTelemetryFilename();
        m_telemetryFile.open(m_telemetryFilename, std::ios::out | std::ios::app);
        if(!m_telemetryFile.is_open())
        {
            return log<text_log,-1>("Failed to open telemetry file: " + m_telemetryFilename, logPrio::LOG_ERROR);
        }
        m_telemetryFile << "timestamp,volts_1,amps_1,volts_2,amps_2" << std::endl;
    }
    return 0;
}

int MagAOX::app::scpiPowerCtrl::stopTelemetryLogging()
{
    if(m_telemetryFile.is_open())
    {
        m_telemetryFile.flush();
        m_telemetryFile.close();
    }
    return 0;
}

int MagAOX::app::scpiPowerCtrl::writeTelemetryData()
{
    if(!m_telemetryEnabled) return 0;
    if(!m_telemetryFile.is_open())
    {
        if(startTelemetryLogging() < 0) return -1;
    }

    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()) % std::chrono::seconds(1);
    std::tm tm_now;
    gmtime_r(&now_time_t, &tm_now);
    char timebuf[64];
    std::snprintf(timebuf, sizeof(timebuf), "%04d-%02d-%02dT%02d:%02d:%02d.%09ldZ",
                  tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                  tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,
                  static_cast<long>(now_ns.count()));

    float v1 = (m_channelVoltages.size() > 0 ? m_channelVoltages[0] : 0.0f);
    float a1 = (m_channelCurrents.size() > 0 ? m_channelCurrents[0] : 0.0f);
    float v2 = (m_channelVoltages.size() > 1 ? m_channelVoltages[1] : 0.0f);
    float a2 = (m_channelCurrents.size() > 1 ? m_channelCurrents[1] : 0.0f);

    m_telemetryFile << timebuf << "," << v1 << "," << a1 << "," << v2 << "," << a2 << std::endl;
    return 0;
}

std::string MagAOX::app::scpiPowerCtrl::generateTelemetryFilename()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    gmtime_r(&now_time_t, &tm_now);
    char fname[256];
    std::snprintf(fname, sizeof(fname), "%s/scpiPowerCtrl_%04d%02d%02d_%02d%02d%02d.csv", m_telemetryPath.c_str(),
                  tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                  tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
    return std::string(fname);
}

int main(int argc, char ** argv)
{
   MagAOX::app::scpiPowerCtrl pdu;

   return pdu.main(argc, argv);
}
