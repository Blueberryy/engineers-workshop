/**
 * =============================================================================
 * Copyright (C) 2021 SirDigbot
 * =============================================================================
 *
 * This file is part of Engineer's Workshop.
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

#if defined _engieshop_included
    #endinput
#endif
#define _engieshop_included

#include <sdktools> // For a bunch of stuff (mostly spawning related)
#include <tf2_stocks> // TF2_GetObjectType, TF2_GetObjectMode


//==================================================================
// General Constants

// Prefix used for all chat-displayed messages for Engineer's Workshop
#define EW_CHAT_TAG "\x04[SM]\x01"

// Building Type Flags
#define INDEX_SENTRY     0
#define INDEX_MINISENTRY 1
#define INDEX_DISPENSER  2
#define INDEX_ENTRY      3
#define INDEX_EXIT       4
#define BUILDING_SENTRY     (1<<INDEX_SENTRY)
#define BUILDING_MINISENTRY (1<<INDEX_MINISENTRY)
#define BUILDING_DISPENSER  (1<<INDEX_DISPENSER)
#define BUILDING_ENTRY      (1<<INDEX_ENTRY)
#define BUILDING_EXIT       (1<<INDEX_EXIT)
#define BUILDING_TYPES_TOTAL 5

// Building Type Grouped Flags
#define BUILDING_SENTRIES BUILDING_SENTRY|BUILDING_MINISENTRY
#define BUILDING_TELEPORTERS BUILDING_ENTRY|BUILDING_EXIT
#define BUILDING_ALL BUILDING_SENTRIES|BUILDING_DISPENSER|BUILDING_TELEPORTERS

// Maximum size of a building name buffer (EW_StringToBuildingType and EW_GetTranslationKey)
#define MAX_BUILDINGNAME_SIZE 33

// Maximum size of a boolean string buffer (EW_StringToBool)
#define MAX_BOOLSTRING_SIZE 33


//==================================================================
// Specialised Constants

// Structure to contain data output by ProcessTargetString
enum struct TargetData
{
    int count;
    int targets[MAXPLAYERS];
    char name[MAX_TARGET_LENGTH];
    bool nameIsMultiLang;
}

// Structure used to contain and check cooldown times
// (as unix time code/seconds) per each player.
enum struct Cooldown
{
    int times[MAXPLAYERS + 1];
    int interval;

    /**
     * Reset all client's cooldown times.
     * @noreturn
     */
    void Reset()
    {
        for (int i = 0; i < MAXPLAYERS + 1; i++)
            this.times[i] = 0;
    }

    /**
     * Update a client's last cooldown event time to now.
     * If provided, timeOverride must be a positive integer.
     *
     * @param client        Client index.
     * @param timeOverride  Optional. Time value to use instead of now.
     * @noreturn
     */
    void Update(int client, int timeOverride=-1)
    {
        this.times[client] = (timeOverride < 0) ? GetTime() : timeOverride;
    }

    /**
     * Check if player is out of a cooldown.
     *
     * If autoUpdate is true, also uppdate last event time
     * if they aren't in cooldown.
     *
     * @param client        Client index.
     * @param autoUpdate    Automatically update client's new cooldown time.
     * @return              True if in cooldown, false if not.
     */
    bool IsAllowed(int client, bool autoUpdate=true)
    {
        int now = GetTime();
        if (now - this.times[client] <= this.interval)
            return true;
        if (autoUpdate)
            this.Update(client, now);
        return false;
    }
}


//==================================================================
// Spawning Constants

// Results for stock functions that attempt to spawn a building.
enum SpawnResult
{
    Spawn_OK = 0,
    Spawn_Error,
    Spawn_Collision,
    Spawn_NoBuild,
    Spawn_RespawnRoom,
    Spawn_EnemyRespawnRoom
}

// Variations of a sentry that can be spawned with EW_SpawnSentry
enum SentryType
{
    SentryType_Normal = 0,
    SentryType_Mini,
    SentryType_Disposable           // Special type of mini
}

// Spawnflags for use with building spawner stocks
#define SPAWNFLAG_INVULNERABLE (1<<1)   // (2) Engine values. Don't change.
#define SPAWNFLAG_UPGRADEABLE (1<<2)    // (4)
#define SPAWNFLAG_INFAMMO (1<<3)        // (8)

// Building permission flags for use with building spawner stocks
#define ALLOW_NOBUILD (1<<4)        // Allow building in func_nobuild zones.
#define ALLOW_RESPAWN (1<<5)        // Allow building in any func_respawnroom zones.
#define ALLOW_TEAMRESPAWN (1<<6)    // Allow building in same team's respawn zone.
#define ALLOW_COLLISION (1<<7)      // Allow building even if colliding with surface.
#define ALLOW_FLAGS_TOTAL 4         // Total nubmer of ALLOW_* flags


//==================================================================
// Static Globals
static StringMap g_buildingTypeStrings = null;
static StringMap g_boolStrings = null;


//==================================================================
// Get Building Stocks

/**
 * Get the number of buildings of a certain type[s] owned by client.
 *
 * @param client        Client index.
 * @param buildingTypes Building types (Multiple BUILDING_* flags).
 * @return              Number of buildings.
 *
 * @error               Invalid client index.
 */
stock int EW_GetBuildingCount(int client, int buildingTypes=BUILDING_ALL)
{
    if (client < 1 || client > MaxClients) // Block server/0/unowned buildings
    {
        ThrowError("%T", "EW_InvalidClient", LANG_SERVER, client);
        return 0;
    }
    
    int index = MaxClients + 1;
    int type;
    int builder;
    int count = 0;
    while ((index = FindEntityByClassname(index, "obj_*")) != -1)
    {
        // Verify builder and type are valid
        if (!EW_GetTypeAndBuilder(index, type, builder))
            continue;

        // Verify building is of specified type[s] and owned by client
        if (builder != client || !(type & buildingTypes))
            continue;
        count++;
    }

    return count;
}


/**
 * Get the number of buildings of a certain type[s] owned by
 * the target[s] specified by the target string (e.g. "@all").
 *
 * @param str           Pattern to find clients against (ProcessTargetString)
 * @param client        Admin performing the action, or 0 if the server.
 * @param buildingTypes Building types (Multiple BUILDING_* flags).
 * @param filter_flags  COMMAND_FILTER_* flags.
 * @param targetError   Is set to true if target pattern was invalid.
 * @return              Number of buildings.
 *
 * @error               Invalid client index.
 */
stock int EW_GetTargetBuildingCount(
    const char[] str,
    int client,
    int buildingTypes=BUILDING_ALL,
    int filter_flags=0,
    bool &targetError=false)
{
    // Verify args
    if (StrEqual(str, ""))
    {
        targetError = true;
        return 0;
    }
    if (client < 0 || client > MaxClients) // Allow 0 (admin calling ProcessTargets)
    {
        ThrowError("%T", "EW_InvalidClient", LANG_SERVER, client);
        return 0;
    }

    // Process targets
    TargetData data;
    if (!EW_ProcessTargets(str, client, filter_flags, data, false))
    {
        targetError = true;
        return 0;
    }

    // Find and count buildings matching type
    int index = MaxClients + 1;
    int type;
    int builder;
    int count = 0;
    while ((index = FindEntityByClassname(index, "obj_*")) != -1)
    {
        // Verify builder and type are valid
        if (!EW_GetTypeAndBuilder(index, type, builder))
            continue;

        // Verify type is of specified type[s]
        if (!(type & buildingTypes))
            continue;

        // Verify building is owned by one of targeted clients
        for (int i = 0; i < data.count; i++)
        {
            if (data.targets[i] == builder)
            {
                count++;
                break;
            }
        }
    }

    return count;
}


