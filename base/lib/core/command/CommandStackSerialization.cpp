#include <iscore/serialization/DataStreamVisitor.hpp>
#include <qbytearray.h>
#include <qdatastream.h>
#include <qglobal.h>
#include <qlist.h>
#include <qpair.h>
#include <qstack.h>

#include "CommandStack.hpp"
#include "core/application/ApplicationComponents.hpp"
#include "core/application/ApplicationContext.hpp"
#include "iscore/command/SerializableCommand.hpp"
#include "iscore/plugins/customfactory/StringFactoryKey.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const CommandStack& stack)
{
    QList<QPair <QPair <CommandParentFactoryKey, CommandFactoryKey>, QByteArray> > undoStack, redoStack;
    for(const auto& cmd : stack.m_undoable)
    {
        undoStack.push_back({{cmd->parentKey(), cmd->key()}, cmd->serialize()});
    }
    for(const auto& cmd : stack.m_redoable)
    {
        redoStack.push_back({{cmd->parentKey(), cmd->key()}, cmd->serialize()});
    }

    m_stream << undoStack << redoStack;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(CommandStack& stack)
{
    QList<QPair <QPair <CommandParentFactoryKey, CommandFactoryKey>, QByteArray> > undoStack, redoStack;
    m_stream >> undoStack >> redoStack;

    checkDelimiter();


    stack.updateStack([&] ()
    {
        stack.setSavedIndex(-1);

        for(const auto& elt : undoStack)
        {
            auto cmd = context.components.instantiateUndoCommand(
                        elt.first.first,
                        elt.first.second,
                        elt.second);

            cmd->redo();
            stack.m_undoable.push(cmd);
        }

        for(const auto& elt : redoStack)
        {
            auto cmd = context.components.instantiateUndoCommand(
                        elt.first.first,
                        elt.first.second,
                        elt.second);

            stack.m_redoable.push(cmd);
        }
    });
}
