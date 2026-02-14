/**
 * \file fiberAttenCtrl.hpp
 * \brief MagAO-X app for controlling a USB fiber attenuator
 *
 * \ingroup fiberAttenCtrl
 */

#ifndef fiberAttenCtrl_hpp
#define fiberAttenCtrl_hpp

#include "../../libMagAOX/libMagAOX.hpp"
#include "../../magaox_git_version.h"

namespace MagAOX
{
namespace app
{

class fiberAttenCtrl : public MagAOXApp<true>, public tty::usbDevice, public dev::ioDevice
{
public:
    fiberAttenCtrl();
    ~fiberAttenCtrl() noexcept {}

    virtual void setupConfig();
    virtual void loadConfig();
    virtual int appStartup();
    virtual int appLogic();
    virtual int appShutdown();

    INDI_NEWCALLBACK_DECL(fiberAttenCtrl, m_indi_atten);

protected:
    double m_targetAtten{0.0};
    double m_currentAtten{0.0};

    std::mutex m_indiMutex;

    pcf::IndiProperty m_indi_atten;

    int writeCommand(const std::string& cmd);
    int readResponse(std::string& response, int timeout_ms = 500);
    int updateAttenuation(double target);
    int queryAttenuation();
    int connect();
};

fiberAttenCtrl::fiberAttenCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
    dev::ioDevice::m_readTimeout = 2000;
    dev::ioDevice::m_writeTimeout = 1000;
}

void fiberAttenCtrl::setupConfig()
{
    tty::usbDevice::setupConfig(config);
    dev::ioDevice::setupConfig(config);
}

void fiberAttenCtrl::loadConfig()
{
    tty::usbDevice::loadConfig(config);
    dev::ioDevice::loadConfig(config);

    log<text_log>("Loaded USB config: VID=" + m_idVendor + " PID=" + m_idProduct + " SERIAL=" + m_serial);
}

int fiberAttenCtrl::appStartup()
{
    createStandardIndiNumber<double>(m_indi_atten, "attenuation", 0.0, 1000.0, 1.0, "%0.1f");
    m_indi_atten["current"] = m_currentAtten;
    m_indi_atten["target"] = m_targetAtten;
    registerIndiPropertyNew(m_indi_atten, INDI_NEWCALLBACK(m_indi_atten));

    return connect();
}

int fiberAttenCtrl::connect()
{
    int rv = tty::usbDevice::getDeviceName();
    if (rv < 0 && rv != TTY_E_DEVNOTFOUND && rv != TTY_E_NODEVNAMES)
    {
        std::stringstream logs;
            logs << "USB Device NOT FOUND";
        log<text_log>(logs.str());
        state(stateCodes::FAILURE);
        return log<software_critical, -1>({__FILE__, __LINE__, rv, tty::ttyErrorString(rv)});
    }

    if (rv == TTY_E_DEVNOTFOUND || rv == TTY_E_NODEVNAMES)
    {
        state(stateCodes::NODEVICE);
        std::stringstream logs;
            logs << "USB Device NOT FOUND";
        log<text_log>(logs.str());

        if (!stateLogged())
        {
            std::stringstream logs;
            logs << "USB Device " << m_idVendor << ":" << m_idProduct << ":" << m_serial << " not found in udev";
            log<text_log>(logs.str());
        }
        return 0;
    }
    else
    {
        std::stringstream logs;
            logs << "USB Device Found";
        log<text_log>(logs.str());
        state(stateCodes::NOTCONNECTED);
        if (!stateLogged())
        {
            std::stringstream logs;
            logs << "USB Device " << m_idVendor << ":" << m_idProduct << ":" << m_serial << " found in udev as " << m_deviceName;
            log<text_log>(logs.str());
        }

        {
            elevatedPrivileges elPriv(this);
            rv = tty::usbDevice::connect();
        }

        if (rv == TTY_E_NOERROR)
        {
            state(stateCodes::CONNECTED);
            if (!stateLogged())
            {
                std::stringstream logs;
                logs << "Connected to " << m_idVendor << ":" << m_idProduct << ":" << m_serial << " @ " << m_deviceName;
                log<text_log>(logs.str());
            }

            writeCommand("E0\r"); // Disable echo
            return 0;
        }
        else
        {
            state(stateCodes::FAILURE);
            return log<software_critical, -1>({__FILE__, __LINE__, errno, rv, "Error opening connection: " + tty::ttyErrorString(rv)});
        }
    }
}

int fiberAttenCtrl::appLogic()
{
    if (state() == stateCodes::NODEVICE || state() == stateCodes::NOTCONNECTED || state() == stateCodes::ERROR)
    {
        int rv = connect();
        if (rv < 0) return log<software_error, -1>({__FILE__, __LINE__});
    }

    if (state() == stateCodes::CONNECTED || state() == stateCodes::OPERATING)
    {
        // No periodic polling yet. Could add a periodic attenuation query if needed.
        state(stateCodes::OPERATING);
    }

    return 0;
}

int fiberAttenCtrl::appShutdown()
{
    if (m_fileDescrip > 0) {
        close(m_fileDescrip);
        m_fileDescrip = 0;
    }
    return 0;
}

int fiberAttenCtrl::updateAttenuation(double target)
{
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "A%.1f\r", target);
    if (writeCommand(cmd) < 0) return -1;

