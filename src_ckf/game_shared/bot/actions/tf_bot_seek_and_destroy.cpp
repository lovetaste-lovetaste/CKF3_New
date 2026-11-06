// TFBot seek and destroy
// looks for the nearest enemy to kill!!!
// This can also be used as a template as everything is already wired

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#include "bot/hl_bot.h"
#include "bot/actions/tf_bot_seek_and_destroy.h"

void CTFBotSeekAndDestroy::OnEnter(CBot* me)
{
}

void CTFBotSeekAndDestroy::Update(CBot* me)
{
	if (m_giveupTimer.HasStarted() && m_giveupTimer.IsElapsed())
	{
		Done("Finished with give up timer");
		return;
	}

	CBaseEntity* enemy = me->GetEnemy();

	if(me->HasEnemy() && enemy)
		me->SetPathToGoal(enemy->Center());
	else if (!me->HasPath())
	{
		CResupplyRoom* spawn = GetClosestSpawnRoom(me, me->GetEnemyTeam());
		if (spawn)
		{
			CNavArea* area = TheNavAreaGrid.GetNearestNavAreaAttributes(&spawn->Center(), false, me->m_iTeam == TEAM_RED ? NAV_SPAWN_ROOM_BLUE : NAV_SPAWN_ROOM_RED);
			if (area)
			{
				Vector close = area->GetRandomPoint();
				me->SetPathToGoal(close);
			}
		}
		else
		{
			// if, for some reason, theres no spawn rooms, then just go to the nearest enemy directly

			CClosestTFPlayer closest(me, me->GetEnemyTeam());
			ForEachPlayer(closest);

			if (closest.m_closePlayer)
			{
				me->SetPathToGoal(closest.m_closePlayer);
			}
			else
			{
				CBaseEntity* pSpot;

				if (me->m_iTeam == TEAM_RED)
				{
					// info_player_start is for blu spawners

					pSpot = UTIL_FindEntityByClassname(NULL, "info_player_start");

					if (!FNullEnt(pSpot))
					{
						CNavArea* area = TheNavAreaGrid.GetNearestNavArea(&pSpot->Center(), false);
						if (area)
						{
							Vector close = area->GetRandomPoint();
							me->SetPathToGoal(close);
						}
					}
				}
				else
				{
					// info_player_deathmatch is for red spawners

					pSpot = UTIL_FindEntityByClassname(NULL, "info_player_deathmatch");

					if (!FNullEnt(pSpot))
					{
						CNavArea* area = TheNavAreaGrid.GetNearestNavArea(&pSpot->Center(), false);
						if (area)
						{
							Vector close = area->GetRandomPoint();
							me->SetPathToGoal(close);
						}
					}
				}
			}
		}
	}

	Continue();
}

void CTFBotSeekAndDestroy::OnExit(CBot* me)
{
}

void CTFBotSeekAndDestroy::OnResume(CBot* me)
{
}

CResupplyRoom* CTFBotSeekAndDestroy::GetClosestSpawnRoom(CBot* me, int team)
{
	CResupplyRoom* best = NULL;
	float bestdistance = FLT_MAX;

	for (int i = 0; i < g_pGameRules->m_RespawnRooms.Count(); ++i)
	{
		if (!g_pGameRules->m_RespawnRooms.IsValidIndex(i))
			break;

		CResupplyRoom* pRoom = (CResupplyRoom*)CBaseEntity::Instance(g_pGameRules->m_RespawnRooms.Element(i));

		if (!pRoom)
			continue;
		if (team != -2 && pRoom->pev->team != team)
			continue;

		float dist = (pRoom->Center() - me->pev->origin).LengthSquared();
		if (dist < bestdistance)
		{
			bestdistance = dist;
			best = pRoom;
		}
	}

	return best;
}