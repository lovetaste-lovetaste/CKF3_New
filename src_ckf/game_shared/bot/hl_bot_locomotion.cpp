// this is the locomotion system, layered on top of the CImprov system left from Valve
// this kinda mimics the one that the nextbot uses
// the CImprov system already did most of the work, so i didnt need to do much. mainly just give it the ability to crouch and whatnot
// parts of the code is also from source sdk 2013

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "soundent.h"
#include "gamerules.h"
#include "player.h"
#include "client.h"

#include "nav.h"
#include "nav_path.h"
#include "bot_util.h"
#include "improv.h"

#include "hl_bot.h"
#include "hl_bot_locomotion.h"

extern cvar_t cv_bot_stop;

#define BOT_LADDER_FAST_CLIMB 1
// toggles if the bots only use one key or two keys
// 2 keys r faster but the math is weird
// just disable this if it sucks

CTFBotLocomotion::CTFBotLocomotion( CBot* bot )
{
	m_bot = bot;
	m_isCrouching = false;
	m_attemptingLadder = false;
}

bool CTFBotLocomotion::IsAlive(void) const
{
	CBot* player = GetBot();

	if (player)
		return player->IsAlive();

	return false;
}

void CTFBotLocomotion::MoveForward(void)
{
	CBot* player = GetBot();

	if (player)
	{
		player->MoveForward();
	}
}

void CTFBotLocomotion::MoveBackward(void)
{
	CBot* player = GetBot();

	if (player)
	{
		player->MoveBackward();
	}
}

void CTFBotLocomotion::StrafeLeft(void)
{
	CBot* player = GetBot();

	if (player)
	{
		player->StrafeLeft();
	}
}

void CTFBotLocomotion::StrafeRight(void)
{
	CBot* player = GetBot();

	if (player)
	{
		player->StrafeRight();
	}
}

bool CTFBotLocomotion::Jump(void)
{
	CBot* player = GetBot();

	if (player)
	{
		return player->PressJump();
	}

	return false;
}

void CTFBotLocomotion::Crouch(void)
{
	CBot* player = GetBot();

	if (player)
	{
		player->PressDuck();
		m_isCrouching = true;
	}
}

void CTFBotLocomotion::StandUp(void)
{
	CBot* player = GetBot();

	if (player)
	{
		m_isCrouching = false;
	}
}

const Vector& CTFBotLocomotion::GetFeet() const
{
	CBot* player = GetBot();

	if (!player)
		m_feet = g_vecZero;
	else
	{
		Vector feet = player->pev->origin;
		feet.z = player->pev->absmin.z; // idk if this even matters LOLOLOL

		m_feet = feet;
	}

	return m_feet;
}

const Vector& CTFBotLocomotion::GetCentroid() const
{
	CBot* player = GetBot();

	m_centroid = player ? player->Center() : g_vecZero;
	return m_centroid;
}

const Vector& CTFBotLocomotion::GetEyes() const
{
	CBot* player = GetBot();

	m_eyes = player ? player->EyePosition() : g_vecZero;
	return m_eyes;
}

float CTFBotLocomotion::GetMoveAngle(void) const
{
	CBot* bot = GetBot();
	if (!bot)
		return 0.0f;

	const Vector& vel = bot->pev->velocity;

	if (IsMoving())
	{
		float yaw = atan2(vel.y, vel.x) * (180.0f / (float)M_PI);
		if (yaw < 0.0f)
			yaw += 360.0f;
		return yaw;
	}

	return GetFaceAngle();
}

float CTFBotLocomotion::GetFaceAngle(void) const
{
	CBot* bot = GetBot();
	if (!bot)
		return 0.0f;

	return bot->pev->v_angle.y;
}

bool CTFBotLocomotion::IsCrouching(void) const
{
	return m_isCrouching;
}

bool CTFBotLocomotion::IsUsingLadder(void) const
{
	CBot* player = GetBot();

	if (player)
	{
		return player->IsOnLadder();
	}

	return false;
}

