#pragma once
#include <source/Document/ModelMetadata.hpp>

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <ProcessInterface/TimeValue.hpp>

#include <iscore/selection/Selectable.hpp>

#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <QColor>
#include <vector>

namespace OSSIA
{
    class TimeBox;
}

class ProcessModel;
class AbstractConstraintViewModel;
class FullViewConstraintViewModel;

class BoxModel;
class EventModel;
class TimeBox;

/**
 * @brief The ConstraintModel class
 */
// TODO put some of this stuff in the corresponding view models.
class ConstraintModel : public IdentifiedObject<ConstraintModel>
{
        Q_OBJECT

        friend void Visitor<Reader<DataStream>>::readFrom<ConstraintModel> (const ConstraintModel& ev);
        friend void Visitor<Reader<JSONObject>>::readFrom<ConstraintModel> (const ConstraintModel& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<ConstraintModel> (ConstraintModel& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<ConstraintModel> (ConstraintModel& ev);

        // TODO must go in view model
        Q_PROPERTY(double heightPercentage
                   READ heightPercentage
                   WRITE setHeightPercentage
                   NOTIFY heightPercentageChanged)

        // These dates are relative to the beginning of the constraint.
        Q_PROPERTY(TimeValue minDuration
                   READ minDuration
                   WRITE setMinDuration
                   NOTIFY minDurationChanged)
        Q_PROPERTY(TimeValue maxDuration
                   READ maxDuration
                   WRITE setMaxDuration
                   NOTIFY maxDurationChanged)
        Q_PROPERTY(TimeValue playDuration
                   READ playDuration
                   WRITE setPlayDuration
                   NOTIFY playDurationChanged)

        Q_PROPERTY(bool isRigid
                   READ isRigid
                   WRITE setRigid
                   NOTIFY rigidityChanged)

    public:
        class Algorithms
        {
            public:
            static void setDurationInBounds(ConstraintModel& cstr, const TimeValue& time)
            {
                if(cstr.defaultDuration() != time)
                {
                    // Rigid
                    if(cstr.isRigid())
                    {
                        cstr.setMinDuration(time);
                        cstr.setMaxDuration(time);

                        cstr.setDefaultDuration(time);
                    }
                    else // The checking must be done elsewhere if(arg >= m_minDuration && arg <= m_maxDuration)
                    {
                        cstr.setDefaultDuration(time);
                    }
                }
            }

            static void changeAllDurations(ConstraintModel& cstr, const TimeValue& time)
            {
                if(cstr.defaultDuration() != time)
                {
                    cstr.setMinDuration(cstr.minDuration() + (time - cstr.defaultDuration()));
                    cstr.setMaxDuration(cstr.maxDuration() + (time - cstr.defaultDuration()));

                    cstr.setDefaultDuration(time);
                }
            }
        };


        /** Properties of the class **/
        Selectable selection;
        ModelMetadata metadata;

        static QString prettyName()
        { return QObject::tr("Constraint"); }


        /** The class **/
        ConstraintModel(const id_type<ConstraintModel>&,
                        const id_type<AbstractConstraintViewModel>& fullViewId,
                        double yPos,
                        QObject* parent);


        // Copy
        ConstraintModel(const ConstraintModel* source,
                        const id_type<ConstraintModel>& id,
                        QObject* parent);

        // Serialization
        template<typename Deserializer>
        ConstraintModel(Deserializer&& vis, QObject* parent) :
            IdentifiedObject<ConstraintModel> {vis, parent}
        {
            vis.writeTo(*this);
        }

        // Factories for the view models.
        template<typename ViewModelType> // Arg might be an id or a datastream [
        ViewModelType* makeConstraintViewModel(
                const id_type<AbstractConstraintViewModel>& id,
                QObject* parent)
        {
            auto viewmodel = new ViewModelType {id, *this, parent};
            setupConstraintViewModel(viewmodel);
            return viewmodel;
        }

        // Note : the Constraint does not have ownership (it's generally the Deck)
        void setupConstraintViewModel(AbstractConstraintViewModel* viewmodel);

        // Sub-element creation
        void addProcess(ProcessModel*);
        void removeProcess(const id_type<ProcessModel>& processId);

        void addBox(BoxModel*);
        void removeBox(const id_type<BoxModel>& boxId);

        const id_type<EventModel>& startEvent() const;
        const id_type<EventModel>& endEvent() const;
        void setStartEvent(const id_type<EventModel>& eventId);
        void setEndEvent(const id_type<EventModel>& eventId);


        BoxModel* box(const id_type<BoxModel>& id) const;
        ProcessModel* process(
                const id_type<ProcessModel>& processId) const;

        // Copies are done because there might be a loop
        // that might change the vector, and invalidate the
        // iterators, leading to a crash quite difficult to debug.
        std::vector<BoxModel*> boxes() const
        { return m_boxes; }

        std::vector<ProcessModel*> processes() const
        { return m_processes; }

        // Here we won't remove / add things from the outside so it is safe to
        // return a reference
        const QVector<AbstractConstraintViewModel*>& viewModels() const
        { return m_constraintViewModels; }

        const TimeValue& startDate() const;
        void setStartDate(const TimeValue& start);
        void translate(const TimeValue& deltaTime);

        double heightPercentage() const;

        const TimeValue& defaultDuration() const;
        const TimeValue& minDuration() const;
        const TimeValue& maxDuration() const;

        FullViewConstraintViewModel* fullView() const
        {
            return m_fullViewModel;
        }

        void setFullView(FullViewConstraintViewModel* fv);

        const TimeValue& playDuration() const
        {
            return m_playDuration;
        }

        bool isRigid() const
        {
            return m_rigidity;
        }

        iscore::ElementPluginModelList& pluginModelList()
        { return *m_pluginModelList; }
        const iscore::ElementPluginModelList& pluginModelList() const
        { return *m_pluginModelList; }

    signals:
        void processCreated(const QString& processName,
                            const id_type<ProcessModel>& processId);
        void processRemoved(const id_type<ProcessModel>& processId);

        void boxCreated(const id_type<BoxModel>& boxId);
        void boxRemoved(const id_type<BoxModel>& boxId);

        void viewModelCreated(const id_type<AbstractConstraintViewModel>&);
        void viewModelRemoved(const id_type<AbstractConstraintViewModel>&);

        void heightPercentageChanged(double arg);

        void defaultDurationChanged(const TimeValue& arg);
        void minDurationChanged(const TimeValue& arg);
        void maxDurationChanged(const TimeValue& arg);
        void startDateChanged(const TimeValue& arg);

        void playDurationChanged(const TimeValue& arg);

        void rigidityChanged(bool arg);

    public slots:
        void setHeightPercentage(double arg);

        void setDefaultDuration(const TimeValue& arg);
        void setMinDuration(const TimeValue& arg);
        void setMaxDuration(const TimeValue& arg);

        void setPlayDuration(const TimeValue& arg)
        {
            if (m_playDuration == arg)
                return;

            m_playDuration = arg;
            emit playDurationChanged(arg);
        }

        void setRigid(bool arg);

    private slots:
        void on_destroyedViewModel(QObject*);

    private:
        iscore::ElementPluginModelList* m_pluginModelList{};

        std::vector<BoxModel*> m_boxes; // No content -> Phantom ?
        std::vector<ProcessModel*> m_processes;

        // The small view constraint view models that show this constraint
        // The constraint does not have ownership of these: their parent (in the Qt sense) are
        // the scenario view models
        QVector<AbstractConstraintViewModel*> m_constraintViewModels;

        // Model for the full view. It's always a Temporal one (but it could be specialized, in order to provide the extensibility, maybe ?)
        // Note : it is also present in m_constraintViewModels.
        FullViewConstraintViewModel* m_fullViewModel {};

        id_type<EventModel> m_startEvent {};
        id_type<EventModel> m_endEvent {};

        TimeValue m_defaultDuration{std::chrono::milliseconds{200}};
        TimeValue m_minDuration{m_defaultDuration};
        TimeValue m_maxDuration{m_defaultDuration};

        TimeValue m_x {}; // origin

        double m_heightPercentage {0.5};
        TimeValue m_playDuration;
        bool m_rigidity{true};
};
