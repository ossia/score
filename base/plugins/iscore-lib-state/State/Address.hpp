#pragma once
#include <QStringList>
#include <QMetaType>

// For qRegisterDataStreamOperators...
#include <iscore/serialization/DataStreamVisitor.hpp>

namespace iscore
{
/**
 * @brief The Address struct
 *
 * Represents an address in the style of Jamoma :
 *
 *  aDevice:/aNode/anotherNode
 *
 */
struct Address
{
        // Data
        QString device; // No device means that this is the invisible root node.

        QStringList path; // Note : path is empty if address is root: "device:/"
        // In terms of iscore::Node, this means that the node is the device node.

        // Check that the given string is a valid address
        // Note: a "maybe" concept would help here.
        static bool validateString(const QString& str);

        // Make an address from a valid address string
        static Address fromString(const QString& str);
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
}

namespace std {

  template <>
  struct hash<iscore::Address>
  {
    std::size_t operator()(const iscore::Address& k) const
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
Q_DECLARE_METATYPE(iscore::Address)
