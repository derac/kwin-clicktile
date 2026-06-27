#include "effect.h"

#include "core/output.h"
#include "effect/effecthandler.h"
#include "effect/effectwindow.h"
#include "window.h"

#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Tiles
{

namespace
{

qreal distanceToRectSquared(const QPointF &point, const KWin::RectF &rect)
{
    const qreal x = point.x();
    const qreal y = point.y();
    const qreal dx = x < rect.left() ? rect.left() - x : (x > rect.right() ? x - rect.right() : 0.0);
    const qreal dy = y < rect.top() ? rect.top() - y : (y > rect.bottom() ? y - rect.bottom() : 0.0);
    return dx * dx + dy * dy;
}

QString cellString(const Tile &cell)
{
    return QStringLiteral("%1,%2").arg(cell.column).arg(cell.row);
}

} // namespace

bool Effect::beginSelection(const QPointF &point)
{
    if (!m_dragWindow || !m_dragWindow->window()) {
        log(QStringLiteral("selection_begin_failed reason=no_drag_window"));
        return false;
    }

    m_snapWindow = m_dragWindow;
    m_activeOutput = outputForPoint(point);
    if (!m_activeOutput) {
        log(QStringLiteral("selection_begin_failed reason=no_output"));
        clearSelectionState();
        return false;
    }

    m_activeSettings = settingsForOutput(m_activeOutput);
    const Tile anchor = cellAt(m_activeOutput, point);
    m_selection = TileSelection{anchor, anchor};
    m_snapActive = true;
    m_loggedNoOverlayRenderer = false;
    m_loggedOverlayPaintForSelection = false;

    log(QStringLiteral("selection_begin window=%1 output=%2 grid=%3x%4 anchor=%5 point=%6,%7")
            .arg(describeWindow(m_snapWindow),
                 describeOutput(m_activeOutput))
            .arg(m_activeSettings.grid.columns)
            .arg(m_activeSettings.grid.rows)
            .arg(cellString(anchor))
            .arg(point.x(), 0, 'f', 1)
            .arg(point.y(), 0, 'f', 1));

    updateOverlayViews();
    return true;
}

void Effect::updateSelection(const QPointF &point)
{
    if (!m_snapActive || !m_snapWindow) {
        return;
    }

    KWin::LogicalOutput *output = outputForPoint(point);
    if (!output) {
        return;
    }

    const bool outputChanged = output != m_activeOutput;
    if (outputChanged) {
        m_activeOutput = output;
        m_activeSettings = settingsForOutput(output);
        const Tile cell = cellAt(output, point);
        m_selection = TileSelection{cell, cell};
        log(QStringLiteral("selection_output_changed output=%1 grid=%2x%3 anchor=%4")
                .arg(describeOutput(output))
                .arg(m_activeSettings.grid.columns)
                .arg(m_activeSettings.grid.rows)
                .arg(cellString(cell)));
        updateOverlayViews();
        return;
    }

    if (!m_selection) {
        const Tile cell = cellAt(output, point);
        m_selection = TileSelection{cell, cell};
        updateOverlayViews();
        return;
    }

    TileSelection next = *m_selection;
    next.focus = cellAt(output, point);
    if (next == m_selection) {
        return;
    }

    m_selection = next;
    const auto rect = currentSelectionRect();
    log(QStringLiteral("selection_update output=%1 anchor=%2 focus=%3 rect=%4")
            .arg(describeOutput(m_activeOutput),
                 cellString(m_selection->anchor),
                 cellString(m_selection->focus),
                 rect ? describeRect(*rect) : QStringLiteral("<none>")));
    updateOverlayViews();
}

void Effect::finishSelection(const QPointF &point, const QString &reason)
{
    if (!m_snapActive) {
        return;
    }

    updateSelection(point);
    const auto rect = currentSelectionRect();
    log(QStringLiteral("selection_finish reason=%1 window=%2 output=%3 rect=%4")
            .arg(reason,
                 describeWindow(m_snapWindow),
                 describeOutput(m_activeOutput),
                 rect ? describeRect(*rect) : QStringLiteral("<none>")));

    if (rect && m_snapWindow) {
        queueFinalSnap(m_snapWindow, *rect, reason);
    }

    clearSelectionState();
    updateOverlayViews();
    KWin::effects->addRepaintFull();

    if (!m_dragWindow || m_dragWindow != m_pendingSnapWindow) {
        schedulePendingSnap(QStringLiteral("selection_finish_without_active_native_drag"));
    } else {
        log(QStringLiteral("snap_apply_deferred reason=%1 waiting_for_native_drag_finish window=%2 target=%3")
                .arg(reason,
                     describeWindow(m_pendingSnapWindow),
                     m_pendingSnapRect ? describeRect(*m_pendingSnapRect) : QStringLiteral("<none>")));
    }
}

void Effect::cancelSelection(const QString &reason)
{
    if (!m_snapActive && !m_snapWindow) {
        return;
    }

    log(QStringLiteral("selection_cancel reason=%1 window=%2").arg(reason, describeWindow(m_snapWindow)));
    clearSelectionState();
    updateOverlayViews();
    KWin::effects->addRepaintFull();
}

void Effect::clearSelectionState()
{
    m_snapActive = false;
    m_snapWindow.clear();
    m_activeOutput.clear();
    m_selection.reset();
    m_activeSettings = OutputSettings{};
}

void Effect::queueFinalSnap(KWin::EffectWindow *window, const KWin::RectF &rect, const QString &reason)
{
    m_pendingSnapWindow = window;
    m_pendingSnapRect = rect;
    m_pendingSnapReason = reason;
    log(QStringLiteral("snap_apply_queued reason=%1 window=%2 target=%3")
            .arg(reason,
                 describeWindow(window),
                 describeRect(rect)));
}

void Effect::schedulePendingSnap(const QString &reason)
{
    if (!m_pendingSnapWindow || !m_pendingSnapRect) {
        return;
    }

    const QPointer<KWin::EffectWindow> window = m_pendingSnapWindow;
    const KWin::RectF rect = *m_pendingSnapRect;
    const QString queuedReason = m_pendingSnapReason;
    m_pendingSnapWindow.clear();
    m_pendingSnapRect.reset();
    m_pendingSnapReason.clear();

    log(QStringLiteral("snap_apply_scheduled reason=%1 queued_reason=%2 window=%3 target=%4")
            .arg(reason,
                 queuedReason,
                 describeWindow(window),
                 describeRect(rect)));

    QTimer::singleShot(0, this, [this, window, rect, queuedReason] {
        applySnapRect(window, rect, queuedReason + QStringLiteral(":settle0"));
    });
    QTimer::singleShot(80, this, [this, window, rect, queuedReason] {
        applySnapRect(window, rect, queuedReason + QStringLiteral(":settle80"));
    });
}

void Effect::applySnapRect(KWin::EffectWindow *effectWindow, const KWin::RectF &rect, const QString &reason)
{
    if (!effectWindow) {
        log(QStringLiteral("snap_resize_skip reason=no_effect_window apply_reason=%1 target=%2")
                .arg(reason, describeRect(rect)));
        return;
    }

    if (!effectWindow->window()) {
        log(QStringLiteral("snap_resize_skip reason=no_core_window apply_reason=%1 window=%2 target=%3")
                .arg(reason,
                     describeWindow(effectWindow),
                     describeRect(rect)));
        return;
    }

    KWin::Window *window = effectWindow->window();
    const KWin::RectF before = effectWindow->frameGeometry();
    log(QStringLiteral("snap_resize_attempt apply_reason=%1 window=%2 before=%3 target=%4")
            .arg(reason,
                 describeWindow(effectWindow),
                 describeRect(before),
                 describeRect(rect)));

    window->setMaximize(false, false);
    window->setQuickTileMode(KWin::QuickTileMode{KWin::QuickTileFlag::None}, rect.center());
    window->moveResize(rect);

    const KWin::RectF after = effectWindow->frameGeometry();
    log(QStringLiteral("snap_resize_result apply_reason=%1 window=%2 target=%3 after=%4 changed=%5")
            .arg(reason,
                 describeWindow(effectWindow),
                 describeRect(rect),
                 describeRect(after),
                 after == before ? QStringLiteral("false") : QStringLiteral("true")));
}

KWin::LogicalOutput *Effect::outputForPoint(const QPointF &point) const
{
    if (KWin::LogicalOutput *output = KWin::effects->screenAt(point.toPoint())) {
        return output;
    }

    KWin::LogicalOutput *nearest = nullptr;
    qreal nearestDistance = std::numeric_limits<qreal>::max();
    const auto screens = KWin::effects->screens();
    for (KWin::LogicalOutput *screen : screens) {
        const qreal distance = distanceToRectSquared(point, screen->geometryF());
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = screen;
        }
    }
    return nearest;
}

