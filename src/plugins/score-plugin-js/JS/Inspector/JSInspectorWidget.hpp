#pragma once
#include <JS/JSProcessModel.hpp>
#include <Process/Inspector/GenericProcessInspector.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <QString>

#include <verdigris>

class QWidget;
class QLabel;
class QTableWidget;
namespace JS
{
class InspectorWidget final
    : public Process::GenericInspectorWidget<JS::ProcessModel>
{
public:
  using GenericInspectorWidget::GenericInspectorWidget;
};

}
