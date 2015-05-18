#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/ZoomHelper.hpp>

class DeckModel;
class DeckView;
class ICommandDispatcher;
namespace iscore
{
    class SerializableCommand;
}
class ProcessPresenter;
class ProcessModel;
class ProcessViewModel;
class ProcessView;
class BoxView;
class DeckPresenter : public NamedObject
{
        Q_OBJECT

    public:
        DeckPresenter(const DeckModel& model,
                      BoxView* view,
                      QObject* parent);
        virtual ~DeckPresenter();

        const id_type<DeckModel>& id() const;
        const DeckModel& model() const;
        int height() const; // Return the height of the view

        void setWidth(double w);
        void setVerticalPosition(double h);

        // Sets the enabled - disabled graphism for
        // the deck move tool
        void enable();
        void disable();

    signals:
        void askUpdate();

        void pressed(const QPointF&) const;
        void moved(const QPointF&) const;
        void released(const QPointF&) const;

    public slots:
        // From Model
        void on_processViewModelCreated(const id_type<ProcessViewModel>& processId);
        void on_processViewModelDeleted(const id_type<ProcessViewModel>& processId);
        void on_processViewModelSelected(const id_type<ProcessViewModel>& processId);
        void on_heightChanged(double height);
        void on_parentGeometryChanged();

        void on_zoomRatioChanged(ZoomRatio);

    private:
        void on_processViewModelCreated_impl(const ProcessViewModel&);

        void updateProcessesShape();

        const DeckModel& m_model;
        DeckView* m_view{};
        using ProcessPair = QPair<ProcessPresenter*, ProcessView*>;
        QVector<ProcessPair> m_processes;

        // Maybe move this out of the state of the presenter ?
        int m_currentResizingValue {}; // Used when the deckView is being resized.

        ZoomRatio m_zoomRatio {};

        bool m_enabled{true};
};

