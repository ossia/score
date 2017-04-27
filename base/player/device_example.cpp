#include "player.hpp"
#include <ossia/network/generic/generic_device.hpp>

int main()
{
  // Create a device
  ossia::net::generic_device dev;

  // The name has to match one of the devices in the loaded file
  dev.set_name("OSCdevice");

  // Add a custom callback on the device
  auto address = ossia::net::create_node(dev, "/foo/bar").create_address(ossia::val_type::FLOAT);
  address->add_callback([] (const ossia::value& val) {
    std::cerr << val << std::endl;
  });

  // Create a player and load a file
  iscore::Player p;
  p.load("/tmp/device.scorejson");

  // The device will replace the current implementation that was loaded
  p.registerDevice(dev);

  // Execution occurs in a separate thread
  p.play();
  std::this_thread::sleep_for(std::chrono::seconds(5));
  p.stop();

  return 0;
}