/**
 * Get the number of buildings of a certain type[s].
 *
 * @param buildingTypes Building types (Multiple BUILDING_* flags).
 * @return              Number of buildings.
 */
stock int EW_GetAllBuildingCount(int buildingTypes=BUILDING_ALL)
{
    int index = MaxClients + 1;
    int type;
    int builder;
    int count = 0;
    while ((index = FindEntityByClassname(index, "obj_*")) != -1)
    {
        // Verify builder and type are valid
        // We verify builder so we can skip server/unowned buildings.
        if (!EW_GetTypeAndBuilder(index, type, builder))
            continue;

        // Verify type is of specified type[s]
        if (type & buildingTypes)
            count++;
    }
    return count;
}


/**
 * Get the building indexes of a certain type[s] owned by client.
 *
 * @param client        Client index.
 * @param buildingTypes Building types (Multiple BUILDING_* flags).
 * @param buildings     Output buffer to write entity indexes to.
 * @param size          Size of output buffer.
 * @return              Number of buildings output.
 *
 * @error               Invalid client index.
 */
stock int EW_GetBuildings(
    int client,
    int buildingTypes,
    int[] buildings,
    int size)
{
    if (client < 1 || client > MaxClients) // Block server/0/unowned buildings
    {
        ThrowError("%T", "EW_InvalidClient", LANG_SERVER, client);
        return 0;
    }
    int count = EW_FindBuildings(client, buildingTypes, buildings, size, 0);
    return count;
}


/**
 * Get an ArrayList of indexes for all buildings of a certain type[s]
 * owned by client.
 *
 * Caller must free the ArrayList handle.
 *
 * @param client        Client index.
 * @param buildingTypes Building types (Multiple BUILDING_* flags).
 * @return              ArrayList of entity indexes.
 *
 * @error               Invalid client index.
 */ 
stock ArrayList EW_GetBuildingsList(int client, int buildingTypes)
{
    // Verify client, if invalid throw error since return value is bad.
    if (client < 1 || client > MaxClients) // Block server/0/unowned buildings
    {
        ThrowError("%T", "EW_InvalidClient", LANG_SERVER, client);
        return null;
    }
    
    // Create output ArrayList
    ArrayList buildings = new ArrayList();
    EW_FindBuildingsArrayList(client, buildingTypes, buildings);
    return buildings;
}


/**
 * Get the building indexes of a certain type[s] owned by
 * the target[s] specified by the target string (e.g. "@all").
 *
 * @param str           Pattern to find clients against (ProcessTargetString)
 * @param client        Admin performing the action, or 0 if the server.
 * @param buildingTypes Building types (Multiple BUILDING_* flags).
 * @param filter_flags  COMMAND_FILTER_* flags.
 * @param buildings     Output buffer to write entity indexes to.
 * @param size          Size of output buffer.
 * @param targetError   Is set to true if target pattern was invalid.
 * @return              Number of buildings output.
 *
 * @error               Invalid client index.
 */
stock int EW_GetTargetBuildings(
    const char[] str,
    int client,
    int buildingTypes,
    int filter_flags,
    int[] buildings,
    int size,
    bool &targetError=false)
{
    // Verify args
    if (StrEqual(str, ""))
    {
        targetError = true;
        return 0;
    }
    if (client < 0 || client > MaxClients) // Allow 0 (admin calling ProcessTargets)
    {
        ThrowError("%T", "EW_InvalidClient", LANG_SERVER, client);
        return 0;
    }

    // Process targets
    TargetData data;
    if (!EW_ProcessTargets(str, client, filter_flags, data, false))
    {
        targetError = true;
        return 0;
    }

    // Collect and output buildings to array
    int count = 0;
    for (int i = 0; i < data.count; i++)
    {
        if (data.targets[i]) // Skip server/0/unowned buildings
            count += EW_FindBuildings(data.targets[i], buildingTypes, buildings, size, count);
    }
    return count;
}


/**
 * Get an ArrayList of entity indexes for all buildings of a certain type[s]
 * owned by the target[s] specified by the target string (e.g. "@all").
 *
 * Caller must free the ArrayList handle.
 *
 * @param str           Pattern to find clients against (ProcessTargetString)
 * @param client        Admin performing the action, or 0 if the server.
 * @param buildingTypes Building types (Multiple BUILDING_* flags).
 * @param filter_flags  COMMAND_FILTER_* flags.
 * @return              ArrayList of indexes, Null if invalid target pattern.
 *
 * @error               Invalid client index.
 */ 
stock ArrayList EW_GetTargetBuildingsList(
    const char[] str,
    int client,
    int buildingTypes,
    int filter_flags)
{
    // Verify args
    if (StrEqual(str, ""))
        return null;
    if (client < 0 || client > MaxClients) // Allow 0 (admin calling ProcessTargets)
    {
        ThrowError("%T", "EW_InvalidClient", LANG_SERVER, client);
        return null;
    }

    // Process targets
    TargetData data;
    if (!EW_ProcessTargets(str, client, filter_flags, data, false))
        return null;

    // Create output arraylist
    ArrayList buildings = new ArrayList();
    for (int i = 0; i < data.count; i++)
    {
        if (data.targets[i]) // Skip server/0/unowned buildings
            EW_FindBuildingsArrayList(data.targets[i], buildingTypes, buildings);
    }
    return buildings;
}


/**
 * Get the building entity indexes of a certain type[s].
 *
 * @param buildingTypes Building types (Multiple BUILDING_* flags).
 * @param buildings     Output buffer to write entity indexes to.
 * @param size          Size of output buffer.
 * @return              Number of buildings written to buffer.
 */
stock int EW_GetAllBuildings(int buildingTypes, int[] buildings, int size)
{
    int count = 0;
    for (int i = 1; i <= MaxClients; i++) // Skip server/0/unowned buildings
        count += EW_FindBuildings(i, buildingTypes, buildings, size, count);
    return count;
}


/**
 * Get an ArrayList of entity indexes for all buildings of
 * a certain type[s].
 *
 * Caller must free the ArrayList handle.
 *
 * @param buildingTypes Building types (Multiple BUILDING_* flags).
 * @return              ArrayList of entity indexes.
 */ 
stock ArrayList EW_GetAllBuildingsList(int buildingTypes)
{
    ArrayList buildings = new ArrayList();
    for (int i = 1; i <= MaxClients; i++) // Skip server/0/unowned buildings
        EW_FindBuildingsArrayList(i, buildingTypes, buildings);
    return buildings;
}


/**
 * Find all buildings matching the specified types that are owned by client
 * and add their indexes into an output array (starting at offset).
 *
 * @param client        Client index.
 * @param buildingTypes Building types (Multiple BUILDING_* flags).
 * @param buildings     Output buffer to write entity indexes to.
 * @param size          Size of output buffer.
 * @param offset        Offset of output buffer to start outputting at.
 * @return              Number of buildings written to buffer.
 *
 * @error               Invalid client index.
 */
