//  ____         __  __ _       
// |  _ \ _   _ / _|/ _(_)_ __  
// | |_) | | | | |_| |_| | '_  | 
// |  __/| |_| |  _|  _| | | | |
// |_|    \__,_|_| |_| |_|_| |_|          
//
// BSD 3-Clause License

// Copyright (c) 2019, Thomas Gourgues (thomas.gourgues@gmail.com)
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

#ifndef PFN_EVENTBUS_HPP
#define PFN_EVENTBUS_HPP

#include <iostream>
#include <puffin/common/integer_sequence.hpp>
#include <puffin/common/function_traits.hpp>

#include "event.hpp"
#include "handler.hpp"

#include <map>
#include <vector>
#include <algorithm>
#include <type_traits>

struct void_t {};

namespace pfn {
namespace events {

template<typename T, typename = typename T::type>
class basic_event_bus {};

///
/// \brief A small synchronous event bus
///
template<typename T, typename... Events>
class basic_event_bus<T, event_pack<Events...>> { 
public:
  using event_types = std::tuple<Events...>;

  template<typename Event>
  using handler_type = handler<Event>;

  template<typename Event>
  using handler_list = std::vector<handler_type<Event>>;

  template<typename Event>
  using handler_tuple = std::tuple<Event, handler_list<Event>>;

  template<typename F>
  using argument_type = typename function_traits<F>::template arg_type<0>;

  template<typename F>
  using raw_argument_type = typename std::remove_cv
  <
    typename std::remove_reference<argument_type<F>>::type
  >::type;
  
public:
  basic_event_bus() {}

  ~basic_event_bus() { clear_all(); }

  ///
  /// \brief Adds a handler for e specific Event type
  ///
  template<typename Event>
  void add_handler(handler_type<Event> handler)
  {
    //handler_type<Event> h((size_t)this, fn);
    get_handler_list<Event>().push_back(handler);
  }

  ///
  /// \brief Adds a handler for a specific Event type
  ///
  template<typename Event>
  void add_handler(const typename handler_type<Event>::fn_type& fn)
  {
    handler_type<Event> h((size_t)this, fn);
    get_handler_list<Event>().push_back(h);
  }

  ///
  /// \brief Adds a class handler for a specific event
  ///
  template<typename Event, typename Class>
  void_t add_handler(Class* owner)
  {
    handler_type<Event> h((size_t)owner, std::bind(static_cast<void(Class::*)(const Event&)>(&Class::handle), owner, std::placeholders::_1));
    get_handler_list<Event>().push_back(h);
    return void_t();
  }

  ///
  /// \brief Removed an event handler
  ///
  template<typename Event, typename Class>
  void_t remove_handler(Class* owner)
  {
    auto& handlers = get_handler_list<Event>();
    handlers.erase(
      std::remove_if(handlers.begin(), handlers.end(), [owner](const handler_type<Event>& h) { return h == (size_t)owner; }),
      handlers.end()
    );

    return void_t();
  }

  ///
  /// \brief Sends an event
  ///
  template<typename Event>
  void send(const Event& e)
  {
    for(auto& f : get_handler_list<Event>()) {
      f(e);
    }
  }

  ///
  /// \brief Clears handlers for an Event type
  ///
  template<typename Event>
  void clear() 
  {
    clear_handlers<Event>();
  }

  ///
  /// \brief Clears all event handlers
  ///
  void clear_all()
  {
     auto l = {clear_handlers<Events>()...};
  }

protected:
  template<typename Event>
  handler_list<Event>& get_handler_list()
  {
    return std::get<1>(std::get<handler_tuple<Event>>(handlers_));
  }

  template<typename Event>
  void_t clear_handlers()
  {
    get_handler_list<Event>().clear();
    return void_t();
  }

  template<typename... F>
  void apply(F... i) {}

private:
  std::tuple<handler_tuple<Events>...> handlers_;
};

template<typename Events>
using event_bus = basic_event_bus<Events>;

template<typename... Events, typename BEvents, typename Class>
void register_events(event_bus<BEvents>& b, Class* owner)
{
  auto l = {void_t(), b.template add_handler<Events>(owner)...};
}

template<typename... Events, typename BEvents, typename Class>
void unregister_events(event_bus<BEvents>& b, Class* owner)
{
  auto l = {void_t(), b.template remove_handler<Events>(owner)...};
}

}
}

#endif // PFN_EVENTBUS_HPP
