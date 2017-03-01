#pragma once
#include <QQuickPaintedItem>
#include <iscore_lib_base_export.h>
class ISCORE_LIB_BASE_EXPORT GraphicsItem : public QQuickPaintedItem
{
  Q_OBJECT
public:
  GraphicsItem(QQuickItem* parent = nullptr):
    QQuickPaintedItem{parent}
  {
    setAcceptedMouseButtons(Qt::AllButtons);
  }

  virtual ~GraphicsItem();
  virtual int type() const { return 0; }
};
