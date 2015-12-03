#pragma once
#include <iscore/application/ApplicationComponents.hpp>

namespace iscore
{
class ApplicationComponents;
class Application;
class DocumentManager;
struct ApplicationContext
{
        explicit ApplicationContext(
                iscore::Application& app,
                const ApplicationComponents&,
                DocumentManager&);
        ApplicationContext(const ApplicationContext&) = delete;
        ApplicationContext(ApplicationContext&&) = delete;
        ApplicationContext& operator=(const ApplicationContext&) = delete;

        iscore::Application& app;
        const iscore::ApplicationComponents& components;
        DocumentManager& documents;
};
}
