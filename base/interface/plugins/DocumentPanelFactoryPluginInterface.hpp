#pragma once
#include <QString>
#include <interface/docpanel/DocumentPanel.hpp>

namespace iscore
{
	class DocumentPanelFactoryPluginInterface
	{
		public:
			virtual ~DocumentPanelFactoryPluginInterface() = default;

			// List the possible documents kinds.
			virtual QStringList document_list() const = 0;
			virtual DocumentPanel* document_make(QString name) = 0;
	};
}

#define DocumentPanelFactoryPluginInterface_iid "org.ossia.i-score.plugins.DocumentPanelFactoryPluginInterface"

Q_DECLARE_INTERFACE(iscore::DocumentPanelFactoryPluginInterface, DocumentPanelFactoryPluginInterface_iid)
