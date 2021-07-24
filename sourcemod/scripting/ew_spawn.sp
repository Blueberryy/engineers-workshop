/**
 * =============================================================================
 * Engineer's Workshop - Spawn
 * Commands to spawn buildings.
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
#define UPDATE_URL "https://sirdigbot.github.io/engineers-workshop/updater_spawn.txt"

public Plugin myinfo = 
{
    name = "[TF2] Engineer's Workshop - Spawn",
    author = "SirDigbot",
    description = "Commands to spawn buildings.",
    version = PLUGIN_VERSION,
    url = PLUGIN_URL
};

#define BEAM_SPRITE "sprites/laser.vmt"
#define HALO_SPRITE "sprites/halo01.vmt"


//==================================================================
// Globals
ConVar g_Cvar_IsUpdateEnabled;
ConVar g_Cvar_MaxBuildDistance;
ConVar g_Cvar_BuildLevel;

ConVar g_Cvar_BuildFlags;
int g_BuildFlags;
ConVar g_Cvar_ForceBuildFlags;
int g_ForceBuildFlags;

// Model indexes for sprites used in CreateBeamRing
int g_BeamSprite;
int g_HaloSprite;


//==================================================================
// Forwards
public void OnPluginStart()
{
    LoadTranslations("common.phrases");
    LoadTranslations("engineersworkshop.phrases");
    LoadTranslations("ew_spawn.phrases");
    
    // Create Cvars
    CreateConVar("sm_ew_spawn_version", PLUGIN_VERSION, "Engineer's Workshop - Spawn version. Do Not Touch!", FCVAR_SPONLY|FCVAR_REPLICATED|FCVAR_NOTIFY|FCVAR_DONTRECORD);
    g_Cvar_IsUpdateEnabled = CreateConVar("sm_ew_spawn_update", "1", "Update Engineer's Workshop - Spawn Automatically (Requires Updater)\n(Default: 1)", FCVAR_NONE, true, 0.0, true, 1.0);
    g_Cvar_IsUpdateEnabled.AddChangeHook(OnCvarChanged);
    g_Cvar_MaxBuildDistance = CreateConVar("sm_ew_spawn_maxbuilddistance", "130.0", "Maximum distance a player can build using sm_build.\nUse -1.0 for infinite range.\n(Default: 10.0)", FCVAR_NONE, true, -1.0, false);
    g_Cvar_BuildLevel = CreateConVar("sm_ew_spawn_build_level", "1", "Upgrade level for buildings spawned with sm_build.\n(Default: 1)", FCVAR_NONE, true, 1.0, true, 3.0);

    g_Cvar_BuildFlags = CreateConVar("sm_ew_spawn_build_flags", "", "Building permission flags indicating where a building can be built with sm_build\n - NOBUILD -- Allow in nobuild zones.\n - TEAMRESPAWN -- Allow in same team's respawn room.\n - RESPAWN -- Allow in any respawn room (Overrides TEAMRESPAWN).\n - COLLISION -- Allow even if colliding with surface.\n(Default: \"\")", FCVAR_NONE);
    g_BuildFlags = ParseBuildingPermissionFlags(g_Cvar_BuildFlags);
    g_Cvar_BuildFlags.AddChangeHook(OnCvarChanged);

    g_Cvar_ForceBuildFlags = CreateConVar("sm_ew_spawn_forcebuild_flags", "NOBUILD,RESPAWN", "Building permission flags indicating where a building can be built with sm_forcebuild\n - NOBUILD -- Allow in nobuild zones.\n - TEAMRESPAWN -- Allow in same team's respawn room.\n - RESPAWN -- Allow in any respawn room (Overrides TEAMRESPAWN).\n - COLLISION -- Allow even if colliding with surface.\n(Default: \"NOBUILD,RESPAWN\")", FCVAR_NONE);
    g_ForceBuildFlags = ParseBuildingPermissionFlags(g_Cvar_ForceBuildFlags);
    g_Cvar_ForceBuildFlags.AddChangeHook(OnCvarChanged);


    // Create commands
    RegAdminCmd("sm_forcebuild", Command_ForceBuild, ADMFLAG_CHEATS, "Spawn any building where you're aiming.");
    RegAdminCmd("sm_build", Command_Build, ADMFLAG_CHEATS, "Spawn a building where you're aiming.");
}

public void OnMapStart()
{
    g_BeamSprite = PrecacheModel(BEAM_SPRITE, true);
    g_HaloSprite = PrecacheModel(HALO_SPRITE, true);
}


//==================================================================
// Cvar Updating
public void OnCvarChanged(ConVar cvar, const char[] oldValue, const char[] newValue)
{
    if (cvar == g_Cvar_BuildFlags)
        g_BuildFlags = ParseBuildingPermissionFlags(g_Cvar_BuildFlags);
    else if (cvar == g_Cvar_ForceBuildFlags)
        g_ForceBuildFlags = ParseBuildingPermissionFlags(g_Cvar_ForceBuildFlags);
#if defined _updater_included
    else if (cvar == g_Cvar_IsUpdateEnabled)
        g_Cvar_IsUpdateEnabled.BoolValue ? Updater_AddPlugin(UPDATE_URL) : Updater_RemovePlugin();
#endif
}


//==================================================================
// Commands

/**
 * Forms:
 * sm_build <Building>
 */
