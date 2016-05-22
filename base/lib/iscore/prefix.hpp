#ifndef ISCORE_PREFIX_HEADER
#define ISCORE_PREFIX_HEADER
#pragma warning(push, 0) // MSVC
#pragma GCC system_header
#pragma clang system_header

#include <QSize>
#include <QComboBox>
#include <QAbstractItemModel>
#include <QMenu>
#include <QFormLayout>
#include <QLayout>
#include <QPen>
#include <QPointer>
#include <QIODevice>
#include <QFlags>
#include <QMetaType>
#include <QPair>
#include <QAction>
#include <QLineEdit>
#include <QPushButton>
#include <QColor>
#include <QGridLayout>
#include <QLabel>
#include <QApplication>
#include <QPainter>
#include <QJsonArray>
#include <QMap>
#include <QBoxLayout>
#include <QRect>
#include <QGraphicsItem>
#include <QVector>
#include <QDataStream>
#include <QStringList>
#include <QVariant>
#include <QWidget>
#include <QtGlobal>
#include <QPoint>
#include <QDebug>
#include <QJsonValue>
#include <QJsonObject>
#include <QList>
#include <QByteArray>
#include <QObject>
#include <QString>

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

#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/VariantBasedNode.hpp>
#include <iscore/tools/TreePath.hpp>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

#include <iscore/selection/Selection.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#endif
