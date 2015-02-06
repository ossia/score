#include "AddressBar.hpp"
#include "ClickableLabel.hpp"

AddressBar::AddressBar(QWidget* parent):
	QWidget{parent},
	m_layout{new QHBoxLayout{this}}
{
    setLayout(m_layout);
}


void AddressBar::setTargetObject(ObjectPath&& path)
{
	QLayoutItem *child;
	while ((child = m_layout->takeAt(0)) != 0)
	{
		child->widget()->deleteLater();
		delete child;
	}

	m_currentPath = path;
	for(auto& identifier : m_currentPath)
	{
		auto lab = new ClickableLabel{QString{"%1%2"}.arg(identifier.objectName())
													  .arg(identifier.id() ? "." + QString::number(*identifier.id()) : ""), this};
		connect(lab, SIGNAL(clicked(ClickableLabel*)),
				this, SLOT(on_elementClicked(ClickableLabel*)));

		m_layout->addWidget(lab);
		m_layout->addWidget(new QLabel{"/"});
	}

	m_layout->addStretch();
}

void AddressBar::on_elementClicked(ClickableLabel* clicked)
{
	int index = m_layout->indexOf(clicked) / 2;
	if(index < m_currentPath.vec().size())
	{
		auto vec = m_currentPath.vec();
		vec.resize(index + 1);

		ObjectPath newPath{std::move(vec)};

		emit objectSelected(newPath);
	}
}
