#include "QGraphicsLineEdit.hpp"

#include <QTextDocument>

#include <private/qwidgettextcontrol_p.h>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsLineEdit)
namespace score
{

QGraphicsLineEdit::QGraphicsLineEdit(QGraphicsItem* parent)
    : QGraphicsTextItem{parent}
{
  setTextInteractionFlags(Qt::TextEditorInteraction);
  auto ctl = this->findChild<QWidgetTextControl*>();
  if(ctl)
  {
    ctl->setAcceptRichText(false);
  }

  QObject::connect(
      this->document(), &QTextDocument::contentsChanged, this,
      &QGraphicsLineEdit::checkSize);
}

void QGraphicsLineEdit::dropEvent(QGraphicsSceneDragDropEvent* drop)
{
  QGraphicsTextItem::dropEvent(drop);
  const auto& urlList = drop->mimeData()->urls();

  if(!urlList.isEmpty())
  {
    this->setPlainText(urlList[0].toLocalFile());
  }
}

void QGraphicsLineEdit::focusOutEvent(QFocusEvent* e)
{
  QGraphicsTextItem::focusOutEvent(e);
  editingFinished();
}

QVariant QGraphicsLineEdit::itemChange(GraphicsItemChange change, const QVariant& value)
{
  return QGraphicsTextItem::itemChange(change, value);
}

void QGraphicsLineEdit::checkSize()
{
  if(auto rect = boundingRect(); m_previousSize != rect)
  {
    m_previousSize = rect;
    sizeChanged(rect.size());
  }
}
}
