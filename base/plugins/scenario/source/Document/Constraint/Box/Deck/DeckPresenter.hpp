#pragma once
#include <tools/NamedObject.hpp>
#include <tools/SettableIdentifier.hpp>

class DeckModel;
class DeckView;
namespace iscore
{
    class SerializableCommand;
}
class ProcessPresenterInterface;
class ProcessViewModelInterface;
class DeckPresenter : public NamedObject
{
        Q_OBJECT

    public:
        DeckPresenter(DeckModel* model,
                      DeckView* view,
                      QObject* parent);
        virtual ~DeckPresenter();

        id_type<DeckModel> id() const;
        int height() const; // Return the height of the view

        void setWidth(int w);
        void setVerticalPosition(int h);

    signals:
        void submitCommand(iscore::SerializableCommand*);
        void elementSelected(QObject*);

        void askUpdate();

    public slots:
        // From Model
        void on_processViewModelCreated(id_type<ProcessViewModelInterface> processId);
        void on_processViewModelDeleted(id_type<ProcessViewModelInterface> processId);
        void on_processViewModelSelected(id_type<ProcessViewModelInterface> processId);
        void on_heightChanged(int height);
        void on_parentGeometryChanged();

        // From View
        void on_bottomHandleSelected();
        void on_bottomHandleChanged(int newHeight);
        void on_bottomHandleReleased();

        void on_horizontalZoomChanged(int);

    private:
        void on_processViewModelCreated_impl(ProcessViewModelInterface*);

        void updateProcessesShape();

        DeckModel* m_model;
        DeckView* m_view;
        QVector<ProcessPresenterInterface*> m_processes;

        // Maybe move this out of the state of the presenter ?
        int m_currentResizingValue {}; // Used when the deckView is being resized.

        int m_horizontalZoomSliderVal {};
};

