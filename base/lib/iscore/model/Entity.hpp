#pragma once

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/component/Component.hpp>

namespace iscore
{
template<typename T>
class Entity :
        public IdentifiedObject<T>
{
    public:
        using IdentifiedObject<T>::IdentifiedObject;

        template<typename... Args>
        Entity(const Entity& other, Args&&... args):
            IdentifiedObject<T>{ std::forward<Args>(args)...},
            m_metadata{other.metadata()}
        {

        }

        const iscore::Components& components() const { return m_components; }
        iscore::Components& components() { return m_components; }
        const iscore::ModelMetadata& metadata() const { return m_metadata; }
        iscore::ModelMetadata& metadata() { return m_metadata; }

    private:
        iscore::Components m_components;
        ModelMetadata m_metadata;
};

}