stock int EW_FindBuildings(int client, int buildingTypes, int[] buildings, int size, int offset)
{
    // Check if client can own any buildings before doing expensive search
    if (client < 1 || client > MaxClients)
    {
        ThrowError("%T", "EW_InvalidClient", LANG_SERVER, client);
        return 0;
    }

    int index = MaxClients + 1;
    int type;
    int builder;
    int count = 0;
    while ((index = FindEntityByClassname(index, "obj_*")) != -1)
    {
        // Verify builder and type are valid
        if (!EW_GetTypeAndBuilder(index, type, builder))
            continue;

        // Verify building is of specified type[s] and owned by client
        if (builder != client || !(type & buildingTypes))
            continue;

        // Add building to output array and count
        buildings[count + offset] = index;
        count++;

        // End of array
        if (count + offset == size)
            break;
    }

    return count;
}


/**
 * Find all buildings matching the specified types that are owned by client
 * and push their indexes into an output ArrayList.
 *
 * @param client        Client index.
 * @param buildingTypes Building types (Multiple BUILDING_* flags).
 * @param buildings     Output ArrayList to write entity indexes to.
 * @return              Number of buildings written to buffer.
 *
 * @error               Invalid client index, or invalid ArrayList output.
 */
stock int EW_FindBuildingsArrayList(int client, int buildingTypes, ArrayList buildings)
{
    // Check we can actually output values
    if (buildings == null)
    {
        ThrowError("%T", "EW_NullArrayList", LANG_SERVER); 
        return 0;
    }

    // Check if client can own any buildings before doing expensive search
    if (client < 1 || client > MaxClients)
    {
        ThrowError("%T", "EW_InvalidClient", LANG_SERVER, client);
        return 0;
    }

    int index = MaxClients + 1;
    int type;
    int builder;
    int count = 0;
    while ((index = FindEntityByClassname(index, "obj_*")) != -1)
    {
        // Verify builder and type are valid
        if (!EW_GetTypeAndBuilder(index, type, builder))
            continue;

        // Verify building is of specified type[s] and owned by client
        if (builder != client || !(type & buildingTypes))
            continue;

        // Add building to output arraylist and count
        buildings.Push(index);
        count++;
    }

    return count;
}


//==================================================================
// Entity Information Stocks

/**
 * Is entity a mini building (e.g. Mini-sentry)
 * Buildings only have this set after placement.
 *
 * @param entity        Entity index.
 * @param knownBuilding Optional. Is entity already known to be a building.
 * @return              True if mini building, false otherwise.
 */
stock bool EW_IsMiniBuilding(int entity, bool knownBuilding=false)
{
    if (knownBuilding || HasEntProp(entity, Prop_Send, "m_bMiniBuilding"))
        return view_as<bool>(GetEntProp(entity, Prop_Send, "m_bMiniBuilding"));
    return false;
}


/**
 * Get the building index of the building a sapper is attached to
 *
 * This function will return unexpected results if used on any building that
 * isnt a sapper.
 *
 * @param sapper        Sapper index
 * @param knownSapper   Optional. Is sapper already known to be a valid sapper.
 * @return              Entity index, -1 if invalid.
 */
stock int EW_GetSappedBuilding(int sapper, bool knownSapper=false)
{
    if (knownSapper || HasEntProp(sapper, Prop_Send, "m_hBuiltOnEntity"))
        return GetEntPropEnt(sapper, Prop_Send, "m_hBuiltOnEntity");
    return -1;
}


/**
 * Is an entity a valid building.
 *
 * @param entity     Entity index.
 * @return           True if entity is a building.
 */
stock bool EW_IsABuilding(int entity)
{
    if (!IsValidEntity(entity))
        return false;

    // m_bMiniBuilding exists in CBaseObject.
    // Not only do all valid building types subclass this, but it's required
    // that entities used in EW_IsPointInNoBuild are a CBaseObject.
    // So this is a convenient way to check this.
    return HasEntProp(entity, Prop_Send, "m_bMiniBuilding");
}


/**
 * Get the type and builder of an entity, if any.
 * Return value verifies that both values are valid.
 *
 * @param entity        Entity index.
 * @param type          Output of the building type.
 * @param builder       Output of the building owner.
 * @return              True if client and building are both valid.
 *
 * @error               Invalid entity.
 */
stock bool EW_GetTypeAndBuilder(int entity, int& type, int& builder)
{
    type = EW_GetBuildingType(entity); // Safely handles non-buildings already
    builder = EW_GetBuilder(entity, false); // Do not assume entity is building
    return type && builder != -1;
}


/**
 * Get the owner of a building.
 * Buildings only get a builder after being placed.
 *
 * @param entity        Entity index.
 * @param knownBuilding Optional. Is entity already known to be a building.
 * @return              Client index, -1 if invalid.
 */
stock int EW_GetBuilder(int entity, bool knownBuilding=false)
{
    if (knownBuilding || HasEntProp(entity, Prop_Send, "m_hBuilder"))
        return GetEntPropEnt(entity, Prop_Send, "m_hBuilder");
    return -1;
}


/**
 * Get the building type of an entity.
 * Returns 0 if invalid entity or entity is not a building.
 *
 * @param entity        Building entity.
 * @return              Building type (Single BUILDING_* flag) or 0 if invalid.
 */
stock int EW_GetBuildingType(int entity)
{
    if (!EW_IsABuilding(entity))
        return 0;

    switch (TF2_GetObjectType(entity))
    {
        case TFObject_Sentry:
        {
            if (EW_IsMiniBuilding(entity, true))
                return BUILDING_MINISENTRY;
            return BUILDING_SENTRY;
        }

        case TFObject_Dispenser: 
            return BUILDING_DISPENSER;
            
        case TFObject_Teleporter:
        {
            TFObjectMode mode = TF2_GetObjectMode(entity);
            if (mode == TFObjectMode_Entrance)
                return BUILDING_ENTRY;
            else if (mode == TFObjectMode_Exit)
                return BUILDING_EXIT;
        }
    }

    // TFObject_CartDispenser and TFObjectMode_None are invalid
    return 0;
}


//==================================================================
// Helper Stocks

/**
 * Get the building type from a string.
 * Returns 0 if invalid.
 *
 * Valid string values are configured inside engieshop_data.cfg
 *
 * @param str           String input.
 * @return              Building type (Single BUILDING_* flag) or 0 if invalid.
 */
stock int EW_StringToBuildingType(const char[] str)
{
    InitStringsCache();

    // Create copy of str
    int len = strlen(str);
    if (len < 1)
        return 0;
    char[] buffer = new char[len + 1];

    // Force uppercase for case-insensitive lookup
    for (int i = 0; i < len + 1; i++)
        buffer[i] = CharToUpper(str[i]);
    buffer[len] = '\0';
    
    int buildingType = 0;
    g_buildingTypeStrings.GetValue(buffer, buildingType);
    return buildingType;
}


/**
 * Get the boolean value from a string that represents a boolean state.
 * E.g. "1" or "0", "On" or "Off", "Enabled" or "Disabled", etc.
 *
 * Valid string values are configured inside engieshop_data.cfg
 *
 * @param str           Case-insenitive string input.
 * @param error         Optional error state output. True if string is invalid.
 * @return              True if string represents a true boolean value.
 */
