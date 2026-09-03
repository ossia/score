#include "AddressItemModel.hpp"

#include <State/Expression.hpp>
#include <State/ValueConversion.hpp>
#include <State/Widgets/UnitWidget.hpp>
#include <State/Widgets/Values/ExpandableTextEdit.hpp>

#include <Explorer/Commands/Update/UpdateAddressSettings.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/serialization/AnySerialization.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/IntSlider.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_traits.hpp>

#include <ossia-qt/metatypes.hpp>

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QPointer>
#include <QSpinBox>
#include <QTimer>

#include <wobjectimpl.h>

Q_DECLARE_METATYPE(ossia::net::tags)
W_OBJECT_IMPL(Explorer::AddressItemModel)

namespace Explorer
{
AddressItemModel::AddressItemModel(QObject* parent)
    : QAbstractItemModel{parent}
{
}

void AddressItemModel::setState(
    DeviceExplorerModel* model, Device::NodePath nodepath,
    const Device::FullAddressSettings& s)
{
  beginResetModel();
  m_model = model;
  m_path = nodepath;
  m_settings = s;

  endResetModel();
}

void AddressItemModel::clear()
{
  beginResetModel();
  m_settings = {};
  m_model = nullptr;
  m_path = {};
  endResetModel();
}

bool AddressItemModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  namespace onet = ossia::net;
  if(index.column() != 1)
    return false;

  if(role != Qt::EditRole && role != Qt::CheckStateRole)
    return false;

  if(index.row() < 0 || index.row() >= rowCount({}))
    return false;

  if(!m_model)
    return false;

  if(m_settings.address.path.empty())
    return false; // TODO rename the device itself

  auto* node = static_cast<Device::Node*>(
      m_model->convertPathToIndex(m_path).internalPointer());
  if(!node || !node->is<Device::AddressSettings>() || !node->parent())
    return false;

  const auto address = Device::address(*node).address;

  const bool checked = (role == Qt::CheckStateRole)
                           ? (static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked)
                           : value.toBool();

  const Device::AddressSettings before = node->get<Device::AddressSettings>();
  Device::AddressSettings after = before;

