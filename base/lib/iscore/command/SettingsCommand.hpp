#pragma once
#include <iscore/command/Command.hpp>
#include <boost/call_traits.hpp>
namespace iscore
{

template<typename T>
class ISCORE_LIB_BASE_EXPORT SettingsCommand :
        public iscore::Command
{
    public:
        using parameter_t = T;
        using parameter_pass_t = typename boost::call_traits<typename T::param_type>::param_type;
        SettingsCommand() = default;
        SettingsCommand(
                typename T::model_type& obj,
                parameter_pass_t newval):
            m_model{obj},
            m_new{newval}
        {
            m_old = (m_model.*T::getter)();
        }

        virtual ~SettingsCommand() = default;

        void undo() const final override
        {
            (m_model.*T::setter)(m_old);
        }

        void redo() const final override
        {
            (m_model.*T::setter)(m_new);
        }

        void update(
                typename T::model_type&,
                parameter_pass_t newval)
        {
            m_new = newval;
        }

    private:
        typename T::model_type& m_model;
        typename T::param_type m_old, m_new;
};
}

#define ISCORE_SETTINGS_COMMAND_DECL(name) \
    public: \
        using iscore::SettingsCommand<parameter_t>::SettingsCommand; \
        name() = default; \
        virtual const CommandFactoryKey& key() const override { return static_key(); } \
    static const CommandFactoryKey& static_key() \
    { \
        static const CommandFactoryKey var{#name}; \
        return var; \
    } \
    private:
