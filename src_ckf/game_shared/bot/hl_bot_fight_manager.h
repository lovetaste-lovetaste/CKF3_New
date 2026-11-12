#ifndef HLBOTFIGHTMANAGER
#define HLBOTFIGHTMANAGER

// this is meant to allow bots to fight ppl independant on the action it was doing if that fight would help said action
// eg: bot is trying to cap point but there is an enemy in the way. since killing the enemy would help the bot cap the point, the fight manager takes over and controls the bot
// this also helps me seperate the fighting movement as opposed to the pathfinding, so bots can move in a specific way when fighting

class CBot;
class CBasePlayer;

#include "weapons.h"

enum FightType
{
	FIGHT_NULL,

	FIGHT_TEAM,		// team fights
	FIGHT_DUEL,		// one on one
	FIGHT_CHASE,	// chasing an enemy
	FIGHT_RETREAT,	// running away from an enemy
};

class CBotFightManager
{
public:
	CBotFightManager()
	{
		m_bot = NULL;
		m_bFocused = false;
		m_bWillWeWin = false;
		m_directionPicked = CLOSEST_DIRECTION;
		m_bWalkToEnemy = true;
		m_enemy = NULL;
	}

	void OnEnterNewFight();
	void Update(FightType fightType = FIGHT_CHASE);

	void SetBot(CBot* me) { m_bot = me; }
	void SetFocused(bool focus) { m_bFocused = focus; }

	enum CircleStrafeDirection
	{
		NULL_DIR,

		LEFT,
		RIGHT,
		CLOSEST_DIRECTION,	// go to center of enemy screen
		GO_TO_BACK,			// go behind enemy ( as if it was a spy )
	};

	void MoveAroundTarget(CircleStrafeDirection direction = CLOSEST_DIRECTION, float range = 512.0f, bool walkToEnemyFirst = false);
	// circle strafe around target
	// if walkToEnemyFirst is true, then it doesnt start strafing left/right until it gets in range. otherwise, it strafes at the same time
private:
	CBot* m_bot;
	EHANDLE m_enemy;

	bool m_bFocused;
	bool m_bWalkToEnemy;
	bool m_bWillWeWin;

	CircleStrafeDirection m_directionPicked;

	CountdownTimer m_TimeToKillTimer;
	CountdownTimer m_RocketCheckTimer;
	CountdownTimer m_recomputeRetreatSpot;
	CountdownTimer m_recomputeChasePath;
};

inline float WeaponTimeToKillEnt(CBasePlayerWeapon* pWeapon, CBaseEntity* target)
{
	if (!pWeapon || !pWeapon->m_pPlayer || !target)
		return FLT_MAX;

	float ourDMG = pWeapon->GetDamage();

	if (ourDMG > 0.0f)
	{
		float distance = (pWeapon->m_pPlayer->pev->origin - target->pev->origin).Length();

		int burst = pWeapon->GetExtraShots();

		// multi hit weapons need their extra shots considered so the damage is accurate
		if (burst > 0)
			ourDMG *= burst + 1;

		int shots = (int)ceil(target->pev->health / ourDMG);

		float ttk = (shots - 1) * pWeapon->GetFiringRate();

		// if reloads are needed, then increase ttk by the amount of time it would take to reload
		bool ammoLimited = pWeapon->GetMaxClip() != -1;
		if (ammoLimited && shots > pWeapon->m_iClip)
			ttk += pWeapon->GetReloadRate() * (shots - pWeapon->m_iClip);

		// if the weapons need to be in range ( melee weapons ), and we/they are out of range, then increase ttk by the time it would take to reach the range required
		float range = pWeapon->GetMaxRange();

		if (range && range > 0 && distance > range)
			ttk += (distance - range) / pWeapon->m_pPlayer->GetPlayerMaxSpeed();

		return ttk;
	}

	return FLT_MAX;
}

inline float EntTimeToKillEnt(CBasePlayer* player, CBaseEntity* target)
{
	if (!player || !target)
		return FLT_MAX;

	CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)player->m_pActiveItem;
	if (!pWeapon)
		return FLT_MAX;

	return WeaponTimeToKillEnt(pWeapon, target);
}

#endif