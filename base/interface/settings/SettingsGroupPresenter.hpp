#pragma once
#include <QObject>
#include <core/settings/SettingsPresenter.hpp>

namespace iscore
{
	class SettingsGroupModel;
	class SettingsGroupView;
	class SettingsGroupPresenter : public QObject
	{
		public:
			SettingsGroupPresenter(SettingsPresenter* parent_presenter,
								   SettingsGroupModel* model,
								   SettingsGroupView* view):
				QObject{parent_presenter},
				m_model{model},
				m_view{view},
				m_parentPresenter{parent_presenter}
			{}

			virtual ~SettingsGroupPresenter() = default;
			virtual void on_accept() = 0;
			virtual void on_reject() = 0;

			virtual QString settingsName() = 0;
			virtual QIcon settingsIcon() = 0;

		protected:
			SettingsGroupModel* m_model;
			SettingsGroupView* m_view;
			SettingsPresenter* m_parentPresenter;
	};
}
