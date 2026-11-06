#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "soundent.h"
#include "gamerules.h"
#include "player.h"
#include "client.h"
#include "bot_util.h"

#include "hl_bot.h"
#include "hl_bot_aim_interface.h"

// tuning
#define AIM_SACCADE_SPEED 1400.0f		// this was based off nb_saccade_speed but i havent added that yet
#define AIM_MAX_DELTA_T 0.1f		// never turn for more than this much time in one Update()
#define AIM_ON_TARGET_TOLERANCE 0.98f	// dot product

#ifndef DEG2RAD
#define DEG2RAD( x  )  ( (float)(x) * (float)(M_PI / 180.f) )
#endif
#define AIM_RAD2DEG( x ) ( (float)(x) * (float)(180.f / M_PI) )

static float AimAngleNormalize(float angle)
{
	angle = fmodf(angle, 360.0f);
	if (angle > 180.0f)
	{
		angle -= 360.0f;
	}
	else if (angle < -180.0f)
	{
		angle += 360.0f;
	}

	return angle;
}

static float AimAngleDiff(const float destAngle, const float srcAngle)
{
	return AimAngleNormalize(destAngle - srcAngle);
}

static float AimApproachAngle(const float target, float value, float speed)
{
	float delta = AimAngleDiff(target, value);

	if (speed < 0.0f)
		speed = -speed;

	if (delta > speed)
		value += speed;
	else if (delta < -speed)
		value -= speed;
	else
		value = target;

	return AimAngleNormalize(value);
}

static float AimRemapVal(float val, float A, float B, float C, float D)
{
	if (A == B)
		return val >= B ? D : C;
	return C + (D - C) * (val - A) / (B - A);
}

static Vector AimForwardFromAngles(const Vector& angles)
{
	float pitch = DEG2RAD(angles.x);
	float yaw = DEG2RAD(angles.y);

	float sp = sinf(pitch), cp = cosf(pitch);
	float sy = sinf(yaw), cy = cosf(yaw);

	return Vector(cp * cy, cp * sy, -sp);
}

//--------------------------------------------------------------------------------------------------------------
void CTFBotAimInterface::Reset(void)
{
	for (int i = 0; i < LOOK_AT_COUNT; i++)
	{
		m_lookAtPosition[i] = Vector(0, 0, 0);
		m_lookAtSubject[i] = NULL;
		m_lookAtVelocity[i] = Vector(0, 0, 0);
		m_lookAtExpireTimer[i].Invalidate();
		m_lookAtDurationTimer[i].Start();
	}

	m_lookAtPriority = BORING;
	m_isSightedIn = false;
	m_hasBeenSightedIn = false;
	m_lastUpdateTime = gpGlobals->time;
}

bool CTFBotAimInterface::IsAlive(void) const
{
	CBot* player = GetBot();

	if (player)
		return player->IsAlive() != 0;

	return false;
}

Vector CTFBotAimInterface::GetFeet() const
{
	CBot* player = GetBot();

	if (!player)
		return g_vecZero;

	Vector feet = player->pev->origin;
	feet.z = player->pev->absmin.z;

	return feet;
}

Vector CTFBotAimInterface::GetCentroid() const
{
	CBot* player = GetBot();

	return player ? player->Center() : g_vecZero;
}

Vector CTFBotAimInterface::GetEyes() const
{
	CBot* player = GetBot();

	return player ? player->EyePosition() : g_vecZero;
}

float CTFBotAimInterface::GetMoveAngle(void) const
{
	CBot* bot = GetBot();
	if (!bot)
		return 0.0f;

	const Vector& vel = bot->pev->velocity;

	if (vel.Length2D() > 10.0f)
	{
		float yaw = atan2f(vel.y, vel.x) * (180.0f / (float)M_PI);
		if (yaw < 0.0f)
			yaw += 360.0f;
		return yaw;
	}

	return GetFaceAngle();
}

float CTFBotAimInterface::GetFaceAngle(void) const
{
	CBot* bot = GetBot();
	if (!bot)
		return 0.0f;

	return bot->pev->v_angle.y;
}

//--------------------------------------------------------------------------------------------------------------
int CTFBotAimInterface::FindActiveLevel(void) const
{
	for (int i = LOOK_AT_COUNT - 1; i >= 0; i--)
	{
		if (!m_lookAtExpireTimer[i].IsElapsed())
			return i;
	}

	return -1;
}

