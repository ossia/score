#pragma once
#include <iscore/application/ApplicationComponents.hpp>

namespace iscore
{
class ApplicationComponents;
struct ApplicationSettings;
class Settings;
class DocumentManager;
class MenubarManager;

struct ApplicationContext
{
        explicit ApplicationContext(
                const iscore::ApplicationSettings&,
                const iscore::Settings&,
                const ApplicationComponents&,
                DocumentManager&,
                iscore::MenubarManager&);
        ApplicationContext(const ApplicationContext&) = delete;
        ApplicationContext(ApplicationContext&&) = delete;
        ApplicationContext& operator=(const ApplicationContext&) = delete;

        const iscore::ApplicationSettings& applicationSettings;
        const iscore::Settings& settings;
        const iscore::ApplicationComponents& components;
        DocumentManager& documents;
        iscore::MenubarManager& menuBar;
};

// By default this is defined in iscore::Application
ISCORE_LIB_BASE_EXPORT const ApplicationContext& AppContext();
}
