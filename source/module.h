#include "interface.h"
#include <vector>
#include "detours.h"

enum Module_Compatibility
{
	LINUX32 = 1,
	LINUX64,
	WINDOWS32,
	WINDOWS64,
};

class ConVar;
class KeyValues;
class IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) {};
	virtual void LuaInit(bool bServerInit) {};
	virtual void LuaShutdown() {};
	virtual void InitDetour(bool bPreServer) {}; // bPreServer = Called before the Dedicated Server was started
	virtual void Think(bool bSimulating) {};
	virtual void Shutdown() { Detour::Remove(m_pID); };
	virtual const char* Name() = 0;
	virtual int Compatibility() = 0; // Idk give it a better name later.
	virtual bool IsEnabledByDefault() { return true; };

public:
	unsigned int m_pID = 0; // Set by the CModuleManager!
};

class CModule
{
public:
	~CModule();
	void SetModule(IModule* module);
	void SetEnabled(bool bEnabled, bool bForced = false);
	inline IModule* GetModule() { return m_pModule; };
	inline bool IsEnabled() { return m_bEnabled; };
	inline ConVar* GetConVar() { return m_pCVar; };
	inline bool IsCompatible() { return m_bCompatible; };

protected:
	IModule* m_pModule = NULL;
	ConVar* m_pCVar = NULL;
	char* m_pCVarName = NULL;
	bool m_bEnabled = false;
	bool m_bCompatible = false;
	bool m_bStartup = false;
};

#define LoadStatus_PreDetourInit (1<<1)
#define LoadStatus_Init (1<<2)
#define LoadStatus_DetourInit (1<<3)
#define LoadStatus_LuaInit (1<<4)
#define LoadStatus_LuaServerInit (1<<5)

class CModuleManager
{
public:
	CModuleManager();
	void RegisterModule(IModule* mdl);
	CModule* FindModuleByConVar(ConVar* convar);
	CModule* FindModuleByName(const char* name);
	inline int GetStatus() { return m_pStatus; };
	inline CreateInterfaceFn& GetAppFactory() { return m_pAppFactory; };
	inline CreateInterfaceFn& GetGameFactory() { return m_pGameFactory; };
	inline void SetGhostInj() { m_bGhostInj = true; };
	inline bool IsUsingGhostInj() { return m_bGhostInj; };

	void Setup(CreateInterfaceFn appfn, CreateInterfaceFn gamefn);
	void Init();
	void LuaInit(bool bServerInit);
	void LuaShutdown();
	void InitDetour(bool bPreServer);
	void Think(bool bSimulating);
	void Shutdown();

private:
	std::vector<CModule*> m_pModules;
	int m_pStatus = 0;
	CreateInterfaceFn m_pAppFactory = NULL;
	CreateInterfaceFn m_pGameFactory = NULL;
	bool m_bGhostInj = false;
};
extern CModuleManager g_pModuleManager;