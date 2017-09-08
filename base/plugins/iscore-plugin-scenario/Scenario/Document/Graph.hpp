#pragma once
#include <boost/graph/adjacency_list.hpp>
#include <iscore/tools/std/HashMap.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class TimeSyncModel;
class IntervalModel;
class ScenarioInterface;

using GraphVertex = Scenario::TimeSyncModel*;
using GraphEdge = Scenario::IntervalModel*;

using Graph = boost::adjacency_list<
  boost::vecS,
  boost::vecS,
  boost::directedS,
  GraphVertex,
  GraphEdge>;

/**
 * @brief A directed graph of all the TimeSync%s in a ScenarioInterface.
 *
 * The vertices are the TimeSync%s, the edges are the IntervalModel%s.
 * The graph is built upon construction.
 *
 */
struct ISCORE_PLUGIN_SCENARIO_EXPORT TimenodeGraph
{
  TimenodeGraph(const Scenario::ScenarioInterface& scenar);

  const Graph& graph() const
  { return m_graph; }
  const auto& edges() const
  { return m_edges; }
  const auto& vertices() const
  { return m_vertices; }

  //! Writes graphviz output on stdout
  void writeGraphviz();

private:
  Graph m_graph;

  iscore::hash_map<
      const Scenario::TimeSyncModel*,
      Graph::vertex_descriptor> m_vertices;
  iscore::hash_map<
      const Scenario::IntervalModel*,
      Graph::edge_descriptor> m_edges;
};

}