KWin::RectF Effect::workAreaForOutput(KWin::LogicalOutput *output) const
{
    if (!output) {
        return KWin::RectF();
    }

    KWin::RectF area = KWin::effects->clientArea(KWin::MaximizeArea, output);
    if (area.isEmpty()) {
        area = output->geometryF();
    }
    return area;
}

OutputSettings Effect::settingsForOutput(KWin::LogicalOutput *output) const
{
    if (!output) {
        return OutputSettings{
            QStringLiteral("unknown-output"),
            outputToken(QStringLiteral("unknown-output")),
            QStringLiteral("Unknown monitor"),
            TileGrid{2, 2},
        };
    }

    const QString key = outputKey(output->name(), output->manufacturer(), output->model(), output->serialNumber());
    const QString label = outputLabel(output->name(), output->manufacturer(), output->model(), output->serialNumber());
    return readOutputSettings(m_config ? m_config : openKWinConfig(), key, label, output->geometryF());
}

Tile Effect::cellAt(KWin::LogicalOutput *output, const QPointF &point) const
{
    const KWin::RectF area = workAreaForOutput(output);
    const TileGrid grid = settingsForOutput(output).grid;

    if (area.isEmpty()) {
        return Tile{0, 0};
    }

    const qreal x = std::clamp(point.x(), area.left(), std::nextafter(area.right(), area.left()));
    const qreal y = std::clamp(point.y(), area.top(), std::nextafter(area.bottom(), area.top()));
    const qreal relativeX = x - area.left();
    const qreal relativeY = y - area.top();

    int column = static_cast<int>(std::floor(relativeX * grid.columns / area.width()));
    int row = static_cast<int>(std::floor(relativeY * grid.rows / area.height()));
    column = std::clamp(column, 0, grid.columns - 1);
    row = std::clamp(row, 0, grid.rows - 1);
    return Tile{column, row};
}

