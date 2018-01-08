// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProtocolFactoryInterface.hpp"

namespace Device
{
ProtocolFactory::~ProtocolFactory() = default;

int ProtocolFactory::visualPriority() const
{
  return 0;
}

}
