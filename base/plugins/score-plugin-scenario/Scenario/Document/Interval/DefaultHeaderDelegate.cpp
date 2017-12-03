// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DefaultHeaderDelegate.hpp"
#include <Process/Process.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Automation/AutomationModel.hpp>

#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <State/Unit.hpp>

namespace Scenario
{
DefaultHeaderDelegate::DefaultHeaderDelegate(Process::LayerPresenter& p): presenter{p}
{
  con(presenter.model(), &Process::ProcessModel::prettyNameChanged,
      this, &DefaultHeaderDelegate::updateName);
  con(presenter.model(), &Process::ProcessModel::durationChanged,
      this, &DefaultHeaderDelegate::updateName);
  // TODO connect zoom signal or onDisplaySizeChanged to updateName

  updateMin();
  m_mincache.setFont(ScenarioStyle::instance().Bold10Pt);
  m_mincache.setCacheEnabled(true);

  updateMax();
  m_maxcache.setFont(ScenarioStyle::instance().Bold10Pt);
  m_maxcache.setCacheEnabled(true);

  updateUnit();
  m_unitcache.setFont(ScenarioStyle::instance().Bold10Pt);
  m_unitcache.setCacheEnabled(true);

  updateName();
  m_textcache.setFont(ScenarioStyle::instance().Bold10Pt);
  m_textcache.setCacheEnabled(true);

  if ( const auto autom = dynamic_cast<const Automation::ProcessModel*>(&presenter.model()))
  {
    connect(autom, &Automation::ProcessModel::minChanged, this, &DefaultHeaderDelegate::updateMin);
    connect(autom, &Automation::ProcessModel::maxChanged, this, &DefaultHeaderDelegate::updateMax);
    connect(autom, &Automation::ProcessModel::unitChanged, this, &DefaultHeaderDelegate::updateUnit);
  }
}

void DefaultHeaderDelegate::updateName()
{
  QString name = presenter.model().prettyName();
  updateCache(m_textcache, name);

  // try to avoid to wide QTextLayout
  if ( size() > boundingRect().width() )
  {
    int first = name.indexOf("/");
    if (first > 0)
    {
      int second = name.indexOf("/", first+1);
      if (second < 0) second = first;

      QString head = name.left(second);

      int last = name.lastIndexOf("/");
      if (last == second) head = "";

      QString tail = name.right(name.size()-last);
      name = head + "..." + tail;
    }
    updateCache(m_textcache, name);
  }
}

void DefaultHeaderDelegate::updateMin(double f)
{
  if ( const auto autom = dynamic_cast<const Automation::ProcessModel*>(&presenter.model()))
  {
    float min = autom->min();
    updateCache(m_mincache, QString("min: %1").arg(QString::number(min)));
  }
}

void DefaultHeaderDelegate::updateMax(double f)
{
  if ( const auto autom = dynamic_cast<const Automation::ProcessModel*>(&presenter.model()))
  {
    float max = autom->max();
    updateCache(m_maxcache, QString("max: %1").arg(QString::number(max)));
  }
}

void DefaultHeaderDelegate::updateUnit()
{
  if ( const auto autom = dynamic_cast<const Automation::ProcessModel*>(&presenter.model()))
  {
    State::Unit unit = autom->unit();
    updateCache(m_unitcache, QString("unit: %1").arg(QString::fromStdString(ossia::get_pretty_unit_text(unit))));
  }
}

void DefaultHeaderDelegate::updateCache(QTextLayout& cache, const QString& txt)
{
  cache.setText(txt);
  cache.beginLayout();

  QTextLine line = cache.createLine();
  line.setPosition(QPointF{0., 0.});

  cache.endLayout();

  update();
}

void DefaultHeaderDelegate::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setPen(ScenarioStyle::instance().IntervalHeaderSeparator);
  m_textcache.draw(painter, QPointF{0., 0.});

  if (boundingRect().width() > size())
  {
    int offset = 2*m_gap + m_unitcache.boundingRect().width();
    m_unitcache.draw(painter, QPointF(qMax(boundingRect().width(),50.) - offset, 0.));
    offset += m_gap + m_maxcache.boundingRect().width();
    m_maxcache.draw(painter, QPointF(qMax(boundingRect().width(),50.) - offset, 0.));
    offset += m_gap + m_mincache.boundingRect().width();
    m_mincache.draw(painter, QPointF(qMax(boundingRect().width(),100.) - offset, 0.));
  }
}

int DefaultHeaderDelegate::size()
{
  return m_textcache.boundingRect().width()
      + qMax(m_mincache.boundingRect().width() , 50.)
      + qMax(m_maxcache.boundingRect().width() , 50.)
      + qMax(m_unitcache.boundingRect().width(), 50.)
      + 4 * m_gap;

}

}