stock bool EW_StringToBool(const char[] str, bool &error=false)
{
    InitStringsCache();

    // Create copy of str
    int len = strlen(str);
    if (len < 1)
    {
        // Empty string is false and invalid/error
        error = true;
        return false;
    }
    char[] buffer = new char[len + 1];

    // Force uppercase for case-insensitive lookup
    for (int i = 0; i < len + 1; i++)
        buffer[i] = CharToUpper(str[i]);
    buffer[len] = '\0';
    
    bool result = false;
    error = !g_boolStrings.GetValue(buffer, result);
    return result;
}


/**
 * Retrieves the building type from a command argument string given its index.
 *
 * @param arg     Argument number to retrieve.
 * @return        Building types (Multiple BUILDING_* flags), or 0 if invalid.
 */
stock int EW_GetCmdArgBuildingType(int arg)
{
    char buffer[MAX_BUILDINGNAME_SIZE];
    GetCmdArg(arg, buffer, sizeof(buffer));
    return EW_StringToBuildingType(buffer);
}


/**
 * Retrieves the boolean value from a command argument string given its index.
 *
 * @param arg     Argument number to retrieve.
 * @param error   Optional error state output. True if string is invalid.
 * @return        True if string represents a true boolean value.
 */
stock bool EW_GetCmdArgBoolString(int arg, bool &error=false)
{
    char buffer[MAX_BOOLSTRING_SIZE];
    GetCmdArg(arg, buffer, sizeof(buffer));
    return EW_StringToBool(buffer, error);
}


/**
 * Convert a single BUILDING_* flag into an INDEX_* constant.
 * If multiple or invalid flags are given this will return -1.
 *
 * @param buildingType  Building type (Single BUILDING_* flag).
 * @return              Array index (INDEX_* constant), -1 if invalid type.
 */
stock int EW_TypeToIndex(int buildingType)
{ 
    switch (buildingType)
    {
        case BUILDING_SENTRY: return INDEX_SENTRY;
        case BUILDING_MINISENTRY: return INDEX_MINISENTRY;
        case BUILDING_DISPENSER: return INDEX_DISPENSER;
        case BUILDING_ENTRY: return INDEX_ENTRY;
        case BUILDING_EXIT: return INDEX_EXIT;
    }
    return -1; // Invalid index = -1
}


/**
 * Convert an INDEX_* constant into a BUILDING_* flag.
 * Returns 0 if invalid.
 *
 * @param typeIndex     Array index (INDEX_* constant)
 * @return              Building type (Single BUILDING_* flag), 0 if invalid.
 */
stock int EW_IndexToType(int typeIndex)
{
    switch (typeIndex)
    {
        case INDEX_SENTRY: return BUILDING_SENTRY;
        case INDEX_MINISENTRY: return BUILDING_MINISENTRY;
        case INDEX_DISPENSER: return BUILDING_DISPENSER;
        case INDEX_ENTRY: return BUILDING_ENTRY;
        case INDEX_EXIT: return BUILDING_EXIT;
    }
    return 0; // No type = 0
}


/**
 * Set values in an bool array (intended to store a value for each type of
 * building), only setting the value for the building types specified.
 *
 * @param value             Value to set.
 * @param buildingTypes     Building types (Multiple BUILDING_* flags)
 * @param array             Output array of building types.
 * @noreturn
 */
stock void EW_SetBuildingArrayBool(
    bool value,
    int buildingTypes,
    bool array[BUILDING_TYPES_TOTAL])
{
    // Can't use EW_TypeToIndex because we want to handle all type flags
    // individually, to allow skipping values.
    if (buildingTypes & BUILDING_SENTRY)
        array[INDEX_SENTRY] = value;
    if (buildingTypes & BUILDING_MINISENTRY)
        array[INDEX_MINISENTRY] = value;
    if (buildingTypes & BUILDING_DISPENSER)
        array[INDEX_DISPENSER] = value;
    if (buildingTypes & BUILDING_ENTRY)
        array[INDEX_ENTRY] = value;
    if (buildingTypes & BUILDING_EXIT)
        array[INDEX_EXIT] = value;
}


/**
 * Check if an building type integer contains only a single building type.
 *
 * @param x             Signed or unsigned integer.
 * @return              True if single flag, false if multiple.
 */
stock bool EW_IsSingleType(int buildingType)
{
    // If value is non-zero and is a power of 2 it is a single flag
    return buildingType > 0 && (buildingType & (buildingType - 1)) == 0;
}


/**
 * Get the translation key for the name of a building by its type.
 * This value can then be used with %T for displaying building names
 * or with {N:t} formats inside other translations.
 *
 * e.g. BUILDING_SENTRIES returns "EW_Sentries", so you can do:
 * PrintToChatAll("%T", "MSG_DeletedBuildings", LANG_SERVER, target, "EW_Sentries");
 * Which might say "[SM] Deleted PlayerName's Sentries."
 *
 * @param buildingTypes     Building types (Multiple BUILDING_* flags).
 * @param plural            Get the plural names for buildings.
 * @param str               Output array.
 * @param size              Max size of output array.
 * @noreturn
 */
stock void EW_GetTranslationKey(int buildingTypes, bool plural, char[] str, int size)
{
    if (plural)
    {
        switch (buildingTypes)
        {
            case BUILDING_SENTRY, BUILDING_SENTRIES: strcopy(str, size, "EW_Sentries");
            case BUILDING_MINISENTRY: strcopy(str, size, "EW_MiniSentries");
            case BUILDING_DISPENSER: strcopy(str, size, "EW_Dispensers");
            case BUILDING_ENTRY: strcopy(str, size, "EW_EntryTeles");
            case BUILDING_EXIT: strcopy(str, size, "EW_ExitTeles");
            case BUILDING_TELEPORTERS: strcopy(str, size, "EW_Teleporters");
            // Default to BUILDING_ALL if it's an unknown combination
            default: strcopy(str, size, "EW_Buildings");
        }
    }
    else
    {
        switch (buildingTypes)
        {
            case BUILDING_SENTRY: strcopy(str, size, "EW_Sentry");
            case BUILDING_SENTRIES: strcopy(str, size, "EW_Sentries");
            case BUILDING_MINISENTRY: strcopy(str, size, "EW_MiniSentry");
            case BUILDING_DISPENSER: strcopy(str, size, "EW_Dispenser");
            case BUILDING_ENTRY: strcopy(str, size, "EW_EntryTele");
            case BUILDING_EXIT: strcopy(str, size, "EW_ExitTele");
            case BUILDING_TELEPORTERS: strcopy(str, size, "EW_Teleporters");
            // Default to BUILDING_ALL if it's an unknown combination
            default: strcopy(str, size, "EW_Buildings");
        }
    }
}


//==================================================================
// Misc. Stocks

/**
 * Show an annotation to a client.
 *
 * @param client        Client index
 * @param followEntity  Entity for annotation to follow. -1 for none.
 * @param duration      Duration of annotation in seconds.
 * @param showEffect    Show with popup animation.
 * @param position      World position of annotation. Overrides followEntity.
 * @param sound         Sound file (relative to '/sound'). Must be precached.
 * @param format        Formatting rules.
 * @param ...           Variable number of format parameters.
 * @return              True if annotation was shown, false otherwise.
 *
 * @error               Invalid client or follow entity.
 */
