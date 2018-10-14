#pragma once
#include <QMetaType>
#include <QString>

#include <wobjectdefs.h>

namespace Protocols
{
struct WSSpecificSettings
{
  QString address;
  QString text;
};
}
Q_DECLARE_METATYPE(Protocols::WSSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::WSSpecificSettings)
