// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressBarItem.hpp"

#include "ClickableLabelItem.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/graphics/YPos.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/path/ObjectIdentifier.hpp>

#include <QString>

#include <wobjectimpl.h>

#include <cstddef>
#include <vector>
W_OBJECT_IMPL(Scenario::AddressBarItem)

namespace Scenario
{
AddressBarItem::AddressBarItem(const score::DocumentContext& ctx, QGraphicsItem* parent)
    : QGraphicsItem{parent}, m_ctx{ctx}
{
  this->setFlag(QGraphicsItem::ItemHasNoContents, true);
}

void AddressBarItem::setTargetObject(ObjectPath&& path)
{
  qDeleteAll(m_items);
  m_items.clear();

  m_currentPath = std::move(path);

  double currentWidth = 0.;

  int i = -1;
  for (auto& identifier : m_currentPath)
  {
    i++;
    if (!identifier.objectName().contains("IntervalModel")
        && !identifier.objectName().contains("ConstraintModel"))
      continue;

    auto thisPath = m_currentPath;
    auto& pathVec = thisPath.vec();
    pathVec.resize(i + 1);
    IntervalModel& thisObj = thisPath.find<IntervalModel>(m_ctx);

    QString txt = thisObj.metadata().getName();

    auto lab = new ClickableLabelItem{
        thisObj.metadata(), [&](ClickableLabelItem*) { intervalSelected(thisObj); }, txt, this};

    lab->setIndex(i);
    connect(lab, &ClickableLabelItem::textChanged, this, &AddressBarItem::redraw);

    m_items.append(lab);
    lab->setPos(currentWidth, -4.);
    currentWidth += 4. + lab->boundingRect().width();

    auto sep = new SeparatorItem{this};
    sep->setPos(currentWidth, -1.);
    currentWidth += 4. + sep->boundingRect().width();
    m_items.append(sep);
  }

  prepareGeometryChange();
  m_width = currentWidth;
}

double AddressBarItem::width() const
{
  return m_width;
}

QRectF AddressBarItem::boundingRect() const
{
  return {};
}

void AddressBarItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
}

void AddressBarItem::redraw()
{
  double currentWidth = 0.;
  for (auto obj : m_items)
  {
    obj->setPos(currentWidth, 0.);
    currentWidth += 10. + obj->boundingRect().width();
  }

  needRedraw();
}
}
