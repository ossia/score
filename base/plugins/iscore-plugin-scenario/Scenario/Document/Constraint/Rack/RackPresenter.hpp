#pragma once
#include "Slot/SlotPresenter.hpp"
#include "Slot/SlotModel.hpp"

#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>

class Process;
class SlotPresenter;
class RackModel;
class RackView;
class SlotModel;

namespace iscore
{
    class SerializableCommand;
}

class RackPresenter final : public NamedObject
{
        Q_OBJECT

    public:
        RackPresenter(const RackModel& model,
                     RackView* view,
                     QObject* parent);
        virtual ~RackPresenter();

        const RackModel& model() const
        { return m_model; }
        const RackView& view() const;

        qreal height() const;
        qreal width() const;
        void setWidth(qreal);

        const Id<RackModel>& id() const;
        const IdContainer<SlotPresenter,SlotModel>& getSlots() const // here we use the 'get' prefix, because 'slots' is keyWord for Qt ...
        { return m_slots; }

        void setDisabledSlotState();
        void setEnabledSlotState();


    signals:
        void askUpdate();

        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);


    public slots:
        void on_durationChanged(const TimeValue&);

        void on_askUpdate();

        void on_zoomRatioChanged(ZoomRatio);
        void on_slotPositionsChanged();

    private:
        void on_slotCreated(const SlotModel&);
        void on_slotRemoved(const SlotModel&);

        void on_slotCreated_impl(const SlotModel& m);

        // Updates the shape of the view
        void updateShape();

        const RackModel& m_model;
        RackView* m_view;
        IdContainer<SlotPresenter,SlotModel> m_slots;

        ZoomRatio m_zoomRatio{};
        TimeValue m_duration {};
};

