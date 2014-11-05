#pragma once
#include <QtCore/QMetaType>
#include <QtCore/QString>

class BonjourRecord
{
public:
    BonjourRecord() {}
    BonjourRecord(const QString &name, const QString &regType, const QString &domain)
        : serviceName(name), registeredType(regType), replyDomain(domain)
    {}
    BonjourRecord(const char *name, const char *regType, const char *domain)
    {
        serviceName = QString::fromUtf8(name);
        registeredType = QString::fromUtf8(regType);
        replyDomain = QString::fromUtf8(domain);
    }
    QString serviceName;
    QString registeredType;
    QString replyDomain;
    bool operator==(const BonjourRecord &other) const {
        return serviceName == other.serviceName
               && registeredType == other.registeredType
               && replyDomain == other.replyDomain;
    }
};

Q_DECLARE_METATYPE(BonjourRecord)
