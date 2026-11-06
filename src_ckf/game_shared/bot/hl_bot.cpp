// koroz bot code
// i name it something as if its my own bot, but a good chunk of this is just repurposed nextbot code
// known entity, pathfollower (NOT THE CTFBOTLOCOMOTION, I MADE THAT), and other stuff
// AKA I STOLE VALVE CODE FROM SOURCE SDK 2013
// sorry...
// the action interface is mostly mine though #swag
// although, that is basically a bootleg nextbot action interface............

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "client.h"		// ClientConnect()/ClientPutInServer()
#include "hl_bot.h"
#include "hl_bot_manager.h"
#include "hl_bot_locomotion.h"
#include "hl_bot_aim_interface.h"

#include "bot/actions/tf_bot_idle.h"
#include "bot/actions/tf_bot_seek_and_destroy.h"
#include "bot/actions/tf_bot_control_point_attack.h"
#include "bot/actions/tf_bot_control_point_defend.h"
#include "bot/actions/tf_bot_control_point_moniter.h"
#include "bot/actions/tf_bot_get_health.h"
#include "bot/actions/tf_bot_get_ammo.h"
#include "bot/actions/tf_bot_retreat.h"
#include "bot/actions/tf_bot_medic_heal.h"
#include "bot/actions/tf_bot_engineer.h"

#include "bot_names.h"

#define BOT_NAME_COUNT (sizeof(g_BotNames) / sizeof(g_BotNames[0])) // just in case the name list changes

extern cvar_t cv_bot_debug;
extern cvar_t* g_psv_gravity;

static const float BotCommandInterval = 1.0f / 30.0f;
static const float BotFullThinkInterval = BotCommandInterval * 10.0f; // for every 10 commands, one full think is applied
// just ten ticks though so its fine

CBot::CBot()
	: m_actionInterface(new CTFBotIdle), m_aimInterface(this)
{
	// init for da bots
	
	m_flNextBotThink = m_flNextFullBotThink = m_flPreviousCommandTime = m_forwardSpeed = m_strafeSpeed = m_verticalSpeed = m_flLastWeaponHeard = 0.0f;
	m_buttonFlags = 0;

	m_path.Invalidate();
	m_repathTimer.Invalidate();
	m_stuckTimer.Start();
	m_hEnemy = NULL;

	m_pathfollower.SetImprov( new CTFBotLocomotion(this) );
	m_pathfollower.SetPath(&m_path);
	m_actionInterface.SetBot(this);
	m_actionInterface.m_scenarioMoniterInterface.SetBot(this); // just in case
	m_actionInterface.m_scenarioMoniterInterface.SetHealthAction( new CTFBotGetHealth );
	m_actionInterface.m_scenarioMoniterInterface.SetRetreatAction( new CTFBotRetreat );
	m_actionInterface.m_scenarioMoniterInterface.SetAmmoAction( new CTFBotGetAmmo );
	m_actionInterface.m_scenarioMoniterInterface.SetWinLostActions(new CTFBotSeekAndDestroy, new CTFBotRetreat);
	m_actionInterface.m_fightManager.SetBot(this);
}
//void SelectDesiredWeapon()
CTFAction* CBot::DesiredAction(int forceclass)
{
	int iClass = (forceclass && forceclass <= CLASS_SPY && forceclass >= CLASS_SCOUT) ? forceclass : m_iClass;

	if (iClass == CLASS_MEDIC)
		return new CTFBotMedicHeal;

	if (iClass == CLASS_ENGINEER)
		return new CTFBotEngineerAction;

	if (g_pGameRules->CPExist())
	{
	// CONSOLE_ECHO("DesiredAction is control point related\n");
	
	//	if(g_pGameRules->m_ControlPoints.Count() == 1)
	//	{
			// assume that it is koth related
	//		return new CTFBotControlPointAttackKoth;
	//	}
		// doesnt matter

	//	if (m_iClass != CLASS_ENGINEER)
	//		return new CTFBotControlPointAttack;
	//	else
			return new CTFBotControlPointMoniter; // handles both attacking and defending
	}

	return new CTFBotSeekAndDestroy;
}

