#include "InterpolationInspectorWidget.hpp"
#include <Interpolation/Commands/ChangeAddress.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <iscore/widgets/ReactiveLabel.hpp>
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>

namespace Interpolation
{
InspectorWidget::InspectorWidget(
        const ProcessModel& automationModel,
        const iscore::DocumentContext& doc,
        QWidget* parent) :
    InspectorWidgetDelegate_T {automationModel, parent},
    m_dispatcher{doc.commandStack}
{
    using namespace Explorer;
    setObjectName("InterpolationInspectorWidget");
    setParent(parent);

    auto vlay = new QFormLayout;
    vlay->setSpacing(0);
    vlay->setContentsMargins(0,0,0,0);

    // LineEdit
    // If there is a DeviceExplorer in the current document, use it
    // to make a widget.
    // TODO instead of doing this, just make an address line edit factory.
    auto plug = doc.findPlugin<DeviceDocumentPlugin>();
    DeviceExplorerModel* explorer{};
    if(plug)
        explorer = &plug->explorer();
    m_lineEdit = new AddressAccessorEditWidget{explorer, this};

    m_lineEdit->setAddress(process().address());
    con(process(), &ProcessModel::addressChanged,
            m_lineEdit, &AddressAccessorEditWidget::setAddress);

    connect(m_lineEdit, &AddressAccessorEditWidget::addressChanged,
            this, &InspectorWidget::on_addressChange);

    vlay->addRow(tr("Address"), m_lineEdit);

    this->setLayout(vlay);
}

void InspectorWidget::on_addressChange(const ::State::AddressAccessor& addr)
{
    // Various checks
    if(addr == process().address())
        return;

    if(addr.address.path.isEmpty())
        return;

    if(addr == State::AddressAccessor{})
    {
        m_dispatcher.submitCommand(new ChangeAddress{process(), {}, {}, {}});
    }
    else
    {
        // Try to find a matching state in the start & end state in order to update the process
        auto cst = dynamic_cast<Scenario::ConstraintModel*>(process().parent());
        if(!cst)
            return;
        auto parent_scenario = dynamic_cast<Scenario::ScenarioInterface*>(cst->parent());
        if(!parent_scenario)
            return;

        State::Value sv, ev;

        {
            auto& ss = Scenario::startState(*cst, *parent_scenario);
            Process::MessageNode* snode = Process::try_getNodeFromAddress(ss.messages().rootNode(), addr);
            if(snode && snode->hasValue())
                sv = *snode->value();
        }

        {
            auto& es = Scenario::endState(*cst, *parent_scenario);
            Process::MessageNode* enode = Process::try_getNodeFromAddress(es.messages().rootNode(), addr);
            if(enode && enode->hasValue())
                ev = *enode->value();
        }

        m_dispatcher.submitCommand(new ChangeAddress{process(), addr, sv, ev});
    }
}


StateInspectorWidget::StateInspectorWidget(
        const ProcessState& object,
        const iscore::DocumentContext& doc,
        QWidget* parent):
    InspectorWidgetBase{object, doc, parent},
    m_state{object},
    m_label{new QLabel}
{
    std::list<QWidget*> vec;
    vec.push_back(m_label);

    con(m_state, &ProcessStateDataInterface::stateChanged,
        this,    &StateInspectorWidget::on_stateChanged);

    on_stateChanged();

    updateSectionsView(safe_cast<QVBoxLayout*>(layout()), vec);
}

void StateInspectorWidget::on_stateChanged()
{
    m_label->setText(State::convert::toPrettyString(m_state.message().value));
}

StateInspectorFactory::StateInspectorFactory() :
    InspectorWidgetFactory {}
{

}

Inspector::InspectorWidgetBase* StateInspectorFactory::makeWidget(
        const QList<const QObject*>& sourceElements,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new StateInspectorWidget{
                safe_cast<const ProcessState&>(*sourceElements.first()),
                doc,
                parent};
}

bool StateInspectorFactory::matches(const QList<const QObject*>& objects) const
{
    return dynamic_cast<const ProcessState*>(objects.first());
}
}
