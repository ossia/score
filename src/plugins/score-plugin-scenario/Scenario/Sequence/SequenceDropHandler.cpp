// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequenceDropHandler.hpp"

#include <Scenario/Sequence/Commands/SetSequenceNamespace.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <Device/Node/NodeListMimeSerialization.hpp>

#include <QMimeData>

namespace Sequence
{

bool SequenceDropHandler::drop(
    const score::DocumentContext& ctx, const Scenario::IntervalModel& interval,
    QPointF, const QMimeData& mime)
{
  if(!mime.hasFormat(score::mime::nodelist()))
    return false;

  auto* seq = findSequenceProcess(const_cast<Scenario::IntervalModel&>(interval));
  if(!seq)
    return false;

  Mime<Device::FreeNodeList>::Deserializer des{mime};
  const Device::FreeNodeList nl = des.deserialize();

  CommandDispatcher<> dispatcher{ctx.commandStack};
  for(const auto& node : nl)
  {
    State::AddressAccessor addr{node.first};
    if(addr.address.device.isEmpty())
      continue;
    dispatcher.submit<Sequence::Command::AddSequenceParameter>(*seq, addr);
  }

  return true;
}

} // namespace Sequence
