#include "effect.h"

#include "core/output.h"
#include "effect/effecthandler.h"
#include "effect/quickeffect.h"

#include <QQuickItem>
#include <QStandardPaths>
#include <QVariantMap>

namespace Tiles
{

QVariantMap Effect::initialProperties(KWin::LogicalOutput *screen)
{
    const OutputSettings settings = settingsForOutput(screen);
    const KWin::RectF screenGeometry = screen ? screen->geometryF() : KWin::RectF();
    const KWin::RectF workArea = workAreaForOutput(screen);
    QVariantMap properties;
    properties.insert(QStringLiteral("overlayVisible"), m_snapActive && screen == m_activeOutput);
    properties.insert(QStringLiteral("columns"), settings.grid.columns);
    properties.insert(QStringLiteral("rows"), settings.grid.rows);
    properties.insert(QStringLiteral("gridColor"), m_colors.gridColor);
    properties.insert(QStringLiteral("selectionColor"), m_colors.selectionColor);
    properties.insert(QStringLiteral("selectionBorderColor"), m_colors.selectionBorderColor);
    properties.insert(QStringLiteral("workAreaX"), workArea.x() - screenGeometry.x());
    properties.insert(QStringLiteral("workAreaY"), workArea.y() - screenGeometry.y());
    properties.insert(QStringLiteral("workAreaWidth"), workArea.width());
    properties.insert(QStringLiteral("workAreaHeight"), workArea.height());

    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    if (m_snapActive && screen == m_activeOutput) {
        if (const auto selection = normalizedSelection()) {
            left = selection->anchor.column;
            top = selection->anchor.row;
            right = selection->focus.column + 1;
            bottom = selection->focus.row + 1;
        }
    }
    properties.insert(QStringLiteral("selectionLeft"), left);
    properties.insert(QStringLiteral("selectionTop"), top);
    properties.insert(QStringLiteral("selectionRight"), right);
    properties.insert(QStringLiteral("selectionBottom"), bottom);
    return properties;
}

void Effect::updateOverlayViews()
{
    const auto screens = KWin::effects->screens();
    for (KWin::LogicalOutput *screen : screens) {
        updateOverlayView(screen);
    }

    if (m_snapActive) {
        KWin::effects->addRepaintFull();
    }
}

void Effect::updateOverlayView(KWin::LogicalOutput *screen)
{
    if (!screen || !isRunning()) {
        return;
    }

    KWin::QuickSceneView *view = viewForScreen(screen);
    if (!view || !view->rootItem()) {
        return;
    }

    QQuickItem *root = view->rootItem();
    const OutputSettings settings = settingsForOutput(screen);
    const KWin::RectF screenGeometry = screen->geometryF();
    const KWin::RectF workArea = workAreaForOutput(screen);
    root->setProperty("overlayVisible", m_snapActive && screen == m_activeOutput);
    root->setProperty("columns", settings.grid.columns);
    root->setProperty("rows", settings.grid.rows);
    root->setProperty("gridColor", m_colors.gridColor);
    root->setProperty("selectionColor", m_colors.selectionColor);
    root->setProperty("selectionBorderColor", m_colors.selectionBorderColor);
    root->setProperty("workAreaX", workArea.x() - screenGeometry.x());
    root->setProperty("workAreaY", workArea.y() - screenGeometry.y());
    root->setProperty("workAreaWidth", workArea.width());
    root->setProperty("workAreaHeight", workArea.height());

    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    if (m_snapActive && screen == m_activeOutput) {
        if (const auto selection = normalizedSelection()) {
            left = selection->anchor.column;
            top = selection->anchor.row;
            right = selection->focus.column + 1;
            bottom = selection->focus.row + 1;
        }
    }

    root->setProperty("selectionLeft", left);
    root->setProperty("selectionTop", top);
    root->setProperty("selectionRight", right);
    root->setProperty("selectionBottom", bottom);
    view->scheduleRepaint();
}

QString Effect::overlayQmlPath() const
{
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  QStringLiteral("kwin/effects/kwin-clicktile/contents/ui/main.qml"));
}

} // namespace Tiles
