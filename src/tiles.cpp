#include "effect.h"

#include "core/output.h"
#include "effect/effecthandler.h"
#include "effect/effectwindow.h"

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

} // namespace

bool Effect::beginSelection(const QPointF &point)
{
    if (!m_dragWindow || !m_dragWindow->window()) {
        return false;
    }

    m_snapWindow = m_dragWindow;
    m_anchorOutput = outputForPoint(point);
    if (!m_anchorOutput) {
        clearSelectionState();
        return false;
    }

    m_anchorSettings = settingsForOutput(m_anchorOutput);
    m_activeOutput = m_anchorOutput;
    m_activeSettings = m_anchorSettings;
    const Tile anchor = cellAt(m_anchorOutput, m_anchorSettings, point);
    m_selection = TileSelection{anchor, anchor};
    m_snapActive = true;
    endNativeDragForSelection();

    moveWindowToSelection();
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
        moveWindowToSelection();
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
    moveWindowToSelection();
    updateOverlayViews();
}

void Effect::finishSelection(const QPointF &point)
{
    if (!m_snapActive) {
        return;
    }

    updateSelection(point);
    clearSelectionState();
    updateOverlayViews();
    KWin::effects->addRepaintFull();
}

void Effect::cancelSelection()
{
    if (!m_snapActive && !m_snapWindow) {
        return;
    }

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
