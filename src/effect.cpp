#include "effect.h"

#include "input_filter.h"

#include "effect/effecthandler.h"
#include "effect/effectwindow.h"

#include <KConfigGroup>

namespace Tiles
{

namespace
{

constexpr const char *BuildTag = "kwin-clicktile_build=0.6.9";

} // namespace

Effect::Effect()
{
    log(QStringLiteral("build_marker %1").arg(QString::fromLatin1(BuildTag)));
    m_config = openKWinConfig();
    m_configWatcher = KConfigWatcher::create(m_config);
    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group, const QByteArrayList &) {
        if (group.name() == effectConfigGroupName()) {
            log(QStringLiteral("config_changed"));
            reconfigure(ReconfigureAll);
        }
    });
    reloadConfig();

    log(QStringLiteral("overlay_renderer type=passive_paint_screen"));

    m_inputFilter = std::make_unique<InputFilter>(this);
    KWin::input()->installInputEventFilter(m_inputFilter.get());
    log(QStringLiteral("input_filter_installed order=Effects behavior=consume_right_chord_passive_overlay"));

    connect(KWin::effects, &KWin::EffectsHandler::windowAdded, this, &Effect::wireWindow);
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this, &Effect::unwireWindow);
    connect(KWin::effects, &KWin::EffectsHandler::windowClosed, this, &Effect::unwireWindow);
    connect(KWin::effects, &KWin::EffectsHandler::screenRemoved, this, [this](KWin::LogicalOutput *screen) {
        if (m_anchorOutput == screen || m_activeOutput == screen) {
            cancelSelection(QStringLiteral("active_output_removed"));
        }
    });

    const auto windows = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *window : windows) {
        wireWindow(window);
    }

    log(QStringLiteral("loaded; wired_windows=%1").arg(m_wiredWindows.size()));
}

Effect::~Effect()
{
    cancelSelection(QStringLiteral("effect_unloaded"));
    if (m_inputFilter) {
        KWin::input()->uninstallInputEventFilter(m_inputFilter.get());
        m_inputFilter.reset();
    }
    log(QStringLiteral("input_filter_uninstalled"));
    log(QStringLiteral("unloaded"));
}

bool Effect::supported()
{
    return true;
}

bool Effect::enabledByDefault()
{
    return false;
}

void Effect::reconfigure(ReconfigureFlags flags)
{
    KWin::Effect::reconfigure(flags);
    reloadConfig();
}

void Effect::reloadConfig()
{
    if (!m_config) {
        m_config = openKWinConfig();
    }
    m_config->reparseConfiguration();
    m_colors = readOverlayColors(m_config);
    if (m_anchorOutput) {
        m_anchorSettings = settingsForOutput(m_anchorOutput);
    }
    if (m_activeOutput) {
        m_activeSettings = settingsForOutput(m_activeOutput);
    }
    updateOverlayViews();
    log(QStringLiteral("reconfigure colors grid=%1 selection=%2 border=%3")
            .arg(m_colors.gridColor.name(QColor::HexArgb),
                 m_colors.selectionColor.name(QColor::HexArgb),
                 m_colors.selectionBorderColor.name(QColor::HexArgb)));
}

bool Effect::isActive() const
{
    return m_snapActive;
}

bool Effect::blocksDirectScanout() const
{
    return m_snapActive;
}

} // namespace Tiles
