#include "EditValue.hpp"
#include <Document/State/StateModel.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <ProcessInterface/State/ProcessStateDataInterface.hpp>

EditValue::EditValue(
        Path<MessageItemModel> &&device_tree,
        const MessageNodePath& nodePath,
        const iscore::Value& value):
    iscore::SerializableCommand{factoryName(),
                                commandName(),
                                description()},
    m_path{device_tree},
    m_nodePath{nodePath}
{
    auto& model = m_path.find();
    m_oldState = model.rootNode();

    auto n = m_nodePath.toNode(&model.rootNode());
    ISCORE_ASSERT(n && n->parent() && n->parent()->parent());

    // TODO fetch the new values from the processes by asking for it ?
    // This makes sense if we are in a mode where all the values in
    // the state are collapsed into one.
    // What if we don't want this and we instead want all three values sent ?

    auto& sm = model.stateModel;
    iscore::MessageList mess{iscore::Message{address(*n), value}};
    for(ProcessStateDataInterface* prevProc : sm.previousProcesses())
    {
        auto lst = prevProc->setMessages(mess, m_oldState);

    }

}

void EditValue::undo()
{
    ISCORE_TODO;
    /*
    auto& model = m_path.find();
    model.editData(m_nodePath, m_old);
    */
}

void EditValue::redo()
{
    ISCORE_TODO;
    /*
    auto& model = m_path.find();
    model.editData(m_nodePath, m_new);
    */
}

void EditValue::serializeImpl(QDataStream &d) const
{
    ISCORE_TODO;
    /*
    d << m_path
      << m_nodePath
      << m_old
      << m_new;
      */
}

void EditValue::deserializeImpl(QDataStream &d)
{
    /*
    d >> m_path
      >> m_nodePath
      >> m_old
      >> m_new;
      */
}
