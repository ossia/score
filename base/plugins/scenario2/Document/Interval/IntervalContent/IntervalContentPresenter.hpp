#pragma once
#include <QNamedObject>
class StoreyPresenter;
class IntervalContentModel;
class IntervalContentView;
class IntervalContentPresenter : public QNamedObject
{
	Q_OBJECT

	public:
		IntervalContentPresenter(IntervalContentModel* model,
								 IntervalContentView* view,
								 QObject* parent);
		virtual ~IntervalContentPresenter();

	private:
		IntervalContentModel* m_model;
		IntervalContentView* m_view;
		std::vector<StoreyPresenter*> m_storeys;
};

