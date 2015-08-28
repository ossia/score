#include "AddStateToEvent.hpp"

#include "Document/State/StateModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddStateToStateModel::AddStateToStateModel(
        Path<StateModel>&& path,
        const iscore::StatePath& parent_path,
        const iscore::StateNode& state,
        int posInParent) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_path{std::move(path)},
    m_parentPath{parent_path},
    m_state{state},
    m_pos{posInParent}
{

}


void AddStateToStateModel::undo()
{
    auto& stateModel = m_path.find();
    auto parent = m_parentPath.toNode(&stateModel.states().rootNode());
    stateModel.states().removeState(parent->childAt(m_pos));
}

void AddStateToStateModel::redo()
{
    auto& stateModel = m_path.find();
    auto parent = m_parentPath.toNode(&stateModel.states().rootNode());
    stateModel.states().addState(parent,
                                 new StateNode{m_state},
                                 m_pos);
}

void AddStateToStateModel::serializeImpl(QDataStream& s) const
{
    s << m_path << m_parentPath << m_state << m_pos;
}

void AddStateToStateModel::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_parentPath >> m_state >> m_pos;
}
