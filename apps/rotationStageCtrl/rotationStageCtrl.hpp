#ifndef rotationStageCtrl_hpp
#define rotationStageCtrl_hpp

#include <string>
#include <vector>
#include <cmath>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <thread>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cctype>

#include "../../libMagAOX/libMagAOX.hpp"
#include "../../magaox_git_version.h"

namespace MagAOX {
namespace app {

/* Elliptec protocol (as in your working CLI)
 *  TX (no CRLF): <ADDR><cmd>[args]
 *  RX: CRLF-terminated
 *  cmds: in, gs, gp, hoX, st, us, om, svHH, maXXXXXXXX, mrXXXXXXXX
 */

class rotationStageCtrl
  : public MagAOXApp<>
  , public dev::stdMotionStage<rotationStageCtrl>
  , public dev::telemeter<rotationStageCtrl>
{
  friend class dev::stdMotionStage<rotationStageCtrl>;
  friend class dev::telemeter<rotationStageCtrl>;

public:
  rotationStageCtrl();
  ~rotationStageCtrl() noexcept {}

  // lifecycle
  void setupConfig() override;
  void loadConfig() override;
  int  appStartup() override;
  int  appLogic() override;
  int  appShutdown() override { closePort_(); return 0; }

  // stdMotionStage surface
  int   stop();                 // immediate stop
  int   startHoming();          // home (waits until GS!=Busy, applies offset)
  float presetNumber();         // nearest preset index (1-based) or -1
  int   moveTo(float pos);      // absolute move in preset space (1..N)

  // degree convenience for other call sites
  int moveTo(const double &deg) { return moveAbsDeg_(deg); }

  // telemeter wrappers expected by dev::telemeter
  int checkRecordTimes();
  int recordTelem(const telem_stage *);
  int recordStage(bool force = false);

  // INDI NEW callbacks (ours)
  INDI_NEWCALLBACK_DECL(rotationStageCtrl, m_ipAbsDeg);
  INDI_NEWCALLBACK_DECL(rotationStageCtrl, m_ipRelDeg);
  INDI_NEWCALLBACK_DECL(rotationStageCtrl, m_ipVelPct);
  INDI_NEWCALLBACK_DECL(rotationStageCtrl, m_ipOptimize);
  INDI_NEWCALLBACK_DECL(rotationStageCtrl, m_ipSave);

protected:
  // ---------- Config ----------
  std::string m_port;
  int         m_baud {9600};
  unsigned    m_startupDelayMs {200};

  // Elliptec serial
  char        m_addr {'0'};           // ASCII hex nibble
  int         m_readTimeoutMs {3000};
  int         m_postWriteSleepMs {0};

  // Behavior / UI
  int         m_velPercent {40};      // 0..100
  double      m_homeOffsetDeg {0.0};  // relative deg after home
  bool        m_allowMultiturn {false}; // if false: wrap abs to [0,720)

  // Conversion
  uint32_t    m_pulsesPerRev {0};     // pulses per 360 deg (auto/override)

  // Optional command aliases
  std::string m_cmdOptimize {"om"};
  std::string m_cmdSave     {"us"};

  // User presets (degrees)
  std::vector<std::string> m_userPresetNames;
  std::vector<double>      m_userPresetDeg;

  // ---------- Runtime ----------
  int      m_fd {-1};
  bool     m_connected {false};

  double   m_posDeg {0.0};
  int32_t  m_posPulses {0};
  bool     m_homed {false};

  uint8_t  m_gs {0x00};               // 0x00 OK, 0x09 Busy
  int8_t   m_moving {-1};             // -2 off, -1 not homed, 0 idle, 1 moving, 2 homing

  enum class Pending { None, MoveAbs, MoveRel, Home, OffsetRel, Optimize, Stop, Velocity, Save };
  Pending  m_pending {Pending::None};

  // INDI owned here
  pcf::IndiProperty m_ipAbsDeg;    // number current/target
  pcf::IndiProperty m_ipRelDeg;    // number target-only (we ignore "current")
  pcf::IndiProperty m_ipVelPct;    // number current/target
  pcf::IndiProperty m_ipStatus;    // text "current" only
  pcf::IndiProperty m_ipOptimize;  // request switch
  pcf::IndiProperty m_ipSave;      // request switch

  // ---------- Serial ----------
  static speed_t to_termios_baud_(int baud);
  int  openPort_();
  void closePort_();
  int  drainInput_();
  int  writeAll_(const std::string &s);
  int  readFrame_(std::string &out, int timeout_ms);

  // ---------- Protocol helpers ----------
  inline std::string frame_(const std::string& cmd) {
    std::string f; f.reserve(1+cmd.size());
    f.push_back(m_addr); f += cmd; return f;
  }
  int  txrx_(const std::string& cmd, std::string *reply, int timeout_ms);

  // Queries
  int  q_info_();       // "in"
  int  q_status_();     // "gs"
  int  q_position_();   // "gp"

  // Commands
  int  cmd_home_(uint8_t dir=0);                // "hoX"
  int  cmd_stop_();                             // "st"
  int  cmd_optimize_wait_();                    // "om" + wait until OK
  int  cmd_save_();                             // "us"
  int  cmd_setvel_(int pct);                    // "svHH"
  int  cmd_moveAbs_pulses_(int32_t pulses);     // "maXXXXXXXX"
  int  cmd_moveRel_pulses_(int32_t pulses);     // "mrXXXXXXXX"

  // Degree-facing
  int      moveAbsDeg_(double deg);
  int      moveRelDeg_(double ddeg);
  int32_t  degToPulses_(double deg) const;
  double   pulsesToDeg_(int32_t pulses) const;

  // Poll and reflect device state
  int  pollDevice_();     // gp + gs + resolve m_pending/m_moving
  void updateStatus_();   // push status text, absDeg.current
};

/* ========================= impl ========================= */

inline rotationStageCtrl::rotationStageCtrl()
: MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
  // stdMotionStage preset UI (1..N index). We'll map presets -> degrees.
  m_presetNotation  = "stage";
  m_powerMgtEnabled = true;
}

/* ---------- Config ---------- */

inline void rotationStageCtrl::setupConfig()
{
  // Serial
  config.add("stage.port", "", "stage.port", argType::Required, "stage", "port", false, "string", "Serial device path");
  config.add("stage.baud", "9600", "stage.baud", argType::Required, "stage", "baud", false, "int", "Baud rate");
  config.add("stage.address", "0", "stage.address", argType::Optional, "stage", "address", false, "string", "Elliptec address nibble (0-F)");
  config.add("serial.readTimeoutMs", "3000", "serial.readTimeoutMs", argType::Optional, "serial", "readTimeoutMs", false, "int", "Read timeout (ms)");
  config.add("serial.postWriteSleepMs", "0", "serial.postWriteSleepMs", argType::Optional, "serial", "postWriteSleepMs", false, "int", "Sleep after write (ms)");

  // Behavior
  config.add("stage.homeOffset", "0.0", "stage.homeOffset", argType::Optional, "stage", "homeOffset", false, "double", "Relative offset (deg) to move after homing");
  config.add("stage.allowMultiturn", "false", "stage.allowMultiturn", argType::Optional, "stage", "allowMultiturn", false, "bool", "If false, wrap abs to [0,720)");

  // Conversion
  config.add("stage.pulsesPerRev", "0", "stage.pulsesPerRev", argType::Optional, "stage", "pulsesPerRev", false, "uint", "Override pulses per 360deg (0=auto)");

  // Motion UI
  config.add("motion.velPercent", "40",  "motion.velPercent", argType::Optional, "motion", "velPercent", false, "int", "Velocity percent 0..100");

  // Optional command aliases
  config.add("device.optimizeCmd", "om", "device.optimizeCmd", argType::Optional, "device", "optimizeCmd", false, "string", "Optimize command");
  config.add("device.saveCmd",     "us", "device.saveCmd",     argType::Optional, "device", "saveCmd",     false, "string", "Save command");

  // Presets (degrees)
  config.add("stages.names", "",     "stages.names", argType::Optional, "stages", "names", false, "vector<string>", "Preset names");
  config.add("stages.positions", "", "stages.positions", argType::Optional, "stages", "positions", false, "vector<double>", "Preset positions (deg)");

  // Bases
  dev::stdMotionStage<rotationStageCtrl>::setupConfig(config);
  dev::telemeter<rotationStageCtrl>::setupConfig(config);
}

inline void rotationStageCtrl::loadConfig()
{
  config(m_port, "stage.port");
  config(m_baud, "stage.baud");

  std::string a; config(a, "stage.address");
  if(!a.empty()) {
    char c = (char)std::toupper((unsigned char)a[0]);
    if(std::isxdigit((unsigned char)c)) m_addr = c;
  }

  config(m_readTimeoutMs, "serial.readTimeoutMs");
  config(m_postWriteSleepMs, "serial.postWriteSleepMs");
  config(m_homeOffsetDeg, "stage.homeOffset");
  config(m_allowMultiturn, "stage.allowMultiturn");

  uint32_t ppr=0; config(ppr, "stage.pulsesPerRev"); if(ppr) m_pulsesPerRev = ppr;

  config(m_velPercent, "motion.velPercent");
  if(m_velPercent < 0) m_velPercent = 0;
  if(m_velPercent > 100) m_velPercent = 100;

  config(m_userPresetNames, "stages.names");
  config(m_userPresetDeg,   "stages.positions");

  // Adapt to stdMotionStage (numeric preset index 1..N)
  m_presetNames.clear(); m_presetPositions.clear();
  if(!m_userPresetNames.empty() && m_userPresetNames.size()==m_userPresetDeg.size()) {
    m_presetNames = m_userPresetNames;
    m_presetPositions.resize(m_userPresetDeg.size());
    for(size_t i=0;i<m_userPresetDeg.size();++i) m_presetPositions[i] = static_cast<float>(i+1);
  }

  dev::stdMotionStage<rotationStageCtrl>::loadConfig(config);
  dev::telemeter<rotationStageCtrl>::loadConfig(config);
}

/* ---------- Startup/Logic ---------- */

inline int rotationStageCtrl::appStartup()
{
  if(state() == stateCodes::UNINITIALIZED)
    return log<text_log,-1>("UNINITIALIZED in appStartup", logPrio::LOG_CRITICAL);

  // absDeg: current/target (user writes target)
  CREATE_REG_INDI_NEW_NUMBERD(m_ipAbsDeg,  "absDeg",     0.0,  720.0, 0.001, "", "Absolute (deg)", "rotation");

  // relDeg: target-only (manual registration to avoid macro mismatch)
  m_ipRelDeg = pcf::IndiProperty(pcf::IndiProperty::Number);
  m_ipRelDeg.setName("relDeg");
  m_ipRelDeg.addIfNoExist(pcf::IndiElement("target", 0.0));
  if(registerIndiPropertyNew(m_ipRelDeg,
                             std::string("relDeg"),
                             pcf::IndiProperty::Number,
                             pcf::IndiProperty::ReadWrite,
                             INDI_IDLE,
                             rotationStageCtrl::st_newCallBack_m_ipRelDeg) < 0)
  {
    return log<software_error,-1>({__FILE__,__LINE__,"register relDeg failed"});
  }

  CREATE_REG_INDI_NEW_NUMBERI(m_ipVelPct,  "velocity",   0,    100,   1,     "", "Velocity (%)",   "rotation");

  // STATUS text (current only)
  m_ipStatus = pcf::IndiProperty(pcf::IndiProperty::Text);
  m_ipStatus.setName("status");
  m_ipStatus.addIfNoExist(pcf::IndiElement("current", "INITIALIZED"));
  REG_INDI_NEWPROP_NOCB(m_ipStatus, "status", pcf::IndiProperty::Text);

  // Toggles
  CREATE_REG_INDI_NEW_REQUESTSWITCH(m_ipOptimize, "optimize");
  CREATE_REG_INDI_NEW_REQUESTSWITCH(m_ipSave,     "save");

  indi::updateIfChanged(m_ipVelPct, "current", m_velPercent, m_indiDriver, INDI_IDLE);
  indi::updateIfChanged(m_ipAbsDeg, "current", m_posDeg,     m_indiDriver, INDI_IDLE);

  // Bases (adds home/stop + preset widgets)
  if(dev::stdMotionStage<rotationStageCtrl>::appStartup() < 0) return log<software_critical,-1>({__FILE__,__LINE__});
  if(dev::telemeter<rotationStageCtrl>::appStartup() < 0)      return log<software_error,-1>({__FILE__,__LINE__});

  updateStatus_();
  return 0;
}

inline int rotationStageCtrl::appLogic()
{
  if(state() == stateCodes::INITIALIZED)
    return log<text_log,-1>("In appLogic but INITIALIZED", logPrio::LOG_CRITICAL);

  if(powerState() != 1 || powerStateTarget() != 1) return 0;

  if(!m_connected) {
    if(m_startupDelayMs) std::this_thread::sleep_for(std::chrono::milliseconds(m_startupDelayMs));
    if(openPort_() == 0) {
      m_connected = true;
      state(stateCodes::CONNECTED);
      log<text_log>("rotationStageCtrl connected on " + m_port);

      drainInput_();
      (void)q_info_();
      (void)q_position_();
      (void)q_status_();
      (void)cmd_setvel_(m_velPercent);
      (void)q_status_();

      if(!m_userPresetDeg.empty()) this->m_preset = presetNumber(); else this->m_preset = 0.0f;

      m_moving = (m_homed ? 0 : -1);
      updateStatus_();
    } else {
      state(stateCodes::NOTCONNECTED);
      m_moving = -2;
      updateStatus_();
      return 0;
    }
  }

  if(pollDevice_() < 0) {
    closePort_();
    m_connected = false;
    state(stateCodes::NOTCONNECTED);
    m_moving = -2;
    updateStatus_();
    return 0;
  }

  // Push abs position
  indi::updateIfChanged(m_ipAbsDeg, "current", m_posDeg, m_indiDriver, (m_gs==0x09?INDI_BUSY:INDI_IDLE));

  // Base widgets and telemetry
  (void)dev::stdMotionStage<rotationStageCtrl>::updateINDI();
  (void)dev::telemeter<rotationStageCtrl>::appLogic();

  return 0;
}

/* ---------- stdMotionStage surface ---------- */

inline int rotationStageCtrl::stop()
{
  int rc = cmd_stop_();
  if(rc == 0) {
    m_pending = Pending::None;
    m_moving = 0;
    // reflect stop button off
    indi::updateSwitchIfChanged(this->m_indiP_stop, "request", pcf::IndiElement::Off, m_indiDriver, INDI_IDLE);
    updateStatus_();
  }
  return rc;
}

inline int rotationStageCtrl::startHoming()
{
  // Home toggle behavior: stdMotionStage sets On; issue, wait until GS!=Busy, apply offset, set Off.
  int rc = cmd_home_(0);
  if(rc < 0) return rc;

  for(;;) {
    if(q_status_() < 0) break;
    if(m_gs != 0x09) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  (void)q_position_();
  m_homed = true;
  m_moving = 0;

  if(std::abs(m_homeOffsetDeg) > 0.0) {
    if(moveRelDeg_(m_homeOffsetDeg) == 0) {
      for(;;) {
        if(q_status_() < 0) break;
        if(m_gs != 0x09) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      (void)q_position_();
    }
  }

  indi::updateSwitchIfChanged(this->m_indiP_home, "request", pcf::IndiElement::Off, m_indiDriver, INDI_IDLE);

  updateStatus_();
  return 0;
}

inline float rotationStageCtrl::presetNumber()
{
  if(m_userPresetDeg.empty()) return -1.0f;
  double best = 1e300; size_t idx = 0;
  for(size_t i=0;i<m_userPresetDeg.size();++i){
    double d = std::abs(m_userPresetDeg[i] - m_posDeg);
    if(d < best){ best = d; idx = i; }
  }
  return static_cast<float>(idx + 1);
}

inline int rotationStageCtrl::moveTo(float presetIndex)
{
  if(m_userPresetDeg.empty()) return -1;
  int idx = static_cast<int>(std::lround(presetIndex)) - 1;
  if(idx < 0) idx = 0;
  if(idx >= (int)m_userPresetDeg.size()) idx = (int)m_userPresetDeg.size()-1;

  this->m_preset_target = static_cast<float>(idx + 1);
  m_moving = 1;
  return moveAbsDeg_(m_userPresetDeg[idx]);
}

/* ---------- telemeter wrappers ---------- */

inline int rotationStageCtrl::checkRecordTimes()
{
  return dev::telemeter<rotationStageCtrl>::checkRecordTimes(telem_stage());
}

inline int rotationStageCtrl::recordTelem(const telem_stage *)
{
  return recordStage(true);
}

inline int rotationStageCtrl::recordStage(bool force)
{
  return dev::stdMotionStage<rotationStageCtrl>::recordStage(force);
}

/* ---------- INDI NEW callbacks (ours) ---------- */

INDI_NEWCALLBACK_DEFN(rotationStageCtrl, m_ipAbsDeg)(const pcf::IndiProperty &ipRecv)
{
  INDI_VALIDATE_CALLBACK_PROPS(m_ipAbsDeg, ipRecv);
  double tgt = 0.0;
  if(indiTargetUpdate(m_ipAbsDeg, tgt, ipRecv, true) < 0) return log<software_error,-1>({__FILE__,__LINE__});
  indi::updateIfChanged(m_ipAbsDeg, "target", tgt, m_indiDriver, INDI_BUSY);

  if(!m_userPresetDeg.empty()) {
    size_t best=0; double err=1e300;
    for(size_t i=0;i<m_userPresetDeg.size();++i){
      double e=std::abs(m_userPresetDeg[i]-tgt);
      if(e<err){err=e; best=i;}
    }
    this->m_preset_target = static_cast<float>(best+1);
  } else {
    this->m_preset_target = 0.0f;
  }

  if(moveAbsDeg_(tgt) < 0) return log<software_error,-1>({__FILE__,__LINE__,"moveAbsDeg failed"});
  return 0;
}

INDI_NEWCALLBACK_DEFN(rotationStageCtrl, m_ipRelDeg)(const pcf::IndiProperty &ipRecv)
{
  // relDeg target-only
  if(ipRecv.getName() != m_ipRelDeg.getName()) return -1;
  if(!ipRecv.find("target")) return 0;
  double d = ipRecv.at("target").getValue<double>();
  indi::updateIfChanged(m_ipRelDeg, "target", d, m_indiDriver, INDI_BUSY);
  this->m_preset_target = this->m_preset;
  if(moveRelDeg_(d) < 0) return log<software_error,-1>({__FILE__,__LINE__,"moveRelDeg failed"});
  return 0;
}

INDI_NEWCALLBACK_DEFN(rotationStageCtrl, m_ipVelPct)(const pcf::IndiProperty &ipRecv)
{
  INDI_VALIDATE_CALLBACK_PROPS(m_ipVelPct, ipRecv);
  int pct = 0;
  if(indiTargetUpdate(m_ipVelPct, pct, ipRecv, true) < 0) return log<software_error,-1>({__FILE__,__LINE__});
  if(pct < 0) { pct = 0; }
  if(pct > 100) { pct = 100; }
  if(cmd_setvel_(pct) < 0) return log<software_error,-1>({__FILE__,__LINE__,"setvel failed"});
  m_velPercent = pct;
  indi::updateIfChanged(m_ipVelPct, "current", m_velPercent, m_indiDriver, INDI_IDLE);
  indi::updateIfChanged(m_ipVelPct, "target",  m_velPercent, m_indiDriver, INDI_IDLE);
  return 0;
}

INDI_NEWCALLBACK_DEFN(rotationStageCtrl, m_ipOptimize)(const pcf::IndiProperty &ipRecv)
{
  INDI_VALIDATE_CALLBACK_PROPS(m_ipOptimize, ipRecv);
  if(!ipRecv.find("request")) return 0;
  if(ipRecv.at("request").getSwitchState() == pcf::IndiElement::On) {
    indi::updateSwitchIfChanged(m_ipOptimize, "request", pcf::IndiElement::On, m_indiDriver, INDI_BUSY);
    if(cmd_optimize_wait_() < 0) {
      indi::updateSwitchIfChanged(m_ipOptimize, "request", pcf::IndiElement::Off, m_indiDriver, INDI_IDLE);
    }
  }
  return 0;
}

INDI_NEWCALLBACK_DEFN(rotationStageCtrl, m_ipSave)(const pcf::IndiProperty &ipRecv)
{
  INDI_VALIDATE_CALLBACK_PROPS(m_ipSave, ipRecv);
  if(!ipRecv.find("request")) return 0;
  if(ipRecv.at("request").getSwitchState() == pcf::IndiElement::On) {
    indi::updateSwitchIfChanged(m_ipSave, "request", pcf::IndiElement::On, m_indiDriver, INDI_BUSY);
    (void)cmd_save_();
    indi::updateSwitchIfChanged(m_ipSave, "request", pcf::IndiElement::Off, m_indiDriver, INDI_IDLE);
  }
  return 0;
}

/* ---------- Serial ---------- */

inline speed_t rotationStageCtrl::to_termios_baud_(int b)
{
  switch(b){
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    default: return B9600;
  }
}

inline int rotationStageCtrl::openPort_()
{
  closePort_();
  m_fd = ::open(m_port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if(m_fd < 0) return -1;

  termios tio{};
  if(tcgetattr(m_fd, &tio) < 0) { closePort_(); return -1; }
  cfmakeraw(&tio);
  speed_t sp = to_termios_baud_(m_baud);
  cfsetispeed(&tio, sp);
  cfsetospeed(&tio, sp);
  tio.c_cflag |= (CLOCAL | CREAD);
  tio.c_cflag &= ~CRTSCTS; // no HW flow
  tio.c_cc[VMIN]  = 0;
  tio.c_cc[VTIME] = 0;
  if(tcsetattr(m_fd, TCSANOW, &tio) < 0) { closePort_(); return -1; }
  tcflush(m_fd, TCIOFLUSH);
  return 0;
}

inline void rotationStageCtrl::closePort_()
{
  if(m_fd >= 0) { ::close(m_fd); m_fd = -1; }
}

inline int rotationStageCtrl::drainInput_()
{
  if(m_fd < 0) return -1;
  char tmp[256];
  for(;;) {
    ssize_t n = ::read(m_fd, tmp, sizeof(tmp));
    if(n <= 0) break;
  }
  return 0;
}

inline int rotationStageCtrl::writeAll_(const std::string &s)
{
  if(m_fd < 0) return -1;
  size_t off=0;
  while(off < s.size()) {
    ssize_t w = ::write(m_fd, s.data()+off, s.size()-off);
    if(w < 0) {
      if(errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      return -1;
    }
    off += (size_t)w;
  }
  if(m_postWriteSleepMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(m_postWriteSleepMs));
  return 0;
}

inline int rotationStageCtrl::readFrame_(std::string &out, int timeout_ms)
{
  out.clear();
  if(m_fd < 0) return -1;
  auto start = std::chrono::steady_clock::now();
  char ch;
  while(true) {
    if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count() > timeout_ms)
      return 1; // timeout
    struct pollfd pfd{m_fd, POLLIN, 0};
    int pr = ::poll(&pfd, 1, 25);
    if(pr <= 0) continue;
    ssize_t r = ::read(m_fd, &ch, 1);
    if(r == 1) {
      out.push_back(ch);
      size_t n = out.size();
      if(n >= 2 && out[n-2]=='\r' && out[n-1]=='\n') return 0;
      continue;
    }
  }
}

inline int rotationStageCtrl::txrx_(const std::string& cmd, std::string *reply, int timeout_ms)
{
  std::string f = frame_(cmd);
  if(writeAll_(f) < 0) return -1;
  if(!reply) return 0;
  return readFrame_(*reply, timeout_ms);
}

/* ---------- Protocol queries ---------- */

inline int rotationStageCtrl::q_info_()
{
  std::string r;
  if(txrx_("in", &r, m_readTimeoutMs) != 0) return -1;
  if(r.size() >= 2 && r[r.size()-2]=='\r') r.resize(r.size()-2);

  if(m_pulsesPerRev == 0 && r.size() >= 8) {
    bool hex=true;
    for(size_t i=r.size()-8;i<r.size();++i) {
      if(!std::isxdigit((unsigned char)r[i])) { hex=false; break; }
    }
    if(hex) {
      uint32_t v=0;
      for(size_t i=r.size()-8;i<r.size();++i){
        char c = (char)std::toupper((unsigned char)r[i]);
        v = (v<<4) | (uint32_t)((c<='9')? (c-'0') : (10+(c-'A')));
      }
      if(v) m_pulsesPerRev = v;
    }
  }
  return 0;
}

inline int rotationStageCtrl::q_status_()
{
  std::string r;
  if(txrx_("gs", &r, m_readTimeoutMs) != 0) return -1;
  if(r.size() >= 6 && (r[1]=='G'||r[1]=='g') && (r[2]=='S'||r[2]=='s')
     && std::isxdigit((unsigned char)r[3]) && std::isxdigit((unsigned char)r[4])) {
    auto nyb = [](char c)->uint8_t{ c=(char)std::toupper((unsigned char)c); return (c<='9')? (c-'0') : (10+(c-'A')); };
    m_gs = (uint8_t)((nyb(r[3])<<4) | nyb(r[4]));
  }
  return 0;
}

inline int rotationStageCtrl::q_position_()
{
  std::string r;
  if(txrx_("gp", &r, m_readTimeoutMs) != 0) return -1;
  if(r.size() >= 11 && (r[1]=='P'||r[1]=='p') && (r[2]=='O'||r[2]=='o')) {
    int32_t pulses = 0;
    for(int i=0;i<8;i++){
      char c = (char)std::toupper((unsigned char)r[3+i]);
      if(!std::isxdigit((unsigned char)c)) { pulses = m_posPulses; break; }
      pulses = (pulses<<4) | (int32_t)((c<='9')? (c-'0') : (10+(c-'A')));
    }
    m_posPulses = pulses;
    m_posDeg = pulsesToDeg_(m_posPulses);

    if(!m_userPresetDeg.empty()) this->m_preset = presetNumber(); else this->m_preset = 0.0f;
  }
  return 0;
}

/* ---------- Commands ---------- */

inline int rotationStageCtrl::cmd_home_(uint8_t dir)
{
  char nib = "0123456789ABCDEF"[dir & 0xF];
  std::string r;
  m_pending = Pending::Home;
  m_moving  = 2;
  if(txrx_(std::string("ho") + nib, &r, m_readTimeoutMs) < 0) return -1;
  return 0;
}

inline int rotationStageCtrl::cmd_stop_()
{
  std::string r;
  m_pending = Pending::Stop;
  if(txrx_("st", &r, m_readTimeoutMs) < 0) return -1;
  for(int tries=0; tries<60; ++tries) {
    if(q_status_() == 0 && m_gs != 0x09) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  m_pending = Pending::None;
  return 0;
}

inline int rotationStageCtrl::cmd_optimize_wait_()
{
  std::string r;
  m_pending = Pending::Optimize;
  if(txrx_(m_cmdOptimize, &r, m_readTimeoutMs) < 0) return -1;
  for(;;) {
    if(q_status_() < 0) break;
    if(m_gs != 0x09) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  indi::updateSwitchIfChanged(m_ipOptimize, "request", pcf::IndiElement::Off, m_indiDriver, INDI_IDLE);
  m_pending = Pending::None;
  return 0;
}

inline int rotationStageCtrl::cmd_save_()
{
  std::string r;
  m_pending = Pending::Save;
  int rc = txrx_(m_cmdSave, &r, m_readTimeoutMs);
  m_pending = Pending::None;
  return rc;
}

inline int rotationStageCtrl::cmd_setvel_(int pct)
{
  if(pct < 0) { pct = 0; }
  if(pct > 100) { pct = 100; }
  char buf[3]; std::snprintf(buf, sizeof(buf), "%02X", pct);
  std::string r;
  m_pending = Pending::Velocity;
  int rc = txrx_(std::string("sv") + buf, &r, m_readTimeoutMs);
  m_pending = Pending::None;
  return rc;
}

inline int rotationStageCtrl::cmd_moveAbs_pulses_(int32_t pulses)
{
  char hex[9]; std::snprintf(hex, sizeof(hex), "%08X", (uint32_t)pulses);
  std::string r;
  m_pending = Pending::MoveAbs;
  m_moving  = 1;
  if(txrx_(std::string("ma") + hex, &r, m_readTimeoutMs) < 0) return -1;
  return 0;
}

inline int rotationStageCtrl::cmd_moveRel_pulses_(int32_t pulses)
{
  char hex[9]; std::snprintf(hex, sizeof(hex), "%08X", (uint32_t)pulses);
  std::string r;
  m_pending = Pending::MoveRel;
  m_moving  = 1;
  if(txrx_(std::string("mr") + hex, &r, m_readTimeoutMs) < 0) return -1;
  return 0;
}

/* ---------- Degree wrappers ---------- */

inline int32_t rotationStageCtrl::degToPulses_(double deg) const
{
  if(m_pulsesPerRev == 0) return 0;
  return (int32_t)std::llround((deg/360.0) * (double)m_pulsesPerRev);
}

inline double rotationStageCtrl::pulsesToDeg_(int32_t pulses) const
{
  if(m_pulsesPerRev == 0) return 0.0;
  return (double)pulses * 360.0 / (double)m_pulsesPerRev;
}

inline int rotationStageCtrl::moveAbsDeg_(double deg)
{
  if(!m_allowMultiturn) {
    while(deg < 0.0)   deg += 720.0;
    while(deg >= 720.) deg -= 720.0;
  }
  if(!m_userPresetDeg.empty()) {
    size_t best=0; double err=1e300;
    for(size_t i=0;i<m_userPresetDeg.size();++i){
      double e=std::abs(m_userPresetDeg[i]-deg);
      if(e<err){err=e; best=i;}
    }
    this->m_preset_target = static_cast<float>(best+1);
  } else {
    this->m_preset_target = 0.0f;
  }
  int32_t p = degToPulses_(deg);
  return cmd_moveAbs_pulses_(p);
}

inline int rotationStageCtrl::moveRelDeg_(double ddeg)
{
  int32_t p = degToPulses_(ddeg);
  return cmd_moveRel_pulses_(p);
}

/* ---------- Poll/resolve ---------- */

inline int rotationStageCtrl::pollDevice_()
{
  if(q_position_() < 0) return -1;
  if(q_status_()   < 0) return -1;

  if(m_gs == 0x09) { // BUSY
    switch(m_pending) {
      case Pending::Home:       m_moving = 2; break;
      case Pending::MoveAbs:
      case Pending::MoveRel:
      case Pending::OffsetRel:
      case Pending::Optimize:
      case Pending::Velocity:
      case Pending::Save:
      case Pending::Stop:
        m_moving = 1; break;
      case Pending::None:
        if(m_moving <= 0) m_moving = 1; // external motion
        break;
    }
  } else {            // OK (idle)
    switch(m_pending) {
      case Pending::Home:
        m_homed = true;
        indi::updateSwitchIfChanged(this->m_indiP_home, "request", pcf::IndiElement::Off, m_indiDriver, INDI_IDLE);
        m_pending = Pending::None; m_moving = 0; break;

      case Pending::OffsetRel:
      case Pending::MoveAbs:
      case Pending::MoveRel:
      case Pending::Velocity:
      case Pending::Save:
      case Pending::Stop:
        m_pending = Pending::None; m_moving = 0; break;

      case Pending::Optimize:
        indi::updateSwitchIfChanged(m_ipOptimize, "request", pcf::IndiElement::Off, m_indiDriver, INDI_IDLE);
        m_pending = Pending::None; m_moving = 0; break;

      case Pending::None:
        m_moving = (m_homed ? 0 : -1); break;
    }
  }

  updateStatus_();
  return 0;
}

inline void rotationStageCtrl::updateStatus_()
{
  if(!m_indiDriver) return;

  std::string s;
  if(!m_connected) s = "NOT_CONNECTED";
  else if(m_gs == 0x09) s = (m_pending == Pending::Home ? "HOMING" : "BUSY");
  else if(!m_homed)     s = "NOT_HOMED";
  else                  s = "OK";

  bool changed=false;
  if(!m_ipStatus.find("current")) {
    m_ipStatus.addIfNoExist(pcf::IndiElement("current", s)); changed=true;
  } else {
    const std::string cur = m_ipStatus.at("current").getValue<std::string>();
    if(cur != s) { m_ipStatus.at("current").setValue(s); changed=true; }
  }
  if(changed) {
    m_ipStatus.setState(INDI_IDLE);
    m_ipStatus.setTimeStamp(pcf::TimeStamp());
    m_indiDriver->sendSetProperty(m_ipStatus);
  }
}

} // namespace app
} // namespace MagAOX

#endif // rotationStageCtrl_hpp

