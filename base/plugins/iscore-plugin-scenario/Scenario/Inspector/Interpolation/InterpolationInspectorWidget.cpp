#include "InterpolationInspectorWidget.hpp"
#include <Interpolation/Commands/ChangeAddress.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
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
        explorer = &plug->explorer;
    m_lineEdit = new AddressEditWidget{explorer, this};

    m_lineEdit->setAddress(process().address());
    con(process(), &ProcessModel::addressChanged,
            m_lineEdit, &AddressEditWidget::setAddress);

    connect(m_lineEdit, &AddressEditWidget::addressChanged,
            this, &InspectorWidget::on_addressChange);

    vlay->addRow(tr("Address"), m_lineEdit);

    // Min / max
    auto start_label = new QLabel;
    auto end_label = new QLabel;

    vlay->addRow(tr("Start"), start_label);
    vlay->addRow(tr("End"), end_label);

    con(process(), &ProcessModel::startChanged, this, [=] (const State::Value& v) {
        start_label->setText(State::convert::toPrettyString(v));
    });
    con(process(), &ProcessModel::endChanged, this, [=] (const State::Value& v) {
        end_label->setText(State::convert::toPrettyString(v));
    });

    this->setLayout(vlay);
}

void InspectorWidget::on_addressChange(const ::State::Address& addr)
{
    // Various checks
    if(addr == process().address())
        return;

    if(addr.path.isEmpty())
        return;

    if(addr == State::Address{})
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
}
