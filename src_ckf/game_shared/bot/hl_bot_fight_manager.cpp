// fighting!!!

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "hl_bot.h"
#include "hl_bot_fight_manager.h"
#include "nav_chasepath.h"

float EntTimeToKillEnt(CBasePlayer* player, CBaseEntity* target)
{
	if (!player)
		return FLT_MAX;

	CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)player->m_pActiveItem;
	if (!pWeapon)
		return FLT_MAX;

	float ourDMG = pWeapon->GetDamage();

	if (ourDMG > 0.0f)
	{
		float distance = (player->pev->origin - target->pev->origin).Length();

		int burst = pWeapon->GetExtraShots();

		// multi hit weapons need their extra shots considered so the damage is accurate
		if (burst > 0)
			ourDMG *= burst + 1;

		int shots = (int)ceil(target->pev->health / ourDMG);

		float ttk = (shots - 1) * pWeapon->GetFiringRate();

		// if reloads are needed, then increase ttk by the amount of time it would take to reload
		bool ammoLimited = pWeapon->GetMaxClip() != WEAPON_NOCLIP;
		if (ammoLimited && shots > pWeapon->m_iClip)
			ttk += pWeapon->GetReloadRate() * (shots - pWeapon->m_iClip);

		// if the weapons need to be in range ( melee weapons ), and we/they are out of range, then increase ttk by the time it would take to reach the range required
		float range = pWeapon->GetMaxRange();

		if (range && range > 0 && distance > range)
			ttk += (distance - range) / player->GetPlayerMaxSpeed();

		return ttk;
	}

	return FLT_MAX;
}

void CBotFightManager::Update(FightType fightType)
{
	if (!m_bot || !m_bot->HasEnemy())
		return;

	/*
	if (!m_bot->GetEnemy()->IsPlayer())
	{
		// buildings are deliberately here!!!

		return;
	}
	*/

	CBaseEntity* target = m_bot->GetEnemy();
	if (!target)
		return;

	switch (fightType)
	{
	case FIGHT_DUEL:
	{
		if (m_RocketCheckTimer.IsElapsed())
		{
			m_RocketCheckTimer.Start(RANDOM_FLOAT(0.2,1.0));

			CBaseEntity* pEntity = NULL;
			Vector vecSrc = m_bot->pev->origin;

			while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, 128.0)) != NULL)
			{
				//if (pEntity->Classify() != CLASS_PROJECTILE)
					//continue;
				//no point

				if (!FClassnameIs(pEntity->pev, "pj_rocket"))
					continue;

				Vector vecToBot = vecSrc - pEntity->pev->origin;
				Vector vecVel = pEntity->pev->velocity;

				if (vecVel.Length() < 1.0f)
					continue;

				float flDot = DotProduct(vecVel.Normalize(), vecToBot.Normalize());

				if (flDot < 0.7f) 
					continue;
				// only pay attention to rockets that are actually going to the bot
				// raise toward 1.0 for stricter aim

				// we found a valid rocket, so we should JUMP!!!
				m_bot->PressJump();

				CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)m_bot->m_pActiveItem;
				if (pWeapon && pWeapon->m_iId == WEAPON_FLAMETHROWER)
				{
					CFlamethrower* pFlame = (CFlamethrower*)pWeapon;
					if (pFlame->m_flNextSecondaryAttack < UTIL_WeaponTimeBase() && m_bot->m_rgAmmo[pFlame->m_iPrimaryAmmoType] > 20)
					{
						m_bot->m_aimInterface.CreateAimSpot(target, MANDATORY, 1.0f);
						m_bot->PressSecondaryAttack();
					}
				}


				break;
			}
		}

		if (m_pickSideTimer.IsElapsed() || m_directionPicked == NULL_DIR)
		{
			// todo: use enemy movement dir to see if we should mirror or anti mirror

			m_pickSideTimer.Start(RANDOM_FLOAT(0.1, 2.0));
			m_directionPicked = (CircleStrafeDirection)RANDOM_LONG(LEFT, GO_TO_BACK);
			m_bWalkToEnemy = RANDOM_LONG(0, 2) == 0 ? false : true;
		}
		
		if(m_TimeToKillTimer.IsElapsed())
		{
			m_TimeToKillTimer.Start(RANDOM_FLOAT(0.1,0.33));
			m_bWillWeWin = true;

			// if fighting an entity that can actually fight back, only fight if we can kill them before it can kill us. otherwise, RUN!!!

			float ttk = EntTimeToKillEnt(m_bot, target);
			float targetTTK = FLT_MAX;

			if (target->IsPlayer())
			{
				CBasePlayer* pPTarget = (CBasePlayer*)target;
				targetTTK = EntTimeToKillEnt(pPTarget, m_bot);
			}
			else if(target->Classify() == CLASS_BUILDABLE)
			{
				CBaseBuildable* pBuilding = (CBaseBuildable*)target;
				if (pBuilding)
				{
					int iBuildClass = pBuilding->GetBuildClass();

					if (iBuildClass == BUILDABLE_SENTRY)
					{
						// only sentries can fight back
						CBuildSentry* pSentry = dynamic_cast<CBuildSentry*>(pBuilding);

						if (pSentry && !(pSentry->m_iFlags & (BUILD_BUILDING | BUILD_UPGRADING) || pSentry->m_pSapper) )
						{
							if (pSentry->m_iAmmo)
							{
								int shots = (int)ceil(m_bot->pev->health / 13); // assume max bullet damage, 13
								shots = min(shots, pSentry->m_iAmmo);

								float firstShot = max(0.0f, pSentry->m_flNextAttack - gpGlobals->time); // i should probably add this to the player version LOLOL
								targetTTK = firstShot + (shots - 1) * pSentry->m_flROF;
							}

							if (pSentry->m_iLevel == 3 && pSentry->m_iRocket)
							{
								float firstRocket = max(0.0f, pSentry->m_flNextRocket - gpGlobals->time);
								if (firstRocket < targetTTK)
									targetTTK = min(targetTTK, firstRocket); // if the rocket can kill before the bullets will then just override the ttk
							}
						}
					}
				}
			}

			m_bWillWeWin = ttk < targetTTK && targetTTK - ttk >= 0.5f;
			// checks if we will be able to kill the enemy at least half a second faster
		}

		if (!m_bWillWeWin)
		{
			Update(FIGHT_RETREAT); // if we would die first, then just run away
			return;
		}

		// we prob will win this, so fight!!!

		MoveAroundTarget(m_directionPicked, 256.0, m_bWalkToEnemy);
		
		break;
	}

	case FIGHT_CHASE:
	{
		// simple chase
		
		if (ChasePathBuilder::DirectChase(m_bot, target, true))
		{
			m_recomputeChasePath.Invalidate();
			break;
		}

		if (m_recomputeChasePath.IsElapsed() && ChasePathBuilder::IsRepathNeeded(m_bot, target))
		{
			m_recomputeChasePath.Start(0.5f);

			ChasePathBuilder chase(m_bot, target, true, FASTEST_ROUTE);
			if (!chase.m_foundValidPosition || !m_bot->MakePath(chase.m_endArea, chase.m_endposition))
			{
				m_recomputeChasePath.Start(RANDOM_FLOAT(0.5f, 1.0f));
				m_bot->StopPath();
			}
		}

		break;
	}
	
	case FIGHT_RETREAT:
	{
		// simple retreat

		if (m_recomputeRetreatSpot.IsElapsed())
		{
			m_recomputeRetreatSpot.Start(RANDOM_FLOAT(0.5f, 2.0f));
		
			CBaseEntity* threat = m_bot->GetEnemy();
			if (threat)
			{
				RetreatPathBuilder retreat(m_bot, threat, 512.0f);
				if (!(retreat.m_foundValidPosition && m_bot->MakePath(retreat.m_endArea)))
				{
					//m_recomputeRetreatSpot.Invalidate();
					m_recomputeRetreatSpot.Start(RANDOM_FLOAT(0.5f, 1.0f));
					m_bot->StopPath();
				}
			}
		}

		break;
	}

	default:break;
	}

	//ALERT(at_console, "Bot is using fight manager update");

	return;
}

