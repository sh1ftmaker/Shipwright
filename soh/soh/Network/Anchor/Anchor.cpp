#include <nlohmann/json.hpp>

#include "Anchor.h"
#include "soh/Enhancements/nametag.h"
#include "soh/ObjectExtension/ObjectExtension.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
}

// MARK: - Overrides

void Anchor::Enable() {
#ifdef __EMSCRIPTEN__
    // The room comes from the HANDSHAKE packet, like on desktop. Saved URLs of the retired PartyKit relay
    // (*.partykit.dev, shut down October 2026) are replaced by the bridge.
    std::string wsUrl = CVarGetString(CVAR_REMOTE_ANCHOR("WebSocketURL"), SOH_WEB_ANCHOR_URL);
    if (wsUrl.empty() || wsUrl.find("partykit.dev") != std::string::npos) {
        wsUrl = SOH_WEB_ANCHOR_URL;
        CVarSetString(CVAR_REMOTE_ANCHOR("WebSocketURL"), wsUrl.c_str());
    }

    Network::EnableWebSocket(wsUrl);
    isEnabled = true;
#else
    Network::Enable(CVarGetString(CVAR_REMOTE_ANCHOR("Host"), "anchor.hm64.org"),
                    CVarGetInteger(CVAR_REMOTE_ANCHOR("Port"), 43383));
#endif
#ifdef __EMSCRIPTEN__
    // Every tab of the game shares the saved settings, and the server closes the older connection when a client ID
    // connects again, so two tabs starting from the saved ID would knock each other off forever. Start each page
    // with a fresh ID and only keep the one this page was given (reconnects reuse it without coming through here).
    static bool hasPageClientId = false;
    if (!hasPageClientId) {
        ownClientId = 0;
        hasPageClientId = true;
    }
#else
    ownClientId = CVarGetInteger(CVAR_REMOTE_ANCHOR("LastClientId"), 0);
#endif
    roomState.ownerClientId = 0;
}

void Anchor::Disable() {
    Network::Disable();

    clients.clear();
    RefreshClientActors();
}

void Anchor::OnConnected() {
    // Everything but the handshake waits until the server has given us our ID (sessionReady, ALL_CLIENT_STATE)
    sessionReady = false;
    {
        std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
        incomingPacketQueue = {};
    }
    SendPacket_Handshake();
    RegisterHooks();

    // Requested once the room state has arrived (HandlePacket_UpdateRoomState): it depends on the room's settings,
    // which a page that just connected doesn't know yet
    pendingTeamStateRequest = true;
    if (IsSaveLoaded()) {
        AutoEquipSwordAndShield();
    }
}

void Anchor::AutoEquipSwordAndShield() {
    // Equip the best sword the player owns
    for (int sword = EQUIP_VALUE_SWORD_BIGGORON; sword >= EQUIP_VALUE_SWORD_KOKIRI; sword--) {
        if (CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, sword)) {
            gSaveContext.equips.equipment &= ~gEquipMasks[EQUIP_TYPE_SWORD];
            gSaveContext.equips.equipment |= (sword << gEquipShifts[EQUIP_TYPE_SWORD]);
            break;
        }
    }

    // Equip the best shield the player owns
    for (int shield = EQUIP_VALUE_SHIELD_MIRROR; shield >= EQUIP_VALUE_SHIELD_DEKU; shield--) {
        if (CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, shield)) {
            gSaveContext.equips.equipment &= ~gEquipMasks[EQUIP_TYPE_SHIELD];
            gSaveContext.equips.equipment |= (shield << gEquipShifts[EQUIP_TYPE_SHIELD]);
            break;
        }
    }
}

void Anchor::OnDisconnected() {
    sessionReady = false;
    pendingTeamStateRequest = false;

    // Nobody else is here until the server says so again (ALL_CLIENT_STATE after reconnecting): their dummies and
    // compass icons go away and no teleports are offered. Actor states from before the drop are stale.
    for (auto& [clientId, client] : clients) {
        if (!client.self) {
            client.online = false;
        }
    }
    shouldRefreshActors = true;
    AnchorActorSync::Reset();

    {
        std::lock_guard<std::mutex> lock(outgoingPacketQueueMutex);
        outgoingPacketQueue = {};
    }

    RegisterHooks();
}

#ifdef __EMSCRIPTEN__
// A keepalive goes out every few seconds and comes back from the server (PING), so a connection that hasn't
// brought anything in for a while is dead, even if the browser still thinks it's open
static const double KEEPALIVE_INTERVAL_MS = 3000;
static const double LIVENESS_TIMEOUT_MS = 10000;

void Anchor::OnTimerTick() {
    if (!isEnabled || !CheckLiveness(LIVENESS_TIMEOUT_MS)) {
        return;
    }

    if (isConnected && sessionReady && emscripten_get_now() - lastPingTime >= KEEPALIVE_INTERVAL_MS) {
        SendPacket_Ping();
    }
}
#endif