bool CTFBotLocomotion::IsJumping(void) const
{
	CBot* player = GetBot();

	if (player)
	{
		return !(player->pev->flags & FL_ONGROUND);
	}

	return false;
}

bool CTFBotLocomotion::IsOnGround(void) const
{
	CBot* player = GetBot();

	if (player)
	{
		return player->pev->flags & FL_ONGROUND;
	}

	return false;
}

bool CTFBotLocomotion::IsMoving(void) const
{
	CBot* player = GetBot();

	if (player)
	{
		return player->pev->velocity.Length2D() > 10.0;
	}
	
	return false;
}

bool CTFBotLocomotion::IsStopped(void) const
{
	return cv_bot_stop.value == 1.0f;
}

bool CTFBotLocomotion::IsVisible(const Vector& pos, bool testFOV) const
{
	CBot* player = GetBot();

	if (!player)
	{
		return false;
	}

	if (testFOV)
	{
		Vector copy = pos;
		if(!player->FInViewCone(&copy))
			return false;
	}

	return (player->FVisible(pos) == TRUE);
}

// from the source sdk
bool CTFBotLocomotion::GetSimpleGroundHeightWithFloor(const Vector* pos, float* height, Vector* normal)
{
	CNavArea* area = GetLastKnownArea();
	if (GetSimpleGroundHeight(pos, height, normal))
	{
		if (area && area->IsOverlapping(pos))
		{
			*height = max((*height), area->GetZ(pos));
			return true;
		}
	}
	return false;
}

CNavArea* CTFBotLocomotion::GetLastKnownArea(void) const
{
	CBot* player = GetBot();

	if (!player)
		return NULL;

	return player->m_lastNavArea;
}

void CTFBotLocomotion::TrackPath(const Vector& pathGoal, float deltaT)
{
	CBot* player = GetBot();

	if (!player)
		return;

	if (player->pev->flags & FL_ONGROUND)
	{
		if (deltaT > StepHeight)
		{
			player->PressDuck();	// to make sure that the jump is an actual crouchjump
			player->PressJump();
		}
	}
	else
	{
		// if airborne, crouch
		player->PressDuck();
	}

	player->MoveTowardPos(pathGoal);
}

void CTFBotLocomotion::StartLadder(const CNavLadder* ladder, NavTraverseType how, const Vector* approachPos, const Vector* departPos)
{
	if(how == GO_LADDER_UP)
		Jump();
	// you can jump onto ladders while on them

	m_attemptingLadder = true;
}

