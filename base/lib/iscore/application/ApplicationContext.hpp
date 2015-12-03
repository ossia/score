#pragma once
#include <iscore/application/ApplicationComponents.hpp>

namespace iscore
{
class ApplicationComponents;
class ApplicationSettings;
class DocumentManager;
struct ApplicationContext
{
        explicit ApplicationContext(
                const iscore::ApplicationSettings&,
                const ApplicationComponents&,
                DocumentManager&);
        ApplicationContext(const ApplicationContext&) = delete;
        ApplicationContext(ApplicationContext&&) = delete;
        ApplicationContext& operator=(const ApplicationContext&) = delete;

        const iscore::ApplicationSettings& settings;
        const iscore::ApplicationComponents& components;
        DocumentManager& documents;
};

// By default this is defined in iscore::Application
const ApplicationContext& AppContext();
}
