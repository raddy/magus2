#include "mvp/logging.hpp"

#ifndef TLOG_NOFMTLOG
#  include <fmtlog.h>
#endif

#include <cstdio>

namespace magus2::mvp::logging {

void start() {
#ifndef TLOG_NOFMTLOG
  fmtlog::setLogFile(stdout, false);
  fmtlog::setLogLevel(fmtlog::DBG);
  fmtlog::setHeaderPattern("{HMSf} {l}[{t}] ");
  fmtlog::startPollingThread(1000000);
#endif
}

void stop() {
#ifndef TLOG_NOFMTLOG
  fmtlog::stopPollingThread();
  fmtlog::poll(true);
#endif
}

}  // namespace magus2::mvp::logging
