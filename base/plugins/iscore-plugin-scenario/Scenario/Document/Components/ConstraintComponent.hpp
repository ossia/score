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
    public:
        struct ProcessPair {
                Process::ProcessModel& process;
                ProcessComponent_T& component;
        };

        Scenario::ConstraintModel& constraint;
        System_T& system;

        ConstraintComponentHierarchyManager(
                Component_T& component,
                Scenario::ConstraintModel& cst,
                System_T& doc,
                QObject* component_as_parent):
            constraint{cst},
            system{doc},
            m_component{component},
            m_componentFactory{doc.context().app.components.template factory<ProcessComponentFactoryList_T>()},
            m_parentObject{component_as_parent}
        {
            for(auto& process : constraint.processes)
            {
                add(process);
            }

            constraint.processes.mutable_added.connect<
                    ConstraintComponentHierarchyManager,
                    &ConstraintComponentHierarchyManager::add>(this);

            constraint.processes.removing.connect<
                    ConstraintComponentHierarchyManager,
                    &ConstraintComponentHierarchyManager::remove>(this);
        }

        const std::list<ProcessPair>& processes() const
        { return m_children; }

        void add(Process::ProcessModel& process)
        {
            // Will return a factory for the given process if available
            if(auto factory = m_componentFactory.factory(process, system))
            {
                // The subclass should provide this function to construct
                // the correct component relative to this process.
                auto proc_comp = m_component.make_processComponent(
                                     getStrongId(process.components),
                                     *factory, process, system, m_parentObject);
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

        void clear()
        {
            for(ProcessPair element : m_children)
            {
                remove(element);
            }
            m_children.clear();
        }

        ~ConstraintComponentHierarchyManager()
        {
            clear();
        }


    private:
        Component_T& m_component;
        const ProcessComponentFactoryList_T& m_componentFactory;

        QObject* m_parentObject{};

        std::list<ProcessPair> m_children; // todo map ? multi_index with both index of the component and of the process ?
};
