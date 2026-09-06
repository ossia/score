#include "AddressBarWidget.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/Skin.hpp>
#include <score/model/path/ObjectIdentifier.hpp>

#include <QEnterEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>

#include <wobjectimpl.h>

#include <functional>

W_OBJECT_IMPL(Scenario::AddressBarWidget)

namespace Scenario
{
namespace
{
// Painted by hand rather than a QToolButton: the application style gives
// buttons paddings that push their text off-centre in the thin navigation bar.
class AddressBarButton final : public QWidget
{
public:
  explicit AddressBarButton(IntervalModel& itv, QWidget* parent)
      : QWidget{parent}
  {
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFont(score::Skin::instance().Bold10Pt);
    setText(itv.metadata().getName());
    connect(
        &itv.metadata(), &score::ModelMetadata::NameChanged, this,
        &AddressBarButton::setText);
  }

  std::function<void()> onClick;

  void setText(const QString& t)
  {
    m_text = t;
    updateGeometry();
    update();
  }

  QSize sizeHint() const override
  {
    const QFontMetrics fm{font()};
    return {fm.horizontalAdvance(m_text) + 8, fm.height() + 4};
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    auto& skin = score::Skin::instance();
    QPainter p{this};
    p.setFont(font());
    p.setPen(m_hover ? skin.Base4.color() : skin.Light.color());
    p.drawText(rect(), Qt::AlignCenter, m_text);
  }

  void enterEvent(QEnterEvent*) override
  {
    m_hover = true;
    update();
  }
  void leaveEvent(QEvent*) override
  {
    m_hover = false;
    update();
  }
  void mousePressEvent(QMouseEvent* e) override
  {
    if(e->button() == Qt::LeftButton && onClick)
      onClick();
    e->accept();
  }

private:
  QString m_text;
  bool m_hover{};
};

class AddressBarSeparator final : public QWidget
{
public:
  explicit AddressBarSeparator(QWidget* parent)
      : QWidget{parent}
  {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFont(score::Skin::instance().Bold10Pt);
  }

  QSize sizeHint() const override
  {
    const QFontMetrics fm{font()};
    return {fm.horizontalAdvance(QStringLiteral("/")) + 2, fm.height() + 4};
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p{this};
    p.setFont(font());
    p.setPen(score::Skin::instance().HalfLight.color());
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("/"));
  }
};
}

AddressBarWidget::AddressBarWidget(const score::DocumentContext& ctx, QWidget* parent)
    : QWidget{parent}
    , m_ctx{ctx}
{
  setObjectName("AddressBar");
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setToolTip(tr("Address bar\nClick here to travel to the specific hierarchy level"));

  m_layout = new QHBoxLayout{this};
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0);
}

AddressBarWidget::~AddressBarWidget() { }

void AddressBarWidget::clear()
{
  // Deferred: a click on an entry ends up here, from that entry's own
  // mouse handler.
  for(auto w : m_items)
  {
    m_layout->removeWidget(w);
    w->hide();
    w->deleteLater();
  }
  m_items.clear();
}

void AddressBarWidget::setTargetObject(ObjectPath&& path)
{
  clear();
  m_currentPath = std::move(path);

  int i = -1;
  for(auto& identifier : m_currentPath)
  {
    i++;
    if(!identifier.objectName().contains("IntervalModel")
       && !identifier.objectName().contains("ConstraintModel"))
      continue;

    auto thisPath = m_currentPath;
    thisPath.vec().resize(i + 1);
    auto& itv = thisPath.find<IntervalModel>(m_ctx);

    if(!m_items.empty())
    {
      auto sep = new AddressBarSeparator{this};
      m_layout->addWidget(sep);
      m_items.push_back(sep);
    }

    auto btn = new AddressBarButton{itv, this};
    btn->onClick = [this, itv = QPointer<IntervalModel>{&itv}] {
      if(itv)
        intervalSelected(itv);
    };
    m_layout->addWidget(btn);
    m_items.push_back(btn);
  }
}
}
