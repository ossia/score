// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/range/iterator_range.hpp>

#include "Graph.hpp"

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <boost/graph/connected_components.hpp>
#include <score/model/ModelMetadata.hpp>

namespace Scenario
{

TimenodeGraph::TimenodeGraph(
    const Scenario::ProcessModel& scenar):
  m_scenario{scenar}
{
  for(auto& tn : scenar.getTimeSyncs())
  {
    m_vertices[&tn] = boost::add_vertex(&tn, m_graph);
  }

  for(auto& cst : scenar.getIntervals())
  {
    m_edges[&cst] = boost::add_edge(
          m_vertices[&Scenario::startTimeSync(cst, scenar)],
          m_vertices[&Scenario::endTimeSync(cst, scenar)],
          &cst,
          m_graph).first;

  }
}

void TimenodeGraph::writeGraphviz()
{
  auto get_name = [] (auto* elt) { return elt->metadata().getName().toStdString(); };

  std::stringstream s;
  boost::write_graphviz(s, m_graph, [&] (auto& out, const auto& v) {
    out << "[label=\"" << get_name(this->m_graph[v]) << "\"]";
  }, [&] (auto& out, const auto& v) {
    out << "[label=\"" << get_name(this->m_graph[v]) << "\"]";
  });

  std::cout << s.str() << std::endl << std::flush;
}

TimenodeGraphComponents TimenodeGraph::components()
{
  std::vector<int> component(boost::num_vertices(m_graph));
  int num = boost::connected_components(m_graph, &component[0]);

  std::vector<TimenodeGraphConnectedComponent> comps(num);
  for(auto vtx : m_vertices)
  {
    auto& comp = comps[component[vtx.second]];
    comp.syncs.push_back(vtx.first);
    for(auto& cst : Scenario::previousIntervals(*vtx.first, m_scenario))
    {
      comp.intervals.push_back(&m_scenario.interval(cst));
    }
    for(auto& cst : Scenario::nextIntervals(*vtx.first, m_scenario))
    {
      comp.intervals.push_back(&m_scenario.interval(cst));
    }
  }
  return {m_scenario, comps};
}

bool TimenodeGraphComponents::isInMain(const EventModel& c) const
{
  return isInMain(Scenario::parentTimeSync(c, parentScenario(c)));
}
bool TimenodeGraphComponents::isInMain(const StateModel& c) const
{
  return isInMain(Scenario::parentTimeSync(c, parentScenario(c)));
}
bool TimenodeGraphComponents::isInMain(const TimeSyncModel& c) const
{
  auto rs = &scenario.startTimeSync();

  auto it = ossia::find_if(comps, [&] (const auto& comp) {
    return ossia::contains(comp.syncs, rs) && ossia::contains(comp.syncs, &c);
  });
  return it != comps.end();
}

bool TimenodeGraphComponents::isInMain(const IntervalModel& c) const
{
  auto rs = &scenario.startTimeSync();

  auto it = ossia::find_if(comps, [&] (const auto& comp) {
    return ossia::contains(comp.syncs, rs) && ossia::contains(comp.intervals, &c);
  });
  return it != comps.end();
}

const TimenodeGraphConnectedComponent& TimenodeGraphComponents::component(
    const Scenario::TimeSyncModel& c) const
{
  auto it = ossia::find_if(comps, [&] (const auto& comp) {
    return ossia::contains(comp.syncs, &c);
  });
  SCORE_ASSERT(it != comps.end());
  return *it;
}

bool TimenodeGraphConnectedComponent::isMain(const ProcessModel& root) const
{
  auto rs = &root.startTimeSync();
  return ossia::contains(syncs, rs);
}

}
