#pragma once
#include <QVariant>

class DataType { };
class Impulse : public DataType { }; // ignore QVariant
class Boolean : public DataType { }; // QVariant = bool
class Integer : public DataType { }; // QVariant = uint32
class Decimal : public DataType { }; // QVariant = float
class String  : public DataType { }; // QVariant = std::string
class Tuple   : public DataType { }; // QVariant = std::tuple ou struct...

class Domain { };
enum class AccessMode
{
    Get, Set, Both
};
enum class BoundingMode
{
    Free, Clip, Wrap, Fold
};
struct Message
{
    QString address;
    QVariant value; // On peut utiliser val.type() et val.canConvert() directement...
    DataType type; // ?

    AccessMode mode {AccessMode::Both};
    BoundingMode minBoundingMode;
    BoundingMode maxBoundingMode;

    Domain domain;
};
