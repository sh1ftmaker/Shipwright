#pragma once

// Request that the game boot straight into save slot `slot` (0-2), creating it with `name`
// (ASCII, up to 8 chars) if it doesn't exist. Must be called during init, before the first frame.
// An empty name becomes "Guest".
void WebJoin_RequestBootToSlot(int slot, const char* name);
// Request that the game boot straight into a new Boss Rush run with the default options, which isn't kept in a save
// slot (so everyone joining a room with it starts the same way). Must be called during init, like the above.
void WebJoin_RequestBootToBossRush();
bool WebJoin_IsPending();