void Anchor::ProcessOutgoingPackets() {
    if (!isConnected || !sessionReady) {
        return;
    }

    // Copy all queued packets while holding the lock, then send them after releasing
    std::queue<nlohmann::json> packetsToSend;
    {
        std::lock_guard<std::mutex> lock(outgoingPacketQueueMutex);
        packetsToSend.swap(outgoingPacketQueue);
    }

    // Send packets without holding the lock
    while (!packetsToSend.empty()) {
        nlohmann::json payload = packetsToSend.front();
        packetsToSend.pop();

        // Set when sending rather than when queueing, as packets queued before the server gave us our ID had none
        payload["clientId"] = ownClientId;
        if (!payload.contains("quiet")) {
            SPDLOG_DEBUG("[Anchor] Sending payload:\n{}", payload.dump());
        }
        Network::SendJsonToRemote(payload);
    }
}

void Anchor::SendJsonToRemote(nlohmann::json payload) {
    if (!isConnected) {
        return;
    }

    std::string packetType = payload["type"].get<std::string>();
    if (packetType == HANDSHAKE) {
        payload["clientId"] = ownClientId;
        Network::SendJsonToRemote(payload);
        return;
    }

    // Until the server has given us our ID, positions and the like would only be stale by the time they go out
    if (!sessionReady && IsRealtimePacket(packetType)) {
        return;
    }

    if (!payload.contains("quiet")) {
        SPDLOG_DEBUG("[Anchor] Queuing payload:\n{}", payload.dump());
    }

    // Queue the packet to be sent on the network thread
    std::lock_guard<std::mutex> lock(outgoingPacketQueueMutex);
    outgoingPacketQueue.push(payload);
}

void Anchor::OnIncomingJson(nlohmann::json payload) {
    // If it doesn't contain a type, it's not a valid payload
    if (!payload.contains("type")) {
        return;
    }

    // If it's not a quiet payload, log it
    if (!payload.contains("quiet")) {
        SPDLOG_DEBUG("[Anchor] Received payload:\n{}", payload.dump());
    }

    std::string packetType = payload["type"].get<std::string>();

    // Our own keepalive coming back: receiving it was the point (Network::CheckLiveness)
    if (packetType == PING) {
        HandlePacket_Ping(payload);
        return;
    }

    // Ignore packets from mismatched clients, except for ALL_CLIENT_STATE, UPDATE_CLIENT_STATE, and PLAYER_UPDATE
    if (packetType != ALL_CLIENT_STATE && packetType != UPDATE_CLIENT_STATE && packetType != PLAYER_UPDATE) {
        if (payload.contains("clientId")) {
            uint32_t clientId = payload["clientId"].get<uint32_t>();
            if (clients.contains(clientId) && clients[clientId].clientVersion != clientVersion) {
                return;
            }
        }
    }

    // Queue all packets to be processed on the game thread
    std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
    incomingPacketQueue.push(payload);
}

