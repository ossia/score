#pragma once
#include "Slot/SlotPresenter.hpp"
#include "Slot/SlotModel.hpp"

#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <ProcessInterface/ZoomHelper.hpp>

class ProcessModel;
class SlotPresenter;
class BoxModel;
class BoxView;
class SlotModel;

namespace iscore
{
    class SerializableCommand;
}

class BoxPresenter : public NamedObject
{
        Q_OBJECT

    public:
        BoxPresenter(const BoxModel& model,
                     BoxView* view,
                     QObject* parent);
        virtual ~BoxPresenter();

        const BoxView& view() const;

        int height() const;
        int width() const;
        void setWidth(int);

        const id_type<BoxModel>& id() const;
        auto getSlots() const // here we use the 'get' prefix, because 'slots' is keyWord for Qt ...
        { return m_slots; }

        void setDisabledSlotState();
        void setEnabledSlotState();


    signals:
        void askUpdate();

        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);


    public slots:
        void on_durationChanged(const TimeValue& duration);
        void on_slotCreated(const id_type<SlotModel>& slotId);
        void on_slotRemoved(const id_type<SlotModel>& slotId);

        void on_askUpdate();

        void on_zoomRatioChanged(ZoomRatio val);
        void on_slotPositionsChanged();

    private:
        void on_slotCreated_impl(const SlotModel& m);

        // Updates the shape of the view
        void updateShape();

        const BoxModel& m_model;
        BoxView* m_view;
        IdContainer<SlotPresenter,SlotModel> m_slots;

        ZoomRatio m_zoomRatio{};
        TimeValue m_duration {};
};