public Action Command_Build(int client, int args)
{
    // Must be ingame to use this since it always goes where you're aiming
    if (!client || !IsClientInGame(client))
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_InGame");

    // Menu mode -- TBA
    if (args == 0)
        return EW_RejectCommand(client, "BUILD MENU MODE TBA");
    
    // Command mode, Ensure correct amount of args
    if (args != 1)
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_Build_CmdHelp");

    // Get args
    int buildingType = EW_GetCmdArgBuildingType(1);

    // Verify args -- The order of rejection here on makes usage as clear as possible!
    if (buildingType == 0) 
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_Build_CmdHelp");
    if (!EW_IsSingleType(buildingType))
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_SingleBuildingTypeOnly");

    // Get position and angles where client is aiming
    float position[3];
    float angles[3] = {0.0, ...};
    if (!EW_GetClientAimOrigin(client, position, g_Cvar_MaxBuildDistance.FloatValue))
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_Build_MaxDistance");
    GetClientEyeAngles(client, angles); 
    angles[0] = 0.0; // Only keep the yaw [1]
    angles[2] = 0.0;

    // Spawn building
    SpawnResult result = EW_SpawnBuilding(client, buildingType, position, angles, g_Cvar_BuildLevel.IntValue, g_BuildFlags);
    if (result != Spawn_OK)
    {
        switch (result)
        {
            case Spawn_Collision: return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_Collision");
            case Spawn_NoBuild: return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_NoBuild");
            case Spawn_RespawnRoom: return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_RespawnRoom");
            // Default to Spawn_Error
            default: return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_Failed");
        }
    }

    // Success, Get the building name translation key and print message
    char buildingName[MAX_BUILDINGNAME_SIZE];
    EW_GetTranslationKey(buildingType, false, buildingName, sizeof(buildingName));
    ReplyToCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_Spawn", g_Cvar_BuildLevel.IntValue, buildingName);
    CreateBeamRing(position);
    return Plugin_Handled;
}


/**
 * Forms:
 * sm_forcebuild <Building> <Level> [Owner] [X] [Y] [Z]
 */
