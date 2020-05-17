#pragma once
#include <score/tools/std/HashMap.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/directed_graph.hpp>

#include <nano_observer.hpp>
#include <score_plugin_scenario_export.h>

namespace Scenario
{
class TimeSyncModel;
class IntervalModel;
class EventModel;
class StateModel;
class ScenarioInterface;
class ProcessModel;

using GraphVertex = Scenario::TimeSyncModel*;
using GraphEdge = Scenario::IntervalModel*;

using Graph = boost::directed_graph<GraphVertex, GraphEdge>;
/*
using Graph = boost::adjacency_list<
    boost::vecS,
    boost::vecS,
    boost::directedS,
    GraphVertex,
    GraphEdge>;
*/
/**
 * @brief A directed graph of all the TimeSync%s in a ScenarioInterface.
 *
 * The vertices are the TimeSync%s, the edges are the IntervalModel%s.
 * The graph is built upon construction.
 *
 */

struct SCORE_PLUGIN_SCENARIO_EXPORT TimenodeGraphConnectedComponent
{
  std::vector<const Scenario::TimeSyncModel*> syncs;
  std::vector<const Scenario::IntervalModel*> intervals;

  bool isMain(const Scenario::ProcessModel&) const;
};
struct SCORE_PLUGIN_SCENARIO_EXPORT TimenodeGraphComponents
{
  const Scenario::ProcessModel& scenario;
  std::vector<TimenodeGraphConnectedComponent> comps;

  const TimenodeGraphConnectedComponent& component(const Scenario::TimeSyncModel& c) const;
  bool isInMain(const Scenario::TimeSyncModel& c) const;
  bool isInMain(const Scenario::IntervalModel& c) const;
  bool isInMain(const Scenario::EventModel& c) const;
  bool isInMain(const Scenario::StateModel& c) const;
};
struct SCORE_PLUGIN_SCENARIO_EXPORT TimenodeGraph : public Nano::Observer
{
  TimenodeGraph(const Scenario::ProcessModel& scenar);

  const Graph& graph() const { return m_graph; }
  const auto& edges() const { return m_edges; }
  const auto& vertices() const { return m_vertices; }

  bool hasCycles() const noexcept;
  //! Writes graphviz output on stdout
  void writeGraphviz();

  TimenodeGraphComponents components();

private:
  void intervalsChanged(const IntervalModel&);
  void timeSyncsChanged(const TimeSyncModel&);
  void recompute();

  const Scenario::ProcessModel& m_scenario;
  Graph m_graph;
  bool m_cycles{};

  score::hash_map<const Scenario::TimeSyncModel*, Graph::vertex_descriptor> m_vertices;
  score::hash_map<const Scenario::IntervalModel*, Graph::edge_descriptor> m_edges;
};
}
