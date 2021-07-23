/**
 * =============================================================================
 * Engineer's Workshop - Commands
 * Miscellaneous building related commands.
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
#define UPDATE_URL "https://sirdigbot.github.io/engineers-workshop/updater_commands.txt"

public Plugin myinfo = 
{
    name = "[TF2] Engineer's Workshop - Commands",
    author = "SirDigbot",
    description = "Miscellaneous building related commands.",
    version = PLUGIN_VERSION,
    url = PLUGIN_URL
};


//==================================================================
// Globals
ConVar g_Cvar_IsUpdateEnabled;

//==================================================================
// Forwards
public void OnPluginStart()
{
    LoadTranslations("common.phrases");
    LoadTranslations("engineersworkshop.phrases");
    LoadTranslations("ew_commands.phrases");
    
    // Create Cvars
    CreateConVar("sm_ew_commands_version", PLUGIN_VERSION, "Engineer's Workshop - Commands version. Do Not Touch!", FCVAR_SPONLY|FCVAR_REPLICATED|FCVAR_NOTIFY|FCVAR_DONTRECORD);
    g_Cvar_IsUpdateEnabled = CreateConVar("sm_ew_commands_update", "1", "Update Engineer's Workshop - Commands Automatically (Requires Updater)\n(Default: 1)", FCVAR_NONE, true, 0.0, true, 1.0);
    g_Cvar_IsUpdateEnabled.AddChangeHook(OnCvarChanged);
    
    // Create commands
    RegAdminCmd("sm_sap", Command_Sap, ADMFLAG_CHEATS, "Sap a building. Pretend you're a discount Zeus.");
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
// Commands

/**
 * Forms:
 * "sm_sap <Target> <Building> [Invulnerable 1/0] [Damage]"
 */
public Action Command_Sap(int client, int args)
{
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