public Action Command_ForceBuild(int client, int args)
{
    bool clientInGame = client != 0 && IsClientInGame(client);

    // Menu mode -- TBA
    if (args == 0)
    {
        if (!clientInGame)
            return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_InGame");
        return EW_RejectCommand(client, "FORCEBUILD MENU MODE TBA");
    }
    
    // Command mode, Ensure correct amount of args
    // This mode only requires in-game if you dont specify world position
    if (args != 2 && args != 3 && args != 6)
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_CmdHelp");

    // Get args
    int level;
    float position[3];
    float angles[3] = {0.0, ...};
    int buildingType = EW_GetCmdArgBuildingType(1);
    int owner;
    char ownerName[MAX_NAME_LENGTH];
    char buffer[MAX_BUILDINGNAME_SIZE]; // Fits building name and integer

    // Get level
    GetCmdArg(2, buffer, sizeof(buffer));
    level = StringToInt(buffer);

    // Get position..
    if (args == 6)
    {
        // ..from world coordinates
        for (int i = 0; i < 3; i++)
        {
            GetCmdArg(i + 4, buffer, sizeof(buffer));
            position[i] = StringToFloat(buffer);
        }
    }
    else
    {
        // ..where client is aiming (must be in game to aim)
        if (!clientInGame)
            return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_InGame");
        EW_GetClientAimOrigin(client, position);
    }

    // Get owner, if any. Default to client.
    if (args >= 3)
    {
        GetCmdArg(3, ownerName, sizeof(ownerName));
        owner = FindTarget(client, ownerName, false); // Allow bots, why not it's a party.
        if (owner == -1)
            return Plugin_Handled; // FindTarget handles error message
    }
    else
    {
        // Client must be ingame to own a building
        if (!clientInGame)
            return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_InGame");
        owner = client;
    }

    // Get angles -- Get the client's angles and only keep the yaw
    if (clientInGame)
    {
        GetClientEyeAngles(client, angles);
        angles[0] = 0.0;
        angles[2] = 0.0;
    }

    
    // Verify args
    if (buildingType == 0)
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_InvalidBuilding");
    if (!EW_IsSingleType(buildingType))
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_SingleBuildingTypeOnly");
    if (level < 1 || level > 3)
        return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_InvalidBuildingLevel");


    // Spawn building :D
    SpawnResult result = EW_SpawnBuilding(owner, buildingType, position, angles, level, g_ForceBuildFlags);
    if (result != Spawn_OK)
    {
        switch (result)
        {
            case Spawn_Collision: return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_Collision");
            case Spawn_NoBuild: return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_NoBuild");
            case Spawn_RespawnRoom: return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_RespawnRoom");
            // Default to Spawn_Error
            default: return EW_RejectCommand(client, "%s %t", EW_CHAT_TAG, "EW_ForceBuild_Failed");
        }
    }


    // Success, Get the building name translation key and print message
    EW_GetTranslationKey(buildingType, false, buffer, sizeof(buffer));
    if (owner == client)
        ShowActivity2(client, EW_CHAT_TAG, " %t", "EW_ForceBuild_Spawn", level, buffer);
    else
        ShowActivity2(client, EW_CHAT_TAG, " %t", "EW_ForceBuild_SpawnOwner", level, buffer, ownerName);

    CreateBeamRing(position);
    return Plugin_Handled;
}



//==================================================================
// Stocks

/**
 * Create a ring effect thing that looks pretty cool
 */
static stock void CreateBeamRing(
    const float position[3],
    int red=75,
    int green=255,
    int blue=75,
    int alpha=255)
{
    int beamColor[4];
    beamColor[0] = red;
    beamColor[1] = green;
    beamColor[2] = blue;
    beamColor[3] = alpha;
    TE_SetupBeamRingPoint(position, 10.0, 250.0, g_BeamSprite, g_HaloSprite, 0, 15, 0.5, 5.0, 0.0, beamColor, 10, 0);
    TE_SendToAll();
}


/**
 * Take a string of ALLOW_* flags and process it into
 * the actual ALLOW_* flags.
 */
static stock int ParseBuildingPermissionFlags(ConVar cvar)
{
    // Get string
    char buffer[ALLOW_FLAGS_TOTAL * 32];
    char exploded[ALLOW_FLAGS_TOTAL][32];
    cvar.GetString(buffer, sizeof(buffer));
    ExplodeString(buffer, ",", exploded, sizeof(exploded), sizeof(exploded[]));

    // Clean string and find flags
    int flags = 0;
    for (int i = 0; i < ALLOW_FLAGS_TOTAL; i++)
    {
        TrimString(exploded[i]);

        if (StrEqual(exploded[i], "NOBUILD", true))
            flags |= ALLOW_NOBUILD;
        else if (StrEqual(exploded[i], "RESPAWN", true))
            flags |= ALLOW_RESPAWN;
        else if (StrEqual(exploded[i], "TEAMRESPAWN", true))
            flags |= ALLOW_TEAMRESPAWN;
        else if (StrEqual(exploded[i], "COLLISION", true))
            flags |= ALLOW_COLLISION;
    }

    return flags;
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
