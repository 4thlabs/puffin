puffin_declare_samples(NAME path_literal_sample SOURCES path_literal_sample.cpp DEPS puffin::common)
puffin_declare_samples(NAME event_bus_sample SOURCES event_bus_sample.cpp DEPS puffin::events)

if(TARGET webkit_asio)
  puffin_declare_samples(NAME webkit_server_sample SOURCES webkit_server_sample.cpp DEPS puffin::webkit_asio)
endif()

if(TARGET webkit_asio AND TARGET rest_json)
  puffin_declare_samples(NAME rest_sample SOURCES rest_sample.cpp DEPS puffin::webkit_asio puffin::rest_json nlohmann_json::nlohmann_json)
endif()
