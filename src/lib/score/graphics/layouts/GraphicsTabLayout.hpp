#pragma once

#include <score/graphics/GraphicsLayout.hpp>

#include <functional>

namespace score
{
class QGraphicsEnum;

class SCORE_LIB_BASE_EXPORT GraphicsTabLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsTabLayout();

  void addTab(QString tab);
  int currentIndex() const noexcept { return m_currentIndex; }
  void setCurrentIndex(int index);
  void setTabBarVisible(bool visible) noexcept { m_showTabBar = visible; }
  std::function<void(int)> onCurrentIndexChanged;

  void layout() override;

private:
  int m_currentIndex{};
  bool m_showTabBar{true};
  QGraphicsEnum* m_tabBar{};
  std::vector<QGraphicsItem*> m_pages;
  std::vector<QString> m_tabs;
};

}
