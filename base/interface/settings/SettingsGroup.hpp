#pragma once
#include <memory>

class QWidget;
namespace iscore
{
	class SettingsPresenter;
	class SettingsGroupPresenter;
	class SettingsGroupModel;


	class SettingsGroupView
	{
		public:
			virtual ~SettingsGroupView() = default;
			virtual void setPresenter(SettingsGroupPresenter* presenter)
			{
				m_presenter = presenter;
			}

			SettingsGroupPresenter* getPresenter()
			{
				return m_presenter;
			}

			virtual QWidget* getWidget() = 0; // Y U NO QML? . ps : ownership transfer ? ? ? what about "this" ?

		protected:
			SettingsGroupPresenter* m_presenter;
	};

	// This should separate the settings model and settings view (there should be no need for a presenter here?)
	// Als, should settings be remotely transmitted so that they are the same everywhere ? (guess : yes)
	class SettingsGroup
	{
		public:
			virtual SettingsGroupView* makeView() = 0;
			// Mission : transmettre au présenteur global pour validation de l'action.
			// Ou bien c'est directement la vue qui s'en charge?
			// Risque de duplication dans le cas SmallView / StandardView / FullView...
			virtual SettingsGroupPresenter* makePresenter(SettingsPresenter*, SettingsGroupModel* m, SettingsGroupView* v) = 0;
			virtual SettingsGroupModel* makeModel() = 0; // Accédé par les commandes uniquement.
	};
}