int CBot::ChooseGoodClass(void)
{
	static int offenseRoster[] =
	{
		CLASS_ENGINEER,
		CLASS_SOLDIER,
		CLASS_MEDIC,
		CLASS_HEAVY,
		CLASS_DEMOMAN,
		CLASS_SCOUT,

		CLASS_PYRO,
		CLASS_SOLDIER,
		CLASS_DEMOMAN,
		CLASS_SNIPER,
		CLASS_MEDIC,
		CLASS_SPY,
	};

	static int deathmatchRoster[] =
	{
		CLASS_SOLDIER,
		CLASS_SCOUT,
		CLASS_DEMOMAN,
		CLASS_SCOUT,
		CLASS_SOLDIER,
		CLASS_SNIPER,

		CLASS_PYRO,
		CLASS_SOLDIER,
		CLASS_ENGINEER,
		CLASS_MEDIC,
		CLASS_SPY,
		CLASS_HEAVY,
	};

	/*
	static int defenseRoster[] =
	{
		CLASS_MEDIC,
		CLASS_ENGINEER,
		CLASS_ENGINEER,
		CLASS_HEAVY,
		CLASS_MEDIC,
		CLASS_DEMOMAN,

		CLASS_SOLDIER,
		CLASS_SNIPER,
		CLASS_SOLDIER,
		CLASS_PYRO,
		CLASS_SCOUT,
		CLASS_SPY,
	};

	not needed
	*/

	// assume offense
	int* desiredRoster = offenseRoster;

	if (!g_pGameRules->CPExist())
		desiredRoster = deathmatchRoster;

	// count classes in use by my team, not including me
	CCountClassMembers currentRoster(this, m_iTeam);
	ForEachPlayer(currentRoster);

	int classCount[9] = { 0,0,0,0,0,0,0,0,0 };
	for (int i = 0; i < 12; ++i)
	{
		if (currentRoster.m_count[desiredRoster[i]] > classCount[desiredRoster[i]])
		{
			// if we have enough of this class, skip it
			classCount[desiredRoster[i]]++;
		}
		else
		{
			return desiredRoster[i];
		}
	}

	return 0;
}

void CBot::UpdateKnownEntities(void)
{
	// construct set of potentially visible objects
	CUtlVector< CBaseEntity* > potentiallyVisible;
	CollectPotentiallyVisibleEntities(&potentiallyVisible);

	// collect set of visible and recognized entities at this moment
	CollectVisible visibleNow(this);
	FOR_EACH_VEC(potentiallyVisible, pit)
	{
		if (visibleNow(potentiallyVisible[pit]) == false)
			break;
	};

	// update known set with new data
	{
		// Mark obsolete entities for removal instead of removing immediately
		CUtlVector<int> indicesToRemove;

		int i;
		for (i = 0; i < m_knownEntityVector.Count(); ++i)
		{
			CKnownEntity& known = m_knownEntityVector[i];

			// clear out obsolete knowledge
			if (known.GetEntity() == NULL || known.IsObsolete())
			{
				indicesToRemove.AddToTail(i);
				continue;
			}

			if (visibleNow.Contains(known.GetEntity()))
			{
				// this visible entity was already known (but perhaps not visible until now)
				known.UpdatePosition();
				known.UpdateVisibilityStatus(true);

				// has our reaction time just elapsed?
				/*
				if (gpGlobals->time - known.GetTimeWhenBecameVisible() >= 0.0f &&
					m_lastVisionUpdateTimestamp - known.GetTimeWhenBecameVisible() < 0.0f)
				{
					// OnSight(known.GetEntity());
					// this will probably be with the action interface
				}
				*/

				// restart 'not seen' timer
				
				int team = known.GetTeam();

				if (team != -1 && (team == TEAM_RED || team == TEAM_BLUE))
					m_notVisibleTimer[team].Start();
			}
			else // known entity is not currently visible
			{
				if (known.IsVisibleInFOVNow())
				{
					// previously known and visible entity is now no longer visible
					known.UpdateVisibilityStatus(false);

					// lost sight of this entity
					// OnLostSight(known.GetEntity());
					// again, probably with the action interface
				}

				if (!known.HasLastKnownPositionBeenSeen())
				{
					// can we see the entity's last know position?
					if (m_pathfollower.GetImprov()->IsVisible(known.GetLastKnownPosition()))
					{
						known.MarkLastKnownPositionAsSeen();
					}
				}
			}
		}

		// Remove marked entities in reverse order to preserve indices
		for (int removeIdx = indicesToRemove.Count() - 1; removeIdx >= 0; --removeIdx)
		{
			m_knownEntityVector.Remove(indicesToRemove[removeIdx]);
		}
	}

	// check for new recognizes that were not in the known set
	int i, j;
	for (i = 0; i < visibleNow.m_recognized.Count(); ++i)
	{
		for (j = 0; j < m_knownEntityVector.Count(); ++j)
		{
			if (visibleNow.m_recognized[i] == m_knownEntityVector[j].GetEntity())
			{
				break;
			}
		}

		if (j == m_knownEntityVector.Count())
		{
			// recognized a previously unknown entity (emit OnSight() event after reaction time has passed)
			CKnownEntity known(visibleNow.m_recognized[i]);
			known.UpdatePosition();
			known.UpdateVisibilityStatus(true);
			m_knownEntityVector.AddToTail(known);
		}
	}
}

