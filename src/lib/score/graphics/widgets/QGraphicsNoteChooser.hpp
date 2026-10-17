#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QFont>
#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <functional>
#include <optional>
#include <verdigris>

namespace score
{

class SCORE_LIB_BASE_EXPORT QGraphicsNoteChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsNoteChooser)
  SCORE_GRAPHICS_ITEM_TYPE(120)

  static constexpr int m_min = -2;
  static constexpr int m_max = 127;
  double m_width = 30;
  double m_height = 28;
  std::function<QString(int)> m_format;
  std::optional<QFont> m_font;
  double m_curValue{};
  int m_value{};
  bool m_grab{};

public:
  explicit QGraphicsNoteChooser(QGraphicsItem* parent);

  void setValue(int v);
  int value() const;

  void setSize(QSizeF sz);
  //! Text shown for a value, on one line; unset shows the note name above
  //! the number.
  void setFormatter(std::function<QString(int)> f);
  //! Unset uses the skin's small mono font.
  void setFont(const QFont& f);

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  bool sceneEvent(QEvent* event) override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
  QRectF boundingRect() const override;
};

}