  switch(index.row())
  {
    case Rows::Value: {
      if(role == Qt::CheckStateRole && before.value.target<bool>())
      {
        after.value = checked;
        m_model->deviceModel().updateProxy.updateRemoteValue(address, after.value);
        pushedValue(address, after.value);
        return true;
      }
      else if(value.canConvert<ossia::value>())
      {
        after.value = value.value<ossia::value>();

        // Note : if we want to disable remote updating, we have to do it
        // here (e.g. if this becomes a settings)
        m_model->deviceModel().updateProxy.updateRemoteValue(address, after.value);
        pushedValue(address, after.value);
        return true;
      }
      else
      {
        // In this case we don't make a command, but we directly push the
        // new value.
        auto copy = State::convert::fromQVariant(value);

        // We may have to convert types.
        const ossia::value& orig = before.value;
        if(copy.v.which() != orig.v.which() && !State::convert::convert(orig, copy))
          return false;

        after.value = copy;

        // Note : if we want to disable remote updating, we have to do it
        // here (e.g. if this becomes a settings)
        m_model->deviceModel().updateProxy.updateRemoteValue(address, copy);
        pushedValue(address, after.value);
        return true;
      }
    }

    case Rows::Name: {
      const auto name = value.toString();
      if(name.isEmpty() || !m_model->canRenameNode(*node))
        return false;
      after.name = name;
      break;
    }

    case Rows::Type: {
      const auto type = value.value<ossia::val_type>();
      after.value = ossia::convert(before.value, type);

      if(after.value.get_type() != before.value.get_type())
        after.domain = ossia::init_domain(type);
      break;
    }

    case Rows::Min: {
      if(value.canConvert<ossia::value>())
      {
        after.domain.get().set_min(value.value<ossia::value>());
      }
      else
      {
        // In this case we don't make a command, but we directly push the
        // new value.
        auto copy = State::convert::fromQVariant(value);

        // We may have to convert types.
        const ossia::value& orig = before.value;
        if(copy.v.which() != orig.v.which() && !State::convert::convert(orig, copy))
          return false;

        after.domain.get().set_min(copy);
      }
      break;
    }
    case Rows::Max: {
      if(value.canConvert<ossia::value>())
      {
        after.domain.get().set_max(value.value<ossia::value>());
      }
      else
      {
        // In this case we don't make a command, but we directly push the
        // new value.
        auto copy = State::convert::fromQVariant(value);

        // We may have to convert types.
        const ossia::value& orig = before.value;
        if(copy.v.which() != orig.v.which() && !State::convert::convert(orig, copy))
          return false;

        after.domain.get().set_max(copy);
      }
      break;
    }
    case Rows::Values: {
      auto vals = State::convert::value<std::vector<ossia::value>>(
          value.canConvert<ossia::value>() ? value.value<ossia::value>()
                                           : State::convert::fromQVariant(value));

      if(before.value.valid())
      {
        for(auto& v : vals)
        {
          if(v.get_type() != before.value.get_type())
            State::convert::convert(before.value, v);
        }
      }

      auto& dom = after.domain.get();
      if(dom)
      {
        ossia::set_values(dom, vals);
      }
      else if(!vals.empty())
      {
        // ossia has no domain for every type, and building an unknown one throws.
        auto fresh = ossia::init_domain(vals.front().get_type());
        if(!fresh)
          return false;
        ossia::set_values(fresh, vals);
        dom = std::move(fresh);
      }
      break;
    }
    case Rows::Access: {
      after.ioType = (ossia::access_mode)value.toInt();
      break;
    }
    case Rows::Bounding: {
      after.clipMode = (ossia::bounding_mode)value.toInt();
      break;
    }
    case Rows::Repetition: {
      after.repetitionFilter = value.toInt() != 0 ? ossia::repetition_filter::ON
                                                  : ossia::repetition_filter::OFF;
      break;
    }
    case Rows::Unit: {
      after.unit.get() = value.value<State::Unit>().get();
      break;
    }
    default: {
      int idx = index.row() - Rows::Count;
      if(ossia::valid_index(idx, after.extendedAttributes))
      {
        auto it = after.extendedAttributes.begin();
        std::advance(it, idx);
        if(it->first == onet::text_description())
        {
          if(auto* cur = ossia::any_cast<onet::description>(&it->second);
             cur && *cur == value.toString().toStdString())
            return false;
          it->second = value.toString().toStdString();
        }
        else if(it->first == onet::text_tags())
        {
          onet::tags tags;
          for(const auto& tag : value.toString().split(',', Qt::SkipEmptyParts))
          {
            const auto trimmed = tag.trimmed();
            if(!trimmed.isEmpty())
              tags.push_back(trimmed.toStdString());
          }

          if(auto* cur = ossia::any_cast<onet::tags>(&it->second); cur && *cur == tags)
            return false;
          it->second = std::move(tags);
        }
        else if(it->first == onet::text_default_value())
        {
          if(value.canConvert<ossia::value>())
          {
            it->second = value.value<ossia::value>();
          }
        }
        else if(it->first == onet::text_refresh_rate())
        {
          if(auto* cur = ossia::any_cast<onet::refresh_rate_attribute::type>(&it->second);
             cur && *cur == value.toInt())
            return false;
          it->second = value.toInt();
        }
        else if(it->first == onet::text_value_step_size())
        {
          it->second = value.toDouble();
        }
        else if(it->first == onet::text_priority())
        {
          it->second = value.toInt();
        }
      }
    }
  }

  // Extended attributes are excluded: ossia::any does not compare.
  if(index.row() < Rows::Count && after == before)
    return false;

  if(!m_model->checkAddressEditable(*node->parent(), before, after))
    return false;

  CommandDispatcher<> disp{m_model->commandStack()};
  disp.submit(new Explorer::Command::UpdateAddressSettings{
      m_model->deviceModel(), m_path, after});

  return true;
}

QModelIndex AddressItemModel::index(int row, int column, const QModelIndex& parent) const
{
  if(parent == QModelIndex{})
  {
    return createIndex(row, column, nullptr);
  }
  return {};
}

