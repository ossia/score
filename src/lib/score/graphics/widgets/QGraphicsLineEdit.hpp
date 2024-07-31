#pragma once

#include <QGraphicsItem>
#include <QGraphicsSceneEvent>
#include <QRectF>
#include <QSizeF>
#include <QTextDocument>

#include <private/qwidgettextcontrol_p.h>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{

struct SCORE_LIB_BASE_EXPORT QGraphicsLineEdit : public QGraphicsTextItem
{
  W_OBJECT(QGraphicsLineEdit)
public:
  explicit QGraphicsLineEdit(QGraphicsItem* parent);

  void dropEvent(QGraphicsSceneDragDropEvent* drop) override;
  void focusOutEvent(QFocusEvent* e) override;
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

  void checkSize();

  void sizeChanged(QSizeF sz) E_SIGNAL(SCORE_LIB_BASE_EXPORT, sizeChanged, sz)
  void editingFinished() E_SIGNAL(SCORE_LIB_BASE_EXPORT, editingFinished)

private:
  QRectF m_previousSize;
};

}
