#ifndef PUFFIN_IMDB_NLOHMANN_HPP
#define PUFFIN_IMDB_NLOHMANN_HPP

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace puffin {
namespace imdb {

template<typename Key>
struct json_sort {
  Key key;

  bool operator()(const json& v1, const json&v2) {
    return (v1.contains(key) && v2.contains(key)) ? v1[key] < v2[key] : false;
  }
};

template<typename Key>
struct json_contains {
  Key key;
  std::string text = "";

  bool operator()(const json& v1) {
    return v1.contains(key) ? v1[key].template get<std::string>().find(text) != std::string::npos : false;
  }
};

template<typename Key>
struct json_exists {
    Key key;

    bool operator()(const json& j) {
        return j.contains(key);
    }
};

} // namespace imdb
} // namespace puffin

#endif // PUFFIN_IMDB_NLOHMANN_HPP