QModelIndex AddressItemModel::parent(const QModelIndex&) const
{
  return {};
}

int AddressItemModel::rowCount(const QModelIndex&) const
{
  if(m_settings.address.device.isEmpty())
    return 0;

  if(!m_settings.value.valid())
    return 2;

  return Rows::Count + extendedCount();
}

int AddressItemModel::columnCount(const QModelIndex&) const
{
  return 2;
}

QVariant AddressItemModel::valueColumnData(const State::Value& val, int role) const
{
  if(role == Qt::DisplayRole || role == Qt::EditRole)
  {
    if(ossia::is_array(val))
    {
      return State::convert::toPrettyString(val);
    }
    else if(role == Qt::DisplayRole && val.get_type() == ossia::val_type::STRING)
    {
      return State::convert::stringCellText(
          QByteArray::fromStdString(*val.target<std::string>()));
    }
    else
    {
      return State::convert::value<QVariant>(val);
    }
  }
  else if(role == Qt::ToolTipRole)
  {
    // What the one-line row had to fold away.
    if(const auto* s = val.target<std::string>())
    {
      const auto tip = State::convert::stringCellToolTip(QByteArray::fromStdString(*s));
      if(!tip.isEmpty())
        return tip;
    }
  }

  return {};
}

QVariant AddressItemModel::data(const QModelIndex& index, int role) const
{
  namespace onet = ossia::net;
  if(role == Qt::DisplayRole)
  {
    if(!m_settings.value.valid())
    {
      switch(index.column())
      {
        case 0: {
          switch(index.row())
          {
            case Rows::Name:
              return tr("Name");
            case Rows::Address:
              return tr("Address");
            default:
              break;
          }
          break;
        }
        case 1: {
          switch(index.row())
          {
            case Rows::Name:
              return m_settings.address.path.last();
            case Rows::Address:
              return m_settings.address.toString();
            default:
              break;
          }
          break;
        }
        default:
          break;
      }
      return {};
    }

    switch(index.column())
    {
      case 0: {
        switch(index.row())
        {
          case Rows::Name:
            return tr("Name");
          case Rows::Address:
            return tr("Address");
          case Rows::Value:
            return tr("Value");
          case Rows::Type:
            return tr("Type");
          case Rows::Min:
            return tr("Min");
          case Rows::Max:
            return tr("Max");
          case Rows::Values:
            return tr("Values");
          case Rows::Unit:
            return tr("Unit");
          case Rows::Access:
            return tr("Access");
          case Rows::Bounding:
            return tr("Bounding");
          case Rows::Repetition:
            return tr("Repetition");
          default: {
            int idx = index.row() - Rows::Count;
            if(ossia::valid_index(idx, m_settings.extendedAttributes))
            {
              auto it = m_settings.extendedAttributes.begin();
              std::advance(it, idx);
              auto str = QString::fromStdString(it->first);
              if(!str.isEmpty())
                str[0] = str[0].toUpper();
              for(int i = 1; i < str.size(); i++)
              {
                if(str[i].isUpper())
                {
                  str.insert(i, ' ');
                  i++;
                }
              }
              return str;
            }
          }
        }

        break;
      }
      case 1: {
        switch(index.row())
        {
          case Rows::Name:
            return m_settings.address.path.last();
          case Rows::Address:
            return m_settings.address.toString();
          case Rows::Value:
            return valueColumnData(m_settings.value, role);
          case Rows::Type: {
            return State::convert::ValuePrettyTypesArray()[(int)m_settings.value
                                                               .get_type()];
          }
          case Rows::Min: {
            return valueColumnData(ossia::get_min(m_settings.domain.get()), role);
          }
          case Rows::Max: {
            return valueColumnData(ossia::get_max(m_settings.domain.get()), role);
          }
          case Rows::Values: {
            return valueColumnData(ossia::get_values(m_settings.domain.get()), role);
          }
          case Rows::Unit: {
            return State::prettyUnitText(m_settings.unit.get());
          }
          case Rows::Access: {
            if(m_settings.ioType)
            {
              return Device::AccessModePrettyText()[*m_settings.ioType];
            }
            else
            {
              return tr("None");
            }
          }
          case Rows::Bounding: {
            return Device::ClipModePrettyStringMap()[m_settings.clipMode];
          }
          case Rows::Repetition: {
            return m_settings.repetitionFilter == ossia::repetition_filter::ON
                       ? tr("Filtered")
                       : tr("Unfiltered");
          }
          default: {
            int idx = index.row() - Rows::Count;
            if(ossia::valid_index(idx, m_settings.extendedAttributes))
            {
              auto it = m_settings.extendedAttributes.begin();
              std::advance(it, idx);
              if(it->first == onet::text_description())
              {
                return State::convert::toSingleLine(QString::fromStdString(
                    ossia::any_cast<onet::description>(it->second)));
              }
              else if(it->first == onet::text_tags())
              {
                const auto& tags = ossia::any_cast<onet::tags>(it->second);

                QStringList l;
                for(const auto& s : tags)
                  l += QString::fromStdString(s);
                return l.join(", ");
              }
              else if(it->first == onet::text_default_value())
              {
                const auto& v
                    = ossia::any_cast<onet::default_value_attribute::type>(it->second);
                return valueColumnData(v, role);
              }
              else if(it->first == onet::text_refresh_rate())
              {
                return ossia::any_cast<onet::refresh_rate_attribute::type>(it->second);
              }
              else if(it->first == onet::text_value_step_size())
              {
                return ossia::any_cast<onet::value_step_size_attribute::type>(
                    it->second);
              }
              else if(it->first == onet::text_priority())
              {
                return ossia::any_cast<onet::priority_attribute::type>(it->second);
              }
            }
            return {};
          }
        }

        break;
      }
      default:
        break;
    }
  }
  else if(role == Qt::EditRole)
  {
    if(index.column() == 1)
    {
      switch(index.row())
      {
        case Rows::Name:
          return m_settings.address.path.last();
        case Rows::Type:
          return (int)m_settings.value.get_type();
        case Rows::Access:
          return m_settings.ioType ? (int)*m_settings.ioType : -1;
        case Rows::Bounding:
          return (int)m_settings.clipMode;
        case Rows::Unit:
          return QVariant::fromValue(m_settings.unit);
        case Rows::Value:
          return QVariant::fromValue(m_settings.value);
        case Rows::Min:
          return QVariant::fromValue(ossia::get_min(m_settings.domain.get()));
        case Rows::Max:
          return QVariant::fromValue(ossia::get_max(m_settings.domain.get()));
        case Rows::Values:
          // As one value, so that the editor reads it like any other.
          return QVariant::fromValue(
              ossia::value{ossia::get_values(m_settings.domain.get())});
        default: {
          int idx = index.row() - Rows::Count;
          if(ossia::valid_index(idx, m_settings.extendedAttributes))
          {
            auto it = m_settings.extendedAttributes.begin();
            std::advance(it, idx);
            if(it->first == onet::text_description())
            {
              return QString::fromStdString(
                  ossia::any_cast<onet::description>(it->second));
            }
            else if(it->first == onet::text_tags())
            {
              return QVariant::fromValue(ossia::any_cast<onet::tags>(it->second));
            }
            else if(it->first == onet::text_default_value())
            {
              return QVariant::fromValue(
                  ossia::any_cast<onet::default_value_attribute::type>(it->second));
            }
            else if(it->first == onet::text_refresh_rate())
            {
              return ossia::any_cast<onet::refresh_rate_attribute::type>(it->second);
            }
            else if(it->first == onet::text_value_step_size())
            {
              return ossia::any_cast<onet::value_step_size_attribute::type>(it->second);
            }
            else if(it->first == onet::text_priority())
            {
              return ossia::any_cast<onet::priority_attribute::type>(it->second);
            }
          }
          return {};
        }
      }
    }
    else
    {
      return {};
    }
  }
  else if(role == Qt::ToolTipRole)
  {
    // What a one-line row had to fold away, and nothing otherwise.
    if(index.column() != 1)
      return {};

    if(index.row() == Rows::Value)
      return valueColumnData(m_settings.value, role);

    const int idx = index.row() - Rows::Count;
    if(ossia::valid_index(idx, m_settings.extendedAttributes))
    {
      auto it = m_settings.extendedAttributes.begin();
      std::advance(it, idx);
      if(it->first == onet::text_description())
      {
        const auto text
            = QString::fromStdString(ossia::any_cast<onet::description>(it->second));
        if(State::convert::isMultiLine(text))
          return text;
      }
      else if(it->first == onet::text_tags())
      {
        return tr("One tag per comma");
      }
    }
    return {};
  }
  else if(role == Qt::CheckStateRole)
  {
    if(index.column() == 1)
    {
      switch(index.row())
      {
        case Rows::Repetition:
          return m_settings.repetitionFilter == ossia::repetition_filter::ON
                     ? Qt::Checked
                     : Qt::Unchecked;
        case Rows::Value: {
          if(auto b = m_settings.value.target<bool>())
          {
            return *b ? Qt::Checked : Qt::Unchecked;
          }
        }
        default:
          break;
      }
    }
  }

  return {};
}

