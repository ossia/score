// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "player.hpp"

#include <ossia/network/generic/generic_device.hpp>

#include <QtEnvironmentVariables>

#include <iostream>
int main()
{
  qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
  qunsetenv("QT_LOGGING_RULES");
  // Create a device
  ossia::net::generic_device dev;

  // The name has to match one of the devices in the loaded file
  dev.set_name("OSCdevice");

  // Add a custom callback on the device
  auto address = ossia::net::create_node(dev, "/foo/bar")
                     .create_parameter(ossia::val_type::FLOAT);
  address->add_callback([](const ossia::value& val) {
    std::cerr << ossia::value_to_pretty_string(val) << std::endl;
  });

  // Create a player instance
  std::atomic_bool ready{};
  score::Player p{[&] { ready = true; }};

  while(!ready)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // The device will replace the implementation that will be loaded with the
  // same name.
  p.registerDevice(dev);

  // Load a file
  p.load("/home/jcelerier/test-simple-audio.score");

  // Execution occurs in a separate thread
  p.play();
  std::this_thread::sleep_for(std::chrono::seconds(5));
  p.stop();

  return 0;
}
