#include "ProtocolFactoryInterface.hpp"

namespace Device
{
ProtocolFactory::~ProtocolFactory() = default;

int ProtocolFactory::visualPriority() const
{
  return 0;
}
}