bool AddressItemModel::editableProperties() const
{
  if(!m_model)
    return false;
  auto* dev = m_model->deviceModel().list().findDevice(m_settings.address.device);
  return dev && dev->capabilities().canSetProperties;
}

bool AddressItemModel::editableName() const
{
  if(!m_model)
    return false;

  auto* node = static_cast<Device::Node*>(
      m_model->convertPathToIndex(m_path).internalPointer());
  return node && m_model->canRenameNode(*node);
}

void AddressItemModel::pushedValue(const State::Address& addr, const ossia::value& v)
{
  m_settings.value = v;
  const auto idx = index(Rows::Value, 1, {});
  dataChanged(idx, idx);

  // Deferred: the tree pushes the change back here and resets this model.
  // An impulse is not a state: there is nothing to mirror.
  if(v.get_type() == ossia::val_type::IMPULSE)
    return;

  QPointer self{this};
  QTimer::singleShot(0, this, [self, addr, v] {
    if(self && self->m_model)
      self->m_model->deviceModel().updateProxy.updateLocalValue(
          State::AddressAccessor{addr}, v);
  });
}

int AddressItemModel::extendedCount() const noexcept
{
  return int(m_settings.extendedAttributes.size());
}

Qt::ItemFlags AddressItemModel::flags(const QModelIndex& index) const
{
  if(index.column() == 0)
    return {Qt::ItemIsEnabled};

  Qt::ItemFlags f = QAbstractItemModel::flags(index);
  static const constexpr std::array<Qt::ItemFlags, Rows::Count> flags{{
      {Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable} // name
      ,
      {Qt::ItemIsEnabled | Qt::ItemIsSelectable} // address
      ,
      {Qt::ItemIsEditable} // value
      ,
      {Qt::ItemIsEditable} // type
      ,
      {Qt::ItemIsEditable} // min
      ,
      {Qt::ItemIsEditable} // max
      ,
      {Qt::ItemIsEditable} // values
      ,
      {Qt::ItemIsEditable} // unit
      ,
      {Qt::ItemIsEditable} // access
      ,
      {Qt::ItemIsEditable} // bounding
      ,
      {Qt::ItemIsUserCheckable | Qt::ItemIsEnabled} // repetition
  }};

  if(index.row() < Rows::Count)
    f |= flags[index.row()];
  else
    f |= Qt::ItemIsEditable;

  // Every row but the value describes the parameter rather than holding it.
  if(index.row() != Rows::Value && !editableProperties())
    f &= ~Qt::ItemIsEditable;

  if(index.row() == Rows::Name && !editableName())
    f &= ~Qt::ItemIsEditable;

  if(index.row() == Value)
  {
    if(m_settings.value.target<bool>())
    {
      f |= Qt::ItemIsUserCheckable;
      f |= Qt::ItemIsEnabled;
    }
  }

  return f;
}

