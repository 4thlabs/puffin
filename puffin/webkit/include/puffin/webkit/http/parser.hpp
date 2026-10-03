//  ____         __  __ _
// |  _ \ _   _ / _|/ _(_)_ __
// | |_) | | | | |_| |_| | '_  |
// |  __/| |_| |  _|  _| | | | |
// |_|    \__,_|_| |_| |_|_| |_|
//
// BSD 3-Clause License

// Copyright (c) 2025, Thomas Gourgues (thomas.gourgues@gmail.com)
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

#ifndef PUFFIN_WEBKIT_HTTP_PARSER_HPP
#define PUFFIN_WEBKIT_HTTP_PARSER_HPP

#include <puffin/webkit/detail/string.hpp>
#include <puffin/webkit/http/request.hpp>
#include <puffin/webkit/http/response.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace puffin {
namespace webkit {

enum class parse_status {
  need_more, ///< All the input was consumed, the message is not complete yet
  done,      ///< The message is complete, remaining input belongs to the next message
  error      ///< The input is not a valid HTTP message, see parser::error()
};

enum class parse_error {
  none,
  bad_start_line,
  bad_version,
  bad_status_code,
  bad_header,
  bad_content_length,
  bad_transfer_encoding,
  bad_chunk,
  header_too_large,
  body_too_large,
  unexpected_eof
};

struct parse_result {
  parse_status status;
  std::size_t consumed; ///< Number of bytes of the input that belong to the message
};

struct parser_limits {
  std::size_t max_header_size = 64 * 1024;      ///< Start line and headers
  std::size_t max_body_size = 8 * 1024 * 1024;
};

/**
 * @brief Incremental HTTP/1.x parser, without any I/O.
 *
 * Bytes are pushed with feed() as they arrive from the transport. The parser handles Content-Length and chunked
 * bodies, and for responses, bodies delimited by the end of the connection (call finish() on end of stream).
 * Once done, reset() prepares the parser for the next message on the same connection.
 */
template<typename Message>
class basic_parser {
  static_assert(std::is_same_v<Message, request> || std::is_same_v<Message, response>,
                "basic_parser handles request or response");

  static constexpr bool is_request = std::is_same_v<Message, request>;

  /// Chunk size lines, with their extensions
  static constexpr std::size_t max_chunk_line_size = 4096;

  enum class state {
    start_line,
    headers,
    body,
    chunk_size,
    chunk_data,
    chunk_data_end,
    trailers,
    body_until_eof,
    done,
    error
  };

public:
  explicit basic_parser(parser_limits limits = {})
      : limits_(limits)
  {}

  /// Pushes new bytes to the parser
  parse_result feed(std::string_view data)
  {
    if (state_ == state::done)
      return {parse_status::done, 0};

    if (state_ == state::error)
      return {parse_status::error, 0};

    buffer_.append(data.data(), data.size());
    run();

    if (state_ == state::error)
      return {parse_status::error, data.size()};

    if (state_ == state::done) {
      // Bytes left in the buffer belong to the next message, they all come from this input
      std::size_t leftover = buffer_.size() - pos_;
      buffer_.clear();
      pos_ = 0;
      return {parse_status::done, data.size() - leftover};
    }

    compact();
    return {parse_status::need_more, data.size()};
  }

  /// Signals the end of the stream
  parse_status finish()
  {
    if (state_ == state::body_until_eof) {
      state_ = state::done;
    } else if (state_ != state::done && state_ != state::error) {
      fail(parse_error::unexpected_eof);
    }

    return status();
  }

  /// For responses to a HEAD request: headers describe a body that is not sent
  void skip_body(bool skip = true) { skip_body_ = skip; }

  /// True if no byte of the current message was received yet
  bool idle() const { return state_ == state::start_line && buffer_.empty(); }

  bool done() const { return state_ == state::done; }

  parse_status status() const
  {
    if (state_ == state::done)
      return parse_status::done;

    if (state_ == state::error)
      return parse_status::error;

    return parse_status::need_more;
  }

  parse_error error() const { return error_; }

  const Message& message() const { return message_; }
  Message& message() { return message_; }

  /// Moves the parsed message out and resets the parser
  Message release()
  {
    Message m = std::move(message_);
    reset();
    return m;
  }

