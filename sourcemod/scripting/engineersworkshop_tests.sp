/**
 * =============================================================================
 * Engineer's Workshop - Tests
 * Tests for the shared Engineer's Workshop Include
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
#include <profiler>

#pragma newdecls required // After 3rd-party includes
#include <engineersworkshop>


//==================================================================
// Constants
#define PLUGIN_VERSION "1.0.0"
#define PLUGIN_URL "https://sirdigbot.github.io/engineers-workshop/"

public Plugin myinfo = 
{
    name = "[TF2] Engineer's Workshop - Tests",
    author = "SirDigbot",
    description = "Tests for the shared Engineer's Workshop Include",
    version = PLUGIN_VERSION,
    url = PLUGIN_URL
};


//==================================================================
// Globals


//==================================================================
// Forwards
public void OnPluginStart()
{
    LoadTranslations("common.phrases");
    LoadTranslations("engineersworkshop.phrases");

    // Create commands
    RegAdminCmd("sm_ewtest", Command_RunTests, ADMFLAG_ROOT, "Run the Engineer's Workshop tests.");
    RegAdminCmd("sm_ewtests", Command_RunTests, ADMFLAG_ROOT, "Run the Engineer's Workshop tests.");
}


//==================================================================
// Commands

/**
 * sm_ewtest
 */
public Action Command_RunTests(int client, int args)
{
    // To call these tests is being a bit charitable,
    // it's really just so I can see that the functions
    // are outputting mostly expected values, and measure their performance.
    if (!client)
        return EW_RejectCommand(client, "Must be run ingame.");

    // This is not an exhaustive list of tests, but it does include most of the
    // stuff that is either really important to test or most prone to errors.
    // Profiling is only used where it has meaningful measurements.
    Profiler p = new Profiler();
    Tests_BuildingCount(client, p);
    Tests_GetBuildings(client, p);
    Tests_GetTargetBuildings(p);
    Tests_GetAllBuildings(p);
    Tests_StringToType(p);
    Tests_IsSingleType();
    Tests_ProcessTarget();
    Tests_GetClientAimOrigin(client, p);
    Tests_InRespawnRoom(client, p);
    delete p;
    return Plugin_Handled;
}


//==================================================================
// Testing Stocks
stock void Separator()
{
    PrintToServer("========================================");
}


void TestResult(const char[] desc, bool result)
{
    if (result)
        PrintToServer("PASSED %s", desc);
    else
        PrintToServer("***FAILED %s***", desc);
}


/**
 * Log array and arraylist of building indexes
 */
void PrintBuildings(char[] desc, int[] buildings, int size, ArrayList list)
{
    PrintToServer(desc);

    static char arrayString[192];
    static char listString[192];
    static char buffer[32];

    // Clear static variables before using
    arrayString[0] = '\0';
    listString[0] = '\0';
    buffer[0] = '\0';

    for (int i = 0; i < size; i++)
    {
        // Create comma-separated string of indexes
        // from both array and arraylist (no comma for last item)
        Format(buffer, sizeof(buffer), "%i%s", buildings[i], (i < size - 1) ? ", " : "");
        StrCat(arrayString, sizeof(arrayString), buffer);

        // Only use values from arraylist if it is valid handle
        int val = 0;
        if (list != null && list.Length > i) // Don't assume same size lists
            val = list.Get(i);
        Format(buffer, sizeof(buffer), "%i%s", val, (i < size - 1) ? ", " : "");
        StrCat(listString, sizeof(listString), buffer);
    }

    PrintToServer("A[%s] -- AL[%s]", arrayString, (list == null) ? "null" : listString);
}


//==================================================================
// Test 'cases' if you want to be charitable

/**
 * Test all of the building counting functions
 */
void Tests_BuildingCount(int client, Profiler p)
{
    // EW_GetBuildingCount
    p.Start();
    int count = EW_GetBuildingCount(client);
    p.Stop();
    PrintToServer("EW_GetBuildingCount ALL: %i", count);
    PrintToServer("EW_GetBuildingCount SENTRY: %i", EW_GetBuildingCount(client, BUILDING_SENTRY));
    PrintToServer("EW_GetBuildingCount MINISENTRY: %i", EW_GetBuildingCount(client, BUILDING_MINISENTRY));
    PrintToServer("EW_GetBuildingCount DISPENSER: %i", EW_GetBuildingCount(client, BUILDING_DISPENSER));
    PrintToServer("EW_GetBuildingCount ENTRY: %i", EW_GetBuildingCount(client, BUILDING_ENTRY));
    PrintToServer("EW_GetBuildingCount EXIT: %i", EW_GetBuildingCount(client, BUILDING_EXIT));
    PrintToServer("EW_GetBuildingCount Performance: %f", p.Time);

    // EW_GetTargetBuildingCount
    p.Start();
    count = EW_GetTargetBuildingCount("@all", client);
    p.Stop();
    PrintToServer("EW_GetTargetBuildingCount @all, ALL: %i", count);
    PrintToServer("EW_GetTargetBuildingCount Performance: %f", p.Time);

    // EW_GetAllBuildingCount
    p.Start();
    count = EW_GetAllBuildingCount();
    p.Stop();
    PrintToServer("EW_GetAllBuildingCount ALL: %i", count);
    PrintToServer("EW_GetAllBuildingCount Performance: %f", p.Time);

    Separator();
}


