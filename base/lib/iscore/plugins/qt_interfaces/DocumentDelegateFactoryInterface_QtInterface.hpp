#pragma once

#include <vector>

namespace iscore
{
    class DocumentDelegateFactoryInterface;

    class DocumentDelegateFactoryInterface_QtInterface
    {
        public:
            virtual ~DocumentDelegateFactoryInterface_QtInterface();
            virtual std::vector<DocumentDelegateFactoryInterface*> documents() = 0;
    };
}

#define DocumentDelegateFactoryInterface_QtInterface_iid "org.ossia.i-score.plugins.DocumentDelegateFactoryInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::DocumentDelegateFactoryInterface_QtInterface, DocumentDelegateFactoryInterface_QtInterface_iid)
