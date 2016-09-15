#pragma once
#include <Process/ExpandMode.hpp>

#include <Process/TimeValue.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/model/Entity.hpp>
#include <QByteArray>
#include <QString>
#include <vector>

#include <iscore/component/Component.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/plugins/customfactory/SerializableInterface.hpp>
#include <iscore_lib_process_export.h>

namespace Process { class LayerModel; }
class ProcessStateDataInterface;

namespace Process
{
class ProcessModelFactory; class LayerFactory;
class ProcessModel;
class LayerFactory;

/**
 * @brief The Process class
 *
 * Interface to implement to make a process.
 */
class ISCORE_LIB_PROCESS_EXPORT ProcessModel:
        public iscore::Entity<ProcessModel>,
        public iscore::SerializableInterface<ProcessModelFactory>
{
        Q_OBJECT

        ISCORE_SERIALIZE_FRIENDS(Process::ProcessModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Process::ProcessModel, JSONObject)
        friend class Process::LayerFactory; // to register layers

    public:
        ProcessModel(
                TimeValue duration,
                const Id<ProcessModel>& id,
                const QString& name,
                QObject* parent);

        ProcessModel(Deserializer<DataStream>& vis, QObject* parent);
        ProcessModel(Deserializer<JSONObject>& vis, QObject* parent);

        virtual ~ProcessModel();

        virtual ProcessModel* clone(
                const Id<Process::ProcessModel>& newId,
                QObject* newParent) const = 0;

        // A user-friendly text to show to the users
        virtual QString prettyName() const
        { return metadata().getName(); }


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
        virtual void startExecution() { }
        virtual void stopExecution() { }
        virtual void reset() { }

        /// States. The process has ownership.
        virtual ProcessStateDataInterface* startStateData() const { return nullptr; }
        virtual ProcessStateDataInterface* endStateData() const { return nullptr; }

        /// Selection
        virtual Selection selectableChildren() const { return {}; }
        virtual Selection selectedChildren() const { return {}; }
        virtual void setSelection(const Selection& s) const { }

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

        // Used to scale the process.
        // This should be commutative :
        //   setDurationWithScale(2); setDurationWithScale(3);
        // yields the same result as :
        //   setDurationWithScale(3); setDurationWithScale(2);
        virtual void setDurationAndScale(const TimeValue& newDuration)
        { setDuration(newDuration); }

        // Does nothing if newDuration < currentDuration
        virtual void setDurationAndGrow(const TimeValue& newDuration)
        { setDuration(newDuration); }

        // Does nothing if newDuration > currentDuration
        virtual void setDurationAndShrink(const TimeValue& newDuration)
        { setDuration(newDuration); }

    private:
        void addLayer(LayerModel* m);
        void removeLayer(LayerModel* m);

        // Ownership : the parent is the Slot or another widget, not the process.
        // A process view is never displayed alone, it is always in a view, which is in a rack.
        std::vector<LayerModel*> m_layers;
        TimeValue m_duration;
        bool m_useParentDuration{true};
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

Q_DECLARE_METATYPE(Id<Process::ProcessModel>)
