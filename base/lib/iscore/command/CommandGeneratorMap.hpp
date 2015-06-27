#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <QString>

#include <boost/mpl/list.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/for_each.hpp>

#include <map>

class CommandGenerator
{
    public:
        virtual ~CommandGenerator() = default;
        virtual iscore::SerializableCommand* operator()() const = 0;
};

template<typename T>
class CommandGeneratorImpl : public CommandGenerator
{
    public:
        iscore::SerializableCommand* operator()() const override
        {
            return new T;
        }
};


using CommandGeneratorMap = std::map<QString, std::unique_ptr<CommandGenerator>>;

template<typename CommandFactory>
struct CommandGeneratorMapInserter
{
        template< typename TheCommand > void operator()(boost::type<TheCommand>)
        {
            CommandFactory::map.insert(
                        std::move(
                            std::pair<QString, std::unique_ptr<CommandGenerator>>{
                                TheCommand::commandName(),
                                std::unique_ptr<CommandGenerator>(new CommandGeneratorImpl<TheCommand>)
                            }
                        )
            );
        }
};
