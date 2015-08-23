#pragma once
#include <QVariant>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

namespace iscore {
class State
{
        ISCORE_SERIALIZE_FRIENDS(iscore::State, DataStream)
        ISCORE_SERIALIZE_FRIENDS(iscore::State, JSONObject)

    public:
        State() = default;
        ~State() = default;
        State(const State&) = default;
        State(State&&) = default;
        State& operator=(const State&) = default;
        State& operator=(State&&) = default;

        template<typename T>
        explicit State(const T& t):
            m_data{QVariant::fromValue(t)}
        {

        }

        State(const QVariant& variant):
            m_data{variant}
        {
        }

        const QVariant& data() const
        {
            return m_data;
        }

        bool operator==(const State& s) const
        {
            return m_data == s.m_data;
        }

        bool operator!=(const State& s) const
        {
            return m_data != s.m_data;
        }

    private:
        QVariant m_data;
};

using StateList = QList<State>;
}
Q_DECLARE_METATYPE(iscore::State)
Q_DECLARE_METATYPE(iscore::StateList)


std::size_t hash_value(const iscore::State& state) noexcept;

struct state_hash
{
    std::size_t operator()(const iscore::State& state) const noexcept
    {
        return hash_value(state);
    }
};
/*
#include "Message.hpp"
#include "ProcessState.hpp"
class StateNode
{
    public:
        enum class Type { RootNode, Container, Messages, Dynamic  }; // Container only has child

        StateNode();

        StateNode(InvisibleRootStateNodeTag);
        bool isInvisibleRoot() const;

        // Address
        StateNode(const iscore::AddressSettings& settings,
             StateNode* parent = nullptr);

        // Device
        StateNode(const DeviceSettings& settings,
             StateNode* parent = nullptr);
        bool isDevice() const;

        // Clone
        StateNode(const StateNode& source,
             StateNode* parent = nullptr);
        StateNode& operator=(const StateNode& source);

        ~StateNode();

        void setParent(StateNode* parent);
        StateNode* parent() const;
        StateNode* childAt(int index) const;  //return 0 if invalid index
        int indexOfChild(const StateNode* child) const;  //return -1 if not found
        int childCount() const;
        bool hasChildren() const;
        QList<StateNode*> children() const;

        void insertChild(int index, StateNode* n);
        void addChild(StateNode* n);
        void swapChildren(int oldIndex, int newIndex);
        StateNode* takeChild(int index);
        void removeChild(StateNode* child);

        Type type() const { return m_type; }
    private:
        union {
                bool m_rootNode;
                bool m_container;
                iscore::MessageList m_messages;
                ProcessState* m_dynState;
        };

        Type m_type{};
};
*/
