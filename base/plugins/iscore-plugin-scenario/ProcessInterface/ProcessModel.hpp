#pragma once
#include <ProcessInterface/TimeValue.hpp>
#include <ProcessInterface/State/ProcessStateDataInterface.hpp>
#include <ProcessInterface/ExpandMode.hpp>

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
class DataStream;
class JSONObject;

class LayerModel;
// TODO rename file ?
/**
 * @brief The ProcessModel class
 *
 * Interface to implement to make a process.
 */
class Process: public IdentifiedObject<Process>
{
        Q_OBJECT

        ISCORE_SERIALIZE_FRIENDS(Process, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Process, JSONObject)

    public:
        iscore::ElementPluginModelList* pluginModelList{}; // Note: has to be initialized by the sub-classes.

        using IdentifiedObject<Process>::IdentifiedObject;
        Process(
                const TimeValue& duration,
                const id_type<Process>& id,
                const QString& name,
                QObject* parent);

        template<typename Impl>
        Process(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject {vis, parent}
        {
            vis.writeTo(*this);
        }

        virtual Process* clone(
                const id_type<Process>& newId,
                QObject* newParent) const = 0;

        virtual QString processName() const = 0; // Needed for serialization.

        //// View models interface
        // For deterministic operation in a command,
        // we have to generate some data (like ids...) before making a new view model.
        // This data is valid for construction only for the current state
        // of the scenario.
        virtual QByteArray makeViewModelConstructionData() const;

        // TODO pass the name of the view model to be created
        // (e.g. temporal / logical...).
        LayerModel* makeLayer(
                const id_type<LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent);

        // Load
        LayerModel* loadLayer(
                const VisitorVariant& v,
                QObject* parent);

        // Clone
        LayerModel* cloneLayer(
                const id_type<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent);

        // For use where the view model is ephemeral (e.g. process panel)
        LayerModel* makeTemporaryLayer(
                const id_type<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent);
        // Do a copy.
        QVector<LayerModel*> layers() const;

        //// Features of a process
        /// Duration
        // Used to scale the process.
        // This should be commutative :
        //   setDurationWithScale(2); setDurationWithScale(3);
        // yields the same result as :
        //   setDurationWithScale(3); setDurationWithScale(2);
        virtual void setDurationAndScale(const TimeValue& newDuration) = 0;

        // Does nothing if newDuration < currentDuration
        virtual void setDurationAndGrow(const TimeValue& newDuration) = 0;

        // Does nothing if newDuration > currentDuration
        virtual void setDurationAndShrink(const TimeValue& newDuration) = 0;

        void expandProcess(ExpandMode mode, const TimeValue& t);

        // TODO might not be useful... put in protected ?
        // Constructor needs it, too.
        void setDuration(const TimeValue& other);

        const TimeValue& duration() const;

        /// Execution
        virtual void reset() = 0;

        /// States
        virtual DynamicStateDataInterface* startState() const = 0;
        virtual DynamicStateDataInterface* endState() const = 0;

        /// Selection
        virtual Selection selectableChildren() const = 0;
        virtual Selection selectedChildren() const = 0;
        virtual void setSelection(const Selection& s) const = 0;

        // protected:
        virtual void serialize(const VisitorVariant& vis) const = 0;

    protected:
        // Clone
        Process(
                const Process& other,
                const id_type<Process>& id,
                const QString& name,
                QObject* parent);

        virtual LayerModel* makeLayer_impl(
                const id_type<LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) = 0;
        virtual LayerModel* loadLayer_impl(
                const VisitorVariant&,
                QObject* parent) = 0;
        virtual LayerModel* cloneLayer_impl(
                const id_type<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent) = 0;


    private:
        void addLayer(LayerModel* m);
        void removeLayer(LayerModel* m);

        // Ownership : the parent is the Slot or another widget, not the process.
        // A process view is never displayed alone, it is always in a view, which is in a rack.
        QVector<LayerModel*> m_layers;
        TimeValue m_duration;
};

template<typename T>
QVector<typename T::layer_type*> layers(const T& processModel)
{
    QVector<typename T::layer_type*> v;

    for(auto& elt : processModel.layers())
    {
        v.push_back(static_cast<typename T::layer_type*>(elt));
    }

    return v;
}

Process* parentProcess(QObject* obj);
const Process* parentProcess(const QObject* obj);
