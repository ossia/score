#pragma once
#include <Library/RecursiveFilterProxy.hpp>

#include <score/widgets/SearchLineEdit.hpp>

#include <QSortFilterProxyModel>
#include <QTreeView>

#include <functional>

namespace Library
{
class RecursiveFilterProxy;
struct ItemModelFilterLineEdit final : public score::SearchLineEdit
{
public:
  ItemModelFilterLineEdit(RecursiveFilterProxy& proxy, QTreeView& tv, QWidget* p)
      : score::SearchLineEdit{p}
      , m_proxy{proxy}
      , m_view{tv}
  {
    connect(this, &QLineEdit::textEdited, this, [=] { search(); });
  }

  void search() override
  {
    if(text() != m_proxy.pattern())
    {
      m_proxy.setPattern(text());

      if(!text().isEmpty())
      {
        m_view.expandAll();

        if(m_proxy.hasChildren())
        {
          auto item_to_select = QModelIndex();
          while(m_proxy.hasChildren(item_to_select))
          {
            item_to_select = m_proxy.index(0, 0, item_to_select);
          }
          m_view.setCurrentIndex(item_to_select);
        }
      }
    }

    if(text().isEmpty())
    {
      m_proxy.invalidate();
      m_view.collapseAll();
    }
    if(reset)
    {
      reset();
    }
  }

  std::function<void()> reset;
  RecursiveFilterProxy& m_proxy;
  QTreeView& m_view;
};

}