const CKnownEntity* CBot::GetPrimaryKnownThreat(bool onlyVisibleThreats)
{
	if (m_knownEntityVector.Count() == 0)
	{
		m_hEnemy = NULL;
		return NULL;
	}

	const CKnownEntity* threat = NULL;
	int i;

	// find the first valid entity
	for (i = 0; i < m_knownEntityVector.Count(); ++i)
	{
		int team = m_knownEntityVector[i].GetTeam();
		bool enemy = team != -1 && team != m_iTeam;

		// check in case status changes between updates
		if ( (m_knownEntityVector[i].GetTimeSinceBecameKnown() >= 0.0f) && !m_knownEntityVector[i].IsObsolete() && enemy )
		{
			if (!onlyVisibleThreats || m_knownEntityVector[i].IsVisibleRecently())
			{
				threat = &m_knownEntityVector[i];
				break;
			}
		}
	}

	if (threat == NULL)
	{
		m_hEnemy = NULL;
		return NULL;
	}

	for (++i; i < m_knownEntityVector.Count(); ++i)
	{
		int team = m_knownEntityVector[i].GetTeam();
		bool enemy = team != -1 && team != m_iTeam;

		// check in case status changes between updates
		if (m_knownEntityVector[i].GetTimeSinceBecameKnown() >= 0.0f && !m_knownEntityVector[i].IsObsolete() && enemy)
		{
			if (!onlyVisibleThreats || m_knownEntityVector[i].IsVisibleRecently())
			{
				if( (m_knownEntityVector[i].GetLastKnownPosition() - pev->origin).IsLengthLessThan( (threat->GetLastKnownPosition() - pev->origin).Length() ) )
					threat = &m_knownEntityVector[i];
				// SelectMoreDangerousThreat has not been implemented yet...
			}
		}
	}

	// cache off threat
	m_hEnemy = threat ? threat->GetEntity() : NULL;

	return threat;
}

const CKnownEntity* CBot::GetPrimaryKnownTeammate(bool onlyVisibleThreats)
{
	if (m_knownEntityVector.Count() <= 0)
	{
		return NULL;
	}

	const CKnownEntity* threat = NULL;
	float bestHealth = FLT_MAX;

	// find the first valid entity
	for (int i = 0; i < m_knownEntityVector.Count(); ++i)
	{
		const CKnownEntity& firstThreat = m_knownEntityVector[i];
		CBaseEntity* ent = firstThreat.GetEntity();
		if (!ent || !ent->IsPlayer())
			continue;

		CBasePlayer* teammate = (CBasePlayer*)ent;
		if (teammate->m_iHealerCount > 0)
			continue;

		bool isTeammate = firstThreat.GetTeam() == m_iTeam;

		if (!isTeammate)
		{
			if (teammate->IsPlayerDisguised() && teammate->m_iDisguiseTeam == m_iTeam)
				isTeammate = true;
		}

		// check in case status changes between updates
		if ((firstThreat.GetTimeSinceBecameKnown() >= 0.0f) && !firstThreat.IsObsolete() && isTeammate)
		{
			if (!onlyVisibleThreats || firstThreat.IsVisibleRecently())
			{
				float hp = firstThreat.GetEntity()->pev->health;
				if (hp < bestHealth)
				{
					bestHealth = hp;
					threat = &firstThreat;
				}
			}
		}
	}

	return threat;
}

CBot* CBot::CreateBot(const char* name, bool quotaManaged)
{
	if (UTIL_ClientsInGame() >= gpGlobals->maxClients)
	{
		CONSOLE_ECHO("bot_add: Server is full.\n");
		return NULL;
	}

	// pick a default name if none was given (or the given one is taken)
	char botName[64];
	if (name && *name && !UTIL_IsNameTaken(name))
	{
		strncpy(botName, name, sizeof(botName) - 1);
		botName[sizeof(botName) - 1] = '\0';
	}
	else
	{	
		do
		{
			//sprintf(botName, "Bot%02d", serial++);
			sprintf(botName, "%s", g_BotNames[RANDOM_LONG(0, BOT_NAME_COUNT - 1)]);
			// this SHOULD have a indent thingy so that the random is not needed
			// however, the chance of a random bot getting the same name as another one has is literally less than 1/7000, so its whatever
			// plus the while loop will make sure same names dont happen anyway
		} while (UTIL_IsNameTaken(botName));
	}

	edict_t* pent = CREATE_FAKE_CLIENT(botName);
	if (FNullEnt(pent))
	{
		CONSOLE_ECHO("bot_add: pfnCreateFakeClient() failed - no free client slots?\n");
		return NULL;
	}

	// run the normal connection path
	char reject[128];
	if (0 == ClientConnect(pent, STRING(pent->v.netname), "127.0.0.1", reject))
	{
		SERVER_COMMAND(UTIL_VarArgs("kick \"%s\"\n", STRING(pent->v.netname)));
		return NULL;
	}

	FREE_PRIVATE(pent);
	CBot* bot = GetClassPtr((CBot*)VARS(pent));

	ClientPutInServer(pent);

	bot->m_bQuotaManaged = quotaManaged;
	bot->m_kickStatus = NOT_KICKED;

	return bot;
}

