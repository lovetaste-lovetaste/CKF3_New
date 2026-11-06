// port of the source sdk's NextBotChasePath. the only important thing here is ChasePathBuilder
// this WOULD be in nav_area.h but the classes has too much shit so it is seperated for now

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "nav.h"
#include "nav_path.h"
#include "nav_area.h"
#include "bot_util.h"
#include "hl_bot.h"
#include "nav_chasepath.h"

extern cvar_t cv_bot_debug;

static inline Vector OriginToFeet(CBaseEntity* ent, const Vector& origin)
{
	return Vector(origin.x, origin.y, origin.z + ent->pev->mins.z);
	// THIS IS PROBABLY NOT NEEDED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
}

float WalkableFraction(CBot* bot, const Vector& fromFeet, const Vector& toFeet)
{
	const float hullHalfHeight = 36.0f;
	const float sampleStep = 32.0f;
	const float maxRise = StepHeight;
	const float maxDrop = JumpCrouchHeight;

	Vector lift(0, 0, hullHalfHeight + StepHeight);

	TraceResult tr;
	UTIL_TraceHull(fromFeet + lift, toFeet + lift, ignore_monsters, human_hull, ENT(bot->pev), &tr);

	if (tr.fStartSolid || tr.fAllSolid || tr.flFraction <= 0.0f)
		return 0.0f;

	float fraction = tr.flFraction;
	Vector end = fromFeet + (toFeet - fromFeet) * fraction;

	float dx = end.x - fromFeet.x;
	float dy = end.y - fromFeet.y;
	float dist = sqrt(dx * dx + dy * dy);

	int steps = (int)ceil(dist / sampleStep);
	if (steps < 1)
		steps = 1;
	if (steps > 64)
		steps = 64;

	float prevZ = fromFeet.z;

	for (int i = 1; i <= steps; ++i)
	{
		Vector p = fromFeet + (end - fromFeet) * ((float)i / (float)steps);

		TraceResult ground;
		UTIL_TraceLine(Vector(p.x, p.y, prevZ + maxRise), Vector(p.x, p.y, prevZ - maxDrop), ignore_monsters, ENT(bot->pev), &ground);

		if (ground.fStartSolid || ground.fAllSolid || ground.flFraction >= 1.0f)
		{
			// wall too tall to step over / drop too deep
			return fraction * ((float)(i - 1) / (float)steps);
		}

		prevZ = ground.vecEndPos.z;
	}

	return fraction;
}

ChasePath::ChasePath(SubjectChaseType chaseHow)
{
	m_failTimer.Invalidate();
	m_throttleTimer.Invalidate();
	m_lifetimeTimer.Invalidate();
	m_chaseHow = chaseHow;
}

void ChasePath::Invalidate(CBot* bot)
{
	m_throttleTimer.Invalidate();
	m_lifetimeTimer.Invalidate();

	if (bot)
		bot->StopPath();
}

void ChasePath::Update(CBot* bot, CBaseEntity* subject, RouteType routeType, Vector* pPredictedSubjectPos)
{
	if (!bot)
		return;

	RefreshPath(bot, subject, routeType, pPredictedSubjectPos);
}

bool ChasePath::IsRepathNeeded(CBot* bot, CBaseEntity* subject) const
{
	// the closer we get, the more accurate our path needs to be
	Vector to = subject->pev->origin - bot->pev->origin;

	const float minTolerance = 0.0f;		// 25.0f;
	const float toleranceRate = 0.33f;		// 1.0f; // 0.15f;

	float tolerance = minTolerance + toleranceRate * to.Length();

	return (subject->pev->origin - bot->m_pathGoal).IsLengthGreaterThan(tolerance);
}

