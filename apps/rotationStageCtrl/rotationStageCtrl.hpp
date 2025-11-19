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

#include "../../libMagAOX/libMagAOX.hpp"
#include "../../magaox_git_version.h"

namespace MagAOX {
namespace app {

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

  // stdMotionStage CRTP surface
  int startHoming();             // initiate homing
  int stop();                    // immediate stop
  int moveTo(const double &deg); // absolute move
  int presetNumber();            // nearest preset index (0-based) or -1

  // telemeter surface patterned on hsfwCtrl
  int checkRecordTimes();
  int recordTelem(const telem_stage *);
  int recordStage(bool force = false);

  // INDI NEW callbacks (use your macros)
  INDI_NEWCALLBACK_DECL(rotationStageCtrl, m_ipAbsDeg);
  INDI_NEWCALLBACK_DECL(rotationStageCtrl, m_ipRelDeg);
  INDI_NEWCALLBACK_DECL(rotationStageCtrl, m_ipVelPct);
  INDI_NEWCALLBACK_DECL(rotationStageCtrl, m_ipJogStep);

protected:
  // config
  std::string m_port;
  int         m_baud {9600};
  unsigned    m_startupDelayMs {200};
  int         m_velPercent {40};
  double      m_jogStepDeg {1.0};

  // post-home offset (degrees, relative move from home)
  double      m_homeOffsetDeg {0.0};

  // optional presets
  std::vector<std::string> m_presetNames;
  std::vector<double>      m_presetValuesDeg;

  // runtime
  int    m_fd {-1};
  bool   m_connected {false};
  double m_posDeg {0.0};
  bool   m_homed {false};
  int    m_moving {0}; // -1 unknown, 0 idle, 1 moving, 2 homing

  // homing offset state machine
  enum class HomePhase { Idle, WaitingHomeStable, SentOffset, WaitingOffsetStable };
  HomePhase m_homePhase {HomePhase::Idle};
  double    m_lastPosForStable {0.0};
  std::chrono::steady_clock::time_point m_lastMotionTime {};
  static constexpr int kStableMsRequired = 500; // quiet time to consider motion finished
  static constexpr double kStablePosEps  = 1e-3;

  // INDI properties owned here
  pcf::IndiProperty m_ipAbsDeg;   // number current/target
  pcf::IndiProperty m_ipRelDeg;   // number current/target
  pcf::IndiProperty m_ipVelPct;   // number current/target
  pcf::IndiProperty m_ipJogStep;  // number current/target

  // --- Device protocol (adjust strings if your controller differs) ---
  static constexpr const char* CMD_MOVE_ABS = "MA "; // + "<deg>\r\n"
  static constexpr const char* CMD_MOVE_REL = "MR "; // + "<deg>\r\n"
  static constexpr const char* CMD_SET_VEL  = "SV "; // + "<pct>\r\n"
  static constexpr const char* CMD_STOP     = "ST\r\n";
  static constexpr const char* CMD_HOME     = "HM\r\n";
  static constexpr const char* CMD_QUERYPOS = "QP\r\n"; // expects "P=<deg>\n"

  // device I/O
  int  openPort_();
  void closePort_();
  int  writeCmd_(const std::string &frame);
  int  queryStatus_();
  int  moveAbs_(double deg);
  int  moveRel_(double ddeg);
  int  setVelocity_(int pct);
  int  home_();
  int  stop_();

  // helpers
  static speed_t to_termios_baud_(int baud);
};

/* ========================= impl ========================= */

inline rotationStageCtrl::rotationStageCtrl()
: MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
  m_presetNotation  = "deg";
  m_powerMgtEnabled = true;
}

inline void rotationStageCtrl::setupConfig()
{
  config.add("stage.port", "", "stage.port", argType::Required, "stage", "port", false, "string", "Serial device path");
  config.add("stage.baud", "9600", "stage.baud", argType::Required, "stage", "baud", false, "int", "Baud rate");
  config.add("stage.startupDelayMs", "200", "stage.startupDelayMs", argType::Optional, "stage", "startupDelayMs", false, "int", "Delay after power on");

  // home offset (relative from home, in degrees)
  config.add("stage.homeOffset", "0.0", "stage.homeOffset", argType::Optional, "stage", "homeOffset", false, "double", "Relative offset (deg) to move after homing");

  config.add("motion.velPercent", "40",  "motion.velPercent", argType::Optional, "motion", "velPercent", false, "int", "Velocity percent 0..255");
  config.add("motion.jogStepDeg", "1.0", "motion.jogStepDeg", argType::Optional, "motion", "jogStepDeg", false, "double", "Jog step in degrees");

  // Optional preset names/positions (vectors)
  config.add("stages.names", "",     "stages.names", argType::Optional, "stages", "names", false, "vector<string>", "Preset names");
  config.add("stages.positions", "", "stages.positions", argType::Optional, "stages", "positions", false, "vector<double>", "Preset positions (deg)");

  dev::stdMotionStage<rotationStageCtrl>::setupConfig(config);
  dev::telemeter<rotationStageCtrl>::setupConfig(config);
}

