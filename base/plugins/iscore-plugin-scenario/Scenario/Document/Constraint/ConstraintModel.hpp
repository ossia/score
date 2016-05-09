#pragma once
#include <iscore/tools/Metadata.hpp>
#include <Process/ModelMetadata.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/ModelConsistency.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/selection/Selectable.hpp>
#include <QObject>
#include <nano_signal_slot.hpp>
#include <Scenario/Document/Constraint/ExecutionState.hpp>

#include <QString>
#include <QVector>

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/component/Component.hpp>
class DataStream;
class JSONObject;

namespace Scenario
{
class StateModel;
class ConstraintViewModel;
class FullViewConstraintViewModel;

class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintModel final :
        public IdentifiedObject<ConstraintModel>,
        public Nano::Observer
{
        Q_OBJECT

        ISCORE_SERIALIZE_FRIENDS(Scenario::ConstraintModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Scenario::ConstraintModel, JSONObject)

        // TODO must go in view model
        Q_PROPERTY(double heightPercentage
                   READ heightPercentage
                   WRITE setHeightPercentage
                   NOTIFY heightPercentageChanged)

    public:
        /** Properties of the class **/
        iscore::Components components;
        Selectable selection;
        ModelMetadata metadata;
        ModelConsistency consistency;
        ConstraintDurations duration{*this};

        iscore::ElementPluginModelList pluginModelList;

        /** The class **/
        ConstraintModel(const Id<ConstraintModel>&,
                        const Id<ConstraintViewModel>& fullViewId,
                        double yPos,
                        QObject* parent);

        ~ConstraintModel();

        // Copy
        ConstraintModel(const ConstraintModel &source,
                        const Id<ConstraintModel>& id,
                        QObject* parent);

        // Serialization
        template<typename Deserializer>
        ConstraintModel(Deserializer&& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            initConnections();
            vis.writeTo(*this);
        }

        // Factories for the view models.
        template<typename ViewModelType> // Arg might be an id or a datastream [
        ViewModelType* makeConstraintViewModel(
                const Id<ConstraintViewModel>& id,
                QObject* parent)
        {
            auto viewmodel = new ViewModelType {id, *this, parent};
            setupConstraintViewModel(viewmodel);
            return viewmodel;
        }

        // Note : the Constraint does not have ownership (it's generally the Slot)
        void setupConstraintViewModel(ConstraintViewModel* viewmodel);

        const Id<StateModel>& startState() const;
        void setStartState(const Id<StateModel>& eventId);

        const Id<StateModel>& endState() const;
        void setEndState(const Id<StateModel> &endState);

        // Here we won't remove / add things from the outside so it is safe to
        // return a reference
        const QVector<ConstraintViewModel*>& viewModels() const
        { return m_constraintViewModels; }

        const TimeValue& startDate() const;
        void setStartDate(const TimeValue& start);
        void translate(const TimeValue& deltaTime);

        double heightPercentage() const;


        FullViewConstraintViewModel* fullView() const
        {
            return m_fullViewModel;
        }

        void setFullView(FullViewConstraintViewModel* fv);



        void startExecution();
        void stopExecution();
        // Resets the execution display recursively
        void reset();

        NotifyingMap<RackModel> racks;
        NotifyingMap<Process::ProcessModel> processes;

        bool looping() const;
        void setLooping(bool looping);

        void setHeightPercentage(double arg);
        void setExecutionState(ConstraintExecutionState);
        ConstraintExecutionState executionState() const
        { return m_executionState; }
    signals:
        void viewModelCreated(const ConstraintViewModel&);
        void viewModelRemoved(const QObject*);

        void heightPercentageChanged(double);

        void startDateChanged(const TimeValue&);

        void focusChanged(bool);
        void loopingChanged(bool);
        void executionStateChanged(Scenario::ConstraintExecutionState);
        void executionStarted();
        void executionStopped();

    private:
        void on_destroyedViewModel(ConstraintViewModel* obj);
        void initConnections();
        void on_rackAdded(const RackModel& rack);

        // The small view constraint view models that show this constraint
        // The constraint does not have ownership of these: their parent (in the Qt sense) are
        // the scenario view models
        QVector<ConstraintViewModel*> m_constraintViewModels;

        // Model for the full view.
        // Note : it is also present in m_constraintViewModels.
        FullViewConstraintViewModel* m_fullViewModel {};

        Id<StateModel> m_startState;
        Id<StateModel> m_endState;

        TimeValue m_startDate; // origin

        double m_heightPercentage {0.5};

        bool m_looping{false};
        ConstraintExecutionState m_executionState{};
};
}

DEFAULT_MODEL_METADATA(Scenario::ConstraintModel, "Constraint")