void Anchor::ProcessIncomingPacketQueue() {
#ifdef __EMSCRIPTEN__
    // On Emscripten, poll the WebSocket for incoming messages
    // (there's no receive thread, so we poll from the game thread)
    PollIncoming();
#endif

    // Copy all queued packets while holding the lock, then process them after releasing
    std::queue<nlohmann::json> packetsToProcess;
    {
        std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
        packetsToProcess.swap(incomingPacketQueue);
    }

    // Process packets without holding the lock
    while (!packetsToProcess.empty()) {
        nlohmann::json payload = packetsToProcess.front();
        packetsToProcess.pop();

        std::string packetType = payload["type"].get<std::string>();

        isProcessingIncomingPacket = true;

        try {
            // packetType here is a string so we can't use a switch statement
            if (packetType == ALL_CLIENT_STATE)
                HandlePacket_AllClientState(payload);
            else if (packetType == DAMAGE_PLAYER)
                HandlePacket_DamagePlayer(payload);
            else if (packetType == DISABLE_ANCHOR)
                HandlePacket_DisableAnchor(payload);
            else if (packetType == ENTRANCE_DISCOVERED)
                HandlePacket_EntranceDiscovered(payload);
            else if (packetType == GAME_COMPLETE)
                HandlePacket_GameComplete(payload);
            else if (packetType == GIVE_ITEM)
                HandlePacket_GiveItem(payload);
            else if (packetType == OCARINA_SFX)
                HandlePacket_OcarinaSfx(payload);
            else if (packetType == PLAYER_UPDATE)
                HandlePacket_PlayerUpdate(payload);
            else if (packetType == PLAYER_SFX)
                HandlePacket_PlayerSfx(payload);
            else if (packetType == UPDATE_TEAM_STATE)
                HandlePacket_UpdateTeamState(payload);
            else if (packetType == REQUEST_TEAM_STATE)
                HandlePacket_RequestTeamState(payload);
            else if (packetType == REQUEST_TELEPORT)
                HandlePacket_RequestTeleport(payload);
            else if (packetType == SERVER_MESSAGE)
                HandlePacket_ServerMessage(payload);
            else if (packetType == SET_CHECK_STATUS)
                HandlePacket_SetCheckStatus(payload);
            else if (packetType == SET_FLAG)
                HandlePacket_SetFlag(payload);
            else if (packetType == TELEPORT_TO)
                HandlePacket_TeleportTo(payload);
            else if (packetType == UNSET_FLAG)
                HandlePacket_UnsetFlag(payload);
            else if (packetType == UPDATE_BEANS_COUNT)
                HandlePacket_UpdateBeansCount(payload);
            else if (packetType == UPDATE_CLIENT_STATE)
                HandlePacket_UpdateClientState(payload);
            else if (packetType == UPDATE_ROOM_STATE)
                HandlePacket_UpdateRoomState(payload);
            else if (packetType == UPDATE_DUNGEON_ITEMS)
                HandlePacket_UpdateDungeonItems(payload);
            else if (packetType == ACTOR_STATE)
                AnchorActorSync::HandlePacket_ActorState(payload);
            else if (packetType == ACTOR_HEALTH)
                AnchorActorSync::HandlePacket_ActorHealth(payload);
            else if (packetType == ACTOR_KILL)
                AnchorActorSync::HandlePacket_ActorKill(payload);
            else if (packetType == BLOCK_PUSH)
                AnchorActorSync::HandlePacket_BlockPush(payload);
            else if (packetType == BLOCK_POS)
                AnchorActorSync::HandlePacket_BlockPos(payload);
            else if (packetType == BOSS_STATE)
                AnchorActorSync::HandlePacket_BossState(payload);
            else if (packetType == BOSS_HIT)
                AnchorActorSync::HandlePacket_BossHit(payload);
            else if (packetType == BOSS_DEFEATED)
                AnchorActorSync::HandlePacket_BossDefeated(payload);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Anchor] Exception while processing incoming packet {}", e.what());
            SPDLOG_ERROR("[Anchor] Packet: {}", payload.dump());
        }

        isProcessingIncomingPacket = false;
    }
}

// MARK: - Misc/Helpers

// Kills all existing anchor actors and respawns them with the new client data

struct DummyPlayerClientId {
    uint32_t clientId = 0;
};
static ObjectExtension::Register<DummyPlayerClientId> DummyPlayerClientIdRegister;

uint32_t Anchor::GetDummyPlayerClientId(const Actor* actor) {
    const DummyPlayerClientId* clientId = ObjectExtension::GetInstance().Get<DummyPlayerClientId>(actor);
    return clientId != nullptr ? clientId->clientId : 0;
}

void Anchor::SetDummyPlayerClientId(const Actor* actor, uint32_t clientId) {
    ObjectExtension::GetInstance().Set<DummyPlayerClientId>(actor, DummyPlayerClientId{ clientId });
}

void Anchor::RefreshClientActors() {
    if (!IsSaveLoaded()) {
        return;
    }

    Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;

    while (actor != NULL) {
        if (actor->id == ACTOR_EN_OE2 && actor->update == DummyPlayer_Update) {
            NameTag_RemoveAllForActor(actor);
            Actor_Kill(actor);
        }
        actor = actor->next;
    }

    for (auto& [clientId, client] : clients) {
        if (!client.online || client.self) {
            continue;
        }

        spawningDummyPlayerForClientId = clientId;
        // We are using a hook `ShouldActorInit` to override the init/update/draw/destroy functions of the Player we
        // spawn We quickly store a mapping of "index" to clientId, then within the init function we use this to get the
        // clientId and store it on player->zTargetActiveTimer (unused s32 for the dummy) for convenience
        auto dummy =
            Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_PLAYER, client.posRot.pos.x, client.posRot.pos.y,
                        client.posRot.pos.z, client.posRot.rot.x, client.posRot.rot.y, client.posRot.rot.z, 0);
        client.player = (Player*)dummy;
    }
    spawningDummyPlayerForClientId = 0;
}

bool Anchor::IsRealtimePacket(const std::string& type) {
    return type == PLAYER_UPDATE || type == PLAYER_SFX || type == OCARINA_SFX || type == ACTOR_STATE ||
           type == BLOCK_POS || type == BOSS_STATE;
}

bool Anchor::IsSaveLoaded() {
    if (gPlayState == nullptr) {
        return false;
    }

    if (GET_PLAYER(gPlayState) == nullptr) {
        return false;
    }

    if (gSaveContext.fileNum < 0 || gSaveContext.fileNum > 2) {
        return false;
    }

    if (gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return false;
    }

    return true;
}
