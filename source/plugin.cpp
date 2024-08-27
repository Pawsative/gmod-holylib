#include <GarrysMod/FactoryLoader.hpp>
#include "filesystem.h"
#include "plugin.h"
#include "lua.h"
#include <tier1.h>
#include <tier2/tier2.h>
#include <GarrysMod/Symbols.hpp>
#include <GarrysMod/Lua/LuaShared.h>
#include "module.h"
#include "player.h"
#include "tier0/icommandline.h"

#define DEDICATED
#include "vstdlib/jobthread.h"

// The plugin is a static singleton that is exported as an interface
CServerPlugin g_HolyLibServerPlugin;
#ifdef LIB_HOLYLIB
IServerPluginCallbacks* GetHolyLibPlugin()
{
	return &g_HolyLibServerPlugin;
}
#else
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_HolyLibServerPlugin);
#endif


//---------------------------------------------------------------------------------
// Purpose: constructor/destructor
//---------------------------------------------------------------------------------
CServerPlugin::CServerPlugin()
{
	m_iClientCommandIndex = 0;
}

CServerPlugin::~CServerPlugin()
{
}

DLL_EXPORT void HolyLib_PreLoad() // ToDo: Make this a CServerPlugin member later!
{
	if (!Util::ShouldLoad())
	{
		Msg("HolyLib already exists? Stopping.\n");
		return;
	}

	g_pModuleManager.SetGhostInj();
	g_pModuleManager.InitDetour(true);
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is loaded, load the interface we need from the engine
//---------------------------------------------------------------------------------
CGlobalVars *gpGlobals = NULL;
bool CServerPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	Msg("--- HolyLib Plugin loading ---\n");

	if (!Util::ShouldLoad())
	{
		Msg("HolyLib already exists? Stopping.\n");
		Msg("--- HolyLib Plugin finished loading ---\n");
		return false; // What if we return false?
	}
	 
	ConnectTier1Libraries(&interfaceFactory, 1);
	ConnectTier2Libraries(&interfaceFactory, 1);
#ifndef ARCHITECTURE_X86_64
	ConnectTier3Libraries(&interfaceFactory, 1);
#endif

	engine = (IVEngineServer*)interfaceFactory(INTERFACEVERSION_VENGINESERVER, NULL);

	IPlayerInfoManager* playerinfomanager = (IPlayerInfoManager*)gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER, NULL);
	Detour::CheckValue("get interface", "playerinfomanager", playerinfomanager != NULL);

	if ( playerinfomanager )
		gpGlobals = playerinfomanager->GetGlobalVars();

	g_pModuleManager.Setup(interfaceFactory, gameServerFactory); // Setup so that Util won't cause issues
	Lua::AddDetour();
	Util::AddDetour();
	g_pModuleManager.Init();
	g_pModuleManager.InitDetour(false);

	if (CommandLine()->FindParm("-holylib_debug_forceregister"))
	{
		ConVar_Register(); // ConVars currently cause a crash on level shutdown. I probably need to find some hidden vtable function AGAIN.
	}
	/*
	 * Debug info about the crash from what I could find(could be wrong):
	 * - Where: engine.so
	 * - Offset: 0x106316
	 * - Manifest: 8648260017074424678
	 * - Which function: CUtlLinkedList<T,S,ML,I,M>::AllocInternal( bool multilist )
	 * - Which line (Verify): typename M::Iterator_t it = m_Memory.IsValidIterator( m_LastAlloc ) ? m_Memory.Next( m_LastAlloc ) : m_Memory.First();
	 * - Why: I have no Idea. Maybe the class is different in our sdk?
	 */

	Msg("--- HolyLib Plugin finished loading ---\n");

	return true;
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unloaded (turned off)
//---------------------------------------------------------------------------------
void CServerPlugin::Unload(void)
{
	g_pModuleManager.Shutdown();
	Detour::Remove(0);
	Detour::ReportLeak();

	if (CommandLine()->FindParm("-holylib_debug_forceregister"))
	{
		ConVar_Unregister();
	}

	DisconnectTier1Libraries();
	DisconnectTier2Libraries();
#ifndef ARCHITECTURE_X86_64
	DisconnectTier3Libraries();
#endif
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is paused (i.e should stop running but isn't unloaded)
//---------------------------------------------------------------------------------
void CServerPlugin::Pause(void)
{
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unpaused (i.e should start executing again)
//---------------------------------------------------------------------------------
void CServerPlugin::UnPause(void)
{
}

//---------------------------------------------------------------------------------
// Purpose: the name of this plugin, returned in "plugin_print" command
//---------------------------------------------------------------------------------
const char* CServerPlugin::GetPluginDescription(void)
{
	std::string pName = "HolyLib Serverplugin V0.4";

#if GITHUB_RUN_DATA == 0 // DATA should always fallback to 0. We will set it to 1 in releases.
	pName.append("DEV (Run: ");
	pName.append(std::to_string(GITHUB_RUN_NUMBER));
	pName.append(")");
#endif

#pragma warning( disable : 26816 ) // Should probably use "once" instead of "disable"
	return pName.c_str(); // A warning but I don't care about it :^
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CServerPlugin::LevelInit(char const *pMapName)
{

}

//---------------------------------------------------------------------------------
// Purpose: called after LUA Initialized.
//---------------------------------------------------------------------------------
bool CServerPlugin::LuaInit()
{
	GarrysMod::Lua::ILuaInterface* LUA = Lua::GetRealm(GarrysMod::Lua::State::SERVER);
	if (LUA == nullptr) {
		Msg("Failed to get ILuaInterface! (Realm: Server)\n");
		return false;
	}

	Lua::Init(LUA);

	return true;
}

//---------------------------------------------------------------------------------
// Purpose: called just before LUA is shutdown.
//---------------------------------------------------------------------------------
void CServerPlugin::LuaShutdown()
{
	Lua::Shutdown();
}

//---------------------------------------------------------------------------------
// Purpose: called on level start, when the server is ready to accept client connections
//		edictCount is the number of entities in the level, clientMax is the max client count
//---------------------------------------------------------------------------------
void CServerPlugin::ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{
	if (!g_Lua) {
		LuaInit();
	}

	Lua::ServerInit();
}

//---------------------------------------------------------------------------------
// Purpose: called once per server frame, do recurring work here (like checking for timeouts)
//---------------------------------------------------------------------------------
void CServerPlugin::GameFrame(bool simulating)
{
	g_pModuleManager.Think(simulating);
}

//---------------------------------------------------------------------------------
// Purpose: called on level end (as the server is shutting down or going to a new map)
//---------------------------------------------------------------------------------
void CServerPlugin::LevelShutdown(void) // !!!!this can get called multiple times per map change
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client spawns into a server (i.e as they begin to play)
//---------------------------------------------------------------------------------
void CServerPlugin::ClientActive(edict_t *pEntity)
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client leaves a server (or is timed out)
//---------------------------------------------------------------------------------
void CServerPlugin::ClientDisconnect(edict_t *pEntity)
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client spawns?
//---------------------------------------------------------------------------------
void CServerPlugin::ClientPutInServer(edict_t *pEntity, char const *playername)
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CServerPlugin::SetCommandClient(int index)
{
	m_iClientCommandIndex = index;
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CServerPlugin::ClientSettingsChanged(edict_t *pEdict)
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client joins a server
//---------------------------------------------------------------------------------
PLUGIN_RESULT CServerPlugin::ClientConnect(bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen)
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a client types in a command (only a subset of commands however, not CON_COMMAND's)
//---------------------------------------------------------------------------------
PLUGIN_RESULT CServerPlugin::ClientCommand(edict_t *pEntity, const CCommand &args)
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a client is authenticated
//---------------------------------------------------------------------------------
PLUGIN_RESULT CServerPlugin::NetworkIDValidated(const char *pszUserName, const char *pszNetworkID)
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a cvar value query is finished
//---------------------------------------------------------------------------------
void CServerPlugin::OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue)
{
}

void CServerPlugin::OnEdictAllocated(edict_t *edict)
{
}

void CServerPlugin::OnEdictFreed(const edict_t *edict)
{
}