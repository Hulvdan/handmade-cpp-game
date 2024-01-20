#include "bf_types.h"

enum class TileState {
    NONE,
    EXCLUDED,
    INCLUDED,
};

struct TileRule {
    TextureID texture_id;
    TileState states[8];
};

struct SmartTile {
    int rules_count;
    TileRule* rules;
};
