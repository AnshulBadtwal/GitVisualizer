#include "delegates/GraphLaneDelegate.h"
#include "models/CommitTableModel.h"
#include "core/GraphLayoutEngine.h"

namespace Git {

GraphLaneDelegate::GraphLaneDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize GraphLaneDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(24);
    return size;
}

void GraphLaneDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    if (index.column() != GraphColumn::Graph) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QVariant varData = index.data(CustomRoles::CommitGraphNodeRole);
    if (!varData.canConvert<CommitNode>()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    CommitNode node = qvariant_cast<CommitNode>(varData);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Render selection background highlights
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    }

    // 1. Splines & Vector connections
    drawConnections(painter, option.rect, node);

    // 2. Commit Node (Circle)
    drawNode(painter, option.rect, node);

    // 3. Render Branch Badges & Tags
    drawBadges(painter, option.rect, node);

    painter->restore();
}

void GraphLaneDelegate::drawConnections(QPainter *painter, const QRect &rect, const CommitNode &node) const {
    int startX = rect.left() + (node.lane * LaneWidth) + (LaneWidth / 2);
    int startY = rect.center().y();

    for (const auto &conn : node.connections) {
        int targetX = rect.left() + (conn.targetLane * LaneWidth) + (LaneWidth / 2);
        int targetY = rect.bottom() + ((conn.targetRow - (rect.top() / rect.height())) * rect.height());

        QPen pen(conn.color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);

        QPainterPath path;
        path.moveTo(startX, startY);

        if (startX == targetX) {
            path.lineTo(targetX, targetY);
        } else {
            // Cubic Bezier curve for branch splits and merge joins
            int midY = startY + (rect.height() / 2);
            path.cubicTo(startX, midY, targetX, midY, targetX, targetY);
        }

        painter->drawPath(path);
    }
}

void GraphLaneDelegate::drawNode(QPainter *painter, const QRect &rect, const CommitNode &node) const {
    int cx = rect.left() + (node.lane * LaneWidth) + (LaneWidth / 2);
    int cy = rect.center().y();

    QColor color = GraphLayoutEngine::colorForLane(node.lane);

    if (node.isHead) {
        // Outline halo for HEAD commit
        painter->setPen(QPen(Qt::white, 2));
        painter->setBrush(color);
        painter->drawEllipse(QPoint(cx, cy), NodeRadius + 2, NodeRadius + 2);
    } else {
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawEllipse(QPoint(cx, cy), NodeRadius, NodeRadius);
    }
}

void GraphLaneDelegate::drawBadges(QPainter *painter, const QRect &rect, const CommitNode &node) const {
    if (node.branchNames.empty() && node.tagNames.empty()) {
        return;
    }

    int startX = rect.left() + (node.maxLanes * LaneWidth) + 8;
    int y = rect.top() + 3;
    int height = rect.height() - 6;

    QFont font = painter->font();
    font.setPointSize(8);
    font.setBold(true);
    painter->setFont(font);

    auto renderBadge = [&](const QString &text, const QColor &bgColor, const QColor &textColor) {
        QFontMetrics fm(font);
        int textWidth = fm.horizontalAdvance(text);
        int badgeWidth = textWidth + 10;

        QRect badgeRect(startX, y, badgeWidth, height);
        
        QPainterPath path;
        path.addRoundedRect(badgeRect, 3, 3);
        
        painter->fillPath(path, bgColor);
        painter->setPen(textColor);
        painter->drawText(badgeRect, Qt::AlignCenter, text);

        startX += badgeWidth + 4;
    };

    // Draw local/remote branches (Green badges for HEAD, dark gray for standard refs)
    for (const auto &branch : node.branchNames) {
        QString name = QString::fromUtf8(branch.c_str());
        QColor bg = node.isHead ? QColor(46, 204, 113) : QColor(52, 73, 94);
        renderBadge(name, bg, Qt::white);
    }

    // Draw tags (Amber badges)
    for (const auto &tag : node.tagNames) {
        QString name = QString::fromUtf8(tag.c_str());
        renderBadge(name, QColor(241, 196, 15), Qt::black);
    }
}

} // namespace Git
