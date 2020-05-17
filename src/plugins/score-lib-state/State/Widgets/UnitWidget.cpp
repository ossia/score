// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "UnitWidget.hpp"

#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/detail/for_each.hpp>
#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/dataspace/detail/dataspace_parse.hpp>

#include <QComboBox>
#include <QHBoxLayout>
#include <QMenuView/qmenuview.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(State::UnitWidget)
W_OBJECT_IMPL(State::DestinationQualifierWidget)

namespace State
{
UnitWidget::UnitWidget(Qt::Orientation orient, QWidget* parent)
    : QWidget{parent}
{
  if (orient == Qt::Horizontal)
    m_layout = new score::MarginLess<QHBoxLayout>{this};
  else
    m_layout = new score::MarginLess<QVBoxLayout>{this};

  m_dataspace = new QComboBox{this};
  m_unit = new QComboBox{this};
  m_layout->addWidget(m_dataspace);
  m_layout->addWidget(m_unit);

  // Fill dataspace. Unit is filled each time the dataspace changes
  m_dataspace->addItem(tr("None"), QVariant::fromValue(State::Unit{}));
  ossia::for_each_tagged(ossia::dataspace_u_list{}, [=](auto d) {
    // For each dataspace, add its text to the combo box
    using dataspace_type =
        typename ossia::matching_unit_u_list<typename decltype(d)::type>::type;
    ossia::string_view text
        = ossia::dataspace_traits<dataspace_type>::text()[0];

    m_dataspace->addItem(
        QString::fromUtf8(text.data(), text.size()),
        QVariant::fromValue(State::Unit{ossia::unit_t{dataspace_type{}}}));
  });

  // Signals
  connect(
      m_dataspace,
      SignalUtils::QComboBox_currentIndexChanged_int(),
      this,
      [=](int i) {
        on_dataspaceChanged(m_dataspace->itemData(i).value<State::Unit>());
      });

  connect(
      m_unit,
      SignalUtils::QComboBox_currentIndexChanged_int(),
      this,
      [=](int i) { unitChanged(m_unit->itemData(i).value<State::Unit>()); });
}

UnitWidget::UnitWidget(
    const State::Unit& u,
    Qt::Orientation orient,
    QWidget* parent)
    : UnitWidget{orient, parent}
{
  setUnit(u);
}

State::Unit UnitWidget::unit() const
{
  return m_unit->currentData().value<State::Unit>();
}

void UnitWidget::setUnit(const State::Unit& unit)
{
  QSignalBlocker b(this);
  auto& u = unit.get();
  if (u)
  {
    // First update the dataspace combobox
    m_dataspace->setCurrentIndex(u.which() + 1);

    // Then set the correct unit
    ossia::apply_nonnull(
        [=](auto dataspace) { m_unit->setCurrentIndex(dataspace.which()); },
        u.v);
  }
  else
  {
    // "None" dataspace
    m_dataspace->setCurrentIndex(0);
  }
}

void UnitWidget::on_dataspaceChanged(const State::Unit& unit)
{
  {
    QSignalBlocker _{this};
    m_unit->clear();
  }

  auto& d = unit.get();
  if (d)
  {
    {
      QSignalBlocker _{this};

      // Set to default unit, which is the first one for each type list

      // First lift ourselves in the dataspace realm
      ossia::apply_nonnull(
          [=](auto dataspace) -> void {
            // Then For each unit in the dataspace, add it to the unit
            // combobox.
            using typelist = typename ossia ::matching_unit_u_list<decltype(
                dataspace)>::type;
            ossia::for_each_tagged(typelist{}, [=](auto u) {
              using unit_type = typename decltype(u)::type;
              ossia::string_view text
                  = ossia::unit_traits<unit_type>::text()[0];

              m_unit->addItem(
                  QString::fromUtf8(text.data(), text.size()),
                  QVariant::fromValue(
                      State::Unit{ossia::unit_t{unit_type{}}}));
            });
          },
          d.v);
    }

    unitChanged(m_unit->currentData().value<State::Unit>());
  }
  else
  {
    // No unit
    unitChanged({});
  }
}
class EmptyModel final : public QAbstractItemModel
{
public:
  // QAbstractItemModel interface
public:
  QModelIndex
  index(int row, int column, const QModelIndex& parent) const override
  {
    return QModelIndex();
  }
  QModelIndex parent(const QModelIndex& child) const override
  {
    return QModelIndex();
  }
  int rowCount(const QModelIndex& parent) const override { return 0; }
  int columnCount(const QModelIndex& parent) const override { return 0; }
  QVariant data(const QModelIndex& index, int role) const override
  {
    return {};
  }
};
class UnitModel final : public QAbstractItemModel
{
public:
  struct UnitDataModel;
  struct DataspaceModel;

