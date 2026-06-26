#pragma once

#include <QColor>
#include <QRectF>
#include <QString>

#include <KSharedConfig>

namespace Tiles
{

inline constexpr int MinGridDimension = 1;
inline constexpr int MaxGridDimension = 64;

struct TileGrid {
    int columns = 2;
    int rows = 2;
};

struct OverlayColors {
    QColor gridColor;
    QColor selectionColor;
    QColor selectionBorderColor;
};

struct OutputSettings {
    QString key;
    QString token;
    QString label;
    TileGrid grid;
};

QString effectConfigGroupName();
QString outputKey(const QString &name, const QString &manufacturer, const QString &model, const QString &serialNumber);
QString outputToken(const QString &key);
QString outputLabel(const QString &name, const QString &manufacturer, const QString &model, const QString &serialNumber);
TileGrid sanitizeGrid(int columns, int rows);
TileGrid defaultGridForGeometry(const QRectF &geometry);
OverlayColors defaultOverlayColors();

KSharedConfigPtr openKWinConfig();
OverlayColors readOverlayColors(const KSharedConfigPtr &config);
void writeOverlayColors(const KSharedConfigPtr &config, const OverlayColors &settings);
OutputSettings readOutputSettings(const KSharedConfigPtr &config, const QString &key, const QString &label, const QRectF &geometry);
void writeOutputSettings(const KSharedConfigPtr &config, const OutputSettings &settings);

} // namespace Tiles
