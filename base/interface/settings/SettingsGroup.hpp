#pragma once
#include <memory>
namespace iscore
{
	class SettingsGroupView
	{
	};

	class SettingsGroupPresenter
	{
	};

	class SettingsGroupModel
	{
	};

	// This should separate the settings model and settings view (there should be no need for a presenter here?)
	// Als, should settings be remotely transmitted so that they are the same everywhere ? (guess : yes)
	class SettingsGroup
	{
		public:
			virtual std::unique_ptr<SettingsGroupView> makeView() = 0;
			// Mission : transmettre au présenteur global pour validation de l'action.
			// Ou bien c'est directement la vue qui s'en charge?
			// Risque de duplication dans le cas SmallView / StandardView / FullView...
			virtual std::unique_ptr<SettingsGroupPresenter> makePresenter() = 0;
			virtual std::unique_ptr<SettingsGroupModel> makeModel() = 0; // Accédé par les commandes uniquement.
	};
}
