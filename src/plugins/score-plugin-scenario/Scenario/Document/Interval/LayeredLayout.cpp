#include "LayeredLayout.hpp"

#include <ossia/detail/small_vector.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>

namespace Scenario
{
namespace
{
constexpr double columnGap = 90.;
constexpr double rowGap = 24.;
constexpr double dummyHeight = 6.;
constexpr double dummyGap = 8.;
constexpr double componentGap = 80.;
constexpr double singletonRowWidth = 1600.;

constexpr int orderingSweeps = 24;
constexpr int placementSweeps = 12;

struct Node
{
  QSizeF size;
  bool dummy{};
  int layer{};
  int rank{};
  double y{};
};

void placeInOrder(
    std::vector<Node>& nodes, const std::vector<int>& layer,
    const std::vector<double>& desired, const std::vector<double>& weight)
{
  const std::size_t n = layer.size();
  std::vector<double> offset(n);
  for(std::size_t i = 1; i < n; i++)
  {
    const auto& prev = nodes[layer[i - 1]];
    const bool dummy = prev.dummy || nodes[layer[i]].dummy;
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

class Component
{
public:
  Component(std::vector<Node>& nodes, std::vector<LayoutEdge> edges, std::vector<int> members)
      : m_nodes{nodes}
      , m_edges{std::move(edges)}
      , m_members{std::move(members)}
  {
  }

  void run()
  {
    buildAdjacency();
    breakCycles();
    buildAdjacency();
    assignLayers();
    addDummies();
    buildAdjacency();
    order();
    placeVertically();
  }

  std::vector<std::vector<int>> layers;

private:
  void buildAdjacency()
  {
    m_in.assign(m_nodes.size(), {});
    m_out.assign(m_nodes.size(), {});
    for(std::size_t i = 0; i < m_edges.size(); i++)
    {
      m_out[m_edges[i].from].push_back(i);
      m_in[m_edges[i].to].push_back(i);
    }
  }

  void breakCycles()
  {
    std::vector<uint8_t> state(m_nodes.size());
    std::vector<std::pair<int, std::size_t>> stack;
    std::vector<bool> isBack(m_edges.size());
    for(int root : m_members)
    {
      if(state[root])
        continue;
      stack.push_back({root, 0});
      state[root] = 1;
      while(!stack.empty())
      {
        auto& [v, next] = stack.back();
        if(next < m_out[v].size())
        {
          const auto ei = m_out[v][next++];
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
      if(isBack[i])
      {
        auto& e = m_edges[i];
        e = LayoutEdge{e.to, e.from, e.toY, e.fromY};
      }
  }

  void assignLayers()
  {
    for(int v : m_topo)
      m_nodes[v].layer = 0;
    for(int v : m_topo)
      for(auto ei : m_out[v])
      {
        auto& to = m_nodes[m_edges[ei].to];
        to.layer = std::max(to.layer, m_nodes[v].layer + 1);
      }
    for(auto it = m_topo.rbegin(); it != m_topo.rend(); ++it)
    {
      if(m_out[*it].empty())
        continue;
      int closest = std::numeric_limits<int>::max();
      for(auto ei : m_out[*it])
        closest = std::min(closest, m_nodes[m_edges[ei].to].layer);
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
    std::vector<LayoutEdge> segments;
    segments.reserve(m_edges.size());
    for(const auto& e : m_edges)
    {
      int prev = e.from;
      double prevY = e.fromY;
      for(int l = m_nodes[e.from].layer + 1; l < m_nodes[e.to].layer; l++)
      {
        const int d = int(m_nodes.size());
        m_nodes.push_back(Node{QSizeF{0., dummyHeight}, true, l});
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
        m_nodes[l[i]].rank = int(i);
  }

  int64_t crossings() const
  {
    int64_t total = 0;
    std::vector<std::pair<int, int>> segs;
    std::vector<int> tree;
    for(std::size_t l = 0; l + 1 < layers.size(); l++)
    {
      segs.clear();
      for(int v : layers[l])
        for(auto ei : m_out[v])
          segs.push_back({m_nodes[v].rank, m_nodes[m_edges[ei].to].rank});
      std::sort(segs.begin(), segs.end());

      const std::size_t n = layers[l + 1].size();
      tree.assign(n + 1, 0);
      int64_t inserted = 0;
      for(const auto& [from, to] : segs)
      {
        int64_t notAbove = 0;
        for(int i = to + 1; i > 0; i -= i & -i)
          notAbove += tree[i];
        total += inserted - notAbove;
        for(int i = to + 1; i <= int(n); i += i & -i)
          tree[i]++;
        inserted++;
      }
    }
    return total;
  }

  void sortLayer(std::vector<int>& l, bool fromPredecessors)
  {
    std::vector<std::pair<double, int>> keys;
    keys.reserve(l.size());
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
      m_nodes[l[i]].rank = int(i);
    }
  }

  void order()
  {
    auto best = layers;
    int64_t bestCrossings = crossings();
    for(int sweep = 0; sweep < orderingSweeps && bestCrossings > 0; sweep++)
    {
      if(sweep % 2 == 0)
        for(std::size_t l = 1; l < layers.size(); l++)
          sortLayer(layers[l], true);
      else
        for(int l = int(layers.size()) - 2; l >= 0; l--)
          sortLayer(layers[l], false);

      if(const auto c = crossings(); c < bestCrossings)
      {
        bestCrossings = c;
        best = layers;
      }
    }
    layers = std::move(best);
    for(auto& l : layers)
      for(std::size_t i = 0; i < l.size(); i++)
        m_nodes[l[i]].rank = int(i);
  }

  void placeLayer(const std::vector<int>& l, int direction)
  {
    std::vector<double> desired, weight;
    desired.reserve(l.size());
    weight.reserve(l.size());
    ossia::small_vector<double, 8> targets;
    for(int v : l)
    {
      targets.clear();
      if(direction <= 0)
        for(auto ei : m_in[v])
        {
          const auto& e = m_edges[ei];
          targets.push_back(m_nodes[e.from].y + e.fromY - e.toY);
        }
      if(direction >= 0)
        for(auto ei : m_out[v])
        {
          const auto& e = m_edges[ei];
          targets.push_back(m_nodes[e.to].y + e.toY - e.fromY);
        }

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
        weight.push_back(m_nodes[v].dummy ? 2. : 1.);
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
  std::vector<LayoutEdge> m_edges;
  std::vector<int> m_members;
  std::vector<int> m_topo;
  std::vector<std::vector<std::size_t>> m_in, m_out;
};
}

std::vector<QPointF>
layeredLayout(const std::vector<QSizeF>& sizes, const std::vector<LayoutEdge>& edges)
{
  const int count = int(sizes.size());
  std::vector<QPointF> res(count);
  if(count == 0)
    return res;

  std::vector<Node> nodes;
  nodes.reserve(count);
  for(const auto& s : sizes)
    nodes.push_back(Node{s});

  std::vector<int> root(count);
  std::iota(root.begin(), root.end(), 0);
  auto find = [&](int v) {
    while(root[v] != v)
      v = root[v] = root[root[v]];
    return v;
  };
  std::vector<LayoutEdge> valid;
  valid.reserve(edges.size());
  for(const auto& e : edges)
  {
    if(e.from < 0 || e.to < 0 || e.from >= count || e.to >= count || e.from == e.to)
      continue;
    valid.push_back(e);
    root[find(e.from)] = find(e.to);
  }

  std::vector<int> componentOf(count, -1);
  std::vector<std::vector<int>> members;
  for(int v = 0; v < count; v++)
  {
    int& c = componentOf[find(v)];
    if(c < 0)
    {
      c = int(members.size());
      members.emplace_back();
    }
    members[c].push_back(v);
  }
  std::vector<std::vector<LayoutEdge>> componentEdges(members.size());
  for(const auto& e : valid)
    componentEdges[componentOf[find(e.from)]].push_back(e);

  std::vector<std::size_t> byDecreasingSize(members.size());
  std::iota(byDecreasingSize.begin(), byDecreasingSize.end(), 0);
  std::stable_sort(
      byDecreasingSize.begin(), byDecreasingSize.end(),
      [&](auto a, auto b) { return members[a].size() > members[b].size(); });

  double y = 0.;
  double widest = 0.;
  std::vector<int> singletons;
  for(auto c : byDecreasingSize)
  {
    if(members[c].size() == 1)
    {
      singletons.push_back(members[c].front());
      continue;
    }

    Component component{nodes, std::move(componentEdges[c]), members[c]};
    component.run();

    double top = std::numeric_limits<double>::max();
    double bottom = std::numeric_limits<double>::lowest();
    for(const auto& l : component.layers)
      for(int v : l)
      {
        top = std::min(top, nodes[v].y);
        bottom = std::max(bottom, nodes[v].y + nodes[v].size.height());
      }

    double x = 0.;
    for(const auto& l : component.layers)
    {
      double width = 0.;
      for(int v : l)
      {
        if(nodes[v].dummy)
          continue;
        res[v] = {x, y + nodes[v].y - top};
        width = std::max(width, nodes[v].size.width());
      }
      x += width + columnGap;
    }
    widest = std::max(widest, x - columnGap);
    y += bottom - top + componentGap;
  }

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
    res[v] = {x, y};
    x += nodes[v].size.width() + rowGap;
    rowHeight = std::max(rowHeight, nodes[v].size.height());
  }
  return res;
}
}
