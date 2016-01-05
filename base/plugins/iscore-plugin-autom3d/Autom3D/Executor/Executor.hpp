#pragma once
#include <OSSIA/ProcessModel/TimeProcessWithConstraint.hpp>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/ExecutorContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <Autom3D/Point.hpp>

class vtkParametricSpline;
class DeviceDocumentPlugin;
class DeviceList;
namespace State
{
class Address;
}
namespace RecreateOnPlay
{
class ConstraintElement;
}
namespace OSSIA {
class State;
class StateElement;
class Address;
}  // namespace OSSIA

namespace Autom3D
{
class ProcessModel;
namespace Executor
{
class ProcessExecutor final :
        public TimeProcessWithConstraint
{
    public:
        ProcessExecutor(
                const State::Address& addr,
                const std::vector<Point>& spline,
                const DeviceList& devices);
        ~ProcessExecutor();

        std::shared_ptr<OSSIA::StateElement> state(
                const OSSIA::TimeValue&,
                const OSSIA::TimeValue&) override;

        const std::shared_ptr<OSSIA::State>& getStartState() const override
        {
            return m_start;
        }

        const std::shared_ptr<OSSIA::State>& getEndState() const override
        {
            return m_end;
        }

    private:
        const DeviceList& m_devices;

        std::shared_ptr<OSSIA::State> m_start;
        std::shared_ptr<OSSIA::State> m_end;

        std::shared_ptr<OSSIA::Address> m_addr;

        vtkParametricSpline* m_spline{};

};

}
}
