#pragma once
#include <QWidget>

#include <utility>
#include <vector>
#include <verdigris>
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
namespace Scenario
{
class ExtendedMetadataWidget final : public QWidget
{
  W_OBJECT(ExtendedMetadataWidget)
public:
  ExtendedMetadataWidget(const QVariantMap& map, QWidget* parent);

  void update(const QVariantMap& map);

  void setup(const QVariantMap& map);

  QVariantMap currentMap() const;

public:
  void dataChanged() W_SIGNAL(dataChanged);

private:
  std::pair<QLabel*, QLineEdit*> makeRow(const QString& l, const QString& r, int row);

  void insertRow(const QString& label, int row);

  void on_rowChanged(int i);

  void on_rowRemoved(int i);

  void setup_impl(const QVariantMap& map);

  QFormLayout* m_layout{};
  std::vector<std::pair<QLabel*, QLineEdit*>> m_widgets;
  QPushButton* m_addButton{};
};
}
