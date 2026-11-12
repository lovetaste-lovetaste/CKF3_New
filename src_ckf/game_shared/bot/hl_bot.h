#ifndef ROOSTER_BOT_H
#define ROOSTER_BOT_H

#include "nav_path.h"
#include "trigger.h"
#include "known_entity.h"
#include "hl_bot_action_interface.h"
#include "hl_bot_aim_interface.h"
#include "hl_bot_fight_manager.h"
#include "GameEvent.h"
#include "hl_bot_learning.h"

#define FOR_EACH_VEC( vecName, iteratorName ) \
	for ( int iteratorName = 0; iteratorName < vecName.Count(); iteratorName++ )

static const float BotCommandInterval = 1.0f / 30.0f;
static const float BotFullThinkInterval = BotCommandInterval * 10.0f; // for every 10 commands, one full think is applied
// just ten ticks though so its fine

enum KickStatus
{
	NOT_KICKED = 0,
	GOING_TO_BE_KICKED = 1,
	KICKED = 2,
};

class CBot : public CBasePlayer
{
public:
	CBot(void);
	static CBot* CreateBot(const char* name, bool quotaManaged = false);
	virtual void Spawn(void);
	void Think(void);
	void OnEvent(GameEventType event, CBaseEntity* entity, CBaseEntity* other);

	// this is from the source sdk 2013
	float TransientlyConsistentRandomValue(float period = 10.0f, int seedValue = 0)
	{
		int timeMod = (int)(gpGlobals->time / period) + 1;
		return fabs(cos((float)(seedValue + (entindex() * timeMod))));
	}

	float GetMoveSpeed(void) const;
	void MoveForward(void);
	void MoveBackward(void);
	void StrafeLeft(void);
	void StrafeRight(void);
	bool PressJump(void);
	void PressDuck(void);
	void PressPrimaryAttack(void);
	void PressSecondaryAttack(void);
	void MoveTowardPos(const Vector& pos);

	// recompute goal
	void SetPathToGoal(const Vector& goal,	RouteType routeType = NORMAL_ROUTE); // should probably make a non const overload for this LOOLOLOLOL
	void SetPathToGoal(CBasePlayer* goal,	RouteType routeType = NORMAL_ROUTE);
	void SetPathToGoal(CBaseEntity* goal,	RouteType routeType = NORMAL_ROUTE);
	bool HasPath(void) { return m_path.IsValid(); }
	void ForcePath(CNavPath path) { m_path = path; } // only use this if u know what ur doing!!!!!!!!!!!!!
	bool MakePath(CNavArea* endArea, const Vector& goal)
	{
		if (!endArea)
			return false;
		// only use this if u know what ur doing!!!!!!!!!!!!!
		Vector start = pev->origin;

		if (!m_path.BuildFromParentChain(&start, &goal, endArea))
		{
			m_path.Invalidate();
			return false;
		}

		m_pathfollower.Reset();
		m_pathGoal = goal;
		return true;
	}
	bool MakePath(CNavArea* endArea)
	{
		if (!endArea)
			return false;
		return MakePath(endArea, *endArea->GetCenter());
	}
	void StopPath()
	{
		if (!HasPath())
			return;

		m_path.Invalidate();
	}
	bool IsStuckFor(float duration) const { return m_stuckTimer.IsGreaterThen(duration); }

	CNavPath m_path;					// the path itself
	CNavPathFollower m_pathfollower;	// pathfollower + the improv is in here
	Vector m_pathGoal;					// where the current path leads
	CountdownTimer m_repathTimer;		// limits how often we recompute the path

	CTFBotAimInterface m_aimInterface;

	bool HasEnemy() { return m_hEnemy && !FNullEnt(m_hEnemy->pev) && (m_hEnemy->Classify() == CLASS_BUILDABLE || m_hEnemy->IsPlayer()); }
	CBaseEntity* GetEnemy()
	{
		CBaseEntity* e = m_hEnemy;

		return e;
	}

	virtual BOOL IsBot(void) const { return TRUE; }

	bool m_bQuotaManaged;
	KickStatus m_kickStatus; // this could probably be done another way, but this was the only way i could get bots kicked without instantly crashing the game

	CUtlVector< CKnownEntity > m_knownEntityVector;		// the set of enemies/friends we are aware of
	const CKnownEntity* GetPrimaryKnownThreat(bool onlyVisibleThreats = true);
	const CKnownEntity* GetPrimaryKnownTeammate(bool onlyVisibleThreats = true);

	float m_flLastWeaponHeard;

	void SelectDesiredWeapon();
private:
	// decision logic
	void Update(void);
	void Upkeep(void);

	void ResetCommand(void);
	void ExecuteCommand(void);
	byte ThrottledMsec(void) const;
	void UpdateStuckMonitor(void);

	CTFAction* DesiredAction(int forceclass = 0);

	void UpdateKnownEntities(void);

	int ChooseGoodClass(void);

	float m_forwardSpeed;
	float m_strafeSpeed;
	float m_verticalSpeed;
	unsigned short m_buttonFlags;

