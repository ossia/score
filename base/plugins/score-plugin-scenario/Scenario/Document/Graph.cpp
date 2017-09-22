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
#include <Scenario/Process/ScenarioInterface.hpp>

#include <score/model/ModelMetadata.hpp>

namespace Scenario
{

TimenodeGraph::TimenodeGraph(
    const Scenario::ScenarioInterface& scenar)
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

}
