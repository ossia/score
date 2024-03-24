#include "Library.hpp"

namespace Wokwi
{

const DeviceLibrary& DeviceLibrary::instance()
{
  static const DeviceLibrary devices;
  return devices;
}

DeviceLibrary::DeviceLibrary()
{
  initMCUs();
  initBoards();
  initDevices();
}

}
