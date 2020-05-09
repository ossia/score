#pragma once
#include <score/widgets/SearchLineEdit.hpp>

#include <QSortFilterProxyModel>
#include <QTreeView>

namespace Library
{

struct ItemModelFilterLineEdit final : public score::SearchLineEdit
{
public:
  ItemModelFilterLineEdit(
      QSortFilterProxyModel& proxy,
      QTreeView& tv,
      QWidget* p)
      : score::SearchLineEdit{p}
      , m_proxy{proxy}
      , m_view{tv}
  {
    connect(this, &QLineEdit::textEdited, this, [=] { search(); });
  }

  void search() override
  {
    if (text() != m_proxy.filterRegExp().pattern())
    {
      m_proxy.setFilterRegExp(
          QRegExp(text(), Qt::CaseInsensitive, QRegExp::FixedString));

      if (text().isEmpty())
        m_view.collapseAll();
      else
        m_view.expandAll();
    }
  }

  QSortFilterProxyModel& m_proxy;
  QTreeView& m_view;
};

}
