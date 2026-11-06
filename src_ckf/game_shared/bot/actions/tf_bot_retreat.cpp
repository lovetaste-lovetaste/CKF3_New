// tf_bot_retreat

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#include "bot/hl_bot.h"
#include "bot/actions/tf_bot_retreat.h"

/*
// collect nearby areas that provide cover from threats
class CSearchForCover
{
public:
	CSearchForCover(CBot* me)
	{
		// THIS ASSUMES THAT WE ARE HIDING FROM ENEMIES
		m_me = me;

		FOR_EACH_VEC(me->m_knownEntityVector, iter)
		{
			CKnownEntity& known = me->m_knownEntityVector.Element(iter);

			if (known.GetTeam() != m_me->m_iTeam && known.GetEntity() && known.GetEntity()->IsPlayer())
			{
				CBasePlayer* enemy = (CBasePlayer*)known.GetEntity();
				if (enemy && enemy->IsAlive())
				{
					if (enemy->IsPlayerDisguised() && enemy->m_iDisguiseTeam == m_me->m_iTeam)
						continue;

					if (enemy->IsPlayerCloaked())
						continue;

					m_hideFromTheseEntities.AddToTail(enemy);
				}
			}
		}

		m_viability = NOT_VIABLE;

		m_numFound = 0;
	}
	enum { MAX_SPOTS = 256 };

	enum Viability
	{
		NOT_VIABLE = 0,		// enemy can see this point
		KINDA_VIABLE = 1,	// enemy can technically see this point but it is outside their fov
		VIABLE = 2,			// enemy cannot see this point
		VERY_VIABLE = 3,	// enemy cannot see this point and it is far away
	};

	bool operator() (CNavArea* area)
	{
		if (m_hideFromTheseEntities.Count() == 0 || ++m_numFound > MAX_SPOTS)
			return false;
		// STOP COUNTING

		Viability areaTotalViability = VERY_VIABLE;
		FOR_EACH_VEC(m_hideFromTheseEntities, iter)
		{
			CBasePlayer* enemy = m_hideFromTheseEntities.Element(iter);
			Vector point = *area->GetCenter();
			point.z += HalfHumanHeight;

			bool enemyVisible = enemy->FVisible(point);
			bool enemyInFov = enemy->FInViewCone(&point);

			int v = (!enemyVisible ? 2 : 0) + (!enemyInFov ? 1 : 0);
			Viability newViability = (Viability)((((v) < (2)) ? (v) : (2)));

			if (newViability == VIABLE)
			{
				if ((enemy->pev->origin - point).IsLengthGreaterThan(1024.0f))
					newViability = VERY_VIABLE;
			}

			if (newViability < areaTotalViability)
				areaTotalViability = newViability;
			// this area is considered as bad as its worse viable area
			// eg: 9/10 players cant see it, making it viable. however, one player can see it, making it not_viable
			// since someone can see it, it is considered a not_viable area
		}

		if (areaTotalViability > m_viability)
		{
			// this new area is considered more viable than the set of old ones, so start a new set at this level
			m_viability = areaTotalViability;
			m_coverAreaVector.RemoveAll();
		}

		if (areaTotalViability == m_viability) // adds the new area to it. ignores worse ones LOLOL
			m_coverAreaVector.AddToTail(area);

		return true;
	}

	CBot* m_me;
	CUtlVector< CBasePlayer* > m_hideFromTheseEntities;
	CUtlVector< CNavArea* > m_coverAreaVector;
	Viability m_viability;
	int m_numFound;
};
*/

void CTFBotRetreat::OnEnter(CBot* me)
{
	m_recomputeRetreatSpot.Invalidate();
}

void CTFBotRetreat::Update(CBot* me)
{
	if (!me || !me->m_lastNavArea)
		return;

	if (m_recomputeRetreatSpot.IsElapsed())
	{
		m_recomputeRetreatSpot.Start(RANDOM_FLOAT(1.5f, 3.0f));

		float bestDist = FLT_MAX;
		CBaseEntity* best = NULL;

		// run away from our enemy if there is somewhere to run to
		bool retreating = false;
		CBaseEntity* threat = me->GetEnemy();
		if (threat)
		{
			RetreatPathBuilder retreat(me, threat, 500.0f);
			if (retreat.m_foundValidPosition && me->MakePath(retreat.m_endArea))
				retreating = true;
			else
			{
				//m_recomputeRetreatSpot.Invalidate();
				m_recomputeRetreatSpot.Start(RANDOM_FLOAT(0.5f, 1.0f));
				me->StopPath();
			}
		}

		// nothing to run from (or nowhere to run to) so head for our nearest resupply instead
		if (!retreating)
		{
			while (best = UTIL_FindEntityByClassname(best, "func_resupplycase"))
			{
				if (best->pev->team == TEAM_UNASSIGNED || best->pev->team == me->m_iTeam)
				{
					me->SetPathToGoal(best);
					break;
				}
			}
		}
	}

	Continue();
}

void CTFBotRetreat::OnExit(CBot* me)
{
}

void CTFBotRetreat::OnResume(CBot* me)
{
}