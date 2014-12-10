#ifndef REORDERWIDGET_HPP
#define REORDERWIDGET_HPP

#include <QWidget>
#include <QListWidget>

class ReorderWidget : public QWidget
{
		Q_OBJECT
	public:
		explicit ReorderWidget (QWidget* parent = 0);
		ReorderWidget (std::vector<QWidget*> widgets, QWidget* parent = 0 );

	signals:

	public slots:

	private:
		std::vector<QWidget*> _widgets;
};

#endif // REORDERWIDGET_HPP
