#pragma once
#include <QString>

#include <verdigris>

namespace Protocols
{
struct HTTPSpecificSettings
{
  QString text;
};
}
Q_DECLARE_METATYPE(Protocols::HTTPSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::HTTPSpecificSettings)
