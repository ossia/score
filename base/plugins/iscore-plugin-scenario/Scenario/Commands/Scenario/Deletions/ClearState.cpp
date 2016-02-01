#include <QDebug>
#include <algorithm>

#include "ClearState.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>

namespace Scenario
{
namespace Command
{

ClearState::ClearState(Path<StateModel>&& path) :
    m_path {std::move(path) }
{
    const auto& state = m_path.find();

    m_serializedStates = marshall<DataStream>(state.messages().rootNode());
}

void ClearState::undo() const
{
    /*
    auto& state = m_path.find();
    Process::MessageNode states;
    QDataStream s(m_serializedStates);
    s >> states;

    state.messages() = states;
    */
}

void ClearState::redo() const
{
    /*
    auto& state = m_path.find();

    state.messages() = Device::Node{};
    */
}

void ClearState::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_serializedStates;
}

void ClearState::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_serializedStates;
}
}
}
