#pragma once
#include <tools/NamedObject.hpp>

class StoreyPresenter;
class ConstraintContentModel;
class ConstraintContentView;
class StoreyModel;

namespace iscore
{
	class SerializableCommand;
}

class ConstraintContentPresenter : public NamedObject
{
	Q_OBJECT

	public:
		ConstraintContentPresenter(ConstraintContentModel* model,
								 ConstraintContentView* view,
								 QObject* parent);
		virtual ~ConstraintContentPresenter();

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);

	public slots:
		void on_storeyCreated(int storeyId);
		void on_storeyDeleted(int storeyId);

	private:
		void on_storeyCreated_impl(StoreyModel* m);

		ConstraintContentModel* m_model;
		ConstraintContentView* m_view;
		std::vector<StoreyPresenter*> m_storeys;
};

