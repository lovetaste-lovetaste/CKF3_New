#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#include "bot/hl_bot.h"
#include "bot/actions/tf_bot_medic_heal.h"

void CTFBotMedicHeal::OnEnter(CBot* me)
{
}

void CTFBotMedicHeal::Update(CBot* me)
{
	m_bRetreat = false;

	if (!me)
		return;

	const CKnownEntity* teammate = me->GetPrimaryKnownTeammate(false);
	if (teammate)
	{
		me->SetPathToGoal(teammate->GetLastKnownPosition());
		/*
		me->m_aimInterface.CreateAimSpot(teammate->GetEntity(), IMPORTANT);

		CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)me->m_pActiveItem;

		if (!pWeapon)
			return;

		if (pWeapon->m_iId != WEAPON_MEDIGUN)
			me->SwitchSlotWeapon(WEAPON_SLOT_SECONDARY);
		else
		{
			if (me->m_iUbercharge >= 100 && me->pev->health < ((me->pev->max_health * 0.5) || (teammate->GetEntity()->pev->max_health * 0.5)) )
			{
				me->PressSecondaryAttack();
				pWeapon->SecondaryAttack(); // just in case
			}
			else
				me->PressPrimaryAttack();
		}
		*/
	}
	else
		m_bRetreat = true;

	Continue();
}

void CTFBotMedicHeal::OnExit(CBot* me)
{
}

void CTFBotMedicHeal::OnResume(CBot* me)
{
}

bool CTFBotMedicHeal::ShouldRetreat(CBot* me)
{
	return m_bRetreat;
}