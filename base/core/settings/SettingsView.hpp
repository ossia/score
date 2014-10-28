#pragma once
#include <set>
#include <memory>
#include <interface/settings/SettingsGroup.hpp>

#include <QDialog>
#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QStackedWidget>
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

			QVBoxLayout* m_vertLayout{new QVBoxLayout{this}};
			QHBoxLayout* m_hboxLayout{new QHBoxLayout{this}};
			QListWidget* m_settingsList{new QListWidget{this}};
			QVBoxLayout* m_subVertLayout{new QVBoxLayout{this}};
			QStackedWidget* m_stackedWidget{new QStackedWidget{this}};

			QDialogButtonBox* m_buttons{new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
															 this}};

	};
}
