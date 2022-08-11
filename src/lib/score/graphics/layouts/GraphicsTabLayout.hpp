#pragma once

#include <score/graphics/GraphicsLayout.hpp>

namespace score
{

class SCORE_LIB_BASE_EXPORT GraphicsTabLayout
    : public QObject
    , public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsTabLayout();

  void addTab(QString tab);

  void layout() override;

private:
  QGraphicsItem* current{};
  std::vector<QGraphicsItem*> m_pages;
  std::vector<QString> m_tabs;
};

}
