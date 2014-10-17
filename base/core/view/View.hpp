#pragma once
#include <QMainWindow>
#include <QDebug>
#include <QMenuBar>
namespace iscore
{
	class View : public QMainWindow
	{
		public:
			View(QObject* parent):
				QMainWindow{}
			{
				this->menuBar()->addMenu("Fichier");
			}

	};
}