void CBot::Spawn(void)
{
	// make sure to do econ shit before the Spawn() thingy!!!!!!!!!!!!!!!!!!!!!!!!!!!
	m_iLoadoutEconIndex[CLASS_SOLDIER][0] = RANDOM_LONG(0, 1) == 1 ? ITEM_ROCKETLAUNCHER : ITEM_BLACKBOX;


	// let the game set everything up
	CBasePlayer::Spawn();

	pev->flags |= (FL_CLIENT | FL_FAKECLIENT);
	SetThink(NULL);
	pev->nextthink = -1;
	m_flNextBotThink = gpGlobals->time + BotCommandInterval;
	m_flNextFullBotThink = gpGlobals->time + RANDOM_FLOAT(0, BotFullThinkInterval);
	m_flPreviousCommandTime = gpGlobals->time;
	m_path.Invalidate();
	m_pathGoal = pev->origin;
	m_repathTimer.Invalidate();
	m_stuckSpot = pev->origin;
	m_stuckTimer.Start();
	m_hEnemy = NULL;
	m_aimInterface.Reset();
	m_actionInterface.Reset(DesiredAction());
	ResetCommand();
	m_knownEntityVector.RemoveAll();
	m_flLastWeaponHeard = -1.0f;
}

void CBot::Think(void)
{
	if (m_kickStatus != NOT_KICKED)
		return;

	if (gpGlobals->time < m_flNextBotThink)
		return;

	m_flNextBotThink = gpGlobals->time + BotCommandInterval;

	if (m_iTeam != TEAM_BLU && m_iTeam != TEAM_RED)
	{
		HandleMenu_ChooseTeam(this, 0);
		ResetCommand();
		ExecuteCommand();
		return;
		//CONSOLE_ECHO("%s is trying to pick a team.\n", STRING(pev->netname));
		// if not on a team, bots automatically pick the team
	}

	if (!(CLASS_SPY >= m_iClass && m_iClass >= CLASS_SCOUT))
	{
		HandleMenu_ChooseClass(this, 0);
		ResetCommand();
		ExecuteCommand();
		return;
		//CONSOLE_ECHO("%s is trying to pick a class.\n", STRING(pev->netname));
	}
	else if(m_hReevaluateClassTimer.IsElapsed())
	{
		m_hReevaluateClassTimer.Start(RANDOM_FLOAT(1.0f,3.0f));
		int new_class = ChooseGoodClass();
		NavAttributeType spawn = m_iTeam == TEAM_BLUE ? NAV_SPAWN_ROOM_BLUE : NAV_SPAWN_ROOM_RED;
		if (m_iClass != new_class && ( (m_lastNavArea && (m_lastNavArea->GetAttributes() & spawn)) || !IsAlive() ) )
		{
			HandleMenu_ChooseClass(this, ChooseGoodClass());
			ResetCommand();
			ExecuteCommand();
			return;
		}
	}

	ResetCommand();
	if (!TheNavAreaGrid.IsNavmeshValid()) // this disables bots if there is no navmesh to work with, to prevent certain errors
	{
		ExecuteCommand();
		m_path.Invalidate();
		// CONSOLE_ECHO("NO NAVMESH!");
		return;
	}

	if (!IsAlive())
	{
		ExecuteCommand();
		m_path.Invalidate();
		// CONSOLE_ECHO("%s is dead!", STRING(pev->netname));
		return;
	}

	if (gpGlobals->time >= m_flNextFullBotThink)
	{
		if (TheHLBots->CheckUpdateLimit()) // if there have been too much updates this cycle, then update the next tick
		{
			m_flNextFullBotThink = gpGlobals->time + BotFullThinkInterval;
			Update();
		}

		// a potential bug with this is that bots at the end of the playerlist could be stalled forever due to them being last in line
		// this would only happen if the bots all spawned at the same time and the BotFullThinkInterval was fast enough to catch up with the updatelimit
		// doesnt happen with default settings though, so whatever.....
	}

	// maintains inbetween stuff
	Upkeep();
	
	ExecuteCommand();
}

void CBot::Update(void)
{
	UpdateStuckMonitor();
	if (m_hEnemy)
	{
		CBaseEntity* e = m_hEnemy;
		if (!e || FNullEnt(e->pev) || !e->IsAlive())
			m_hEnemy = NULL;

		// this shouldnt need to happen now that we ported the known entity system
	}

	if(m_hKnownEntityRecalculateTimer.IsElapsed())
	{
		UpdateKnownEntities();
		GetPrimaryKnownThreat(); // caches m_hEnemy in here

		/*
		CClosestTFPlayer closest(this, GetEnemyTeam());
		ForEachPlayer(closest);

		if (closest.m_closePlayer)
			m_hEnemy = closest.m_closePlayer;

			simple look, do not use this anymore
		*/

		m_hKnownEntityRecalculateTimer.Start(RANDOM_FLOAT(0.2f, 0.6f));
	}

	m_actionInterface.Debug(cv_bot_debug.value == 1.0);
	m_actionInterface.Update();
}

