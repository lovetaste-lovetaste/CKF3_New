// go attack the points

// MEANT FOR ATTACK AND DEFEND! FOR REGULAR CP / KOTH, LOOK AT CTFBOTCONTROLPOINTMONITER

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#include "bot/hl_bot.h"
#include "bot/actions/tf_bot_control_point_attack.h"
#include "bot/actions/tf_bot_seek_and_destroy.h"
#include "bot/actions/tf_bot_retreat.h"

void CTFBotControlPointAttack::OnEnter(CBot* me)
{
}

void CTFBotControlPointAttack::Update(CBot* me)
{
	CControlPoint* controlpoint = g_pGameRules->GetNextControlPointToAttack(me->m_iTeam);

	if (controlpoint)
	{
		if ( !(me->Center() < controlpoint->pev->absmax && me->Center() > controlpoint->pev->absmin) ) // if not already inside the point area
		{
			Vector point = controlpoint->Center();
			point.x = RANDOM_FLOAT(controlpoint->pev->absmax.x, controlpoint->pev->absmin.x);
			point.y = RANDOM_FLOAT(controlpoint->pev->absmax.y, controlpoint->pev->absmin.y);
			point.z = controlpoint->Center().z;

			me->SetPathToGoal(point);
		}
	}
	else
	{
		Suspend_For(new CTFBotSeekAndDestroy, "No control points to attack!");
		return;
	}

	Continue();
}

void CTFBotControlPointAttack::OnExit(CBot* me)
{
}

void CTFBotControlPointAttack::OnResume(CBot* me)
{
}