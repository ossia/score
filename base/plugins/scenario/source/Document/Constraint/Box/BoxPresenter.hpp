#pragma once
#include <tools/NamedObject.hpp>

class DeckPresenter;
class BoxModel;
class BoxView;
class DeckModel;

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
		int id() const;

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);

		void askUpdate();

	public slots:
		void on_deckCreated(int deckId);
		void on_deckRemoved(int deckId);

		void on_askUpdate();

	private:
		void on_deckCreated_impl(DeckModel* m);

		// Updates the shape of the view
		void updateShape();

		BoxModel* m_model;
		BoxView* m_view;
		std::vector<DeckPresenter*> m_decks;
};

