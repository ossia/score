#pragma once
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/CommandStackFacade.hpp>
#include <iscore/selection/FocusManager.hpp>
#include <boost/optional.hpp>
class IdentifiedObjectAbstract;
namespace iscore
{
class Document;
class CommandStack;
class SelectionStack;
class ObjectLocker;
class DocumentPlugin;
struct ISCORE_LIB_BASE_EXPORT DocumentContext
{
        friend class iscore::Document;
        static DocumentContext fromDocument(iscore::Document& d);

        const iscore::ApplicationContext& app;
        iscore::Document& document;
        const iscore::CommandStackFacade commandStack;
        iscore::SelectionStack& selectionStack;
        iscore::ObjectLocker& objectLocker;
        const iscore::FocusFacade focus;

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

    protected:
        DocumentContext(iscore::Document& d);
        DocumentContext(const DocumentContext&) = default;
        DocumentContext(DocumentContext&&) = default;
        DocumentContext& operator=(const DocumentContext&) = default;
        DocumentContext& operator=(DocumentContext&&) = default;
};

using MaybeDocument = boost::optional<const iscore::DocumentContext&>;
}
