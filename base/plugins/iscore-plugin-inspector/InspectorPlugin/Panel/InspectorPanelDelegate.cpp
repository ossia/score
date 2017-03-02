#include "InspectorPanelDelegate.hpp"
#include "Implementation/InspectorPanel.hpp"
#include "Implementation/SelectionStackWidget.hpp"

#include <Inspector/InspectorWidgetList.hpp>

#include <QVBoxLayout>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/widgets/MarginLess.hpp>

namespace InspectorPanel
{
PanelDelegate::PanelDelegate(const iscore::GUIApplicationContext& ctx)
    : iscore::PanelDelegate{ctx}, m_widget{new QWidget}
{
  new iscore::MarginLess<QVBoxLayout>{m_widget};
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
  delete m_stack;
  m_stack = nullptr;
  delete m_inspectorPanel;
  m_inspectorPanel = nullptr;
  if (newm)
  {
    auto lay = static_cast<iscore::MarginLess<QVBoxLayout>*>(m_widget->layout());

    auto& fact
        = newm->app.interfaces<Inspector::InspectorWidgetList>();
    SelectionStack& stack = newm->selectionStack;
    m_stack = new SelectionStackWidget{stack, m_widget};
    m_inspectorPanel = new InspectorPanelWidget{fact, stack, m_widget};


    m_inspectorPanel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    lay->addWidget(m_stack);
    lay->addWidget(m_inspectorPanel);

    setNewSelection(stack.currentSelection());
  }
}

void PanelDelegate::setNewSelection(const Selection& s)
{
  if (m_inspectorPanel)
    m_inspectorPanel->newItemsInspected(s);
}
}
