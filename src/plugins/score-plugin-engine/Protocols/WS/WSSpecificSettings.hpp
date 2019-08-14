#pragma once
#include <QString>

#include <verdigris>

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
