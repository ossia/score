// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationState.hpp"

#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <Process/State/ProcessStateDataInterface.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Automation/AutomationModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/network/dataspace/dataspace_visitors.hpp>

class QObject;
namespace Automation
{
ProcessState::ProcessState(ProcessModel& model, double watchedPoint, QObject* parent)
    : ProcessStateDataInterface{model, parent}
    , m_point{watchedPoint}
{
  SCORE_ASSERT(0 <= watchedPoint && watchedPoint <= 1);

  // The neighbouring states and automations and the inspector listen to
  // this state: a curve change that leaves its message as it was (a resize,
  // a point in the middle of the curve) must not reach them, or resizing an
  // interval with many automations rebuilds the message trees of every state.
  con(this->process(), &ProcessModel::curveChanged, this, &ProcessState::updateMessages);
  con(this->process(), &ProcessModel::addressChanged, this,
      &ProcessState::updateMessages);
  // With tween the start has no message of its own and takes the address over
  if(m_point == 0.)
    con(this->process(), &ProcessModel::tweenChanged, this, &ProcessState::updateMessages);
}

void ProcessState::updateMessages()
{
  auto current = messages();
  if(m_announced && *m_announced == current)
    return;
  m_announced = std::move(current);
  stateChanged();
}

// TESTME
::State::Message ProcessState::message() const
{
  if(m_point == 0. && process().tween())
    return {};

  // Set-up a message
  ::State::Message m;
  m.address = process().address();

  // Look in the tree if there is a corresponding node,
  // so that we can get the type that we should convert to.
  // Default is float.
  ossia::value treeValue = ossia::value(0.f);
  auto& ctx = score::IDocument::documentContext(process());
  auto tree = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(tree)
  {
    auto node = Device::try_getNodeFromAddress(tree->rootNode(), m.address.address);
    if(node && node->is<Device::AddressSettings>())
    {
      treeValue = node->get<Device::AddressSettings>().value;
    }
  }

  for(const auto& seg : process().curve().segments())
  {
    // OPTIMIZEME introduce another index on that has an ordering on curve
    // segments
    // to make this fast (just checking for the first and the last).
    if(seg.start().x() <= m_point && seg.end().x() >= m_point)
    {
      m.value = float(
          seg.valueAt(m_point) * (process().max() - process().min()) + process().min());

      return m;
    }
  }

  return {};
}

double ProcessState::point() const
{
  return m_point;
}

ProcessModel& ProcessState::process() const
{
  return static_cast<ProcessModel&>(ProcessStateDataInterface::process());
}

std::vector<State::AddressAccessor> ProcessState::matchingAddresses()
{
  // TODO have a better check of "address validity"
  if(!process().address().address.device.isEmpty())
    return {process().address()};
  return {};
}

std::vector<State::AddressAccessor> ProcessState::takenOverAddresses() const
{
  if(m_point == 0. && process().tween()
     && !process().address().address.device.isEmpty())
    return {process().address()};
  return {};
}

::State::MessageList ProcessState::messages() const
{
  if(!process().address().address.device.isEmpty())
  {
    auto mess = message();
    if(!mess.address.address.device.isEmpty())
      return {mess};
  }

  return {};
}

// TESTME
::State::MessageList ProcessState::setMessages(
    const ::State::MessageList& received, const Process::MessageNode&)
{
  if(m_point != 0. && m_point != 1.)
    return messages();

  for(const auto& mess : received)
  {
    if(mess.address == process().address())
    {
      // Scale min, max, and the value
      // TODO convert to the real type of the curve.
      auto val = State::convert::value<float>(mess.value);
      if(val < process().min() && std::abs(val - process().min()) > 1e-9)
        process().setMin(val);
      if(val > process().max() && std::abs(val - process().max()) > 1e-9)
        process().setMax(val);

      val = (val - process().min()) / (process().max() - process().min());

      if(m_point == 0.)
      {
        // The segments are in x order.
        const auto& sorted = process().curve().sortedSegments();
        if(!sorted.empty() && sorted.front()->start().x() == 0.)
        {
          // Same (fuzzy) comparison as SegmentModel::setStart: an exact one
          // sees the float differ from the stored double and announces a change
          // setStart does not make, which two automations on the same address
          // around a state then ping-pong forever.
          auto& seg = *sorted.front();
          if(const Curve::Point pt{0, val}; pt != seg.start())
          {
            seg.setStart(pt);
            process().curve().changed();
          }
        }
      }
      else if(m_point == 1)
      {
        const auto& sorted = process().curve().sortedSegments();
        if(!sorted.empty() && sorted.back()->end().x() == 1.)
        {
          auto& seg = *sorted.back();
          if(const Curve::Point pt{1, val}; pt != seg.end())
          {
            seg.setEnd(pt);
            process().curve().changed();
          }
        }
      }
      return messages();
    }
  }
  return messages();
}
}