  void reset()
  {
    message_ = Message{};
    state_ = state::start_line;
    error_ = parse_error::none;
    buffer_.clear();
    pos_ = 0;
    header_size_ = 0;
    remaining_ = 0;
    skip_body_ = false;
  }

private:
  void run()
  {
    bool progress = true;

    while (progress && state_ != state::done && state_ != state::error) {
      switch (state_) {
        case state::start_line: progress = parse_start_line(); break;
        case state::headers: progress = parse_header_line(); break;
        case state::body: progress = parse_body(); break;
        case state::chunk_size: progress = parse_chunk_size(); break;
        case state::chunk_data: progress = parse_chunk_data(); break;
        case state::chunk_data_end: progress = parse_chunk_data_end(); break;
        case state::trailers: progress = parse_trailer_line(); break;
        case state::body_until_eof: progress = parse_body_until_eof(); break;
        default: progress = false; break;
      }
    }
  }

  /// Extracts a line ending with CRLF (or LF), nullopt if not complete yet
  std::optional<std::string_view> next_line(bool counts_in_header)
  {
    std::string_view pending(buffer_.data() + pos_, buffer_.size() - pos_);
    auto lf = pending.find('\n');

    if (lf == std::string_view::npos) {
      if (counts_in_header && header_size_ + pending.size() > limits_.max_header_size)
        fail(parse_error::header_too_large);
      else if (!counts_in_header && pending.size() > max_chunk_line_size)
        fail(parse_error::bad_chunk);
      return std::nullopt;
    }

    pos_ += lf + 1;

    if (counts_in_header) {
      header_size_ += lf + 1;

      if (header_size_ > limits_.max_header_size) {
        fail(parse_error::header_too_large);
        return std::nullopt;
      }
    }

    std::string_view line = pending.substr(0, lf);

    if (!line.empty() && line.back() == '\r')
      line.remove_suffix(1);

    return line;
  }

  static bool is_token_char(char c)
  {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
      return true;

    switch (c) {
      case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+':
      case '-': case '.': case '^': case '_': case '`': case '|': case '~':
        return true;
      default:
        return false;
    }
  }

  static bool is_token(std::string_view s)
  {
    if (s.empty())
      return false;

    for (char c : s) {
      if (!is_token_char(c))
        return false;
    }

    return true;
  }

  static std::optional<version> parse_version(std::string_view s)
  {
    if (s.size() != 8 || s.substr(0, 5) != "HTTP/" || s[6] != '.')
      return std::nullopt;

    if (s[5] < '0' || s[5] > '9' || s[7] < '0' || s[7] > '9')
      return std::nullopt;

    return version{s[5] - '0', s[7] - '0'};
  }

  bool parse_start_line()
  {
    auto line = next_line(true);

    if (!line)
      return false;

    // Robustness (RFC 9112 2.2): ignore empty lines before the start line
    if (line->empty())
      return true;

    if constexpr (is_request) {
      // method SP request-target SP HTTP-version
      auto sp1 = line->find(' ');
      auto sp2 = line->rfind(' ');

      if (sp1 == std::string_view::npos || sp1 == sp2)
        return fail(parse_error::bad_start_line);

      auto method = line->substr(0, sp1);
      auto target = line->substr(sp1 + 1, sp2 - sp1 - 1);
      auto v = parse_version(line->substr(sp2 + 1));

      if (!is_token(method) || target.empty() || target.find(' ') != std::string_view::npos)
        return fail(parse_error::bad_start_line);

      if (!v)
        return fail(parse_error::bad_version);

      message_.method(std::string(method));
      message_.target(std::string(target));
      message_.version(*v);
    } else {
      // HTTP-version SP status-code SP [reason-phrase]
      auto sp1 = line->find(' ');

      if (sp1 == std::string_view::npos)
        return fail(parse_error::bad_start_line);

      auto v = parse_version(line->substr(0, sp1));

      if (!v)
        return fail(parse_error::bad_version);

      auto rest = line->substr(sp1 + 1);
      auto code = rest.substr(0, 3);

      if (code.size() != 3 || (rest.size() > 3 && rest[3] != ' '))
        return fail(parse_error::bad_status_code);

      auto value = detail::parse_decimal(code);

      if (!value || *value < 100)
        return fail(parse_error::bad_status_code);

      message_.version(*v);
      message_.status(static_cast<int>(*value), std::string(rest.size() > 4 ? rest.substr(4) : std::string_view()));
    }

    state_ = state::headers;
    return true;
  }

  bool parse_field(std::string_view line, headers& target)
  {
    // Obsolete line folding is rejected (RFC 9112 5.2)
    if (line.front() == ' ' || line.front() == '\t')
      return fail(parse_error::bad_header);

    auto colon = line.find(':');

    if (colon == std::string_view::npos)
      return fail(parse_error::bad_header);

    auto name = line.substr(0, colon);

    if (!is_token(name))
      return fail(parse_error::bad_header);

    target.add(std::string(name), std::string(detail::trim(line.substr(colon + 1))));
    return true;
  }

