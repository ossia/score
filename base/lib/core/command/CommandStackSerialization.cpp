#include "CommandStack.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>

#include <iscore/tools/std/StdlibWrapper.hpp>
#include <iscore/presenter/PresenterInterface.hpp>
#include <core/presenter/Presenter.hpp>

using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const CommandStack& stack)
{
    QList<QPair <QPair <std::string, std::string>, QByteArray> > undoStack, redoStack;
    for(const auto& cmd : stack.m_undoable)
    {
        undoStack.push_back({{cmd->parentName(), cmd->name()}, cmd->serialize()});
    }
    for(const auto& cmd : stack.m_redoable)
    {
        redoStack.push_back({{cmd->parentName(), cmd->name()}, cmd->serialize()});
    }

    m_stream << undoStack << redoStack;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(CommandStack& stack)
{
    QList<QPair <QPair <std::string, std::string>, QByteArray> > undoStack, redoStack;
    m_stream >> undoStack >> redoStack;

    checkDelimiter();


    stack.updateStack([&] ()
    {
        stack.setSavedIndex(-1);

        for(const auto& elt : undoStack)
        {
            auto cmd = IPresenter::instantiateUndoCommand(
                        elt.first.first,
                        elt.first.second,
                        elt.second);

            cmd->redo();
            stack.m_undoable.push(cmd);
        }

        for(const auto& elt : redoStack)
        {
            auto cmd = IPresenter::instantiateUndoCommand(
                        elt.first.first,
                        elt.first.second,
                        elt.second);

            stack.m_redoable.push(cmd);
        }
    });
}
