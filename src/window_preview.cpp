#include "effect.h"

#include "effect/effecthandler.h"
#include "effect/effectwindow.h"
#include "window.h"

#include <cmath>

namespace Tiles
{

namespace
{

bool rectsNearlyEqual(const KWin::RectF &first, const KWin::RectF &second)
{
    constexpr qreal tolerance = 0.5;
    return std::abs(first.left() - second.left()) < tolerance
        && std::abs(first.top() - second.top()) < tolerance
        && std::abs(first.width() - second.width()) < tolerance
        && std::abs(first.height() - second.height()) < tolerance;
}

} // namespace

void Effect::beginLivePreview()
{
    if (!m_snapWindow || !m_snapWindow->window()) {
        log(QStringLiteral("live_preview_begin_skip reason=no_window window=%1").arg(describeWindow(m_snapWindow)));
        return;
    }

    if (m_livePreviewWindow == m_snapWindow && m_livePreviewRestoreRect) {
        return;
    }

    const KWin::RectF restoreRect = m_dragGeometry && !m_dragGeometry->isEmpty()
        ? *m_dragGeometry
        : m_snapWindow->frameGeometry();
    if (restoreRect.isEmpty()) {
        log(QStringLiteral("live_preview_begin_skip reason=empty_restore window=%1").arg(describeWindow(m_snapWindow)));
        return;
    }

    m_livePreviewWindow = m_snapWindow;
    m_livePreviewRestoreRect = restoreRect;
    m_livePreviewLastRect.reset();

    log(QStringLiteral("live_preview_begin window=%1 restore=%2")
            .arg(describeWindow(m_livePreviewWindow),
                 describeRect(restoreRect)));
}

void Effect::endNativeDragForLivePreview(const QString &reason)
{
    if (!m_snapWindow || !m_snapWindow->window()) {
        log(QStringLiteral("live_preview_native_drag_end_skip reason=no_window update_reason=%1 window=%2")
                .arg(reason,
                     describeWindow(m_snapWindow)));
        return;
    }

    KWin::Window *window = m_snapWindow->window();
    if (!window->isInteractiveMove() && !window->isInteractiveResize()) {
        log(QStringLiteral("live_preview_native_drag_end_skip reason=not_interactive update_reason=%1 window=%2")
                .arg(reason,
                     describeWindow(m_snapWindow)));
        return;
    }

    const KWin::RectF before = m_snapWindow->frameGeometry();
    window->endInteractiveMoveResize();
    const bool stillInteractiveAfterEnd = window->isInteractiveMove() || window->isInteractiveResize();
    if (stillInteractiveAfterEnd) {
        window->cancelInteractiveMoveResize();
    }

    if (m_dragWindow == m_snapWindow) {
        m_dragWindow.clear();
        m_dragGeometry.reset();
    }

    const bool stillInteractiveAfterCancel = window->isInteractiveMove() || window->isInteractiveResize();
    log(QStringLiteral("live_preview_native_drag_end reason=%1 window=%2 before=%3 after=%4 fallback_cancel=%5 still_interactive=%6")
            .arg(reason,
                 describeWindow(m_snapWindow),
                 describeRect(before),
                 describeRect(m_snapWindow ? m_snapWindow->frameGeometry() : KWin::RectF()),
                 stillInteractiveAfterEnd ? QStringLiteral("true") : QStringLiteral("false"),
                 stillInteractiveAfterCancel ? QStringLiteral("true") : QStringLiteral("false")));
}

void Effect::updateLivePreview(const QString &reason)
{
    if (!m_snapActive || !m_snapWindow) {
        return;
    }

    if (m_livePreviewWindow != m_snapWindow || !m_livePreviewRestoreRect) {
        beginLivePreview();
    }

    const auto target = currentSelectionRect();
    if (!target || target->isEmpty()) {
        return;
    }

    if (!m_snapWindow->window()) {
        log(QStringLiteral("live_preview_move_skip reason=no_core_window update_reason=%1 window=%2 target=%3")
                .arg(reason,
                     describeWindow(m_snapWindow),
                     describeRect(*target)));
        return;
    }

    const KWin::RectF before = m_snapWindow->frameGeometry();
    if (m_livePreviewLastRect && rectsNearlyEqual(*m_livePreviewLastRect, *target) && rectsNearlyEqual(before, *target)) {
        return;
    }

    KWin::Window *window = m_snapWindow->window();
    m_applyingLivePreviewMove = true;
    window->setMaximize(false, false);
    window->setQuickTileMode(KWin::QuickTileMode{KWin::QuickTileFlag::None}, target->center());
    window->moveResize(*target);
    m_applyingLivePreviewMove = false;

    m_livePreviewLastRect = *target;

    const KWin::RectF after = m_snapWindow->frameGeometry();
    log(QStringLiteral("live_preview_move reason=%1 window=%2 before=%3 target=%4 after=%5 changed=%6")
            .arg(reason,
                 describeWindow(m_snapWindow),
                 describeRect(before),
                 describeRect(*target),
                 describeRect(after),
                 rectsNearlyEqual(before, after) ? QStringLiteral("false") : QStringLiteral("true")));
}

void Effect::finishLivePreview(bool restore, const QString &reason)
{
    const QPointer<KWin::EffectWindow> window = m_livePreviewWindow;
    const std::optional<KWin::RectF> restoreRect = m_livePreviewRestoreRect;
    const std::optional<KWin::RectF> lastRect = m_livePreviewLastRect;

    m_livePreviewWindow.clear();
    m_livePreviewRestoreRect.reset();
    m_livePreviewLastRect.reset();

    if (!restore) {
        log(QStringLiteral("live_preview_commit reason=%1 window=%2 last=%3")
                .arg(reason,
                     describeWindow(window),
                     lastRect ? describeRect(*lastRect) : QStringLiteral("<none>")));
        return;
    }

    if (!window || !restoreRect) {
        log(QStringLiteral("live_preview_restore_skip reason=%1 window=%2 restore=%3")
                .arg(reason,
                     describeWindow(window),
                     restoreRect ? describeRect(*restoreRect) : QStringLiteral("<none>")));
        return;
    }

    if (!window->window()) {
        log(QStringLiteral("live_preview_restore_skip reason=no_core_window cancel_reason=%1 window=%2 restore=%3")
                .arg(reason,
                     describeWindow(window),
                     describeRect(*restoreRect)));
        return;
    }

    const KWin::RectF before = window->frameGeometry();
    if (!rectsNearlyEqual(before, *restoreRect)) {
        m_applyingLivePreviewMove = true;
        window->window()->moveResize(*restoreRect);
        m_applyingLivePreviewMove = false;
    }

    log(QStringLiteral("live_preview_restore reason=%1 window=%2 before=%3 restore=%4 after=%5")
            .arg(reason,
                 describeWindow(window),
                 describeRect(before),
                 describeRect(*restoreRect),
                 describeRect(window->frameGeometry())));
}

} // namespace Tiles
