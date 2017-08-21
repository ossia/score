// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InspectorPanelDelegate.hpp"
#include "Implementation/InspectorPanel.hpp"
#include "Implementation/SelectionStackWidget.hpp"

#include <Inspector/InspectorWidgetList.hpp>

#include <QVBoxLayout>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/ClearLayout.hpp>

namespace InspectorPanel
{
PanelDelegate::PanelDelegate(const iscore::GUIApplicationContext& ctx)
    : iscore::PanelDelegate{ctx}, m_widget{new QWidget}
{
  new iscore::MarginLess<QVBoxLayout>{m_widget};
  m_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  m_widget->setMinimumHeight(400);
  m_widget->setMinimumWidth(250);
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const iscore::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const iscore::PanelStatus status{true, Qt::RightDockWidgetArea, 10,
                                          QObject::tr("Inspector"),
                                          QObject::tr("Ctrl+Shift+I")};

  return status;
}

void PanelDelegate::on_modelChanged(
    iscore::MaybeDocument oldm, iscore::MaybeDocument newm)
{
  using namespace iscore;
  delete m_inspectorPanel;
  m_inspectorPanel = nullptr;

  auto lay = static_cast<iscore::MarginLess<QVBoxLayout>*>(m_widget->layout());
  iscore::clearLayout(lay);
  if (newm)
  {
    SelectionStack& stack = newm->selectionStack;

    auto& fact
        = newm->app.interfaces<Inspector::InspectorWidgetList>();
    m_inspectorPanel = new InspectorPanelWidget{fact, stack, lay, m_widget, m_widget};
    m_widget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
  }
}

void PanelDelegate::setNewSelection(const Selection& s)
{
  if (m_inspectorPanel)
    m_inspectorPanel->newItemsInspected(s);
}
}
