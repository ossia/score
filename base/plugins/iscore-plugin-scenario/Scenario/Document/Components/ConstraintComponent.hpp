#pragma once
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>

namespace Scenario
{

template<typename Component_T>
class ConstraintComponent :
        public Component_T
{
    public:
        template<typename... Args>
        ConstraintComponent(Scenario::ConstraintModel& cst, Args&&... args):
            Component_T{std::forward<Args>(args)...},
            m_constraint{cst}
        {

        }

        Scenario::ConstraintModel& constraint() const
        { return m_constraint; }

    private:
        Scenario::ConstraintModel& m_constraint;
};

template<typename System_T>
using GenericConstraintComponent =
    Scenario::ConstraintComponent<iscore::GenericComponent<System_T>>;
}

// TODO put it in namespace, too
template<
        typename Component_T,
        typename ProcessComponent_T,
        typename ProcessComponentFactoryList_T>
class ConstraintComponentHierarchyManager :
        public Component_T,
        public Nano::Observer
{
    public:
        using hierarchy_t = ConstraintComponentHierarchyManager;
        struct ProcessPair {
                Process::ProcessModel& process;
                ProcessComponent_T& component;
        };

        template<typename... Args>
        ConstraintComponentHierarchyManager(Args&&... args):
            Component_T{std::forward<Args>(args)...},
            m_componentFactory{Component_T::system().context().app.components.template factory<ProcessComponentFactoryList_T>()}
        {
            NotifyingMap<Process::ProcessModel>& processes = Component_T::constraint().processes;
            for(auto& process : processes)
            {
                add(process);
            }

            processes.mutable_added.connect<
                    ConstraintComponentHierarchyManager,
                    &ConstraintComponentHierarchyManager::add>(this);

            processes.removing.connect<
                    ConstraintComponentHierarchyManager,
                    &ConstraintComponentHierarchyManager::remove>(this);
        }

        auto& processes() const
        { return m_children; }

        void add(Process::ProcessModel& process)
        {
            // Will return a factory for the given process if available
            if(auto factory = m_componentFactory.factory(process))
            {
                // The subclass should provide this function to construct
                // the correct component relative to this process.
                auto proc_comp = Component_T::make_processComponent(
                            getStrongId(process.components), *factory, process);
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
            Component_T::removing(pair.process, pair.component);
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
        const ProcessComponentFactoryList_T& m_componentFactory;

        std::list<ProcessPair> m_children; // todo map ? multi_index with both index of the component and of the process ?
};
