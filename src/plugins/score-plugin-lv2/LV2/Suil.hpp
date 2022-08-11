#pragma once

#include <ossia/detail/dylib_loader.hpp>

#include <cassert>

#include <suil-0/suil/suil.h>

class libsuil
{
public:
  decltype(&::suil_init) init{};
  decltype(&::suil_host_new) host_new{};
  decltype(&::suil_host_set_touch_func) host_set_touch_func{};
  decltype(&::suil_host_free) host_free{};
  decltype(&::suil_ui_supported) ui_supported{};
  decltype(&::suil_instance_new) instance_new{};
  decltype(&::suil_instance_free) instance_free{};
  decltype(&::suil_instance_get_handle) instance_get_handle{};
  decltype(&::suil_instance_get_widget) instance_get_widget{};
  decltype(&::suil_instance_port_event) instance_port_event{};
  decltype(&::suil_instance_extension_data) instance_extension_data{};

  static const libsuil& instance()
  {
    static const libsuil self;
    return self;
  }

private:
  ossia::dylib_loader library;

  libsuil()
      : library("libsuil-0.so.0")
  {
    init = library.symbol<decltype(&::suil_init)>("suil_init");
    host_new = library.symbol<decltype(&::suil_host_new)>("suil_host_new");
    host_set_touch_func = library.symbol<decltype(&::suil_host_set_touch_func)>(
        "suil_host_set_touch_func");
    host_free = library.symbol<decltype(&::suil_host_free)>("suil_host_free");
    ui_supported = library.symbol<decltype(&::suil_ui_supported)>("suil_ui_supported");
    instance_new = library.symbol<decltype(&::suil_instance_new)>("suil_instance_new");
    instance_free
        = library.symbol<decltype(&::suil_instance_free)>("suil_instance_free");
    instance_get_handle = library.symbol<decltype(&::suil_instance_get_handle)>(
        "suil_instance_get_handle");
    instance_get_widget = library.symbol<decltype(&::suil_instance_get_widget)>(
        "suil_instance_get_widget");
    instance_port_event = library.symbol<decltype(&::suil_instance_port_event)>(
        "suil_instance_port_event");
    instance_extension_data = library.symbol<decltype(&::suil_instance_extension_data)>(
        "suil_instance_extension_data");

    assert(init);
    assert(host_new);
    assert(host_set_touch_func);
    assert(host_free);
    assert(ui_supported);
    assert(instance_new);
    assert(instance_free);
    assert(instance_get_handle);
    assert(instance_get_widget);
    assert(instance_port_event);
    assert(instance_extension_data);
  }
};
