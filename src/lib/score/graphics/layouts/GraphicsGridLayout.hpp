#pragma once

#include <score/graphics/GraphicsLayout.hpp>

namespace score
{

class SCORE_LIB_BASE_EXPORT GraphicsGridLayout : public GraphicsLayout
{
  public:
    using GraphicsLayout::GraphicsLayout;
    ~GraphicsGridLayout();

    void setColumns(int columns);

    void layout() override;
  private:
    int m_columns{};
};

}
