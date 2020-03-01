#include "AddressItemModel.hpp"

#include <Explorer/Commands/Update/UpdateAddressSettings.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <State/Expression.hpp>
#include <State/ValueConversion.hpp>
#include <State/Widgets/UnitWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia-qt/metatypes.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_traits.hpp>

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QSpinBox>

#include <wobjectimpl.h>

Q_DECLARE_METATYPE(ossia::net::tags)
W_OBJECT_IMPL(Explorer::AddressItemModel)
W_OBJECT_IMPL(Explorer::AddressValueWidget)

namespace Explorer
{
AddressItemModel::AddressItemModel(QObject* parent)
    : QAbstractItemModel{parent}
{
}

void AddressItemModel::setState(
    DeviceExplorerModel* model,
    Device::NodePath nodepath,
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

bool AddressItemModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
  namespace onet = ossia::net;
  if (index.column() != 1)
    return false;

  if (index.row() < 0 || index.row() > rowCount({}))
    return false;

  if (!m_model)
    return false;

  if (m_settings.address.path.empty())
    return false; // TODO just rename device ?

  Device::AddressSettings before, after;
  before.name = m_settings.address.path.last();
  (Device::AddressSettingsCommon&)before = m_settings;
  after = before;

  switch (index.row())
  {
    case Rows::Value:
    {
      if (value.canConvert<ossia::value>())
      {
        after.value = value.value<ossia::value>();

        // Note : if we want to disable remote updating, we have to do it
        // here (e.g. if this becomes a settings)
        m_model->deviceModel().updateProxy.updateRemoteValue(
            m_settings.address, after.value);
        return true;
      }
      else
      {
        // In this case we don't make a command, but we directly push the
        // new value.
        auto copy = State::convert::fromQVariant(value);

        // We may have to convert types.
        const ossia::value& orig = before.value;
        if (copy.v.which() != orig.v.which()
            && !State::convert::convert(orig, copy))
          return false;

        after.value = copy;

        // Note : if we want to disable remote updating, we have to do it
        // here (e.g. if this becomes a settings)
        m_model->deviceModel().updateProxy.updateRemoteValue(
            m_settings.address, copy);
        return true;
      }
    }

    case Rows::Type:
    {
      after.value
          = ossia::convert(before.value, value.value<ossia::val_type>());
      break;
    }

    case Rows::Min:
    {
      if (value.canConvert<ossia::value>())
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
        if (copy.v.which() != orig.v.which()
            && !State::convert::convert(orig, copy))
          return false;

        after.domain.get().set_min(copy);
      }
      break;
    }
    case Rows::Max:
    {
      if (value.canConvert<ossia::value>())
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
        if (copy.v.which() != orig.v.which()
            && !State::convert::convert(orig, copy))
          return false;

        after.domain.get().set_max(copy);
      }
      break;
    }
    case Rows::Access:
    {
      after.ioType = (ossia::access_mode)value.toInt();
      break;
    }
    case Rows::Bounding:
    {
      after.clipMode = (ossia::bounding_mode)value.toInt();
      break;
    }
    case Rows::Repetition:
    {
      after.repetitionFilter = value.toInt() != 0
                                   ? ossia::repetition_filter::ON
                                   : ossia::repetition_filter::OFF;
      break;
    }
    case Rows::Unit:
    {
      after.unit.get() = value.value<State::Unit>().get();
      break;
    }
    default:
    {
      int idx = index.row() - Rows::Count;
      if (idx >= 0 && idx < (int)after.extendedAttributes.size())
      {
        auto it = after.extendedAttributes.begin();
        std::advance(it, idx);
        if (it.key() == onet::text_description())
        {
          it.value() = value.toString().toStdString();
        }
        else if (it.key() == onet::text_tags())
        {
          // TODO
        }
        else if (it->first == onet::text_default_value())
        {
          if (value.canConvert<ossia::value>())
          {
            it.value() = value.value<ossia::value>();
          }
        }
        else if (it->first == onet::text_refresh_rate())
        {
          it.value() = value.toInt();
        }
        else if (it->first == onet::text_value_step_size())
        {
          it.value() = value.toDouble();
        }
        else if (it->first == onet::text_priority())
        {
          it.value() = value.toInt();
        }
      }
    }
  }

  auto node
      = (Device::Node*)m_model->convertPathToIndex(m_path).internalPointer();
  if (!node)
    return false;

  if (!m_model->checkAddressEditable(*node, before, after))
    return false;

  CommandDispatcher<> disp{m_model->commandStack()};
  disp.submit(new Explorer::Command::UpdateAddressSettings{
      m_model->deviceModel(), m_path, after});

  return true;
}

