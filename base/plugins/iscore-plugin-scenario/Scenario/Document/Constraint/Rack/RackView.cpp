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
  //this->setFlag(QQuickPaintedItem::ItemHasContents, false);
  this->setZ(ZPos::Rack);
  this->setCursor(QCursor(Qt::ArrowCursor));
}

void RackView::paint(QPainter*)
{
}
}
