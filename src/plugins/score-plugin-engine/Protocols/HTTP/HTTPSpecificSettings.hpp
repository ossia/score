#pragma once
#include <QMetaType>
#include <QString>

#include <wobjectdefs.h>

namespace Protocols
{
struct HTTPSpecificSettings
{
  QString text;
};
}
Q_DECLARE_METATYPE(Protocols::HTTPSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::HTTPSpecificSettings)