static void Bot_LookAt(CBaseEntity* self, const Vector& point)
{
	// this is temporary!!!!!

	Vector to = point - (self->pev->origin + self->pev->view_ofs);

	if (to.Length() < 1.0f)
		return; // point is on the eye, avoid a zero vector

	float dist2D = sqrtf(to.x * to.x + to.y * to.y);

	self->pev->v_angle.x = -(float)(atan2(to.z, dist2D) * 180.0 / M_PI); // pitch
	self->pev->v_angle.y = UTIL_VecToYaw(to);                            // yaw
	self->pev->v_angle.z = 0.0f;
	self->pev->ideal_yaw = self->pev->v_angle.y;
}

void CBot::Upkeep(void)
{
	if (pev->waterlevel >= 3)
	{
		// if underwater, get up
		PressJump();
	}

	float aimDuration = BotFullThinkInterval;
	const CKnownEntity* threat = GetPrimaryKnownThreat();
	CBaseEntity* enemy = threat ? threat->GetEntity() : NULL;
	bool enemyfound = enemy && HasEnemy();

	if (m_iClass == CLASS_MEDIC)
	{
		const CKnownEntity* teammate = GetPrimaryKnownTeammate();
		CBaseEntity* mate = teammate ? teammate->GetEntity() : NULL;

		if (mate && mate->IsPlayer())
		{
			CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)m_pActiveItem;

			if (!pWeapon)
				return;

			if (pWeapon->m_iId != WEAPON_MEDIGUN)
				SwitchSlotWeapon(WEAPON_SLOT_SECONDARY);
			else
			{
				if (m_iUbercharge >= 100 && (enemyfound || pev->health < (pev->max_health * 0.5)))
					PressSecondaryAttack();
				else
					PressPrimaryAttack();
			}

			if (m_pathfollower.GetImprov()->IsVisible(mate->Center()))
				m_aimInterface.CreateAimSpot(mate, INTERESTING, aimDuration);
			else
				m_aimInterface.CreateAimSpot(teammate->GetLastKnownPosition(), INTERESTING, aimDuration);
		}
	}
	
	if (enemyfound)
	{
		LookAtPriorityType aimPriority;
		Vector m_aimPosition = pev->origin;

		if (m_pathfollower.GetImprov()->IsVisible(enemy->Center()))
		{
			aimPriority = CRITICAL;
			m_aimPosition = enemy->Center();
		}
		else
		{
			aimPriority = IMPORTANT;
			m_aimPosition = threat->GetLastKnownPosition();
		}
		// this seem impractical

		
		CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)m_pActiveItem;

		if (pWeapon)
		{
			switch (m_pActiveItem->m_iId)
			{
				case WEAPON_SNIPERIFLE:
				{
					Vector head = enemy->EyePosition();
					if (m_pathfollower.GetImprov()->IsVisible(head))
						m_aimPosition = head;

					CSniperifle* pSniperRifle = (CSniperifle*)pWeapon;

					if (m_rgAmmo[pSniperRifle->m_iPrimaryAmmoType] > 0)
					{
						if (!(pWeapon->m_iWeaponState & WEAPONSTATE_CHARGING))
						{
							PressSecondaryAttack();
						}
						else if (pSniperRifle && pSniperRifle->m_fCharge > 10 && m_aimInterface.IsSightedIn())
						{
							PressPrimaryAttack();
						}
					}
					break;
				}
				case WEAPON_MINIGUN:
				{
					CMinigun* pMinigun = (CMinigun*)pWeapon;

					if (m_rgAmmo[pMinigun->m_iPrimaryAmmoType] > 0)
					{
						if (m_pathfollower.GetImprov()->IsVisible(enemy->Center()) && m_aimInterface.IsSightedIn())
							PressPrimaryAttack();
						// jakulo: this specifically does NOT use GetLastKnownPosition so that bots dont shoot at targets that they do not see anymore
						// eg: bot is aware of someone but they just lost sight of them, meaning that the last known position is right before they lost track
						// since they can still SEE the last known, they keep shooting at that point even if nobody is there
						// the Center() check makes it so that bots keep aiming, but they dont actually shoot at it
						// hastag REALISM!!!!!!!!!!!!!!!!!!!!!!!!!
						else
							PressSecondaryAttack();
					}
					break;
				}
				case WEAPON_BAT:
				case WEAPON_SHOVEL:
				case WEAPON_FIREAXE:
				case WEAPON_BOTTLE:
				case WEAPON_FIST:
				case WEAPON_WRENCH:
				case WEAPON_BONESAW:
				case WEAPON_KUKRI:
				{
					if ((pev->origin - m_aimPosition).IsLengthLessThan(48.0f))
					{
						PressPrimaryAttack();
					}
					// if close enough, then attack
					// TODO: swing prediction maybe?
					// TODO: use weapon range

					break;
				}
				case WEAPON_BUTTERFLY:
				{
					CButterfly* pKnife = (CButterfly*)pWeapon;
					if (pKnife && (pKnife->IsBackFace(pev->angles, enemy->pev->angles) || ( (pev->origin - m_aimPosition).IsLengthLessThan(48.0f) && enemy->Classify() != CLASS_PLAYER)))
					{
						PressPrimaryAttack();
					}
					// only swings if the spy can backstab
					// if the enemy is not a player, then the spy just butterknifes it

					break;
				}
				case WEAPON_STICKYLAUNCHER:
				{
					CStickyLauncher* pStickybombLauncher = (CStickyLauncher*)pWeapon;
					if (pStickybombLauncher)
					{
						if (!(pWeapon->m_iWeaponState & WEAPONSTATE_CHARGING))
							PressPrimaryAttack();
						else
							PressSecondaryAttack();
					}
					// TODO: this only lets the demo sticky spam. eventually, this should have charging based off the player distance
					break;
				}
				case WEAPON_FLAMETHROWER:
				{
					CFlamethrower* pFlamethrower = (CFlamethrower*)pWeapon;
					if (m_rgAmmo[pFlamethrower->m_iPrimaryAmmoType] > 0 && (pev->origin - m_aimPosition).IsLengthLessThan(1024.0f) && m_aimInterface.HasBeenSightedIn())
						PressPrimaryAttack();

					break;
				}
				default:
				{
					if (pWeapon->m_iClip > 0 && m_aimInterface.IsSightedIn())
					{
						PressPrimaryAttack();
					}
					break;
				}
			}
		}

		m_aimInterface.CreateAimSpot(m_aimPosition, aimPriority, aimDuration);
	}

	/*
	if(HasPath())
	{
		m_pathfollower.FindPathPoint(512.0f, &m_aimPosition, NULL);

		// TODO: use GetSpotEncounter
	}
	// for some reason this breaks the pathfinding
	// TODO: fix this shit dawg
	*/

	const float minMoveSpeed = 1.0f;
	// below this we count as standing still
	// super small so that it keeps the vel stuff until we are REALLY still
	Vector point = pev->origin;
	Vector eyePos = EyePosition();
	Vector horizVel = pev->velocity;
	horizVel.z = 0.0f;
	Vector lookDir;
	if (horizVel.Length() > minMoveSpeed)
	{
		// face the direction we're travelling
		// on paths this SHOULD produce a natural look ahead without needing to compute it from the path
		// this might look weird when the bot stops though, so eventually the path itself should be used
	}
	else
	{
		// standing still
		// just face where ur already looking but flat in the horizon
		float yawRad = pev->v_angle.y * (float)(M_PI / 180.0);
		lookDir = Vector(cos(yawRad), sin(yawRad), 0.0f);
	}

	point = eyePos + lookDir * 128.0f;
	point.z = eyePos.z;
	m_aimInterface.CreateAimSpot(point, BORING, aimDuration);

	// actions can call m_aimInterface.CreateAimSpot() themselves with a higher priority (IMPORTANT/MANDATORY) to override this
	m_aimInterface.Update();
	m_pathfollower.Debug(cv_bot_debug.value == 1.0);
	m_pathfollower.Update(BotCommandInterval);

	if (m_actionInterface.ShouldWeFight())
		m_actionInterface.m_fightManager.Update(FIGHT_DUEL);
}

