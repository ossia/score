// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InspectorWidgetFactoryInterface.hpp"

namespace Inspector
{
InspectorWidgetFactory::~InspectorWidgetFactory() = default;

bool InspectorWidgetFactory::update(
    QWidget*, const QList<const IdentifiedObjectAbstract*>& obj) const
{
  return false;
}
}
