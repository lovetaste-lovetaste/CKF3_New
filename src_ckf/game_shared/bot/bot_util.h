#ifndef BOT_UTIL_H
#define BOT_UTIL_H

#include "eiface.h"
#include "player.h"
#include "buildable.h"
#include "shared_util.h"

//--------------------------------------------------------------------------------------------------------------
// Bot/nav cvars
//--------------------------------------------------------------------------------------------------------------
extern cvar_t cv_bot_traceview;
extern cvar_t cv_bot_stop;
extern cvar_t cv_bot_stop_attack;
extern cvar_t cv_bot_show_nav;
extern cvar_t cv_bot_show_danger;
extern cvar_t cv_bot_nav_edit;
extern cvar_t cv_bot_nav_zdraw;
extern cvar_t cv_bot_debug;
extern cvar_t cv_bot_quicksave;
extern cvar_t cv_bot_last_area_update_tolerance;

extern CBaseBuildable* g_Buildables[3][5][MAX_BUILDABLES_PER_BUCKET];
extern int g_BuildableCount[3][5];

#define SIGN( num )		(((num) < 0) ? -1 : 1)
#define ABS( num )		(SIGN(num) * (num))
#define RAD_TO_DEG		180.0f / 3.14159265358979f

#define CREATE_FAKE_CLIENT		( *g_engfuncs.pfnCreateFakeClient )
#define GET_USERINFO			( *g_engfuncs.pfnGetInfoKeyBuffer )
#define SET_CLIENT_KEY_VALUE	( *g_engfuncs.pfnSetClientKeyValue )

//--------------------------------------------------------------------------------------------------------------
extern void BotPrecache( void );

extern int UTIL_ClientsInGame( void );
extern bool UTIL_IsNameTaken( const char *name );	///< return true if given name is already in use by another player

extern void UTIL_DrawBeamPoints( Vector vecStart, Vector vecEnd, int iLifetime, byte bRed, byte bGreen, byte bBlue );
extern CBasePlayer *UTIL_GetClosestPlayer( const Vector *pos, float *distance = NULL );
extern CBasePlayer *UTIL_GetClosestPlayer( const Vector *pos, int team, float *distance = NULL );	///< closest alive player on the given team
extern CBaseEntity *UTIL_GetLocalPlayer( void );	///< return the listen server host (client 1)

extern int BotTeamID( CBasePlayer *player );

extern void CONSOLE_ECHO( const char *pszMsg, ... );
extern void CONSOLE_ECHO_LOGGED( const char *pszMsg, ... );

extern void HintMessageToAllPlayers( const char *message );

// fast table-based trig, used by the danger-drawing code
extern void InitBotTrig( void );
extern float BotCOS( float angle );
extern float BotSIN( float angle );

//--------------------------------------------------------------------------------------------------------------

class IntervalTimer
{
public:
	IntervalTimer( void )
	{
		m_timestamp = -1.0f;
	}

	void Reset( void )
	{
		m_timestamp = gpGlobals->time;
	}

	void Start( void )
	{
		m_timestamp = gpGlobals->time;
	}

	void Invalidate( void )
	{
		m_timestamp = -1.0f;
	}

	bool HasStarted( void ) const
	{
		return (m_timestamp > 0.0f);
	}

	/// if not started, elapsed time is very large
	float GetElapsedTime( void ) const
	{
		return (HasStarted()) ? (gpGlobals->time - m_timestamp) : 99999.9f;
	}

	bool IsLessThen( float duration ) const
	{
		return (gpGlobals->time - m_timestamp < duration) ? true : false;
	}

	bool IsGreaterThen( float duration ) const
	{
		return (gpGlobals->time - m_timestamp > duration) ? true : false;
	}

private:
	float m_timestamp;
};

class CountdownTimer
{
public:
	CountdownTimer( void )
	{
		m_timestamp = -1.0f;
		m_duration = 0.0f;
	}

	void Reset( void )
	{
		m_timestamp = gpGlobals->time + m_duration;
	}

	void Start( float duration )
	{
		m_timestamp = gpGlobals->time + duration;
		m_duration = duration;
	}

	void Invalidate( void )
	{
		m_timestamp = -1.0f;
	}

	bool HasStarted( void ) const
	{
		return (m_timestamp > 0.0f);
	}

	bool IsElapsed( void ) const
	{
		return (gpGlobals->time > m_timestamp);
	}

private:
	float m_duration;
	float m_timestamp;
};