enum OnEventWeaponSoundPriority
{
	SND_SILENT,
	SND_QUIET,
	SND_NORMAL,
	SND_LOUD,
	SND_VERY_LOUD,

	SND_COUNT
};

#define WEAPON_HEARING_TIME 1.0f // how long a bot's hearing becomes muffled after hearing a gunshot

void CBot::OnEvent(GameEventType event, CBaseEntity* entity, CBaseEntity* other)
{
	if (!event)
		return;

	switch (event)
	{
	case EVENT_WEAPON_FIRED:
	{
		if (!other || !other->IsPlayer()) return;

		CBasePlayer* pPlayer = (CBasePlayer*)other;

		int enteam = pPlayer->IsPlayerDisguised() ? pPlayer->m_iDisguiseTeam : pPlayer->m_iTeam;
		if (enteam == m_iTeam) // nobody cares about TEAMMATES!!!!!!!!!!!!!!!!!!
			return;

		CBasePlayerWeapon* pWeapon = (CBasePlayerWeapon*)entity;
		if (!pWeapon)
			return;

		OnEventWeaponSoundPriority sndwpnpriority = SND_SILENT;

		switch (pWeapon->m_iId)
		{
		case WEAPON_BAT:
		case WEAPON_SHOVEL:
		case WEAPON_FIREAXE:
		case WEAPON_BOTTLE:
		case WEAPON_FIST:
		case WEAPON_WRENCH:
		case WEAPON_BONESAW:
		case WEAPON_KUKRI:
		case WEAPON_BUTTERFLY:
		{
			sndwpnpriority = SND_QUIET;
			break;
		}
		case WEAPON_SNIPERIFLE:
		case WEAPON_SMG:
		case WEAPON_REVOLVER:
		case WEAPON_FLAMETHROWER:
		case WEAPON_PISTOL:
		case WEAPON_SYRINGEGUN:
		{
			sndwpnpriority = SND_NORMAL;
			break;
		}
		case WEAPON_SCATTERGUN:
		case WEAPON_SHOTGUN:
		{
			sndwpnpriority = SND_LOUD;
			break;
		}
		case WEAPON_STICKYLAUNCHER:
		case WEAPON_GRENADELAUNCHER:
		case WEAPON_MINIGUN:
		case WEAPON_ROCKETLAUNCHER:
		{
			sndwpnpriority = SND_VERY_LOUD;
			break;
		}
		default:break;
		}

		if (sndwpnpriority <= SND_SILENT)
			return;

		if (m_flLastWeaponHeard + WEAPON_HEARING_TIME > gpGlobals->time)
			sndwpnpriority = (OnEventWeaponSoundPriority)(sndwpnpriority / 2);

		float distance = (pPlayer->pev->origin - pev->origin).Length();

		if (distance < (sndwpnpriority * 512.0f))
		{
			// we heard a weapon!!!

			m_flLastWeaponHeard = gpGlobals->time; // ear is muffled
			m_aimInterface.CreateAimSpot(pPlayer->GetGunPosition(), INTERESTING, 30.0f);
		}

		break;
	}
	//	case EVENT_PLAYER_FOOTSTEP: this is not worth it as of right now lol. this will probably be readded, but for now just leave this commented
	case EVENT_PLAYER_VOICELINE:
	{
		if (entity && entity->IsPlayer() && FVisible(entity))
		{
			CBasePlayer* pPlayer = (CBasePlayer*)entity;
			int myteam = m_iTeam;
			int enteam = pPlayer->IsPlayerDisguised() && pPlayer->m_iDisguiseTeam != myteam ? pPlayer->m_iDisguiseTeam : pPlayer->m_iTeam;

			if(enteam != myteam)
				m_aimInterface.CreateAimSpot(entity->Center(), (entity->Center() - Center()).IsLengthLessThan(256.0f) ? INTERESTING : NORMAL);
		}
		break;
	}
	case EVENT_PLAYER_SCREAMED:
	{
		if (!entity) break;

		m_aimInterface.CreateAimSpot(entity->Center(), IMPORTANT);
		break;
	}
	case EVENT_INVALID:
	default:
		break;
	}
}

