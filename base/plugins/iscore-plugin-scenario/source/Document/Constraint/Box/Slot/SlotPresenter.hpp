#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/ZoomHelper.hpp>

class SlotModel;
class SlotView;
namespace iscore
{
    class SerializableCommand;
}
class ProcessPresenter;
class ProcessModel;
class ProcessViewModel;
class ProcessView;
class BoxView;
class SlotPresenter : public NamedObject
{
        Q_OBJECT

    public:
        SlotPresenter(const SlotModel& model,
                      BoxView* view,
                      QObject* parent);
        virtual ~SlotPresenter();

        const id_type<SlotModel>& id() const;
        const SlotModel& model() const;
        int height() const; // Return the height of the view

        void setWidth(double w);
        void setVerticalPosition(double h);

        // Sets the enabled - disabled graphism for
        // the slot move tool
        void enable();
        void disable();

        using ProcessPair = QPair<ProcessPresenter*, ProcessView*>;
        const QVector<ProcessPair>& processes() const { return m_processes; }

    signals:
        void askUpdate();

        void pressed(const QPointF&) const;
        void moved(const QPointF&) const;
        void released(const QPointF&) const;

    public slots:
        // From Model
        void on_processViewModelCreated(const id_type<ProcessViewModel>& processId);
        void on_processViewModelDeleted(const id_type<ProcessViewModel>& processId);
        void on_processViewModelPutToFront(const id_type<ProcessViewModel>& processId);
        void on_heightChanged(double height);
        void on_parentGeometryChanged();

        void on_zoomRatioChanged(ZoomRatio);

    private:
        void on_processViewModelCreated_impl(const ProcessViewModel&);

        void updateProcessesShape();

        const SlotModel& m_model;
        SlotView* m_view{};
        QVector<ProcessPair> m_processes;

        // Maybe move this out of the state of the presenter ?
        int m_currentResizingValue {}; // Used when the m_slotView is being resized.

        ZoomRatio m_zoomRatio {};

        bool m_enabled{true};
};

