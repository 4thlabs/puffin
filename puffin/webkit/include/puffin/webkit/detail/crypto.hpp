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

#ifndef PUFFIN_WEBKIT_DETAIL_CRYPTO_HPP
#define PUFFIN_WEBKIT_DETAIL_CRYPTO_HPP

// Minimal header-only SHA-256 (FIPS 180-4) and HMAC-SHA256 (RFC 2104), used to sign session cookies without
// depending on OpenSSL. Not meant as a general purpose crypto library.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace puffin {
namespace webkit {
namespace detail {

using sha256_digest = std::array<std::uint8_t, 32>;

class sha256 {
public:
  sha256() { reset(); }

  void reset()
  {
    state_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    size_ = 0;
    buffered_ = 0;
  }

  void update(std::string_view data)
  {
    for (char c : data) {
      block_[buffered_++] = static_cast<std::uint8_t>(c);

      if (buffered_ == block_.size()) {
        transform();
        buffered_ = 0;
      }
    }

    size_ += data.size();
  }

  sha256_digest finish()
  {
    std::uint64_t bits = size_ * 8;

    block_[buffered_++] = 0x80;

    if (buffered_ > 56) {
      while (buffered_ < 64)
        block_[buffered_++] = 0;

      transform();
      buffered_ = 0;
    }

    while (buffered_ < 56)
      block_[buffered_++] = 0;

    for (int i = 7; i >= 0; --i)
      block_[buffered_++] = static_cast<std::uint8_t>(bits >> (i * 8));

    transform();

    sha256_digest digest;

    for (std::size_t i = 0; i < 8; ++i) {
      digest[i * 4] = static_cast<std::uint8_t>(state_[i] >> 24);
      digest[i * 4 + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
      digest[i * 4 + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
      digest[i * 4 + 3] = static_cast<std::uint8_t>(state_[i]);
    }

    reset();
    return digest;
  }

  static sha256_digest hash(std::string_view data)
  {
    sha256 h;
    h.update(data);
    return h.finish();
  }

private:
  static constexpr std::uint32_t rotr(std::uint32_t x, int n) noexcept { return (x >> n) | (x << (32 - n)); }

  void transform()
  {
    static constexpr std::uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    std::uint32_t w[64];

    for (std::size_t i = 0; i < 16; ++i) {
      w[i] = (std::uint32_t(block_[i * 4]) << 24) | (std::uint32_t(block_[i * 4 + 1]) << 16) |
             (std::uint32_t(block_[i * 4 + 2]) << 8) | std::uint32_t(block_[i * 4 + 3]);
    }

    for (std::size_t i = 16; i < 64; ++i) {
      std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    auto [a, b, c, d, e, f, g, h] = state_;

    for (std::size_t i = 0; i < 64; ++i) {
      std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      std::uint32_t ch = (e & f) ^ (~e & g);
      std::uint32_t t1 = h + s1 + ch + k[i] + w[i];
      std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      std::uint32_t t2 = s0 + maj;

      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  std::array<std::uint32_t, 8> state_;
  std::array<std::uint8_t, 64> block_{};
  std::uint64_t size_ = 0;
  std::size_t buffered_ = 0;
};

inline sha256_digest hmac_sha256(std::string_view key, std::string_view message)
{
  std::array<std::uint8_t, 64> block{};

  if (key.size() > block.size()) {
    auto hashed = sha256::hash(key);
    std::copy(hashed.begin(), hashed.end(), block.begin());
  } else {
    for (std::size_t i = 0; i < key.size(); ++i)
      block[i] = static_cast<std::uint8_t>(key[i]);
  }

  std::string ipad(64, '\0'), opad(64, '\0');

  for (std::size_t i = 0; i < block.size(); ++i) {
    ipad[i] = static_cast<char>(block[i] ^ 0x36);
    opad[i] = static_cast<char>(block[i] ^ 0x5c);
  }

  sha256 inner;
  inner.update(ipad);
  inner.update(message);
  auto inner_digest = inner.finish();

  sha256 outer;
  outer.update(opad);
  outer.update(std::string_view(reinterpret_cast<const char*>(inner_digest.data()), inner_digest.size()));
  return outer.finish();
}

/**
 * @brief Compares two strings in a time that does not depend on where they differ
 */
inline bool constant_time_equal(std::string_view lhs, std::string_view rhs) noexcept
{
  if (lhs.size() != rhs.size())
    return false;

  unsigned char diff = 0;

  for (std::size_t i = 0; i < lhs.size(); ++i)
    diff |= static_cast<unsigned char>(lhs[i] ^ rhs[i]);

  return diff == 0;
}

/**
 * @brief Base64url without padding (RFC 4648 5), cookie safe
 */
inline std::string base64url_encode(std::string_view data)
{
  static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string out;
  out.reserve((data.size() + 2) / 3 * 4);

  std::size_t i = 0;

  for (; i + 2 < data.size(); i += 3) {
    std::uint32_t n = (std::uint32_t(std::uint8_t(data[i])) << 16) | (std::uint32_t(std::uint8_t(data[i + 1])) << 8) |
                      std::uint32_t(std::uint8_t(data[i + 2]));
    out += alphabet[(n >> 18) & 63];
    out += alphabet[(n >> 12) & 63];
    out += alphabet[(n >> 6) & 63];
    out += alphabet[n & 63];
  }

  if (i + 1 == data.size()) {
    std::uint32_t n = std::uint32_t(std::uint8_t(data[i])) << 16;
    out += alphabet[(n >> 18) & 63];
    out += alphabet[(n >> 12) & 63];
  } else if (i + 2 == data.size()) {
    std::uint32_t n = (std::uint32_t(std::uint8_t(data[i])) << 16) | (std::uint32_t(std::uint8_t(data[i + 1])) << 8);
    out += alphabet[(n >> 18) & 63];
    out += alphabet[(n >> 12) & 63];
    out += alphabet[(n >> 6) & 63];
  }

  return out;
}

inline std::string base64url_encode(const sha256_digest& digest)
{
  return base64url_encode(std::string_view(reinterpret_cast<const char*>(digest.data()), digest.size()));
}

} // namespace detail
} // namespace webkit
} // namespace puffin

#endif // PUFFIN_WEBKIT_DETAIL_CRYPTO_HPP
