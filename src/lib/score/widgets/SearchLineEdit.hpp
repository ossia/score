#pragma once
#include <QAction>
#include <QLineEdit>
#include <wobjectdefs.h>

namespace score
{

class SearchLineEdit : public QLineEdit
{
  W_OBJECT(SearchLineEdit)
public:
  SearchLineEdit(QWidget* parent);

  ~SearchLineEdit() override;
  virtual void search() = 0;
};

}