/**
 * Test both of the GetBuildings functions
 */
void Tests_GetBuildings(int client, Profiler p)
{
    int buildings[32];
    ArrayList list = null;
    int count;

    // EW_GetBuildings and EW_GetBuildingsList
    count = EW_GetBuildings(client, BUILDING_ALL, buildings, sizeof(buildings));
    list = EW_GetBuildingsList(client, BUILDING_ALL);
    PrintBuildings("EW_GetBuildings ALL", buildings, count, list);
    delete list;

    count = EW_GetBuildings(client, BUILDING_SENTRY, buildings, sizeof(buildings));
    list = EW_GetBuildingsList(client, BUILDING_SENTRY);
    PrintBuildings("EW_GetBuildings SENTRY", buildings, count, list);
    delete list;

    count = EW_GetBuildings(client, BUILDING_MINISENTRY, buildings, sizeof(buildings));
    list = EW_GetBuildingsList(client, BUILDING_MINISENTRY);
    PrintBuildings("EW_GetBuildings MINISENTRY", buildings, count, list);
    delete list;

    count = EW_GetBuildings(client, BUILDING_DISPENSER, buildings, sizeof(buildings));
    list = EW_GetBuildingsList(client, BUILDING_DISPENSER);
    PrintBuildings("EW_GetBuildings DISPENSER", buildings, count, list);
    delete list;

    count = EW_GetBuildings(client, BUILDING_ENTRY, buildings, sizeof(buildings));
    list = EW_GetBuildingsList(client, BUILDING_ENTRY);
    PrintBuildings("EW_GetBuildings ENTRY", buildings, count, list);
    delete list;

    count = EW_GetBuildings(client, BUILDING_EXIT, buildings, sizeof(buildings));
    list = EW_GetBuildingsList(client, BUILDING_EXIT);
    PrintBuildings("EW_GetBuildings EXIT", buildings, count, list);
    delete list;

    p.Start();
    count = EW_GetBuildings(client, BUILDING_ALL, buildings, sizeof(buildings));
    p.Stop();
    float t1 = p.Time;
    p.Start();
    list = EW_GetBuildingsList(client, BUILDING_ALL);
    p.Stop();
    float t2 = p.Time;
    PrintToServer("EW_GetBuildings Performance: A:%f, AL:%f", t1, t2);
    delete list;

    Separator();
}


/**
 * Test both of the GetTargetBuildings functions
 */
void Tests_GetTargetBuildings(Profiler p)
{
    int buildings[32];
    ArrayList list = null;
    int count;

    // EW_GetTargetBuildings and EW_GetTargetBuildingsList
    count = EW_GetTargetBuildings("@all", 0, BUILDING_ALL, 0, buildings, sizeof(buildings));
    list = EW_GetTargetBuildingsList("@all", 0, BUILDING_ALL, 0);
    PrintBuildings("EW_GetTargetBuildings ALL", buildings, count, list);
    delete list;

    count = EW_GetTargetBuildings("@all", 0, BUILDING_SENTRY, 0, buildings, sizeof(buildings));
    list = EW_GetTargetBuildingsList("@all", 0, BUILDING_SENTRY, 0);
    PrintBuildings("EW_GetTargetBuildings SENTRY", buildings, count, list);
    delete list;

    count = EW_GetTargetBuildings("@all", 0, BUILDING_MINISENTRY, 0, buildings, sizeof(buildings));
    list = EW_GetTargetBuildingsList("@all", 0, BUILDING_MINISENTRY, 0);
    PrintBuildings("EW_GetTargetBuildings MINISENTRY", buildings, count, list);
    delete list;

    count = EW_GetTargetBuildings("@all", 0, BUILDING_DISPENSER, 0, buildings, sizeof(buildings));
    list = EW_GetTargetBuildingsList("@all", 0, BUILDING_DISPENSER, 0);
    PrintBuildings("EW_GetTargetBuildings DISPENSER", buildings, count, list);
    delete list;

    count = EW_GetTargetBuildings("@all", 0, BUILDING_ENTRY, 0, buildings, sizeof(buildings));
    list = EW_GetTargetBuildingsList("@all", 0, BUILDING_ENTRY, 0);
    PrintBuildings("EW_GetTargetBuildings ENTRY", buildings, count, list);
    delete list;

    count = EW_GetTargetBuildings("@all", 0, BUILDING_EXIT, 0, buildings, sizeof(buildings));
    list = EW_GetTargetBuildingsList("@all", 0, BUILDING_EXIT, 0);
    PrintBuildings("EW_GetTargetBuildings EXIT", buildings, count, list);
    delete list;

    p.Start();
    count = EW_GetTargetBuildings("@all", 0, BUILDING_ALL, 0, buildings, sizeof(buildings));
    p.Stop();
    float t1 = p.Time;
    p.Start();
    list = EW_GetTargetBuildingsList("@all", 0, BUILDING_ALL, 0);
    p.Stop();
    float t2 = p.Time;
    PrintToServer("EW_GetTargetBuildings Performance: A:%f, AL:%f", t1, t2);
    delete list;

    Separator();
}


