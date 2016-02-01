#include <QDebug>
#include <algorithm>

#include "ClearState.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>

namespace Scenario
{
namespace Command
{

ClearState::ClearState(
        Path<StateModel>&& path) :
    m_path {std::move(path) }
{
    const auto& state = m_path.find();

    m_oldState = Process::getUserMessages(state.messages().rootNode());
}

void ClearState::undo() const
{
    auto& state = m_path.find();

    Process::MessageNode n = state.messages().rootNode();
    updateTreeWithMessageList(
                n,
                m_oldState);

    state.messages() = std::move(n);
}

void ClearState::redo() const
{
    auto& state = m_path.find();

    Process::MessageNode n = state.messages().rootNode();
    removeAllUserMessages(n);
    state.messages() = std::move(n);
}

void ClearState::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_oldState;
}

void ClearState::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_oldState;
}
}
}
