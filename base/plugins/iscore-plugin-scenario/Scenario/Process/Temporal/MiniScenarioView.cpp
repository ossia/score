// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MiniScenarioView.hpp"
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <QPainter>
#include <QGraphicsScene>
namespace Scenario
{

MiniScenarioView::MiniScenarioView(const ProcessModel& sc, QGraphicsItem* p)
  : MiniLayer{p}
  , m_scenario{sc}
{
  m_scenario.constraints.added.connect<MiniScenarioView, &MiniScenarioView::on_elementChanged>(this);
  m_scenario.constraints.removed.connect<MiniScenarioView, &MiniScenarioView::on_elementChanged>(this);

  connect(&m_scenario, &Scenario::ProcessModel::constraintMoved,
          this, [=] {
    update();
  });
}

void MiniScenarioView::paint_impl(QPainter* p) const
{
  auto& skin = ScenarioStyle::instance();
  const auto h = height() - 8;

  auto& pen = skin.MiniScenarioPen;
  for(const Scenario::ConstraintModel& c : m_scenario.constraints)
  {
    auto col = c.metadata().getColor().getColor().color();
    col.setAlphaF(1.0);
    pen.setColor(col);
    p->setPen(pen);
    auto def = c.duration.defaultDuration().toPixels(zoom());
    auto st = c.startDate().toPixels(zoom());
    auto y = c.heightPercentage();
    p->drawLine(QPointF{st, 4. + y * h},
                QPointF{st + def, 4. + y * h});
  }

}

}