inline bool IsEntityValid( CBaseEntity *entity )
{
	if (entity == NULL)
		return false;

	if (FNullEnt( entity->pev ))
		return false;

	if (FStrEq( STRING( entity->pev->netname ), "" ))
		return false;

	if (entity->pev->flags & FL_DORMANT)
		return false;

	return true;
}

inline bool IsIntersecting2D( const Vector &startA, const Vector &endA,
															const Vector &startB, const Vector &endB,
															Vector *result = NULL )
{
	float denom = (endA.x - startA.x) * (endB.y - startB.y) - (endA.y - startA.y) * (endB.x - startB.x);
	if (denom == 0.0f)
	{
		// parallel
		return false;
	}

	float numS = (startA.y - startB.y) * (endB.x - startB.x) - (startA.x - startB.x) * (endB.y - startB.y);
	if (numS == 0.0f)
	{
		// coincident
		return true;
	}

	float numT = (startA.y - startB.y) * (endA.x - startA.x) - (startA.x - startB.x) * (endA.y - startA.y);

	float s = numS / denom;
	if (s < 0.0f || s > 1.0f)
	{
		// intersection is not within line segment of startA to endA
		return false;
	}

	float t = numT / denom;
	if (t < 0.0f || t > 1.0f)
	{
		// intersection is not within line segment of startB to endB
		return false;
	}

	// compute intesection point
	if (result)
		*result = startA + s * (endA - startA);

	return true;
}

inline float AngleToFaceCoords(CBaseEntity* pEntity, const Vector& inputtedCoords, bool absoluteAngle = false, bool clientBasedOrigin = true)
{
	if (!pEntity)
		return 0.0f;

	Vector clientPos = pEntity->pev->origin;
	Vector targetPos = inputtedCoords;

	Vector clientAngles = clientBasedOrigin ? pEntity->pev->v_angle : pEntity->pev->angles;

	float resultAngle = 0.0f;

	if (absoluteAngle)
	{
		Vector flatAngles(0.0f, clientAngles.y, 0.0f);

		Vector forward;
		UTIL_MakeVectors(flatAngles);
		forward = gpGlobals->v_forward;
		forward = forward.Normalize();

		clientPos.z = targetPos.z = 0.0f;

		Vector targetVector = targetPos - clientPos;

		if (targetVector.Length() < 0.001f)
			return 0.0f;

		targetVector = targetVector.Normalize();

		float dot = DotProduct(targetVector, forward);

		if (dot > 1.0f)  dot = 1.0f;
		if (dot < -1.0f) dot = -1.0f;

		resultAngle = acos(dot) * RAD_TO_DEG;
	}
	else
	{
		Vector targetVector = targetPos - clientPos;
		Vector anglevector = UTIL_VecToAngles(targetVector);

		//resultAngle = clientAngles.y - anglevector.y;
		resultAngle = UTIL_AngleDiff(clientAngles.y, anglevector.y);

		if (resultAngle < -180.0f)
			resultAngle = 360.0f + resultAngle;

		if (resultAngle > 180.0f)
			resultAngle = 360.0f - resultAngle;
	}

	return resultAngle;
}

template < typename Functor >
bool ForEachPlayer(Functor& func)
{
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBasePlayer* player = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(i));

		if (!IsEntityValid(player))
			continue;

		if (!player->IsPlayer())
			continue;

		if (func(player) == false)
			return false;
	}

	return true;
}

template <typename Functor>
bool ForEachBuildable(Functor& func)
{
	for (int iTeam = 1; iTeam <= 2; ++iTeam)
	{
		for (int iClass = 1; iClass <= 4; ++iClass)
		{
			int count = g_BuildableCount[iTeam][iClass];
			for (int i = 0; i < count; ++i)
			{
				CBaseEntity* pBuildable = static_cast<CBaseEntity*>(g_Buildables[iTeam][iClass][i]);

				if (FNullEnt(pBuildable))
					continue;

				if (func(pBuildable) == false)
					return false;
			}
		}
	}

	return true;
}

template <typename Functor>
bool ForEachBuildableFilter(Functor& func, int iTeam = 0, int iClass = 0)
{
	for (int t = 1; t <= 2; ++t)
	{
		if (iTeam && t != iTeam) continue;
		for (int c = 1; c <= 4; ++c)
		{
			if (iClass && c != iClass) continue;

			int count = g_BuildableCount[t][c];
			for (int i = 0; i < count; ++i)
			{
				CBaseBuildable* pBuildable = g_Buildables[t][c][i];

				if (FNullEnt(pBuildable))
					continue;

				if (func(pBuildable) == false)
					return false;
			}
		}
	}
	return true;
}

#endif // BOT_UTIL_H

