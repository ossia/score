#pragma once
#include <QPluginLoader>
#include <QMap>
namespace iscore
{
	//Has the ownership of the plug-ins.
	class PluginManager : public QObject
	{
			Q_OBJECT
		public:
			using QObject::QObject;
			~PluginManager()
			{
				clearPlugins();
			}

			QMap<QString, QObject*> availablePlugins() const
			{
				return m_availablePlugins;
			}

			void reloadPlugins();

		signals:
			void newPlugin(QObject*);

		private:
			void clearPlugins();
			QMap<QString, QObject*> m_availablePlugins;
	};
}
