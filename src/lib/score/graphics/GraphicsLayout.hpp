#pragma once
#include <score/graphics/RectItem.hpp>

namespace score
{
struct BrushSet;
static constexpr const qreal default_margin = 5.;
static constexpr const qreal default_padding = 5.;
class SCORE_LIB_BASE_EXPORT GraphicsLayout : public score::BackgroundItem
{
  public:
    explicit GraphicsLayout(QGraphicsItem* parent);
    ~GraphicsLayout();

    virtual void layout();
    virtual void centerContent();

    void setBrush(score::BrushSet& b);
    void setMargin(qreal m);
    void setPadding(qreal p);

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  protected:
    score::BrushSet* m_bg{};
    qreal m_margin{default_margin};
    qreal m_padding{default_padding};
};

}