stock bool EW_ShowAnnotation(
    int client,
    int followEntity,
    float duration=2.0,
    bool showEffect=true,
    float position[3]=NULL_VECTOR,
    const char[] sound=NULL_STRING,
    const char[] format=NULL_STRING, 
    any ...)
{
    // Get a mostly-unique id offset to prevent clashing with other plugins'
    // annotations (otherwise we'd just use the client index and always clash)
    static int pluignOffset = -1;
    if (pluignOffset < 0)
        pluignOffset = GetRandomInt(0, 2147483647 - (MAXPLAYERS + 1));


    // Verify client
    if (client < 1 || client > MaxClients) // Can't display to server
    {
        ThrowError("%T", "EW_InvalidClient", LANG_SERVER, client);
        return false;
    }
    // Due to integers being limited to 32 bit, there's no way to set
    // the visibilityBitfield for any client index higher than 32.
    if (client >= 32)
        return false;
    
    // Reject (but don't error on) bots or connecting players
    if (IsFakeClient(client) || !IsClientInGame(client)) 
        return false;

    // Check if we are following entity or using a position
    bool usingPos = !IsNullVector(position);
    if (!usingPos && !IsValidEntity(followEntity)) // TODO: Should this be edict?
    {
        ThrowError("%T", "EW_InvalidEntity", LANG_SERVER, followEntity);
        return false;
    }

    // Verify duration
    if (duration < 0.0)
        return true; // Let's pretend we did the homework, the teacher can't check

    // Precache sound if it is not default
    bool defaultSound = IsNullString(sound);
    if (!defaultSound)
        PrecacheSound(sound);
        
    // Get formatted input string
    int len = strlen(format) + 255;
    char[] message = new char[len];
    if (!IsNullString(format))
        VFormat(message, len, format, 8);
    else
        message[0] = '\0';


    // Create annotation
    Event annotation = CreateEvent("show_annotation");
    if (annotation == null)
        return false;

    // Show only one per player, per plugin.
    annotation.SetInt("id", client + pluignOffset);
    if (!usingPos)
        annotation.SetInt("follow_entindex", followEntity);
    else
    {
        annotation.SetFloat("worldPosX", position[0]);
        annotation.SetFloat("worldPosY", position[1]);
        annotation.SetFloat("worldPosZ", position[2]);
        annotation.SetFloat("worldNormalX", 0.0);
        annotation.SetFloat("worldNormalY", 0.0);
        annotation.SetFloat("worldNormalZ", 1.0); // +Z is up in source
    }
    annotation.SetInt("visibilityBitfield", (1 << client));
    annotation.SetFloat("lifetime", duration);
    if (!defaultSound)
        annotation.SetString("play_sound", sound); // play_sound is optional
    annotation.SetString("text", message);
    annotation.SetBool("show_effect", showEffect);
    annotation.Fire();
    return true;
}


/**
 * Wrapper for ReplyToCommand used for rejecting a command,
 * usually because the arguments provided are invalid in some way.
 *
 * @param client        Client index.
 * @param format        Formatting rules.
 * @param ...           Variable number of format parameters.
 * @return              Always returns Plugin_Handled.
 */
stock Action EW_RejectCommand(int client, const char[] format, any ...)
{
    int len = strlen(format) + 255;
    char[] str = new char[len];
    VFormat(str, len, format, 3);
    ReplyToCommand(client, str);
    return Plugin_Handled;
}


/**
 * Wrapper for ProcessTargetString that outputs a TargetData structure.
 *
 * @param str               Pattern to find clients against.
 * @param client            Admin performing the action, or 0 if the server.
 * @param filter_flags      COMMAND_FILTER_* flags.
 * @param data              Ouput data.
 * @return                  False if no targets found.
 */
stock bool EW_ProcessTargets(
    const char[] str,
    int client,
    int filter_flags,
    TargetData data,
    bool replyError=true)
{
    data.count = ProcessTargetString(str,
                    client,
                    data.targets,
                    sizeof(data.targets),
                    filter_flags,
                    data.name,
                    sizeof(data.name),
                    data.nameIsMultiLang);
    if (data.count <= 0)
    {
        // On fail, ProcessTargetString returns COMMAND_TARGET_* reason
        // So count is our error code
        if (replyError)
            ReplyToTargetError(client, data.count);
        return false;
    }
    return true;
}


//==================================================================
// Spawning Stocks

/**
 * Get the world position where a client is aiming.
 *
 * @param client        Client index.
 * @param origin        Output vector.
 * @param maxDist       Maximum distance allowed to aim. -1 for infinite.
 * @param flags         Optional. TraceRay flags (MASK_*).
 * @return              True if client is aiming within range at solid surface.
 */
stock bool EW_GetClientAimOrigin(int client, float origin[3], float maxDist=-1.0, int flags=MASK_SOLID)
{
    float eyeAngles[3];
    float eyePos[3];
    GetClientEyePosition(client, eyePos);
    GetClientEyeAngles(client, eyeAngles);

    TR_TraceRayFilter(eyePos, eyeAngles, flags, RayType_Infinite, TRIgnoreSelf, client);
    if (TR_DidHit(null))
    {
        // Get end position of ray, but put in temp until we've checked it.
        float originTemp[3];
        TR_GetEndPosition(originTemp, null);

        // Check if end position it outside of range
        if (maxDist > 0.0 && GetVectorDistance(eyePos, originTemp, false) > maxDist)
            return false;
        
        for (int i = 0; i < sizeof(origin); i++)
            origin[i] = originTemp[i];
        return true;
    }
    return false;
}

// Trace filter for EW_GetClientAimOrigin
static stock bool TRIgnoreSelf(int entity, int contentsMask, any data)
{
    return entity != data;
}


/**
 * Check if a build position is in a func_nobuild zone that disallows
 * that building type.
 *
 * @param building      Entity index of building.
 * @param buildOrigin   Position to build at.
 * @return              True if position is in a func_nobuild zone.
 */
stock bool EW_IsPointInNoBuild(int building, const float buildOrigin[3])
{
    static Handle sdkCall = null;

    if (sdkCall == null)
    {
        GameData data = new GameData("tf2.engineersworkshop");
        if (data == null)
            SetFailState("%T", "EW_MissingGameData", LANG_SERVER);
        
        // Big thanks to asherkin, Psyk0tik, nosoop and Addie for helping with this function.
        StartPrepSDKCall(SDKCall_Static);
        PrepSDKCall_SetFromConf(data, SDKConf_Signature, "PointInNoBuild");
        PrepSDKCall_AddParameter(SDKType_Vector, SDKPass_ByRef);
        PrepSDKCall_AddParameter(SDKType_CBaseEntity, SDKPass_Pointer);
        PrepSDKCall_SetReturnInfo(SDKType_Bool, SDKPass_Plain);
        sdkCall = EndPrepSDKCall();
        delete data;

        if (sdkCall == null)
            SetFailState("%T", "EW_GameDataPrepError", LANG_SERVER, "PointInNoBuild");
    }

    // Only CBaseObject is supported
    if (!EW_IsABuilding(building))
        return false;
    return SDKCall(sdkCall, buildOrigin, building);
}


/**
 * Check if a position is within an active respawn room (func_respawnroom).
 *
 * If an entity is provided, and the origin is not in a respawn room,
 * this will check if the entity is within a respawn room.
 * Additionally, checkIfSameTeam is true, this will check that the entity
 * is also on the same team as the respawn room.
 *
 * If the origin is within a respawn room, checkIfSameTeam is ALWAYS true.
 * But it can only check the team if supplied a valid entity.
 *
 * @param origin            Position to check is inside respawn room.
 * @param entity            Optional. Entity index to check after origin.
 * @param checkIfSameTeam   Check if respawn room belongs to the entity's team.
 * @return                  True if origin/entity is in func_respawnroom zone.
 */
