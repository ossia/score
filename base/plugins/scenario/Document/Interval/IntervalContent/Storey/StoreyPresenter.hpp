#pragma once
#include <tools/NamedObject.hpp>

class StoreyModel;
class StoreyView;
namespace iscore
{
	class SerializableCommand;
	class ProcessPresenterInterface;
	class ProcessViewModelInterface;
}
class StoreyPresenter : public NamedObject
{
	Q_OBJECT

	public:
		StoreyPresenter(StoreyModel* model,
						StoreyView* view,
						QObject* parent);
		virtual ~StoreyPresenter();

		int id() const;

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);

	public slots:
		void on_processViewModelCreated(int processId);
		void on_processViewModelDeleted(int processId);

	private:
		void on_processViewModelCreated_impl(iscore::ProcessViewModelInterface*);

		StoreyModel* m_model;
		StoreyView* m_view;
		std::vector<iscore::ProcessPresenterInterface*> m_processes;
};

