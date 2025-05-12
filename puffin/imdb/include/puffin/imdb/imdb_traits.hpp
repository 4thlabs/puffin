#ifndef PUFFIN_IMDB_TRAITS_HPP
#define PUFFIN_IMDB_TRAITS_HPP

#include <map>
#include <list>
#include <vector>
#include <functional>

namespace puffin {
namespace imdb {

template<typename Key, typename Value, typename Allocator = std::allocator<Value>>
struct imdb_traits {
  using key_type   = Key;
  using value_type = Value;

  using ref_value_type = std::reference_wrapper<value_type>;

  using data_type = std::list<value_type, Allocator>;
  using view_type = std::vector<ref_value_type>;

  using view_map = std::map<key_type, view_type>;

  using index_map = std::map<key_type, view_map>;

  using fn_filter_type = std::function<bool(const Value&)>;
  using fn_order_type = std::function<bool(const Value&, const Value&)>;
};

} // namespace imdb
} // namespace puffin

#endif // PUFFIN_IMDB_TRAITS_HPP
