#pragma once
#include <QObject>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <chrono>
#include <exception>
#include <memory>
#include <ossia/detail/pod_vector.hpp>
#include <score/plugins/Addon.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <score/tools/std/HashMap.hpp>
#include <set>
namespace score
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
    GraphVertex() : addon{}
    {
    }
    explicit GraphVertex(const score::Addon* add) : addon{add}
    {
    }
    const score::Addon* addon{};
  };

  using Graph = boost::
      adjacency_list<boost::vecS, boost::vecS, boost::directedS, GraphVertex>;

public:
  explicit PluginDependencyGraph(const std::vector<score::Addon>& addons)
  {
    if (addons.empty())
      return;

    score::hash_map<PluginKey, int64_t> keys;
    std::vector<const score::Addon*> not_loaded;

    // First add all the vertices to the graph
    for (const score::Addon& addon : addons)
    {
      auto vx = boost::add_vertex(GraphVertex{&addon}, m_graph);
      keys[addon.key] = vx;
    }

    // If A depends on B, then there is an edge from B to A.
    for (const score::Addon& addon : addons)
    {
      auto addon_k = keys[addon.key];
      for (const auto& k : addon.plugin->required())
      {
        auto it = keys.find(k);
        if (it != keys.end())
        {
          boost::add_edge(addon_k, it->second, m_graph);
        }
        else
        {
          boost::clear_vertex(addon_k, m_graph);
          boost::remove_vertex(addon_k, m_graph);
          not_loaded.push_back(&addon);
          break;
        }
      }
    }

    if (!not_loaded.empty())
      qDebug() << not_loaded.size()
               << "plugins were not loaded due to a dependency problem.";

    // Then do a topological sort, to detect cycles and to be able to iterate
    // easily afterwards.
    ossia::int_vector topo_order;
    topo_order.reserve(addons.size() - not_loaded.size());

    try
    {
      boost::topological_sort(m_graph, std::back_inserter(topo_order));
      for (auto e : topo_order)
        m_sorted.push_back(*m_graph[e].addon);
    }
    catch (const std::exception& e)
    {
      qDebug() << "Invalid plug-in graph: " << e.what();
      m_graph.clear();
    }
  }

  auto& sortedAddons() const
  {
    return m_sorted;
  }

private:
  Graph m_graph;
  std::vector<score::Addon> m_sorted;
};
}
}