stock bool EW_IsPointInRespawnRoom(
    const float origin[3],
    int entity=INVALID_ENT_REFERENCE,
    bool checkIfSameTeam=false)
{
    static Handle sdkCall = null;

    if (sdkCall == null)
    {
        GameData data = new GameData("tf2.engineersworkshop");
        if (data == null)
            SetFailState("%T", "EW_MissingGameData", LANG_SERVER);
        
        StartPrepSDKCall(SDKCall_Static);
        PrepSDKCall_SetFromConf(data, SDKConf_Signature, "PointInRespawnRoom");
        PrepSDKCall_AddParameter(SDKType_CBaseEntity, SDKPass_Pointer, VDECODE_FLAG_ALLOWNULL);
        PrepSDKCall_AddParameter(SDKType_Vector, SDKPass_ByRef);
        PrepSDKCall_AddParameter(SDKType_Bool, SDKPass_Plain);
        PrepSDKCall_SetReturnInfo(SDKType_Bool, SDKPass_Plain);
        sdkCall = EndPrepSDKCall();
        delete data;

        if (sdkCall == null)
            SetFailState("%T", "EW_GameDataPrepError", LANG_SERVER, "PointInRespawnRoom");
    }

    // Any CBaseEntity is supported including null (it will only check origin).
    // However we must use INVALID_ENT_REFERENCE instead of null or we error.
    if (entity < 1 || entity > GetMaxEntities())
        entity = INVALID_ENT_REFERENCE;
    return SDKCall(sdkCall, entity, origin, checkIfSameTeam);
}


/**
 * Check if an entity is in a respawn room.
 * Additionally, if checkIfSameTeam is true you can check if an entity is on
 * the same team as the respawn room if it is within one.
 *
 * This is a wrapper for EW_IsPointInRespawnRoom that uses an probably-invalid
 * origin to force it to only check the entity.
 *
 * @param entity            Entity index.
 * @param checkIfSameTeam   Check if respawn room belongs to the entity's team.
 * @return                  True if entity is in respawn room.
 */
stock bool EW_IsEntityInRespawnRoom(int entity, bool checkIfSameTeam=false)
{
    // Witchcraft -- view_as<float>(0x7F7FFFFF) is FLOAT_MAX. thank mr nosoop.
    static const float invalidPos[3] = {view_as<float>(0x7F7FFFFF), ...};
    return EW_IsPointInRespawnRoom(invalidPos, entity, checkIfSameTeam);
}


/**
 * Is a building allowed to be built at a certain position.
 * This will check for collision obstruction and func_nobuild zones.
 *
 * The building permission flag ALLOW_TEAMRESPAWN wont work as expected if
 * the building has no current team.
 *
 * @param building          Entity index of building.
 * @param vecMins           Entity m_vecMins.
 * @param vecMaxs           Entity m_vecMaxs.
 * @param pos               Position to build at.
 * @param buildFlags        Building permission flags (ALLOW_* flags)
 * @return                  Spawn_OK if building allowed.
 */
stock SpawnResult EW_CanBuildAtPos(
    int building,
    const float vecMins[3],
    const float vecMaxs[3],
    const float pos[3],
    int buildFlags)
{
    if (!IsValidEdict(building))
        return Spawn_Error;

    // Names just to make code logic more readable
    bool nobuildAllowed = view_as<bool>(buildFlags & ALLOW_NOBUILD);
    bool respawnAllowed = view_as<bool>(buildFlags & ALLOW_RESPAWN);
    bool sameTeamAllowed = view_as<bool>(buildFlags & ALLOW_TEAMRESPAWN);
    bool collisionAllowed = view_as<bool>(buildFlags & ALLOW_COLLISION);

    if (!nobuildAllowed && EW_IsPointInNoBuild(building, pos))
        return Spawn_NoBuild;
    
    if (!respawnAllowed)
    {
        // If any respawn room not allowed, check if it's the SAME TEAM'S room.
        //
        // This means passing true which will only return true if inside the
        // same teams respawn room--so we need to invert the result to deny
        // players on the wrong team.
        //
        // Because this is used by the spawning stocks, and the entity
        // hasn't actually been spawned yet, we can only check the spawn origin
        // since the respawn room's IsTouching(building) func wont pass true.
        // This ignores the hull but can correctly handle teams (if set prior).
        // TODO: Fix this if possible.
        if (sameTeamAllowed && !EW_IsPointInRespawnRoom(pos, building, true))
            return Spawn_EnemyRespawnRoom;
        else if (!sameTeamAllowed && EW_IsPointInRespawnRoom(pos))
            return Spawn_RespawnRoom;
    } 

    if (!collisionAllowed)
    {
        // Scale hull to reduce collisions
        float scaledMins[3];
        float scaledMaxs[3];
        for (int i = 0; i < 3; i++)
        {
            scaledMins[i] = vecMins[i] * 0.85; // Don't scale too low
            scaledMaxs[i] = vecMaxs[i] * 0.85;
        }

        // This is not a great way to check hull collision.
        // Ideally we'd use CBaseObject::CalculatePlacementPos and then
        // UTIL_TraceEntity, but we cant access m_vecBuildOrigin AND
        // sourcemod doesn't provide TraceEntity (which returns sm_trace_t
        // so we can't use it anyway).
        
        // Increase Z gradually to see if we can spawn slightly in the air.
        // We don't need to actually move the building, it's just
        // to add some tolerance for terrain.
        for (int i = 0; i < 4; i++)
        {
            TR_TraceHull(pos, pos, scaledMins, scaledMaxs, MASK_SOLID);
            if (!TR_DidHit(null))
                break;
            else if (i == 3) // Last loop and still a hit means we cant spawn
                return Spawn_Collision;

            scaledMins[2] += 2.0;
            scaledMaxs[2] += 2.0;
        }
    }
    return Spawn_OK;
}


/**
 * Pelipoika's Building Spawning Stocks, but cleaned up a little
 * If it ain't broke don't fix it :P
 * https://forums.alliedmods.net/showthread.php?p=2148102
 */


/**
 * Spawn a building at a position.
 * Only a single building type can be specified.
 *
 * @param builder       Client index.
 * @param buildingType  Building type (Single BUILDING_* flag).
 * @param position      Coordinates to spawn building at.
 * @param angles        Angles to spawn building facing.
 * @param level         Building upgrade level.
 * @param buildFlags    Building permission flags (ALLOW_* flags)
 * @param spawnFlags    Optional. Entity Spawnflags (SPAWNFLAG_* flags)
 * @return              Spawn_OK on success.
 */
