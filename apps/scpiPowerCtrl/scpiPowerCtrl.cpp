/** \file scpiPowerCtrl.cpp
  * \brief The MagAO-X Tripp Lite Power Distribution Unit controller main program.
  *
  * \author Jared R. Males (jaredmales@gmail.com)
  * 
  * \ingroup scpiPowerCtrl_files
  */


#include "scpiPowerCtrl.hpp"
void MagAOX::app::scpiPowerCtrl::startPollThread()
{
    if(m_pollThreadStarted) return;
    m_polling = true;
    m_pollThread = std::thread(&scpiPowerCtrl::pollLoop, this);
    m_pollThreadStarted = true;
}

void MagAOX::app::scpiPowerCtrl::stopPollThread()
{
    m_polling = false;
    if(m_pollThreadStarted && m_pollThread.joinable()) m_pollThread.join();
    m_pollThreadStarted = false;
}

void MagAOX::app::scpiPowerCtrl::pollLoop()
{
    const int sleep_us = std::max(1, 1000000 / std::max(1, m_pollRateHz));
    while(m_polling)
    {
        auto loop_start = std::chrono::steady_clock::now();

        {
            std::unique_lock<std::mutex> lock(m_indiMutex, std::try_to_lock);
            if(lock.owns_lock())
            {
                for(int ch = 0; ch < m_numChannels; ++ch)
                {
                    // Select channel
                    std::string res;
                    std::string cmd_sel = std::string("INST:NSEL ") + std::to_string(ch + 1) + "\n";
                    if (send_scpi(cmd_sel, res))
                    {
                        // Query output state to update outletStates
                        std::string outp;
                        if (send_scpi("OUTP?\n", outp))
                        {
                            int st = 0; try { st = std::stoi(outp); } catch(...) { st = 0; }
                            m_outletStates[ch] = (st == 1 ? OUTLET_STATE_ON : OUTLET_STATE_OFF);
                        }

                        // Query measurements
                        std::string volt, curr;
                        if (send_scpi("MEAS:VOLT?\n", volt))
                        {
                            volt.erase(volt.find_last_not_of(" \n\r\t") + 1);
                            try { m_channelVoltages[ch] = std::stof(volt); } catch(...) {}
                        }
                        if (send_scpi("MEAS:CURR?\n", curr))
                        {
                            curr.erase(curr.find_last_not_of(" \n\r\t") + 1);
                            try { m_channelCurrents[ch] = std::stof(curr); } catch(...) {}
                        }
                    }
                }

                // Push measurement INDI updates quickly.
                updateIfChanged(m_indiP_outlet1volt_meas, "current", m_channelVoltages[0]);
                updateIfChanged(m_indiP_outlet1curr_meas, "current", m_channelCurrents[0]);
                if(m_numChannels > 1)
                {
                    updateIfChanged(m_indiP_outlet2volt_meas, "current", m_channelVoltages[1]);
                    updateIfChanged(m_indiP_outlet2curr_meas, "current", m_channelCurrents[1]);
                }
                if(m_numChannels > 2)
                {
                    updateIfChanged(m_indiP_outlet3volt_meas, "current", m_channelVoltages[2]);
                    updateIfChanged(m_indiP_outlet3curr_meas, "current", m_channelCurrents[2]);
                }
                if(m_numChannels > 3)
                {
                    updateIfChanged(m_indiP_outlet4volt_meas, "current", m_channelVoltages[3]);
                    updateIfChanged(m_indiP_outlet4curr_meas, "current", m_channelCurrents[3]);
                }
                dev::outletController<scpiPowerCtrl>::updateINDI();

                // Telemetry write at high rate if enabled
                if(m_telemetryEnabled)
                {
                    writeTelemetryData();
                }
            }
        }

        // Sleep to maintain rate
        auto elapsed = std::chrono::steady_clock::now() - loop_start;
        auto sleep_dur = std::chrono::microseconds(sleep_us) - std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
        if(sleep_dur.count() > 0) std::this_thread::sleep_for(sleep_dur);
    }
}
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
        m_telemetryFile << "epoch_ns,volts_1,amps_1,volts_2,amps_2" << std::endl;
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
    long long epoch_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    float v1 = (m_channelVoltages.size() > 0 ? m_channelVoltages[0] : 0.0f);
    float a1 = (m_channelCurrents.size() > 0 ? m_channelCurrents[0] : 0.0f);
    float v2 = (m_channelVoltages.size() > 1 ? m_channelVoltages[1] : 0.0f);
    float a2 = (m_channelCurrents.size() > 1 ? m_channelCurrents[1] : 0.0f);

    m_telemetryFile << epoch_ns << "," << v1 << "," << a1 << "," << v2 << "," << a2 << std::endl;
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
