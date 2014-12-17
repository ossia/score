#pragma once
#include <tools/NamedObject.hpp>

class StoreyPresenter;
class BoxModel;
class BoxView;
class StoreyModel;

namespace iscore
{
	class SerializableCommand;
}

class BoxPresenter : public NamedObject
{
	Q_OBJECT

	public:
		BoxPresenter(BoxModel* model,
								 BoxView* view,
								 QObject* parent);
		virtual ~BoxPresenter();

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);

	public slots:
		void on_storeyCreated(int storeyId);
		void on_storeyDeleted(int storeyId);

		void update();
	private:
		void on_storeyCreated_impl(StoreyModel* m);

		BoxModel* m_model;
		BoxView* m_view;
		std::vector<StoreyPresenter*> m_storeys;
};

