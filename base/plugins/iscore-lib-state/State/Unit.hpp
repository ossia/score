#pragma once
#include <QMetaType>
#include <ossia/editor/dataspace/dataspace_base_fwd.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>

#include <memory>
#include <iscore_lib_state_export.h>
namespace State
{
struct ISCORE_LIB_STATE_EXPORT Unit
{
        Q_GADGET
    public:
        Unit();
        Unit(const Unit& other);
        Unit(Unit&& other);
        Unit& operator=(const Unit& other);
        Unit& operator=(Unit&& other);
        ~Unit();

        Unit(const ossia::unit_t&);
        Unit& operator=(const ossia::unit_t&);

        operator const ossia::unit_t&() const;
        operator ossia::unit_t&();

        bool operator==(const State::Unit& other) const;
        bool operator!=(const State::Unit& other) const;

        const ossia::unit_t& get() const;
        ossia::unit_t& get();

    private:
        std::unique_ptr<ossia::unit_t> unit;
};
}

template<>
struct ISCORE_LIB_STATE_EXPORT TSerializer<DataStream, void, ossia::unit_t>
{
        static void readFrom(
                DataStream::Serializer& s,
                const ossia::unit_t& var);

        static void writeTo(
                DataStream::Deserializer& s,
                ossia::unit_t& var);
};

template<>
struct ISCORE_LIB_STATE_EXPORT TSerializer<DataStream, void, State::Unit>
{
        static void readFrom(
                DataStream::Serializer& s,
                const State::Unit& var);

        static void writeTo(
                DataStream::Deserializer& s,
                State::Unit& var);
};

Q_DECLARE_METATYPE(State::Unit)