inline void rotationStageCtrl::loadConfig()
{
  config(m_port, "stage.port");
  config(m_baud, "stage.baud");
  config(m_startupDelayMs, "stage.startupDelayMs");

  config(m_homeOffsetDeg, "stage.homeOffset");

  config(m_velPercent, "motion.velPercent");
  config(m_jogStepDeg, "motion.jogStepDeg");

  // presets are optional
  config(m_presetNames, "stages.names");
  config(m_presetValuesDeg, "stages.positions");

  dev::stdMotionStage<rotationStageCtrl>::loadConfig(config);
  dev::telemeter<rotationStageCtrl>::loadConfig(config);
}

inline int rotationStageCtrl::appStartup()
{
  if(state() == stateCodes::UNINITIALIZED) {
    log<text_log>("In appStartup but UNINITIALIZED", logPrio::LOG_CRITICAL);
    return -1;
  }

  if(dev::stdMotionStage<rotationStageCtrl>::appStartup() < 0) return log<software_critical,-1>({__FILE__,__LINE__});
  if(dev::telemeter<rotationStageCtrl>::appStartup()   < 0)   return log<software_error,-1>({__FILE__,__LINE__});

  // NEW number props with current/target elements
  CREATE_REG_INDI_NEW_NUMBERD(m_ipAbsDeg,  "absDeg",    -1.0e9, 1.0e9, 0.001, "", "Absolute (deg)", "rotation");
  CREATE_REG_INDI_NEW_NUMBERD(m_ipRelDeg,  "relDeg",    -1.0e9, 1.0e9, 0.001, "", "Relative (deg)", "rotation");
  CREATE_REG_INDI_NEW_NUMBERI(m_ipVelPct,  "velocity",  0,      255,   1,     "", "Velocity (%)",   "rotation");
  CREATE_REG_INDI_NEW_NUMBERD(m_ipJogStep, "jogStepDeg",0.0,    360.0, 0.001, "", "Jog step (deg)", "rotation");

  // seed currents
  try {
    if(m_ipVelPct.find("current"))  m_ipVelPct.at("current").setValue(m_velPercent);
    if(m_ipJogStep.find("current")) m_ipJogStep.at("current").setValue(m_jogStepDeg);
    if(m_ipAbsDeg.find("current"))  m_ipAbsDeg.at("current").setValue(m_posDeg);
  } catch(...) {}

  // initialize stability timer
  m_lastPosForStable = m_posDeg;
  m_lastMotionTime = std::chrono::steady_clock::now();

  return 0;
}

inline int rotationStageCtrl::appLogic()
{
  if(state() == stateCodes::INITIALIZED) {
    log<text_log>("In appLogic but INITIALIZED.", logPrio::LOG_CRITICAL);
    return -1;
  }

  if(powerState() != 1 || powerStateTarget() != 1) return 0;

  if(!m_connected) {
    if(m_startupDelayMs) std::this_thread::sleep_for(std::chrono::milliseconds(m_startupDelayMs));
    if(openPort_() == 0) {
      m_connected = true;
      state(stateCodes::CONNECTED);
      log<text_log>("rotationStageCtrl connected on " + m_port);
      (void)setVelocity_(m_velPercent); // best-effort apply configured velocity
      m_lastPosForStable = m_posDeg;
      m_lastMotionTime = std::chrono::steady_clock::now();
    } else {
      state(stateCodes::NOTCONNECTED);
      return 0;
    }
  }

  // poll device
  if(queryStatus_() < 0) {
    closePort_();
    m_connected = false;
    state(stateCodes::NOTCONNECTED);
    return 0;
  }

  // publish absolute current
  try { if(m_ipAbsDeg.find("current")) m_ipAbsDeg.at("current").setValue(m_posDeg); } catch(...) {}

  // homing offset state machine
  if(m_homePhase != HomePhase::Idle) {
    const auto now = std::chrono::steady_clock::now();
    const auto quiet_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastMotionTime).count();

    switch(m_homePhase) {
      case HomePhase::WaitingHomeStable:
        // when homing motion seems complete, issue relative offset
        if(quiet_ms >= kStableMsRequired) {
          if(std::abs(m_homeOffsetDeg) > 0.0) {
            if(moveRel_(m_homeOffsetDeg) == 0) {
              m_homePhase = HomePhase::SentOffset;
              // next, wait for offset motion to settle
              m_homePhase = HomePhase::WaitingOffsetStable;
            } else {
              // command failed; retry next loop
            }
          } else {
            m_homePhase = HomePhase::Idle;
          }
        }
        break;

      case HomePhase::WaitingOffsetStable:
        if(quiet_ms >= kStableMsRequired) {
          m_homePhase = HomePhase::Idle;
        }
        break;

      case HomePhase::SentOffset: // folded into WaitingOffsetStable above
      case HomePhase::Idle:
        break;
    }
  }

  // std motion/telem
  if(dev::stdMotionStage<rotationStageCtrl>::updateINDI() < 0) log<software_error>({__FILE__,__LINE__});
  if(dev::telemeter<rotationStageCtrl>::appLogic() < 0)       log<software_error>({__FILE__,__LINE__});

  return 0;
}

