#pragma once
#include <set>
#include <memory>
#include <interface/settings/SettingsGroup.hpp>

#include <QDialog>
#include <QWidget>
#include <QHBoxLayout>
#include <QDialogButtonBox>

namespace iscore
{
	class SettingsView : public QDialog
	{
			Q_OBJECT
		public:
			SettingsView(QWidget* parent);
			void addSettingsView(SettingsGroupView* view);

		private:
			std::set<SettingsGroupView*> m_pluginViews;

			QHBoxLayout* m_layout{new QHBoxLayout{this}};
			QDialogButtonBox* m_buttons{new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
															 this}};

	};
}
