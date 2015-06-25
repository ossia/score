#pragma once
#include <QStringList>
#include <QMetaType>

namespace iscore
{
struct Address
{
        // Data
        QString device;
        QStringList path; // Note : path is empty if address is root: "device:/"


        // Check that the given string is a valid address
        // Note: a "maybe" concept would help here.
        static bool validateString(const QString& str);

        // Make an address from a valid address string
        static Address fromString(const QString& str);

        // Utility
        QString toString() const
        { return device + ":/" + path.join("/"); }

        bool operator==(const Address& a) const
        {
            return device == a.device && path == a.path;
        }
        bool operator!=(const Address& a) const
        {
            return !(*this == a);
        }

        // For qDeclareMetaTypeStreamOperators...
        friend QDataStream& operator<<(QDataStream& s, const iscore::Address& a);
        friend QDataStream& operator>>(QDataStream& s, iscore::Address& a);
};
}
Q_DECLARE_METATYPE(iscore::Address)
