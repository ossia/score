// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SimpleExpressionEditorWidget.hpp"

#include <Device/Widgets/AddressAccessorEditWidget.hpp>
#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Inspector/ExpressionValidator.hpp>
#include <State/Expression.hpp>
#include <State/Relation.hpp>

#include <score/model/Skin.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/TextLabel.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QComboBox>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QToolButton>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::SimpleExpressionEditorWidget)
namespace Scenario
{
static const auto& ExpressionEditorComparators()
{
  static const std::vector<std::pair<ExpressionEditorComparator, QString>> map{
      {ExpressionEditorComparator::Equal, "="},
      {ExpressionEditorComparator::Different, QString::fromUtf8("≠")},
      {ExpressionEditorComparator::Greater, ">"},
      {ExpressionEditorComparator::Lower, "<"},
      {ExpressionEditorComparator::GreaterEqual, QString::fromUtf8("≥")},
      {ExpressionEditorComparator::LowerEqual, QString::fromUtf8("≤")},
      {ExpressionEditorComparator::Pulse, "Pulse"},
      {ExpressionEditorComparator::AlwaysTrue, "Always"},
      {ExpressionEditorComparator::AlwaysFalse, "Never"}};

  return map;
}

class SimpleComboBox : public QComboBox
{
public:
  SimpleComboBox(QWidget* parent) : QComboBox{parent}
  {
    setMinimumSize(20, 16);
    setMaximumSize(40, 16);
    setFont(score::Skin::instance().MonoFontSmall);

    setFrame(false);
  }
  void paintEvent(QPaintEvent* ev)
  {
    QPainter p;
    p.begin(this);
    QStyleOptionComboBox opt;
    opt.initFrom(this);
    // style()->drawPrimitive (QStyle::PE_PanelButtonBevel, &opt, &p, this);
    // style()->drawPrimitive (QStyle::PE_FrameLineEdit, &opt, &p, this);
    // style()->drawPrimitive (QStyle::PE_, &opt, &p, this);
    style()->drawPrimitive(QStyle::PE_PanelButtonCommand, &opt, &p, this);
    style()->drawItemText(&p, rect(), Qt::AlignCenter, palette(), isEnabled(), currentText());
    p.end();
  }
};

SimpleExpressionEditorWidget::SimpleExpressionEditorWidget(
    const score::DocumentContext& doc,
    int index,
    QWidget* parent,
    QMenu* menu)
    : QWidget(parent), id{index}
{
  auto mainLay = new score::MarginLess<QHBoxLayout>{this};

  m_binOperator = new SimpleComboBox{this};

  m_address = new Device::AddressAccessorEditWidget{doc, this};
  m_ok = new TextLabel{QStringLiteral("/!\\ "), this};

  m_comparator = new SimpleComboBox{this};
  m_value = new QLineEdit{this};

  auto btnWidg = new QWidget{this};
  auto btnLay = new score::MarginLess<QHBoxLayout>{btnWidg};
  m_rmBtn = new QToolButton{btnWidg};
  m_rmBtn->setText(QStringLiteral("-"));
  m_rmBtn->setAutoRaise(true);
  m_rmBtn->setMaximumSize(30, 30);
  auto remIcon = makeIcons(
      QStringLiteral(":/icons/condition_remove_on.png"),
      QStringLiteral(":/icons/condition_remove_off.png"),
      QStringLiteral(":/icons/condition_remove_disabled.png"));

  m_rmBtn->setIcon(remIcon);

  m_addBtn = new QToolButton{btnWidg};
  m_addBtn->setText(QStringLiteral("+"));
  m_addBtn->setMaximumSize(30, 30);
  m_addBtn->setAutoRaise(true);
  m_addBtn->setVisible(false);
  auto addIcon = makeIcons(
      QStringLiteral(":/icons/condition_add_on.png"),
      QStringLiteral(":/icons/condition_add_off.png"),
      QStringLiteral(":/icons/condition_add_disabled.png"));

  m_addBtn->setIcon(addIcon);

  m_menuBtn = new Inspector::MenuButton{btnWidg};
  m_menuBtn->setObjectName(QStringLiteral("SettingsMenu"));
  m_menuBtn->setMaximumSize(30, 30);
  connect(m_menuBtn, &QToolButton::clicked, menu, [menu]() { menu->popup(QCursor::pos()); });

  QSizePolicy sp = m_menuBtn->sizePolicy();
  sp.setRetainSizeWhenHidden(true);
  m_menuBtn->setSizePolicy(sp);
  m_menuBtn->setVisible(false);

  btnLay->addWidget(m_rmBtn);
  btnLay->addWidget(m_addBtn);
  btnLay->addWidget(m_menuBtn);

  // Main Layout

  mainLay->addWidget(m_ok);
  mainLay->addWidget(m_binOperator, 0, Qt::AlignHCenter);
  mainLay->addWidget(m_address, 10);
  mainLay->addWidget(m_comparator, 0, Qt::AlignHCenter);
  mainLay->addWidget(m_value, 2);

  mainLay->addWidget(btnWidg, 0, Qt::AlignRight);

  // Connections

  connect(m_rmBtn, &QToolButton::clicked, this, [=]() { removeTerm(id); });
  connect(m_addBtn, &QToolButton::clicked, this, [=]() { addTerm(); });

  /// EDIT FINSHED
  connect(m_address, &Device::AddressAccessorEditWidget::addressChanged, this, [&]() {
    on_editFinished();
  });
  connect(m_comparator, &QComboBox::currentTextChanged, this, [&] { on_editFinished(); });
  connect(
      m_value, &QLineEdit::editingFinished, this, &SimpleExpressionEditorWidget::on_editFinished);
  connect(m_binOperator, &QComboBox::currentTextChanged, this, [&] { on_editFinished(); });

  // enable value field
  connect(
      m_comparator,
      SignalUtils::QComboBox_currentIndexChanged_int(),
      this,
      &SimpleExpressionEditorWidget::on_comparatorChanged);

  m_ok->setVisible(false);
  m_value->setEnabled(false);

  // Fill ComboBox

  m_binOperator->setObjectName("BinOpComboBox");
  m_comparator->setObjectName("BinOpComboBox");

  m_binOperator->addItem(" ");
  m_binOperator->addItem("&");
  m_binOperator->addItem("|");

  auto& lst = ExpressionEditorComparators();

  for (auto& c : lst)
  {
    m_comparator->addItem(c.second, QVariant::fromValue(c.first));
  }

  // By default: always true
  m_comparator->setCurrentIndex(lst.size() - 2);

  QSizePolicy sp_retain = m_binOperator->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  m_binOperator->setSizePolicy(sp_retain);

  if (id == 0)
  {
    m_binOperator->setVisible(false);
  }
  else
  {
    m_binOperator->setCurrentIndex(1);
  }
}

void SimpleExpressionEditorWidget::decreaseId()
{
  id--;
  SCORE_ASSERT(id >= 0);

  if (id == 0)
    m_binOperator->setVisible(false);
  else
    m_binOperator->setCurrentIndex(1);
}

State::Expression SimpleExpressionEditorWidget::relation()
{
  int i = 1;
  QString expr = currentRelation();

  m_validator.validate(expr, i);
  if (m_validator.validate(expr, i) == QValidator::State::Acceptable)
  {
    return *m_validator.get();
  }

  else
    return State::Expression{};
}

optional<State::BinaryOperator> SimpleExpressionEditorWidget::binOperator()
{
  switch (m_binOperator->currentIndex())
  {
    case 1:
      return State::BinaryOperator::AND;
    case 2:
      return State::BinaryOperator::OR;
    default:
      return ossia::none;
  }
}

void SimpleExpressionEditorWidget::setRelation(const State::Relation& r)
{
  auto lptr = r.lhs.target<ossia::value>();
  auto rptr = r.rhs.target<ossia::value>();
  if (lptr && rptr)
  {
    auto lv = *lptr;
    auto rv = *rptr;

    if (r.op == ossia::expressions::comparator::EQUAL && lv == rv)
    {
      m_comparator->setCurrentIndex(ExpressionEditorComparator::AlwaysTrue);
    }
    else
    {
      m_comparator->setCurrentIndex(ExpressionEditorComparator::AlwaysFalse);
    }
    m_address->setAddress(State::AddressAccessor{});
    m_value->clear();
  }
  else
  {
    if (auto addr_ptr = r.lhs.target<State::Address>())
    {
      m_address->setAddress(State::AddressAccessor{*addr_ptr});
    }
    else if (auto acc_ptr = r.lhs.target<State::AddressAccessor>())
    {
      m_address->setAddress(*acc_ptr);
    }

    auto s = State::toString(r.rhs);
    m_value->setText(s);

    m_comparator->setCurrentIndex(static_cast<int>(r.op));

    m_relation = State::toString(r);

    int i;
    m_ok->setVisible(m_validator.validate(m_relation, i) != QValidator::State::Acceptable);
  }
}

void SimpleExpressionEditorWidget::setPulse(const State::Pulse& p)
{
  m_address->setAddress(State::AddressAccessor{p.address});
  m_value->clear();

  m_comparator->setCurrentIndex(ExpressionEditorComparator::Pulse);
  m_relation = State::toString(p);

  int i;
  m_ok->setVisible(m_validator.validate(m_relation, i) != QValidator::State::Acceptable);
}

void SimpleExpressionEditorWidget::setOperator(State::BinaryOperator o)
{
  switch (o)
  {
    case State::BinaryOperator::AND:
      m_binOperator->setCurrentIndex(1);
      m_op = "and";
      break;
    case State::BinaryOperator::OR:
      m_binOperator->setCurrentIndex(2);
      m_op = "or";
      break;
    default:
      m_binOperator->setCurrentIndex(0);
      m_op = "";
      break;
  }
}

void SimpleExpressionEditorWidget::setOperator(State::UnaryOperator u)
{
  // TODO : add the unary operator
  /*    switch (u) {
          case State::UnaryOperator::Not :
              break;
          default:
              m_binOperator->setCurrentIndex(0);
              break;
      }
      */
}

void SimpleExpressionEditorWidget::on_editFinished()
{
  QString expr = currentRelation();
  if (expr == m_relation && m_op == currentOperator())
    return;

  int i = 1;
  m_op = currentOperator();
  m_relation = expr;
  bool b = m_validator.validate(expr, i) == QValidator::State::Acceptable;
  m_ok->setVisible(!b);
  if (b)
    editingFinished();
}

void SimpleExpressionEditorWidget::on_comparatorChanged(int i)
{
  switch (i)
  {
    case ExpressionEditorComparator::Equal:
    case ExpressionEditorComparator::Different:
    case ExpressionEditorComparator::Greater:
    case ExpressionEditorComparator::Lower:
    case ExpressionEditorComparator::GreaterEqual:
    case ExpressionEditorComparator::LowerEqual:
      m_address->setEnabled(true);
      m_value->setEnabled(true);
      m_address->setVisible(true);
      m_value->setVisible(true);
      break;

    case ExpressionEditorComparator::Pulse:
      m_address->setEnabled(true);
      m_value->setEnabled(false);
      m_address->setVisible(true);
      m_value->setVisible(false);
      break;

    case ExpressionEditorComparator::AlwaysTrue:
    case ExpressionEditorComparator::AlwaysFalse:
      m_address->setEnabled(false);
      m_value->setEnabled(false);
      m_address->setVisible(false);
      m_value->setVisible(false);
      break;

    default:
      m_address->setEnabled(false);
      m_value->setEnabled(false);
      m_address->setVisible(false);
      m_value->setVisible(false);
      break;
  }
}

QString SimpleExpressionEditorWidget::currentRelation()
{
  QString addr = "%" + m_address->addressString() + "%";

  switch (m_comparator->currentIndex())
  {
    case ExpressionEditorComparator::Greater:
      return addr + " > " + m_value->text();
    case ExpressionEditorComparator::Lower:
      return addr + " < " + m_value->text();
    case ExpressionEditorComparator::Equal:
      return addr + " == " + m_value->text();
    case ExpressionEditorComparator::GreaterEqual:
      return addr + " >= " + m_value->text();
    case ExpressionEditorComparator::LowerEqual:
      return addr + " <= " + m_value->text();
    case ExpressionEditorComparator::Different:
      return addr + " != " + m_value->text();

    case ExpressionEditorComparator::Pulse:
      return addr + " impulse";
    case ExpressionEditorComparator::AlwaysTrue:
      return State::defaultTrueExpression().toString();
    case ExpressionEditorComparator::AlwaysFalse:
      return State::defaultFalseExpression().toString();
  }

  return "";
}

QString SimpleExpressionEditorWidget::currentOperator()
{
  return m_binOperator->currentText();
}

void SimpleExpressionEditorWidget::enableRemoveButton(bool b)
{
  m_rmBtn->setVisible(b);
}

void SimpleExpressionEditorWidget::enableAddButton(bool b)
{
  m_addBtn->setVisible(b);

  QSizePolicy sp = m_menuBtn->sizePolicy();
  sp.setRetainSizeWhenHidden(!b);
  m_menuBtn->setSizePolicy(sp);
}

void SimpleExpressionEditorWidget::enableMenuButton(bool b)
{
  m_menuBtn->setVisible(b);
  m_menuBtn->setEnabled(b);
}

}
