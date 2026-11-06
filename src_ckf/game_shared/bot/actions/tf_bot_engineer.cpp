// this is probably the heaviest action that is being ran tbh
// todo: optimize this shit....

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#include "bot/hl_bot.h"
#include "bot/actions/tf_bot_engineer.h"
#include "bot/actions/tf_bot_seek_and_destroy.h"
#include "bot/actions/tf_bot_control_point_defend.h"

enum EngieBuildStatusAttrib
{
	NO_SENTRY							= (1 << 0),
	NO_DISPENSER						= (1 << 1),
	NO_TELEPORTER_ENTRANCE				= (1 << 2),
	NO_TELEPORTER_EXIT					= (1 << 3),

	DAMAGED_SENTRY						= (1 << 4),
	DAMAGED_DISPENSER					= (1 << 5),
	DAMAGED_TELEPORTER_ENTRANCE			= (1 << 6),
	DAMAGED_TELEPORTER_EXIT				= (1 << 7),

	NON_MAX_LEVEL_SENTRY				= (1 << 8),
	NON_MAX_LEVEL_DISPENSER				= (1 << 9),
	NON_MAX_LEVEL_TELEPORTER_ENTRANCE	= (1 << 10),
	NON_MAX_LEVEL_TELEPORTER_EXIT		= (1 << 11),
};

static int GetBuildingAttributes(CBot* me)
{
	if (!me || me->m_iClass != CLASS_ENGINEER)
		return 0;

	int attrib = 0;

	// the damaged attribute takes priority on purpose
	// i guess you COULD seperate it.. but theres no point, cuz damaged buildings cant get upgraded until they get fixed

	if (!me->m_pBuildable[BUILDABLE_SENTRY - 1])
		attrib |= NO_SENTRY;
	else if (me->m_pBuildable[BUILDABLE_SENTRY - 1]->pev->health < me->m_pBuildable[BUILDABLE_SENTRY - 1]->pev->max_health || (me->m_pBuildable[BUILDABLE_SENTRY - 1]->m_iFlags & BUILD_SAPPERED))
		attrib |= DAMAGED_SENTRY;
	else if (me->m_pBuildable[BUILDABLE_SENTRY - 1]->GetBuildLevel() < 3)
		attrib |= NON_MAX_LEVEL_SENTRY;

	if (!me->m_pBuildable[BUILDABLE_DISPENSER - 1])
		attrib |= NO_DISPENSER;
	else if (me->m_pBuildable[BUILDABLE_DISPENSER - 1]->pev->health < me->m_pBuildable[BUILDABLE_DISPENSER - 1]->pev->max_health || (me->m_pBuildable[BUILDABLE_DISPENSER - 1]->m_iFlags & BUILD_SAPPERED))
		attrib |= DAMAGED_DISPENSER;
	else if (me->m_pBuildable[BUILDABLE_DISPENSER - 1]->GetBuildLevel() < 3)
		attrib |= NON_MAX_LEVEL_DISPENSER;

	if (!me->m_pBuildable[BUILDABLE_ENTRANCE - 1])
		attrib |= NO_TELEPORTER_ENTRANCE;
	else if (me->m_pBuildable[BUILDABLE_ENTRANCE - 1]->pev->health < me->m_pBuildable[BUILDABLE_ENTRANCE - 1]->pev->max_health || (me->m_pBuildable[BUILDABLE_ENTRANCE - 1]->m_iFlags & BUILD_SAPPERED))
		attrib |= DAMAGED_TELEPORTER_ENTRANCE;
	else if (me->m_pBuildable[BUILDABLE_ENTRANCE - 1]->GetBuildLevel() < 3)
		attrib |= NON_MAX_LEVEL_TELEPORTER_ENTRANCE;

	if (!me->m_pBuildable[BUILDABLE_EXIT - 1])
		attrib |= NO_TELEPORTER_EXIT;
	else if (me->m_pBuildable[BUILDABLE_EXIT - 1]->pev->health < me->m_pBuildable[BUILDABLE_EXIT - 1]->pev->max_health || (me->m_pBuildable[BUILDABLE_EXIT - 1]->m_iFlags & BUILD_SAPPERED))
		attrib |= DAMAGED_TELEPORTER_EXIT;
	else if (me->m_pBuildable[BUILDABLE_EXIT - 1]->GetBuildLevel() < 3)
		attrib |= NON_MAX_LEVEL_TELEPORTER_EXIT;

	return attrib;
}

void CTFBotEngineerAction::OnEnter(CBot* me)
{
}

