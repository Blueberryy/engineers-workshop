/**
 * =============================================================================
 * Engineer's Workshop - AntiSap
 * Prevent buildings from being sapped.
 *
 * Copyright (C) 2021 SirDigbot
 * =============================================================================
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma semicolon 1

//==================================================================
// Includes
#include <sourcemod>
#undef REQUIRE_PLUGIN
#tryinclude <updater>
#define REQUIRE_PLUGIN

#pragma newdecls required // After 3rd-party includes
#include <engineersworkshop>


//==================================================================
// Constants
#define PLUGIN_VERSION "1.0.0"
#define PLUGIN_URL "https://sirdigbot.github.io/engineers-workshop/"
#define UPDATE_URL "https://sirdigbot.github.io/engineers-workshop/updater_antisap.txt"

public Plugin myinfo = 
{
    name = "[TF2] Engineer's Workshop - AntiSap",
    author = "SirDigbot",
    description = "Prevent buildings from being sapped.",
    version = PLUGIN_VERSION,
    url = PLUGIN_URL
};

#define ANNOTATION_COOLDOWN 4 // Seconds before another annotation can appear


//==================================================================
// Globals
ConVar g_Cvar_IsUpdateEnabled;
ConVar g_Cvar_NotifyBlock;
bool g_CannotBeSapped[MAXPLAYERS + 1][BUILDING_TYPES_TOTAL];

Cooldown g_notifyCooldown;


//==================================================================
// Forwards
public void OnPluginStart()
{
    LoadTranslations("common.phrases");
    LoadTranslations("engineersworkshop.phrases");
    LoadTranslations("ew_antisap.phrases");
    
    // Create Cvars
    CreateConVar("sm_ew_antisap_version", PLUGIN_VERSION, "Engineer's Workshop - AntiSap version. Do Not Touch!", FCVAR_SPONLY|FCVAR_REPLICATED|FCVAR_NOTIFY|FCVAR_DONTRECORD);
    g_Cvar_IsUpdateEnabled = CreateConVar("sm_ew_antisap_update", "1", "Update Engineer's Workshop - AntiSap Automatically (Requires Updater)\n(Default: 1)", FCVAR_NONE, true, 0.0, true, 1.0);
    g_Cvar_IsUpdateEnabled.AddChangeHook(OnCvarChanged);
    g_Cvar_NotifyBlock = CreateConVar("sm_ew_antisap_notify", "2", "Notify attacker when AntiSap blocks a sapper.\n 0 - Don't notify.\n 1 - Notify in chat.\n 2 - Notify with annotation.\n(Default: 1)", FCVAR_NONE, true, 0.0, true, 2.0);
    
    RegAdminCmd("sm_antisap", Command_AntiSap, ADMFLAG_CHEATS, "Prevent buildings from being sapped.");

    // Hook events
    HookEvent("player_sapped_object", Event_Sapped);
    AddNormalSoundHook(view_as<NormalSHook>(Hook_NormalSound));

    // Load initial settings
    ResetAllSettings();

    /**
     * Overrides
     * sm_antisap_target -- Client can target others with sm_antisap
     */
}

public void OnClientDisconnect_Post(int client)
{
    ResetSettings(client);
}


//==================================================================
// Event Hooks
public void Event_Sapped(Event ev, const char[] name, bool dontBroadcast)
{
    // Get sapper index
    int sapper = ev.GetInt("sapperid", -1);
    if (!IsValidEntity(sapper))
        return;

    // Get attached building info and attacker index
    int client = GetClientOfUserId(ev.GetInt("ownerid", 0));
    int attacker = GetClientOfUserId(ev.GetInt("userid", 0));
    int building = EW_GetSappedBuilding(sapper, true);
    int type = EW_GetBuildingType(building); // Checks building index
    int typeIndex = EW_TypeToIndex(type);
    if (type == 0 || typeIndex == -1)
        return;

    // If builder has antisap enabled for that building type,
    // delete the sapper and notify the attacker
    if (g_CannotBeSapped[client][typeIndex])
    {
        if (attacker && IsClientInGame(attacker))
        {
            if (g_Cvar_NotifyBlock.IntValue == 1)
                PrintToChat(attacker, "%s %t", EW_CHAT_TAG, "EW_AntiSap_Blocked");
            else if (g_Cvar_NotifyBlock.IntValue == 2 && !g_notifyCooldown.IsAllowed(attacker))
                EW_ShowAnnotation(attacker, building, 2.0, false, NULL_VECTOR, NULL_STRING, "%t", "EW_AntiSap_Blocked");
        }
        
        RemoveEntity(sapper);
    }
}


