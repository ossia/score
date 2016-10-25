#pragma once
#include <QMetaType>
#include <ossia/editor/dataspace/dataspace.hpp>
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>

#include <brigand/algorithms.hpp>
Q_DECLARE_METATYPE(ossia::unit_t)

template<>
struct TSerializer<DataStream, void, ossia::unit_t>
{
        using var_t = ossia::unit_t;

        static void readFrom(
                DataStream::Serializer& s,
                const var_t& var)
        {
            s.stream() << (quint64)var.which();

            if(var)
            {
              eggs::variants::apply([&] (auto unit) {
                s.stream() << (quint64) unit.which();
              }, var);
            }

            s.insertDelimiter();
        }

        static void writeTo(
                DataStream::Deserializer& s,
                var_t& var)
        {
            quint64 ds_which;
            s.stream() >> ds_which;

            if(ds_which != (quint64)var.npos)
            {
                quint64 unit_which;
                s.stream() >> unit_which;
                var = ossia::make_unit(ds_which, unit_which);
            }
            s.checkDelimiter();
        }
};