void CBotFightManager::MoveAroundTarget(CircleStrafeDirection direction, float range, bool walkToEnemyFirst)
{
	if (!m_bot || !m_bot->HasEnemy())
		return;

	CBaseEntity* target = m_bot->GetEnemy();

	if (!target || !target->IsAlive())
		return;

	if (range < 0.0f)
		range = 0.0f;

	//m_bot->m_aimInterface.CreateAimSpot(target, IMPORTANT, 0.25f);

	Vector toEnemy = target->pev->origin - m_bot->pev->origin;
	float dist = toEnemy.Length2D();

	float tolerance = range * 0.1f;
	if (tolerance < 16.0f)
		tolerance = 16.0f;

	bool tooFar = dist > (range + tolerance);
	bool tooClose = dist < (range - tolerance);

	bool goLeft = true;

	switch (direction)
	{
	case RIGHT:
	{
		goLeft = false;
		break;
	}
	case CLOSEST_DIRECTION:
	{
		float ang = AngleToFaceCoords(target, m_bot->pev->origin, false, target->IsPlayer());

		goLeft = (ang < 0.0f);
		break;
	}
	case GO_TO_BACK:
	{
		float ang = AngleToFaceCoords(target, m_bot->pev->origin, false, target->IsPlayer());

		goLeft = (ang >= 0.0f);
		break;
	}
	case LEFT:
	default:
		break;
	}

	if (tooFar)
	{
		Update(FIGHT_CHASE);
		// just use the chase logic
	}
	else if (tooClose)
		m_bot->MoveBackward();

	if (!(walkToEnemyFirst && tooFar))
	{
		if (goLeft)
			m_bot->StrafeLeft();
		else
			m_bot->StrafeRight();
	}
}