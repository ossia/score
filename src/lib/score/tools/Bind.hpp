#pragma once
#include <qobjectdefs.h>

#include <utility>

/**
 * @brief con A wrapper around Qt's connect
 *
 * Allows the first argument to be a reference
 */
template <typename T, typename... Args>
QMetaObject::Connection con(const T& t, Args&&... args)
{
  return T::connect(&t, std::forward<Args>(args)...);
}

template <typename T, typename Property, typename U, typename Slot, typename... Args>
QMetaObject::Connection bind(T& t, const Property&, const U* tgt, Slot&& slot, Args&&... args)
{
  slot((t.*(Property::get))());

  return T::connect(
      &t, Property::notify, tgt, std::forward<Slot>(slot), std::forward<Args>(args)...);
}

template <typename Entities, typename Presenter>
void bind(const Entities& model, Presenter& presenter)
{
  for (auto& entity : model)
    presenter.on_created(entity);

  model.mutable_added.template connect<&Presenter::on_created>(presenter);
  model.removed.template connect<&Presenter::on_removing>(presenter);
}
