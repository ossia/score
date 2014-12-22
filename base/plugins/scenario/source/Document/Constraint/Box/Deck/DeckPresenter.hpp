#pragma once
#include <tools/NamedObject.hpp>

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

		int id() const;
		int height() const; // Return the height of the view

		// Position in the Box : 1st, second...
		int position() const;

		// Vertical position in pixels in the scene
		void setVerticalPosition(int pos);


	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);

		void askUpdate();

	public slots:
		// From Model
		void on_processViewModelCreated(int processId);
		void on_processViewModelDeleted(int processId);
		void on_heightChanged(int height);

		// From View
		void on_bottomHandleSelected();
		void on_bottomHandleChanged(int newHeight);
		void on_bottomHandleReleased();



	private:
		void on_processViewModelCreated_impl(ProcessViewModelInterface*);

		DeckModel* m_model;
		DeckView* m_view;
		QVector<ProcessPresenterInterface*> m_processes;

		// Maybe move this out of the state of the presenter ?
		int m_currentResizingValue{}; // Used when the deckView is being resized.
};