  bool parse_header_line()
  {
    auto line = next_line(true);

    if (!line)
      return false;

    if (!line->empty())
      return parse_field(*line, message_.headers());

    return start_body();
  }

  bool start_body()
  {
    const auto& h = message_.headers();

    if constexpr (!is_request) {
      int code = message_.status_code();

      if (skip_body_ || (code >= 100 && code < 200) || code == 204 || code == 304)
        return complete();
    }

    auto encodings = h.get_all("Transfer-Encoding");

    if (!encodings.empty()) {
      // Transfer-Encoding with Content-Length is a request smuggling vector (RFC 9112 6.1)
      if (is_request && h.contains("Content-Length"))
        return fail(parse_error::bad_transfer_encoding);

      // chunked must be the final encoding
      std::string_view last = detail::trim(encodings.back());
      auto comma = last.rfind(',');

      if (comma != std::string_view::npos)
        last = detail::trim(last.substr(comma + 1));

      if (detail::iequals(last, "chunked")) {
        state_ = state::chunk_size;
        return true;
      }

      if (is_request)
        return fail(parse_error::bad_transfer_encoding);

      state_ = state::body_until_eof;
      return true;
    }

    auto lengths = h.get_all("Content-Length");

    if (!lengths.empty()) {
      std::optional<std::size_t> length;

      for (auto value : lengths) {
        auto parsed = detail::parse_decimal(detail::trim(value));

        if (!parsed || (length && *length != *parsed))
          return fail(parse_error::bad_content_length);

        length = parsed;
      }

      if (*length > limits_.max_body_size)
        return fail(parse_error::body_too_large);

      if (*length == 0)
        return complete();

      message_.body().reserve(*length);
      remaining_ = *length;
      state_ = state::body;
      return true;
    }

    if constexpr (is_request) {
      return complete();
    } else {
      state_ = state::body_until_eof;
      return true;
    }
  }

  std::size_t take(std::size_t max)
  {
    std::size_t n = std::min(max, buffer_.size() - pos_);
    message_.body().append(buffer_.data() + pos_, n);
    pos_ += n;
    return n;
  }

  bool parse_body()
  {
    remaining_ -= take(remaining_);

    if (remaining_ == 0)
      return complete();

    return false;
  }

  bool parse_chunk_size()
  {
    auto line = next_line(false);

    if (!line)
      return false;

    // chunk-size [ chunk-ext ]
    auto size_part = detail::trim(line->substr(0, line->find(';')));
    auto size = detail::parse_hex(size_part);

    if (!size)
      return fail(parse_error::bad_chunk);

    if (*size == 0) {
      state_ = state::trailers;
      return true;
    }

    if (message_.body().size() + *size > limits_.max_body_size)
      return fail(parse_error::body_too_large);

    remaining_ = *size;
    state_ = state::chunk_data;
    return true;
  }

  bool parse_chunk_data()
  {
    remaining_ -= take(remaining_);

    if (remaining_ != 0)
      return false;

    state_ = state::chunk_data_end;
    return true;
  }

  bool parse_chunk_data_end()
  {
    auto line = next_line(false);

    if (!line)
      return false;

    if (!line->empty())
      return fail(parse_error::bad_chunk);

    state_ = state::chunk_size;
    return true;
  }

  bool parse_trailer_line()
  {
    auto line = next_line(true);

    if (!line)
      return false;

    if (line->empty())
      return complete();

    // Trailer fields are merged into the headers
    return parse_field(*line, message_.headers());
  }

  bool parse_body_until_eof()
  {
    if (message_.body().size() + (buffer_.size() - pos_) > limits_.max_body_size)
      return fail(parse_error::body_too_large);

    take(buffer_.size() - pos_);
    return false;
  }

  bool complete()
  {
    state_ = state::done;
    return false;
  }

  bool fail(parse_error e)
  {
    error_ = e;
    state_ = state::error;
    return false;
  }

  void compact()
  {
    if (pos_ > 0) {
      buffer_.erase(0, pos_);
      pos_ = 0;
    }
  }

private:
  parser_limits limits_;
  Message message_;

  state state_ = state::start_line;
  parse_error error_ = parse_error::none;

  std::string buffer_;
  std::size_t pos_ = 0;
  std::size_t header_size_ = 0;
  std::size_t remaining_ = 0;
  bool skip_body_ = false;
};

using request_parser = basic_parser<request>;
using response_parser = basic_parser<response>;

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_HTTP_PARSER_HPP
