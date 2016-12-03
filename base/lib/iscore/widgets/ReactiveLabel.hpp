#pragma once
#include <QLabel>
#include <iscore/tools/Todo.hpp>

namespace iscore
{
template <typename Property_T>
class ReactiveLabel : public QLabel
{
public:
  ReactiveLabel(const typename Property_T::model_type& model, QWidget* parent)
      : QLabel{(model.*Property_T::get())(), parent}
  {
    con(model, Property_T::notify(), this, &QLabel::setText);
  }
};
}