void CTFBotEngineerAction::Update(CBot* me)
{
	if (!me)
		return;

	if (me->HasEnemy())
	{
		me->SwitchSlotWeapon(WEAPON_SLOT_PRIMARY);
		Suspend_For(new CTFBotSeekAndDestroy(RANDOM_FLOAT(3.0f, 12.0f)), "There is an enemy!");
		return;
	}

	int attrib = GetBuildingAttributes(me);
	
	if (attrib & NO_TELEPORTER_ENTRANCE)
	{
		CNavArea* area = TheNavAreaGrid.GetNearestNavAreaAttributes(&me->Center(), true, (me->m_iTeam == TEAM_RED ? NAV_SPAWN_ROOM_RED_EXIT : NAV_SPAWN_ROOM_BLU_EXIT), (NAV_NO_BUILD_ZONE | NAV_NO_TELE_ENTRANCE));
		if (area)
		{
			me->SetPathToGoal(area->GetRandomPoint());

			CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)me->m_pActiveItem;
			if (pWeapon)
			{
				if (pWeapon->m_iId == WEAPON_BUILDPDA)
				{
					if (me->m_iCarryBluePrint)
					{
						if(CanPlaceOnArea(me, area))
							me->PressPrimaryAttack();
					}
					else
						me->Build_Start(BUILDABLE_ENTRANCE);
				}
				else
					me->SwitchSlotWeapon(WEAPON_SLOT_PDA);
			}
		
			Continue();
			return;
		}
	}
	
	if (attrib & NO_TELEPORTER_EXIT)
	{
		if (g_pGameRules->CPExist())
		{
			CControlPoint* cp = g_pGameRules->GetNextControlPointToDefend(me->m_iTeam);
			if (cp)
			{
				CNavArea* area = TheNavAreaGrid.GetNearestNavAreaAttributes(&cp->Center(), true, 0, (NAV_NO_BUILD_ZONE | NAV_NO_TELE_EXIT));
				if (area)
				{
					const Vector* retreat = FindNearbyRetreatSpot(me, &cp->Center(), area, 1024.0f, me->GetEnemyTeam(), true);
					if (retreat)
					{
						area = TheNavAreaGrid.GetNearestNavAreaAttributes(retreat, true, 0, (NAV_NO_BUILD_ZONE | NAV_NO_TELE_EXIT));
						if (area)
						{
							me->SetPathToGoal(area->GetRandomPoint());

							CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)me->m_pActiveItem;
							if (pWeapon)
							{
								if (pWeapon->m_iId == WEAPON_BUILDPDA)
								{
									if (me->m_iCarryBluePrint)
									{
										if (CanPlaceOnArea(me, area))
											me->PressPrimaryAttack();
									}
									else
										me->Build_Start(BUILDABLE_EXIT);
								}
								else
									me->SwitchSlotWeapon(WEAPON_SLOT_PDA);
							}

							Continue();
							return;
						}
					}
					else
					{
						me->SetPathToGoal(area->GetRandomPoint());

						CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)me->m_pActiveItem;
						if (pWeapon)
						{
							if (pWeapon->m_iId == WEAPON_BUILDPDA)
							{
								if (me->m_iCarryBluePrint)
								{
									if (CanPlaceOnArea(me, area))
										me->PressPrimaryAttack();
								}
								else
									me->Build_Start(BUILDABLE_EXIT);
							}
							else
								me->SwitchSlotWeapon(WEAPON_SLOT_PDA);
						}

						Continue();
						return;
					}
				}
			}
		}
	}
	
	if (attrib & NO_DISPENSER)
	{
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

		if (pBest)
		{
			CNavArea* area = TheNavAreaGrid.GetNearestNavAreaAttributes(&pBest->pev->origin, true, 0, (NAV_NO_BUILD_ZONE | NAV_NO_DISPENSER));
			if (area)
			{
				me->SetPathToGoal(*area->GetCenter());

				CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)me->m_pActiveItem;
				if (pWeapon)
				{
					if (pWeapon->m_iId == WEAPON_BUILDPDA)
					{
						if (me->m_iCarryBluePrint)
						{
							if (CanPlaceOnArea(me, area))
								me->PressPrimaryAttack();
						}
						else
							me->Build_Start(BUILDABLE_DISPENSER);
					}
					else
						me->SwitchSlotWeapon(WEAPON_SLOT_PDA);
				}

				Continue();
				return;
			}
		}
	}
	
	if (attrib & NO_SENTRY)
	{
		if (g_pGameRules->CPExist())
		{
			CControlPoint* cp = g_pGameRules->GetNextControlPointToDefend(me->m_iTeam);
			if (cp)
			{
				CNavArea* area = TheNavAreaGrid.GetNearestNavAreaAttributes(&cp->Center(), true, 0, (NAV_NO_BUILD_ZONE | NAV_NO_SENTRY));
				if (area)
				{
					me->SetPathToGoal(*area->GetCenter());

					CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)me->m_pActiveItem;
					if (pWeapon)
					{
						if (pWeapon->m_iId == WEAPON_BUILDPDA)
						{
							if (me->m_iCarryBluePrint)
							{
								if (CanPlaceOnArea(me, area))
									me->PressPrimaryAttack();
							}
							else
								me->Build_Start(BUILDABLE_SENTRY);
						}
						else
							me->SwitchSlotWeapon(WEAPON_SLOT_PDA);
					}

					Continue();
					return;
				}
			}
		}
	}

	// this will have healing shit here once i redo the aiming system
	// for now, just look for kills
	Suspend_For(new CTFBotSeekAndDestroy(RANDOM_FLOAT(5.0f, 30.0f)), "Bored. Looking for kills!");
	return;

	//Continue();
}

bool CTFBotEngineerAction::CanPlaceOnArea(CBot* me, CNavArea* area)
{
	if (!area || !me)
		return false;

	Vector vecAngles = me->pev->v_angle + me->pev->punchangle;
	vecAngles.x = 0;
	UTIL_MakeVectors(vecAngles);
	// i forgot if this part is needed LOOOOOOOOOOOOL
	// iirc it has something to do with v_forward

	Vector vecSrc = me->GetGunPosition() + gpGlobals->v_forward * 60;

	if (area->Contains(&vecSrc) && me->Build_PreDeploy(vecSrc))
		return true;

	return false;
}

void CTFBotEngineerAction::OnExit(CBot* me)
{
}

void CTFBotEngineerAction::OnResume(CBot* me)
{
}