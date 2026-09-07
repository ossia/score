#include "GraphicsTabLayout.hpp"

#include <score/graphics/widgets/QGraphicsEnum.hpp>
#include <score/tools/Debug.hpp>
namespace score
{

GraphicsTabLayout::~GraphicsTabLayout() { }

void GraphicsTabLayout::addTab(QString tab)
{
  m_tabs.push_back(tab);
}

void GraphicsTabLayout::setCurrentIndex(int index)
{
  if(index < 0 || index >= std::ssize(m_tabs))
    return;
  if(m_currentIndex < std::ssize(m_pages))
    m_pages[m_currentIndex]->setVisible(false);
  m_currentIndex = index;
  if(index < std::ssize(m_pages))
    m_pages[index]->setVisible(true);
  if(m_tabBar)
    m_tabBar->setValue(index);
}

void GraphicsTabLayout::layout()
{
  auto items = this->childItems();
  if(m_tabBar)
    items.removeOne(m_tabBar);
  updateChildrenRects(items);
  m_pages.assign(items.begin(), items.end());

  SCORE_ASSERT(items.size() == std::ssize(m_tabs));
  const int N = items.size();

  if(m_showTabBar && !m_tabBar)
  {
    m_tabBar = new score::QGraphicsEnum{m_tabs, this};
    connect(m_tabBar, &score::QGraphicsEnum::currentIndexChanged, this, [this](int idx) {
      setCurrentIndex(idx);
      if(onCurrentIndexChanged)
        onCurrentIndexChanged(idx);
    });
  }
  double y = m_padding;
  if(m_tabBar)
  {
    m_tabBar->setVisible(m_showTabBar);
    if(m_showTabBar)
    {
      m_tabBar->setPos(m_padding, m_padding);
      m_tabBar->columns = m_tabs.size();
      m_tabBar->updateRect();
      y += m_tabBar->boundingRect().height();
    }
  }
  // Layout the pages
  for(int i = 0; i < N; i++)
  {
    auto page = items[i];
    page->setPos(m_padding, y);
    page->setVisible(false);
  }

  setCurrentIndex(m_currentIndex);
}

}