/**
 * Test both of the GetAllBuildings functions
 */
void Tests_GetAllBuildings(Profiler p)
{
    int buildings[32];
    ArrayList list = null;
    int count;

    // EW_GetAllBuildings and EW_GetAllBuildingsList
    count = EW_GetAllBuildings(BUILDING_ALL, buildings, sizeof(buildings));
    list = EW_GetAllBuildingsList(BUILDING_ALL);
    PrintBuildings("EW_GetAllBuildings ALL", buildings, count, list);
    delete list;

    count = EW_GetAllBuildings(BUILDING_SENTRY, buildings, sizeof(buildings));
    list = EW_GetAllBuildingsList(BUILDING_SENTRY);
    PrintBuildings("EW_GetAllBuildings SENTRY", buildings, count, list);
    delete list;

    count = EW_GetAllBuildings(BUILDING_MINISENTRY, buildings, sizeof(buildings));
    list = EW_GetAllBuildingsList(BUILDING_MINISENTRY);
    PrintBuildings("EW_GetAllBuildings MINISENTRY", buildings, count, list);
    delete list;

    count = EW_GetAllBuildings(BUILDING_DISPENSER, buildings, sizeof(buildings));
    list = EW_GetAllBuildingsList(BUILDING_DISPENSER);
    PrintBuildings("EW_GetAllBuildings DISPENSER", buildings, count, list);
    delete list;

    count = EW_GetAllBuildings(BUILDING_ENTRY, buildings, sizeof(buildings));
    list = EW_GetAllBuildingsList(BUILDING_ENTRY);
    PrintBuildings("EW_GetAllBuildings ENTRY", buildings, count, list);
    delete list;

    count = EW_GetAllBuildings(BUILDING_EXIT, buildings, sizeof(buildings));
    list = EW_GetAllBuildingsList(BUILDING_EXIT);
    PrintBuildings("EW_GetAllBuildings EXIT", buildings, count, list);
    delete list;

    p.Start();
    count = EW_GetAllBuildings(BUILDING_ALL, buildings, sizeof(buildings));
    p.Stop();
    float t1 = p.Time;
    p.Start();
    list = EW_GetAllBuildingsList(BUILDING_ALL);
    p.Stop();
    float t2 = p.Time;
    PrintToServer("EW_GetAllBuildings Performance: A:%f, AL:%f", t1, t2);
    delete list;

    Separator();
}

/**
 * Tests for StringTo type convertors functions
 */
