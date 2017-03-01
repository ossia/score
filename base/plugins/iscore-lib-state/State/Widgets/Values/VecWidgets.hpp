#pragma once
#include <ossia/network/domain/domain.hpp>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <State/Value.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SignalUtils.hpp>
#include <iscore/widgets/TextLabel.hpp>

namespace State
{
struct ISCORE_LIB_STATE_EXPORT VecEditBase : public QWidget
{
  Q_OBJECT
public:
  using QWidget::QWidget;

signals:
  void changed();
};

template <std::size_t N>
class VecWidget final : public VecEditBase
{
public:
  VecWidget(QWidget* parent) : VecEditBase{parent}
  {
    auto lay = new QHBoxLayout;
    this->setLayout(lay);

    for (std::size_t i = 0; i < N; i++)
    {
      auto box = new QDoubleSpinBox{this};
      box->setMinimum(-9999);
      box->setMaximum(9999);
      box->setValue(0);

      connect(box, &QDoubleSpinBox::editingFinished, this, [=] { changed(); });

      lay->addWidget(box);
      m_boxes[i] = box;
    }
  }

  void setValue(std::array<float, N> v)
  {
    for (std::size_t i = 0; i < N; i++)
    {
      m_boxes[i]->setValue(v[i]);
    }
  }

  std::array<float, N> value() const
  {
    std::array<float, N> v;
    for (std::size_t i = 0; i < N; i++)
    {
      v[i] = m_boxes[i]->value();
    }
    return v;
  }

private:
  std::array<QDoubleSpinBox*, N> m_boxes;
};

using Vec2DEdit = VecWidget<2>;
using Vec3DEdit = VecWidget<3>;
using Vec4DEdit = VecWidget<4>;

template <std::size_t N>
class VecDomainWidget final : public QWidget
{
public:
  using domain_type = ossia::domain_base<std::array<float, N>>;
  using set_type = boost::container::flat_set<std::array<float, N>>;

  VecDomainWidget(QWidget* parent) : QWidget{parent}
  {
    auto lay = new iscore::MarginLess<QFormLayout>{this};
    this->setLayout(lay);

    auto min_l = new TextLabel{tr("Min"), this};
    min_l->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    m_min = new VecWidget<N>{this};
    auto max_l = new TextLabel{tr("Max"), this};
    max_l->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    m_max = new VecWidget<N>{this};
    lay->addWidget(min_l);
    lay->addWidget(m_min);
    lay->addWidget(max_l);
    lay->addWidget(m_max);
  }

  domain_type domain() const
  {
    domain_type dom;

    dom.min = m_min->value();
    dom.max = m_max->value();

    return dom;
  }

  void setDomain(ossia::domain dom_base)
  {
    m_min->setValue(ossia::fill_vec<N>(0));
    m_max->setValue(ossia::fill_vec<N>(1));

    if (auto dom_p = dom_base.target<domain_type>())
    {
      auto& dom = *dom_p;

      if (dom.min)
        m_min->setValue(*dom.min);
      if (dom.max)
        m_max->setValue(*dom.max);
    }
  }

private:
  VecWidget<N>* m_min{};
  VecWidget<N>* m_max{};
};
}
