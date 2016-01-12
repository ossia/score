#pragma once
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>

template<
        typename Component_T,
        typename System_T,
        typename ProcessComponent_T,
        typename ProcessComponentFactoryList_T>
class ConstraintComponentHierarchyManager : public Nano::Observer
{
    private:
        struct ProcessPair {
                Process::ProcessModel& process;
                ProcessComponent_T& component;
        };

    public:
        ConstraintComponentHierarchyManager(
                Component_T& component,
                Scenario::ConstraintModel& cst,
                const System_T& doc,
                const iscore::DocumentContext& ctx,
                QObject* component_as_parent):
            m_constraint{cst},
            m_component{component},
            m_componentFactory{ctx.app.components.factory<ProcessComponentFactoryList_T>()},
            m_system{doc},
            m_context{ctx},
            m_parentObject{component_as_parent}
        {
            for(auto& process : m_constraint.processes)
            {
                add(process);
            }

            m_constraint.processes.mutable_added.connect<
                    ConstraintComponentHierarchyManager,
                    &ConstraintComponentHierarchyManager::add>(this);

            m_constraint.processes.removing.connect<
                    ConstraintComponentHierarchyManager,
                    &ConstraintComponentHierarchyManager::remove>(this);
        }

        void add(Process::ProcessModel& process)
        {
            // Will return a factory for the given process if available
            if(auto factory = m_componentFactory.factory(process, m_system, m_context))
            {
                // The subclass should provide this function to construct
                // the correct component relative to this process.
                auto proc_comp = m_component.make_processComponent(
                                     getStrongId(process.components),
                                     *factory, process, m_system, m_context, m_parentObject);
                if(proc_comp)
                {
                    process.components.add(proc_comp);
                    m_children.emplace_back(ProcessPair{process, *proc_comp});
                }
            }
        }

        void remove(const Process::ProcessModel& process)
        {
            auto it = find_if(m_children, [&] (auto pair) {
                return &pair.process == &process;
            });

            if(it != m_children.end())
            {
                remove(*it);
                m_children.erase(it);
            }
        }

        void remove(const ProcessPair& pair)
        {
            m_component.removing(pair.process, pair.component);
            pair.process.components.remove(pair.component);
        }

        ~ConstraintComponentHierarchyManager()
        {
            for(ProcessPair element : m_children)
            {
                remove(element);
            }
        }

    private:
        Scenario::ConstraintModel& m_constraint;
        Component_T& m_component;
        const ProcessComponentFactoryList_T& m_componentFactory;
        const System_T& m_system;
        const iscore::DocumentContext& m_context;

        QObject* m_parentObject{};

        std::list<ProcessPair> m_children; // todo map ? multi_index with both index of the component and of the process ?
};
