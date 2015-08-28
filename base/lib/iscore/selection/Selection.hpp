#pragma once
#include <QObject>
#include <unordered_set>

/**
 * A selection is a set of objects.
 */
using Selection = std::unordered_set<const QObject*>;

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
            sel.erase(pressedModel);
        }
        else
        {
            sel.insert(pressedModel);
        }
    }
    else
    {
        sel.insert(pressedModel);
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
            newSelection.insert(elt);
    }

    return newSelection;
}
