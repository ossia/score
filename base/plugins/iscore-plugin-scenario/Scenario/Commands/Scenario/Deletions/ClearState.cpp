#include <QDebug>
#include <algorithm>

#include "ClearState.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>


using namespace iscore;
using namespace Scenario::Command;

ClearState::ClearState(Path<StateModel>&& path) :
    m_path {std::move(path) }
{
    ISCORE_TODO;
    /*
    const auto& state = m_path.find();

    m_serializedStates = marshall<DataStream>(state.messages().rootNode());
    */
}

void ClearState::undo() const
{
    ISCORE_TODO;
    /*
    auto& state = m_path.find();
    iscore::Node states;
    QDataStream s(m_serializedStates);
    s >> states;

    state.messages() = states;
    */
}

void ClearState::redo() const
{
    ISCORE_TODO;
    /*
    auto& state = m_path.find();

    state.messages() = iscore::Node{};
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
