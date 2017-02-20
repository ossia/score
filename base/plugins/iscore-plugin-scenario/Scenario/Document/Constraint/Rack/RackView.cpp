#include <QtGlobal>
#include <QCursor>
#include "RackView.hpp"
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
class QPainter;

class QWidget;

namespace Scenario
{
RackView::RackView(QQuickPaintedItem* parent) : QQuickPaintedItem{parent}
{
  this->setFlag(QQuickPaintedItem::ItemHasContents, false);
  this->setZ(ZPos::Rack);
  this->setCursor(QCursor(Qt::ArrowCursor));
}

QRectF RackView::boundingRect() const
{
  return {0, 0, m_width, m_height};
}

void RackView::paint(QPainter*)
{
}
}
