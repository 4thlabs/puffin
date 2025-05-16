#ifndef PUFFIN_RANGES_TO_VECTOR_HPP
#define PUFFIN_RANGES_TO_VECTOR_HPP

#include <ranges>

namespace puffin {
namespace ranges {

/**
 * @brief A to_vector implementation (C++23 alike)
 */
struct to_vector_impl {

  struct closure {
    template<std::ranges::range R>
    constexpr auto operator()(R&& r) const {
      auto rc = r | std::views::common;
      std::vector<std::ranges::range_value_t<R>> res;

      if constexpr (requires { std::ranges::size(r); }) {
        res.reserve(std::ranges::size(r));
      }

      res.insert(res.begin(), rc.begin(), rc.end());

      return res;
    }
  };

  auto operator()() const -> closure
  {
    return closure{};
  }
};

inline constexpr auto to_vector = to_vector_impl{};

template<std::ranges::range R>
auto constexpr operator|(R&& r, const to_vector_impl::closure& c)
{
  return c(std::forward<R>(r));
}

} // namespace ranges
} // namespace puffin

#endif // PUFFIN_RANGES_TO_VECTOR_HPP
