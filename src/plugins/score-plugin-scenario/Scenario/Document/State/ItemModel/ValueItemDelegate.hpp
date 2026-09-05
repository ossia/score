#pragma once
#include <QSet>
#include <QStyledItemDelegate>

#include <score_plugin_scenario_export.h>

namespace Scenario
{
/**
 * @brief The row's value as an ossia::value.
 *
 * The display and edit roles carry the value's *text*, which cannot tell a
 * vec2f from a two-element list. A model whose value column offers this role
 * gets typed editors from ValueItemDelegate.
 */
inline constexpr int OssiaValueRole = Qt::UserRole + 41;

/**
 * @brief Gives a value column the Device Explorer's editors.
 *
 * make_value_widget knows the type, so a vec gets its components rather than a
 * bracketed string, and every editor gets copy, paste and the text form on the
 * right-click. A state carries no unit, so the unit-led editors (colour
 * swatch, XY pad) do not appear here.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT ValueItemDelegate final : public QStyledItemDelegate
{
public:
  explicit ValueItemDelegate(int valueColumn, QObject* parent = nullptr);
  ~ValueItemDelegate();

private:
  QWidget* createEditor(
      QWidget* parent, const QStyleOptionViewItem& option,
      const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index)
      const override;
  void updateEditorGeometry(
      QWidget* editor, const QStyleOptionViewItem& option,
      const QModelIndex& index) const override;

  //! The view has let this editor go: see DeviceExplorerDelegate, same story.
  void destroyEditor(QWidget* editor, const QModelIndex& index) const override;

  //! The editors this delegate made that the view still owns.
  bool owns(QWidget* editor) const noexcept { return m_live.contains(editor); }

  mutable QSet<QWidget*> m_live;
  int m_column{};
};
}
