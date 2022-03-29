#include "GraphicsTabLayout.hpp"
#include <score/graphics/widgets/QGraphicsEnum.hpp>
#include <score/tools/Debug.hpp>
namespace score
{

GraphicsTabLayout::~GraphicsTabLayout()
{

}

void GraphicsTabLayout::addTab(QString tab)
{
  m_tabs.push_back(tab);
}

void GraphicsTabLayout::layout()
{
  const auto& items = this->childItems();
  m_pages.assign(items.begin(), items.end());

  SCORE_ASSERT(items.size() == m_tabs.size());
  const int N = items.size();

  // Create a button for each item
  auto tab_bar = new score::QGraphicsEnum{m_tabs, this};
  tab_bar->setPos(m_padding, m_padding);
  tab_bar->columns = m_tabs.size();
  tab_bar->updateRect();
  connect(tab_bar, &score::QGraphicsEnum::currentIndexChanged,
          this, [this] (int idx) {
    auto page = m_pages[idx];
    if(page != current)
    {
      page->setVisible(true);
      current->setVisible(false);
      current = page;
    }
  });

  const double y = tab_bar->boundingRect().height() + m_padding;
  // Layout the pages
  for(int i = 0; i < N; i++)
  {
    auto page = items[i];
    page->setPos(m_padding, y);
    page->setVisible(false);
  }

  // Show the first tab
  if(N > 0)
  {
    current = items[0];
    current->setVisible(true);
  }
}

}
