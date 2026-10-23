#include "QGraphicsLineEdit.hpp"

#include <score/model/Skin.hpp>

#include <QPainter>

#include <QTextDocument>

#include <private/qwidgettextcontrol_p.h>

#include <wobjectimpl.h>

#if defined(__EMSCRIPTEN__)
#include <score/application/GUIApplicationContext.hpp>
#include <score/graphics/GraphicsItem.hpp>
#endif

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

void QGraphicsLineEdit::focusInEvent(QFocusEvent* e)
{
  QGraphicsTextItem::focusInEvent(e);
#if defined(__EMSCRIPTEN__)
  score::reclaimMainWindowFocus();
  score::retargetInputMethod(getView(*this));
#endif
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

//! The field is drawn under the text, so that an empty one can still be seen
//! and clicked.
void QGraphicsLineEdit::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(skin.NoPen);
  painter->setBrush(skin.Emphasis2.main.brush);
  painter->drawRoundedRect(boundingRect().adjusted(0, 1, 0, -1), 1, 1);
  painter->setRenderHint(QPainter::Antialiasing, false);
  QGraphicsTextItem::paint(painter, option, widget);
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
