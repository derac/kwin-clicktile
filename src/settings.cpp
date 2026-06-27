#include "settings.h"

#include <QCryptographicHash>
#include <QStringList>

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

QString outputNameFromKey(const QString &key)
{
    const QString name = key.section(QLatin1Char('|'), 0, 0).trimmed();
    return name.isEmpty() ? key.trimmed() : name;
}

QString outputSettingsToken(const QString &key)
{
    const QString name = outputNameFromKey(key);
    return outputToken(name.isEmpty() ? key : name);
}

bool hasGridForToken(const KConfigGroup &group, const QString &token)
{
    return !token.isEmpty()
        && (group.hasKey(outputColumnsEntry(token)) || group.hasKey(outputRowsEntry(token)));
}

TileGrid readGridForToken(const KConfigGroup &group, const QString &token, const TileGrid &defaults)
{
    return sanitizeGrid(group.readEntry(outputColumnsEntry(token), defaults.columns),
                        group.readEntry(outputRowsEntry(token), defaults.rows));
}

void writeGridForToken(KConfigGroup &group, const QString &token, const OutputSettings &settings)
{
    if (token.isEmpty()) {
        return;
    }

    group.writeEntry(outputColumnsEntry(token), settings.grid.columns, KConfigBase::Notify);
    group.writeEntry(outputRowsEntry(token), settings.grid.rows, KConfigBase::Notify);
    group.writeEntry(outputLabelEntry(token), settings.label, KConfigBase::Notify);
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
        QColor(96, 190, 255, 205),
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
    const QString token = outputSettingsToken(key);
    const TileGrid defaults = defaultGridForGeometry(geometry);
    const KConfigGroup group(config, effectConfigGroupName());
    const QString previousToken = outputToken(key);
    const QString readToken = hasGridForToken(group, token) || previousToken == token
        ? token
        : previousToken;

    return OutputSettings{
        key,
        token,
        label,
        readGridForToken(group, readToken, defaults),
    };
}

void writeOutputSettings(const KSharedConfigPtr &config, const OutputSettings &settings)
{
    KConfigGroup group(config, effectConfigGroupName());
    const QString token = outputSettingsToken(settings.key);

    writeGridForToken(group, token, settings);
}

} // namespace Tiles
