#pragma once
#include <Process/ZoomHelper.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <QPoint>
#include <utility>
#include <vector>

#include <nano_signal_slot.hpp>
#include <iscore/document/DocumentContext.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Process
{
class ProcessList;
class LayerView;
class LayerModel;
class LayerPresenter;
}

namespace Scenario
{
class RackView;
class SlotModel;
class SlotView;
struct SlotProcessData
{
        using ProcessPair = std::pair<Process::LayerPresenter*, Process::LayerView*>;

        SlotProcessData() = default;
        SlotProcessData(const SlotProcessData&) = default;
        SlotProcessData(SlotProcessData&&) = default;
        SlotProcessData& operator=(const SlotProcessData&) = default;
        SlotProcessData& operator=(SlotProcessData&&) = default;
        SlotProcessData(const Process::LayerModel* m, std::vector<ProcessPair>&& procs):
            model(m),
            processes(std::move(procs))
        {

        }

        const Process::LayerModel* model{};
        std::vector<ProcessPair> processes;
};

class ISCORE_PLUGIN_SCENARIO_EXPORT SlotPresenter final : public NamedObject, public Nano::Observer
{
        Q_OBJECT

    public:
        SlotPresenter(
                const iscore::DocumentContext& pl,
                const SlotModel& model,
                RackView* view,
                QObject* parent);
        virtual ~SlotPresenter();

        const Id<SlotModel>& id() const;
        const SlotModel& model() const;
        int height() const; // Return the height of the view

        void setWidth(qreal w);
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
        void on_layerModelCreated(const Process::LayerModel&);
        void on_layerModelRemoved(const Process::LayerModel&);
        void on_layerModelPutToFront(const Process::LayerModel&);

        void on_layerModelCreated_impl(const Process::LayerModel&);

        void updateProcesses();
        void updateProcessShape(const SlotProcessData&);
        void updateProcessesShape();

        const Process::ProcessList& m_processList;
        const SlotModel& m_model;
        SlotView* m_view{};
        std::vector<SlotProcessData> m_processes;

        // Maybe move this out of the state of the presenter ?
        int m_currentResizingValue {}; // Used when the m_slotView is being resized.

        ZoomRatio m_zoomRatio {};

        bool m_enabled{true};
        bool m_looping{false};
};
}
