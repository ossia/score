#include "player.hpp"
#include <ossia/network/generic/generic_device.hpp>

int main(int argc, char** argv)
{
    ossia::net::generic_device dev;
    dev.set_name("OSCdevice");

    auto address = ossia::net::create_node(dev, "/foo/bar").create_address(ossia::val_type::FLOAT);
    address->add_callback([] (const ossia::value& val) {
       std::cerr << val << std::endl;
    });

    //if(argc > 1)
    {
        iscore::Player p;
        p.load("/tmp/device.scorejson");
        p.registerDevice(dev);

        p.play();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        p.stop();
    }
    return 0;
}