/* ---- CRTP: stdMotionStage ---- */

inline int rotationStageCtrl::startHoming()
{
  // send device home; when done, we’ll optionally apply stage.homeOffset
  int rc = home_();
  if(rc == 0) {
    m_homePhase = (std::abs(m_homeOffsetDeg) > 0.0) ? HomePhase::WaitingHomeStable : HomePhase::Idle;
    m_lastPosForStable = m_posDeg;
    m_lastMotionTime = std::chrono::steady_clock::now();
  }
  return rc;
}

inline int rotationStageCtrl::stop()
{
  // cancel any pending offset workflow; user explicitly stopped
  m_homePhase = HomePhase::Idle;
  return stop_();
}

inline int rotationStageCtrl::moveTo(const double &deg)
{
  // any commanded absolute move cancels the post-home offset sequence
  m_homePhase = HomePhase::Idle;
  return moveAbs_(deg);
}

inline int rotationStageCtrl::presetNumber()
{
  if(m_presetValuesDeg.empty()) return -1;
  double best = 1e300;
  int idx = -1;
  for(size_t i=0;i<m_presetValuesDeg.size();++i){
    double d = std::abs(m_presetValuesDeg[i] - m_posDeg);
    if(d < best){ best = d; idx = (int)i; }
  }
  return idx;
}

/* ---- CRTP: telemeter ---- */

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

/* ---- INDI NEW callbacks ---- */

INDI_NEWCALLBACK_DEFN(rotationStageCtrl, m_ipAbsDeg)(const pcf::IndiProperty &ipRecv)
{
  INDI_VALIDATE_CALLBACK_PROPS(m_ipAbsDeg, ipRecv);
  try {
    if(!ipRecv.find("target")) return 0;
    double tgt = ipRecv.at("target").getValue<double>();
    if(moveAbs_(tgt) < 0) return log<software_error,-1>({__FILE__,__LINE__,"moveAbs failed"});
    if(m_ipAbsDeg.find("current")) m_ipAbsDeg.at("current").setValue(tgt);
  } catch(...) {
    return log<software_error,-1>({__FILE__,__LINE__,"absDeg callback exception"});
  }
  return 0;
}

INDI_NEWCALLBACK_DEFN(rotationStageCtrl, m_ipRelDeg)(const pcf::IndiProperty &ipRecv)
{
  INDI_VALIDATE_CALLBACK_PROPS(m_ipRelDeg, ipRecv);
  try {
    if(!ipRecv.find("target")) return 0;
    double d = ipRecv.at("target").getValue<double>();
    if(moveRel_(d) < 0) return log<software_error,-1>({__FILE__,__LINE__,"moveRel failed"});
  } catch(...) {
    return log<software_error,-1>({__FILE__,__LINE__,"relDeg callback exception"});
  }
  return 0;
}

INDI_NEWCALLBACK_DEFN(rotationStageCtrl, m_ipVelPct)(const pcf::IndiProperty &ipRecv)
{
  INDI_VALIDATE_CALLBACK_PROPS(m_ipVelPct, ipRecv);
  try {
    if(!ipRecv.find("target")) return 0;
    int pct = ipRecv.at("target").getValue<int>();
    if(pct < 0) pct = 0; if(pct > 255) pct = 255;
    if(setVelocity_(pct) < 0) return log<software_error,-1>({__FILE__,__LINE__,"setVelocity failed"});
    m_velPercent = pct;
    if(m_ipVelPct.find("current")) m_ipVelPct.at("current").setValue(m_velPercent);
  } catch(...) {
    return log<software_error,-1>({__FILE__,__LINE__,"velocity callback exception"});
  }
  return 0;
}

