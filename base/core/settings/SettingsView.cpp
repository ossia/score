#include <core/settings/SettingsView.hpp>

using namespace iscore;


SettingsView::SettingsView(QWidget* parent):
	QDialog{parent}
{
	this->setLayout(m_vertLayout);
	auto centerWidg = new QWidget{this};
	centerWidg->setLayout(m_hboxLayout);
	m_hboxLayout->addWidget(m_settingsList);

	m_vertLayout->addWidget(centerWidg);
	m_vertLayout->addWidget(m_buttons);

	connect(m_buttons, &QDialogButtonBox::accepted,
			this,	   &SettingsView::accept);
	connect(m_buttons, &QDialogButtonBox::rejected,
			this,	   &SettingsView::reject);
}

void SettingsView::addSettingsView(SettingsGroupView* view)
{
	m_stackedWidget->addWidget(view->getWidget());
	m_hboxLayout->addWidget(view->getWidget());
	m_pluginViews.insert(view);
}
