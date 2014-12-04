#pragma once
#include <QNamedObject>
class StoreyPresenter;
class IntervalContentModel;
class IntervalContentView;
class StoreyModel;
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

	public slots:
		void on_storeyCreated(int storeyId);
		void on_storeyRemoved(int storeyId);

	private:
		void on_storeyCreated_impl(StoreyModel* m);

		IntervalContentModel* m_model;
		IntervalContentView* m_view;
		std::vector<StoreyPresenter*> m_storeys;
};

