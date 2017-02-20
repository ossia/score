#ifndef ISCORE_PREFIX_HEADER
#define ISCORE_PREFIX_HEADER
#pragma warning(push, 0) // MSVC
#pragma GCC system_header
#pragma clang system_header

//
#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QByteArray>
#include <QColor>
#include <QComboBox>
#include <QDataStream>
#include <QDebug>
#include <QFlags>
#include <QFormLayout>
#include <QGridLayout>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QMap>
#include <QMenu>
#include <QMetaType>
#include <QObject>
#include <QPainter>
#include <QPair>
#include <QPen>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QWidget>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <tuple>
#include <typeinfo>
#include <utility>
#include <vector>

#include <iscore/tools/std/Optional.hpp>


#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <iscore/command/AggregateCommand.hpp>

#include <iscore/model/IdentifiedObjectMap.hpp>
#include <iscore/model/path/Path.hpp>

#include <iscore/model/tree/TreeNode.hpp>
#include <iscore/model/tree/TreePath.hpp>
#include <iscore/model/tree/VariantBasedNode.hpp>

#include <iscore/selection/Selection.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>

#endif
