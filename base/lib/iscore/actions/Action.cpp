#include "Action.hpp"
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <iscore/selection/SelectionStack.hpp>
namespace iscore
{

void ActionManager::reset(iscore::Document* doc)
{

    // Cleanup
    QObject::disconnect(focusConnection);
    QObject::disconnect(selectionConnection);

    MaybeDocument mdoc;
    if(doc)
    {
        mdoc = doc->context();
    }


    // Setup connections
    if(doc)
    {
        focusConnection =
                con(doc->focusManager(), &FocusManager::changed,
                    this, [=] { focusChanged(mdoc); });
        selectionConnection =
                con(doc->selectionStack(), &SelectionStack::currentSelectionChanged,
                    this, [=] (const auto&) { this->selectionChanged(mdoc); });
    }


    // Reset all the actions
    documentChanged(mdoc);
    focusChanged(mdoc);
    selectionChanged(mdoc);
}

void ActionManager::documentChanged(MaybeDocument doc)
{
    for(auto& c_pair : m_docConditions)
    {
        DocumentActionCondition& cond = *c_pair.second;
        if(cond(doc))
        {
            cond.action(doc);
        }
    }
}

void ActionManager::focusChanged(MaybeDocument doc)
{
    for(auto& c_pair : m_focusConditions)
    {
        FocusActionCondition& cond = *c_pair.second;
        if(cond(doc))
        {
            cond.action(doc);
        }
    }
}

void ActionManager::selectionChanged(MaybeDocument doc)
{
    for(auto& c_pair : m_selectionConditions)
    {
        SelectionActionCondition& cond = *c_pair.second;
        if(cond(doc))
        {
            cond.action(doc);
        }
    }
}

void ActionManager::resetCustomActions(MaybeDocument doc)
{
    for(auto& c_pair : m_customConditions)
    {
        CustomActionCondition& cond = *c_pair.second;
        if(cond(doc))
        {
            cond.action(doc);
        }
    }
}

}
