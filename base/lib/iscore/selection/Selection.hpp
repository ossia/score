#pragma once
#include <QObject>
#include <QPointer>

/**
 * A selection is a set of objects.
 */
class Selection final : private QList<QPointer<const QObject>>
{
        using base_type = QList<QPointer<const QObject>>;
    public:
        using base_type::base_type;
        using base_type::begin;
        using base_type::end;
        using base_type::cbegin;
        using base_type::cend;
        using base_type::erase;
        using base_type::empty;
        using base_type::clear;
        using base_type::contains;
        using base_type::removeAll;

        static Selection fromList(const QList<const QObject*> & other)
        {
            Selection s;
            for(auto elt : other)
            {
                s.base_type::append(elt);
            }
            return s;
        }

        void append(base_type::value_type obj)
        {
            if(!contains(obj))
                base_type::append(obj);
        }

        bool operator==(const Selection& other) const
        {
            return base_type::operator !=(static_cast<const base_type&>(other));
        }

        bool operator!=(const Selection& other) const
        {
            return base_type::operator !=(static_cast<const base_type&>(other));
        }

        QList<const QObject*> toList() const
        {
            QList<const QObject*> l;
            for(const auto& elt : *this)
                l.push_back(elt);
            return l;
        }
};

template<typename T>
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
Selection filterSelections(
        T* pressedModel,
        Selection sel,
        bool cumulation)
{
    if(!cumulation)
    {
        sel.clear();
    }

    // If the pressed element is selected
    if(pressedModel->selection.get())
    {
        if(cumulation)
        {
            sel.removeAll(pressedModel);
        }
        else
        {
            sel.append(pressedModel);
        }
    }
    else
    {
        sel.append(pressedModel);
    }

    return sel;
}

/**
 * @brief filterSelections Like the other method but with multi-selections.
 *
 * For instance if multiples rectangles are drawn with the mouse
 * with ctrl pressed.
 */
inline Selection filterSelections(
        Selection& newSelection,
        const Selection& currentSelection,
        bool cumulation)
{
    if(cumulation)
    {
        for(auto& elt : currentSelection)
            newSelection.append(elt);
    }

    return newSelection;
}
