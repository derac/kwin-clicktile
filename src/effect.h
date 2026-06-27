#pragma once

#include "settings.h"
#include "effect/effect.h"
#include "input.h"

#include <QFile>
#include <QPointer>
#include <QSet>
#include <QString>
#include <memory>
#include <optional>

#include <KConfigWatcher>

namespace Tiles
{

class InputFilter;

struct Tile {
    int column = 0;
    int row = 0;

    bool operator==(const Tile &) const = default;
};

struct TileSelection {
    Tile anchor;
    Tile focus;

    bool operator==(const TileSelection &) const = default;
};

class Effect : public KWin::Effect
{
    Q_OBJECT

public:
    Effect();
    ~Effect() override;

    static bool supported();
    static bool enabledByDefault();

    void reconfigure(ReconfigureFlags flags) override;
    void paintScreen(const KWin::RenderTarget &renderTarget,
                     const KWin::RenderViewport &viewport,
                     int mask,
                     const KWin::Region &deviceRegion,
                     KWin::LogicalOutput *screen) override;
    bool isActive() const override;
    bool blocksDirectScanout() const override;

private:
    friend class InputFilter;

    // Input and native drag tracking.
    bool filterPointerMotion(KWin::PointerMotionEvent *event);
    bool filterPointerButton(KWin::PointerButtonEvent *event);
    void wireWindow(KWin::EffectWindow *window);
    void unwireWindow(KWin::EffectWindow *window);
    void onMoveResizeStarted(KWin::EffectWindow *window);
    void onMoveResizeFinished(KWin::EffectWindow *window);

    // Grid selection and deferred window placement.
    bool beginSelection(const QPointF &point);
    void updateSelection(const QPointF &point);
    void finishSelection(const QPointF &point, const QString &reason);
    void cancelSelection(const QString &reason);
    void clearSelectionState();
    void queueFinalSnap(KWin::EffectWindow *window, const KWin::RectF &rect, const QString &reason);
    void schedulePendingSnap(const QString &reason);
    void applySnapRect(KWin::EffectWindow *window, const KWin::RectF &rect, const QString &reason);

    // Overlay rendering.
    void updateOverlayViews();
    void drawOverlay(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport, KWin::LogicalOutput *screen, int mask, const KWin::Region &deviceRegion);
    bool drawGlRect(const KWin::RenderViewport &viewport, const KWin::RectF &rect, const QColor &color);
    int drawGridGeometry(const KWin::RenderViewport &viewport, KWin::LogicalOutput *screen);

    // Output/grid helpers.
    KWin::LogicalOutput *outputForPoint(const QPointF &point) const;
    KWin::RectF workAreaForOutput(KWin::LogicalOutput *output) const;
    OutputSettings settingsForOutput(KWin::LogicalOutput *output) const;
    Tile cellAt(KWin::LogicalOutput *output, const QPointF &point) const;
    std::optional<KWin::RectF> currentSelectionRect() const;
    std::optional<TileSelection> normalizedSelection() const;
    QString overlayQmlPath() const;

    // Diagnostics.
    void openLogFile();
    void log(const QString &message);
    QString describeOutput(KWin::LogicalOutput *output) const;
    QString describeWindow(KWin::EffectWindow *window) const;
    QString describeRect(const KWin::RectF &rect) const;
    QString describeButtons(Qt::MouseButtons buttons) const;
    QString describeButton(Qt::MouseButton button) const;
    QString describeButtonState(KWin::PointerButtonState state) const;

    QSet<KWin::EffectWindow *> m_wiredWindows;
    QPointer<KWin::EffectWindow> m_dragWindow;
    QPointer<KWin::EffectWindow> m_snapWindow;
    QPointer<KWin::EffectWindow> m_pendingSnapWindow;
    QPointer<KWin::LogicalOutput> m_activeOutput;
    std::unique_ptr<InputFilter> m_inputFilter;
    KSharedConfigPtr m_config;
    KConfigWatcher::Ptr m_configWatcher;
    OverlayColors m_colors = defaultOverlayColors();
    OutputSettings m_activeSettings;
    std::optional<TileSelection> m_selection;
    std::optional<KWin::RectF> m_pendingSnapRect;
    QString m_pendingSnapReason;
    QFile m_logFile;
    bool m_sawRightPress = false;
    bool m_snapActive = false;
    bool m_suppressNextRightRelease = false;
    bool m_loggedNoOverlayRenderer = false;
    bool m_loggedOverlayPaintForSelection = false;
};

} // namespace Tiles
