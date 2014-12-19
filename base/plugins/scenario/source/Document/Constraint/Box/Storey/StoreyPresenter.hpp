#pragma once
#include <tools/NamedObject.hpp>

class StoreyModel;
class StoreyView;
namespace iscore
{
	class SerializableCommand;
}
class ProcessPresenterInterface;
class ProcessViewModelInterface;
class StoreyPresenter : public NamedObject
{
	Q_OBJECT

	public:
		StoreyPresenter(StoreyModel* model,
						StoreyView* view,
						QObject* parent);
		virtual ~StoreyPresenter();

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

		StoreyModel* m_model;
		StoreyView* m_view;
		QVector<ProcessPresenterInterface*> m_processes;

		// Maybe move this out of the state of the presenter ?
		int m_currentResizingValue{}; // Used when the storeyView is being resized.
};

