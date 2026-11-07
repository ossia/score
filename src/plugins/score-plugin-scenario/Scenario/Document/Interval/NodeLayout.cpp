#include "NodeLayout.hpp"

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/Interval/LayoutNodes.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/selection/SelectionStack.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QMenu>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace Scenario
{
namespace
{
// NodeItem's geometry: the title is drawn above the position, the ports on
// either side of the content, one every 10 px.
constexpr double titleHeight = 20.;
constexpr double portMargin = 10.;
constexpr double portSpacing = 10.;
constexpr double minimumContentWidth = 75.;
constexpr double minimumContentHeight = 22.;

constexpr double columnGap = 90.;
constexpr double rowGap = 24.;
constexpr double dummyHeight = 6.;
constexpr double dummyGap = 8.;
constexpr double componentGap = 80.;
constexpr double singletonRowWidth = 1600.;
constexpr QPointF layoutOrigin{40., 40.};

constexpr int orderingSweeps = 24;
constexpr int placementSweeps = 12;

struct Node
{
  const Process::ProcessModel* process{};
  QSizeF size;
  QPointF origin;
  int layer{};
  int rank{};
  double y{};
};

struct Edge
{
  int from{}, to{};
  double fromY{}, toY{};
};

template <typename Ports>
double portOffset(const Ports& ports, const Process::Port* port)
{
  int row = 0;
  for(auto* p : ports)
  {
    if(p->displayHandledExplicitly)
      continue;
    if(p == port)
      return titleHeight + row * portSpacing + portSpacing / 2.;
    row++;
  }
  return titleHeight / 2.;
}

int displayedPortCount(const auto& ports)
{
  return std::count_if(ports.begin(), ports.end(), [](auto* p) {
    return !p->displayHandledExplicitly;
  });
}

QRectF estimatedBounds(const Process::ProcessModel& proc)
{
  const auto sz = proc.size();
  const int rows
      = std::max(displayedPortCount(proc.inlets()), displayedPortCount(proc.outlets()));
  return {
      -portMargin, -titleHeight,
      std::max(sz.width(), minimumContentWidth) + 2. * portMargin,
      std::max({sz.height(), minimumContentHeight, rows * portSpacing + 2.})
          + titleHeight};
}

const Process::ProcessModel*
owningProcess(const QObject* obj, const IntervalModel& interval)
{
  for(; obj; obj = obj->parent())
    if(obj->parent() == &interval)
      return qobject_cast<const Process::ProcessModel*>(obj);
  return nullptr;
}

// Positions minimising the squared distance to `desired` under the
// constraint that the nodes stay in order and do not overlap: isotonic
// regression (pool adjacent violators) of desired - minimal offset.
void placeInOrder(
    std::vector<Node>& nodes, const std::vector<int>& layer,
    const std::vector<double>& desired, const std::vector<double>& weight)
{
  const std::size_t n = layer.size();
  std::vector<double> offset(n);
  for(std::size_t i = 1; i < n; i++)
  {
    const auto& prev = nodes[layer[i - 1]];
    const bool dummy = !prev.process || !nodes[layer[i]].process;
    offset[i] = offset[i - 1] + prev.size.height() + (dummy ? dummyGap : rowGap);
  }

  struct Block
  {
    double weight, sum;
    std::size_t count;
    double value() const { return sum / weight; }
  };
  std::vector<Block> blocks;
  for(std::size_t i = 0; i < n; i++)
  {
    blocks.push_back({weight[i], weight[i] * (desired[i] - offset[i]), 1});
    while(blocks.size() > 1 && blocks[blocks.size() - 2].value() > blocks.back().value())
    {
      auto last = blocks.back();
      blocks.pop_back();
      blocks.back().weight += last.weight;
      blocks.back().sum += last.sum;
      blocks.back().count += last.count;
    }
  }
  std::size_t i = 0;
  for(const auto& b : blocks)
    for(std::size_t k = 0; k < b.count; k++, i++)
      nodes[layer[i]].y = offset[i] + b.value();
}

struct ComponentLayout
{
  std::vector<int> members;
  QRectF bounds;
};

class Layout
{
public:
  Layout(std::vector<Node>& nodes, const std::vector<Edge>& edges)
      : m_nodes{nodes}
  {
    for(const auto& e : edges)
      m_edges.push_back(e);
  }

  void run(const std::vector<int>& members)
  {
    m_members = members;
    breakCycles();
    assignLayers();
    addDummies();
    order();
    placeVertically();
  }

  std::vector<std::vector<int>> layers;

private:
  std::vector<int> successorsOf(int v, const std::vector<Edge>& edges) const
  {
    std::vector<int> res;
    for(const auto& e : edges)
      if(e.from == v)
        res.push_back(e.to);
    return res;
  }

  void breakCycles()
  {
    ossia::hash_map<int, int> state;
    std::vector<Edge> kept;
    std::vector<bool> isBack(m_edges.size());
    std::vector<std::pair<int, std::size_t>> stack;
    std::vector<std::vector<std::size_t>> out(m_nodes.size());
    for(std::size_t i = 0; i < m_edges.size(); i++)
      out[m_edges[i].from].push_back(i);

    for(int root : m_members)
    {
      if(state[root])
        continue;
      stack.push_back({root, 0});
      state[root] = 1;
      while(!stack.empty())
      {
        auto& [v, next] = stack.back();
        if(next < out[v].size())
        {
          const auto ei = out[v][next++];
          const int w = m_edges[ei].to;
          if(state[w] == 1)
            isBack[ei] = true;
          else if(!state[w])
          {
            state[w] = 1;
            stack.push_back({w, 0});
          }
        }
        else
        {
          state[v] = 2;
          m_topo.push_back(v);
          stack.pop_back();
        }
      }
    }
    std::reverse(m_topo.begin(), m_topo.end());
    for(std::size_t i = 0; i < m_edges.size(); i++)
      if(!isBack[i])
        kept.push_back(m_edges[i]);
    m_edges = std::move(kept);
  }

  void assignLayers()
  {
    for(int v : m_topo)
      m_nodes[v].layer = 0;
    for(int v : m_topo)
      for(const auto& e : m_edges)
        if(e.from == v)
          m_nodes[e.to].layer = std::max(m_nodes[e.to].layer, m_nodes[v].layer + 1);

    // Everything but the sinks moves right up to its first consumer, so that
    // a parameter source sits next to what it feeds and not in column 0.
    for(auto it = m_topo.rbegin(); it != m_topo.rend(); ++it)
    {
      int closest = std::numeric_limits<int>::max();
      for(const auto& e : m_edges)
        if(e.from == *it)
          closest = std::min(closest, m_nodes[e.to].layer);
      if(closest != std::numeric_limits<int>::max())
        m_nodes[*it].layer = closest - 1;
    }

    int lowest = std::numeric_limits<int>::max();
    for(int v : m_members)
      lowest = std::min(lowest, m_nodes[v].layer);
    for(int v : m_members)
      m_nodes[v].layer -= lowest;
  }

  void addDummies()
  {
    std::vector<Edge> segments;
    for(const auto& e : m_edges)
    {
      int prev = e.from;
      double prevY = e.fromY;
      for(int l = m_nodes[e.from].layer + 1; l < m_nodes[e.to].layer; l++)
      {
        const int d = m_nodes.size();
        m_nodes.push_back(Node{nullptr, QSizeF{0., dummyHeight}, {}, l});
        m_members.push_back(d);
        segments.push_back({prev, d, prevY, dummyHeight / 2.});
        prev = d;
        prevY = dummyHeight / 2.;
      }
      segments.push_back({prev, e.to, prevY, e.toY});
    }
    m_edges = std::move(segments);

    int count = 0;
    for(int v : m_members)
      count = std::max(count, m_nodes[v].layer + 1);
    layers.assign(count, {});
    for(int v : m_members)
      layers[m_nodes[v].layer].push_back(v);
    for(auto& l : layers)
      for(std::size_t i = 0; i < l.size(); i++)
        m_nodes[l[i]].rank = i;

    m_in.assign(m_nodes.size(), {});
    m_out.assign(m_nodes.size(), {});
    for(std::size_t i = 0; i < m_edges.size(); i++)
    {
      m_out[m_edges[i].from].push_back(i);
      m_in[m_edges[i].to].push_back(i);
    }
  }

  int crossings() const
  {
    int total = 0;
    for(const auto& l : layers)
    {
      std::vector<std::pair<int, int>> segs;
      for(int v : l)
        for(auto ei : m_out[v])
          segs.push_back({m_nodes[v].rank, m_nodes[m_edges[ei].to].rank});
      for(std::size_t i = 0; i < segs.size(); i++)
        for(std::size_t j = i + 1; j < segs.size(); j++)
          if((segs[i].first - segs[j].first) * (segs[i].second - segs[j].second) < 0)
            total++;
    }
    return total;
  }

  void sortLayer(std::vector<int>& l, bool fromPredecessors)
  {
    std::vector<std::pair<double, int>> keys;
    for(int v : l)
    {
      const auto& adj = fromPredecessors ? m_in[v] : m_out[v];
      double key = m_nodes[v].rank;
      if(!adj.empty())
      {
        double sum = 0.;
        for(auto ei : adj)
          sum += m_nodes[fromPredecessors ? m_edges[ei].from : m_edges[ei].to].rank;
        key = sum / adj.size();
      }
      keys.push_back({key, v});
    }
    std::stable_sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) {
      return a.first < b.first;
    });
    for(std::size_t i = 0; i < l.size(); i++)
    {
      l[i] = keys[i].second;
      m_nodes[l[i]].rank = i;
    }
  }

  void order()
  {
    auto best = layers;
    int bestCrossings = crossings();
    for(int sweep = 0; sweep < orderingSweeps && bestCrossings > 0; sweep++)
    {
      if(sweep % 2 == 0)
        for(std::size_t l = 1; l < layers.size(); l++)
          sortLayer(layers[l], true);
      else
        for(int l = int(layers.size()) - 2; l >= 0; l--)
          sortLayer(layers[l], false);

      if(const int c = crossings(); c < bestCrossings)
      {
        bestCrossings = c;
        best = layers;
      }
    }
    layers = std::move(best);
    for(auto& l : layers)
      for(std::size_t i = 0; i < l.size(); i++)
        m_nodes[l[i]].rank = i;
  }

  void placeLayer(const std::vector<int>& l, int direction)
  {
    std::vector<double> desired, weight;
    for(int v : l)
    {
      std::vector<double> targets;
      auto collect = [&](const std::vector<std::size_t>& adj, bool incoming) {
        for(auto ei : adj)
        {
          const auto& e = m_edges[ei];
          targets.push_back(
              incoming ? m_nodes[e.from].y + e.fromY - e.toY
                       : m_nodes[e.to].y + e.toY - e.fromY);
        }
      };
      if(direction <= 0)
        collect(m_in[v], true);
      if(direction >= 0)
        collect(m_out[v], false);

      if(targets.empty())
      {
        desired.push_back(m_nodes[v].y);
        weight.push_back(0.1);
      }
      else
      {
        std::sort(targets.begin(), targets.end());
        const auto n = targets.size();
        desired.push_back(
            n % 2 ? targets[n / 2] : (targets[n / 2 - 1] + targets[n / 2]) / 2.);
        weight.push_back(m_nodes[v].process ? 1. : 2.);
      }
    }
    placeInOrder(m_nodes, l, desired, weight);
  }

  void placeVertically()
  {
    for(const auto& l : layers)
      placeInOrder(
          m_nodes, l, std::vector<double>(l.size(), 0.),
          std::vector<double>(l.size(), 1.));

    for(int sweep = 0; sweep < placementSweeps; sweep++)
    {
      if(sweep % 2 == 0)
        for(std::size_t l = 1; l < layers.size(); l++)
          placeLayer(layers[l], -1);
      else
        for(int l = int(layers.size()) - 2; l >= 0; l--)
          placeLayer(layers[l], 1);
    }
    for(const auto& l : layers)
      placeLayer(l, 0);
  }

  std::vector<Node>& m_nodes;
  std::vector<Edge> m_edges;
  std::vector<int> m_members;
  std::vector<int> m_topo;
  std::vector<std::vector<std::size_t>> m_in, m_out;
};
}

