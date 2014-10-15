#pragma once

namespace iscore
{
	class SettingsModel;
	class SettingsView;
	class SettingsPresenter
	{
		public:
		SettingsPresenter(SettingsModel* model, SettingsView* view)
		{

		}

		private:
			SettingsModel* m_model;
			SettingsView* m_view;
	};
}
