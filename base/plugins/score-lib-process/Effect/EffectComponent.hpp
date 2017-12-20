#pragma once
#include <Effect/EffectModel.hpp>

namespace Process
{

template<typename EffectBase_T, typename Component_T>
class EffectComponentBase :
        public Component_T
{
    public:
        template<typename... Args>
        EffectComponentBase(EffectBase_T& cst, Args&&... args):
            Component_T{std::forward<Args>(args)...},
            m_effect{cst}
        {

        }

        EffectBase_T& effect() const
        { return m_effect; }

    private:
        EffectBase_T& m_effect;
};

template<typename Component_T>
using EffectComponent = EffectComponentBase<Process::EffectModel, Component_T>;

template<typename System_T>
using GenericEffectComponent =
    Process::EffectComponent<score::GenericComponent<System_T>>;



template<typename EffectComponentBase_T, typename Effect_T>
class GenericEffectComponent_T : public EffectComponentBase_T
{
    public:
        using model_type = Effect_T;
        using EffectComponentBase_T::EffectComponentBase_T;

        const Effect_T& effect() const
        { return static_cast<const Effect_T&>(EffectComponentBase_T::effect()); }
};

}