std::vector<NodePlacement> layoutNodes(
    const IntervalModel& interval, std::vector<const Process::ProcessModel*> procs,
    const score::DocumentContext& ctx, const NodeGeometry& geometry)
{
  const bool subset = !procs.empty();
  if(!subset)
    for(const auto& p : interval.processes)
      procs.push_back(&p);
  std::sort(procs.begin(), procs.end(), [](auto* a, auto* b) { return a->id() < b->id(); });
  if(procs.empty())
    return {};

  std::vector<Node> nodes;
  ossia::hash_map<const Process::ProcessModel*, int> index;
  std::optional<QRectF> previousBounds;
  for(auto* p : procs)
  {
    index[p] = nodes.size();
    std::optional<QRectF> bounds;
    if(geometry.bounds)
      bounds = geometry.bounds(*p);
    const QRectF r = bounds ? *bounds : estimatedBounds(*p);
    nodes.push_back(Node{p, r.size(), r.topLeft()});
    const QRectF placed = r.translated(p->position());
    previousBounds = previousBounds ? previousBounds->united(placed) : placed;
  }

  auto portY = [&](const Node& n, const auto& ports, const Process::Port* port) {
    if(geometry.portY)
      if(auto y = geometry.portY(*port))
        return *y - n.origin.y();
    return portOffset(ports, port);
  };

  std::vector<Edge> edges;
  for(std::size_t i = 0; i < procs.size(); i++)
  {
    for(auto* outlet : procs[i]->outlets())
    {
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
        if(std::any_of(edges.begin(), edges.end(), [&](const Edge& e) {
             return e.from == int(i) && e.to == to;
           }))
          continue;
        edges.push_back(
            {int(i), to, portY(nodes[i], procs[i]->outlets(), outlet),
             portY(nodes[to], procs[to]->inlets(), inlet)});
      }
    }
  }

  std::vector<int> root(nodes.size());
  std::iota(root.begin(), root.end(), 0);
  auto find = [&](int v) {
    while(root[v] != v)
      v = root[v] = root[root[v]];
    return v;
  };
  for(const auto& e : edges)
    root[find(e.from)] = find(e.to);

  std::vector<std::vector<int>> components;
  {
    ossia::hash_map<int, std::size_t> componentOf;
    for(std::size_t v = 0; v < procs.size(); v++)
    {
      auto [it, inserted] = componentOf.try_emplace(find(v), components.size());
      if(inserted)
        components.emplace_back();
      components[it->second].push_back(v);
    }
  }
  std::stable_sort(components.begin(), components.end(), [](auto& a, auto& b) {
    return a.size() > b.size();
  });

  std::vector<NodePlacement> res;
  auto place = [&](const Node& n, QPointF topLeft) {
    res.push_back({n.process, topLeft - n.origin});
  };

  double y = 0.;
  double widest = 0.;
  std::vector<int> singletons;
  for(const auto& members : components)
  {
    if(members.size() == 1)
    {
      singletons.push_back(members.front());
      continue;
    }

    std::vector<Edge> local;
    for(const auto& e : edges)
      if(find(e.from) == find(members.front()))
        local.push_back(e);

    Layout layout{nodes, local};
    layout.run(members);

    double x = 0.;
    double top = std::numeric_limits<double>::max(), bottom = 0.;
    for(const auto& l : layout.layers)
      for(int v : l)
      {
        top = std::min(top, nodes[v].y);
        bottom = std::max(bottom, nodes[v].y + nodes[v].size.height());
      }
    for(const auto& l : layout.layers)
    {
      double width = 0.;
      for(int v : l)
      {
        if(!nodes[v].process)
          continue;
        place(nodes[v], {x, y + nodes[v].y - top});
        width = std::max(width, nodes[v].size.width());
      }
      x += width + columnGap;
    }
    widest = std::max(widest, x - columnGap);
    y += bottom - top + componentGap;
  }

  {
    const double rowWidth = std::max(widest, singletonRowWidth);
    double x = 0., rowHeight = 0.;
    for(int v : singletons)
    {
      if(x > 0. && x + nodes[v].size.width() > rowWidth)
      {
        x = 0.;
        y += rowHeight + rowGap;
        rowHeight = 0.;
      }
      place(nodes[v], {x, y});
      x += nodes[v].size.width() + rowGap;
      rowHeight = std::max(rowHeight, nodes[v].size.height());
    }
  }

  const QPointF origin = subset ? previousBounds->topLeft() : layoutOrigin;
  for(auto& p : res)
    p.position += origin;
  return res;
}

