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

    setStyleSheet(R"_(
QScrollArea
{
    border: 1px solid #3A3939;
    border-radius: 2px;
    padding: 0;
    background-color: #12171A;
}
QScrollArea QLabel
{
    background-color: #12171A;
}
)_");
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
