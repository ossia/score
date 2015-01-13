#pragma once
#include <QHBoxLayout>
#include <QWidget>
#include <tools/SettableIdentifier.hpp>
#include <tools/ObjectPath.hpp>

class ClickableLabel;
class AddressBar : public QWidget
{
		Q_OBJECT
	public:
		AddressBar(QWidget* parent);
		void setTargetObject(ObjectPath&&);

	signals:
		void objectSelected(ObjectPath path);

	private slots:
		void on_elementClicked(ClickableLabel*);

	private:
		QHBoxLayout* m_layout{};
		ObjectPath m_currentPath;
};
