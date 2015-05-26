#include "OSSIAControl.hpp"
#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Node.h>
#include <API/Headers/Network/Protocol.h>

#include <API/Headers/Editor/Expression.h>
#include <API/Headers/Editor/ExpressionNot.h>
#include <API/Headers/Editor/ExpressionAtom.h>
#include <API/Headers/Editor/ExpressionComposition.h>
#include <API/Headers/Editor/ExpressionValue.h>

OSSIAControl::OSSIAControl(iscore::Presenter* pres):
    iscore::PluginControlInterface {pres, "IScoreCohesionControl", nullptr}
{
    using namespace OSSIA;
    Local localDevice;
    m_localDevice = Device::create(localDevice, "ALocalDevice");
    // Two parts :
    // One that maintains the devices for each document
    // (and disconnects / reconnects them when the current document changes)
    // Also during execution, one shouldn't be able to switch document.

    // Another part that, at execution time, creates structures corresponding
    // to the Scenario plug-in with the OSSIA API.

    // TODO create the local device here.
}
