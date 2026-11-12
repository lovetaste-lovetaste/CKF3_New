// go defend the points

// MEANT FOR ATTACK AND DEFEND! FOR REGULAR CP / KOTH, LOOK AT CTFBOTCONTROLPOINTMONITER

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#include "bot/hl_bot.h"
#include "bot/actions/tf_bot_control_point_defend.h"
#include "bot/actions/tf_bot_seek_and_destroy.h"

void CTFBotControlPointDefend::OnEnter(CBot* me)
{
	m_seekTimer.Invalidate();
}

void CTFBotControlPointDefend::Update(CBot* me)
{
	CControlPoint* controlpoint = g_pGameRules->GetNextControlPointToDefend(me->m_iTeam);

	if (controlpoint)
	{
		Vector point = controlpoint->Center();
		point.x = RANDOM_FLOAT(controlpoint->pev->absmax.x, controlpoint->pev->absmin.x);
		point.y = RANDOM_FLOAT(controlpoint->pev->absmax.y, controlpoint->pev->absmin.y);
		point.z = controlpoint->Center().z;
		m_seekTimer.Invalidate();
		me->SetPathToGoal(point);
		
		if (IsInside(me->pev->origin, controlpoint))//(me->Center() - controlpoint->Center()).IsLengthLessThan( (controlpoint->pev->absmax - controlpoint->pev->absmin).Length() / 2.0f ))
		{
			// inside point area already

			if (me->m_iClass == CLASS_HEAVY)
			{
				CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)me->m_pActiveItem;

				if (pWeapon->m_iId == WEAPON_MINIGUN)
					me->PressSecondaryAttack();
			}

			if (!m_seekTimer.HasStarted())
			{
				m_seekTimer.Start(RANDOM_FLOAT(1.0f, 10.0f));
			}
			else if (m_seekTimer.IsElapsed())
			{
				Suspend_For(new CTFBotSeekAndDestroy(RANDOM_FLOAT(10.0f, 30.0f)), "Bored. Looking for kills!");
				return;
			}
		}
	}
	else
	{
		Suspend_For(new CTFBotSeekAndDestroy(), "No control points to defend!");
		return;
	}

	Continue();
}

void CTFBotControlPointDefend::OnExit(CBot* me)
{
	m_seekTimer.Invalidate();
}

void CTFBotControlPointDefend::OnResume(CBot* me)
{
	m_seekTimer.Invalidate();
}