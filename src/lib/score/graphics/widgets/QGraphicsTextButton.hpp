#pragma once
#include <score/graphics/widgets/Constants.hpp>
#include <score/graphics/TextItem.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>
namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsTextButton final
    : public score::SimpleTextItem
{
  W_OBJECT(QGraphicsTextButton)

  bool m_pressed{};
public:
  QGraphicsTextButton(QGraphicsItem* parent,QString text="");

  void pressed(bool b) E_SIGNAL(SCORE_LIB_BASE_EXPORT, pressed,b);
  void bang();

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) final override;
  };
}


