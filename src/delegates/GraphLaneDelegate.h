#ifndef GRAPHLANEDELEGATE_H
#define GRAPHLANEDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QFont>
#include <QString>
#include <vector>
#include "core/GitTypes.h"

namespace Git {

class GraphLaneDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    // Geometry shared with the view (see CommitGraphView::setupView).
    static constexpr int RowHeight = 24;
    static constexpr int LaneWidth = 16;
    static constexpr int NodeRadius = 4;

    explicit GraphLaneDelegate(QObject *parent = nullptr);
    ~GraphLaneDelegate() override = default;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    static constexpr int LanePadding = 8;          // gap between the last lane and the badges
    static constexpr int BadgePadding = 5;         // horizontal padding inside a badge
    static constexpr int BadgeSpacing = 4;         // gap between two badges
    static constexpr int MaxBadgeAreaWidth = 320;  // badges never make the column wider than this

    struct Badge {
        QString text;
        QColor background;
        QColor foreground;
    };

    [[nodiscard]] static std::vector<Badge> badgesFor(const CommitNode &node);
    [[nodiscard]] static QFont badgeFont(const QFont &base);
    [[nodiscard]] static int lanesWidth(const CommitNode &node);
    [[nodiscard]] static double laneCenterX(const QRect &rect, int lane);

    // Draws one row's lines: pass-through lanes, lines from above, lines to the parents.
    void drawConnections(QPainter *painter, const QRect &rect, const CommitNode &node) const;
    void drawNode(QPainter *painter, const QRect &rect, const CommitNode &node, const QPalette &palette) const;
    void drawBadges(QPainter *painter, const QRect &rect, const QFont &baseFont, const CommitNode &node) const;
};

} // namespace Git

#endif // GRAPHLANEDELEGATE_H
