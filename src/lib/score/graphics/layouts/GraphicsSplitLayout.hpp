#pragma once

#include <score/graphics/GraphicsLayout.hpp>

namespace score
{


class SCORE_LIB_BASE_EXPORT GraphicsSplitLayout : public GraphicsLayout
{
  public:
    using GraphicsLayout::GraphicsLayout;
    ~GraphicsSplitLayout();

    void layout() override;
    void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;
  private:
    std::vector<QGraphicsItem*> m_splits;
};

}
