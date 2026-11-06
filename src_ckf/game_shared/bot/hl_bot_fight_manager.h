#ifndef HLBOTFIGHTMANAGER
#define HLBOTFIGHTMANAGER

// this is meant to allow bots to fight ppl independant on the action it was doing if that fight would help said action
// eg: bot is trying to cap point but there is an enemy in the way. since killing the enemy would help the bot cap the point, the fight manager takes over and controls the bot
// this also helps me seperate the fighting movement as opposed to the pathfinding, so bots can move in a specific way when fighting

class CBot;

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
		m_directionPicked = CLOSEST_DIRECTION;
		m_pickSideTimer.Invalidate();
		m_bWalkToEnemy = true;
	}

	void Update(FightType fightType = FIGHT_CHASE);

	void SetBot(CBot* me) { m_bot = me; }
	void SetFocused(bool focus) { m_bFocused = focus; }

	enum CircleStrafeDirection
	{
		NULL_DIR,
		LEFT,
		RIGHT,
		CLOSEST_DIRECTION,	// go to center
		GO_TO_BACK,
	};

	void MoveAroundTarget(CircleStrafeDirection direction = CLOSEST_DIRECTION, float range = 512.0f, bool walkToEnemyFirst = false);
	// circle strafe around target
	// if walkToEnemyFirst is true, then it doesnt start strafing left/right until it gets in range. otherwise, it strafes at the same time
private:
	CBot* m_bot;
	bool m_bFocused;
	CircleStrafeDirection m_directionPicked;
	bool m_bWalkToEnemy;
	CountdownTimer m_pickSideTimer;
	CountdownTimer m_TimeToKillTimer;
	CountdownTimer m_RocketCheckTimer;
	bool m_bWillWeWin;
	CountdownTimer m_recomputeRetreatSpot;
	CountdownTimer m_recomputeChasePath;
};

#endif