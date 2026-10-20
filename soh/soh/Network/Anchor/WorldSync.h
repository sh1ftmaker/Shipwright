#pragma once

#ifdef __cplusplus

#include <nlohmann/json.hpp>

extern "C" {
#include "z64.h"
}

/**
 * Helpers for sharing the world, flags and key items between teammates (the Anchor packets in Packets/ use them).
 *
 * Policy: progress is shared, consumables are per player. Flags, equipment, songs, quest items, heart pieces,
 * dungeon keys and the like reach everyone; rupees, recovery hearts, magic jars, ammo refills and bottle contents
 * stay with whoever picked them up (the first pickup of an ammo item is still shared, it unlocks the slot).
 *
 * Races: an item or small key that came out of a chest, a collectible or a Gold Skulltula carries that source flag.
 * A teammate who set the same flag themselves got their own copy, so they drop it (both opened the chest at once).
 * A small key spent on a door carries the door's switch flag the same way, so two players unlocking one door at the
 * same moment spend one key each, the same as one player would.
 */
namespace AnchorWorldSync {

// A flag that produced an item (a chest, collectible or GS token) or that a key opened (a door).
// sceneNum is SCENE_ID_MAX for global flags (FLAG_GS_TOKEN), and -1 when there is no source.
struct FlagRef {
    s16 sceneNum = -1;
    s16 flagType = 0;
    s16 flag = 0;

    bool IsValid() const {
        return sceneNum >= 0;
    }
};

void WriteSource(nlohmann::json& payload, const FlagRef& source);
FlagRef ReadSource(const nlohmann::json& payload);

// Flags each player keeps for themselves: positional dungeon state (Water Temple water level, Forest Temple
// elevator), the castle collapse timer, Great Fairy cutscene state, minigame/race state (eventInf) and the
// swordless flag. They're never sent, applied or merged.
bool IsPerPlayerFlag(s16 sceneNum, s16 flagType, s16 flag);
// Switch flags 0x38-0x3F are cleared on every room change, so they only mean something in the room that set them
bool IsRoomLocalFlag(s16 flagType, s16 flag);
// Temporary flags (switch and collectible 0x20+) only last until the scene changes, so they aren't queued
bool IsTempFlag(s16 sceneNum, s16 flagType, s16 flag);

bool IsFlagSet(const FlagRef& ref);
// Sets a flag the way an incoming SET_FLAG does, and remembers that a teammate set it
void SetFlagFromTeammate(const FlagRef& ref);
void MarkSetByTeammate(const FlagRef& ref);
// True if this player set the flag (or claimed its item) themselves, so a teammate's item from it is a duplicate
bool ClaimedLocally(const FlagRef& ref);
void Reset();

// Local game events, to find the source of the next item or key change
void OnLocalFlagSet(s16 sceneNum, s16 flagType, s16 flag);
FlagRef TakeItemSource(GetItemEntry itemEntry);
FlagRef TakeDoorSource();

// Consumables (see the policy above) aren't sent to teammates
bool IsConsumable(GetItemEntry itemEntry);
// Remembers the inventory after a change we already know about
void RefreshSnapshot();
// How many small keys of this dungeon were spent since the last snapshot (0 when a door didn't take one)
s8 TakeKeysSpent(u16 mapIndex);
// Small key counts start at -1 (never had one)
void ApplyKeyDelta(u16 mapIndex, s8 delta);
// Bean count went down (planted one) since the last snapshot
bool TakeBeanPlanted();

} // namespace AnchorWorldSync

#endif
