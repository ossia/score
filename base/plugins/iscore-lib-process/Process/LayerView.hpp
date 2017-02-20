#pragma once

#include <QQuickPaintedItem>
#include <QRect>
#include <QtGlobal>
#include <iscore_lib_process_export.h>

class QPainter;

class QWidget;

namespace Process
{
class ISCORE_LIB_PROCESS_EXPORT LayerView : public QQuickPaintedItem
{
  Q_OBJECT
public:
  LayerView(QQuickPaintedItem* parent);

  virtual ~LayerView();

  QRectF boundingRect() const final override;
  void paint(
      QPainter* painter) final override;

  void setHeight(qreal height);
  qreal height() const;

  void setWidth(qreal width);
  qreal width() const;

signals:
  void heightChanged();
  void widthChanged();

protected:
  virtual void paint_impl(QPainter*) const = 0;

private:
  qreal m_height{};
  qreal m_width{};
};
}