stock SpawnResult EW_SpawnBuilding(
    int builder,
    int buildingType,
    const float position[3],
    const float angles[3],
    int level=1,
    int buildFlags=0,
    int spawnFlags=SPAWNFLAG_UPGRADEABLE)
{
    switch (buildingType)
    {
        case BUILDING_SENTRY: return EW_SpawnSentry(builder, position, angles, SentryType_Normal, level, buildFlags, spawnFlags);
        case BUILDING_MINISENTRY: return EW_SpawnSentry(builder, position, angles, SentryType_Mini, level, buildFlags, spawnFlags);
        case BUILDING_DISPENSER: return EW_SpawnDispenser(builder, position, angles, level, buildFlags, spawnFlags);
        case BUILDING_ENTRY: return EW_SpawnTeleporter(builder, position, angles, TFObjectMode_Entrance, level, buildFlags, spawnFlags);
        case BUILDING_EXIT: return EW_SpawnTeleporter(builder, position, angles, TFObjectMode_Exit, level, buildFlags, spawnFlags);
    }
    return Spawn_Error;
}

/**
 * Spawn a sentry at a position.
 *
 * @param builder       Client index.
 * @param position      Coordinates to spawn building at.
 * @param angles        Angles to spawn building facing.
 * @param type          Type of sentry to spawn.
 * @param level         Building upgrade level.
 * @param buildFlags    Building permission flags (ALLOW_* flags)
 * @param spawnFlags    Optional. Entity Spawnflags (SPAWNFLAG_* flags)
 * @return              Spawn_OK on success.
 */
stock SpawnResult EW_SpawnSentry(
    int builder,
    const float position[3],
    const float angles[3],
    SentryType type=SentryType_Normal,
    int level=1,
    int buildFlags=0,
    int spawnFlags=SPAWNFLAG_UPGRADEABLE)
{
    static const float m_vecMins[3] = {-20.0, -20.0, 0.0};
    static const float m_vecMaxs[3] = {20.0, 20.0, 66.0};
    static const float m_vecMinsMini[3] = {-15.0, -15.0, 0.0};
    static const float m_vecMaxsMini[3] = {15.0, 15.0, 49.5};
    static const float m_vecMinsDisposable[3] = {-13.0, -13.0, 0.0};
    static const float m_vecMaxsDisposable[3] = {13.0, 13.0, 42.9};

    int sentry = CreateEntityByName("obj_sentrygun");
    if (!IsValidEntity(sentry))
        return Spawn_Error;

    // Set team before calling EW_CanBuildAtPos (to handle ALLOW_TEAMRESPAWN)
    SetVariantInt(GetClientTeam(builder));
    AcceptEntityInput(sentry, "SetTeam");

    // Verify building is allowed to be built in the specified position
    SpawnResult result;
    if (type == SentryType_Mini)
        result = EW_CanBuildAtPos(sentry, m_vecMinsMini, m_vecMaxsMini, position, buildFlags);
    else if (type == SentryType_Disposable)
        result = EW_CanBuildAtPos(sentry, m_vecMinsDisposable, m_vecMaxsDisposable, position, buildFlags);
    else // if (type == SentryType_Normal)
        result = EW_CanBuildAtPos(sentry, m_vecMins, m_vecMaxs, position, buildFlags);
    if (result != Spawn_OK)
    {
        RemoveEntity(sentry);
        return result;
    }


    // Set default properties
    AcceptEntityInput(sentry, "SetBuilder", builder);
    DispatchKeyValueVector(sentry, "origin", position);
    DispatchKeyValueVector(sentry, "angles", angles);

    SetEntProp(sentry, Prop_Send, "m_iUpgradeLevel", level);
    SetEntProp(sentry, Prop_Send, "m_iHighestUpgradeLevel", level);
    SetEntProp(sentry, Prop_Data, "m_spawnflags", spawnFlags);
    SetEntProp(sentry, Prop_Send, "m_bBuilding", 1);

    if (type == SentryType_Mini)
    {
        SetEntProp(sentry, Prop_Send, "m_bMiniBuilding", 1);
        SetEntProp(sentry, Prop_Send, "m_nSkin", level == 1 ? GetClientTeam(builder) : GetClientTeam(builder) - 2);
        DispatchSpawn(sentry);

        SetVariantInt(100);
        AcceptEntityInput(sentry, "SetHealth");

        SetEntPropFloat(sentry, Prop_Send, "m_flModelScale", 0.75);
        SetEntPropVector(sentry, Prop_Send, "m_vecMins", m_vecMinsMini);
        SetEntPropVector(sentry, Prop_Send, "m_vecMaxs", m_vecMaxsMini);
    }
    else if (type == SentryType_Disposable)
    {
        SetEntProp(sentry, Prop_Send, "m_bMiniBuilding", 1);
        SetEntProp(sentry, Prop_Send, "m_bDisposableBuilding", 1);

        SetEntProp(sentry, Prop_Send, "m_nSkin", level == 1 ? GetClientTeam(builder) : GetClientTeam(builder) - 2);
        DispatchSpawn(sentry);

        SetVariantInt(100);
        AcceptEntityInput(sentry, "SetHealth");

        SetEntPropFloat(sentry, Prop_Send, "m_flModelScale", 0.60);
        SetEntPropVector(sentry, Prop_Send, "m_vecMins", m_vecMinsDisposable);
        SetEntPropVector(sentry, Prop_Send, "m_vecMaxs", m_vecMaxsDisposable);
    }
    else // if (type == SentryType_Normal)
    {
        SetEntProp(sentry, Prop_Send, "m_nSkin", GetClientTeam(builder) - 2);
        DispatchSpawn(sentry);
    }

    return result;
}


/**
 * Spawn a dispenser at a position.
 *
 * @param builder       Client index.
 * @param position      Coordinates to spawn building at.
 * @param angles        Angles to spawn building facing.
 * @param level         Building upgrade level.
 * @param buildFlags    Building permission flags (ALLOW_* flags)
 * @param spawnFlags    Optional. Entity Spawnflags (SPAWNFLAG_* flags)
 * @return              Spawn_OK on success.
 */
stock SpawnResult EW_SpawnDispenser(
    int builder,
    const float position[3],
    const float angles[3],
    int level=1,
    int buildFlags=0,
    int spawnFlags=SPAWNFLAG_UPGRADEABLE)
{
    static const float m_vecMins[3] = {20.0, -20.0, 0.0};
    static const float m_vecMaxs[3] = {20.0, 20.0, 55.0};

    int dispenser = CreateEntityByName("obj_dispenser");
    if (!IsValidEntity(dispenser))
        return Spawn_Error;

    // Set team before calling EW_CanBuildAtPos (to handle ALLOW_TEAMRESPAWN)
    SetVariantInt(GetClientTeam(builder));
    AcceptEntityInput(dispenser, "SetTeam");

    // Verify building is allowed to be built in the specified position
    SpawnResult result = EW_CanBuildAtPos(dispenser, m_vecMins, m_vecMaxs, position, buildFlags);
    if (result != Spawn_OK)
    {
        RemoveEntity(dispenser);
        return result;
    }

    // Set properties
    DispatchKeyValueVector(dispenser, "origin", position);
    DispatchKeyValueVector(dispenser, "angles", angles);
    SetEntProp(dispenser, Prop_Send, "m_iHighestUpgradeLevel", level);
    SetEntProp(dispenser, Prop_Data, "m_spawnflags", spawnFlags);
    SetEntProp(dispenser, Prop_Send, "m_bBuilding", 1);
    DispatchSpawn(dispenser);

    SetEntProp(dispenser, Prop_Send, "m_nSkin", GetClientTeam(builder) - 2);
    ActivateEntity(dispenser); // Fixes screen

    AcceptEntityInput(dispenser, "SetBuilder", builder); // Gotta do this after activation.
    return result;
}