  struct TreeNode
  {
    TreeNode* parent{};
    virtual ~TreeNode() {}
  };

  struct AccessorModel : TreeNode
  {
    AccessorModel(ossia::destination_qualifiers a) : accessor{a} {}
    virtual ~AccessorModel() {}
    ossia::destination_qualifiers accessor;
  };

  struct UnitDataModel : TreeNode
  {
    virtual ~UnitDataModel() {}
    ossia::unit_t unit;
    std::vector<AccessorModel> accessors;
  };

  struct DataspaceModel : TreeNode
  {
    virtual ~DataspaceModel() {}
    ossia::unit_t dataspace;
    std::vector<UnitDataModel> units;
  };

  std::vector<DataspaceModel> m_data;

  UnitModel()
  {
    m_data.push_back(DataspaceModel{});

    ossia::for_each_tagged(ossia::dataspace_u_list{}, [&](auto d_t) {
      using dataspace_type = typename decltype(d_t)::type;

      DataspaceModel d;
      d.dataspace =
          typename ossia::matching_unit_u_list<dataspace_type>::type{};
      ossia::for_each_tagged(dataspace_type{}, [&](auto u_t) {
        using unit_type = typename decltype(u_t)::type;

        UnitDataModel u;
        u.unit = unit_type{};
        if constexpr (unit_type::is_multidimensional::value)
        {
          u.accessors.emplace_back(ossia::destination_qualifiers{
              ossia::destination_index{}, unit_type{}});
          for (std::size_t i = 0; i < unit_type::array_parameters().size();
               i++)
          {
            u.accessors.emplace_back(ossia::destination_qualifiers{
                ossia::destination_index{(int32_t)i}, unit_type{}});
          }
        }
        d.units.push_back(std::move(u));
      });
      m_data.push_back(std::move(d));
    });

    for (auto& d : m_data)
    {
      for (auto& u : d.units)
      {
        for (auto& a : u.accessors)
        {
          a.parent = &u;
        }
        u.parent = &d;
      }
    }
  }

  std::tuple<int, int, int> from(const ossia::destination_qualifiers& dq)
  {
    if (dq.unit)
    {
      for (std::size_t i = 0; i < m_data.size(); i++)
      {
        auto& data = m_data[i];
        for (std::size_t j = 0; j < data.units.size(); j++)
        {
          if (dq.unit == data.units[j].unit)
          {
            if (dq.accessors.empty())
            {
              return {i, j, -1};
            }
            else
            {
              return {i, j, dq.accessors[0] + 1};
            }
          }
        }
      }
    }

    return {0, -1, -1};
  }

  bool hasChildren(const QModelIndex& index) const override
  {
    if (!index.isValid())
    {
      return true;
    }
    else if (!index.parent().isValid())
    {
      // Dataspace
      if (index.row() == 0)
        return false;
      else
        return true;
    }
    else if (!index.parent().parent().isValid())
    {
      // Unit
      auto& units = m_data[index.parent().row()].units[index.row()];
      return !units.accessors.empty();
    }
    return false;
  }

  QModelIndex
  index(int row, int column, const QModelIndex& parent) const override
  {
    if (!hasIndex(row, column, parent))
      return QModelIndex{};

    if (!parent.isValid())
    {
      return createIndex(row, column, (void*)&m_data[row]);
    }
    else if (!parent.parent().isValid())
    {
      return createIndex(row, column, (void*)&m_data[parent.row()].units[row]);
    }
    else if (!parent.parent().parent().isValid())
    {
      return createIndex(
          row,
          column,
          (void*)&m_data[parent.parent().row()]
              .units[parent.row()]
              .accessors[row]);
    }
    else
    {
      return {};
    }
  }

