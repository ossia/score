#pragma once
#include <QNamedObject>
class StoreyPresenter;
class IntervalContentModel;
class IntervalContentPresenter : public QNamedObject
{
	Q_OBJECT

	public:
		IntervalContentPresenter(IntervalContentModel* model, QObject* parent);
		virtual ~IntervalContentPresenter() = default;

	private:
		IntervalContentModel* m_model;
		std::vector<StoreyPresenter*> m_storeys;
};