public Action Hook_NormalSound(
    int clients[MAXPLAYERS],
    int &numClients,
    char sample[PLATFORM_MAX_PATH],
    int &entity,
    int &channel,
    float &volume,
    int &level,
    int &pitch,
    int &flags)
{
    // Fix sound bug when sapper is manually deleted, credit to Tylerst
    if (StrEqual(sample, "weapons/sapper_timer.wav", false) || (StrContains(sample, "spy_tape_", false) != -1))
    {
        // Stop sound if sapper entity is killed (wont apply to regular wrench destroys)
        // I don't know why the fuck we check m_hBuiltOnEntity but it works.
        if (!IsValidEntity(GetEntPropEnt(entity, Prop_Send, "m_hBuiltOnEntity")))
            return Plugin_Stop;
    }
    return Plugin_Continue;
}


//==================================================================
// Cvar Updating
public void OnCvarChanged(ConVar cvar, const char[] oldValue, const char[] newValue)
{
#if defined _updater_included
    if (cvar == g_Cvar_IsUpdateEnabled)
        g_Cvar_IsUpdateEnabled.BoolValue ? Updater_AddPlugin(UPDATE_URL) : Updater_RemovePlugin();
#endif
}


//==================================================================
// Settings
void ResetSettings(int client)
{
    // Reset antisap settings for a specific client
    for (int i = 0; i < BUILDING_TYPES_TOTAL; i++)
        g_CannotBeSapped[client][i] = false;
}

void ResetAllSettings()
{
    // Reset antisap settings for all players
    for (int i = 0; i <= MaxClients; i++)
    {
        for (int j = 0; j < BUILDING_TYPES_TOTAL; j++)
            g_CannotBeSapped[i][j] = false;
    }

    g_notifyCooldown.Reset();
    g_notifyCooldown.interval = ANNOTATION_COOLDOWN;
}


//=================================
// Anti Sap Commands

/**
 * Forms:
 * "sm_antisap"
 * "sm_antisap <Building> <1/0>"
 * "sm_antisap [Target] <Building> <1/0>"
 */
public Action Command_AntiSap(int client, int args)
{
    // Menu mode -- TBA
    if (args == 0)
    {
        ReplyToCommand(client, "ANTISAP MENU TBA");
        return Plugin_Handled;
    }

    // Command mode, Get args
    int buildingTypes;
    bool state;
    bool stateError;
    char target[MAX_NAME_LENGTH];

    if (args == 2)
    {
        buildingTypes = EW_GetCmdArgBuildingType(1);
        state = EW_GetCmdArgBoolString(2);
    }
    else if (args == 3)
    {
        // Targeting mode, check client can target with this command
        if (!CheckCommandAccess(client, "sm_antisap_target", ADMFLAG_CHEATS))
        {
            GetCmdArg(0, target, sizeof(target)); // Re-use buffer
            return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_NoTargetAccess", target);
        }

        GetCmdArg(1, target, sizeof(target));
        buildingTypes = EW_GetCmdArgBuildingType(2);
        state = EW_GetCmdArgBoolString(3, stateError);
    }
    else
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_AntiSap_CmdHelp");

    // Verify args
    if (buildingTypes == 0)
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_InvalidBuilding");
    if (stateError)
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_InvalidBoolean");

    // Get the building name translation key
    char buildingName[MAX_BUILDINGNAME_SIZE];
    EW_GetTranslationKey(buildingTypes, true, buildingName, sizeof(buildingName));

    // Apply antisap..
    if (args == 2)
    {
        // ..to client only
        EW_SetBuildingArrayBool(state, buildingTypes, g_CannotBeSapped[client]);
        if (state)
            ReplyToCommand(client, "%s %t", EW_CHAT_TAG, "EW_AntiSap_Enabled", buildingName);
        else
            ReplyToCommand(client, "%s %t", EW_CHAT_TAG, "EW_AntiSap_Disabled", buildingName);
    }
    else
    {
        // ..to all targets
        TargetData data;
        if (!EW_ProcessTargets(target, client, 0, data, true))
            return Plugin_Handled;

        for (int i = 0; i < data.count; i++)
            EW_SetBuildingArrayBool(state, buildingTypes, g_CannotBeSapped[data.targets[i]]);

        if (state)
            ShowActivity2(client, EW_CHAT_TAG, " %t", "EW_AntiSap_TargetEnabled", "_s", data.name, buildingName);
        else
            ShowActivity2(client, EW_CHAT_TAG, " %t", "EW_AntiSap_TargetDisabled", "_s", data.name, buildingName);
    }

    return Plugin_Handled;
}


//==================================================================
// Updater
public void OnConfigsExecuted()
{
#if defined _updater_included
    if (LibraryExists("updater") && g_Cvar_IsUpdateEnabled.BoolValue)
        Updater_AddPlugin(UPDATE_URL);
#endif
}

public void OnLibraryAdded(const char[] name)
{
#if defined _updater_included
    if (StrEqual(name, "updater") && g_Cvar_IsUpdateEnabled.BoolValue)
        Updater_AddPlugin(UPDATE_URL);
#endif
}

public void OnLibraryRemoved(const char[] name)
{
#if defined _updater_included
    if (StrEqual(name, "updater"))
        Updater_RemovePlugin();
#endif
}
