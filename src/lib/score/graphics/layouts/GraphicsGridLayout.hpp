#pragma once

#include <score/graphics/GraphicsLayout.hpp>

namespace score
{

class SCORE_LIB_BASE_EXPORT GraphicsGridColumnsLayout : public GraphicsLayout
{
  public:
    using GraphicsLayout::GraphicsLayout;
    ~GraphicsGridColumnsLayout();

    void setColumns(int columns);

    void layout() override;
  private:
    int m_columns{5};
};

class SCORE_LIB_BASE_EXPORT GraphicsGridRowsLayout : public GraphicsLayout
{
  public:
    using GraphicsLayout::GraphicsLayout;
    ~GraphicsGridRowsLayout();

    void setRows(int rows);

    void layout() override;
  private:
    int m_rows{5};
};

class SCORE_LIB_BASE_EXPORT GraphicsDefaultLayout : public GraphicsLayout
{
  public:
    using GraphicsLayout::GraphicsLayout;
    ~GraphicsDefaultLayout();

    void layout() override;
};

}
