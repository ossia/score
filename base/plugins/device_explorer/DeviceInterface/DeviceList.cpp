#include "DeviceList.hpp"
#include "../Panel/DeviceExplorerModel.hpp"

#include <QApplication>

QStringList DeviceList::getDevices()
{
	QStringList s;
	auto treemodel =
			qApp->findChild<DeviceExplorerModel*>("DeviceExplorerModel");

	for(int i = 0; i < treemodel->rowCount(QModelIndex()); i++)
	{
		auto index = treemodel->index(i,
									  treemodel->getNameColumn(),
									  QModelIndex());
		if(treemodel->isDevice(index))
		{
			QString str = treemodel->data(index, treemodel->getNameColumn()).toString();
			s << str;
		}
	}

	return s;
}

void recursivelyGetAddresses(const DeviceExplorerModel& explorer,
							 QModelIndex index,
							 QString currentAddress,
							 QStringList& list)
{
	for(int i = 0; i < explorer.rowCount(index); i++)
	{
		auto nodeIndex = index.child(i, explorer.getNameColumn());
		QString nodeName = explorer.data(nodeIndex, explorer.getNameColumn()).toString();

		QString nodeAddress = currentAddress + "/" + nodeName;
		list << nodeAddress;

		recursivelyGetAddresses(explorer, nodeIndex, nodeAddress, list);
	}
}

QStringList DeviceList::getAddresses(QString device)
{
	QStringList s;
	auto treemodel =
			qApp->findChild<DeviceExplorerModel*>("DeviceExplorerModel");


	for(int i = 0; i < treemodel->rowCount(QModelIndex()); i++)
	{
		auto deviceIndex = treemodel->index(i,
											treemodel->getNameColumn(),
											QModelIndex());

		if(treemodel->isDevice(deviceIndex))
		{
			QString devicestr = treemodel->data(deviceIndex, treemodel->getNameColumn()).toString();
			if(devicestr == device)
			{
				recursivelyGetAddresses(*treemodel,
										deviceIndex,
										QString{"/%1"}.arg(device),
										s);

			}
		}
	}

	return s;
}


#include <QApplication>
DeviceCompleter::DeviceCompleter(QObject* parent):
	QCompleter{parent}
{
	auto treemodel =
			qApp->findChild<DeviceExplorerModel*>("DeviceExplorerModel");

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
		path = QString{"%1/"}.arg(iter.data(0).toString()) + path;
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
