#pragma once
#include <score/graphics/TextItem.hpp>
#include <score/graphics/widgets/Constants.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>
namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsTextButton final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsTextButton)

  bool m_pressed{};

public:
  QGraphicsTextButton(QString text, QGraphicsItem* parent);

  void pressed() E_SIGNAL(SCORE_LIB_BASE_EXPORT, pressed);
  void dropped(QString filename) E_SIGNAL(SCORE_LIB_BASE_EXPORT, dropped, filename);
  void bang();

  const QString& text() const noexcept { return m_string; }
  void setText(const QString& s)
  {
    m_string = std::move(s);
    updateBounds();
  }
  QRectF boundingRect() const override;

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void updateBounds();
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      final override;
  QRectF m_rect;
  QString m_string;
};
}
