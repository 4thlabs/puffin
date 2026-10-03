//  ____         __  __ _       
// |  _ \ _   _ / _|/ _(_)_ __  
// | |_) | | | | |_| |_| | '_  | 
// |  __/| |_| |  _|  _| | | | |
// |_|    \__,_|_| |_| |_|_| |_|          
//
// BSD 3-Clause License

// Copyright (c) 2019, Thomas Gourgues (thomas.gourgues@gmail.com)
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.

// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.

// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef PFN_LOG_H
#define PFN_LOG_H

#include <iostream>
#include <sstream>
#include <mutex>
#include <memory>

#define DEFAULT_LOGGER logging::logger

#define L_DEBUG logging::severity::debug
#define L_INFO  logging::severity::info
#define L_WARN  logging::severity::warn
#define L_ERROR logging::severity::error

namespace logging {

enum class severity {
  debug = 1,
  info,
  warn,
  error,
};

inline std::ostream& operator<<(std::ostream& os, severity sev) {
    switch (sev) {
    case severity::debug:
        os << "\033[34m" << "DEBUG ";
        break;
    case severity::info:
        os << "\033[32m" << "INFOS ";
        break;
    case severity::warn:
        os << "\033[33m" << "WARN ";
        break;
    case severity::error:
        os << "\033[31m" << "ERROR ";
        break;
    default:
        break;
    }
    return os;
}

class output_policy {
public:
  virtual void write(const std::string& message) = 0;
};

class base_message {
public:
  static severity min_severity;
  static std::unique_ptr<output_policy> output;

  base_message(const char* filename, int line, severity sev)
    : filename_(filename),
      line_(line),
      severity_(sev) {}

  virtual ~base_message() {
    stream_ << std::endl;
    output->write(stream_.str());
  }

  static bool loggable(severity sev) {
    return sev >= min_severity;
  }

  std::ostringstream& stream() {
    stream_ << severity_ << filename_ << "[" << line_ << "]" << ": ";
    return stream_;
  }

private:
  std::ostringstream  stream_;
  std::string         filename_;
  int                 line_;
  severity            severity_;
};

class console_output : public output_policy {
public:
  void write(const std::string& message) override {
    std::lock_guard<std::mutex> lock(output_mutex_);
    std::cout << message << std::flush;
  }
private:
  std::mutex output_mutex_;
};

// The following non-obvious technique for implementation of a
// conditional log stream was taken from google3/base/logging.h.

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class log_voidify {
 public:
  log_voidify() { }
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) { }
};

// Default logger, can be overriden by redefining macro DEFAULT_LOGGER
using logger = logging::base_message;

}

#define LOG_SEVERITY_CONDITION(sev) \
    !DEFAULT_LOGGER::loggable(sev) ? (void) 0 : logging::log_voidify() &

#define LOG(sev) \
    LOG_SEVERITY_CONDITION(sev) \
    DEFAULT_LOGGER(__FILE__, __LINE__, sev).stream()

#endif // PFN_LOG_H