/**
 * Spawn a teleporter at a position.
 *
 * @param builder       Client index.
 * @param position      Coordinates to spawn building at.
 * @param angles        Angles to spawn building facing.
 * @param mode          Which type of teleporter to spawn.
 * @param level         Building upgrade level.
 * @param buildFlags    Building permission flags (ALLOW_* flags)
 * @param spawnFlags    Optional. Entity Spawnflags (SPAWNFLAG_* flags)
 * @return              Spawn_OK on success.
 */
stock SpawnResult EW_SpawnTeleporter(
    int builder,
    const float position[3],
    const float angles[3],
    TFObjectMode mode,
    int level=1,
    int buildFlags=0,
    int spawnFlags=SPAWNFLAG_UPGRADEABLE)
{
    static const float m_vecMins[3] = {-24.0, -24.0, 0.0};
    static const float m_vecMaxs[3] = {24.0, 24.0, 12.0};

    int teleporter = CreateEntityByName("obj_teleporter");
    if (!IsValidEntity(teleporter))
        return Spawn_Error;

    // Set team before calling EW_CanBuildAtPos (to handle ALLOW_TEAMRESPAWN)
    SetVariantInt(GetClientTeam(builder));
    AcceptEntityInput(teleporter, "SetTeam");

    // Verify building is allowed to be built in the specified position
    SpawnResult result = EW_CanBuildAtPos(teleporter, m_vecMins, m_vecMaxs, position, buildFlags);
    if (result != Spawn_OK)
    {
        RemoveEntity(teleporter);
        return result;
    }
    
    // Set properties
    DispatchKeyValueVector(teleporter, "origin", position);
    DispatchKeyValueVector(teleporter, "angles", angles);

    SetEntProp(teleporter, Prop_Send, "m_iHighestUpgradeLevel", level);
    SetEntProp(teleporter, Prop_Data, "m_spawnflags", spawnFlags);
    SetEntProp(teleporter, Prop_Send, "m_bBuilding", 1);
    SetEntProp(teleporter, Prop_Data, "m_iTeleportType", mode);
    SetEntProp(teleporter, Prop_Send, "m_iObjectMode", mode);
    SetEntProp(teleporter, Prop_Send, "m_nSkin", GetClientTeam(builder) - 2);
    DispatchSpawn(teleporter);

    AcceptEntityInput(teleporter, "SetBuilder", builder);
    return result;
}


//==================================================================
// Internal Functions

/**
 * Populate the internal building strings and bool strings
 * maps used by EW_StringToBuildingType and EW_StringToBool respectively.
 */
static void InitStringsCache()
{
    static bool init = false;
    if (init)
        return;
    init = true;

    // Find and verify config file
    char configPath[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, configPath, sizeof(configPath), "configs/engineersworkshop_data.cfg");
    if (!FileExists(configPath))
    {
        SetFailState("%T", "EW_NoConfig", LANG_SERVER, configPath);
        return;
    }

    // Import keyvalues
    KeyValues kv = new KeyValues("EngieShopData");
    if (!kv.ImportFromFile(configPath))
    {
        delete kv;
        SetFailState("%T", "EW_BadConfig", LANG_SERVER, configPath);
        return;
    }

    // Reset existing StringMaps (for safety?)
    delete g_buildingTypeStrings;
    delete g_boolStrings;
    g_buildingTypeStrings = new StringMap();
    g_boolStrings = new StringMap();

    // Add default Building Names
    g_buildingTypeStrings.SetValue("ALL",           BUILDING_ALL);
    g_buildingTypeStrings.SetValue("SENTRY",        BUILDING_SENTRY);
    g_buildingTypeStrings.SetValue("MINISENTRY",    BUILDING_MINISENTRY);
    g_buildingTypeStrings.SetValue("SENTRIES",      BUILDING_SENTRIES);
    g_buildingTypeStrings.SetValue("DISPENSER",     BUILDING_DISPENSER);
    g_buildingTypeStrings.SetValue("ENTRY",         BUILDING_ENTRY);
    g_buildingTypeStrings.SetValue("EXIT",          BUILDING_EXIT);
    g_buildingTypeStrings.SetValue("TELEPORTERS",   BUILDING_TELEPORTERS);

    // Add default Boolean Strings
    g_boolStrings.SetValue("0", false);
    g_boolStrings.SetValue("1", true);

    // Process additional strings
    Config_ProcessBuildingNames(kv);
    kv.Rewind();
    Config_ProcessBoolStrings(kv);

    delete kv;
    return;
}

/**
 * Process additional building type names from the config file
 */ 
static void Config_ProcessBuildingNames(KeyValues &kv)
{
    if (kv.JumpToKey("BuildingNames") && kv.GotoFirstSubKey(false)) // false = traverse values
    {
        char name[MAX_BUILDINGNAME_SIZE];
        char type[16];
        do
        {
            // Check if there is a name to add
            if(!kv.GetSectionName(name, sizeof(name)) || !kv.GetString(NULL_STRING, type, sizeof(type), ""))
                break;

            // Validate strings
            if (StrEqual(name, "") || StrEqual(type, ""))
                continue;

            // Force name to uppercase for case-insensitive lookup
            for (int i = 0; name[i] != '\0'; i++)
                name[i] = CharToUpper(name[i]);

            // Get building type -- These values are ONLY for the types in the config!
            // Can't use EW_StringToBuildingType since we're creating the StringMap it uses.
            int buildingType;
            if (StrEqual(type, "ALL"))
                buildingType = BUILDING_ALL;
            else if (StrEqual(type, "SENTRY"))
                buildingType = BUILDING_SENTRY;
            else if (StrEqual(type, "MINISENTRY"))
                buildingType = BUILDING_MINISENTRY;
            else if (StrEqual(type, "SENTRIES"))
                buildingType = BUILDING_SENTRIES;
            else if (StrEqual(type, "DISPENSER"))
                buildingType = BUILDING_DISPENSER;
            else if (StrEqual(type, "ENTRY"))
                buildingType = BUILDING_ENTRY;
            else if (StrEqual(type, "EXIT"))
                buildingType = BUILDING_EXIT;
            else if (StrEqual(type, "TELEPORTERS"))
                buildingType = BUILDING_TELEPORTERS;
            else
            {
                LogError("%T", "EW_ConfigBadType", LANG_SERVER, type);
                continue;
            }

            // Add to global list
            g_buildingTypeStrings.SetValue(name, buildingType);
        }
        while (kv.GotoNextKey(false));
    }
}

/**
 * Process additional boolean strings from the config file
 */
static void Config_ProcessBoolStrings(KeyValues &kv)
{
    if (kv.JumpToKey("BooleanPhrases") && kv.GotoFirstSubKey(false)) // false = traverse values
    {
        char string[MAX_BOOLSTRING_SIZE];
        bool value;
        do
        {
            // Check if there is a string to add
            if(!kv.GetSectionName(string, sizeof(string)))
                break;

            value = view_as<bool>(kv.GetNum(NULL_STRING, 0)); // Default to false

            // Validate string
            if (StrEqual(string, ""))
                continue;

            // Force name to uppercase for case-insensitive lookup
            for (int i = 0; string[i] != '\0'; i++)
                string[i] = CharToUpper(string[i]);

            // Add to global list
            g_boolStrings.SetValue(string, value);
        }
        while (kv.GotoNextKey(false));
    }
}

// Hello c:
