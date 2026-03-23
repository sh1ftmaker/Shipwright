# Dynamic Anchor Multiplayer - Development Plan

**Goal:** Allow players to drop in/out of PartyKit networking sessions at any time, not just on boot with URL parameters.

## Current Limitations

1. **URL parameters force file select boot**: `?room=X&name=Y` sets `BootSequence=BOOTSEQUENCE_FILESELECT` and `Enabled=1` (see `web_apply_anchor_config()` lines 126-127)
2. **No room switching without restart**: WebSocket URL is built once at boot; changing `RoomId` CVar doesn't update connection
3. **Boot sequence is static**: Hook conditions evaluated once during `ShipInit::InitAll()`; changing `BootSequence` mid-game has no effect

## Existing Assets (Leverage These!)

- **Anchor menu** (`soh/soh/Network/Anchor/Menu.cpp`): Already has Enable/Disable, input fields for RoomId/Name/Color/TeamId
- **CVar system**: Stores all connection parameters
- **Anchor::Enable()/Disable()**: Properly manages WebSocket lifecycle
- **Network::EnableWebSocket()**: Has guard `if (isEnabled) return;` - so `Disable()` must be called before re-`Enable()`

## Proposed Changes

### 1. `soh/soh/web/web_main.cpp` - Decouple URL parsing from boot forcing

**Current behavior** (lines 94-131):
- `web_apply_anchor_config()` parses `window._anchorConfig` (set from URL hash)
- Immediately sets: `Enabled=1`, `BootSequence=FILESELECT`

**New behavior**:
- Rename to `web_apply_anchor_config_from_url()` (still called at boot)
- Remove lines 126-127 (no longer force Enabled/BootSequence)
- Only set WebSocketURL, RoomId, Name, Color, TeamId from URL if provided
- Let users boot normally to title screen

### 2. Add `web_refresh_anchor_config()` - Runtime config rebuild

**New function** (in `web_main.cpp`):
```cpp
void web_refresh_anchor_config() {
    // Build WebSocket URL from current RoomId CVar
    const char* room = CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), "default");
    std::string wsUrl = "wss://soh-anchor.zalo.partykit.dev/party/" + std::string(room);
    CVarSetString(CVAR_REMOTE_ANCHOR("WebSocketURL"), wsUrl.c_str());

    // Reconnect if Anchor is currently enabled
    if (CVarGetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 0)) {
        extern void web_configure_anchor_direct(); // We'll write this
        web_configure_anchor_direct();
    }
}
```

### 3. Add `web_configure_anchor_direct()` - Safe runtime reconnection

**New function** (in `web_main.cpp`):
```cpp
void web_configure_anchor_direct() {
    // Disable first if already enabled
    if (CVarGetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 0)) {
        // Need to call Anchor's Disable method
        // We'll need to expose a C wrapper: extern "C" void anchor_disable();
        // Or we can set CVar and rely on menu's logic
        // Better: add a C-callable wrapper to Anchor.h/.cpp
    }

    // Re-enable (Enable will read the WebSocketURL CVar we just set)
    // ...
}
```

Actually, simpler: Just tie the "Reconnect" button to:
```cpp
CVarSetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 0);
anchor->Disable();  // if we can call from C++
CVarSetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 1);
anchor->Enable();
```

**We need C-callable wrappers** for `Anchor::Enable()` and `Anchor::Disable()` since web_main.cpp includes `extern "C"` block.

Add to `soh/soh/Network/Anchor/Anchor.h`:
```cpp
extern "C" {
    void Anchor_Enable();
    void Anchor_Disable();
}
```

Implement in `Anchor.cpp`:
```cpp
extern "C" void Anchor_Enable() {
    if (Anchor::Instance) {
        Anchor::Instance->Enable();
    }
}
extern "C" void Anchor_Disable() {
    if (Anchor::Instance) {
        Anchor::Instance->Disable();
    }
}
```

### 4. Update `soh/soh/Network/Anchor/Menu.cpp`

Add after the RoomId input (after line 82):
- Call `web_refresh_anchor_config()` when RoomId changes? But that would rebuild URL on every keystroke - not ideal.
- Instead, when user clicks "Enable", call `web_refresh_anchor_config()` first to rebuild URL from RoomId, then re-enable.

**Better approach**:
- Remove the `IsSecret(anchor->isEnabled)` from RoomId input - users should be able to change it even when connected
- After RoomId field loses focus (ImGui::DeactivateIDAfterEdit) or after a small delay, mark that config needs refresh
- Add explicit "Apply & Reconnect" button that:
  1. Calls `web_refresh_anchor_config()` to rebuild URL from RoomId
  2. If already enabled, toggles off and on (or calls Disable/Enable internally)
- Change label from "Enable" to "Connect" when disconnected, "Disconnect" when connected

Actually, simpler: Keep existing "Enable"/"Disable" button, but make its logic:
- If `Enabled` CVar is 0 and user clicks "Connect":
  - Call `web_refresh_anchor_config()` to update WebSocketURL from RoomId
  - Then set `Enabled=1` and call `Anchor_Enable()`
- If `Enabled` CVar is 1 and user clicks "Disconnect":
  - Call `Anchor_Disable()`, set `Enabled=0`

