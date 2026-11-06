// tf_bot_get_health

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#include "bot/hl_bot.h"
#include "bot/actions/tf_bot_get_health.h"

void CTFBotGetHealth::OnEnter(CBot* me)
{
}

void CTFBotGetHealth::Update(CBot* me)
{
	if (!me)
		return;

	CBaseEntity* pEnt = NULL;
	CBaseEntity* pBest = NULL;
	float flBest = 0;

	while ((pEnt = UTIL_FindEntityByClassname(pEnt, "item_healthbox")) != NULL)
	{
		if (!(pEnt->pev->flags & FL_ONGROUND))
			continue;
		if (pEnt->pev->solid != SOLID_TRIGGER || (pEnt->pev->effects & EF_NODRAW))
			continue;

		float flDist = (pEnt->pev->origin - me->pev->origin).Length();
		if (!pBest || flDist < flBest)
		{
			pBest = pEnt;
			flBest = flDist;
		}
	}

	if (!FNullEnt(pBest) && pBest)
	{
		me->SetPathToGoal(pBest);
		Continue();
		return;
	}
	
	pEnt = NULL;
	while (pEnt = UTIL_FindEntityByClassname(pEnt, "func_resupplycase"))
	{
		if (pEnt->pev->team == TEAM_UNASSIGNED || pEnt->pev->team == me->m_iTeam)
		{
			me->SetPathToGoal(pEnt);
			break;
		}
	}

	Continue();
}

void CTFBotGetHealth::OnExit(CBot* me)
{
}

void CTFBotGetHealth::OnResume(CBot* me)
{
}