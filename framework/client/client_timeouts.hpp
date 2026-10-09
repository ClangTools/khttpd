#pragma once
#include <chrono>
#include <climits>
#include <stdexcept>
#include <boost/system/error_code.hpp>
namespace khttpd::framework::client {
struct ClientTimeouts {
  std::chrono::seconds total{30}, connect{30}, read{30}, write{30};
};
inline void validate_client_timeouts(const ClientTimeouts& value) {
  for (auto duration : {value.total, value.connect, value.read, value.write})
    if (duration.count() < 0 || duration.count() > INT_MAX)
      throw std::invalid_argument("client timeout must be between 0 and INT_MAX seconds");
}
class ClientTimeoutCategory final : public boost::system::error_category {
 public:
  const char* name() const noexcept override { return "http.client.timeout"; }
  std::string message(int phase) const override {
    switch(phase) {
      case 1: return "HTTP client total timeout";
      case 2: return "HTTP client connect timeout";
      case 3: return "HTTP client read timeout";
      case 4: return "HTTP client write timeout";
      default: return "HTTP client timeout";
    }
  }
  boost::system::error_condition default_error_condition(int) const noexcept override {
    return boost::system::errc::make_error_condition(boost::system::errc::timed_out);
  }
};
inline boost::system::error_code client_timeout_error(int phase) {
  static ClientTimeoutCategory category;
  return {phase,category};
}
}
