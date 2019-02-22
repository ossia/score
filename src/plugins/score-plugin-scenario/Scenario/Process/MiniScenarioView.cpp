// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MiniScenarioView.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <QGraphicsScene>
#include <QPainter>
namespace Scenario
{

MiniScenarioView::MiniScenarioView(const ProcessModel& sc, QGraphicsItem* p)
    : MiniLayer{p}, m_scenario{sc}
{
  m_scenario.intervals.added.connect<&MiniScenarioView::on_elementChanged>(
      this);
  m_scenario.intervals.removed.connect<&MiniScenarioView::on_elementChanged>(
      this);

  connect(&m_scenario, &Scenario::ProcessModel::intervalMoved, this, [=] {
    update();
  });
}

void MiniScenarioView::paint_impl(QPainter* p) const
{
  auto& skin = Process::Style::instance();
  const auto h = height() - 8;

  auto& pen = skin.MiniScenarioPen;
  for (const Scenario::IntervalModel& c : m_scenario.intervals)
  {
    auto col = c.metadata().getColor().getBrush().color();
    col.setAlphaF(1.0);
    pen.setColor(col);
    p->setPen(pen);
    auto def = c.duration.defaultDuration().toPixels(zoom());
    auto st = c.date().toPixels(zoom());
    auto y = c.heightPercentage();
    p->drawLine(QPointF{st, 4. + y * h}, QPointF{st + def, 4. + y * h});
  }
}
}
