#pragma once
#include <QObject>

namespace iscore
{
	class DocumentDelegateFactoryInterface;
	class DocumentDelegateFactoryInterface_QtInterface
	{
		public:
			virtual ~DocumentDelegateFactoryInterface_QtInterface() = default;

			// List the possible documents kinds.
			virtual QStringList document_list() const = 0;
			virtual DocumentDelegateFactoryInterface* document_make(QString name) = 0;
	};
}

#define DocumentDelegateFactoryInterface_QtInterface_iid "org.ossia.i-score.plugins.DocumentDelegateFactoryInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::DocumentDelegateFactoryInterface_QtInterface, DocumentDelegateFactoryInterface_QtInterface_iid)
