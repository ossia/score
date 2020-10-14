#pragma once
#include <score/model/IdentifiedObjectAbstract.hpp>

#include <QPointer>

#include <verdigris>

/**
 * A selection is a set of objects.
 */
class Selection final : private QList<QPointer<IdentifiedObjectAbstract>>
{
  using base_type = QList<QPointer<IdentifiedObjectAbstract>>;

public:
  using base_type::at;
  using base_type::base_type;
  using base_type::begin;
  using base_type::cbegin;
  using base_type::cend;
  using base_type::clear;
  using base_type::constBegin;
  using base_type::constEnd;
  using base_type::contains;
  using base_type::empty;
  using base_type::end;
  using base_type::erase;
  using base_type::removeAll;
  using base_type::size;

  static Selection fromList(const QList<IdentifiedObjectAbstract*>& other)
  {
    Selection s;
    for (auto elt : other)
    {
      s.base_type::append(elt);
    }
    return s;
  }

  bool contains(const IdentifiedObjectAbstract& obj) const noexcept
  {
    for(const auto& ptr : *this)
    {
      if(ptr.data() == &obj)
        return true;
    }
    return false;
  }

  bool contains(const IdentifiedObjectAbstract* obj) const noexcept
  {
    for(const auto& ptr : *this)
    {
      if(ptr.data() == obj)
        return true;
    }
    return false;
  }

  void append(const IdentifiedObjectAbstract& obj)
  {
    append(&obj);
  }

  void append(const IdentifiedObjectAbstract* obj)
  {
    auto ptr = const_cast<IdentifiedObjectAbstract*>(obj);
    if (!contains(ptr))
      base_type::append(ptr);
  }

  void append(const base_type::value_type& obj)
  {
    if (!contains(obj))
      base_type::append(obj);
  }

  void append(base_type::value_type&& obj)
  {
    if (!contains(obj))
      base_type::append(std::move(obj));
  }

  bool operator==(const Selection& other) const
  {
    return base_type::operator!=(static_cast<const base_type&>(other));
  }

  bool operator!=(const Selection& other) const
  {
    return base_type::operator!=(static_cast<const base_type&>(other));
  }

  QList<const IdentifiedObjectAbstract*> toList() const
  {
    QList<const IdentifiedObjectAbstract*> l;
    for (const auto& elt : *this)
      l.push_back(elt);
    return l;
  }

  SCORE_LIB_BASE_EXPORT
  void removeDuplicates();
};

template <typename T>
/**
 * @brief filterSelections Filter selections in the standard GUI way
 * @param pressedModel The latest pressed item
 * @param sel The current selection
 * @param cumulation Do we want cumulative selection
 * @return The new selection
 *
 * This methods filter selections like it behaves in standard GUIs :
 *  - simply clicking on an item deselects the current selection and selects it
 *  - if Ctrl is pressed :
 *     - if the item was selected it is deselected
 *     - else it is added to the current selection
 */
Selection filterSelections(T* pressedModel, Selection sel, bool cumulation)
{
  if (!cumulation)
  {
    sel.clear();
  }

  const auto ptr = const_cast<std::remove_const_t<T>*>(pressedModel);
  // If the pressed element is selected
  if (pressedModel->selection.get())
  {
    if (cumulation)
    {
      sel.removeAll(ptr);
    }
    else
    {
      sel.append(ptr);
    }
  }
  else
  {
    sel.append(ptr);
  }

  return sel;
}

/**
 * @brief filterSelections Like the other method but with multi-selections.
 *
 * For instance if multiples rectangles are drawn with the mouse
 * with ctrl pressed.
 */
inline Selection
filterSelections(Selection& newSelection, const Selection& currentSelection, bool cumulation)
{
  if (cumulation)
  {
    for (auto& elt : currentSelection)
      newSelection.append(elt);
  }

  return newSelection;
}
W_REGISTER_ARGTYPE(Selection)
