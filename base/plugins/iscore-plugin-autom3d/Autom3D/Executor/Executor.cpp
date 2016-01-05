#include "Executor.hpp"
#include <vtkParametricSpline.h>
#include <vtkPoints.h>
#include <iscore/tools/std/Algorithms.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/Protocols/OSSIADevice.hpp>
#include <Network/Node.h>
#include <Editor/Message.h>
#include <Device/Protocol/DeviceList.hpp>
namespace Autom3D
{
namespace Executor
{

ProcessExecutor::ProcessExecutor(
        const State::Address& addr,
        const std::vector<Point>& spline,
        const DeviceList& devices):
    m_devices{devices},
    m_start{OSSIA::State::create()},
    m_end{OSSIA::State::create()},
    m_spline{vtkParametricSpline::New()}
{
    // Load the address
    // Look for the real node in the device
    auto dev_it = devices.find(addr.device);
    if(dev_it == devices.devices().end())
        return;

    auto dev = dynamic_cast<OSSIADevice*>(*dev_it);
    if(!dev)
        return;

    auto node = iscore::convert::findNodeFromPath(addr.path, &dev->impl());
    if(!node)
        return;

    // Add the real address
    m_addr = node->getAddress();
    if(!m_addr)
        return;

    // Load the spline
    auto points = vtkPoints::New();
    for(const Point& pt : spline)
    {
        points->InsertNextPoint(pt.x(), pt.y(), pt.z());
    }
    m_spline->SetPoints(points);
}

ProcessExecutor::~ProcessExecutor()
{
    m_spline->Delete();
}

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state(
        const OSSIA::TimeValue& t,
        const OSSIA::TimeValue&)
{
    auto st = OSSIA::State::create();
    if(m_addr)
    {
        double u[3]{t, 0, 0};
        double pt[3];
        double du[6];
        m_spline->Evaluate(u, pt, du);

        auto mess = OSSIA::Message::create(m_addr,
                                           new OSSIA::Tuple{
                                               new OSSIA::Float{float(pt[0])},
                                               new OSSIA::Float{float(pt[1])},
                                               new OSSIA::Float{float(pt[2])}});

        st->stateElements().push_back(std::move(mess));
    }

    return st;

}

}
}
