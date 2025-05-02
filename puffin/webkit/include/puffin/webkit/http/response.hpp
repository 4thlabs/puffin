#ifndef PUFFIN_WEBKIT_RESPONSE_HPP
#define PUFFIN_WEBKIT_RESPONSE_HPP

#include <puffin/webkit/http/header.hpp>
#include <puffin/webkit/http/cookie.hpp>

#include <string>
#include <unordered_map>

namespace puffin {
namespace webkit {

template<typename R, typename Container = std::string>
class basic_response :  {
public:
  using header_map = std::unordered_map<std::string, header>;
  using cookie_map = std::unordered_map<std::string, cookie>;

  basic_response()
      : version_("HTTP/1.1") {
    // Default Content-Length
  }

  virtual ~basic_response() {}

  void add_header(header&& h) {
    headers_[h.name()] = std::move(h);
  }

  header_map& headers() {
    return headers_;
  }

  void add_cookie(cookie&& c) {
    cookies_[c.name()] = std::move(c);
  }

  cookie_map& cookies() {
    return cookies_;
  }

  void status_code(int code) {
    status_code_ = code;
  }

  int status_code() {
    return status_code_;
  }

  // Set the status of the response
  void status(int code, const std::string& value) {
    status_code_ = code;
    status_ = std::to_string(code) + " " + value;
  }

  // Set the body of the response
  void body(const std::string& body) {
    body_ = body;
    headers_["Content-Length"] = header("Content-Length", std::to_string(body_.size()));
  }

  void append_body(const char* data, size_t size) {
    body_.append(data, size); // TODO: Optimize;
  }

  const std::string& body() {
    return body_;
  }

private:
  header_map   headers_;
  cookie_map   cookies_;
  int          status_code_;
  std::string  version_;
  std::string  status_;

  Container  body_;
};

template<typename R>
using response = basic_response<R>;

} // namespace webkit
} // namespace puffin


#endif // PUFFIN_WEBKIT_RESPONSE_HPP