void ChasePath::RefreshPath(CBot* bot, CBaseEntity* subject, RouteType routeType, Vector* pPredictedSubjectPos)
{
	// don't change our path if we're on a ladder
	if (bot->HasPath() && bot->IsOnLadder())
	{
		if (cv_bot_debug.value != 0.0f)
			ALERT(at_console, "%3.2f: bot(#%d) ChasePath::RefreshPath failed. Bot is on a ladder.\n", gpGlobals->time, bot->entindex());

		// don't allow repath until a moment AFTER we have left the ladder
		m_throttleTimer.Start(1.0f);

		return;
	}

	if (subject == NULL)
	{
		if (cv_bot_debug.value != 0.0f)
			ALERT(at_console, "%3.2f: bot(#%d) ChasePath::RefreshPath failed. No subject.\n", gpGlobals->time, bot->entindex());

		return;
	}

	if (!m_failTimer.IsElapsed())
		return;

	// if our path subject changed, repath immediately
	if (subject != (CBaseEntity*)m_lastPathSubject)
	{
		if (cv_bot_debug.value != 0.0f)
			ALERT(at_console, "%3.2f: bot(#%d) Chase path subject changed.\n", gpGlobals->time, bot->entindex());

		Invalidate(bot);

		// new subject, fresh attempt
		m_failTimer.Invalidate();
	}

	if (bot->HasPath() && !m_throttleTimer.IsElapsed())
	{
		// require a minimum time between repaths, as long as we have a path to follow
		return;
	}

	if (bot->HasPath() && GetLifetime() > 0.0f && m_lifetimeTimer.IsElapsed())
	{
		// this path's lifetime has elapsed
		Invalidate(bot);
	}

	if (!bot->HasPath() || IsRepathNeeded(bot, subject))
	{
		// the situation has changed so try a new path
		Vector pathTarget = subject->pev->origin;

		if (m_chaseHow == LEAD_SUBJECT)
			pathTarget = pPredictedSubjectPos ? *pPredictedSubjectPos : PredictSubjectPosition(bot, subject);

		ShortestPathCost cost(bot, routeType);
		Vector start = bot->pev->origin;

		if (bot->m_path.Compute(&start, &pathTarget, cost))
		{
			if (cv_bot_debug.value != 0.0f)
				ALERT(at_console, "%3.2f: bot(#%d) REPATH\n", gpGlobals->time, bot->entindex());

			bot->m_pathfollower.Reset();
			bot->m_pathGoal = pathTarget;
			m_lastPathSubject = subject;

			const float minRepathInterval = 0.5f;
			m_throttleTimer.Start(minRepathInterval);

			// track the lifetime of this new path
			float lifetime = GetLifetime();
			if (lifetime > 0.0f)
				m_lifetimeTimer.Start(lifetime);
			else
				m_lifetimeTimer.Invalidate();
		}
		else
		{
			// can't reach subject - throttle retry based on range to subject
			float range = (subject->pev->origin - bot->pev->origin).Length();
			m_failTimer.Start(0.005f * range);

			if (cv_bot_debug.value != 0.0f)
				ALERT(at_console, "%3.2f: bot(#%d) REPATH FAILED\n", gpGlobals->time, bot->entindex());

			Invalidate(bot);
		}
	}
}

Vector ChasePredictSubjectPosition(CBot* bot, CBaseEntity* subject, float leadRadius)
{
	const Vector& subjectPos = subject->pev->origin;

	Vector to = subjectPos - bot->pev->origin;
	to.z = 0.0f;
	float flRangeSq = to.LengthSquared();

	// don't lead if subject is very far away
	float flLeadRadiusSq = leadRadius * leadRadius;
	if (flRangeSq > flLeadRadiusSq)
		return subjectPos;

	// normalize
	float range = sqrt(flRangeSq);
	to = to / (range + 0.0001f);	// avoid divide by zero

	float leadTime = 0.5f + (range / (bot->GetMoveSpeed() + 0.0001f));

	// estimate amount to lead the subject
	Vector lead = subject->pev->velocity * leadTime;
	lead.z = 0.0f;

	if (DotProduct(to, lead) < 0.0f)
	{
		// the subject is moving towards us
		float perpX = -to.y;
		float perpY = to.x;

		float enemyGroundSpeed = lead.x * perpX + lead.y * perpY;

		lead.x = enemyGroundSpeed * perpX;
		lead.y = enemyGroundSpeed * perpY;
	}

	// compute our desired destination
	Vector pathTarget = subjectPos + lead;

	// validate this destination

	// don't lead through walls
	if (lead.LengthSquared() > 36.0f)
	{
		float fraction = WalkableFraction(bot, OriginToFeet(subject, subjectPos), OriginToFeet(subject, pathTarget));

		if (fraction < 1.0f)
		{
			// tried to lead through an unwalkable area so clip to walkable space
			pathTarget = subjectPos + (pathTarget - subjectPos) * fraction;
		}
	}

	// don't lead over cliffs
	CNavArea* leadArea = TheNavAreaGrid.GetNearestNavArea(&pathTarget);

	// area z is a floor height, so compare against the target's feet
	float targetFeetZ = pathTarget.z + subject->pev->mins.z;

	if (!leadArea || leadArea->GetZ(pathTarget.x, pathTarget.y) < targetFeetZ - JumpCrouchHeight)
	{
		// would fall off a cliff
		return subjectPos;
	}

	return pathTarget;
}

Vector ChasePath::PredictSubjectPosition(CBot* bot, CBaseEntity* subject) const
{
	return ChasePredictSubjectPosition(bot, subject, GetLeadRadius());
}

