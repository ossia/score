#pragma once

namespace iscore
{
	class SettingsPresenter;
	class SettingsDelegatePresenterInterface;
	class SettingsDelegateModelInterface;
	class SettingsDelegateViewInterface;

	// This should separate the settings model and settings view (there should be no need for a presenter here?)
	// Als, should settings be remotely transmitted so that they are the same everywhere ? (guess : yes)
	/**
	 * @brief The SettingsGroup class
	 *
	 * Reimplement in order to provide custom settings for the plug-in.
	 */
	class SettingsDelegateFactoryInterface
	{
		public:
			virtual SettingsDelegateViewInterface* makeView() = 0;
			// Mission : transmettre au présenteur global pour validation de l'action.
			// Ou bien c'est directement la vue qui s'en charge?
			// Risque de duplication dans le cas SmallView / StandardView / FullView...
			virtual SettingsDelegatePresenterInterface* makePresenter(SettingsPresenter*, SettingsDelegateModelInterface* m, SettingsDelegateViewInterface* v) = 0;
			virtual SettingsDelegateModelInterface* makeModel() = 0; // Accédé par les commandes uniquement.
	};
}