void autoLayoutNodes(
    const IntervalModel& interval, std::vector<const Process::ProcessModel*> nodes,
    const score::DocumentContext& ctx, const NodeGeometry& geometry)
{
  auto placements = layoutNodes(interval, std::move(nodes), ctx, geometry);
  if(placements.empty())
    return;
  CommandDispatcher<> disp{ctx.commandStack};
  disp.submit(new Command::LayoutNodes{placements});
}

void addNodeLayoutActions(
    QMenu& menu, const IntervalModel& interval, const score::DocumentContext& ctx,
    NodeGeometry geometry, std::function<void()> laidOutAll)
{
  std::vector<const Process::ProcessModel*> selected;
  for(auto* p : filterSelectionByType<Process::ProcessModel>(
          ctx.selectionStack.currentSelection()))
    if(p->parent() == &interval)
      selected.push_back(p);

  auto all = menu.addAction(QObject::tr("Auto-layout"));
  QObject::connect(
      all, &QAction::triggered, &menu, [&interval, &ctx, geometry, laidOutAll] {
    autoLayoutNodes(interval, {}, ctx, geometry);
    if(laidOutAll)
      laidOutAll();
  });
  if(selected.size() > 1)
  {
    auto sel = menu.addAction(QObject::tr("Auto-layout selection"));
    QObject::connect(
        sel, &QAction::triggered, &menu, [&interval, &ctx, selected, geometry] {
      autoLayoutNodes(interval, selected, ctx, geometry);
    });
  }
}
}
