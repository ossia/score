#pragma once
#include <tools/NamedObject.hpp>

#include <QVector>

class DeviceList : public NamedObject
{
	public:
		static QStringList getDevices();
		static QStringList getAddresses(QString device);

};
