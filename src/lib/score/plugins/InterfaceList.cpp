// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InterfaceList.hpp"

#include <QDebug>

#include <typeinfo>
namespace score
{
InterfaceListBase::~InterfaceListBase() = default;

void debug_types(const InterfaceBase* orig, const InterfaceBase* repl) noexcept
{
  qDebug() << "Warning: replacing" << typeid(*orig).name() << "with" << typeid(*repl).name();
}

}
