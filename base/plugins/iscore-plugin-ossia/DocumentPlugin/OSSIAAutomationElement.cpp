#include "OSSIAAutomationElement.hpp"

#include <API/Headers/Editor/Automation.h>
#include <API/Headers/Editor/State.h>
#include <API/Headers/Editor/Message.h>
#include <API/Headers/Editor/Value.h>
#include "../iscore-plugin-curve/Automation/AutomationModel.hpp"
#include "iscore2OSSIA.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include "../iscore-plugin-deviceexplorer/Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp"
#include "Protocols/OSSIADevice.hpp"
OSSIAAutomationElement::OSSIAAutomationElement(const AutomationModel *element, QObject *parent):
    OSSIAProcessElement{parent},
    m_iscore_autom{element},
    m_deviceList{static_cast<DeviceDocumentPlugin>(iscore::IDocument::documentFromObject(element)->model()->pluginModel("DeviceDocumentPlugin")).list()}
{
    using namespace iscore::convert;
    // auto node = getNodeFromPath(element->address().path, &static_cast<OSSIADevice*>(&m_deviceList.device(element->address().device))->impl());

    connect(element, &AutomationModel::addressChanged,
            this, &OSSIAAutomationElement::on_addressChanged);
    on_addressChanged(element->address());
}

std::shared_ptr<OSSIA::TimeProcess> OSSIAAutomationElement::process() const
{
    return m_ossia_autom;
}

#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include "iscore2OSSIA.hpp"
#include "Protocols/OSSIADevice.hpp"
#include <API/Headers/Editor/Curve.h>
#include <API/Headers/Editor/CurveSegment/CurveSegmentLinear.h>
void OSSIAAutomationElement::on_addressChanged(const iscore::Address& addr)
{
    auto doc = iscore::IDocument::documentFromObject(m_iscore_autom);
    auto plug = static_cast<DeviceDocumentPlugin*>(doc->model()->pluginModel("DeviceDocumentPlugin"));
    const auto& devices = plug->list().devices();

    // Look for the real node in the device
    auto dev_it = std::find_if(devices.begin(), devices.end(),
                               [&] (DeviceInterface* dev) {
        return dev->settings().name == addr.device;
    });

    if(dev_it == devices.end())
    {
        // TODO clear the automation
        return;
    }

    auto node = iscore::convert::findNodeFromPath(addr.path, &static_cast<OSSIADevice&>(**dev_it).impl());
    if(!node)
    {
        // TODO clear the automation
        return;
    }

    // Add the real address
    auto address = node->getAddress();
    using namespace OSSIA;

    auto curve = Curve<float>::create();
    auto linearSegment = CurveSegmentLinear<float>::create(curve);

    curve->setInitialValue(0.);
    curve->addPoint(0.5, 1., linearSegment);
    curve->addPoint(1., 0., linearSegment);

    // create a Tuple value of 3 Behavior values based on the same curve
    std::vector<const Value*> t_curves = {new Behavior(curve)};
    Tuple* curves = new Tuple(t_curves); // TODO memleak

    // create an Automation for /test address drived by one curve
    auto new_autom = Automation::create([](
                                               const OSSIA::TimeValue& position,
                                               const OSSIA::TimeValue& date,
                                               std::shared_ptr<OSSIA::State> state)
     {
         qDebug() << "automati callback" << double(position);
     }, address, new Behavior(curve));

    // add "/test 0. 0. 0." message to Automation's start State
    std::vector<const Value*> t_zero = {new Float(0.)};
    Tuple* zero = new Tuple(t_zero);
    auto first_start_message = Message::create(address, new Float(0.));
    new_autom->getStartState()->stateElements().push_back(first_start_message);

    // add "/test 1. 1. 1." message to Automation's end State
    std::vector<const Value*> t_one = {new Float(1.)};
    Tuple* one = new Tuple(t_one);
    auto first_end_message = Message::create(address, new Float(1.));
    new_autom->getEndState()->stateElements().push_back(first_end_message);


    /*
    auto myCurve = Curve<float>::create();
    auto firstCurveSegment = CurveSegmentLinear<float>::create(myCurve);
    auto secondCurveSegment = CurveSegmentLinear<float>::create(myCurve);

    myCurve->setInitialValue(0.);
    myCurve->addPoint(1., 1., firstCurveSegment);
    myCurve->addPoint(2., 0., secondCurveSegment);


    auto new_autom = OSSIA::Automation::create([](
                                              const OSSIA::TimeValue& position,
                                              const OSSIA::TimeValue& date,
                                              std::shared_ptr<OSSIA::State> state)
    {
        qDebug() << "automati callback" << double(position);
    }, address, new Behavior{myCurve});

    if(m_iscore_autom->parent()->objectName() != QString("BaseConstraintModel"))
    {
        new_autom->getClock()->setExternal(true);
    }
    else
    {
        new_autom->getClock()->setSpeed(1.);
        new_autom->getClock()->setGranularity(250.);
    }



    // add "/test 0." message to Automation's start State
    OSSIA::Float zero(0.);
    auto first_start_message = OSSIA::Message::create(address, &zero);
    // TODO is this okay for removal?
    new_autom->getStartState()->stateElements().clear();
    new_autom->getStartState()->stateElements().push_back(first_start_message);

    // add "/test 1." message to Automation's end State
    OSSIA::Float one(1.);
    auto first_end_message = OSSIA::Message::create(address, &one);
    new_autom->getEndState()->stateElements().clear();
    new_autom->getEndState()->stateElements().push_back(first_end_message);

    */
    auto old_autom = m_ossia_autom;
    m_ossia_autom = new_autom;
    emit changed(old_autom, new_autom);
}
