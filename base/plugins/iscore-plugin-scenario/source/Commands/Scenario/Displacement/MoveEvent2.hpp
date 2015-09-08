#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <ProcessInterface/ExpandMode.hpp>
#include "Tools/dataStructures.hpp"
#include "Process/ScenarioModel.hpp"
#include "Process/Algorithms/StandardDisplacementPolicy.hpp"
#include "Process/Algorithms/VerticalMovePolicy.hpp"

class EventModel;
class TimeNodeModel;
class ConstraintModel;
class ConstraintViewModel;
class RackModel;
class ScenarioModel;
struct ElementsProperties;

#include <tests/helpers/ForwardDeclaration.hpp>


namespace Scenario
{
    namespace Command
    {
        template<class DisplacementPolicy>
        class MoveEvent2 : public iscore::SerializableCommand
        {
                // No ISCORE_COMMAND here since it's a template.

#include <tests/helpers/FriendDeclaration.hpp>
            public:
                static const char * commandName()
                {
                    static QByteArray name = QString{"MoveEvent2_%1"}.arg(DisplacementPolicy::name()).toLatin1();
                    return name.constData();
                }
                static QString description()
                {
                    return QObject::tr("Move Event 2 With %1").arg(DisplacementPolicy::name());
                }
                static auto static_uid()
                {
                    using namespace std;
                    hash<string> fn;
                    return fn(std::string(commandName()));
                }
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(MoveEvent2, "ScenarioControl")
                /**
                 * @brief MoveEvent2
                 * @param scenarioPath
                 * @param eventId
                 * @param newDate
                 * !!! in the future it would be better to give directly the delta time of the mouse displacement  !!!
                 * @param mode
                 */
                MoveEvent2(
                    Path<ScenarioModel>&& scenarioPath,
                    const Id<EventModel>& eventId,
                    const TimeValue& newDate,
                    ExpandMode mode)
                :
                SerializableCommand {"ScenarioControl", commandName(), description()},
                m_path {std::move(scenarioPath)},
                m_mode{mode}
                {
                    m_initialDate = newDate;
                    m_oldDate = m_initialDate;

                    update(
                            m_path,
                            eventId,
                            newDate,
                            m_mode);

                }
        void
        update(
                const Path<ScenarioModel>& scenarioPath,
                const Id<EventModel>& eventId,
                const TimeValue& newDate,
                ExpandMode mode)
        {
            TimeValue deltaDate = newDate - m_oldDate;
            m_oldDate = newDate;

            auto& scenario = m_path.find();

            //NOTICE: multiple event displacement functionnality already available this is "retro" compatibility
            QVector <Id<TimeNodeModel>> draggedElements;
            draggedElements.push_back(scenario.event(eventId).timeNode());// retrieve corresponding timenode and store it in array

            DisplacementPolicy::computeDisplacement(scenario, draggedElements, deltaDate, m_savedElementsProperties);

        }
                virtual
                void
                undo() override
                {
                    m_oldDate = m_initialDate;

                    bool useNewValues{false};
                    DisplacementPolicy::updatePositions(
                                m_path.find(),
                                [&] (Process& p, const TimeValue& t){ p.expandProcess(m_mode, t); },
                                m_savedElementsProperties,
                                useNewValues);
                    /*
                    auto& scenar = m_path.find();
                    auto& event = scenar.event(m_eventId);

                    TimeValue deltaDate{};
                    deltaDate = m_oldDate - event.date();

                    StandardDisplacementPolicy::updatePositions(
                                scenar,
                                m_movableTimenodes,
                                m_oldDate - event.date(),
                                [&] (Process& , const TimeValue& ) { });

                    // Now we have to restore the state of each constraint that might have been modified
                    // during this command.
                    for(auto& obj : m_savedConstraints)
                    {
                        // 1. Clear the constraint
                        // TODO Don't use a command since it serializes a ton of unused stuff.
                        ClearConstraint clear_cmd{Path<ConstraintModel>{obj.first.first}};
                        clear_cmd.redo();

                        auto& constraint = obj.first.first.find();
                        // 2. Restore the rackes & processes.

                        // TODO if possible refactor this with ReplaceConstraintContent and ConstraintModel::clone
                        // Be careful however, the code differs in subtle ways
                        {
                            ConstraintModel src_constraint{
                                    Deserializer<DataStream>{obj.first.second},
                                    &constraint}; // Temporary parent

                            std::map<const Process*, Process*> processPairs;

                            // Clone the processes
                            for(const auto& sourceproc : src_constraint.processes)
                            {
                                auto newproc = sourceproc.clone(sourceproc.id(), &constraint);

                                processPairs.insert(std::make_pair(&sourceproc, newproc));
                                constraint.processes.add(newproc);
                            }

                            // Clone the rackes
                            for(const auto& sourcerack : src_constraint.racks)
                            {
                                // A note about what happens here :
                                // Since we want to duplicate our process view models using
                                // the target constraint's cloned shared processes (they might setup some specific data),
                                // we maintain a pair mapping each original process to their cloned counterpart.
                                // We can then use the correct cloned process to clone the process view model.
                                auto newrack = new RackModel{
                                        sourcerack,
                                        sourcerack.id(),
                                        [&] (const SlotModel& source, SlotModel& target)
                                        {
                                            for(const auto& lm : source.layers)
                                            {
                                                // We can safely reuse the same id since it's in a different slot.
                                                Process* proc = processPairs[&lm.processModel()];
                                                // TODO harmonize the order of parameters (source first, then new id)
                                                target.layers.add(proc->cloneLayer(lm.id(), lm, &target));
                                            }
                                        },
                                        &constraint};
                                constraint.racks.add(newrack);
                            }
                        }

                        // 3. Restore the correct rackes in the constraint view models
                        for(auto& viewmodel : constraint.viewModels())
                        {
                            viewmodel->showRack(obj.second[viewmodel->id()]);
                        }
                    }
                    */
                    //updateEventExtent(m_eventId, scenar);

                }
                virtual
                void
                redo() override
                {


                    bool useNewValues{true};
                    DisplacementPolicy::updatePositions(
                                m_path.find(),
                                [&] (Process& p, const TimeValue& t){ p.expandProcess(m_mode, t); },
                                m_savedElementsProperties,
                                useNewValues);
                    /*
                    auto& scenar = m_path.find();
                    auto& event = scenar.event(m_eventId);

                    TimeValue deltaDate{};
                    deltaDate = m_newDate - event.date();

                    StandardDisplacementPolicy::updatePositions(
                                scenar,
                                m_movableTimenodes,
                                deltaDate,
                                [&] (Process& p, const TimeValue& t)
                    { p.expandProcess(m_mode, t); });
                    */
                    //updateEventExtent(m_eventId, scenar);
                }



                const Path<ScenarioModel>& path() const
                { return m_path; }

            protected:

                virtual
                void
                serializeImpl(QDataStream& s) const override
                {
                    s << m_path
                      << m_savedElementsProperties
                      << (int)m_mode
                      << m_savedConstraints;
                }

                virtual
                void
                deserializeImpl(QDataStream& s) override
                {
                    int mode;
                    s >> m_path
                      >> m_savedElementsProperties
                      >> mode
                      >> m_savedConstraints;

//                    m_mode = static_cast<ExpandMode>(mode);
                }

                private:
            private:
                DisplacementPolicy m_displacementPolicy;
                ElementsProperties m_savedElementsProperties;
                Path<ScenarioModel> m_path;

                TimeValue m_oldDate; //used to compute the deltaTime
                TimeValue m_initialDate; //used to compute the deltaTime and respect undo behavior

                ExpandMode m_mode{ExpandMode::Scale};

                QVector<
                    QPair<
                        QPair<
                            Path<ConstraintModel>,
                            QByteArray
                        >, // The constraint data
                        QMap< // Mapping for the view models of this constraint
                            Id<ConstraintViewModel>,
                            Id<RackModel>
                        >
                     >
                > m_savedConstraints;
        };

    }
}
