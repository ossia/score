#pragma once
#include <QObject>
#include <QSet>
using Selection = QList<const QObject*>;

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
            sel.removeAll(pressedModel);
        }
        else
        {
            sel.push_back(pressedModel);
        }
    }
    else
    {
        sel.push_back(pressedModel);
    }

    return sel;
}

inline Selection filterSelections(
        const Selection& newSelection,
        const Selection& currentSelection,
        bool cumulation)
{
    return cumulation ? (newSelection + currentSelection).toSet().toList() : newSelection;
}
