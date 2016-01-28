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

        // Utility
        QString toString() const;

        bool operator==(const Address& a) const
        {
            return device == a.device && path == a.path;
        }
        bool operator!=(const Address& a) const
        {
            return !(*this == a);
        }
};

inline QStringList stringList(const State::Address& addr)
{
    return QStringList{} << addr.device << addr.path;
}
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
