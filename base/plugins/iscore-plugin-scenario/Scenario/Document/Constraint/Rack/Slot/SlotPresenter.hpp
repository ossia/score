#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Process/ZoomHelper.hpp>

class SlotModel;
class SlotView;
namespace iscore
{
    class SerializableCommand;
}
class LayerPresenter;
class Process;
class LayerModel;
class LayerView;
class RackView;

struct SlotProcessData
{
        using ProcessPair = std::pair<LayerPresenter*, LayerView*>;

        SlotProcessData() = default;
        SlotProcessData(const SlotProcessData&) = default;
        SlotProcessData(SlotProcessData&&) = default;
        SlotProcessData& operator=(const SlotProcessData&) = default;
        SlotProcessData& operator=(SlotProcessData&&) = default;
        SlotProcessData(const LayerModel* m, std::vector<ProcessPair>&& procs):
            model(m),
            processes(std::move(procs))
        {

        }

        const LayerModel* model{};
        std::vector<ProcessPair> processes;
};

class SlotPresenter final : public NamedObject
{
        Q_OBJECT

    public:
        SlotPresenter(const SlotModel& model,
                      RackView* view,
                      QObject* parent);
        virtual ~SlotPresenter();

        const Id<SlotModel>& id() const;
        const SlotModel& model() const;
        int height() const; // Return the height of the view

        void setWidth(double w);
        void setVerticalPosition(double h);

        // Sets the enabled - disabled graphism for
        // the slot move tool
        void enable();
        void disable();

        const auto& processes() const { return m_processes; }

        void on_heightChanged(double);
        void on_parentGeometryChanged();

        void on_zoomRatioChanged(ZoomRatio);
        void on_loopingChanged(bool);

    signals:
        void askUpdate();

        void pressed(const QPointF&) const;
        void moved(const QPointF&) const;
        void released(const QPointF&) const;

    private:
        // From Model
        void on_layerModelCreated(const LayerModel&);
        void on_layerModelDeleted(const LayerModel&);
        void on_layerModelPutToFront(const LayerModel&);

        void on_layerModelCreated_impl(const LayerModel&);

        void updateProcesses();
        void updateProcessShape(const SlotProcessData&);
        void updateProcessesShape();

        const SlotModel& m_model;
        SlotView* m_view{};
        std::vector<SlotProcessData> m_processes;

        // Maybe move this out of the state of the presenter ?
        int m_currentResizingValue {}; // Used when the m_slotView is being resized.

        ZoomRatio m_zoomRatio {};

        bool m_enabled{true};
        bool m_looping{false};
};

