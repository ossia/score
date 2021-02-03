// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InspectorPanelDelegate.hpp"

#include "Implementation/InspectorPanel.hpp"

#include <Inspector/InspectorWidgetList.hpp>

#include <score/selection/SelectionStack.hpp>
#include <score/widgets/ClearLayout.hpp>
#include <score/widgets/MarginLess.hpp>

#include <QVBoxLayout>

namespace InspectorPanel
{

template <int w, int h>
class SizedWidget : public QWidget
{
  QSize sizeHint() const final override { return QSize{w, h}; }
};

PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}, m_widget{new SizedWidget<200, 600>}
{
  new QVBoxLayout{m_widget};
  m_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  m_widget->setMinimumHeight(400);
  m_widget->setMinimumWidth(200);

  m_widget->setStatusTip(
      QObject::tr("The inspector show information on the currently selected items."));
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      true,
      false,
      Qt::RightDockWidgetArea,
      10,
      QObject::tr("Inspector"),
      "inspector",
      QObject::tr("Ctrl+Shift+I")};

  return status;
}

void PanelDelegate::on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm)
{
  using namespace score;
  delete m_inspectorPanel;
  m_inspectorPanel = nullptr;

  auto old_lay = static_cast<QVBoxLayout*>(m_widget->layout());
  auto lay = new QVBoxLayout;
  QWidget{}.setLayout(old_lay);
  m_widget->setLayout(lay);

  if (newm)
  {
    SelectionStack& stack = newm->selectionStack;

    auto& fact = newm->app.interfaces<Inspector::InspectorWidgetList>();
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
