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
#include <boost/container/static_vector.hpp>
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
        Address(const Address& other) noexcept:
            device{other.device},
            path{other.path}
        {

        }

        Address(Address&&) noexcept = default;
        Address& operator=(const Address& other) noexcept
        {
            device = other.device;
            path = other.path;
            return *this;
        }

        Address& operator=(Address&& other) noexcept
        {
            device = std::move(other.device);
            path = std::move(other.path);
            return *this;
        }

        Address(QString d, QStringList p) noexcept :
            device{std::move(d)},
            path{std::move(p)}
        {

        }

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

        bool operator==(const Address& a) const
        {
            return device == a.device && path == a.path;
        }
        bool operator!=(const Address& a) const
        {
            return !(*this == a);
        }
};

ISCORE_LIB_STATE_EXPORT QDebug operator<<(QDebug d, const State::Address& a);

inline QStringList stringList(const State::Address& addr)
{
    return QStringList{} << addr.device << addr.path;
}

using AccessorVector = boost::container::static_vector<char, 8>;
struct ISCORE_LIB_STATE_EXPORT AddressAccessor
{
        AddressAccessor() noexcept = default;
        AddressAccessor(const AddressAccessor& other) noexcept:
            address{other.address},
            accessors{other.accessors}
        {

        }
        AddressAccessor(AddressAccessor&& other) noexcept:
            address{std::move(other.address)},
            accessors{std::move(other.accessors)}
        {

        }
        AddressAccessor& operator=(const AddressAccessor& other) noexcept
        {
            address = other.address;
            accessors = other.accessors;
            return *this;
        }
        AddressAccessor& operator=(AddressAccessor&& other) noexcept
        {
            address = std::move(other.address);
            accessors = std::move(other.accessors);
            return *this;
        }

        AddressAccessor(State::Address a) noexcept :
            address{std::move(a)} { }
        AddressAccessor(State::Address a, const AccessorVector& v) noexcept :
            address{std::move(a)},
            accessors{v}
        {

        }

        AddressAccessor& operator=(const Address& a)
        {
            address = a;
            accessors.clear();
            return *this;
        }

        AddressAccessor& operator=(Address&& a)
        {
            address = std::move(a);
            accessors.clear();
            return *this;
        }

        State::Address address;
        AccessorVector accessors;

        // Utility
        QString toString() const;
        QString toShortString() const;
        QString accessorsString() const;

        static optional<AddressAccessor> fromString(const QString& str);
        bool operator==(const AddressAccessor& other) const
        {
            return address == other.address && accessors == other.accessors;
        }
        bool operator!=(const AddressAccessor& a) const
        {
            return !(*this == a);
        }
};
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
