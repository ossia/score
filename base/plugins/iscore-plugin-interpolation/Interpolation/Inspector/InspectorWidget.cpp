#include "InspectorWidget.hpp"
#include <Interpolation/Commands/ChangeAddress.hpp>
#include <QCheckBox>
#include <QFormLayout>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
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

    // Tween
    m_tween = new QCheckBox;
    vlay->addRow(tr("Tween"), m_tween);
    m_tween->setChecked(process().tween());
    con(process(), &ProcessModel::tweenChanged, m_tween, &QCheckBox::setChecked);
    connect(m_tween, &QCheckBox::toggled,
            this, &InspectorWidget::on_tweenChanged);

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

void InspectorWidget::on_addressChange(const ::State::Address& newAddr)
{
    // Various checks
    if(newAddr == process().address())
        return;

    if(newAddr.path.isEmpty())
        return;

    ISCORE_TODO;
    //auto cmd = new ChangeAddress{process(), newAddr};

    //m_dispatcher.submitCommand(cmd);
}

void InspectorWidget::on_tweenChanged()
{
    bool newVal = m_tween->checkState();
    if(newVal != process().tween())
    {
        auto cmd = new SetTween{process(), newVal};

        m_dispatcher.submitCommand(cmd);
    }
}
}
