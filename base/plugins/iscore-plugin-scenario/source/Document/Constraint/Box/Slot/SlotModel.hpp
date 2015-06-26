#pragma once
#include <ProcessInterface/LayerModel.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include <vector>

class BoxModel;
class ConstraintModel;

class ProcessModel;
class LayerModel;

// Note : the SlotModel is assumed to be in a Box, itself in a Constraint.
class SlotModel : public IdentifiedObject<SlotModel>
{
        Q_OBJECT

        Q_PROPERTY(int height
                   READ height
                   WRITE setHeight
                   NOTIFY heightChanged)

        Q_PROPERTY(bool focus
                   READ focus
                   WRITE setFocus
                   NOTIFY focusChanged)

    public:
        SlotModel(const id_type<SlotModel>& id,
                  BoxModel* parent);

        // Copy
        SlotModel(std::function<void(const SlotModel&, SlotModel&)> lmCopyMethod,
                  const SlotModel& source,
                  const id_type<SlotModel>& id,
                  BoxModel* parent);

        static void copyViewModelsInSameConstraint(const SlotModel&, SlotModel&);

        template<typename Impl>
        SlotModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject<SlotModel> {vis, parent}
        {
            vis.writeTo(*this);
        }

        virtual ~SlotModel() = default;

        void addLayerModel(
                LayerModel*);
        void deleteLayerModel(
                const id_type<LayerModel>& layerModelId);


         // A process is selected for edition when it is
         // the edited process when the interface is clicked.
        void putToFront(
                const id_type<LayerModel>& layerId);
        const id_type<LayerModel>& frontLayerModel() const;

        const auto& layerModels() const
        { return m_layerModels; }

        LayerModel& layerModel(
                const id_type<LayerModel>& layerModelId) const;

        // A slot is always in a constraint
        ConstraintModel& parentConstraint() const;

        int height() const;
        bool focus() const;

    signals:
        void layerModelCreated(const id_type<LayerModel>& layerModelId);
        void layerModelRemoved(const id_type<LayerModel>& layerModelId);
        void layerModelPutToFront(const id_type<LayerModel>& layerModelId);

        void heightChanged(int arg);
        void focusChanged(bool arg);

    public slots:
        void on_deleteSharedProcessModel(const id_type<ProcessModel>& sharedProcessId);

        void setHeight(int arg);
        void setFocus(bool arg);

    private:
        id_type<LayerModel> m_frontLayerModelId;
        IdContainer<LayerModel> m_layerModels;

        int m_height {200};
        bool m_focus{false};
};

/**
 * @brief parentConstraint Utility function to get the parent constraint of a process view model
 * @param lm Process view model pointer
 *
 * @return A pointer to the parent constraint if there is one, or nullptr.
 */
ConstraintModel* parentConstraint(LayerModel* lm);