namespace
{
namespace onet = ossia::net;

//! The extended attribute a row past Rows::Count stands for, or nothing.
const ossia::extended_attributes::value_type*
extendedAttributeAt(const AddressItemModel& model, const QModelIndex& index)
{
  const auto& attrs = model.settings().extendedAttributes;
  const int idx = index.row() - AddressItemModel::Rows::Count;
  if(!ossia::valid_index(idx, attrs))
    return nullptr;

  auto it = attrs.begin();
  std::advance(it, idx);
  return &*it;
}
}

AddressItemDelegate::AddressItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

AddressItemDelegate::~AddressItemDelegate() { }

void AddressItemDelegate::paint(
    QPainter* painter, const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
  QStyledItemDelegate::paint(painter, option, index);
}

QWidget* AddressItemDelegate::createEditor(
    QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  auto model = qobject_cast<const AddressItemModel*>(index.model());
  if(index.column() == 0 || !model)
    return QStyledItemDelegate::createEditor(parent, option, index);

  switch(index.row())
  {
    case AddressItemModel::Rows::Type: {
      auto t = new State::TypeComboBox{parent};
      t->set(index.data(Qt::EditRole).value<ossia::val_type>());
      return t;
    }
    case AddressItemModel::Rows::Access: {
      auto t = new Explorer::AccessModeComboBox{parent};
      t->set(index.data(Qt::EditRole).value<ossia::access_mode>());
      return t;
    }
    case AddressItemModel::Rows::Bounding: {
      auto t = new Explorer::BoundingModeComboBox{parent};
      t->set(index.data(Qt::EditRole).value<ossia::bounding_mode>());
      return t;
    }
    case AddressItemModel::Rows::Unit: {
      auto t = new State::UnitWidget{Qt::Horizontal, parent};
      t->setUnit(index.data(Qt::EditRole).value<State::Unit>());
      return t;
    }
    case AddressItemModel::Rows::Value: {
      if(auto t = make_value_widget(model->settings(), parent))
      {
        if(t->commitsImmediately())
        {
          auto* self = const_cast<AddressItemDelegate*>(this);
          connect(t, &AddressValueWidget::changed, self, [self, t](const ossia::value&) {
            self->commitData(t);
          });
        }
        return t;
      }
      break;
    }
    case AddressItemModel::Rows::Min:
    case AddressItemModel::Rows::Max: {
      if(auto t = make_bound_widget(model->settings(), parent))
        return t;
      break;
    }
    case AddressItemModel::Rows::Values: {
      return make_values_widget(model->settings(), parent);
    }
    default: {
      // Rows past Rows::Count are the extended attributes; Qt's stock factory
      // gives prose a one-line field and an ossia::value no editor at all.
      auto* attr = extendedAttributeAt(*model, index);
      if(!attr)
        break;

      if(attr->first == onet::text_description())
      {
        auto* t = new State::ExpandableTextEdit{parent};
        t->setSubject(QObject::tr("Description"));
        t->setFullText(index.data(Qt::EditRole).toString());
        return t;
      }

      if(attr->first == onet::text_default_value())
      {
        // The default is a value of the parameter's own type, so it gets the
        // value row's editor, domain and unit included.
        Device::AddressSettingsCommon as = model->settings();
        if(auto v = index.data(Qt::EditRole).value<ossia::value>(); v.valid())
          as.value = v;

        // Otherwise the row would offer to reset the default to itself.
        as.extendedAttributes.erase(onet::text_default_value());

        if(auto* t = make_value_widget(as, parent))
          return t;
      }
      break;
    }
  }

  return QStyledItemDelegate::createEditor(parent, option, index);
}

void AddressItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
  if(index.column() == 0)
  {
    QStyledItemDelegate::setEditorData(editor, index);
    return;
  }

  switch(index.row())
  {
    case AddressItemModel::Rows::Type: {
      if(auto cb = qobject_cast<State::TypeComboBox*>(editor))
      {
        auto cur = index.data(Qt::EditRole).toInt();
        if(cur >= 0 && cur < (int)State::convert::ValuePrettyTypesArray().size())
          cb->set((ossia::val_type)cur);
        return;
      }
      break;
    }
    case AddressItemModel::Rows::Access: {
      if(auto cb = qobject_cast<Explorer::AccessModeComboBox*>(editor))
      {
        auto cur = index.data(Qt::EditRole).toInt();
        if(const int max = Device::AccessModePrettyText().size(); cur >= 0 && cur < max)
          cb->set((ossia::access_mode)cur);
        return;
      }
      break;
    }
    case AddressItemModel::Rows::Bounding: {
      if(auto cb = qobject_cast<Explorer::BoundingModeComboBox*>(editor))
      {
        auto cur = index.data(Qt::EditRole).toInt();

        if(ossia::valid_index(cur, Device::ClipModePrettyStringMap()))
          cb->set((ossia::bounding_mode)cur);
        return;
      }
      break;
    }
    case AddressItemModel::Rows::Unit: {
      if(auto cb = qobject_cast<State::UnitWidget*>(editor))
      {
        auto cur = index.data(Qt::EditRole).value<State::Unit>();
        cb->setUnit(cur);
        return;
      }
      break;
    }
    case AddressItemModel::Rows::Value:
    case AddressItemModel::Rows::Min:
    case AddressItemModel::Rows::Max:
    case AddressItemModel::Rows::Values: {
      if(auto cb = qobject_cast<AddressValueWidget*>(editor))
      {
        auto cur = index.data(Qt::EditRole).value<ossia::value>();
        cb->set(cur);
        return;
      }
      break;
    }
    default: {
      if(auto t = qobject_cast<State::ExpandableTextEdit*>(editor))
      {
        t->setFullText(index.data(Qt::EditRole).toString());
        return;
      }
      if(auto cb = qobject_cast<AddressValueWidget*>(editor))
      {
        cb->set(index.data(Qt::EditRole).value<ossia::value>());
        return;
      }
      break;
    }
  }

  QStyledItemDelegate::setEditorData(editor, index);
}

