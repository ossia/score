// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Graph.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <score/model/ModelMetadata.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/range/iterator_range.hpp>

#include <boost/graph/tiernan_all_cycles.hpp>
namespace Scenario
{
/*
struct CycleDetector : public boost::dfs_visitor<>
{
  CycleDetector( )
  {

  }

  template<typename E, typename G>
  void back_edge(const E& e, G& g)
  {
    auto itv = *(Scenario::IntervalModel**)e.get_property();
    if(itv->graphal())
      intervalsInCycles.push_back(itv);
  }

  std::vector<Scenario::IntervalModel*> intervalsInCycles;
};
*/
struct CycleDetector
{
  const Scenario::ProcessModel& scenario;
    CycleDetector(const Scenario::ProcessModel& scenar)
      : scenario{scenar}
    { }

    bool allIntersectGraphal(std::vector<Id<IntervalModel>>& a, std::vector<Id<IntervalModel>>& b)
    {
      std::vector<Id<IntervalModel>> intersect;
      std::sort(a.begin(), a.end(), [] (const auto& lhs, const auto& rhs) { return lhs.val() < rhs.val(); });
      std::sort(b.begin(), b.end(), [] (const auto& lhs, const auto& rhs) { return lhs.val() < rhs.val(); });
      std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(intersect));

      std::vector<IntervalModel*> itvs;
      for(const auto& itv : intersect)
      {
        auto& m = scenario.interval(itv);
        if(!m.graphal())
        {
          return false;
        }
        itvs.push_back(&m);
      }

      for(auto itv : itvs)
        itv->consistency.setValid(false);
      return true;
    }

    void checkIntervalsGraphal(TimeSyncModel& a, TimeSyncModel& b)
    {
      auto prev_a = Scenario::previousIntervals(a, scenario);
      auto next_a = Scenario::nextIntervals(a, scenario);
      auto prev_b = Scenario::previousIntervals(b, scenario);
      auto next_b = Scenario::nextIntervals(b, scenario);

      auto prev_av = std::vector(prev_a.begin(), prev_a.end());
      auto next_av = std::vector(next_a.begin(), next_a.end());

      auto prev_bv = std::vector(prev_b.begin(), prev_b.end());
      auto next_bv = std::vector(next_b.begin(), next_b.end());
      if(allIntersectGraphal(next_av, prev_bv))
        return;
      else if(allIntersectGraphal(next_bv, prev_av))
        return;
    }

    template <typename Path>
    void cycle(const Path& p, const Scenario::Graph& g)
    {
        for(auto i = p.begin(); i != p.end(); ++i) {
          TimeSyncModel* this_ts = (TimeSyncModel*)g[*i];
          TimeSyncModel* next_ts = nullptr;
          auto next = i+1;
          if(next != p.end())
          {
            next_ts = (TimeSyncModel*)g[*next];
          }
          else
          {
            next_ts = (TimeSyncModel*)g[*p.begin()];
          }

          checkIntervalsGraphal(*this_ts, *next_ts);
        }
    }
};

TimenodeGraph::TimenodeGraph(const Scenario::ProcessModel& scenar)
    : m_scenario{scenar}
 {
  for (auto& tn : scenar.getTimeSyncs())
  {
    m_vertices[&tn] = boost::add_vertex(&tn, m_graph);
  }

  for (auto& cst : scenar.getIntervals())
  {
    m_edges[&cst] = boost::add_edge(
                        m_vertices[&Scenario::startTimeSync(cst, scenar)],
                        m_vertices[&Scenario::endTimeSync(cst, scenar)],
                        &cst,
                        m_graph)
                        .first;
  }

  scenar.intervals.added.connect<&TimenodeGraph::intervalsChanged>(this);
  scenar.intervals.removing.connect<&TimenodeGraph::intervalsChanged>(this);
  scenar.timeSyncs.added.connect<&TimenodeGraph::timeSyncsChanged>(this);
  scenar.timeSyncs.removing.connect<&TimenodeGraph::timeSyncsChanged>(this);
}

void TimenodeGraph::recompute()
{
  m_vertices.clear();
  m_edges.clear();
  m_graph.clear();

  for (auto& tn : m_scenario.getTimeSyncs())
  {
    m_vertices[&tn] = boost::add_vertex(&tn, m_graph);
  }

  for (auto& cst : m_scenario.getIntervals())
  {
    cst.consistency.setValid(true);
    m_edges[&cst] = boost::add_edge(
                        m_vertices[&Scenario::startTimeSync(cst, m_scenario)],
                        m_vertices[&Scenario::endTimeSync(cst, m_scenario)],
                        &cst,
                        m_graph)
                        .first;
  }

  CycleDetector vis{m_scenario};
  tiernan_all_cycles(m_graph, vis);
}

void TimenodeGraph::writeGraphviz()
{
  auto get_name
      = [](auto* elt) { return elt->metadata().getName().toStdString(); };

  std::stringstream s;
  boost::write_graphviz(
      s,
      m_graph,
      [&](auto& out, const auto& v) {
        out << "[label=\"" << get_name(this->m_graph[v]) << "\"]";
      },
      [&](auto& out, const auto& v) {
        out << "[label=\"" << get_name(this->m_graph[v]) << "\"]";
      });

  std::cout << s.str() << std::endl << std::flush;
}
/*
TimenodeGraphComponents TimenodeGraph::components()
{
  ossia::int_vector component(boost::num_vertices(m_graph));
  int num = boost::connected_components(m_graph, &component[0]);

  std::vector<TimenodeGraphConnectedComponent> comps(num);
  for (auto vtx : m_vertices)
  {
    auto& comp = comps[component[vtx.second]];
    comp.syncs.push_back(vtx.first);
    for (auto& cst : Scenario::previousIntervals(*vtx.first, m_scenario))
    {
      comp.intervals.push_back(&m_scenario.interval(cst));
    }
    for (auto& cst : Scenario::nextIntervals(*vtx.first, m_scenario))
    {
      comp.intervals.push_back(&m_scenario.interval(cst));
    }
  }
  return {m_scenario, comps};
}
*/
void TimenodeGraph::intervalsChanged(const IntervalModel&)
{
  recompute();
}

void TimenodeGraph::timeSyncsChanged(const TimeSyncModel&)
{
  recompute();
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

  auto it = ossia::find_if(comps, [&](const auto& comp) {
    return ossia::contains(comp.syncs, rs) && ossia::contains(comp.syncs, &c);
  });
  return it != comps.end();
}

bool TimenodeGraphComponents::isInMain(const IntervalModel& c) const
{
  auto rs = &scenario.startTimeSync();

  auto it = ossia::find_if(comps, [&](const auto& comp) {
    return ossia::contains(comp.syncs, rs)
           && ossia::contains(comp.intervals, &c);
  });
  return it != comps.end();
}

const TimenodeGraphConnectedComponent&
TimenodeGraphComponents::component(const Scenario::TimeSyncModel& c) const
{
  auto it = ossia::find_if(comps, [&](const auto& comp) {
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