bool CTFBotLocomotion::TraverseLadder(const CNavLadder* ladder, NavTraverseType how, const Vector* approachPos, const Vector* departPos, float deltaT)
{
	// made by jakulo
	// wyt: he sent it to me but he says that it isnt tested
	// it seems fine though LOL

	CBot* player = GetBot();

	if (!player)
	{
		m_attemptingLadder = false;
		return true;
	}

	// returns true if the bot is completely finished with the ladder
	// returns false if the bot is still climbing

	const float climbKeySpeed = 200.0f;		// MAX_CLIMB_SPEED
	const float driftPenalty = 0.5f;		// how much sideways slide costs in the scoring

	Vector vecNormal(0, 0, 0);
	float fraction = 1.0f;

	for (int i = 0; i < 8; ++i)
	{
		float r = (float)(i * M_PI / 4.0);
		Vector probe(cos(r) * 48.0f, sin(r) * 48.0f, 0);

		TraceResult tr;
		UTIL_TraceLine(player->pev->origin, player->pev->origin + probe, ignore_monsters, ENT(player->pev), &tr);

		if (tr.flFraction < fraction)
		{
			fraction = tr.flFraction;
			vecNormal = tr.vecPlaneNormal;
		}
	}

	if (fraction >= 1.0f)
	{
		// freestanding ladder with no wall behind it
		// assume it faces us along our yaw (we walked into it to get on)
		float r = player->pev->v_angle.y * (float)(M_PI / 180.0);
		vecNormal = Vector(-cos(r), -sin(r), 0);
	}
	
	float dz = departPos->z - player->pev->origin.z;
	if (fabs(dz) < JumpCrouchHeight)
	{
		float r = player->pev->v_angle.y * (float)(M_PI / 180.0);
		float facingAway = cos(r) * vecNormal.x + sin(r) * vecNormal.y;

		if (facingAway > 0.0f)
			MoveForward();
		else
			MoveBackward();

		Jump(); // you can jump off ladders to dismount so just use it here
		m_attemptingLadder = false;
		return true;
	}

	float desiredDir = (how == GO_LADDER_UP) ? 1.0f : -1.0f;

	Vector perp = CrossProduct(Vector(0, 0, 1), vecNormal);
	if (perp.NormalizeInPlace() < 0.001f)
	{
		m_attemptingLadder = false;
		return true;
		// degenerate, just ignore
		// This Should Never Happen
	}

	Vector alongLadder = CrossProduct(vecNormal, perp);

#if BOT_LADDER_FAST_CLIMB
	static const int ladderCombos[] =
	{
		IN_FORWARD | IN_MOVELEFT,
		IN_FORWARD | IN_MOVERIGHT,
		IN_BACK | IN_MOVELEFT,
		IN_BACK | IN_MOVERIGHT,
	};
#else
	static const int ladderCombos[] =
	{
		IN_FORWARD,
		IN_BACK,
	};
#endif

	const int comboCount = (int)(sizeof(ladderCombos) / sizeof(ladderCombos[0]));

	float yr = player->pev->v_angle.y * (float)(M_PI / 180.0);
	float sy = sin(yr);
	float cy = cos(yr);

	int bestKey = IN_FORWARD;
	float bestPitch = -60.0f * desiredDir;
	float bestScore = -1.0e9f;

	for (int c = 0; c < comboCount; ++c)
	{
		float F = 0.0f, R = 0.0f;
		if (ladderCombos[c] & IN_FORWARD)   F += climbKeySpeed;
		if (ladderCombos[c] & IN_BACK)      F -= climbKeySpeed;
		if (ladderCombos[c] & IN_MOVERIGHT) R += climbKeySpeed;
		if (ladderCombos[c] & IN_MOVELEFT)  R -= climbKeySpeed;

		for (float flPitch = -85.0f; flPitch <= 85.0f; flPitch += 10.0f)
		{
			float sp = sin(flPitch * (float)(M_PI / 180.0));
			float cp = cos(flPitch * (float)(M_PI / 180.0));

			Vector vpn(cp * cy, cp * sy, -sp);
			Vector vright(sy, -cy, 0);

			Vector velocity = vpn * F + vright * R;
			float flNormal = DotProduct(velocity, vecNormal);
			Vector lateral = velocity - vecNormal * flNormal;
			Vector result = lateral - alongLadder * flNormal;

			// slight preference for pushing INTO the ladder
			// pushing away while standing at its base shoves us off it (onFloor case)
			// this doesnt really matter but it helps regardless
			float score = (result.z * desiredDir) - driftPenalty * (sqrt(result.x * result.x + result.y * result.y)) - ((flNormal > 0.0f) ? 25.0f : 0.0f);

			if (score > bestScore)
			{
				bestScore = score;
				bestKey = ladderCombos[c];
				bestPitch = flPitch;
			}
		}
	}

	if (bestKey & IN_FORWARD)
		MoveForward();
	else
		MoveBackward();

	if (bestKey & IN_MOVELEFT)
		StrafeLeft();
	else
		StrafeRight();

	player->pev->v_angle.x = bestPitch;
	player->pev->v_angle.z = 0;
	// this sets the angle
	// this will be replaced once i make the aim system ( or at least try to make it similar to nextbots version with priorities and whatnot )

	return false;
}