#pragma once
#include <QAction>
#include <QLineEdit>

namespace score
{

class SearchLineEdit : public QLineEdit
{
public:
  SearchLineEdit(QWidget* parent) : QLineEdit{parent}
  {
    setPlaceholderText("Search");
    auto act = new QAction{this};
    act->setIcon(QIcon(":/icons/search.png"));
    addAction(act, QLineEdit::TrailingPosition);

    connect(this, &QLineEdit::returnPressed, [&]() { search(); });
    connect(act, &QAction::triggered, [&]() { search(); });
  }

  virtual void search() = 0;
};
}
