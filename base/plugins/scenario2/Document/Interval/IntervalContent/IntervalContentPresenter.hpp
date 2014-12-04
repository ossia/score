#pragma once
#include <QNamedObject>
class StoreyPresenter;
class IntervalContentModel;
class IntervalContentView;
namespace iscore
{
	class SerializableCommand;
}
class IntervalContentPresenter : public QNamedObject
{
	Q_OBJECT

	public:
		IntervalContentPresenter(IntervalContentModel* model,
								 IntervalContentView* view,
								 QObject* parent);
		virtual ~IntervalContentPresenter();

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);

	private:
		IntervalContentModel* m_model;
		IntervalContentView* m_view;
		std::vector<StoreyPresenter*> m_storeys;
};

