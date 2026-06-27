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

bool rectsIntersect(const KWin::RectF &first, const KWin::RectF &second)
{
    return !first.isEmpty()
        && !second.isEmpty()
        && first.left() < second.right()
        && first.right() > second.left()
        && first.top() < second.bottom()
        && first.bottom() > second.top();
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
    m_anchorOutput = outputForPoint(point);
    if (!m_anchorOutput) {
        log(QStringLiteral("selection_begin_failed reason=no_output"));
        clearSelectionState();
        return false;
    }

    m_anchorSettings = settingsForOutput(m_anchorOutput);
    m_activeOutput = m_anchorOutput;
    m_activeSettings = m_anchorSettings;
    const Tile anchor = cellAt(m_anchorOutput, m_anchorSettings, point);
    m_selection = TileSelection{anchor, anchor};
    m_snapActive = true;
    m_loggedNoOverlayRenderer = false;
    m_loggedOverlayPaintForSelection = false;
    beginLivePreview();
    endNativeDragForLivePreview(QStringLiteral("selection_begin"));

    log(QStringLiteral("selection_begin window=%1 anchor_output=%2 focus_output=%3 anchor_token=%4 focus_token=%5 anchor_grid=%6x%7 focus_grid=%8x%9 anchor=%10 point=%11,%12")
            .arg(describeWindow(m_snapWindow),
                 describeOutput(m_anchorOutput),
                 describeOutput(m_activeOutput),
                 m_anchorSettings.token,
                 m_activeSettings.token)
            .arg(m_anchorSettings.grid.columns)
            .arg(m_anchorSettings.grid.rows)
            .arg(m_activeSettings.grid.columns)
            .arg(m_activeSettings.grid.rows)
            .arg(cellString(anchor))
            .arg(point.x(), 0, 'f', 1)
            .arg(point.y(), 0, 'f', 1));

    updateLivePreview(QStringLiteral("selection_begin"));
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

    const OutputSettings nextSettings = settingsForOutput(output);
    if (!m_selection) {
        const Tile cell = cellAt(output, nextSettings, point);
        m_selection = TileSelection{cell, cell};
        m_anchorOutput = output;
        m_anchorSettings = nextSettings;
        m_activeOutput = output;
        m_activeSettings = nextSettings;
        updateLivePreview(QStringLiteral("selection_reseed"));
        updateOverlayViews();
        return;
    }

    const bool outputChanged = output != m_activeOutput;
    TileSelection next = *m_selection;
    next.focus = cellAt(output, nextSettings, point);
    if (!outputChanged && next == m_selection) {
        return;
    }

    m_activeOutput = output;
    m_activeSettings = nextSettings;
    m_selection = next;
    if (outputChanged) {
        m_loggedOverlayPaintForSelection = false;
    }

    const auto rect = currentSelectionRect();
    log(QStringLiteral("selection_update anchor_output=%1 focus_output=%2 anchor_token=%3 focus_token=%4 anchor_grid=%5x%6 focus_grid=%7x%8 anchor=%9 focus=%10 cross_output=%11 rect=%12")
            .arg(describeOutput(m_anchorOutput),
                 describeOutput(m_activeOutput),
                 m_anchorSettings.token,
                 m_activeSettings.token)
            .arg(m_anchorSettings.grid.columns)
            .arg(m_anchorSettings.grid.rows)
            .arg(m_activeSettings.grid.columns)
            .arg(m_activeSettings.grid.rows)
            .arg(cellString(m_selection->anchor),
                 cellString(m_selection->focus),
                 m_anchorOutput == m_activeOutput ? QStringLiteral("false") : QStringLiteral("true"),
                 rect ? describeRect(*rect) : QStringLiteral("<none>")));
    updateLivePreview(QStringLiteral("selection_update"));
    updateOverlayViews();
}

void Effect::finishSelection(const QPointF &point, const QString &reason)
{
    if (!m_snapActive) {
        return;
    }

    updateSelection(point);
    const auto rect = currentSelectionRect();
    log(QStringLiteral("selection_finish reason=%1 window=%2 anchor_output=%3 focus_output=%4 rect=%5")
            .arg(reason,
                 describeWindow(m_snapWindow),
                 describeOutput(m_anchorOutput),
                 describeOutput(m_activeOutput),
                 rect ? describeRect(*rect) : QStringLiteral("<none>")));

    if (rect && m_snapWindow) {
        queueFinalSnap(m_snapWindow, *rect, reason);
    }

    finishLivePreview(false, reason);
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
    finishLivePreview(true, reason);
    clearSelectionState();
    updateOverlayViews();
    KWin::effects->addRepaintFull();
}

void Effect::clearSelectionState()
{
    m_snapActive = false;
    m_snapWindow.clear();
    m_anchorOutput.clear();
    m_activeOutput.clear();
    m_selection.reset();
    m_anchorSettings = OutputSettings{};
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

Tile Effect::cellAt(KWin::LogicalOutput *output, const OutputSettings &settings, const QPointF &point) const
{
    const KWin::RectF area = workAreaForOutput(output);
    const TileGrid grid = sanitizeGrid(settings.grid.columns, settings.grid.rows);

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

KWin::RectF Effect::cellRectForOutput(KWin::LogicalOutput *output, const OutputSettings &settings, const Tile &cell) const
{
    const KWin::RectF area = workAreaForOutput(output);
    if (area.isEmpty()) {
        return KWin::RectF();
    }

    const TileGrid grid = sanitizeGrid(settings.grid.columns, settings.grid.rows);
    const int column = std::clamp(cell.column, 0, grid.columns - 1);
    const int row = std::clamp(cell.row, 0, grid.rows - 1);

    const qreal left = area.left() + area.width() * column / grid.columns;
    const qreal right = area.left() + area.width() * (column + 1) / grid.columns;
    const qreal top = area.top() + area.height() * row / grid.rows;
    const qreal bottom = area.top() + area.height() * (row + 1) / grid.rows;
    return KWin::RectF(left, top, right - left, bottom - top);
}

bool Effect::shouldPaintOverlayForOutput(KWin::LogicalOutput *output) const
{
    if (!output || !m_snapActive || !m_selection) {
        return false;
    }

    if (output == m_anchorOutput || output == m_activeOutput) {
        return true;
    }

    const auto selection = currentSelectionRect();
    return selection && rectsIntersect(*selection, workAreaForOutput(output));
}

std::optional<KWin::RectF> Effect::currentSelectionRect() const
{
    if (!m_snapActive || !m_anchorOutput || !m_activeOutput || !m_selection) {
        return std::nullopt;
    }

    const KWin::RectF anchorRect = cellRectForOutput(m_anchorOutput, m_anchorSettings, m_selection->anchor);
    const KWin::RectF focusRect = cellRectForOutput(m_activeOutput, m_activeSettings, m_selection->focus);
    if (anchorRect.isEmpty() || focusRect.isEmpty()) {
        return std::nullopt;
    }

    const qreal left = std::min(anchorRect.left(), focusRect.left());
    const qreal top = std::min(anchorRect.top(), focusRect.top());
    const qreal right = std::max(anchorRect.right(), focusRect.right());
    const qreal bottom = std::max(anchorRect.bottom(), focusRect.bottom());
    return KWin::RectF(left, top, right - left, bottom - top);
}

} // namespace Tiles
