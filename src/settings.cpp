#include "settings.h"

#include <QCryptographicHash>

#include <KConfigGroup>

#include <algorithm>

namespace Tiles
{

namespace
{

QString outputColumnsEntry(const QString &token)
{
    return QStringLiteral("Output_%1_Columns").arg(token);
}

QString outputRowsEntry(const QString &token)
{
    return QStringLiteral("Output_%1_Rows").arg(token);
}

QString outputLabelEntry(const QString &token)
{
    return QStringLiteral("Output_%1_Label").arg(token);
}

} // namespace

QString effectConfigGroupName()
{
    return QStringLiteral("Effect-kwin-clicktile");
}

QString outputKey(const QString &name, const QString &manufacturer, const QString &model, const QString &serialNumber)
{
    QStringList parts;
    parts << name.trimmed() << manufacturer.trimmed() << model.trimmed() << serialNumber.trimmed();
    parts.removeAll(QString());
    return parts.isEmpty() ? QStringLiteral("unknown-output") : parts.join(QLatin1Char('|'));
}

QString outputToken(const QString &key)
{
    return QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex());
}

QString outputLabel(const QString &name, const QString &manufacturer, const QString &model, const QString &serialNumber)
{
    QStringList parts;
    if (!name.trimmed().isEmpty()) {
        parts << name.trimmed();
    }

    QStringList makerModelParts;
    if (!manufacturer.trimmed().isEmpty()) {
        makerModelParts << manufacturer.trimmed();
    }
    if (!model.trimmed().isEmpty()) {
        makerModelParts << model.trimmed();
    }
    const QString makerModel = makerModelParts.join(QLatin1Char(' '));
    if (!makerModel.isEmpty() && !parts.contains(makerModel)) {
        parts << makerModel;
    }

    if (!serialNumber.trimmed().isEmpty()) {
        parts << QStringLiteral("S/N %1").arg(serialNumber.trimmed());
    }

    return parts.isEmpty() ? QStringLiteral("Unknown monitor") : parts.join(QStringLiteral(" - "));
}

TileGrid sanitizeGrid(int columns, int rows)
{
    return TileGrid{
        std::clamp(columns, MinGridDimension, MaxGridDimension),
        std::clamp(rows, MinGridDimension, MaxGridDimension),
    };
}

TileGrid defaultGridForGeometry(const QRectF &geometry)
{
    if (geometry.height() > geometry.width()) {
        return TileGrid{1, 3};
    }
    return TileGrid{2, 2};
}

OverlayColors defaultOverlayColors()
{
    return OverlayColors{
        QColor(80, 200, 255, 58),
        QColor(80, 160, 255, 26),
        QColor(42, 143, 193, 120),
    };
}

KSharedConfigPtr openKWinConfig()
{
    return KSharedConfig::openConfig(QStringLiteral("kwinrc"), KConfig::NoGlobals);
}

OverlayColors readOverlayColors(const KSharedConfigPtr &config)
{
    const OverlayColors defaults = defaultOverlayColors();
    const KConfigGroup group(config, effectConfigGroupName());

    return OverlayColors{
        group.readEntry("GridColor", defaults.gridColor),
        group.readEntry("SelectionColor", defaults.selectionColor),
        group.readEntry("SelectionBorderColor", defaults.selectionBorderColor),
    };
}

void writeOverlayColors(const KSharedConfigPtr &config, const OverlayColors &settings)
{
    KConfigGroup group(config, effectConfigGroupName());
    group.writeEntry("GridColor", settings.gridColor, KConfigBase::Notify);
    group.writeEntry("SelectionColor", settings.selectionColor, KConfigBase::Notify);
    group.writeEntry("SelectionBorderColor", settings.selectionBorderColor, KConfigBase::Notify);
}

OutputSettings readOutputSettings(const KSharedConfigPtr &config, const QString &key, const QString &label, const QRectF &geometry)
{
    const QString token = outputToken(key);
    const TileGrid defaults = defaultGridForGeometry(geometry);
    const KConfigGroup group(config, effectConfigGroupName());

    return OutputSettings{
        key,
        token,
        label,
        sanitizeGrid(group.readEntry(outputColumnsEntry(token).toUtf8().constData(), defaults.columns),
                     group.readEntry(outputRowsEntry(token).toUtf8().constData(), defaults.rows)),
    };
}

void writeOutputSettings(const KSharedConfigPtr &config, const OutputSettings &settings)
{
    KConfigGroup group(config, effectConfigGroupName());
    group.writeEntry(outputColumnsEntry(settings.token).toUtf8().constData(), settings.grid.columns, KConfigBase::Notify);
    group.writeEntry(outputRowsEntry(settings.token).toUtf8().constData(), settings.grid.rows, KConfigBase::Notify);
    group.writeEntry(outputLabelEntry(settings.token).toUtf8().constData(), settings.label, KConfigBase::Notify);
}

} // namespace Tiles
