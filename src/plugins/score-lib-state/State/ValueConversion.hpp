#pragma once
#include <State/Value.hpp>

#include <QChar>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace State
{
namespace convert
{

template <typename To>
To value(const ossia::value& val)
{
  static_assert(sizeof(To) == -1, "Type not supported.");
}

template <>
SCORE_LIB_STATE_EXPORT QVariant value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT int value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT float value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT bool value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT double value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT QString value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT QChar value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT std::string value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT char value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT vec2f value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT vec3f value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT vec4f value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT list_t value(const ossia::value& val);
template <>
SCORE_LIB_STATE_EXPORT ossia::value_map_type value(const ossia::value& val);

SCORE_LIB_STATE_EXPORT bool convert(const ossia::value& orig, ossia::value& toConvert);

// Whether a string is text at all: valid UTF-8 with no control character
// beyond tab, CR and LF. ossia's STRING is a std::string, so a device may put
// a PNG in one, and decoding that as text destroys it.
SCORE_LIB_STATE_EXPORT bool isBinary(const QByteArray& bytes) noexcept;

// What a cell shows for bytes that are not text: the first few in hex, and the
// count.
SCORE_LIB_STATE_EXPORT QString binarySummary(const QByteArray& bytes);

// The head of a one-line summary and its "[+N lines]" marker, so a delegate
// can draw the marker in its own pen. `marker` is empty when the text fits.
struct SingleLine
{
  QString head;
  QString marker;
};
SCORE_LIB_STATE_EXPORT SingleLine splitSingleLine(const QString& text);

// Text collapsed for a one-line table cell: the first line, then a count of
// what did not fit.
SCORE_LIB_STATE_EXPORT QString toSingleLine(const QString& text);

// Whether text() has anything toSingleLine would have to fold away.
SCORE_LIB_STATE_EXPORT bool isMultiLine(const QString& text) noexcept;

// The body of a quoted string literal: backslash, double quote and the
// whitespace a single line cannot carry become escapes, so that what
// toPrettyString writes is what parseValue reads back.
SCORE_LIB_STATE_EXPORT QString escapeStringLiteral(const QString& s);

// Adornishments to allow to differentiate between different value types, e.g.
// 'a', ['a', 12], or "str" for a string.
SCORE_LIB_STATE_EXPORT QString toPrettyString(const ossia::value& val);

// Decimal notation, so that e.g. a byte count reads 137438953472 instead of
// 1.37439e+11. Falls back to the exponent form for magnitudes where decimal
// notation would be unreadable.
SCORE_LIB_STATE_EXPORT QString toPrettyString(float f);

// As the user reads it: a string without its quotes. Composite values keep the
// adorned form, having no plain one.
SCORE_LIB_STATE_EXPORT QString toDisplayString(const ossia::value& val);

// We require the type to crrectly read back (e.g. int / float / char)
// and as an optimization, since we may need it multiple times,
// we chose to leave the caller save it however he wants. Hence the specific
// API.
SCORE_LIB_STATE_EXPORT QString
textualType(const ossia::value& val); // For JSONValue serialization
SCORE_LIB_STATE_EXPORT ossia::value fromQVariant(const QVariant& val);
SCORE_LIB_STATE_EXPORT QString
prettyType(const ossia::value& val); // For display to the user, translated
SCORE_LIB_STATE_EXPORT
QString prettyType(ossia::val_type); // For display to the user, translated
SCORE_LIB_STATE_EXPORT const std::array<const QString, 11>&
ValuePrettyTypesArray(); // For display to the user, translated
SCORE_LIB_STATE_EXPORT const QStringList&
ValuePrettyTypesList(); // For display to the user, translated
SCORE_LIB_STATE_EXPORT const std::array<std::pair<QString, ossia::val_type>, 10>&
ValuePrettyTypesMap();
}
}
