#pragma once
#include <QObject>
#include <exception>
#include <iscore/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <memory>
#include <set>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <iscore/plugins/Addon.hpp>
#include <iscore/tools/std/HashMap.hpp>
#include <chrono>
namespace iscore
{
namespace PluginLoader
{
/**
 * @brief Allows to load plug-ins in the order they all require each other.
 *
 * \todo generalize this to make it usable for all kinf of plug-ins,
 * and if possible DocumentPlugin.
 */
struct PluginDependencyGraph
{
  struct GraphVertex
  {
    GraphVertex(): addon{} { }
    GraphVertex(const iscore::Addon* add): addon{add} { }
    const iscore::Addon* addon{};
  };

  using Graph = boost::adjacency_list<
    boost::vecS,
    boost::vecS,
    boost::directedS,
    GraphVertex>;

public:
  PluginDependencyGraph(const std::vector<iscore::Addon>& addons)
  {
    if(addons.empty())
      return;

    iscore::hash_map<PluginKey, int32_t> keys;
    std::vector<const iscore::Addon*> not_loaded;

    // First add all the vertices to the graph
    for(const iscore::Addon& addon : addons)
    {
      keys[addon.key] = boost::add_vertex(GraphVertex{&addon}, m_graph);
    }

    // If A depends on B, then there is an edge from B to A.
    for(const iscore::Addon& addon : addons)
    {
      auto addon_k = keys[addon.key];
      for(const auto& k : addon.plugin->required())
      {
        auto it = keys.find(k);
        if(it != keys.end())
        {
          boost::add_edge(addon_k, it->second, m_graph);
        }
        else
        {
          boost::remove_vertex(addon_k, m_graph);
          not_loaded.push_back(&addon);
          break;
        }
      }
    }

    if(!not_loaded.empty())
      qDebug() << not_loaded.size() << "plugins were not loaded due to a dependency problem.";

    // Then do a topological sort, to detect cycles and to be able to iterate easily afterwards.
    std::vector<int> topo_order;
    topo_order.reserve(addons.size() - not_loaded.size());

    try {
        boost::topological_sort(m_graph, std::back_inserter(topo_order));
        for(auto e : topo_order)
          m_sorted.push_back(*m_graph[e].addon);
    }
    catch(const std::exception& e) {
      qDebug() << "Invalid plug-in graph: " << e.what();
      m_graph.clear();
    }
  }

  auto& sortedAddons() const
  { return m_sorted; }

private:
  Graph m_graph;
  std::vector<iscore::Addon> m_sorted;

};
}
}