  template <typename T, typename U>
  static int ptr_distance(const T& container, U* ptr)
  {
    int i = 0;
    for (auto& e : container)
    {
      if (&e == ptr)
        return i;
      i++;
    }
    return -1;
  }

  QModelIndex parent(const QModelIndex& child) const override
  {
    if (!child.isValid())
      return QModelIndex{};

    TreeNode* p = (TreeNode*)child.internalPointer();
    if (!p)
      return QModelIndex{};

    if (dynamic_cast<DataspaceModel*>(p))
    {
      return QModelIndex{};
    }
    else if (auto u = dynamic_cast<UnitDataModel*>(p))
    {
      auto i = ptr_distance(m_data, static_cast<DataspaceModel*>(u->parent));
      if (i != -1)
        return createIndex(i, 0, (void*)u->parent);
      else
        return QModelIndex{};
    }
    else if (auto a = dynamic_cast<AccessorModel*>(p))
    {
      auto u = static_cast<UnitDataModel*>(a->parent);
      auto d = static_cast<DataspaceModel*>(u->parent);

      auto i = ptr_distance(d->units, u);
      if (i != -1)
        return createIndex(i, 0, (void*)a->parent);
      else
        return QModelIndex{};
    }
    return QModelIndex{};
  }

  int rowCount(const QModelIndex& parent) const override
  {
    if (parent == QModelIndex{})
    {
      return m_data.size();
    }

    TreeNode* p = (TreeNode*)parent.internalPointer();
    if (!p)
    {
      return 0;
    }
    else if (auto d = dynamic_cast<DataspaceModel*>(p))
    {
      return d->units.size();
    }
    else if (auto u = dynamic_cast<UnitDataModel*>(p))
    {
      return u->accessors.size();
    }
    else
    {
      return 0;
    }
  }

  int columnCount(const QModelIndex& parent) const override { return 1; }

  QVariant data(const QModelIndex& index, int role) const override
  {
    if (role == Qt::DisplayRole)
    {
      if (!index.internalPointer())
        return {};

      auto p = (TreeNode*)index.internalPointer();
      if (auto d = dynamic_cast<DataspaceModel*>(p))
      {
        if (d->dataspace)
          return QString::fromUtf8(
              ossia::get_dataspace_text(d->dataspace).data());
        else
          return tr("None");
      }
      else if (auto u = dynamic_cast<UnitDataModel*>(p))
      {
        return QString::fromUtf8(ossia::get_unit_text(u->unit).data());
      }
      else if (auto a = dynamic_cast<AccessorModel*>(p))
      {
        if (!a->accessor.accessors.empty())
          return QChar(
              get_unit_accessors(a->accessor.unit)[a->accessor.accessors[0]]);
        else
          return QChar();
      }
    }

    return {};
  }
};

static EmptyModel& empty_model() noexcept
{
  static EmptyModel e;
  return e;
}
static UnitModel& unit_model() noexcept
{
  static UnitModel e;
  return e;
}

DestinationQualifierWidget::DestinationQualifierWidget(QWidget* parent)
    : QWidget{parent}
{
  // auto& empty = empty_model();
  auto& m = unit_model();
  m_unitMenu = new QMenuView{this};
  m_unitMenu->setModel(&m);
  connect(
      m_unitMenu,
      &QMenuView::triggered,
      this,
      &DestinationQualifierWidget::on_unitChanged);
}

void DestinationQualifierWidget::chooseQualifier()
{
  if(!m_unitMenu->isVisible())
  {
    m_unitMenu->exec(QCursor::pos());
  }
}

void DestinationQualifierWidget::on_unitChanged(const QModelIndex& idx)
{
  if (m_unitMenu->model() == &empty_model())
  {
    qualifiersChanged({});
    return;
  }

  if (!idx.isValid())
    return;

  if(idx.parent().parent().isValid())// accessor
  {
    if (auto acc = (UnitModel::AccessorModel*)idx.internalPointer())
      qualifiersChanged(acc->accessor);
  }
  else if(idx.parent().isValid())
  {
    if (auto unit = (UnitModel::UnitDataModel*)idx.internalPointer())
    qualifiersChanged(State::DestinationQualifiers{
        ossia::destination_qualifiers{{}, unit->unit}});
  }
  else
  {
    qualifiersChanged({});
  }
}
}
