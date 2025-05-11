#ifndef PUFFIN_IMDB_VIEW_HPP
#define PUFFIN_IMDB_VIEW_HPP

#include <vector>

namespace puffin {
namespace imdb {

template<typename Value, typename Request>
class view {


private:
  Request request_;

  std::vector<std::reference_wrapper<Value>> data_;
};

} // namespace imdb
} // namespace puffin

#endif // PUFFIN_IMDB_VIEW_HPP