Update the callback to:
```cpp
if (anchor->isEnabled) {
    // Disconnect
    CVarSetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    anchor->Disable();
} else {
    // Connect - first refresh URL from RoomId
    web_refresh_anchor_config();
    CVarSetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 1);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    anchor->Enable();
}
```

**But wait** - `web_refresh_anchor_config()` is in `web_main.cpp` and uses EM_JS. It needs to be callable from Menu.cpp. We should either:
- Export it as `extern "C"` from `web_main.cpp`
- Or implement the logic directly in Menu.cpp? No, it needs to set CVar and handle reconnection.

**Best**: Add `EMSCRIPTEN_KEEPALIVE` to `web_refresh_anchor_config()` in `web_main.cpp` so it can be called from C++.

### 5. Fix the Hook System Issue (Maybe?)

**Question:** Will changing `BootSequence` after `ShipInit::InitAll()` affect title screen hooks?

- Hooks registered via `COND_HOOK` - condition evaluated at registration (once)
- Changing CVar later won't re-run the hook function
- However, `BootSequence` is read inside the hook function itself in some places (e.g., `CustomLogoTitle.cpp` line 216: `COND_HOOK(OnZTitleUpdate, CVAR_BOOTSEQUENCE_VALUE == BOOTSEQUENCE_FILESELECT, ...)`)

Let's check `COND_HOOK` definition to be sure. But given our goal is to avoid forcing file select, we just need to stop setting BootSequence from URL. Users who want to start at file select can manually set BootSequence in settings.

**Conclusion**: No need to modify hook system; just stop forcing BootSequence.

## Implementation Order

### Phase 1: Decouple URL config from boot
- [ ] Edit `web_main.cpp`: `web_apply_anchor_config()` - remove `Enabled=1` and `BootSequence=FILESELECT`
- [ ] Keep only WebSocketURL, RoomId, Name, Color, TeamId from URL
- [ ] This alone means URL params will only set defaults, not affect boot

### Phase 2: Add runtime refresh helper
- [ ] In `web_main.cpp`: Add `web_refresh_anchor_config()` that:
  - Builds WebSocket URL from `CVAR_REMOTE_ANCHOR("RoomId")`
  - Sets `CVAR_REMOTE_ANCHOR("WebSocketURL")`
- [ ] Wrap with `EMSCRIPTEN_KEEPALIVE` to export

### Phase 3: Expose Anchor control functions
- [ ] In `Anchor.h`: Add `extern "C"` declarations for `Anchor_Enable()` and `Anchor_Disable()`
- [ ] In `Anchor.cpp`: Implement them

### Phase 4: Update menu behavior
- [ ] In `Menu.cpp`:
  - Remove `.IsSecret(anchor->isEnabled)` from RoomId input (line 79)
  - Modify Enable/Disable button logic: Before enabling, call `web_refresh_anchor_config()` to update URL
  - After enabling, show "Connected" status
  - Change button label to "Connect" when disabled
- [ ] Optionally add "Apply & Reconnect" if RoomId changed

### Phase 5: Testing strategy
- [ ] Build via GitHub Actions
- [ ] Download artifact and test locally (Emscripten build)
- [ ] Test cases:
  1. Load `index.html?room=test&name=player` - should go to title screen (not file select)
  2. Open menu, set RoomId, click Connect - should connect to room
  3. Change RoomId while connected, click Disconnect then Connect - should join new room
  4. Disconnect and reconnect - should work without restart

### Phase 6: Documentation updates
- [ ] Update instructions in menu (remove "All players should start at file select screen")
- [ ] Add note: "You can join/leave at any time. Use the Connect button."

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Hook conditions only evaluated at boot - BootSequence changes won't skip title | We're NOT relying on that; we're simply not forcing file select on boot. Users who want to skip title can manually set BootSequence CVar in settings. |
| `Network::Enable()` guard prevents multiple enables | Ensure `Disable()` is called properly before re-Enable. Menu button handles this. |
| WebSocket URL constructed incorrectly | Test with various RoomId values (empty, special chars) |
| Users might want to join via URL but still see title | That's the desired behavior! The URL params will set RoomId/Name defaults. They still click Connect in menu. |

## Edge Cases

- **Empty RoomId**: Should default to "default" room (current behavior in `web_apply_anchor_config`)
- **Name empty**: Anchor might still connect but with empty name? Allow? Probably fine.
- **Already connected and user changes RoomId**: `web_refresh_anchor_config()` updates WebSocketURL but doesn't reconnect automatically. User must click Disconnect then Connect. (Could add auto-reconnect but explicit is safer)
- **Disable called while connecting**: `Network::Disable()` will cancel connection attempt

## GitHub Actions Constraints

- "This project can only be built in GitHub Actions" - means we can't build locally (missing toolchain?)
- Plan: push changes to fork, create PR or push to branch, let Actions build
- Download the artifact (`Shipwright-Web.zip`) from Actions artifacts tab
- Extract and test locally (requires hosting the `build/` folder via web server)
- Iterate: make changes, commit, push, wait for Actions, download, test

## Summary

This plan transforms Anchor from a **boot-time-only** feature to a **dynamic, on-demand** multiplayer system. Users can:
- Start game normally (title screen) even with URL parameters present
- Configure room/name/color at any time via menu
- Connect/disconnect freely
- Switch rooms without restarting

The changes are minimal and leverage existing infrastructure (CVars, Anchor class, menu system).
