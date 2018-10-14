#pragma once
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
};
}
Q_DECLARE_METATYPE(Protocols::OSCSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::OSCSpecificSettings)
