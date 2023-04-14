#pragma once
#include <score/graphics/widgets/Constants.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsPixmapMultichoice final
    : public QObject
    , public QGraphicsPixmapItem
{
  W_OBJECT(QGraphicsPixmapMultichoice)
  Q_INTERFACES(QGraphicsItem)

  std::vector<QPixmap> m_states;
  int m_current{};

public:
  explicit QGraphicsPixmapMultichoice(QGraphicsItem* parent);

  void toggle();
  void setState(int current);

  void setPixmaps(std::vector<QPixmap> states);

public:
  void toggled(int state) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, state)

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};
}
