#include "DeviceCompleter.hpp"
#include "../Panel/DeviceExplorerModel.hpp"

#include <QApplication>

DeviceCompleter::DeviceCompleter(DeviceExplorerModel* treemodel,
                                 QObject* parent) :
    QCompleter {parent}
{
    setModel(treemodel);

    setCompletionColumn(0);
    setCompletionRole(Qt::DisplayRole);
    setCaseSensitivity(Qt::CaseInsensitive);
}

QString DeviceCompleter::pathFromIndex(const QModelIndex& index) const
{
    QString path;

    QModelIndex iter = index;

    while(iter.isValid())
    {
        path = QString {"%1/"} .arg(iter.data(0).toString()) + path;
        iter = iter.parent();
    }

    return "/" + path.remove(path.length() - 1, 1);
}

QStringList DeviceCompleter::splitPath(const QString& path) const
{
    QString p2 = path;

    if(p2.at(0) == QChar('/'))
    {
        p2.remove(0, 1);
    }

    return p2.split("/");
}
/*

#include <QMenu>
#include "../Panel/Node.hpp"
QMenu* nodeToQMenu(const Node* n)
{
	QMenu* menu = new QMenu{n->name()};

	for(const Node* child : n->children())
	{
		if(child->hasChildren())
			menu->addMenu(nodeToQMenu(child));
		else
			menu->addAction(child->name());
	}

	return menu;
}


QMenu* rootNodeToQMenu()
{
	auto treemodel =
			qApp->findChild<DeviceExplorerModel*>("DeviceExplorerModel");
	if(treemodel->rootNode())
	{
		auto menu = nodeToQMenu(treemodel->rootNode());
		menu->setTitle(QObject::tr("Devices"));
		return menu;
	}

	return new QMenu{QObject::tr("No device")};
}
*/