QModelIndex
AddressItemModel::index(int row, int column, const QModelIndex& parent) const
{
  if (parent == QModelIndex{})
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
  if (m_settings.address.device.isEmpty())
    return 0;

  if (!m_settings.value.valid())
    return 2;

  return Rows::Count + extendedCount();
}

int AddressItemModel::columnCount(const QModelIndex&) const
{
  return 2;
}

QVariant
AddressItemModel::valueColumnData(const State::Value& val, int role) const
{
  if (role == Qt::DisplayRole || role == Qt::EditRole)
  {
    if (ossia::is_array(val))
    {
      // TODO a nice editor for lists.
      return State::convert::toPrettyString(val);
    }
    else
    {
      return State::convert::value<QVariant>(val);
    }
  }

  return {};
}

QVariant AddressItemModel::data(const QModelIndex& index, int role) const
{
  namespace onet = ossia::net;
  if (role == Qt::DisplayRole)
  {
    if (!m_settings.value.valid())
    {
      switch (index.column())
      {
        case 0:
        {
          switch (index.row())
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
        case 1:
        {
          switch (index.row())
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

    switch (index.column())
    {
      case 0:
      {
        switch (index.row())
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
          default:
          {
            int idx = index.row() - Rows::Count;
            if (idx >= 0 && idx < (int)m_settings.extendedAttributes.size())
            {
              auto it = m_settings.extendedAttributes.begin();
              std::advance(it, idx);
              auto str = QString::fromStdString(it.key());
              if (!str.isEmpty())
                str[0] = str[0].toUpper();
              for (int i = 1; i < str.size(); i++)
              {
                if (str[i].isUpper())
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
      case 1:
      {
        switch (index.row())
        {
          case Rows::Name:
            return m_settings.address.path.last();
          case Rows::Address:
            return m_settings.address.toString();
          case Rows::Value:
            return valueColumnData(m_settings.value, role);
          case Rows::Type:
          {
            return State::convert::ValuePrettyTypesArray()
                [(int)m_settings.value.get_type()];
          }
          case Rows::Min:
          {
            return valueColumnData(
                ossia::get_min(m_settings.domain.get()), role);
          }
          case Rows::Max:
          {
            return valueColumnData(
                ossia::get_max(m_settings.domain.get()), role);
          }
          case Rows::Values:
          {
            return valueColumnData(
                ossia::get_values(m_settings.domain.get()), role);
          }
          case Rows::Unit:
          {
            return State::prettyUnitText(m_settings.unit.get());
          }
          case Rows::Access:
          {
            return bool(m_settings.ioType)
                       ? Device::AccessModeText()[*m_settings.ioType]
                       : tr("None");
          }
          case Rows::Bounding:
          {
            return Device::ClipModePrettyStringMap()[m_settings.clipMode];
          }
          case Rows::Repetition:
          {
            return m_settings.repetitionFilter == ossia::repetition_filter::ON
                       ? tr("Filtered")
                       : tr("Unfiltered");
          }
          default:
          {
            int idx = index.row() - Rows::Count;
            if (idx >= 0 && idx < (int)m_settings.extendedAttributes.size())
            {
              auto it = m_settings.extendedAttributes.begin();
              std::advance(it, idx);
              if (it->first == onet::text_description())
              {
                return QString::fromStdString(
                    ossia::any_cast<onet::description>(it->second));
              }
              else if (it->first == onet::text_tags())
              {
                const auto& tags = ossia::any_cast<onet::tags>(it->second);

                QStringList l;
                for (const auto& s : tags)
                  l += QString::fromStdString(s);
                return l.join(", ");
              }
              else if (it->first == onet::text_default_value())
              {
                const auto& v
                    = ossia::any_cast<onet::default_value_attribute::type>(
                        it->second);
                return valueColumnData(v, role);
              }
              else if (it->first == onet::text_refresh_rate())
              {
                return ossia::any_cast<onet::refresh_rate_attribute::type>(
                    it->second);
              }
              else if (it->first == onet::text_value_step_size())
              {
                return ossia::any_cast<onet::value_step_size_attribute::type>(
                    it->second);
              }
              else if (it->first == onet::text_priority())
              {
                return ossia::any_cast<onet::priority_attribute::type>(
                    it->second);
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
  else if (role == Qt::EditRole)
  {
    if (index.column() == 1)
    {
      switch (index.row())
      {
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
          return QVariant::fromValue(
              ossia::get_values(m_settings.domain.get()));
        default:
        {
          int idx = index.row() - Rows::Count;
          if (idx >= 0 && idx < (int)m_settings.extendedAttributes.size())
          {
            auto it = m_settings.extendedAttributes.begin();
            std::advance(it, idx);
            if (it->first == onet::text_description())
            {
              return QString::fromStdString(
                  ossia::any_cast<onet::description>(it->second));
            }
            else if (it->first == onet::text_tags())
            {
              return QVariant::fromValue(
                  ossia::any_cast<onet::tags>(it->second));
            }
            else if (it->first == onet::text_default_value())
            {
              return QVariant::fromValue(
                  ossia::any_cast<onet::default_value_attribute::type>(
                      it->second));
            }
            else if (it->first == onet::text_refresh_rate())
            {
              return ossia::any_cast<onet::refresh_rate_attribute::type>(
                  it->second);
            }
            else if (it->first == onet::text_value_step_size())
            {
              return ossia::any_cast<onet::value_step_size_attribute::type>(
                  it->second);
            }
            else if (it->first == onet::text_priority())
            {
              return ossia::any_cast<onet::priority_attribute::type>(
                  it->second);
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
  else if (role == Qt::CheckStateRole)
  {
    if (index.column() == 1)
    {
      switch (index.row())
      {
        case Rows::Repetition:
          return m_settings.repetitionFilter == ossia::repetition_filter::ON
                     ? Qt::Checked
                     : Qt::Unchecked;
        case Rows::Value:
        {
          if (auto b = m_settings.value.target<bool>())
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

int AddressItemModel::extendedCount() const noexcept
{
  return int(m_settings.extendedAttributes.size());
}

Qt::ItemFlags AddressItemModel::flags(const QModelIndex& index) const
{
  if (index.column() == 0)
    return {Qt::ItemIsEnabled};

  Qt::ItemFlags f = QAbstractItemModel::flags(index);
  static const constexpr std::array<Qt::ItemFlags, Rows::Count> flags{{
      {} // name
      ,
      {} // address
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

  if (index.row() < Rows::Count)
    f |= flags[index.row()];
  else
    f |= Qt::ItemIsEditable;

  if (index.row() == Value)
  {
    if (m_settings.value.target<bool>())
    {
      f |= Qt::ItemIsUserCheckable;
      f |= Qt::ItemIsEnabled;
    }
  }

  return f;
}

AddressItemDelegate::AddressItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

AddressItemDelegate::~AddressItemDelegate() {}

void AddressItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
  QStyledItemDelegate::paint(painter, option, index);
}

class SliderValueWidget final : public AddressValueWidget
{
public:
  SliderValueWidget(int min, int max, QWidget* parent)
      : AddressValueWidget{parent}, m_slider{this}
  {
    m_slider.setOrientation(Qt::Horizontal);
    m_slider.setRange(min, max);
    m_edit.setRange(min, max);

    m_slider.setContentsMargins(0, 0, 0, 0);
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);

    connect(&m_slider, &QSlider::valueChanged, this, [=](int v) {
      m_edit.setValue(v);
    });

    connect(
        &m_edit, SignalUtils::QSpinBox_valueChanged_int(), this, [=](int v) {
          m_slider.setValue(v);
        });

    m_lay.addWidget(&m_slider);
    m_lay.addWidget(&m_edit);
  }

  ossia::value get() const override { return m_slider.value(); }

  void set(ossia::value t) override
  {
    m_slider.setValue(ossia::convert<int>(t));
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  score::Slider m_slider;
  QSpinBox m_edit;
};

class DoubleSliderValueWidget final : public AddressValueWidget
{
public:
  DoubleSliderValueWidget(double min, double max, QWidget* parent)
      : AddressValueWidget{parent}, m_slider{this}
  {
    m_slider.setOrientation(Qt::Horizontal);
    m_edit.setRange(min, max);

    m_slider.setContentsMargins(0, 0, 0, 0);
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);

    connect(
        &m_slider, &score::DoubleSlider::doubleValueChanged, this, [=](double v) {
          m_edit.setValue(min + v * (max - min));
        });

    connect(
        &m_edit,
        SignalUtils::QDoubleSpinBox_valueChanged_double(),
        this,
        [=](double v) { m_slider.setValue((v - min) / (max - min)); });

    m_lay.addWidget(&m_slider);
    m_lay.addWidget(&m_edit);
  }

  ossia::value get() const override { return m_edit.value(); }

  void set(ossia::value t) override
  {
    m_edit.setValue(ossia::convert<float>(t));
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  score::DoubleSlider m_slider;
  QDoubleSpinBox m_edit;
};

class ListValueWidget final : public AddressValueWidget
{
public:
  ListValueWidget(QWidget* parent) : AddressValueWidget{parent}
  {
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);
  }

  ossia::value get() const override
  {
    auto val = State::parseValue(m_edit.text().toStdString());
    if (val)
      return *val;
    return std::vector<ossia::value>{};
  }

  void set(ossia::value t) override
  {
    m_edit.setText(State::convert::toPrettyString(t));
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  QLineEdit m_edit;
};

struct make_unit
{
  template <typename T>
  AddressValueWidget* operator()(T unit)
  {
    return nullptr;
  }

  AddressValueWidget* operator()() { return nullptr; }
};
struct make_dataspace
{
  template <typename T>
  AddressValueWidget* operator()(T ds)
  {
    return ossia::apply(make_unit{}, ds);
  }
  AddressValueWidget* operator()(ossia::color_u ds)
  {
    auto res = ossia::apply(make_unit{}, ds);
    if (!res)
      return nullptr; // TODO generic colorpicker
    return res;
  }
  AddressValueWidget* operator()(ossia::position_u ds)
  {
    auto res = ossia::apply(make_unit{}, ds);
    if (!res)
      return nullptr; // TODO generic position chooser if vec2f ?
    return res;
  }

  AddressValueWidget* operator()() { return nullptr; }
};

AddressValueWidget*
make_value_widget(Device::FullAddressSettings addr, QWidget* parent)
{
  if (auto widg = ossia::apply(make_dataspace{}, addr.unit.get().v))
    return widg;

  auto& dom = addr.domain.get();
  auto min = dom.get_min(), max = dom.get_max();
  if (min.valid() && max.valid() && addr.value.valid())
  {
    switch (addr.value.get_type())
    {
      case ossia::val_type::FLOAT:
        return new DoubleSliderValueWidget{
            ossia::convert<float>(min), ossia::convert<float>(max), parent};
      case ossia::val_type::INT:
        return new SliderValueWidget{
            ossia::convert<int>(min), ossia::convert<int>(max), parent};
      default:
        break;
    }
  }

  switch (addr.value.get_type())
  {
    case ossia::val_type::LIST:
      return new ListValueWidget{parent};
    default:
      break;
  }

  return nullptr;
}

AddressValueWidget*
make_min_widget(Device::FullAddressSettings addr, QWidget* parent)
{
  switch (addr.value.get_type())
  {
    case ossia::val_type::LIST:
      return new ListValueWidget{parent};
    default:
      break;
  }

  return nullptr;
}

AddressValueWidget*
make_max_widget(Device::FullAddressSettings addr, QWidget* parent)
{
  switch (addr.value.get_type())
  {
    case ossia::val_type::LIST:
      return new ListValueWidget{parent};
    default:
      break;
  }

  return nullptr;
}

QWidget* AddressItemDelegate::createEditor(
    QWidget* parent,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
  auto model = qobject_cast<const AddressItemModel*>(index.model());
  if (index.column() == 0 || !model)
    return QStyledItemDelegate::createEditor(parent, option, index);

  switch (index.row())
  {
    case AddressItemModel::Rows::Type:
    {
      auto t = new State::TypeComboBox{parent};
      t->set(index.data(Qt::EditRole).value<ossia::val_type>());
      return t;
    }
    case AddressItemModel::Rows::Access:
    {
      auto t = new Explorer::AccessModeComboBox{parent};
      t->set(index.data(Qt::EditRole).value<ossia::access_mode>());
      return t;
    }
    case AddressItemModel::Rows::Bounding:
    {
      auto t = new Explorer::BoundingModeComboBox{parent};
      t->set(index.data(Qt::EditRole).value<ossia::bounding_mode>());
      return t;
    }
    case AddressItemModel::Rows::Unit:
    {
      auto t = new State::UnitWidget{Qt::Horizontal, parent};
      t->setUnit(index.data(Qt::EditRole).value<State::Unit>());
      return t;
    }
    case AddressItemModel::Rows::Value:
    {
      auto t = make_value_widget(model->settings(), parent);
      if (t)
        return t;
      else
        break;
    }
    case AddressItemModel::Rows::Min:
    {
      auto t = make_min_widget(model->settings(), parent);
      if (t)
        return t;
      else
        break;
    }
    case AddressItemModel::Rows::Max:
    {
      auto t = make_max_widget(model->settings(), parent);
      if (t)
        return t;
      else
        break;
    }
  }

  return QStyledItemDelegate::createEditor(parent, option, index);
}

void AddressItemDelegate::setEditorData(
    QWidget* editor,
    const QModelIndex& index) const
{
  if (index.column() == 0)
  {
    QStyledItemDelegate::setEditorData(editor, index);
    return;
  }

  switch (index.row())
  {
    case AddressItemModel::Rows::Type:
    {
      if (auto cb = qobject_cast<State::TypeComboBox*>(editor))
      {
        auto cur = index.data(Qt::EditRole).toInt();
        if (cur >= 0
            && cur < (int)State::convert::ValuePrettyTypesArray().size())
          cb->set((ossia::val_type)cur);
        return;
      }
      break;
    }
    case AddressItemModel::Rows::Access:
    {
      if (auto cb = qobject_cast<Explorer::AccessModeComboBox*>(editor))
      {
        auto cur = index.data(Qt::EditRole).toInt();
        if (cur >= 0 && cur < Device::AccessModeText().size())
          cb->set((ossia::access_mode)cur);
        return;
      }
      break;
    }
    case AddressItemModel::Rows::Bounding:
    {
      if (auto cb = qobject_cast<Explorer::BoundingModeComboBox*>(editor))
      {
        auto cur = index.data(Qt::EditRole).toInt();
        if (cur >= 0 && cur < Device::ClipModePrettyStringMap().size())
          cb->set((ossia::bounding_mode)cur);
        return;
      }
      break;
    }
    case AddressItemModel::Rows::Unit:
    {
      if (auto cb = qobject_cast<State::UnitWidget*>(editor))
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
    {
      if (auto cb = qobject_cast<AddressValueWidget*>(editor))
      {
        auto cur = index.data(Qt::EditRole).value<ossia::value>();
        cb->set(cur);
        return;
      }
    }
  }

  QStyledItemDelegate::setEditorData(editor, index);
}

void AddressItemDelegate::setModelData(
    QWidget* editor,
    QAbstractItemModel* model,
    const QModelIndex& index) const
{
  if (index.column() == 0)
  {
    QStyledItemDelegate::setModelData(editor, model, index);
    return;
  }

  switch (index.row())
  {
    case AddressItemModel::Rows::Type:
    {
      if (auto cb = qobject_cast<State::TypeComboBox*>(editor))
      {
        model->setData(index, cb->itemData(cb->currentIndex()), Qt::EditRole);
      }
      return;
    }
    case AddressItemModel::Rows::Access:
    {
      if (auto cb = qobject_cast<Explorer::AccessModeComboBox*>(editor))
      {
        model->setData(index, cb->itemData(cb->currentIndex()), Qt::EditRole);
      }
      return;
    }
    case AddressItemModel::Rows::Bounding:
    {
      if (auto cb = qobject_cast<Explorer::BoundingModeComboBox*>(editor))
      {
        model->setData(index, cb->itemData(cb->currentIndex()), Qt::EditRole);
      }
      return;
    }
    case AddressItemModel::Rows::Unit:
    {
      if (auto cb = qobject_cast<State::UnitWidget*>(editor))
      {
        model->setData(index, QVariant::fromValue(cb->unit()), Qt::EditRole);
      }
      return;
    }
    case AddressItemModel::Rows::Value:
    case AddressItemModel::Rows::Min:
    case AddressItemModel::Rows::Max:
    {
      if (auto cb = qobject_cast<AddressValueWidget*>(editor))
      {
        model->setData(index, QVariant::fromValue(cb->get()), Qt::EditRole);
        return;
      }
    }
  }

  QStyledItemDelegate::setModelData(editor, model, index);
}
}
