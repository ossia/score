#pragma once
#if defined(HAS_LV2)
#include <Media/Effect/LV2/LV2EffectModel.hpp>

#include <QDialog>

#include <verdigris>

namespace Media::LV2
{

class Window final : public QDialog
{
  W_OBJECT(Window)
public:
  Window(const LV2EffectModel& e, const score::DocumentContext& ctx, QWidget* parent);

  ~Window() override;
  // TODO void resize(int w, int h);
  static bool is_resizable(LilvWorld* world, const LilvUI& ui);

private:
  void resizeEvent(QResizeEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  const LV2EffectModel& m_model;
  QWidget* m_widget{};
};

using LayerFactory
    = Process::EffectLayerFactory_T<LV2EffectModel, Process::DefaultEffectItem, Window>;
}
#endif
