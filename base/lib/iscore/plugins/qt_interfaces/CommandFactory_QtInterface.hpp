#pragma once
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore_lib_base_export.h>
#include <utility>

#include <iscore/command/SerializableCommand.hpp>

namespace iscore
{
class ISCORE_LIB_BASE_EXPORT CommandFactory_QtInterface
{
    public:
        virtual ~CommandFactory_QtInterface();

        virtual std::pair<const CommandParentFactoryKey, CommandGeneratorMap> make_commands() = 0;
};
}


#define CommandFactory_QtInterface_iid "org.ossia.i-score.plugins.CommandFactory_QtInterface"

Q_DECLARE_INTERFACE(iscore::CommandFactory_QtInterface, CommandFactory_QtInterface_iid)
