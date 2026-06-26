#include "effect.h"

#include "effect/effect.h"

using namespace Tiles;

KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(Effect, "metadata.json", return Effect::supported();, return Effect::enabledByDefault();)

#include "main.moc"
