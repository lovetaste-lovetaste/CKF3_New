#ifndef ROOSTER_BOT_AIM_INTERFACE
#define ROOSTER_BOT_AIM_INTERFACE

#include "bot_util.h"

// this is basically grabbed from the source sdk lolol

#define SPOT_RELEVANCE_LENGTH 10.0 // the default length of time that a spot can be relevant

class CBot;

enum LookAtPriorityType
{
	BORING,						// idle / walking
	NORMAL,						// normal stuff
	INTERESTING,				// last known enemy location, dangerous sound location
	IMPORTANT,					// a danger
	CRITICAL,					// an active threat to our safety
	MANDATORY,					// LOOK AT THIS NOW!!!!!!!!!!!

	LOOK_AT_COUNT
};

class CTFBotAimInterface
{
public:
	CTFBotAimInterface(CBot* bot)
	{
		m_bot = bot;
		Reset();
	}

	CBot* GetBot() const { return m_bot; }

	// forget the current aim spot (call on spawn). does NOT touch the bot's view angles
	void Reset(void);

	bool IsAlive(void) const;

	virtual Vector GetFeet(void) const;
	virtual Vector GetCentroid(void) const;
	virtual Vector GetEyes(void) const;
	virtual float GetMoveAngle(void) const;						///< return direction of movement
	virtual float GetFaceAngle(void) const;						///< return direction of view

	// position of the spot we are currently aiming at (the highest priority spot that is still relevant,
	// as of the last Update())
	Vector GetAimPosition() const { return m_lookAtPosition[m_lookAtPriority]; }
	LookAtPriorityType GetAimPriority() const { return m_lookAtPriority; }
	bool IsSightedIn(void) const { return m_isSightedIn; }		///< view is currently on the aim spot
	bool HasBeenSightedIn(void) const { return m_hasBeenSightedIn; }

	void Update();

	// aim the bot's head towards the given goal
	// each priority keeps its own spot, so a low priority spot never overwrites a higher one,
	// it just waits underneath until the higher ones expire
	// calling this every tick with the same priority just refreshes the spot
	virtual void CreateAimSpot(const Vector& lookAtPos, LookAtPriorityType priority = BORING, float duration = SPOT_RELEVANCE_LENGTH);

	// continually aim the bot's head towards the given subject
	virtual void CreateAimSpot(CBaseEntity* subject, LookAtPriorityType priority = BORING, float duration = SPOT_RELEVANCE_LENGTH);

private:
	// highest priority whose spot hasn't expired yet, or -1 if there isn't one
	int FindActiveLevel(void) const;

	CBot* m_bot;

	// one slot per priority. a slot is "relevant" until its expire timer runs out
	Vector m_lookAtPosition[LOOK_AT_COUNT];				// if the slot's subject is valid, it continually overwrites this position with its own
	EHANDLE m_lookAtSubject[LOOK_AT_COUNT];
	Vector m_lookAtVelocity[LOOK_AT_COUNT];				// world velocity of lookat point, for tracking moving subjects
	CountdownTimer m_lookAtExpireTimer[LOOK_AT_COUNT];	// how long until this lookat expired
	IntervalTimer m_lookAtDurationTimer[LOOK_AT_COUNT];	// how long have we been looking at this target

	LookAtPriorityType m_lookAtPriority;	// the priority we are actually aiming at right now (set by Update)

	bool m_isSightedIn;					// true if we are looking at our current lookat target
	bool m_hasBeenSightedIn;			// true if we have hit the current lookat target

	float m_lastUpdateTime;				// for frame-rate independent turning
};

#endif