void DirectChasePath::Update(CBot* me, CBaseEntity* victim, RouteType routeType, Vector* pPredictedSubjectPos)
{
	if (!me || !victim)
		return;

	bool bComputedPredictedPosition;
	Vector vecPredictedPosition;

	if (!DirectChase(&bComputedPredictedPosition, &vecPredictedPosition, me, victim))
	{
		// path around obstacles to reach our victim
		ChasePath::Update(me, victim, routeType, bComputedPredictedPosition ? &vecPredictedPosition : NULL);
	}

	NotifyVictim(me, victim);
}

bool DirectChasePath::DirectChase(bool* pPredictedPositionComputed, Vector* pPredictedPos, CBot* me, CBaseEntity* victim)
{
	*pPredictedPositionComputed = false;

	// NO LADDER!!!!!!!!!!!!!!!!!!
	if (me->IsOnLadder())
		return false;

	Vector leadVictimPos = PredictSubjectPosition(me, victim);

	// Dont want to have to compute the predicted position twice
	*pPredictedPositionComputed = true;
	*pPredictedPos = leadVictimPos;

	Vector myFeet(me->pev->origin.x, me->pev->origin.y, me->pev->origin.z + me->pev->mins.z);

	if (WalkableFraction(me, myFeet, OriginToFeet(victim, leadVictimPos)) < 1.0f)
		return false;

	// the way is clear so just run to them
	me->MoveTowardPos(leadVictimPos);

	// me->m_aimInterface.CreateAimSpot(victim->Center(), BORING);

	// old path is no longer useful since we moved off of it
	Invalidate(me);

	return true;
}

bool DirectChasePath::IsRepathNeeded(CBot* bot, CBaseEntity* subject) const
{
	if (ChasePath::IsRepathNeeded(bot, subject))
		return true;

	return bot->IsStuckFor(2.0f);
}

void DirectChasePath::NotifyVictim(CBot* me, CBaseEntity* victim)
{
	// this was from the sdk but there is no use for this now
	// kept around for future shit just in case
}


ChasePathBuilder::ChasePathBuilder(CBot* me, CBaseEntity* subject, bool lead, RouteType routeType, float leadRadius)
{
	// USE THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	m_me = me;
	m_subject = subject;
	m_lead = lead;
	m_routeType = routeType;
	m_leadRadius = leadRadius;

	m_endposition = Vector(0, 0, 0);
	m_endArea = NULL;
	m_foundValidPosition = false;

	m_foundValidPosition = ComputePath();
}

bool ChasePathBuilder::ComputePath(void)
{
	m_endArea = NULL;

	if (!m_me || !m_subject)
		return false;

	CNavArea* startArea = TheNavAreaGrid.GetNearestNavArea(&m_me->pev->origin);
	if (!startArea)
		return false;

	Vector goalPos = m_lead ? ChasePredictSubjectPosition(m_me, m_subject, m_leadRadius) : m_subject->pev->origin;

	CNavArea* goalArea = TheNavAreaGrid.GetNearestNavArea(&goalPos);
	if (!goalArea)
		return false;

	ShortestPathCost cost(m_me, m_routeType);
	CNavArea* reached = NULL;

	if (!NavAreaBuildPath(startArea, goalArea, &goalPos, cost, &reached) || !reached)
		return false;

	m_endArea = reached;
	m_endposition = goalPos;

	return true;
}

bool ChasePathBuilder::IsRepathNeeded(CBot* me, CBaseEntity* subject)
{
	if (!me->HasPath())
		return true;

	// the closer we get, the more accurate our path needs to be
	float tolerance = 0.33f * (subject->pev->origin - me->pev->origin).Length();

	return (subject->pev->origin - me->m_pathGoal).IsLengthGreaterThan(tolerance);
}

bool ChasePathBuilder::DirectChase(CBot* me, CBaseEntity* subject, bool lead, float leadRadius)
{
	if (!me || !subject || me->IsOnLadder())
		return false;

	Vector goalPos = lead ? ChasePredictSubjectPosition(me, subject, leadRadius) : subject->pev->origin;

	Vector myFeet(me->pev->origin.x, me->pev->origin.y, me->pev->origin.z + me->pev->mins.z);

	if (WalkableFraction(me, myFeet, OriginToFeet(subject, goalPos)) < 1.0f)
		return false;

	me->MoveTowardPos(goalPos);
	// me->m_aimInterface.CreateAimSpot(subject->Center(), BORING);

	// old path is no longer useful since we've moved off of it
	me->StopPath();

	return true;
}
