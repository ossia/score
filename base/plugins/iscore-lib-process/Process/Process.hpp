#pragma once
#include <Process/ExpandMode.hpp>
#include <Process/ProcessFactoryKey.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <QByteArray>
#include <QString>
#include <vector>

#include <iscore/component/Component.hpp>
#include <iscore/tools/Metadata.hpp>
#include "ModelMetadata.hpp"
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/plugins/customfactory/SerializableInterface.hpp>

namespace Process { class LayerModel; }
class ProcessStateDataInterface;
class QObject;

namespace iscore {
class ElementPluginModelList;
}  // namespace iscore
#include <iscore/tools/SettableIdentifier.hpp>

namespace Process
{
/**
 * @brief The Process class
 *
 * Interface to implement to make a process.
 */
class ISCORE_LIB_PROCESS_EXPORT ProcessModel:
        public IdentifiedObject<ProcessModel>,
        public iscore::SerializableInterface<ProcessModel>
{
        Q_OBJECT

        ISCORE_SERIALIZE_FRIENDS(Process::ProcessModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Process::ProcessModel, JSONObject)

    public:
        iscore::Components components;
        iscore::ElementPluginModelList* pluginModelList{}; // Note: has to be initialized by the sub-classes.
        ModelMetadata metadata;

        using IdentifiedObject<ProcessModel>::IdentifiedObject;
        ProcessModel(
                const TimeValue& duration,
                const Id<ProcessModel>& id,
                const QString& name,
                QObject* parent);

        template<typename Impl>
        ProcessModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject {vis, parent}
        {
            vis.writeTo(*this);
        }

        virtual ~ProcessModel();

        virtual ProcessModel* clone(
                const Id<ProcessModel>& newId,
                QObject* newParent) const = 0;

        // A user-friendly text to show to the users
        virtual QString prettyName() const = 0;

        //// View models interface
        // For deterministic operation in a command,
        // we have to generate some data (like ids...) before making a new view model.
        // This data is valid for construction only for the current state
        // of the scenario.
        virtual QByteArray makeLayerConstructionData() const;

        // TODO pass the name of the view model to be created
        // (e.g. temporal / logical...).
        LayerModel* makeLayer(
                const Id<LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent);

        // Load
        LayerModel* loadLayer(
                const VisitorVariant& v,
                QObject* parent);

        // Clone
        LayerModel* cloneLayer(
                const Id<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent);

        // For use where the view model is ephemeral (e.g. process panel)
        LayerModel* makeTemporaryLayer(
                const Id<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent);
        // Do a copy.
        std::vector<LayerModel*> layers() const;

        //// Features of a process
        /// Duration
        void setParentDuration(ExpandMode mode, const TimeValue& t);

        void setUseParentDuration(bool b)
        {
            if(m_useParentDuration != b)
            {
                m_useParentDuration = b;
                emit useParentDurationChanged(b);
            }
        }

        bool useParentDuration() const
        {
            return m_useParentDuration;
        }

        // TODO might not be useful... put in protected ?
        // Constructor needs it, too.
        void setDuration(const TimeValue& other);

        const TimeValue& duration() const;

        /// Execution
        virtual void startExecution() = 0;
        virtual void stopExecution() = 0;
        virtual void reset() = 0;

        /// States. The process has ownership.
        virtual ProcessStateDataInterface* startStateData() const = 0;
        virtual ProcessStateDataInterface* endStateData() const = 0;

        /// Selection
        virtual Selection selectableChildren() const = 0;
        virtual Selection selectedChildren() const = 0;
        virtual void setSelection(const Selection& s) const = 0;

    signals:
        // True if the execution is running.
        void execution(bool);
        void durationChanged(const TimeValue&);
        void useParentDurationChanged(bool);

    protected:
        // Clone
        ProcessModel(
                const ProcessModel& other,
                const Id<ProcessModel>& id,
                const QString& name,
                QObject* parent);

        virtual LayerModel* makeLayer_impl(
                const Id<LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) = 0;
        virtual LayerModel* loadLayer_impl(
                const VisitorVariant&,
                QObject* parent) = 0;
        virtual LayerModel* cloneLayer_impl(
                const Id<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent) = 0;

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

    private:
        void addLayer(LayerModel* m);
        void removeLayer(LayerModel* m);

        // Ownership : the parent is the Slot or another widget, not the process.
        // A process view is never displayed alone, it is always in a view, which is in a rack.
        std::vector<LayerModel*> m_layers;
        TimeValue m_duration;
        bool m_useParentDuration{};
};

ISCORE_LIB_PROCESS_EXPORT ProcessModel* parentProcess(QObject* obj);
ISCORE_LIB_PROCESS_EXPORT const ProcessModel* parentProcess(const QObject* obj);

}
template<typename T>
std::vector<typename T::layer_type*> layers(const T& processModel)
{
    std::vector<typename T::layer_type*> v;

    for(auto& elt : processModel.layers())
    {
        v.push_back(safe_cast<typename T::layer_type*>(elt));
    }

    return v;
}

DEFAULT_MODEL_METADATA(Process::ProcessModel, "Process")
