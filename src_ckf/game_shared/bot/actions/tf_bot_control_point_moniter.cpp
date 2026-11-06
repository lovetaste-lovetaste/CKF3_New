// go attack the points

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#include "bot/hl_bot.h"
#include "bot/actions/tf_bot_control_point_moniter.h"
#include "bot/actions/tf_bot_control_point_attack.h"
#include "bot/actions/tf_bot_control_point_defend.h"
#include "bot/actions/tf_bot_seek_and_destroy.h"
#include "bot/actions/tf_bot_retreat.h"

void CTFBotControlPointMoniter::OnEnter(CBot* me)
{
}

bool CTFBotControlPointMoniter::ShouldHurry(CBot* me)
{
	if (m_refreshHurryTimer.IsElapsed())
	{
		m_refreshHurryTimer.Start(RANDOM_FLOAT(2.0f,6.0f));

		m_bHurry = g_pGameRules->CPCountPoints(me->m_iTeam) < g_pGameRules->CPCountPoints(me->GetEnemyTeam());
	}

	return m_bHurry;
}

void CTFBotControlPointMoniter::Update(CBot* me)
{
	CControlPoint* attackPoint = g_pGameRules->GetNextControlPointToAttack(me->m_iTeam);
	CControlPoint* defendPoint = g_pGameRules->GetNextControlPointToDefend(me->m_iTeam);

	if (!(attackPoint || defendPoint))
	{
		Suspend_For(new CTFBotSeekAndDestroy, "No control points to attack/defend!");
		return;
	}

	if(attackPoint && !defendPoint)
	{
		Suspend_For(new CTFBotControlPointAttack, "Switching to attacking points only");
		return;
	}
	
	if(defendPoint && !attackPoint)
	{
		Suspend_For(new CTFBotControlPointDefend, "Switching to defending points only");
		return;
	}

	if (me->m_repathTimer.IsElapsed())
	{
		// only rerolls if the bot can actually repath, to prevent errors

		bool defending = false;

		float disFromAttack = (attackPoint->Center() - me->pev->origin).Length2D();
		float disFromDefend = (defendPoint->Center() - me->pev->origin).Length2D();

		float timeToAttack = -1.0f;
		if (attackPoint->m_iState == CP_CAPTURING)
			timeToAttack = (100.0f - attackPoint->m_flProgress) / attackPoint->m_flCapRate;
		//	else if (attackPoint->m_iState == CP_BACK)
			//	timeToAttack = attackPoint->m_flProgress / attackPoint->m_flCapRate;

		float timeToDefend = -1.0f;
		if (defendPoint->m_iState == CP_CAPTURING)
			timeToDefend = (100.0f - defendPoint->m_flProgress) / defendPoint->m_flCapRate;
		//	else if (defendPoint->m_iState == CP_BACK)
			//	timeToDefend = defendPoint->m_flProgress / defendPoint->m_flCapRate;

		switch (defendPoint->m_iState)
		{
		case CP_BLOCKED:
		case CP_CAPTURING:
		case CP_BACK:
			// Unless we can cap faster than the enemy, then defend it

			if (defendPoint->m_iCapTeam != me->m_iTeam)
			{
				defending = true;
				if ((IsInside(me->pev->origin, attackPoint) || disFromAttack < disFromDefend) && attackPoint->m_iCapTeam == me->m_iTeam && timeToAttack < timeToDefend)
				{
					defending = false;
					// we might be able to cap faster, so attack
				}
			}

			break;
		case CP_IDLE:
		default:
			break;
		}

		CControlPoint* controlpoint = defending ? defendPoint : attackPoint;

		Vector point = controlpoint->Center();
		point.x = RANDOM_FLOAT(controlpoint->pev->absmax.x, controlpoint->pev->absmin.x);
		point.y = RANDOM_FLOAT(controlpoint->pev->absmax.y, controlpoint->pev->absmin.y);
		point.z = controlpoint->Center().z;
		me->SetPathToGoal(point);
	}

	Continue();
}

void CTFBotControlPointMoniter::OnExit(CBot* me)
{
}

void CTFBotControlPointMoniter::OnResume(CBot* me)
{
}