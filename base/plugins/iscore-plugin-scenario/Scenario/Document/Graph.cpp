#include "Graph.hpp"

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <iscore/model/ModelMetadata.hpp>

#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/range/iterator_range.hpp>

namespace Scenario
{

TimenodeGraph::TimenodeGraph(
    const Scenario::ScenarioInterface& scenar)
{
  for(auto& tn : scenar.getTimeNodes())
  {
    m_vertices[&tn] = boost::add_vertex(&tn, m_graph);
  }

  for(auto& cst : scenar.getConstraints())
  {
    m_edges[&cst] = boost::add_edge(
          m_vertices[&Scenario::startTimeNode(cst, scenar)],
          m_vertices[&Scenario::endTimeNode(cst, scenar)],
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