	float m_flNextBotThink;
	float m_flNextFullBotThink;
	float m_flPreviousCommandTime;

	float m_lastVisionUpdateTimestamp;
	IntervalTimer m_notVisibleTimer[3];		// for tracking interval since last saw a member of the given team

	Vector m_stuckSpot;					// where we were when the stuck check last ran
	IntervalTimer m_stuckTimer;			// how long we have been near m_stuckSpot

	CTFBotActionInterface m_actionInterface;					// Korozbot action interface

	EHANDLE m_hEnemy;					// the target
	CountdownTimer m_hKnownEntityRecalculateTimer;		// limits how often we change our target
	CountdownTimer m_hReevaluateClassTimer;
};

// this is ripped straight from the source sdk LOLOL
class CClosestTFPlayer
{
public:
	CClosestTFPlayer(const Vector& where, int team = -2)
	{
		m_where = where;
		m_closeRangeSq = FLT_MAX;
		m_closePlayer = NULL;
		m_team = team;
	}

	CClosestTFPlayer(CBaseEntity* entity, int team = -2)
	{
		m_where = (entity->pev->absmax + entity->pev->absmin) / 2.0f;
		m_closeRangeSq = FLT_MAX;
		m_closePlayer = NULL;
		m_team = team;
	}

	bool operator() (CBasePlayer* player)
	{
		if (!player->IsAlive() || player->IsPlayerCloaked())
			return true;

		if (player->m_iTeam != TEAM_RED && player->m_iTeam != TEAM_BLUE)
			return true;

		int team = player->IsPlayerDisguised() ? player->m_iDisguiseTeam : player->m_iTeam;

		if (m_team != -2 && team != m_team)
			return true;

		if ( player->m_iCloak == CLOAK_YES || player->m_iDisguise == DISGUISE_YES )
			return true;

		float rangeSq = (m_where - player->pev->origin).LengthSquared();
		if (rangeSq < m_closeRangeSq)
		{
			m_closeRangeSq = rangeSq;
			m_closePlayer = player;
		}
		return true;
	}

	Vector m_where;
	float m_closeRangeSq;
	CBasePlayer* m_closePlayer;
	int m_team;
};

class CCountClassMembers
{
public:
	CCountClassMembers(const CBot* me, int teamID)
	{
		m_me = me;
		m_myTeam = teamID;
		m_teamSize = 0;

		for (int i = 0; i < 9; ++i)
			m_count[i] = 0;
	}

	bool operator() (CBasePlayer* basePlayer)
	{
		if (basePlayer->m_iTeam != m_myTeam)
			return true;

		++m_teamSize;

		if (m_me == basePlayer)
			return true;

		++m_count[basePlayer->m_iClass];

		return true;
	}

	const CBot* m_me;
	int m_myTeam;
	int m_count[10];
	int m_teamSize;
};

class PopulateVisibleVector
{
public:
	PopulateVisibleVector(CUtlVector< CBaseEntity* >* potentiallyVisible)
	{
		m_potentiallyVisible = potentiallyVisible;
	}

	bool operator() (CBaseEntity* actor)
	{
		m_potentiallyVisible->AddToTail(actor);
		return true;
	}

	CUtlVector< CBaseEntity* >* m_potentiallyVisible;
};

class CollectVisible
{
public:
	CollectVisible(CBot* me)
	{
		m_me = me;
	}

	bool operator() (CBaseEntity* entity)
	{
		if (!entity || entity == m_me || !entity->IsAlive())
			return true;

		if (entity->IsPlayer())
		{
			CBasePlayer* player = (CBasePlayer*)entity;
			if (player->IsPlayerCloaked())
				return true;
			if (player->IsPlayerDisguised() && player->m_iDisguiseTeam == m_me->m_iTeam)
				return true;
		}

		if (entity->Classify() == CLASS_BUILDABLE)
		{
			// nothing
			// TODO: check for buildings that are disabled/being carried
		}

		if ((entity->pev->origin - m_me->pev->origin).Length() > 2048.0f)
			return true;
		if (!m_me->FInViewCone(entity))
			return true;
		if (!m_me->FVisible(entity))
			return true;

		m_recognized.AddToTail(entity);
		return true;
	}

	bool Contains(CBaseEntity* entity) const
	{
		for (int i = 0; i < m_recognized.Count(); ++i)
		{
			if (entity->entindex() == m_recognized[i]->entindex())
			{
				return true;
			}
		}
		return false;
	}
	CBot* m_me;
	CUtlVector< CBaseEntity* > m_recognized;
};

inline void CollectPotentiallyVisibleEntities(CUtlVector< CBaseEntity* >* potentiallyVisible)
{
	potentiallyVisible->RemoveAll();

	// for now only consider players as potentially visible
	// i will iterate through buildings eventually
	PopulateVisibleVector populate(potentiallyVisible);
	ForEachPlayer(populate);
	ForEachBuildable(populate);
}

#endif