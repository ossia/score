#pragma once
#include <QObject>
#include <memory>
#include <set>
#include <exception>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

namespace iscore
{

struct PluginDependencyNode
{
        bool mark = false;

        QObject* plug{};
        PluginDependencyNode(QObject* obj): plug{obj} {}

        std::set<
            std::weak_ptr<PluginDependencyNode>,
            std::owner_less<std::weak_ptr<PluginDependencyNode>>
        > found_dependencies;

        PluginRequirementslInterface_QtInterface*
        requirements() const
        { return qobject_cast<PluginRequirementslInterface_QtInterface*> (plug); }
        PluginControlInterface_QtInterface*
        control() const
        { return qobject_cast<PluginControlInterface_QtInterface*> (plug); }

        bool checkDependencies()
        {
            auto reqs = requirements();
            if(!reqs)
                return true;
            // For some reason there is a crash if
            // doing all_of on an empty QStringList.
            auto required = reqs->required();

            return std::all_of(
                        required.begin(),
                        required.end(),
                        [&] (QString req) { return checkDependency(req); });
        }

        bool checkDependency(QString req)
        {
            auto found_it = std::find_if(
                        found_dependencies.begin(),
                        found_dependencies.end(),
                        [&] (const auto& other_node) {

                auto lock = other_node.lock();
                auto other_reqs = lock->requirements();
                if(!other_reqs)
                    return false;

                auto offered = other_reqs->offered();
                return offered.contains(req);
            });

            return found_it != found_dependencies.end();
        }
};

struct PluginDependencyGraph
{
    private:
        using NodePtr = std::shared_ptr<PluginDependencyNode>;
        QList<NodePtr> nodes;

        QList<NodePtr> nodes_with_missing_deps;

        class CircularDependency : public std::logic_error
        {
            public:
                NodePtr source;
                // Source, target
                CircularDependency(NodePtr src):
                    std::logic_error{"Circular dependency"},
                    source{src}
                {

                }

        };

    public:
        void addNode(QObject* plug)
        {
            auto n = std::make_shared<PluginDependencyNode>(plug);
            if(n->requirements())
            {
                auto reqs = n->requirements()->required();
                if(!reqs.empty())
                {
                    for(const auto& req : reqs)
                    {
                        for(const auto& other : nodes)
                        {
                            auto other_reqs = other->requirements();
                            if(other_reqs && other_reqs->offered().contains(req))
                            {
                                n->found_dependencies.insert(other);
                            }
                        }
                    }
                }


                auto offers = n->requirements()->offered();
                if(!offers.empty())
                {
                    for(const auto& offer : offers)
                    {
                        for(const auto& other : nodes)
                        {
                            auto other_reqs = other->requirements();
                            if(other_reqs && other_reqs->required().contains(offer))
                            {
                                other->found_dependencies.insert(n);
                            }
                        }
                    }
                }
            }

            nodes.append(n);
        }

        template<typename Fun>
        /**
         * @brief visit Will visit the graph in dependency order,
         * without visiting a node twice.
         * @param f a function to apply. void(PluginControlInterface*).
         */
        void visit(Fun f)
        {
            // First get rid of the plug-ins that don't have all their dependencies
            for(auto node : nodes)
            {
                // Check that all the requirements are satisfied.
                if(!node->checkDependencies())
                {
                    nodes.removeOne(node);
                    nodes_with_missing_deps.append(node);
                }
            }

            for(const auto& node : nodes)
            {
                if(node->found_dependencies.empty())
                {
                    f(node->plug);
                    node->mark = true;
                }
                else
                {
                    try
                    {
                        for(const auto& dep : node->found_dependencies)
                        {
                            apply_parents_rec(f, dep.lock(), node);
                        }

                        f(node->plug);
                        node->mark = true;
                    }
                    catch(CircularDependency& e)
                    {
                        // Recursively remove all those
                        // depending on the element.
                        fix_circular_dep(e.source);
                        qDebug() << "A plug-in was removed due to a dependency loop.";
                        continue;
                    }
                }
            }
            qDebug() << nodes_with_missing_deps.size() << "plugins were not loaded.";

        }

        void fix_circular_dep(NodePtr e)
        {
            ISCORE_TODO;
        }

        template<typename Fun>
        void apply_parents_rec(Fun f, NodePtr ptr, NodePtr orig)
        {
            if(ptr->mark)
                return;

            for(auto weak_dep : ptr->found_dependencies)
            {
                auto dep = weak_dep.lock();
                if(dep == orig)
                    throw CircularDependency(orig);

                apply_parents_rec(f, dep, orig);
            }

            f(ptr->plug);
            ptr->mark = true;
        }
};


}