void CBot::SetPathToGoal(CBasePlayer *goal, RouteType routeType)
{
	if (!goal)
		return;

	SetPathToGoal(goal->Center(), routeType);
}

void CBot::SetPathToGoal(CBaseEntity* goal, RouteType routeType)
{
	if (!goal)
		return;

	SetPathToGoal(goal->pev->origin, routeType);
}

void CBot::SetPathToGoal(const Vector& goal, RouteType routeType)
{
	const float goalMovedTolerance = 240.0f;
	// repath if a mobile goal strays this far from the path end

	bool needPath = !m_path.IsValid();

	if (!needPath && m_repathTimer.IsElapsed())
		needPath = true;
	// prevents spam

	if (!needPath && (goal - m_pathGoal).IsLengthGreaterThan(goalMovedTolerance))
		needPath = true;
	// if the origin has moved too much, then change this
	// this allows for path to remain the same if the new goal is close to the old one, to save fps

	if (needPath)
	{
		ShortestPathCost cost(this, routeType);
		Vector start = pev->origin;

		if (m_path.Compute(&start, &goal, cost))
		{
			m_pathfollower.Reset();
			m_pathGoal = goal;
		}
		else
		{
			m_path.Invalidate();
		}

		// don't repath every think even on failure
		m_repathTimer.Start(RANDOM_FLOAT(3.0f, 4.5f));
	}
}

void CBot::ResetCommand(void)
{
	m_forwardSpeed = 0.0f;
	m_strafeSpeed = 0.0f;
	m_verticalSpeed = 0.0f;
	m_buttonFlags = 0;
}

void CBot::ExecuteCommand(void)
{
	byte adjustedMSec = ThrottledMsec();

	if (cv_bot_stop.value != 0.0f)
	{
		ResetCommand();
	}
	else
	{
		// player model angles are "munged" from view angles
		pev->angles = pev->v_angle;
		pev->angles.x /= -3.0f;
	}

	if (cv_bot_stop_attack.value != 0.0f)
		ClearBits(m_buttonFlags, IN_ATTACK);

	m_flPreviousCommandTime = gpGlobals->time;

	(*g_engfuncs.pfnRunPlayerMove)(edict(), pev->v_angle, m_forwardSpeed, m_strafeSpeed, m_verticalSpeed,
		m_buttonFlags, 0, adjustedMSec);
}

