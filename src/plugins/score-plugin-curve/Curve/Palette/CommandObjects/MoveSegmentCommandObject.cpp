// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MoveSegmentCommandObject.hpp"

namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{
MoveSegmentCommandObject::MoveSegmentCommandObject(const score::CommandStackFacade& stack)
//:
// m_dispatcher{stack}
{
}

void MoveSegmentCommandObject::press() { }

void MoveSegmentCommandObject::move() { }

void MoveSegmentCommandObject::release() { }

void MoveSegmentCommandObject::cancel() { }
}
