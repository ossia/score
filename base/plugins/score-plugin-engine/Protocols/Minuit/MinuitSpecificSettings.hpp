#pragma once
#include <QMetaType>
#include <QString>

#include <wobjectdefs.h>


namespace Protocols
{
struct MinuitSpecificSettings
{
  int inputPort{};
  int outputPort{};
  QString host;
  QString localName;
};
}
Q_DECLARE_METATYPE(Protocols::MinuitSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MinuitSpecificSettings)
