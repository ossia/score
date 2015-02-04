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
