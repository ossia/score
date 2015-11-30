#include <iscore/serialization/DataStreamVisitor.hpp>
#include <QByteArray>
#include <QDataStream>
#include <QtGlobal>
#include <QList>
#include <QPair>
#include <QStack>

#include "CommandStack.hpp"
#include <core/application/ApplicationComponents.hpp>
#include <core/application/ApplicationContext.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const CommandStack& stack)
{
    std::vector<CommandData> undoStack, redoStack;
    for(const auto& cmd : stack.m_undoable)
    {
        undoStack.emplace_back(*cmd);
    }
    readFrom_vector_obj_impl(*this, undoStack);

    for(const auto& cmd : stack.m_redoable)
    {
        redoStack.emplace_back(*cmd);
    }
    readFrom_vector_obj_impl(*this, redoStack);

    insertDelimiter();
}
