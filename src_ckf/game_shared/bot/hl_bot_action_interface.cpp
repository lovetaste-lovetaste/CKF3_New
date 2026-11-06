// Korozbot action interface

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "client.h"		// ClientConnect()/ClientPutInServer()

#include "hl_bot.h"
#include "hl_bot_action_interface.h"

CTFBotActionInterface::CTFBotActionInterface(CTFAction* m_baseAction)
	: m_scenarioMoniterInterface(), m_fightManager()
{
	m_bAction = m_baseAction;
	m_bot = NULL;
	debug_info[0] = '\0';
	m_isDebug = false;
	m_flLastDebugUpdate = 0.0f;
	m_scenarioMoniterInterface.SetBot(NULL);
	m_fightManager.SetBot(NULL);
	m_bFighting = false;
}

void CTFBotActionInterface::Update(void)
{
	CBot* bot = GetBot();
	if (!bot || !m_bAction)
		return;

	m_bFighting = false;

	debug_info[0] = '\0';
	bool resumed = false;

	for (int guard = 0; guard < 16; ++guard)
	{
		if (m_Actions.Count() <= 0)
		{
			// if there is no valid action, then use the base action
			// populate m_Actions so this doesnt happen again
			m_bAction->Continue();
			m_Actions.AddToTail(m_bAction);
			m_bAction->OnEnter(bot);
			resumed = false;
		}

		int top = m_Actions.Count() - 1;
		CTFAction* action = m_Actions.Element(top);
		if (!action)
		{
			m_Actions.Remove(top);
			continue;
		}

		sprintf(debug_info, "%s ::", action->GetName());

		if (resumed)
		{
			sprintf(debug_info + strlen(debug_info), " %s", " RESUMED");
			action->Continue();
			action->OnResume(bot);
			resumed = false;
		}

		CTFAction* next_action = action->GetNewAction();
		switch (action->Status())
		{
		case DONE:
			// finished
			// only reason this is not with the !action if check above is bcuz of the debug text
			
			sprintf(debug_info + strlen(debug_info), " %s", " DONE");
			action->OnExit(bot);
			m_Actions.Remove(top);
			DestroyAction(action);
			resumed = true;
			continue;
		case CHANGE_TO:
			// remove the old action entirely, action effectively gets REPLACED
			
			action->OnExit(bot);
			m_Actions.Remove(top);
			DestroyAction(action);
			if (next_action)
			{
				sprintf(debug_info + strlen(debug_info), " CHANGING INTO %s", next_action->GetName());
				m_Actions.AddToTail(next_action);
				next_action->OnEnter(bot);
			}
			else
			{
				sprintf(debug_info + strlen(debug_info), " %s", "  BUGGED CHANGE_TO, REMOVING");
				resumed = true;
			}
			continue;

		case SUSPEND_FOR:
			// action LOWERS IN PRIORITY, but resumes when suspended action is done
			// aka new element is added
			if (next_action)
			{
				sprintf(debug_info + strlen(debug_info), " SUSPENDING FOR %s", next_action->GetName());
				m_Actions.AddToTail(next_action);
				next_action->OnEnter(bot);
			}
			else
			{
				sprintf(debug_info + strlen(debug_info), " %s", "  BUGGED SUSPEND_FOR, REMOVING");
				action->OnExit(bot);
				m_Actions.Remove(top);
				DestroyAction(action);
				resumed = true;
			}
			continue;

		case CONTINUE:
		default:
			// since theres nothing to do, then just continue with the update below
			break;
		}

		int check = m_scenarioMoniterInterface.Check(action);
		bool hurry = action->ShouldHurry(bot);

		if (g_pGameRules->m_iRoundStatus == ROUND_END)
			m_scenarioMoniterInterface.Update(0);
		else if (!hurry)
		{
			bool done = false;
			if (check)
			{
				m_scenarioMoniterInterface.Update(check);
				done = true;
				if (m_isDebug)
					CONSOLE_ECHO("Doing scenario \n");
			}
			else if(bot->HasEnemy())
			{
				// the fight manager is handled in the bot's upkeep() func
				m_bFighting = true;
				done = true;
				if (m_isDebug)
					CONSOLE_ECHO("Fighting enemy \n");
			}

			if (done)
				return;
		}

		action->Update(bot);
		if (m_isDebug)
		{
			// SendDebugInfo(action->GetName());
			// this doesnt wanna work right so just ignore
			// this WOULD make a particle with the name of the action, like how nextbots show the debug text
			// i could not figure it out so whatever
			CONSOLE_ECHO("%s || %s \n", action->GetName(), debug_info && debug_info[0] ? debug_info : "");
		}

		return;		// only one action runs per tick
	}
}

