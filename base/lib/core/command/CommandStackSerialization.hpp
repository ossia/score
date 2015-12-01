#pragma once
#include <core/command/CommandStack.hpp>
#include <iscore/command/CommandData.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>

template<typename RedoFun>
void loadCommandStack(
        const iscore::ApplicationComponents& components,
        Visitor<Writer<DataStream>>& writer,
        iscore::CommandStack& stack,
        RedoFun redo_fun)
{
    std::vector<iscore::CommandData> undoStack, redoStack;
    writeTo_vector_obj_impl(writer, undoStack);
    writeTo_vector_obj_impl(writer, redoStack);

    writer.checkDelimiter();

    stack.undoable().clear();
    stack.redoable().clear();

    stack.updateStack([&] ()
    {
        stack.setSavedIndex(-1);

        for(const auto& elt : undoStack)
        {
            auto cmd = components.instantiateUndoCommand(elt);

            redo_fun(cmd);
            stack.undoable().push(cmd);
        }

        for(const auto& elt : redoStack)
        {
            auto cmd = components.instantiateUndoCommand(elt);

            stack.redoable().push(cmd);
        }
    });
}
