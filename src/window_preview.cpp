#include "effect.h"

#include "effect/effectwindow.h"
#include "window.h"

namespace Tiles
{

void Effect::endNativeDragForSelection(const QString &reason)
{
    if (!m_snapWindow || !m_snapWindow->window()) {
        log(QStringLiteral("selection_native_drag_end_skip reason=no_window update_reason=%1 window=%2")
                .arg(reason,
                     describeWindow(m_snapWindow)));
        return;
    }

    KWin::Window *window = m_snapWindow->window();
    if (!window->isInteractiveMove() && !window->isInteractiveResize()) {
        log(QStringLiteral("selection_native_drag_end_skip reason=not_interactive update_reason=%1 window=%2")
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
    }

    const bool stillInteractiveAfterCancel = window->isInteractiveMove() || window->isInteractiveResize();
    log(QStringLiteral("selection_native_drag_end reason=%1 window=%2 before=%3 after=%4 fallback_cancel=%5 still_interactive=%6")
            .arg(reason,
                 describeWindow(m_snapWindow),
                 describeRect(before),
                 describeRect(m_snapWindow ? m_snapWindow->frameGeometry() : KWin::RectF()),
                 stillInteractiveAfterEnd ? QStringLiteral("true") : QStringLiteral("false"),
                 stillInteractiveAfterCancel ? QStringLiteral("true") : QStringLiteral("false")));
}

void Effect::moveWindowToSelection(const QString &reason)
{
    if (!m_snapActive || !m_snapWindow) {
        return;
    }

    const auto target = currentSelectionRect();
    if (!target || target->isEmpty()) {
        return;
    }

    if (!m_snapWindow->window()) {
        log(QStringLiteral("selection_move_skip reason=no_core_window update_reason=%1 window=%2 target=%3")
                .arg(reason,
                     describeWindow(m_snapWindow),
                     describeRect(*target)));
        return;
    }

    KWin::Window *window = m_snapWindow->window();
    const KWin::RectF before = m_snapWindow->frameGeometry();
    window->setMaximize(false, false);
    window->setQuickTileMode(KWin::QuickTileMode{KWin::QuickTileFlag::None}, target->center());
    window->moveResize(*target);

    const KWin::RectF after = m_snapWindow->frameGeometry();
    log(QStringLiteral("selection_move reason=%1 window=%2 before=%3 target=%4 after=%5 changed=%6")
            .arg(reason,
                 describeWindow(m_snapWindow),
                 describeRect(before),
                 describeRect(*target),
                 describeRect(after),
                 after == before ? QStringLiteral("false") : QStringLiteral("true")));
}

} // namespace Tiles
