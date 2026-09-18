#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QGraphicsItem>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <score_lib_base_export.h>

#include <array>
#include <verdigris>

class QGraphicsProxyWidget;

namespace score
{
class Skin;
class SCORE_LIB_BASE_EXPORT QGraphicsCombo final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsCombo)
  SCORE_GRAPHICS_ITEM_TYPE(30)
  friend struct DefaultComboImpl;
  QRectF m_rect{defaultSliderSize};

public:
  QStringList array;

private:
  int m_value{};
  bool m_grab{};
  bool m_editable{};

  //! Set once the pointer travels far enough to count as a drag, so that a
  //! plain click can be told apart from scrubbing and open the drop-down.
  bool m_dragged{};

  //! +1 or -1 from a press on one half of the stepper until the release, 0
  //! otherwise. Such a press must neither scrub nor open the drop-down.
  int m_pressedStep{};

  //! Whether the pointer is still on the half it pressed; leaving cancels.
  bool m_stepArmed{};

  //! The drop-down currently in the scene, if any. Both mouse buttons can open
  //! one and it is built from the event loop, so without this a second click
  //! before the first editor appears would leave two of them stacked up.
  QPointer<QGraphicsProxyWidget> m_editor;

public:
  template <std::size_t N>
  QGraphicsCombo(const std::array<const char*, N>& arr, QGraphicsItem* parent)
      : QGraphicsCombo{parent}
  {
    array.reserve(N);
    for(auto str : arr)
      array.push_back(str);

    init();
  }

  QGraphicsCombo(QStringList arr, QGraphicsItem* parent)
      : QGraphicsCombo{parent}
  {
    array = std::move(arr);
    init();
  }

  explicit QGraphicsCombo(QGraphicsItem* parent);

  //! Width of the +/- stepper strip kept free at the right of the box.
  static const constexpr double stepperWidth = 12.;

  void init();
  void setRect(const QRectF& r);
  void setValue(int v);
  int value() const;

  //! The strip holding the two stepper buttons, in item coordinates.
  QRectF stepperRect() const noexcept;
  //! Whether there is more than one entry and room to draw the strip.
  bool stepperVisible() const noexcept;

  //! Move the selection by n entries, wrapping, and report it as an edit.
  void step(int n);

  //! Whether a value that is not in the list may be entered.
  void setEditable(bool b);
  bool editable() const noexcept { return m_editable; }

  //! Show the drop-down at that position in scene coordinates.
  void openEditor(QPointF scenePos);

  bool moving = false;

  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

  //! Text matching none of the entries; only sent when editable().
  void valueEdited(const QString& text) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueEdited, text)

  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  bool sceneEvent(QEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

private:
  void paintStepper(QPainter& painter, const score::Skin& skin);
};
}
