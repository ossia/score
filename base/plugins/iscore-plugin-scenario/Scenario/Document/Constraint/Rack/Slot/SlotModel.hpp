#pragma once
#include <Process/LayerModel.hpp>
#include <Process/ModelMetadata.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <QtGlobal>
#include <QObject>
#include <nano_signal_slot.hpp>

#include <QString>
#include <functional>

#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class ConstraintModel;
class DataStream;
class JSONObject;
class Process;
class RackModel;

// Note : the SlotModel is assumed to be in a Rack, itself in a Constraint.
class SlotModel final : public IdentifiedObject<SlotModel>, public Nano::Observer
{
        Q_OBJECT
        ISCORE_METADATA("SlotModel")
        ISCORE_SERIALIZE_FRIENDS(SlotModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(SlotModel, JSONObject)

        Q_PROPERTY(qreal height
                   READ height
                   WRITE setHeight
                   NOTIFY heightChanged)

        Q_PROPERTY(bool focus
                   READ focus
                   WRITE setFocus
                   NOTIFY focusChanged)

    public:
        ModelMetadata metadata;
        SlotModel(const Id<SlotModel>& id,
                  RackModel* parent);

        // Copy
        SlotModel(std::function<void(const SlotModel&, SlotModel&)> lmCopyMethod,
                  const SlotModel& source,
                  const Id<SlotModel>& id,
                  RackModel* parent);

        RackModel& rack() const;

        static void copyViewModelsInSameConstraint(const SlotModel&, SlotModel&);
        static QString description()
        { return QObject::tr("Slot"); }

        template<typename Impl>
        SlotModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            initConnections();
            vis.writeTo(*this);
        }

        virtual ~SlotModel() = default;

         // A process is selected for edition when it is
         // the edited process when the interface is clicked.
        void putToFront(
                const Id<LayerModel>& layerId);
        const LayerModel* frontLayerModel() const;

        // A slot is always in a constraint
        ConstraintModel& parentConstraint() const;

        qreal height() const;
        bool focus() const;

        NotifyingMap<LayerModel> layers;

    signals:
        void layerModelPutToFront(const LayerModel& layerModelId);

        void heightChanged(qreal arg);
        void focusChanged(bool arg);

    public slots:
        void on_deleteSharedProcessModel(const Process& sharedProcessId);

        void setHeight(qreal arg);
        void setFocus(bool arg);

    private:
        void initConnections();

        void on_addLayer(const LayerModel& viewmodel);
        void on_removeLayer(const LayerModel&);

        Id<LayerModel> m_frontLayerModelId;

        qreal m_height {200};
        bool m_focus{false};
};

/**
 * @brief parentConstraint Utility function to get the parent constraint of a process view model
 * @param lm Process view model pointer
 *
 * @return A pointer to the parent constraint if there is one, or nullptr.
 */
ConstraintModel* parentConstraint(LayerModel* lm);
