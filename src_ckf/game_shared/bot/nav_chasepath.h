// port of NextBotChasePath
// jakulo: i tried to leave the comments in like the source sdk version bcuz this shit is annoying and it would probably be better to keep it

#pragma once

#ifndef NAV_CHASE_PATH_H
#define NAV_CHASE_PATH_H

#include "nav_area.h"
#include "hl_bot.h"

class ChasePath
{
public:
	enum SubjectChaseType
	{
		LEAD_SUBJECT,
		DONT_LEAD_SUBJECT
	};

	ChasePath(SubjectChaseType chaseHow = DONT_LEAD_SUBJECT);
	virtual ~ChasePath() {}

	// update path to chase target
	virtual void Update(CBot* bot, CBaseEntity* subject, RouteType routeType = NORMAL_ROUTE, Vector* pPredictedSubjectPos = NULL);

	virtual float GetLeadRadius(void) const;			// range where movement leading begins - beyond this just head right for the subject
	virtual Vector PredictSubjectPosition(CBot* bot, CBaseEntity* subject) const;	// try to cutoff our chase subject, knowing our relative positions and velocities
	virtual bool IsRepathNeeded(CBot* bot, CBaseEntity* subject) const;				// return true if situation has changed enough to warrant recomputing the current path

	virtual float GetLifetime(void) const;				// duration this path is valid. path becomes invalid at its earliest opportunity once this elapses. zero = infinite lifetime

	virtual void Invalidate(CBot* bot);					// cause the path to become invalid (and repath at the earliest opportunity)

	// true while we are waiting to retry after failing to find a path to the subject
	bool IsPathFailing(void) const { return !m_failTimer.IsElapsed(); }

private:
	void RefreshPath(CBot* bot, CBaseEntity* subject, RouteType routeType, Vector* pPredictedSubjectPos);

	CountdownTimer m_failTimer;							// throttle repathing if last path attempt failed
	CountdownTimer m_throttleTimer;						// require a minimum time between re-paths
	CountdownTimer m_lifetimeTimer;
	EHANDLE m_lastPathSubject;							// the subject used to compute the current/last path
	SubjectChaseType m_chaseHow;
};

inline float ChasePath::GetLeadRadius(void) const
{
	return 500.0f;	// 1000.0f;
}

inline float ChasePath::GetLifetime(void) const
{
	// infinite duration
	return 0.0f;
}

class DirectChasePath : public ChasePath
{
public:
	DirectChasePath(ChasePath::SubjectChaseType chaseHow = ChasePath::DONT_LEAD_SUBJECT) : ChasePath(chaseHow)
	{
	}

	// update path to chase target and move bot toward it
	virtual void Update(CBot* me, CBaseEntity* victim, RouteType routeType = NORMAL_ROUTE, Vector* pPredictedSubjectPos = NULL);
	bool DirectChase(bool* pPredictedPositionComputed, Vector* pPredictedPos, CBot* me, CBaseEntity* victim);

	virtual bool IsRepathNeeded(CBot* bot, CBaseEntity* subject) const;

	void NotifyVictim(CBot* me, CBaseEntity* victim);
};

Vector ChasePredictSubjectPosition(CBot* bot, CBaseEntity* subject, float leadRadius = 500.0f);

float WalkableFraction(CBot* bot, const Vector& fromFeet, const Vector& toFeet);

class ChasePathBuilder
{
public:
	ChasePathBuilder(CBot* me, CBaseEntity* subject, bool lead = false, RouteType routeType = NORMAL_ROUTE, float leadRadius = 500.0f);

	bool ComputePath(void);

	// has the subject moved far enough from our path's goal to warrant a new path? (true if we have no path)
	static bool IsRepathNeeded(CBot* me, CBaseEntity* subject);

	// if there is a clear walkable line to the subject, run straight at them. true if we did. call every think
	static bool DirectChase(CBot* me, CBaseEntity* subject, bool lead = false, float leadRadius = 500.0f);

	CBot* m_me;
	CBaseEntity* m_subject;
	bool m_lead;
	RouteType m_routeType;
	float m_leadRadius;

	Vector m_endposition;			// where the path leads (the subject, or the predicted position)
	CNavArea* m_endArea;
	bool m_foundValidPosition;
};

#endif // NAV_CHASE_PATH_H
