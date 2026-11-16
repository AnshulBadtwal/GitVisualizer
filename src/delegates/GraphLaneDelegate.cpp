#include "delegates/GraphLaneDelegate.h"
#include "core/GraphLayoutEngine.h"
#include <QFontMetrics>
#include <QPalette>
#include <algorithm>

namespace Git {

GraphLaneDelegate::GraphLaneDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}


double GraphLaneDelegate::laneCenterX(const QRect &rect, int lane) {
    return rect.left() + lane * LaneWidth + LaneWidth / 2.0;
}

int GraphLaneDelegate::lanesWidth(const CommitNode &node) {
    // maxLanes is the same for every row, so badges line up and the column width is stable.
    return node.maxLanes * LaneWidth + LanePadding;
}

QFont GraphLaneDelegate::badgeFont(const QFont &base) {
    QFont font = base;
    font.setPointSize(8);
    font.setBold(true);
    return font;
}

auto GraphLaneDelegate::badgesFor(const CommitNode &node) -> std::vector<Badge> {
    std::vector<Badge> badges;
    badges.reserve(node.branchNames.size() + node.remoteBranchNames.size() + node.tagNames.size());

    const QColor headBackground(46, 204, 113);

    // Detached HEAD: there is no branch name to show, so mark the commit itself.
    if (node.isHead && node.checkedOutBranch.empty()) {
        badges.push_back({QStringLiteral("HEAD"), headBackground, QColor(Qt::white)});
    }

    // Local branches: dark slate, green for the branch that is checked out.
    for (const auto &name : node.branchNames) {
        const bool checkedOut = node.isHead && name == node.checkedOutBranch;
        badges.push_back({QString::fromStdString(name),
                          checkedOut ? headBackground : QColor(52, 73, 94),
                          QColor(Qt::white)});
    }

    // Remote-tracking branches: blue.
    for (const auto &name : node.remoteBranchNames) {
        badges.push_back({QString::fromStdString(name), QColor(41, 98, 160), QColor(Qt::white)});
    }

    // Tags: amber.
    for (const auto &name : node.tagNames) {
        badges.push_back({QString::fromStdString(name), QColor(241, 196, 15), QColor(Qt::black)});
    }

    return badges;
}

// ---------------------------------------------------------------------------
// Size
// ---------------------------------------------------------------------------

QSize GraphLaneDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(RowHeight);

    if (index.column() != GraphColumn::Graph) {
        return size;
    }

    const auto *node = index.data(CustomRoles::CommitGraphNodeRole).value<const CommitNode *>();
    if (!node) {
        return size;
    }

    // Wide enough for all lanes plus this row's badges, so nothing is clipped.
    int width = lanesWidth(*node);

    const std::vector<Badge> badges = badgesFor(*node);
    if (!badges.empty()) {
        const QFontMetrics metrics(badgeFont(option.font));
        int badgeArea = 0;
        for (const Badge &badge : badges) {
            badgeArea += metrics.horizontalAdvance(badge.text) + 2 * BadgePadding + BadgeSpacing;
        }
        width += std::min(badgeArea, MaxBadgeAreaWidth);
    }

    size.setWidth(width);
    return size;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void GraphLaneDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    // The base class paints the row background: alternating colors and the selection highlight.
    QStyledItemDelegate::paint(painter, option, index);

    if (index.column() != GraphColumn::Graph) {
        return;
    }

    const auto *node = index.data(CustomRoles::CommitGraphNodeRole).value<const CommitNode *>();
    if (!node) {
        return;
    }

    painter->save();
    painter->setClipRect(option.rect);   // nothing may spill into the neighbouring column
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Everything below uses only this row's data, so it looks the same at any scroll position.
    drawConnections(painter, option.rect, *node);
    drawNode(painter, option.rect, *node, option.palette);
    drawBadges(painter, option.rect, option.font, *node);

    painter->restore();
}

void GraphLaneDelegate::drawConnections(QPainter *painter, const QRect &rect, const CommitNode &node) const {
    const double top = rect.top();
    const double bottom = rect.top() + rect.height();
    const double middle = rect.top() + rect.height() / 2.0;
    const double nodeX = laneCenterX(rect, node.lane);

    painter->setBrush(Qt::NoBrush);

    auto applyPen = [painter](const QColor &color) {
        painter->setPen(QPen(color, 2, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
    };

    // 1. Branches that just run through this row.
    for (const int lane : node.passThroughLanes) {
        const double x = laneCenterX(rect, lane);
        applyPen(GraphLayoutEngine::colorForLane(lane));
        painter->drawLine(QPointF(x, top), QPointF(x, bottom));
    }

    // 2. Lines arriving from the row above, ending at the commit dot.
    for (const LaneSegment &segment : node.incoming) {
        const double fromX = laneCenterX(rect, segment.fromLane);
        const double toX = laneCenterX(rect, segment.toLane);

        QPainterPath path;
        path.moveTo(fromX, top);
        if (segment.fromLane == segment.toLane) {
            path.lineTo(toX, middle);
        } else {
            // S-curve that enters and leaves vertically, so it joins the neighbouring rows smoothly.
            path.cubicTo(QPointF(fromX, middle), QPointF(toX, top), QPointF(toX, middle));
        }
        applyPen(segment.color);
        painter->drawPath(path);
    }

    // 3. Lines leaving the commit dot towards its parents.
    for (const Connection &conn : node.connections) {
        const double toX = laneCenterX(rect, conn.targetLane);

        QPainterPath path;
        path.moveTo(nodeX, middle);
        if (conn.targetLane == node.lane) {
            path.lineTo(toX, bottom);
        } else {
            path.cubicTo(QPointF(nodeX, bottom), QPointF(toX, middle), QPointF(toX, bottom));
        }
        applyPen(conn.color);
        painter->drawPath(path);
    }
}

void GraphLaneDelegate::drawNode(QPainter *painter, const QRect &rect, const CommitNode &node,
                                 const QPalette &palette) const {
    const QPointF center(laneCenterX(rect, node.lane), rect.top() + rect.height() / 2.0);
    const QColor color = GraphLayoutEngine::colorForLane(node.lane);

    painter->setBrush(color);
    if (node.isHead) {
        // Ring around the HEAD commit, in the theme's text color so it shows on light and dark themes.
        painter->setPen(QPen(palette.color(QPalette::Text), 2));
        painter->drawEllipse(center, NodeRadius + 2.0, NodeRadius + 2.0);
    } else {
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(center, static_cast<double>(NodeRadius), static_cast<double>(NodeRadius));
    }
}

void GraphLaneDelegate::drawBadges(QPainter *painter, const QRect &rect, const QFont &baseFont,
                                   const CommitNode &node) const {
    const std::vector<Badge> badges = badgesFor(node);
    if (badges.empty()) {
        return;
    }

    const QFont font = badgeFont(baseFont);
    const QFontMetrics metrics(font);
    painter->setFont(font);

    int x = rect.left() + lanesWidth(node);
    const int y = rect.top() + 3;
    const int height = rect.height() - 6;

    for (const Badge &badge : badges) {
        const int width = metrics.horizontalAdvance(badge.text) + 2 * BadgePadding;
        const QRect badgeRect(x, y, width, height);

        QPainterPath path;
        path.addRoundedRect(badgeRect, 3, 3);
        painter->fillPath(path, badge.background);

        painter->setPen(badge.foreground);
        painter->drawText(badgeRect, Qt::AlignCenter, badge.text);

        x += width + BadgeSpacing;
        if (x > rect.right()) {
            break;   // the rest would be clipped anyway
        }
    }
}

} // namespace Git
