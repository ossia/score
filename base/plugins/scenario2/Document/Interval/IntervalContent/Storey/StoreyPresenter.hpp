#pragma once
#include <QNamedObject>
class StoreyModel;
namespace iscore
{
	class ProcessPresenterInterface;
}
class StoreyPresenter : public QNamedObject
{
	Q_OBJECT

	public:
		StoreyPresenter(StoreyModel* const model, QObject* parent);
		virtual ~StoreyPresenter() = default;

	private:
		StoreyModel* m_model;
		std::vector<iscore::ProcessPresenterInterface*> m_processes;
};