//-------------------------------------------------------------------------------------------------------------- 
byte CBot::ThrottledMsec(void) const
{
	// estimate msec for this command from the time since the previous one
	int newMsec = (int)((gpGlobals->time - m_flPreviousCommandTime) * 1000.0f);
	if (newMsec > 255)
		newMsec = 255;

	return (byte)newMsec;
}

void CBot::UpdateStuckMonitor(void)
{
	const float movedTolerance = 30.0f;
	const float jumpAfter = 1.0f;
	const float repathAfter = 3.0f;

	if ((pev->origin - m_stuckSpot).IsLengthGreaterThan(movedTolerance))
	{
		// we are moving fine
		m_stuckSpot = pev->origin;
		m_stuckTimer.Start();
		return;
	}

	if (m_stuckTimer.IsGreaterThen(repathAfter))
	{
		m_path.Invalidate();
		m_repathTimer.Invalidate();
		m_stuckSpot = pev->origin;
		m_stuckTimer.Start();
	}
	else if (m_stuckTimer.IsGreaterThen(jumpAfter))
		PressJump();
}

// UTILITY FUNCTIONS!!
//--------------------------------------------------------------------------------------------------------------
float CBot::GetMoveSpeed(void) const
{
	return pev->maxspeed;
}

//--------------------------------------------------------------------------------------------------------------
void CBot::MoveForward(void)
{
	m_forwardSpeed = GetMoveSpeed();
	SetBits(m_buttonFlags, IN_FORWARD);
	ClearBits(m_buttonFlags, IN_BACK);
}

//--------------------------------------------------------------------------------------------------------------
void CBot::MoveBackward(void)
{
	m_forwardSpeed = -GetMoveSpeed();
	SetBits(m_buttonFlags, IN_BACK);
	ClearBits(m_buttonFlags, IN_FORWARD);
}

//--------------------------------------------------------------------------------------------------------------
void CBot::StrafeLeft(void)
{
	m_strafeSpeed = -GetMoveSpeed();
	SetBits(m_buttonFlags, IN_MOVELEFT);
	ClearBits(m_buttonFlags, IN_MOVERIGHT);
}

//--------------------------------------------------------------------------------------------------------------
void CBot::StrafeRight(void)
{
	m_strafeSpeed = GetMoveSpeed();
	SetBits(m_buttonFlags, IN_MOVERIGHT);
	ClearBits(m_buttonFlags, IN_MOVELEFT);
}

//--------------------------------------------------------------------------------------------------------------
bool CBot::PressJump(void)
{
	SetBits(m_buttonFlags, IN_JUMP);
	return true;
}

//--------------------------------------------------------------------------------------------------------------
void CBot::PressDuck(void)
{
	SetBits(m_buttonFlags, IN_DUCK);
}

//--------------------------------------------------------------------------------------------------------------
void CBot::PressPrimaryAttack(void)
{
	SetBits(m_buttonFlags, IN_ATTACK);
}

//--------------------------------------------------------------------------------------------------------------
void CBot::PressSecondaryAttack(void)
{
	SetBits(m_buttonFlags, IN_ATTACK2);
}

//--------------------------------------------------------------------------------------------------------------

void CBot::MoveTowardPos(const Vector& pos)
{
	//CONSOLE_ECHO("MoveTowardsPos\n");

	float dx = pos.x - pev->origin.x;
	float dy = pos.y - pev->origin.y;

	if (dx * dx + dy * dy < 1.0f)
		return;		// standing on it - nothing to do

	// flip Y so the rotation below lands in the engine's move space
	// (world yaw turns counter-clockwise, sidemove is positive-right)
	dy = -dy;

	float flRad = pev->v_angle.y * (float)(M_PI / 180.0);
	float s = sin(flRad);
	float c = cos(flRad);

	// rotate: x = along the view (forwardmove), y = across it (sidemove)
	float flForward = c * dx - s * dy;
	float flSide = s * dx + c * dy;

	// normalize so distance to the goal doesn't scale the speed
	float flLen = sqrt(flForward * flForward + flSide * flSide);
	if (flLen < 0.001f)
		return;

	flForward /= flLen;
	flSide /= flLen;

	m_forwardSpeed = flForward * GetMoveSpeed();
	m_strafeSpeed = flSide * GetMoveSpeed();
	// this was initially with the movement keys, but that doesnt work as good. it breaks pathing
	// directly editing this SHOULD be fine

	const float dead = 0.1f;
	if (flForward > dead) SetBits(m_buttonFlags, IN_FORWARD);
	if (flForward < -dead) SetBits(m_buttonFlags, IN_BACK);
	if (flSide > dead) SetBits(m_buttonFlags, IN_MOVERIGHT);
	if (flSide < -dead) SetBits(m_buttonFlags, IN_MOVELEFT);
}