void Tests_StringToType(Profiler p)
{
    bool error;
    float time;

    // EW_StringToBuildingType
    TestResult("EW_StringToBuildingType: Empty", EW_StringToBuildingType("") == 0);
    TestResult("EW_StringToBuildingType Default ALL", EW_StringToBuildingType("ALL") == BUILDING_ALL);
    TestResult("EW_StringToBuildingType Default SENTRY", EW_StringToBuildingType("SENTRY") == BUILDING_SENTRY);
    TestResult("EW_StringToBuildingType Default MINISENTRY", EW_StringToBuildingType("MINISENTRY") == BUILDING_MINISENTRY);
    TestResult("EW_StringToBuildingType Default SENTRIES", EW_StringToBuildingType("SENTRIES") == BUILDING_SENTRIES);
    TestResult("EW_StringToBuildingType Default DISPENSER", EW_StringToBuildingType("DISPENSER") == BUILDING_DISPENSER);
    TestResult("EW_StringToBuildingType Default ENTRY", EW_StringToBuildingType("ENTRY") == BUILDING_ENTRY);
    TestResult("EW_StringToBuildingType Default EXIT", EW_StringToBuildingType("EXIT") == BUILDING_EXIT);
    TestResult("EW_StringToBuildingType Default TELEPORTERS", EW_StringToBuildingType("TELEPORTERS") == BUILDING_TELEPORTERS);
    TestResult("EW_StringToBuildingType Case-insensitive", EW_StringToBuildingType("mInIsenTRy") == BUILDING_MINISENTRY);
    TestResult("EW_StringToBuildingType Custom AllBuildings", EW_StringToBuildingType("allbuildings") == BUILDING_ALL);
    TestResult("EW_StringToBuildingType Custom Sentrygun", EW_StringToBuildingType("sENtRYGun") == BUILDING_SENTRY);
    p.Start();
    EW_StringToBuildingType("SENTRY");
    p.Stop();
    time = p.Time;
    PrintToServer("EW_StringToBuildingType Performance: %f", time);
    

    // EW_StringToBool
    TestResult("EW_StringToBool Empty", EW_StringToBool("", error) == false && error == true);
    TestResult("EW_StringToBool Invalid", EW_StringToBool("NotABool", error) == false && error == true);
    TestResult("EW_StringToBool Default 0", EW_StringToBool("0", error) == false && error == false);
    TestResult("EW_StringToBool Default 1", EW_StringToBool("1", error) == true && error == false);
    TestResult("EW_StringToBool Custom Enable", EW_StringToBool("Enable", error) == true && error == false);
    TestResult("EW_StringToBool Custom Disable", EW_StringToBool("Disable", error) == false && error == false);
    TestResult("EW_StringToBool Case-insensitive", EW_StringToBool("eNaBLe", error) == true && error == false);
    p.Start();
    EW_StringToBool("0");
    p.Stop();
    time = p.Time;
    PrintToServer("EW_StringToBool Performance: %f", time);

    Separator();
}


/**
 * Tests for EW_IsSingleType
 */
void Tests_IsSingleType()
{
    // EW_IsSingleType
    TestResult("EW_IsSingleType Zero", EW_IsSingleType(0) == false);
    TestResult("EW_IsSingleType One", EW_IsSingleType(1) == true);
    TestResult("EW_IsSingleType Non-one", EW_IsSingleType(16) == true);
    TestResult("EW_IsSingleType Two flags", EW_IsSingleType(1 | 2) == false);
    TestResult("EW_IsSingleType Multi flags", EW_IsSingleType(1 | 4 | 16) == false);

    Separator();
}


/**
 * Tests for EW_ProcessTargets
 */
void Tests_ProcessTarget()
{
    // EW_ProcessTargets
    TargetData data;
    EW_ProcessTargets("@all", 0, 0, data);

    // If count and name members work so do the others since they're
    // set the same way
    PrintToServer("EW_ProcessTargets @all: %i, %s", data.count, data.name);

    Separator();
}


/**
 * Tests for EW_GetClientAimOrigin
 */
void Tests_GetClientAimOrigin(int client, Profiler p)
{
    // EW_GetClientAimOrigin
    float v[3];
    p.Start();
    bool result = EW_GetClientAimOrigin(client, v);
    p.Stop();
    PrintToServer("EW_GetClientAimOrigin Inf: ret:%i, origin %f, %f, %f", result, v[0], v[1], v[2]);
    v[0] = 0.0;
    v[1] = 0.0;
    v[2] = 0.0;

    result = EW_GetClientAimOrigin(client, v, 400.0);
    PrintToServer("EW_GetClientAimOrigin maxDist 400.0: ret:%i, origin %f, %f, %f", result, v[0], v[1], v[2]);  
    PrintToServer("EW_GetClientAimOrigin Performamce: %f", p.Time);

    Separator();
}


/**
 * Tests for EW_IsPointInRespawnRoom and EW_IsEntityInRespawnRoom
 */
void Tests_InRespawnRoom(int client, Profiler p)
{
    // EW_IsPointInRespawnRoom
    float v[3];
    GetClientAbsOrigin(client, v);
    p.Start();
    bool result = EW_IsPointInRespawnRoom(v);
    p.Stop();
    PrintToServer("EW_IsPointInRespawnRoom Position, (always same team): %i", result);
    PrintToServer("EW_IsPointInRespawnRoom Client, any team (pos valid forces same team): %i", EW_IsPointInRespawnRoom(v, client, false));
    PrintToServer("EW_IsPointInRespawnRoom Client, same team (pos valid forces same team): %i", EW_IsPointInRespawnRoom(v, client, true));
    PrintToServer("EW_IsPointInRespawnRoom Performance: %f", p.Time);

    // EW_IsEntityInRespawnRoom
    p.Start();
    result = EW_IsEntityInRespawnRoom(client, false);
    p.Stop();
    PrintToServer("Client, any team: %i", result);
    PrintToServer("Client, same team: %i", EW_IsEntityInRespawnRoom(client, true));
    PrintToServer("EW_IsEntityInRespawnRoom Performance: %f", p.Time);

    Separator();
}