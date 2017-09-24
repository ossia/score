//#pragma once
//#include <QObject>
//#include <QQuickPaintedItem>
//#include <Process/Dataflow/DataflowObjects.hpp>
//#include <QPainter>

//namespace Dataflow
//{
//class PortItem: public QQuickPaintedItem
//{
//  public:
//    enum Type { Inlet, Outlet, DependencyInlet, DependencyOutlet };
//    PortItem(const score::DocumentContext& ctx, Type, std::size_t idx, NodeItem& node);

//    void paint(QPainter* painter) override;
//    void mousePressEvent(QMouseEvent* ev) override;
//    void mouseMoveEvent(QMouseEvent* ev) override;
//    void mouseReleaseEvent(QMouseEvent* ev) override;

//    void hoverEnterEvent(QHoverEvent* ev) override;
//    void hoverMoveEvent(QHoverEvent* ev) override;
//    void hoverLeaveEvent(QHoverEvent* ev) override;

//    void dragEnterEvent(QDragEnterEvent* ev) override;
//    void dragLeaveEvent(QDragLeaveEvent* ev) override;
//    void dragMoveEvent(QDragMoveEvent* ev) override;

//    void mouseDoubleClickEvent(QMouseEvent* ev) override;

//    void setGlow(bool);

//  private:
//    const score::DocumentContext& m_ctx;
//    NodeItem& m_node;
//    Type m_type{};
//    std::size_t m_index{};
//    bool m_glow{};
//};

//struct NodeItem: public QQuickPaintedItem
//{
//    Q_OBJECT
//  public:
//    Process::Node& node;

//    NodeItem(const score::DocumentContext& ctx, Process::Node& p);
//    ~NodeItem();

//    void recreate();
//    void paint(QPainter* painter) override;

//    QPointF depInlet() const;
//    QPointF depOutlet() const;

//    QPointF inletPosition(int i) const;
//    QPointF outletPosition(int i) const;
//    double xMv() const { return 4.; }
//    double yMv() const { return 4.; }
//    double objectX() const { return x()+4.; }
//    double objectY() const { return y()+4.; }
//    double objectW() const { return width()-8.; }
//    double objectH() const { return height()-8.; }

//  signals:
//    void aboutToDelete();

//  private:
//    QPointF m_clickPos;
//    QPointF m_origPos;
//    void mousePressEvent(QMouseEvent* ev) override;
//    void mouseMoveEvent(QMouseEvent* ev) override;
//    void mouseReleaseEvent(QMouseEvent* ev) override;

//    void hoverEnterEvent(QHoverEvent* ev) override;
//    void hoverMoveEvent(QHoverEvent* ev) override;
//    void hoverLeaveEvent(QHoverEvent* ev) override;

//    void dragEnterEvent(QDragEnterEvent* ev) override;
//    void dragLeaveEvent(QDragLeaveEvent* ev) override;
//    void dragMoveEvent(QDragMoveEvent* ev) override;

//    void mouseDoubleClickEvent(QMouseEvent* ev) override;

//    const score::DocumentContext& m_ctx;
//    PortItem m_depIn;
//    PortItem m_depOut;
//    std::vector<PortItem*> m_inlets;
//    std::vector<PortItem*> m_outlets;
//};

//struct CableItem: public QQuickPaintedItem
//{
//  public:
//    Process::Cable& m_cable;
//    NodeItem* source{};
//    NodeItem* sink{};
//    CableItem(Process::Cable& p, NodeItem* source = nullptr, NodeItem* sink = nullptr);

//    const auto& id() const { return m_cable.id(); }
//    void recreate();
//    void updateRect();
//    void paint(QPainter* painter) override;

//    void press(QPointF pos);
//    void move(QPointF pos);
//    PortItem* release(QPointF pos);

//  private:
//    std::vector<QMetaObject::Connection> cons;

//    QPointF m_clickPos{};
//    QPointF m_curPos{};
//    QLineF m_line;

//    void hoverEnterEvent(QHoverEvent* ev) override;
//    void hoverMoveEvent(QHoverEvent* ev) override;
//    void hoverLeaveEvent(QHoverEvent* ev) override;

//    void dragEnterEvent(QDragEnterEvent* ev) override;
//    void dragLeaveEvent(QDragLeaveEvent* ev) override;
//    void dragMoveEvent(QDragMoveEvent* ev) override;

//    void mouseDoubleClickEvent(QMouseEvent* ev) override;
//};
//}
