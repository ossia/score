// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioExecution.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_sync.hpp>

#include <boost/graph/depth_first_search.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/ContextMenu/PlayFromIntervalInScenario.hpp>

namespace Execution
{

struct dfs_visitor_state
{
  tsl::hopscotch_set<Scenario::IntervalModel*> intervals;
  tsl::hopscotch_set<Scenario::TimeSyncModel*> nodes;
};

struct dfs_visitor : public boost::default_dfs_visitor
{
  // because these geniuses of boost decided to pass the visitor by value...
  std::shared_ptr<dfs_visitor_state> state{std::make_shared<dfs_visitor_state>()};

  void discover_vertex(Scenario::Graph::vertex_descriptor i, const Scenario::Graph& g)
  {
    state->nodes.insert(g[i]);
  }
  void examine_edge(Scenario::Graph::edge_descriptor i, const Scenario::Graph& g)
  {
    state->intervals.insert(g[i]);
  }
};

tsl::hopscotch_set<Scenario::IntervalModel*>
PlayFromIntervalScenarioPruner::intervalsToKeep() const
{
  if (auto sc = dynamic_cast<const Scenario::ProcessModel*>(&scenar))
  {
    Scenario::TimenodeGraph g{*sc};

    // First find the vertex matching the time sync after our interval
    auto vertex = g.vertices().at(&Scenario::endTimeSync(interval, scenar));

    // Do a depth-first search from where we're starting
    dfs_visitor vis;
    std::vector<boost::default_color_type> color_map(boost::num_vertices(g.graph()));

    boost::depth_first_visit(
        g.graph(),
        vertex,
        vis,
        boost::make_iterator_property_map(
            color_map.begin(), boost::get(boost::vertex_index, g.graph()), color_map[0]));

    // Add the first interval
    vis.state->intervals.insert(&interval);
    return vis.state->intervals;
  }
  else
  {
    auto itv = scenar.getIntervals();
    return {&(*itv.begin())};
  }
}

bool PlayFromIntervalScenarioPruner::toRemove(
    const tsl::hopscotch_set<Scenario::IntervalModel*>& toKeep,
    Scenario::IntervalModel& cst) const
{
  auto c_addr = &cst;
  return (toKeep.find(c_addr) == toKeep.end()) && (c_addr != &interval);
}

void PlayFromIntervalScenarioPruner::operator()(
    const Context& exec_ctx,
    const BaseScenarioElement& bs)
{
  auto process_ptr = dynamic_cast<const Process::ProcessModel*>(&scenar);
  if (!process_ptr)
    return;
  // We prune all the superfluous components of the scenario, ie the one that
  // aren't either the started interval, or the ones following it.

  // First build a vector with all the intervals that we want to keep.
  auto toKeep = intervalsToKeep();

  // Get the intervals in the scenario execution
  auto& source_procs = bs.baseInterval().processes();
  auto scenar_proc_it = source_procs.find(process_ptr->id());

  SCORE_ASSERT(scenar_proc_it != source_procs.end());

  auto scenar_comp = dynamic_cast<ScenarioComponent*>((*scenar_proc_it).second.get());
  const auto scenar_intervals = scenar_comp->intervals();
  IntervalComponent* other_cst{};
  for (auto elt : scenar_intervals)
  {
    auto& is = elt.second->scoreInterval();
    if (toRemove(toKeep, is))
    {
      scenar_comp->remove(is);
    }
    else if (&is == &interval)
    {
      other_cst = elt.second.get();
    }
  }

  SCORE_ASSERT(other_cst);

  // Get the time_interval element of the interval we're starting from,
  // unless it is already linked to the beginning.
  auto& start_e = *scenar_comp->OSSIAProcess().get_start_time_sync()->get_time_events()[0];
  auto& new_end_e = other_cst->OSSIAInterval()->get_start_event();
  if (&start_e != &new_end_e)
  {
    auto end_date = new_end_e.get_time_sync().get_date();
    auto new_cst = ossia::time_interval::create(
        ossia::time_interval::exec_callback{},
        *scenar_comp->OSSIAProcess().get_start_time_sync()->get_time_events()[0],
        new_end_e,
        end_date,
        end_date,
        end_date);

    scenar_comp->OSSIAProcess().add_time_interval(new_cst);
  }

  // Then we add a interval from the beginning of the scenario to this one,
  // and we do an offset.

  // TODO how to remove also the states ? for instance if there is a state on
  // the first timesync ?
}
}
