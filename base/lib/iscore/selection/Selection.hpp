#pragma once
#include <QObject>
#include <unordered_set>
using Selection = std::unordered_set<const QObject*>;

template<typename T>
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
