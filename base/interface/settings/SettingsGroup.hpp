#pragma once
#include <memory>
class QWidget;
namespace iscore
{
	class SettingsPresenter;
	class SettingsGroupPresenter;
	class SettingsGroupModel
	{
		public:
			virtual ~SettingsGroupModel() = default;
			virtual void setPresenter(SettingsGroupPresenter* presenter) = 0;
	};

	class SettingsGroupView
	{
		public:
			virtual ~SettingsGroupView() = default;
			virtual void setPresenter(SettingsGroupPresenter* presenter) = 0;
			virtual QWidget* getWidget() = 0; // Y U NO QML? . ps : ownership transfer ? ? ? what about "this" ?
	};

	class SettingsGroupPresenter
	{
		public:
			SettingsGroupPresenter(SettingsPresenter* parent_presenter, SettingsGroupModel* model, SettingsGroupView* view):
				m_model{model},
				m_view{view},
				m_parentPresenter{parent_presenter}
			{}

			virtual ~SettingsGroupPresenter() = default;

		protected:
			SettingsGroupModel* m_model;
			SettingsGroupView* m_view;
			SettingsPresenter* m_parentPresenter;
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
			virtual std::unique_ptr<SettingsGroupPresenter> makePresenter(SettingsPresenter*, SettingsGroupModel* m, SettingsGroupView* v) = 0;
			virtual std::unique_ptr<SettingsGroupModel> makeModel() = 0; // Accédé par les commandes uniquement.
	};
}
