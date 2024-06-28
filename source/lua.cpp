#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/FactoryLoader.hpp>
#include "lua.h"
#include <GarrysMod/InterfacePointers.hpp>
#include "filesystem.h"
#include "module.h"

bool Lua::PushHook(const char* hook)
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->GetField(-1, "hook");
		if (g_Lua->GetType(-1) != GarrysMod::Lua::Type::Table)
		{
			g_Lua->Pop(2);
			DevMsg("Missing hook table!\n");
			return false;
		}

			g_Lua->GetField(-1, "Run");
			if (g_Lua->GetType(-1) != GarrysMod::Lua::Type::Function)
			{
				g_Lua->Pop(3);
				DevMsg("Missing hook.Run function!\n");
				return false;
			} else {
				int reference = g_Lua->ReferenceCreate();
				g_Lua->Pop(2);
				g_Lua->ReferencePush(reference);
				g_Lua->ReferenceFree(reference);
				g_Lua->PushString(hook);
			}

	return true;
}

void Lua::Init(GarrysMod::Lua::ILuaInterface* LUA)
{
	g_Lua = LUA;

	g_pModuleManager.LuaInit(false);
}

void Lua::ServerInit()
{
	if (g_Lua == nullptr) {
		DevMsg(1, "Lua::ServerInit failed! g_Lua is NULL\n");
		return;
	}

	g_pModuleManager.LuaInit(true);
}

void Lua::Shutdown()
{
	g_pModuleManager.LuaShutdown();
}

Detouring::Hook detour_InitLuaClasses;
void hook_InitLuaClasses(GarrysMod::Lua::ILuaInterface* LUA)
{
	detour_InitLuaClasses.GetTrampoline<Symbols::InitLuaClasses>()(LUA);

	Lua::Init(LUA);
}

void Lua::AddDetour() // Our Lua Loader.
{
	SourceSDK::ModuleLoader server_loader("server_srv");
	Detour::Create(
		&detour_InitLuaClasses, "InitLuaClasses",
		server_loader.GetModule(), Symbols::InitLuaClassesSym,
		(void*)hook_InitLuaClasses, 0
	);
}

GarrysMod::Lua::ILuaInterface* Lua::GetRealm(unsigned char realm) {
	SourceSDK::FactoryLoader luashared_loader("lua_shared_srv");
	GarrysMod::Lua::ILuaShared* LuaShared = (GarrysMod::Lua::ILuaShared*)luashared_loader.GetFactory()(GMOD_LUASHARED_INTERFACE, nullptr);
	if (LuaShared == nullptr) {
		Msg("failed to get ILuaShared!\n");
		return nullptr;
	}

	return LuaShared->GetLuaInterface(realm);
}

GarrysMod::Lua::ILuaShared* Lua::GetShared() {
	SourceSDK::FactoryLoader luashared_loader("lua_shared_srv");
	if ( !luashared_loader.GetFactory() )
		Msg("About to crash!\n");

	return luashared_loader.GetInterface<GarrysMod::Lua::ILuaShared>(GMOD_LUASHARED_INTERFACE);
}