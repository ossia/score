// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InspectorPanelDelegate.hpp"
#include "Implementation/InspectorPanel.hpp"

#include <Inspector/InspectorWidgetList.hpp>

#include <QVBoxLayout>
#include <score/selection/SelectionStack.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/ClearLayout.hpp>

namespace InspectorPanel
{
PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}, m_widget{new QWidget}
{
  new score::MarginLess<QVBoxLayout>{m_widget};
  m_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  m_widget->setMinimumHeight(400);
  m_widget->setMinimumWidth(250);
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{true, Qt::RightDockWidgetArea, 10,
                                          QObject::tr("Inspector"),
                                          QObject::tr("Ctrl+Shift+I")};

  return status;
}

void PanelDelegate::on_modelChanged(
    score::MaybeDocument oldm, score::MaybeDocument newm)
{
  using namespace score;
  delete m_inspectorPanel;
  m_inspectorPanel = nullptr;

  auto lay = static_cast<score::MarginLess<QVBoxLayout>*>(m_widget->layout());
  score::clearLayout(lay);
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
