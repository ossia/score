#pragma once
#include <core/application/ApplicationContext.hpp>
#include <iscore/command/CommandStackFacade.hpp>
namespace iscore
{
class Document;
class CommandStack;
class SelectionStack;
class ObjectLocker;
class DocumentPluginModel;

struct DocumentContext
{
        DocumentContext(iscore::Document& d);
        DocumentContext(const DocumentContext&) = delete;
        DocumentContext(DocumentContext&&) = delete;
        DocumentContext& operator=(const DocumentContext&) = delete;
        DocumentContext& operator=(DocumentContext&&) = delete;

        iscore::ApplicationContext app;
        iscore::Document& document;
        iscore::CommandStackFacade& commandStack;
        iscore::SelectionStack& selectionStack;
        iscore::ObjectLocker& objectLocker;

        const std::vector<DocumentPluginModel*>& pluginModels() const;

        template<typename T>
        T& plugin() const
        {
            using namespace std;
            const auto& pms = this->pluginModels();
            auto it = find_if(begin(pms),
                              end(pms),
                              [&](DocumentPluginModel * pm)
            { return dynamic_cast<T*>(pm); });

            ISCORE_ASSERT(it != end(pms));
            return *safe_cast<T*>(*it);
        }

        template<typename T>
        T* findPlugin() const
        {
            using namespace std;
            const auto& pms = this->pluginModels();
            auto it = find_if(begin(pms),
                              end(pms),
                              [&](DocumentPluginModel * pm)
            { return dynamic_cast<T*>(pm); });

            if(it != end(pms))
                return safe_cast<T*>(*it);
            return nullptr;
        }

};
// TODO DocumentComponents : model, pluginmodels...
}
