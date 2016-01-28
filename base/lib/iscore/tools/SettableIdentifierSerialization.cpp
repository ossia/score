#include <boost/optional/optional.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/tools/std/Optional.hpp>
#include <sys/types.h>

template <typename T> class Reader;
template <typename T> class Writer;

