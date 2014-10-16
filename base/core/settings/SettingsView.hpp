#pragma once
#include <set>
#include <memory>
#include <interface/settings/SettingsGroup.hpp>

#include <QWidget>
#include <QHBoxLayout>

namespace iscore
{
	class SettingsView : public QWidget
	{
		public:
			SettingsView():
				QWidget{}
			{
				this->setLayout(m_layout);
			}

			void addSettingsView(std::unique_ptr<SettingsGroupView>&& view)
			{
				m_layout->addWidget(view->getWidget());
				m_pluginViews.insert(std::move(view));
			}

		private:
			std::set<std::unique_ptr<SettingsGroupView>> m_pluginViews;

			QHBoxLayout* m_layout{new QHBoxLayout{this}};

	};
}
