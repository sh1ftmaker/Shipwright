#include <ship/window/gui/IconsFontAwesome4.h>

#include "Anchor.h"
#include "soh/OTRGlobals.h"
#include "soh/util.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

extern "C" {
extern PlayState* gPlayState;
}

void AnchorRoomWindow::Draw() {
    // Stays up while reconnecting, saying so (DrawElement)
    if (!IsVisible() || !Anchor::Instance->isEnabled) {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(0, 0, 0, CVarGetFloat(CVAR_SETTING("Notifications.BgOpacity"), 0.5f)));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

    auto vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::Begin("Anchor Room", nullptr,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

    DrawElement();

    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void AnchorRoomWindow::DrawElement() {
    if (!Anchor::Instance->sessionReady) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "%s", Anchor::Instance->GetConnectionStatus().c_str());
        return;
    }

    bool isGlobalRoom = (std::string("soh-global") == CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));

    if (isGlobalRoom) {
        u32 activeClients = 0;
        for (auto& [clientId, client] : Anchor::Instance->clients) {
            if (client.online) {
                activeClients++;
            }
        }
        ImGui::Text("Players Online: %d", activeClients);
        return;
    }

    // First build a list of teams
    std::set<std::string> teams;
    for (auto& [clientId, client] : Anchor::Instance->clients) {
        teams.insert(client.teamId);
    }

    for (auto& team : teams) {
        if (teams.size() > 1) {
            ImGui::SeparatorText(team.c_str());
        }
        bool isOwnTeam = team == CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");

        // The server never forgets anyone, so players who left (or reloaded an older build, which got a new ID each
        // time) pile up. Offline entries named like someone who is here are hidden; the rest go on one line.
        std::set<std::string> namesHere;
        for (auto& [clientId, client] : Anchor::Instance->clients) {
            if (client.teamId == team && (client.self || client.online)) {
                namesHere.insert(client.self ? CVarGetString(CVAR_REMOTE_ANCHOR("Name"), "") : client.name);
            }
        }
        std::vector<std::string> offlineNames;

        for (auto& [clientId, client] : Anchor::Instance->clients) {
            if (client.teamId != team) {
                continue;
            }
            if (!client.self && !client.online) {
                if (!namesHere.contains(client.name)) {
                    offlineNames.push_back(client.name);
                }
                continue;
            }

            ImGui::PushID(clientId);

            if (client.clientId == Anchor::Instance->roomState.ownerClientId) {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", ICON_FA_GAVEL);
                ImGui::SameLine();
            }

            if (client.self) {
                ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "%s", CVarGetString(CVAR_REMOTE_ANCHOR("Name"), ""));
            } else if (client.paused) {
                ImGui::TextColored(ImVec4(1, 1, 1, 0.5f), "%s - away", client.name.c_str());
            } else {
                ImGui::Text("%s", client.name.c_str());
            }

            if (Anchor::Instance->roomState.showLocationsMode == 2 ||
                (Anchor::Instance->roomState.showLocationsMode == 1 && isOwnTeam)) {
                if ((client.self ? Anchor::Instance->IsSaveLoaded() : client.isSaveLoaded)) {
                    ImGui::SameLine();
                    ImGui::TextColored(
                        ImVec4(1, 1, 1, 0.5f), "- %s",
                        SohUtils::GetSceneName(client.self ? gPlayState->sceneNum : client.sceneNum).c_str());
                }
            }

            if (Anchor::Instance->CanTeleportTo(client.clientId)) {
                ImGui::SameLine();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                if (ImGui::Button(ICON_FA_LOCATION_ARROW, ImVec2(20.0f, 20.0f))) {
                    Anchor::Instance->SendPacket_RequestTeleport(client.clientId);
                }
                ImGui::PopStyleVar();
            }

            if (client.clientVersion != Anchor::clientVersion) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0, 0, 1), ICON_FA_EXCLAMATION_TRIANGLE);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Incompatible version! Will not work together!");
                    ImGui::Text("Yours: %s", Anchor::clientVersion.c_str());
                    ImGui::Text("Theirs: %s", client.clientVersion.c_str());
                    ImGui::EndTooltip();
                }
            }
            uint32_t seed = IS_RANDO ? Rando::Context::GetInstance()->GetSeed() : 0;
            if (client.isSaveLoaded && Anchor::Instance->IsSaveLoaded() && client.seed != seed && client.online &&
                !client.self) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0, 0, 1), ICON_FA_EXCLAMATION_TRIANGLE);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Seed mismatch! Continuing will break things!");
                    ImGui::Text("Yours: %u", seed);
                    ImGui::Text("Theirs: %u", client.seed);
                    ImGui::EndTooltip();
                }
            }
            if (client.isSaveLoaded && Anchor::Instance->IsSaveLoaded() && client.questId >= 0 && client.online &&
                !client.self &&
                Anchor::QuestKind(client.questId) != Anchor::QuestKind(gSaveContext.ship.quest.id)) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), ICON_FA_EXCLAMATION_TRIANGLE);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Different kind of file (Boss Rush, Master Quest, ...)!");
                    ImGui::Text("Items and flags aren't shared with them.");
                    ImGui::EndTooltip();
                }
            }
            ImGui::PopID();
        }

        if (offlineNames.size() == 1) {
            ImGui::TextColored(ImVec4(1, 1, 1, 0.3f), "%s - offline", offlineNames[0].c_str());
        } else if (!offlineNames.empty()) {
            ImGui::TextColored(ImVec4(1, 1, 1, 0.3f), "%d offline", (int)offlineNames.size());
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                for (auto& name : offlineNames) {
                    ImGui::Text("%s", name.c_str());
                }
                ImGui::EndTooltip();
            }
        }
    }
}
