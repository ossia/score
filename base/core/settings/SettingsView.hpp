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
			SettingsView():
				QDialog{}
			{
				this->setLayout(m_layout);
				m_layout->addWidget(m_buttons);
				connect(m_buttons, &QDialogButtonBox::accepted,
						this,	   &SettingsView::accept);
				connect(m_buttons, &QDialogButtonBox::rejected,
						this,	   &SettingsView::reject);
			}

			void addSettingsView(std::unique_ptr<SettingsGroupView>&& view)
			{
				m_layout->addWidget(view->getWidget());
				m_pluginViews.insert(std::move(view));
			}

		signals:
			void accept();
			void reject();

		private:
			std::set<std::unique_ptr<SettingsGroupView>> m_pluginViews;

			QHBoxLayout* m_layout{new QHBoxLayout{this}};
			QDialogButtonBox* m_buttons{new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
															 this}};

	};
}
