#pragma once
#include <QMetaType>
#include <ossia/editor/dataspace/dataspace.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>

#include <brigand/algorithms.hpp>
#include <brigand/functions/arithmetic.hpp>
Q_DECLARE_METATYPE(ossia::unit_t)

// First convert unit_t to an array of sizes
using unit_sizes =
  brigand::transform<
    brigand::as_list<ossia::unit_t>, brigand::bind<brigand::size, brigand::_1>>;

using unit_count = brigand::fold<
  unit_sizes,
  brigand::uint64_t<0>,
  brigand::plus<brigand::_state, brigand::_element>>;

class unit_factory
{

private:
  const std::array<uint64_t, brigand::size<ossia::unit_t>::value> indices;
  const std::array<ossia::unit_t, unit_count::value> units;

  static auto make_dataspace_index_array()
  {
    std::array<uint64_t, brigand::size<ossia::unit_t>::value> arr;

    uint64_t i = 0;
    uint64_t sum = 0;
    brigand::for_each<ossia::unit_t>([&] (auto t) {
      using dataspace_type = typename decltype(t)::type;
      arr[i] = sum;
      sum += brigand::size<dataspace_type>::value;
      i++;
    });

    return arr;
  }

  // Creation of an array where each value is the corresponding unit
  static auto make_unit_array()
  {
    std::array<ossia::unit_t, unit_count::value> arr;

    uint64_t i = 0;
    brigand::for_each<ossia::unit_t>([&] (auto t) {
      using dataspace_type = typename decltype(t)::type;
      brigand::for_each<dataspace_type>([&] (auto u) {
        using unit_type = typename decltype(u)::type;
        arr[i] = unit_type{};
        i++;
      });
    });
    return arr;
  }

public:
  unit_factory():
    indices(make_dataspace_index_array()),
    units(make_unit_array())
  {

  }

  ossia::unit_t get_unit(uint64_t dataspace, uint64_t unit) const
  {
    // Position of the dataspace + position of the unit
    // position of the dataspace is the sum of the n first in unit_sizes

    if(dataspace < indices.size())
    {
      auto idx = indices[dataspace];

      if(idx + unit < units.size())
        return units[idx + unit];
    }

    return {};
  }

};

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
            static const unit_factory units;
            quint64 ds_which;
            s.stream() >> ds_which;

            if(ds_which != (quint64)var.npos)
            {
                quint64 unit_which;
                s.stream() >> unit_which;
                var = units.get_unit(ds_which, unit_which);
            }
            s.checkDelimiter();
        }
};