void CTFBotActionInterface::SendDebugInfo(const char* text)
{
	if (m_flLastDebugUpdate > gpGlobals->time)
		return;

	m_flLastDebugUpdate = gpGlobals->time + 0.1;

	if (!m_bot)
		return;

	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBasePlayer* player = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(i));

		if (!IsEntityValid(player))
			continue;

		if (!player->IsPlayer())
			continue;

		if (player->IsBot())
			continue;

		player->SendDebugText(m_bot->pev, text);

		return;
	}
}

void CTFBotActionInterface::DestroyAction(CTFAction* action)
{
	if (action != m_bAction)	// the base action is reused, never freed
		delete action;
}

void CTFBotActionInterface::Reset(CTFAction* first)
{
	CBot* bot = GetBot();
	m_flLastDebugUpdate = 0.0f;

	for (int i = m_Actions.Count() - 1; i >= 0; --i)
	{
		CTFAction* action = m_Actions.Element(i);
		if (!action)
			continue;
		action->OnExit(bot);
		DestroyAction(action);
	}
	m_Actions.RemoveAll();

	// idle sits at the bottom so there's always something to fall back to
	m_bAction->Continue();
	m_Actions.AddToTail(m_bAction);
	m_bAction->OnEnter(bot);

	if (first)
	{
		m_Actions.AddToTail(first);
		first->OnEnter(bot);
	}
}

void CTFBotScenarioMoniterInterface::Update(int attrib)
{
	if (g_pGameRules->m_iRoundStatus == ROUND_END)
	{
		if (g_pGameRules->m_iRoundWinStatus != m_bot->m_iTeam && m_loseAction)
		{
			m_loseAction->Update(m_bot);
		}
		else if(m_winAction)
		{
			m_winAction->Update(m_bot);
		}

		return;
	}

	if (attrib & RESULT_RETREAT && m_retreatAction)
		m_retreatAction->Update(m_bot);
	else if (attrib & RESULT_HEALTH && m_hpAction)
		m_hpAction->Update(m_bot);
	else if (attrib & RESULT_AMMO && m_ammoAction)
		m_ammoAction->Update(m_bot);
}

int CTFBotScenarioMoniterInterface::Check(CTFAction* action)	// TODO: add option to ignore certain checks
{
	// dual purpose function
	// you can use the results to determine other stuff
	// it is also possible to input them directly into the Update function below, so it'll b easier

	if (!m_bot)
		return 0;

	int attributes = 0;

	const float lowhealthmulti = 0.6f;
	if (m_bot->pev->health < (m_bot->pev->max_health * lowhealthmulti))
	{
		CBaseEntity* pEnt = NULL;
		while ((pEnt = UTIL_FindEntityByClassname(pEnt, "item_healthbox")) != NULL)
		{
			if (!(pEnt->pev->flags & FL_ONGROUND))
				continue;
			if (pEnt->pev->solid != SOLID_TRIGGER || (pEnt->pev->effects & EF_NODRAW))
				continue;

			// we found a good one!

			attributes = (attributes | RESULT_HEALTH);
			break;
		}

		if (FNullEnt(pEnt)) // this should only happen if there is absolutely NO item_healthboxes
			attributes = (attributes | RESULT_RETREAT);
	}

	const float superlowhealthmulti = 0.3f;
	if ((action && action->ShouldRetreat(m_bot)) || m_bot->pev->health < (m_bot->pev->max_health * superlowhealthmulti) )
		attributes = (attributes | RESULT_RETREAT);

	CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)m_bot->m_pActiveItem;
	
	bool engieGetMetal = false;
	if (m_bot->m_iClass == CLASS_ENGINEER && m_bot->m_iMetal < 200)
		engieGetMetal = true;

	if ( (pWeapon && pWeapon->m_iPrimaryAmmoType > 0 && m_bot->m_rgAmmo[pWeapon->m_iPrimaryAmmoType] <= 0) || engieGetMetal )
		attributes = (attributes | RESULT_AMMO);

	return attributes;
}