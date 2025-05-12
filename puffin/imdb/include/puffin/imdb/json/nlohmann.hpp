#ifndef PUFFIN_IMDB_NLOHMANN_HPP
#define PUFFIN_IMDB_NLOHMANN_HPP

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace puffin {
namespace imdb {

template<typename Key>
struct json_sort {
  Key key;

  bool operator()(const json& v1, const json& v2) {
    return (v1.contains(key) && v2.contains(key)) ? v1[key] < v2[key] : false;
  }
};

template<typename Key>
struct json_contains {
  Key key;
  std::string text = "";

  bool operator()(const json& v1) {
    if (!v1.contains(key))
      return false;

    return std::find_if(v1[key].begin(), v1[key].end(), [&](const json& j) {
      return j.get<std::string>().compare(text) == 0;
    }) != std::end(v1[key]);
  }
};

template<typename Key>
struct json_exists {
    Key key;

    bool operator()(const json& v) {
        return v.contains(key);
    }
};

template<typename Key>
struct json_search {
  Key key;
  std::string text = "";

  bool operator()(const json& v1) {
    return v1.contains(key) ? v1[key].template get<std::string>().find(text) != std::string::npos : false;
  }
};

} // namespace imdb
} // namespace puffin

#endif // PUFFIN_IMDB_NLOHMANN_HPP
