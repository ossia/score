#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <ProcessInterface/ZoomHelper.hpp>

class ProcessModel;
class DeckPresenter;
class BoxModel;
class BoxView;
class DeckModel;

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

        int height() const;
        int width() const;
        void setWidth(int);

        const id_type<BoxModel>& id() const;
        auto decks() const
        { return m_decks; }

        void setDisabledDeckState();
        void setEnabledDeckState();


    signals:
        void askUpdate();

        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);


    public slots:
        void on_durationChanged(const TimeValue& duration);
        void on_deckCreated(const id_type<DeckModel>& deckId);
        void on_deckRemoved(const id_type<DeckModel>& deckId);

        void on_askUpdate();

        void on_zoomRatioChanged(ZoomRatio val);
        void on_deckPositionsChanged();

    private:
        void on_deckCreated_impl(const DeckModel& m);

        // Updates the shape of the view
        void updateShape();

        const BoxModel& m_model;
        BoxView* m_view;
        std::vector<DeckPresenter*> m_decks;

        ZoomRatio m_zoomRatio{};
        TimeValue m_duration {};
};