std::optional<KWin::RectF> Effect::currentSelectionRect() const
{
    if (!m_snapActive || !m_activeOutput || !m_selection) {
        return std::nullopt;
    }

    const KWin::RectF area = workAreaForOutput(m_activeOutput);
    if (area.isEmpty()) {
        return std::nullopt;
    }

    const TileGrid grid = m_activeSettings.grid;
    const int leftCell = std::min(m_selection->anchor.column, m_selection->focus.column);
    const int topCell = std::min(m_selection->anchor.row, m_selection->focus.row);
    const int rightCell = std::max(m_selection->anchor.column, m_selection->focus.column) + 1;
    const int bottomCell = std::max(m_selection->anchor.row, m_selection->focus.row) + 1;

    const qreal left = area.left() + area.width() * leftCell / grid.columns;
    const qreal right = area.left() + area.width() * rightCell / grid.columns;
    const qreal top = area.top() + area.height() * topCell / grid.rows;
    const qreal bottom = area.top() + area.height() * bottomCell / grid.rows;
    return KWin::RectF(left, top, right - left, bottom - top);
}

std::optional<TileSelection> Effect::normalizedSelection() const
{
    if (!m_selection) {
        return std::nullopt;
    }

    const Tile anchor{
        std::min(m_selection->anchor.column, m_selection->focus.column),
        std::min(m_selection->anchor.row, m_selection->focus.row),
    };
    const Tile focus{
        std::max(m_selection->anchor.column, m_selection->focus.column),
        std::max(m_selection->anchor.row, m_selection->focus.row),
    };
    return TileSelection{anchor, focus};
}

} // namespace Tiles
