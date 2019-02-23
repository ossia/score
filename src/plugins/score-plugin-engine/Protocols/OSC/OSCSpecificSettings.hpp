#pragma once
#include <ossia/detail/optional.hpp>

#include <QMetaType>
#include <QString>

#include <wobjectdefs.h>

namespace Protocols
{
struct OSCSpecificSettings
{
  int inputPort{};
  int outputPort{};
  QString host;
  ossia::optional<int> rate{};
};
}
Q_DECLARE_METATYPE(Protocols::OSCSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::OSCSpecificSettings)
