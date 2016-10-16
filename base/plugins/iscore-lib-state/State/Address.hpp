#pragma once
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <QMetaType>
#include <QMetaObject>
#include <iscore/tools/std/Optional.hpp>
#include <State/Unit.hpp>
#include <ossia/detail/destination_index.hpp>
#include <ossia/editor/state/message.hpp>
#include <iscore_lib_state_export.h>

namespace State
{
/**
 * @brief The Address struct
 *
 * Represents an address in the style of Jamoma :
 *
 *  aDevice:/aNode/anotherNode
 *
 */
struct ISCORE_LIB_STATE_EXPORT Address
{
        Address() noexcept = default;
        Address(const Address& other) noexcept;
        Address(Address&&) noexcept = default;
        Address& operator=(const Address& other) noexcept;
        Address& operator=(Address&& other) noexcept;
        Address(QString d, QStringList p) noexcept;

        // Data
        QString device; // No device means that this is the invisible root node.

        QStringList path; // Note : path is empty if address is root: "device:/"
        // In terms of Device::Node, this means that the node is the device node.

        // Check that the given string is a valid address
        // Note: a "maybe" concept would help here.
        static bool validateString(const QString& str);
        static bool validateFragment(const QString& s);

        // Make an address from a valid address string
        static Address fromString(const QString& str); // TODO return optional
        static Address rootAddress();

        /**
         * @brief toString
         * @return aDevice:/and/path if valid, else an empty string.
         */
        QString toString() const;

        /**
         * @brief toShortString
         * @return If short, "dev:/foo", else the last fifteen chars.
         */
        QString toShortString() const;

        bool operator==(const Address& a) const;
        bool operator!=(const Address& a) const;
};

using AccessorVector = ossia::destination_index;

struct ISCORE_LIB_STATE_EXPORT AddressAccessor
{
        AddressAccessor() noexcept = default;
        AddressAccessor(const AddressAccessor& other) noexcept;
        AddressAccessor(AddressAccessor&& other) noexcept;
        AddressAccessor& operator=(const AddressAccessor& other) noexcept;
        AddressAccessor& operator=(AddressAccessor&& other) noexcept;

        AddressAccessor(State::Address a) noexcept;
        AddressAccessor(State::Address a, const AccessorVector& v) noexcept;

        AddressAccessor& operator=(const Address& a);
        AddressAccessor& operator=(Address&& a);

        State::Address address;
        ossia::destination_qualifiers qualifiers;

        // Utility
        QString toString() const;
        QString toShortString() const;
        QString accessorsString() const;

        static optional<AddressAccessor> fromString(const QString& str);
        bool operator==(const AddressAccessor& other) const;
        bool operator!=(const AddressAccessor& a) const;
};

/**
 * @brief The AddressAccessorHead struct
 *
 * The head of an address : just the last aprt, e.g. "baz" in "foo:/bar/baz"
 * but with potential qualifiers
 */
struct ISCORE_LIB_STATE_EXPORT AddressAccessorHead
{
        QString name;
        ossia::destination_qualifiers qualifiers;

        QString toString() const;
};


ISCORE_LIB_STATE_EXPORT QDebug operator<<(QDebug d, const State::Address& a);
ISCORE_LIB_STATE_EXPORT QDebug operator<<(QDebug d, const ossia::destination_qualifiers& a);
ISCORE_LIB_STATE_EXPORT QDebug operator<<(QDebug d, const State::AccessorVector& a);
ISCORE_LIB_STATE_EXPORT QDebug operator<<(QDebug d, const State::AddressAccessorHead& a);
ISCORE_LIB_STATE_EXPORT QDebug operator<<(QDebug d, const State::AddressAccessor& a);
ISCORE_LIB_STATE_EXPORT QStringList stringList(const State::Address& addr);
}

namespace std {

  template <>
  struct hash<State::Address>
  {
    std::size_t operator()(const State::Address& k) const
    {
      using std::size_t;
      using std::hash;
      using std::string;

      // Compute individual hash values for first,
      // second and third and combine them using XOR
      // and bit shifting:
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
      return ((qHash(k.device)
               ^ (qHashRange(k.path.begin(), k.path.end()) << 1)) >> 1);
#else
        auto h = qHash(k.device);
        for(const auto& elt : k.path)
        {
            h = (h ^ (qHash(elt) << 1)) >> 1;
        }
        return h;
#endif
    }
  };

}
Q_DECLARE_METATYPE(State::Address)
Q_DECLARE_METATYPE(State::AddressAccessor)
