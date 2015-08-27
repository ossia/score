#pragma once
#include <QMetaType>
#include <QPointer>
#include <State/DynamicStateDataInterface.hpp>

/**
 * @brief The DynamicState class
 *
 * A wrapper for dynamic states defined in plug-ins. It is to be used
 * as a data member in a StateNode.
 */
class DynamicState
{
    public:
        DynamicState() = default;
        DynamicState(const DynamicState& other):
            m_data{other.m_data
                    ? other.m_data->clone(other.m_data->parent())
                    : nullptr}
        {

        }

        DynamicState(DynamicState&&) = default;
        DynamicState& operator=(const DynamicState& other)
        {
            if(&other == this)
                return *this;

            delete m_data;
            m_data = other.m_data
                    ? other.m_data->clone(other.m_data->parent())
                    : nullptr;
            return *this;
        }

        DynamicState& operator=(DynamicState&& other)
        {
            if(&other == this)
                return *this;

            delete m_data;
            m_data = other.m_data;
            other.m_data = nullptr;
            return *this;
        }

        explicit DynamicState(DynamicStateDataInterface* d):
            m_data{d}
        {

        }

        bool operator==(const DynamicState& p) const
        {
            return m_data == p.m_data;
        }

        bool operator<(const DynamicState& p) const
        {
            return false;
        }

    private:
        QPointer<DynamicStateDataInterface> m_data;
};

Q_DECLARE_METATYPE(DynamicState)
