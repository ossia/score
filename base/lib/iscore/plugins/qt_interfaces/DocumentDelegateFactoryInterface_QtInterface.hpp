#pragma once
#include <QObject>
#include <vector>
#include <iscore_lib_base_export.h>

namespace iscore
{
    class DocumentDelegateFactoryInterface;

    class ISCORE_LIB_BASE_EXPORT DocumentDelegateFactoryInterface_QtInterface
    {
        public:
            virtual ~DocumentDelegateFactoryInterface_QtInterface();
            virtual std::vector<DocumentDelegateFactoryInterface*> documents() = 0;
    };
}

#define DocumentDelegateFactoryInterface_QtInterface_iid "org.ossia.i-score.plugins.DocumentDelegateFactoryInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::DocumentDelegateFactoryInterface_QtInterface, DocumentDelegateFactoryInterface_QtInterface_iid)
