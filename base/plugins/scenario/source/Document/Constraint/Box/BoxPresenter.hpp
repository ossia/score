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

		int height() const;

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);

		void askUpdate();

	public slots:
		void on_storeyCreated(int storeyId);
		void on_storeyDeleted(int storeyId);

		void on_askUpdate();
	private:
		void on_storeyCreated_impl(StoreyModel* m);

		// Updates the shape of the view
		void updateShape();

		BoxModel* m_model;
		BoxView* m_view;
		std::vector<StoreyPresenter*> m_decks;
};