//--------------------------------------------------------------------------------------------------------------
void CTFBotAimInterface::CreateAimSpot(const Vector& lookAtPos, LookAtPriorityType priority, float duration)
{
	if (!m_bot)
		return;

	int level = (int)priority;
	if (level < 0 || level >= LOOK_AT_COUNT)
		return;

	if (duration <= 0.0f)
		duration = 0.1f;

	// only this priority's slot is touched. higher and lower slots are left alone
	bool active = !m_lookAtExpireTimer[level].IsElapsed();

	CBaseEntity* currentSubject = m_lookAtSubject[level];

	if (active && !currentSubject)
	{
		// same kind of look, just a new spot
		m_lookAtPosition[level] = lookAtPos;
		m_lookAtExpireTimer[level].Start(duration);
		return;
	}

	m_lookAtPosition[level] = lookAtPos;
	m_lookAtSubject[level] = NULL;
	m_lookAtVelocity[level] = Vector(0, 0, 0);
	m_lookAtDurationTimer[level].Start();
	m_lookAtExpireTimer[level].Start(duration);

	if (level == (int)m_lookAtPriority)
	{
		// we replaced the spot we were aiming at
		m_isSightedIn = false;
		m_hasBeenSightedIn = false;
	}
}

void CTFBotAimInterface::CreateAimSpot(CBaseEntity* subject, LookAtPriorityType priority, float duration)
{
	if (!m_bot || !subject)
		return;

	int level = (int)priority;
	if (level < 0 || level >= LOOK_AT_COUNT)
		return;

	if (duration <= 0.0f)
		duration = 0.1f;

	// only this priority's slot is touched. higher and lower slots are left alone
	bool active = !m_lookAtExpireTimer[level].IsElapsed();

	CBaseEntity* currentSubject = m_lookAtSubject[level];

	if (active && currentSubject == subject)
	{
		// already looking at them
		// just keep the spot alive
		m_lookAtExpireTimer[level].Start(duration);
		return;
	}

	m_lookAtPosition[level] = subject->Center();
	m_lookAtSubject[level] = subject;
	m_lookAtVelocity[level] = subject->pev->velocity;
	m_lookAtDurationTimer[level].Start();
	m_lookAtExpireTimer[level].Start(duration);

	if (level == (int)m_lookAtPriority)
	{
		// we replaced the spot we were aiming at
		m_isSightedIn = false;
		m_hasBeenSightedIn = false;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CTFBotAimInterface::Update()
{
	if (!m_bot)
		return;

	float dt = gpGlobals->time - m_lastUpdateTime;
	m_lastUpdateTime = gpGlobals->time;

	if (dt <= 0.0f)
		return;
	if (dt > AIM_MAX_DELTA_T)
		dt = AIM_MAX_DELTA_T;

	CBot* bot = GetBot();

	if (!bot->IsAlive())
		return;

	int level = FindActiveLevel();
	if (level < 0)
	{
		// nothing to look at
		m_isSightedIn = false;
		return;
	}

	if (level != (int)m_lookAtPriority)
	{
		m_lookAtPriority = (LookAtPriorityType)level;
		m_isSightedIn = false;
		m_hasBeenSightedIn = false;
	}

	CBaseEntity* subject = m_lookAtSubject[level];
	if (subject)
	{
		m_lookAtVelocity[level] = subject->pev->velocity;
		m_lookAtPosition[level] = subject->Center();
	}

	Vector currentAngles = bot->pev->v_angle;

	// aim view towards last look at point
	Vector to = m_lookAtPosition[level] - GetEyes();
	float dist = to.Length();

	if (dist < 1.0f)
		return;

	to = to * (1.0f / dist);

	Vector forward = AimForwardFromAngles(currentAngles);
	float dot = DotProduct(forward, to);

	if (dot > AIM_ON_TARGET_TOLERANCE)
	{
		// on target
		m_isSightedIn = true;
		m_hasBeenSightedIn = true;
	}
	else
	{
		// off target
		m_isSightedIn = false;
	}

	float dist2D = sqrtf(to.x * to.x + to.y * to.y);
	float desiredPitch = -AIM_RAD2DEG(atan2f(to.z, dist2D));
	float desiredYaw = AIM_RAD2DEG(atan2f(to.y, to.x));

	float approachRate = AIM_SACCADE_SPEED;

	const float easeOut = 0.7f;
	if (dot > easeOut)
	{
		float t = AimRemapVal(dot, easeOut, 1.0f, 1.0f, 0.02f);
		const float halfPI = 1.57f;
		approachRate *= sinf(halfPI * t);
	}

	const float easeInTime = 0.25f;
	float elapsed = m_lookAtDurationTimer[level].GetElapsedTime();
	if (elapsed < easeInTime)
	{
		approachRate *= elapsed / easeInTime;
	}

	Vector newAngles = currentAngles;
	newAngles.y = AimApproachAngle(desiredYaw, currentAngles.y, approachRate * dt);
	newAngles.x = AimApproachAngle(desiredPitch, currentAngles.x, 0.5f * approachRate * dt);
	newAngles.z = 0.0f;

	// goldsrc doesnt like looking straight up/down
	// this is probably fine but considering this is restricted on most hl1 and 1.6 servers im just gonna do the same
	if (newAngles.x > 89.0f)
		newAngles.x = 89.0f;
	else if (newAngles.x < -89.0f)
		newAngles.x = -89.0f;

	bot->pev->v_angle = newAngles;
	bot->pev->ideal_yaw = newAngles.y;
}