void AddressItemDelegate::setModelData(
    QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
  if(index.column() == 0)
  {
    QStyledItemDelegate::setModelData(editor, model, index);
    return;
  }

  switch(index.row())
  {
    case AddressItemModel::Rows::Type: {
      if(auto cb = qobject_cast<State::TypeComboBox*>(editor))
      {
        model->setData(index, cb->itemData(cb->currentIndex()), Qt::EditRole);
      }
      return;
    }
    case AddressItemModel::Rows::Access: {
      if(auto cb = qobject_cast<Explorer::AccessModeComboBox*>(editor))
      {
        model->setData(index, cb->itemData(cb->currentIndex()), Qt::EditRole);
      }
      return;
    }
    case AddressItemModel::Rows::Bounding: {
      if(auto cb = qobject_cast<Explorer::BoundingModeComboBox*>(editor))
      {
        model->setData(index, cb->itemData(cb->currentIndex()), Qt::EditRole);
      }
      return;
    }
    case AddressItemModel::Rows::Unit: {
      if(auto cb = qobject_cast<State::UnitWidget*>(editor))
      {
        model->setData(index, QVariant::fromValue(cb->unit()), Qt::EditRole);
      }
      return;
    }
    case AddressItemModel::Rows::Value:
    case AddressItemModel::Rows::Min:
    case AddressItemModel::Rows::Max:
    case AddressItemModel::Rows::Values: {
      if(auto cb = qobject_cast<AddressValueWidget*>(editor))
      {
        if(!cb->edited())
          return;

        if(auto v = cb->get(); v.valid())
          model->setData(index, QVariant::fromValue(v), Qt::EditRole);
        return;
      }
      break;
    }
    default: {
      if(auto t = qobject_cast<State::ExpandableTextEdit*>(editor))
      {
        model->setData(index, t->fullText(), Qt::EditRole);
        return;
      }
      if(auto cb = qobject_cast<AddressValueWidget*>(editor))
      {
        if(!cb->edited())
          return;

        if(auto v = cb->get(); v.valid())
          model->setData(index, QVariant::fromValue(v), Qt::EditRole);
        return;
      }
      break;
    }
  }

  QStyledItemDelegate::setModelData(editor, model, index);
}
}
