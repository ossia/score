#ifndef SCORE_PREFIX_HEADER
#define SCORE_PREFIX_HEADER
#pragma warning(push, 0) // MSVC
#pragma GCC system_header
#pragma clang system_header

////
#include <score/command/AggregateCommand.hpp>
#include <score/model/EntityImpl.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreePath.hpp>
#include <score/model/tree/VariantBasedNode.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/string_map.hpp>

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
#include <QGraphicsItem>
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

#include <cmath>
#include <wobjectimpl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>
#endif