INDI_NEWCALLBACK_DEFN(rotationStageCtrl, m_ipJogStep)(const pcf::IndiProperty &ipRecv)
{
  INDI_VALIDATE_CALLBACK_PROPS(m_ipJogStep, ipRecv);
  try {
    if(!ipRecv.find("target")) return 0;
    double step = ipRecv.at("target").getValue<double>();
    if(step < 0.0) step = 0.0; if(step > 360.0) step = 360.0;
    m_jogStepDeg = step;
    if(m_ipJogStep.find("current")) m_ipJogStep.at("current").setValue(m_jogStepDeg);
  } catch(...) {
    return log<software_error,-1>({__FILE__,__LINE__,"jogStep callback exception"});
  }
  return 0;
}

/* ---- Serial helpers ---- */

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
  tio.c_cc[VMIN]  = 0;
  tio.c_cc[VTIME] = 0;
  if(tcsetattr(m_fd, TCSANOW, &tio) < 0) { closePort_(); return -1; }
  return 0;
}

inline void rotationStageCtrl::closePort_()
{
  if(m_fd >= 0) { ::close(m_fd); m_fd = -1; }
}

inline int rotationStageCtrl::writeCmd_(const std::string &frame)
{
  if(m_fd < 0) return -1;
  ssize_t n = ::write(m_fd, frame.data(), (ssize_t)frame.size());
  if(n < 0) return -1;
  return 0;
}

inline int rotationStageCtrl::queryStatus_()
{
  if(m_fd < 0) return -1;

  // request position
  if(writeCmd_(CMD_QUERYPOS) < 0) return -1;

  // poll for up to 200 ms
  char buf[256]; memset(buf, 0, sizeof(buf));
  struct pollfd pfd{m_fd, POLLIN, 0};
  int pr = ::poll(&pfd, 1, 200);
  if(pr <= 0) return 0; // no update, not fatal

  ssize_t r = ::read(m_fd, buf, sizeof(buf)-1);
  if(r <= 0) return 0;

  // expect "P=<deg>"
  double oldPos = m_posDeg;
  double val = oldPos;
  if(std::sscanf(buf, "P=%lf", &val) == 1) {
    m_posDeg = val;
  }

  // motion/stability heuristic
  if(std::fabs(m_posDeg - oldPos) > kStablePosEps) {
    m_lastMotionTime = std::chrono::steady_clock::now();
    m_moving = 1;
  } else {
    // if no change for long enough, consider idle
    const auto now = std::chrono::steady_clock::now();
    if(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastMotionTime).count() >= kStableMsRequired) {
      if(m_homePhase == HomePhase::WaitingHomeStable) {
        // handled in appLogic state machine
      } else if(m_homePhase == HomePhase::WaitingOffsetStable) {
        // handled in appLogic state machine
      } else {
        m_moving = 0;
      }
    }
  }

  return 0;
}

/* ---- Device commands (edit strings if needed) ---- */

inline int rotationStageCtrl::moveAbs_(double deg)
{
  char line[64];
  std::snprintf(line, sizeof(line), "%s%.6f\r\n", CMD_MOVE_ABS, deg);
  m_moving = 1;
  // issuing a direct move cancels any pending offset workflow
  m_homePhase = HomePhase::Idle;
  return writeCmd_(line);
}

inline int rotationStageCtrl::moveRel_(double ddeg)
{
  char line[64];
  std::snprintf(line, sizeof(line), "%s%.6f\r\n", CMD_MOVE_REL, ddeg);
  m_moving = 1;
  return writeCmd_(line);
}

inline int rotationStageCtrl::setVelocity_(int pct)
{
  char line[32];
  std::snprintf(line, sizeof(line), "%s%d\r\n", CMD_SET_VEL, pct);
  return writeCmd_(line);
}

inline int rotationStageCtrl::home_()
{
  m_moving = 2;
  // begin waiting-for-home-complete phase if we'll need to apply an offset
  if(std::abs(m_homeOffsetDeg) > 0.0) {
    m_homePhase = HomePhase::WaitingHomeStable;
    m_lastMotionTime = std::chrono::steady_clock::now();
  } else {
    m_homePhase = HomePhase::Idle;
  }
  return writeCmd_(CMD_HOME);
}

inline int rotationStageCtrl::stop_()
{
  m_moving = 0;
  return writeCmd_(CMD_STOP);
}

} // namespace app
} // namespace MagAOX

#endif // rotationStageCtrl_hpp

