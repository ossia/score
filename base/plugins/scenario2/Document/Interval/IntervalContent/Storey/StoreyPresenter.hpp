#pragma once
#include <QNamedObject>
class StoreyModel;
class StoreyView;
namespace iscore
{
	class ProcessPresenterInterface;
}
class StoreyPresenter : public QNamedObject
{
	Q_OBJECT

	public:
		StoreyPresenter(StoreyModel* model, StoreyView* view, QObject* parent);
		virtual ~StoreyPresenter() = default;

	private:
		StoreyModel* m_model;
		StoreyView* m_view;
		std::vector<iscore::ProcessPresenterInterface*> m_processes;
};

