#pragma once
#include <QMetaType>
#include <QString>

#include <wobjectdefs.h>

namespace Protocols
{
struct OSCQuerySpecificSettings
{
  QString host;
};
}
Q_DECLARE_METATYPE(Protocols::OSCQuerySpecificSettings)
W_REGISTER_ARGTYPE(Protocols::OSCQuerySpecificSettings)
