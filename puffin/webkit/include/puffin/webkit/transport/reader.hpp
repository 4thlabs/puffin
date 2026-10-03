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

#ifndef PUFFIN_WEBKIT_TRANSPORT_READER_HPP
#define PUFFIN_WEBKIT_TRANSPORT_READER_HPP

#include <puffin/async.hpp>
#include <puffin/webkit/http/parser.hpp>
#include <puffin/webkit/transport/concepts.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace puffin {
namespace webkit {

/**
 * @brief Reads HTTP messages from a stream into a parser.
 *
 * Bytes received after the end of a message (pipelined requests, or a response followed by the next one) are kept
 * for the next read. One reader per connection, used by the server and the client.
 */
class message_reader {
public:
  explicit message_reader(std::size_t buffer_size)
      : buffer_(buffer_size)
  {}

  /**
   * @brief Feeds the parser until its message is complete or invalid.
   * @return done or error, or need_more if the stream ended first. On end of stream the parser is left as is,
   *         the caller decides between finish() and dropping the message.
   */
  template<Stream S, typename Parser>
  async::async<parse_status> read(S& stream, Parser& parser)
  {
    for (;;) {
      if (begin_ != end_) {
        auto result = parser.feed(std::string_view(buffer_.data() + begin_, end_ - begin_));
        begin_ += result.consumed;

        if (result.status != parse_status::need_more)
          co_return result.status;
      }

      begin_ = 0;
      end_ = co_await stream.read_some(std::span<char>(buffer_));

      if (end_ == 0)
        co_return parse_status::need_more;
    }
  }

  /// Drops the bytes kept for the next message, when the connection is closed
  void clear() { begin_ = end_ = 0; }

private:
  std::vector<char> buffer_;
  std::size_t begin_ = 0; ///< Received bytes not fed to a parser yet: [begin_, end_)
  std::size_t end_ = 0;
};

} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_TRANSPORT_READER_HPP
