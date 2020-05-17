#pragma once
#include <Process/Process.hpp>

#include <score/graphics/TextItem.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Bind.hpp>

#include <QGraphicsItem>
#include <QObject>
#include <QPainter>

#include <Effect/EffectLayer.hpp>

namespace Effect
{
struct ItemBase : public QObject, public QGraphicsItem
{
public:
  static const constexpr qreal TitleHeight = 15.;
  static const constexpr qreal FooterHeight = 12.;
  static const constexpr qreal Corner = 2.;
  static const constexpr qreal PortSpacing = 10.;
  static const constexpr qreal InletX0 = 2.;
  static const constexpr qreal InletY0 = 1.;
  static const constexpr qreal OutletX0 = 2.;
  static const constexpr qreal OutletY0 = -12.; // Add to height
  static const constexpr qreal TopButtonX0 = -12.;
  static const constexpr qreal TopButtonY0 = 2.;
  QSizeF size() const noexcept { return m_contentSize; }

protected:
  ItemBase(
      const Process::ProcessModel& process,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent)
      : QGraphicsItem{parent}
  {
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setFlag(ItemIsFocusable, true);
    setFlag(ItemClipsChildrenToShape, true);

    // Title
    m_ui = Process::makeExternalUIButton(process, ctx, this, this);

    auto& skin = score::Skin::instance();
    m_label = new score::SimpleTextItem{skin.Light.main, this};

    if (const auto& label = process.metadata().getLabel(); !label.isEmpty())
      m_label->setText(label);
    else
      m_label->setText(process.prettyShortName());

    con(process.metadata(), &score::ModelMetadata::LabelChanged, this, [&](const QString& label) {
      if (!label.isEmpty())
        m_label->setText(label);
      else
        m_label->setText(process.prettyShortName());
    });

    m_label->setFont(skin.Bold10Pt);
    m_label->setPos({12., 0});

    // Selection
    con(process.selection, &Selectable::changed, this, &ItemBase::setSelected);
  }

  void setSelected(bool s)
  {
    if (m_selected != s)
    {
      m_selected = s;
      if (s)
        setFocus();

      update();
    }
  }

  qreal width() const { return m_contentSize.width(); }
  qreal height() const { return TitleHeight + m_contentSize.height() + FooterHeight; }
  QRectF boundingRect() const final override
  {
    return {0., 0., m_contentSize.width(), TitleHeight + m_contentSize.height() + FooterHeight};
  }

  static void paintNode(QPainter* painter, bool selected, bool hovered, QRectF rect)
  {
    const auto& skin = score::Skin::instance();

    const auto& bset = skin.Emphasis5;
    const auto& fillbrush = skin.Emphasis5;
    const auto& brush = selected ? skin.Base2.darker : hovered ? bset.lighter : bset.main;
    const auto& pen = brush.pen2_solid_round_round;

    painter->setRenderHint(QPainter::Antialiasing, true);

    // Body
    painter->setPen(pen);
    painter->setBrush(fillbrush);

    painter->drawRoundedRect(rect, Corner, Corner);

    painter->setRenderHint(QPainter::Antialiasing, false);

    const auto h = rect.height();
    const auto w = rect.width();
    // Header
    painter->setBrush(brush.brush);
    painter->fillRect(QRectF{1., 0., w - 2., TitleHeight}, brush.brush);

    // Footer
    painter->fillRect(QRectF{1., h - FooterHeight, w - 2., FooterHeight}, brush.brush);
    // painter->setPen(Qt::green);
    // painter->setBrush(Qt::transparent);
    // painter->drawRect(rect);
  }

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
  {
    m_hover = true;
    update();
    event->accept();
  }

  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override { event->accept(); }

  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
  {
    m_hover = false;
    update();
    event->accept();
  }

  // Title
  QGraphicsItem* m_ui{};
  score::SimpleTextItem* m_label{};

  QSizeF m_contentSize{};
  bool m_hover{false};
  bool m_selected{false};
};
}
