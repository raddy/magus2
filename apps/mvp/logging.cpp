#include "mvp/logging.hpp"

#include <fmtlog/fmtlog.h>
#include <fmtlog/fmtlog-inl.h>

#include <cstdio>

namespace magus2::mvp::logging {

void start() {
  fmtlog::setLogFile(stdout, false);
  fmtlog::setLogLevel(fmtlog::DBG);
  fmtlog::setHeaderPattern("{HMSf} {l}[{t}] ");
  fmtlog::startPollingThread(1000000);
}

void stop() {
  fmtlog::stopPollingThread();
  fmtlog::poll(true);
}

}  // namespace magus2::mvp::logging
