#include "NodeLayout.hpp"

#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/NodeItemGeometry.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/LayeredLayout.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/detail/hash_map.hpp>

#include <algorithm>

namespace Scenario
{
namespace
{
constexpr double minimumContentWidth = 75.;
constexpr double minimumContentHeight = 22.;
constexpr QPointF layoutOrigin{40., 40.};

using namespace Process::NodeItemGeometry;

template <typename Ports>
double portOffset(const Ports& ports, const Process::Port* port)
{
  int row = 0;
  for(auto* p : ports)
  {
    if(p->displayHandledExplicitly)
      continue;
    if(p == port)
      return TitleHeight + row * PortSpacing + PortSpacing / 2.;
    row++;
  }
  return TitleHeight / 2.;
}

int displayedPortCount(const auto& ports)
{
  return int(std::count_if(
      ports.begin(), ports.end(), [](auto* p) { return !p->displayHandledExplicitly; }));
}

const Process::ProcessModel* owningProcess(const QObject* obj, const IntervalModel& interval)
{
  for(; obj; obj = obj->parent())
    if(obj->parent() == &interval)
      return qobject_cast<const Process::ProcessModel*>(obj);
  return nullptr;
}
}

QRectF estimatedNodeBounds(const Process::ProcessModel& proc)
{
  QSizeF sz = proc.size();
  if(proc.folded())
    sz = proc.foldedSize().isEmpty() ? QSizeF{} : proc.foldedSize();
  const int rows
      = std::max(displayedPortCount(proc.inlets()), displayedPortCount(proc.outlets()));
  return {
      -SideWidth, -TitleHeight, std::max(sz.width(), minimumContentWidth) + 2. * SideWidth,
      std::max({sz.height(), minimumContentHeight, rows * PortSpacing + 2.})
          + TitleHeight};
}

std::vector<NodePlacement> layoutNodes(
    const IntervalModel& interval, std::vector<const Process::ProcessModel*> procs,
    const score::DocumentContext& ctx, const NodeGeometry& geometry)
{
  const bool subset = !procs.empty();
  if(subset)
    std::erase_if(procs, [&](auto* p) { return !p || p->parent() != &interval; });
  else
    for(const auto& p : interval.processes)
      procs.push_back(&p);
  std::sort(procs.begin(), procs.end(), [](auto* a, auto* b) { return a->id() < b->id(); });
  procs.erase(std::unique(procs.begin(), procs.end()), procs.end());
  if(procs.empty())
    return {};

  std::vector<QSizeF> sizes;
  std::vector<QPointF> origins;
  ossia::hash_map<const Process::ProcessModel*, int> index;
  QRectF previousBounds;
  sizes.reserve(procs.size());
  origins.reserve(procs.size());
  for(std::size_t i = 0; i < procs.size(); i++)
  {
    auto* p = procs[i];
    index[p] = int(i);
    std::optional<QRectF> bounds;
    if(geometry.bounds)
      bounds = geometry.bounds(*p);
    const QRectF r = bounds ? *bounds : estimatedNodeBounds(*p);
    sizes.push_back(r.size());
    origins.push_back(r.topLeft());
    const QRectF placed = r.translated(p->position());
    previousBounds = i == 0 ? placed : previousBounds.united(placed);
  }

  auto portY = [&](int node, const auto& ports, const Process::Port* port) {
    if(geometry.portY)
      if(auto y = geometry.portY(*port))
        return *y - origins[node].y();
    return portOffset(ports, port);
  };

  std::vector<LayoutEdge> edges;
  for(std::size_t i = 0; i < procs.size(); i++)
    for(auto* outlet : procs[i]->outlets())
      for(const auto& path : outlet->cables())
      {
        auto* cable = path.try_find(ctx);
        if(!cable)
          continue;
        auto* inlet = cable->sink().try_find(ctx);
        if(!inlet)
          continue;
        auto it = index.find(owningProcess(inlet, interval));
        if(it == index.end() || it->second == int(i))
          continue;
        const int to = it->second;
        edges.push_back(
            {int(i), to, portY(int(i), procs[i]->outlets(), outlet),
             portY(to, procs[to]->inlets(), inlet)});
      }

  const auto positions = layeredLayout(sizes, edges);
  const QPointF origin = subset ? previousBounds.topLeft() : layoutOrigin;
  std::vector<NodePlacement> res;
  res.reserve(procs.size());
  for(std::size_t i = 0; i < procs.size(); i++)
    res.push_back({procs[i], origin + positions[i] - origins[i]});
  return res;
}

bool autoLayoutNodes(
    const IntervalModel& interval, std::vector<const Process::ProcessModel*> nodes,
    const score::DocumentContext& ctx, const NodeGeometry& geometry)
{
  std::vector<std::pair<const Process::ProcessModel*, QPointF>> positions;
  for(const auto& p : layoutNodes(interval, std::move(nodes), ctx, geometry))
    positions.emplace_back(p.process, p.position);
  auto cmd = std::make_unique<Process::SetNodePositions>(positions);
  if(!cmd->movesAnything())
    return false;
  CommandDispatcher<> disp{ctx.commandStack};
  disp.submit(cmd.release());
  return true;
}
}
