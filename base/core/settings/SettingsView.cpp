#include <core/settings/SettingsView.hpp>

using namespace iscore;


SettingsView::SettingsView(QWidget* parent):
	QDialog{parent}
{
	this->setLayout(m_layout);
	m_layout->addWidget(m_buttons);
	connect(m_buttons, &QDialogButtonBox::accepted,
			this,	   &SettingsView::accept);
	connect(m_buttons, &QDialogButtonBox::rejected,
			this,	   &SettingsView::reject);
}

void SettingsView::addSettingsView(SettingsGroupView* view)
{
	m_layout->addWidget(view->getWidget());
	m_pluginViews.insert(view);
}
