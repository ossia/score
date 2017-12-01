// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DefaultHeaderDelegate.hpp"
#include <Process/Process.hpp>
#include <Process/Style/ScenarioStyle.hpp>

namespace Scenario
{
DefaultHeaderDelegate::DefaultHeaderDelegate(Process::LayerPresenter& p): presenter{p}
{
  con(presenter.model(), &Process::ProcessModel::prettyNameChanged,
      this, &DefaultHeaderDelegate::updateName);
  updateName();
  m_textcache.setFont(ScenarioStyle::instance().Bold10Pt);
  m_textcache.setCacheEnabled(true);
}

void DefaultHeaderDelegate::updateName()
{
  m_textcache.setText(presenter.model().prettyName());
  m_textcache.beginLayout();

  QTextLine line = m_textcache.createLine();
  line.setPosition(QPointF{0., 0.});

  m_textcache.endLayout();

  update();
}

void DefaultHeaderDelegate::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setPen(ScenarioStyle::instance().IntervalHeaderSeparator);
  m_textcache.draw(painter, QPointF{0., 0.});
}
}
