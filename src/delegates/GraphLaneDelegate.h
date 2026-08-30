#ifndef GRAPHLANEDELEGATE_H
#define GRAPHLANEDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include "core/GitTypes.h"

namespace Git {

class GraphLaneDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit GraphLaneDelegate(QObject *parent = nullptr);
    ~GraphLaneDelegate() override = default;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    static constexpr int LaneWidth = 16;
    static constexpr int NodeRadius = 4;

    void drawConnections(QPainter *painter, const QRect &rect, const CommitNode &node) const;
    void drawNode(QPainter *painter, const QRect &rect, const CommitNode &node) const;
    void drawBadges(QPainter *painter, const QRect &rect, const CommitNode &node) const;
};

} // namespace Git

#endif // GRAPHLANEDELEGATE_H
