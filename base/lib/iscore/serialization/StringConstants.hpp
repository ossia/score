#pragma once
#include <QString>
#include <iscore_lib_base_export.h>
namespace iscore
{
struct StringConstants
{
        const QString k;
        const QString v;
        const QString id;
        const QString none;
        const QString Identifiers;
        const QString Type;
        const QString Value;
        const QString Address;
        const QString value;
        const QString address;
        const QString LHS;
        const QString Op;
        const QString RHS;
        const QString Previous;
        const QString Following;
        const QString User;
        const QString Priorities;
        const QString Process;
        const QString Name;
        const QString ObjectName;
        const QString ObjectId;
        const QString Children;
        const QString Min;
        const QString Max;
        const QString Values;
        const QString Device;
        const QString Path;
        const QString ioType;
        const QString ClipMode;
        const QString Unit;
        const QString RepetitionFilter;
        const QString RefreshRate;
        const QString Priority;
        const QString Tags;
        const QString Domain;
        const QString Protocol;
        const QString Duration;
        const QString Metadata;
        const QString lowercase_true;
        const QString lowercase_false;
        const QString True;
        const QString False;
};

ISCORE_LIB_BASE_EXPORT const StringConstants& StringConstant();
}
