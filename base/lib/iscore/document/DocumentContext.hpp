#pragma once
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/CommandStackFacade.hpp>
namespace iscore
{
class Document;
class CommandStack;
class SelectionStack;
class ObjectLocker;
class DocumentPlugin;

struct ISCORE_LIB_BASE_EXPORT DocumentContext
{
        DocumentContext(iscore::Document& d);
        DocumentContext(const DocumentContext&) = delete;
        DocumentContext(DocumentContext&&) = delete;
        DocumentContext& operator=(const DocumentContext&) = delete;
        DocumentContext& operator=(DocumentContext&&) = delete;

        const iscore::ApplicationContext& app;
        iscore::Document& document;
        iscore::CommandStackFacade& commandStack;
        iscore::SelectionStack& selectionStack;
        iscore::ObjectLocker& objectLocker;

        const std::vector<DocumentPlugin*>& pluginModels() const;

        template<typename T>
        T& plugin() const
        {
            using namespace std;
            const auto& pms = this->pluginModels();
            auto it = find_if(begin(pms),
                              end(pms),
                              [&](DocumentPlugin * pm)
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
                              [&](DocumentPlugin * pm)
            { return dynamic_cast<T*>(pm); });

            if(it != end(pms))
                return safe_cast<T*>(*it);
            return nullptr;
        }

};
// TODO DocumentComponents : model, pluginmodels...
}
