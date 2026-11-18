#pragma once
#include <ossia/detail/pod_vector.hpp>

#include <QString>
#include <QTableView>

#include <score_plugin_audio_export.h>

#include <functional>
#include <utility>
#include <vector>

namespace Audio
{
class RoutingModel;

inline constexpr int unrouted = -1;

struct RoutedPort
{
  QString name;
  ossia::int_vector mapping;
};

class SCORE_PLUGIN_AUDIO_EXPORT RoutingMatrix final : public QTableView
{
public:
  using Changes = std::vector<std::pair<int, ossia::int_vector>>;

  enum class Direction
  {
    FromCard,
    ToCard
  };

  explicit RoutingMatrix(Direction d, QWidget* parent = nullptr);
  ~RoutingMatrix() override;

  void setDirection(Direction d);
  void setCardChannels(int count);
  int cardChannels() const noexcept;

  void setPorts(std::vector<RoutedPort> ports);
  const std::vector<RoutedPort>& ports() const noexcept;

  std::function<void(Changes)> onRoute;

  void toggle(int row, int column);
  void connectDiagonally(int row, int column, bool acrossPorts);
  void disconnectRow(int row);
  void disconnectPort(int port);
  void disconnectAll();

  static QString help();

protected:
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void leaveEvent(QEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void contextMenuEvent(QContextMenuEvent* e) override;

private:
  void route(Changes c);
  void setHovered(const QModelIndex& idx);

  RoutingModel* m_model{};
};
}
