// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "RecordProviderFactory.hpp"

#include <Recording/Commands/Record.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/document/DocumentContext.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Recording::RecordContext)
namespace Recording
{
RecordProvider::~RecordProvider() = default;
RecorderFactory::~RecorderFactory() = default;

RecordContext::RecordContext(Scenario::ProcessModel& scenar, Scenario::Point pt)
    : context{score::IDocument::documentContext(scenar)}
    , scenario{scenar}
    , explorer{Explorer::deviceExplorerFromContext(context)}
    , dispatcher{context.commandStack}
    , point{pt}

{
  connect(
      this, &RecordContext::startTimer, this, &RecordContext::on_startTimer, Qt::QueuedConnection);
}
}