    std::string response;
    for (int i = 0; i < 10; ++i) {
        readResponse(response);
        if (response.find("Done") != std::string::npos) break;
        usleep(200000);
    }

    if (queryAttenuation() == 0) {
        updateIfChanged(m_indi_atten, "target", target);
        updateIfChanged(m_indi_atten, "current", m_currentAtten);
    }

    return 0;
}

int fiberAttenCtrl::queryAttenuation()
{
    if (writeCommand("A?\r") < 0) return -1;
    std::string reply;
    if (readResponse(reply) < 0) return -1;

    try {
        size_t colon = reply.find(":");
        size_t paren = reply.find("(", colon);
        std::string val = reply.substr(colon + 1, paren - colon - 1);
        m_currentAtten = std::stod(val);
    } catch (...) {
        return log<software_error, -1>({__FILE__, __LINE__, "Failed to parse attenuation"});
    }
    return 0;
}

int fiberAttenCtrl::writeCommand(const std::string& cmd)
{
    if (write(m_fileDescrip, cmd.c_str(), cmd.length()) < 0)
        return log<software_error, -1>({__FILE__, __LINE__, "Failed to write command"});
    return 0;
}

int fiberAttenCtrl::readResponse(std::string& response, int timeout_ms)
{
    char buf[128];
    int n = read(m_fileDescrip, buf, sizeof(buf)-1);
    if (n < 0) return log<software_error, -1>({__FILE__, __LINE__, "Serial read failed"});
    buf[n] = '\0';
    response = buf;
    return 0;
}

INDI_NEWCALLBACK_DEFN(fiberAttenCtrl, m_indi_atten)(const pcf::IndiProperty &ipRecv)
{
    if (ipRecv.getName() != m_indi_atten.getName())
    {
        log<software_error>({__FILE__, __LINE__, "wrong INDI property received."});
        return -1;
    }

    double atten = 0.0;
    if (ipRecv.find("target"))
    {
        atten = ipRecv["target"].get<double>();
    }

    {
        std::unique_lock<std::mutex> lock(m_indiMutex);
        m_targetAtten = atten;
        int rv = updateAttenuation(atten);
        if (rv < 0)
        {
            log<software_error>({__FILE__, __LINE__, "Error setting attenuation!"});
            return -1;
        }
    }

    updateIfChanged(m_indi_atten, "target", atten);
    updateIfChanged(m_indi_atten, "current", m_currentAtten);

    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // fiberAttenCtrl_hpp
