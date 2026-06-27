#include "settings.h"

#include <QCryptographicHash>
#include <QStringList>

#include <KConfigGroup>

#include <algorithm>
#include <optional>

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

QString outputNameToken(const QString &key)
{
    const QString name = outputNameFromKey(key);
    return name.isEmpty() ? QString() : outputToken(name);
}

std::optional<QString> tokenFromLabelEntry(const QString &entry)
{
    static const QString prefix = QStringLiteral("Output_");
    static const QString suffix = QStringLiteral("_Label");

    if (!entry.startsWith(prefix) || !entry.endsWith(suffix)) {
        return std::nullopt;
    }

    return entry.mid(prefix.size(), entry.size() - prefix.size() - suffix.size());
}

std::optional<QString> storedTokenForOutputName(const KConfigGroup &group, const QString &name)
{
    if (name.isEmpty()) {
        return std::nullopt;
    }

    for (const QString &entry : group.keyList()) {
        const auto token = tokenFromLabelEntry(entry);
        if (!token) {
            continue;
        }

        const QString storedLabel = group.readEntry(entry, QString());
        if (storedLabel == name || storedLabel.startsWith(name + QStringLiteral(" - "))) {
            return token;
        }
    }

    return std::nullopt;
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
    const QString nameToken = outputNameToken(key);
    const QString name = outputNameFromKey(key);

    if (hasGridForToken(group, token)) {
        return OutputSettings{
            key,
            token,
            label,
            readGridForToken(group, token, defaults),
        };
    }

    if (hasGridForToken(group, nameToken)) {
        return OutputSettings{
            key,
            nameToken,
            label,
            readGridForToken(group, nameToken, defaults),
        };
    }

    if (const auto storedToken = storedTokenForOutputName(group, name); storedToken && hasGridForToken(group, *storedToken)) {
        return OutputSettings{
            key,
            *storedToken,
            label,
            readGridForToken(group, *storedToken, defaults),
        };
    }

    return OutputSettings{
        key,
        token,
        label,
        defaults,
    };
}

void writeOutputSettings(const KSharedConfigPtr &config, const OutputSettings &settings)
{
    KConfigGroup group(config, effectConfigGroupName());
    const QString token = settings.token.isEmpty() ? outputToken(settings.key) : settings.token;

    writeGridForToken(group, token, settings);

    const QString nameToken = outputNameToken(settings.key);
    if (nameToken != token) {
        writeGridForToken(group, nameToken, settings);
    }
}

} // namespace Tiles
