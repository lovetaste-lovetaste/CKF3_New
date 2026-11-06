// Korozbot action interface

#ifndef _HL_BOT_ACTION_INTERFACE_
#define _HL_BOT_ACTION_INTERFACE_

#include <tier1\UtlVector.h>

class CBot;

#include "hl_bot_fight_manager.h"

enum ActionResult
{
	DONE,
	CONTINUE,
	CHANGE_TO,
	SUSPEND_FOR,
};

class CTFAction
{
	// OVERRIDE THIS!!!
public:
	CTFAction() : m_Status(CONTINUE), debug("Continue"), new_action(NULL) {}
	virtual ~CTFAction() {}

	virtual void OnEnter(CBot* me) = 0;		// when getting changed/suspended into, OR when the initial action
	virtual void Update(CBot* me) = 0;		// CONTINUE status
	virtual void OnExit(CBot* me) = 0;		// CHANGE_TO or DONE status
	virtual void OnResume(CBot* me) = 0;	// action that was suspended into is now finished, so this action is now the active one
	virtual const char* GetName() const = 0;
	virtual bool ShouldHurry(CBot* me) = 0;
	virtual bool ShouldRetreat(CBot* me) = 0;

	// do not change or inherit these
	ActionResult Status() { return m_Status; }
	CTFAction* GetNewAction() { return new_action; };
	const char* GetNewActionName()
	{
		if (!new_action)
			return "NULL ACTION";

		return new_action->GetName();
	}
	void Done(const char* reason)							{ m_Status = DONE; debug = reason; new_action = NULL; }
	void Change_To(CTFAction* action, const char* reason)	{ m_Status = CHANGE_TO; debug = reason; new_action = action; }
	void Suspend_For(CTFAction* action, const char* reason)	{ m_Status = SUSPEND_FOR; debug = reason; new_action = action; }
	void Continue()											{ m_Status = CONTINUE; debug = "Continue"; new_action = NULL; };
private:
	// dont edit these yourself, otherwise Shit Might Get Weird..
	// use the helper functions above
	ActionResult m_Status;
	const char* debug;
	CTFAction* new_action;
};

enum ScenarioMoniterCheckResult
{
	RESULT_HEALTH				= (1 << 0),
	RESULT_RETREAT				= (1 << 1),
	RESULT_AMMO					= (1 << 2),
};

class CTFBotScenarioMoniterInterface
{
	// handles universal things that would normally stop a player dead in their tracks, regardless of what they are doing
	// eg get health, get ammo, run away, etc
public:
	CTFBotScenarioMoniterInterface()
	{
		m_hpAction = NULL;
		m_retreatAction = NULL;
		m_ammoAction = NULL;
		m_winAction = NULL;
		m_loseAction = NULL;
		m_bot = NULL;
	}

	void SetBot(CBot* me) { m_bot = me; }
	void SetHealthAction(CTFAction* action) { m_hpAction = action; }
	void SetRetreatAction(CTFAction* action) { m_retreatAction = action; }
	void SetAmmoAction(CTFAction* action) { m_ammoAction = action; }

	void SetWinLostActions(CTFAction* win_action, CTFAction* lose_action)
	{
		m_winAction = win_action;
		m_loseAction = lose_action;
	}

	int Check(CTFAction* action);
	void Update(int attrib = 0);
private:
	CBot* m_bot;
	CTFAction* m_hpAction;
	CTFAction* m_retreatAction;
	CTFAction* m_ammoAction;

	CTFAction* m_winAction;
	CTFAction* m_loseAction;
};

class CTFBotActionInterface
{
public:
	CTFBotActionInterface(CTFAction* m_baseAction);

	CBot* GetBot() const { return m_bot; }
	void SetBot(CBot* me) { m_bot = me; m_scenarioMoniterInterface.SetBot(me); }

	virtual void Update();
	
	bool ShouldWeFight() { return m_bFighting; };

	CUtlVector<CTFAction*> m_Actions;
	CTFAction* m_bAction;
	CTFBotScenarioMoniterInterface m_scenarioMoniterInterface;	// Scenario moniter interface
	CBotFightManager m_fightManager;
	void Reset(CTFAction* first = NULL);
	void Debug(bool status) { m_isDebug = status; }			///< turn debugging on/off
private:
	CBot* m_bot;

	bool m_bFighting;

	char debug_info[512];
	bool m_isDebug;
	float m_flLastDebugUpdate;
	void SendDebugInfo(const char* text);

	void DestroyAction(CTFAction* action);
};

static bool IsInside(const Vector& p, CBaseEntity* e)
{
	return p.x > e->pev->absmin.x && p.x < e->pev->absmax.x
		&& p.y > e->pev->absmin.y && p.y < e->pev->absmax.y
		&& p.z > e->pev->absmin.z && p.z < e->pev->absmax.z;
}
// this was for control points but i might as well just make it a universal thing

inline int FNullEnt(CBaseEntity* ent) { return ((!ent) || FNullEnt(ent->edict())); }


#endif