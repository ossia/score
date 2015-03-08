#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <ProcessInterface/ZoomHelper.hpp>

class ProcessSharedModelInterface;
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
        BoxPresenter(BoxModel* model,
                     BoxView* view,
                     QObject* parent);
        virtual ~BoxPresenter();

        int height() const;
        int width() const;
        void setWidth(int);

        id_type<BoxModel> id() const;

    signals:
        void askUpdate();

    public slots:
        void on_durationChanged(TimeValue duration);
        void on_deckCreated(id_type<DeckModel> deckId);
        void on_deckRemoved(id_type<DeckModel> deckId);

        void on_askUpdate();

        void on_zoomRatioChanged(ZoomRatio val);
        void on_deckPositionsChanged();

    private:
        void on_deckCreated_impl(DeckModel* m);

        // Updates the shape of the view
        void updateShape();

        BoxModel* m_model;
        BoxView* m_view;
        std::vector<DeckPresenter*> m_decks;

        ZoomRatio m_zoomRatio{};
        TimeValue m_duration {};
};

