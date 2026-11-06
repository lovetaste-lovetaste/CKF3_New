#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

#include "GameEvent.h"
#include "hl_bot_manager.h"
#include "hl_bot.h"
#include "bot_util.h"
#include "bot_phrases.h"
#include "nav_area.h"

//--------------------------------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------------------------------
CHLBotManager *TheHLBots = NULL;
// this also handles nav generation For Some Reason

cvar_t cv_bot_traceview		= { "bot_traceview", "0", FCVAR_SERVER };
cvar_t cv_bot_stop			= { "bot_stop", "0", FCVAR_SERVER };
cvar_t cv_bot_stop_attack	= { "bot_stop_attack", "0", FCVAR_SERVER };
cvar_t cv_bot_show_nav		= { "bot_show_nav", "0", FCVAR_SERVER };
cvar_t cv_bot_show_danger	= { "bot_show_danger", "0", FCVAR_SERVER };
cvar_t cv_bot_nav_edit		= { "nav_edit", "0", FCVAR_SERVER };
cvar_t cv_bot_nav_zdraw		= { "nav_zdraw", "4", FCVAR_SERVER };
cvar_t cv_bot_debug			= { "bot_debug", "0", FCVAR_SERVER };
cvar_t cv_bot_quicksave		= { "bot_quicksave", "0", FCVAR_SERVER };
cvar_t cv_bot_quota			= { "bot_quota", "0", FCVAR_SERVER };
cvar_t cv_bot_updatelimit	= { "bot_max_update_framelimit", "10", FCVAR_SERVER };
// m_AmountOfUpdates // cv_bot_updatelimit
cvar_t cv_bot_last_area_update_tolerance = { "bot_last_area_update_tolerance", "4.0", FCVAR_SERVER };

void Bot_RegisterCvars( void )
{
	CVAR_REGISTER( &cv_bot_traceview );
	CVAR_REGISTER( &cv_bot_stop );
	CVAR_REGISTER( &cv_bot_stop_attack );
	CVAR_REGISTER( &cv_bot_show_nav );
	CVAR_REGISTER( &cv_bot_show_danger );
	CVAR_REGISTER( &cv_bot_nav_edit );
	CVAR_REGISTER( &cv_bot_nav_zdraw );
	CVAR_REGISTER( &cv_bot_debug );
	CVAR_REGISTER( &cv_bot_quicksave );
	CVAR_REGISTER( &cv_bot_last_area_update_tolerance );
	CVAR_REGISTER( &cv_bot_quota );
	CVAR_REGISTER( &cv_bot_updatelimit );
}

static unsigned int s_navPlace = UNDEFINED_PLACE;

unsigned int GetNavPlace( void )
{
	return s_navPlace;
}

void SetNavPlace( unsigned int place )
{
	s_navPlace = place;
}

static void Bot_ServerCommand( void )
{
	if (TheHLBots)
		TheHLBots->ServerCommand( CMD_ARGV( 0 ) );
}

void InstallBotControl( void )
{
	if (TheHLBots == NULL)
	{
		TheHLBots = new CHLBotManager;
		TheHLBots->AddServerCommands();
	}
}

//--------------------------------------------------------------------------------------------------------------
// Nav edit commands
// ts only works in a listen server!!!!!

static const struct NavEditCommand
{
	const char *name;
	NavEditCmdType cmd;
}
navEditCommands[] =
{
	{ "nav_mark",					EDIT_MARK },
	{ "nav_mark_unnamed",			EDIT_MARK_UNNAMED },
	{ "nav_delete",					EDIT_DELETE },
	{ "nav_split",					EDIT_SPLIT },
	{ "nav_merge",					EDIT_MERGE },
	{ "nav_connect",				EDIT_CONNECT },
	{ "nav_disconnect",				EDIT_DISCONNECT },
	{ "nav_begin_area",				EDIT_BEGIN_AREA },
	{ "nav_end_area",				EDIT_END_AREA },
	{ "nav_splice",					EDIT_SPLICE },
	{ "nav_crouch",					EDIT_ATTRIB_CROUCH },
	{ "nav_jump",					EDIT_ATTRIB_JUMP },
	{ "nav_precise",				EDIT_ATTRIB_PRECISE },
	{ "nav_no_jump",				EDIT_ATTRIB_NO_JUMP },
	{ "nav_toggle_place_mode",		EDIT_TOGGLE_PLACE_MODE },
	{ "nav_toggle_place_painting",	EDIT_TOGGLE_PLACE_PAINTING },
	{ "nav_place_floodfill",		EDIT_PLACE_FLOODFILL },
	{ "nav_place_pick",				EDIT_PLACE_PICK },
	{ "nav_warp",					EDIT_WARP_TO_MARK },
	{ "nav_corner_select",			EDIT_SELECT_CORNER },
	{ "nav_corner_raise",			EDIT_RAISE_CORNER },
	{ "nav_corner_lower",			EDIT_LOWER_CORNER },

	{ NULL, EDIT_NONE }
};

//--------------------------------------------------------------------------------------------------------------
CHLBotManager::CHLBotManager()
{
	m_navLoaded = false;
	m_isGenerating = false;
	m_currentNode = NULL;
	m_generationDir = NORTH;
	m_seedIndex = 0;
	m_editCmd = EDIT_NONE;
	m_AmountOfUpdates = 0;
}

const char *CHLBotManager::GetNavMapFilename( void ) const
{
	static char filename[256];
	sprintf( filename, "maps/%s.nav", STRING( gpGlobals->mapname ) );
	return filename;
}

void CHLBotManager::ServerActivate( void )
{
	m_isGenerating = false;
	m_editCmd = EDIT_NONE;
	SetNavPlace( UNDEFINED_PLACE );
	EditNavAreasReset();
	InitBotTrig();

	NavErrorType result = LoadNavigationMap();
	m_navLoaded = (result == NAV_OK);

	if (result == NAV_OK)
		CONSOLE_ECHO( "Loaded navigation map '%s'.\n", GetNavMapFilename() );
	else if (result == NAV_CANT_ACCESS_FILE)
		CONSOLE_ECHO( "No navigation map for this map. Use 'bot_nav_generate' to create one.\n" );
	else
		CONSOLE_ECHO( "ERROR: Navigation map '%s' is unusable (error %d).\n", GetNavMapFilename(), (int)result );
	m_AmountOfUpdates = 0;
}

void CHLBotManager::ServerDeactivate( void )
{
	DestroyNavigationMap();
	TheBotPhrases->Reset();	

	m_navLoaded = false;
	m_isGenerating = false;
	m_currentNode = NULL;
	m_walkableSeeds.clear();
	m_AmountOfUpdates = 0;
}

//--------------------------------------------------------------------------------------------------------------
void CHLBotManager::ClientDisconnect( CBasePlayer *player )
{
	// theres nothing to use this for
	// keep this here mainly just in case something pops up
}

static bool str_isnumber(const char* s)
{
	if (!s || !*s)
		return false;

	for (; *s; ++s)
		if (*s < '0' || *s > '9')
			return false;

	return true;
}

void CHLBotManager::OnEvent(GameEventType event, CBaseEntity* entity, CBaseEntity* other)
{
	// propogate event to all bots
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBasePlayer* player = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(i));

		if (player == NULL)
			continue;

		if (FNullEnt(player->pev))
			continue;

		if (FStrEq(STRING(player->pev->netname), ""))
			continue;

		if (!player->IsBot())
			continue;

		// do not send self-generated event
		if (entity == player)
			continue;

		CBot* bot = static_cast<CBot*>(player);
		bot->OnEvent(event, entity, other);
	}
}

void CHLBotManager::ServerCommand( const char *pcmd )
{
	if (FStrEq(pcmd, "bot_add"))
	{
	//	CBot::CreateBot((CMD_ARGC() >= 2) ? CMD_ARGV(1) : NULL);
	//	CONSOLE_ECHO("bot_add used.\n");
	//	return;
	//	this is still in production so keep this here just in case this breaks

		int count = 1;
		const char* name = NULL;

		// syntax: bot_add [count] [-name <name>]
		// to be added: -team, -class, -targetdummy

		for (int i = 1; i < CMD_ARGC(); ++i)
		{
			const char* arg = CMD_ARGV(i);

			if (FStrEq(arg, "-name"))
			{
				if (i + 1 >= CMD_ARGC())
				{
					CONSOLE_ECHO("bot_add: -name needs a value.\n");
					return;
				}
				name = CMD_ARGV(++i);
			}
			else if (str_isnumber(arg))
			{
				count = atoi(arg);
			}
			else
			{
				CONSOLE_ECHO("Usage: bot_add [count] [-name <name>]\n");
				return;
			}
		}

		if (count < 1)
			count = 1;
		if (count > gpGlobals->maxClients)
			count = gpGlobals->maxClients;

		int made = 0;
		for (int n = 0; n < count; ++n)
		{
			char botName[64];
			const char* useName = NULL;

			if (name && *name)
			{
				if (n == 0)
					useName = name;
				else
				{
					_snprintf(botName, sizeof(botName), "%s_%d", name, n + 1);
					botName[sizeof(botName) - 1] = '\0';
					useName = botName;
				}
			}

			if (!CBot::CreateBot(useName))
				break;		// server full or no free slot
			// no point continuing

			++made;
		}

		CONSOLE_ECHO("bot_add: added %d bot(s).\n", made);
		return;
	}

	if (FStrEq( pcmd, "nav_generate" ))
	{
		BeginNavGeneration();
		return;
	}

	if (FStrEq( pcmd, "nav_save" ))
	{
		if (SaveNavigationMap( GetNavMapFilename() ))
			CONSOLE_ECHO( "Navigation map saved to '%s'.\n", GetNavMapFilename() );
		else
			CONSOLE_ECHO( "ERROR: Could not save navigation map to '%s'.\n", GetNavMapFilename() );
		return;
	}

	if (FStrEq( pcmd, "nav_load" ))
	{
		NavErrorType result = LoadNavigationMap();
		m_navLoaded = (result == NAV_OK);
		CONSOLE_ECHO( (result == NAV_OK) ? "Navigation map loaded.\n" : "ERROR: Could not load navigation map.\n" );
		return;
	}

	if (FStrEq( pcmd, "nav_check" ))
	{
		SanityCheckNavigationMap( STRING( gpGlobals->mapname ) );
		return;
	}

	if (FStrEq( pcmd, "nav_place_name" ))
	{
		// set the current place used by place painting, e.g: bot_nav_place_name Rooftop
		if (CMD_ARGC() < 2)
		{
			CONSOLE_ECHO( "Usage: nav_place_name <name>\n" );
			return;
		}

		SetNavPlace( TheBotPhrases->NameToID( CMD_ARGV( 1 ) ) );
		CONSOLE_ECHO( "Current nav place set to '%s'.\n", CMD_ARGV( 1 ) );
		return;
	}

	// nav editing commands - queued and consumed once per frame by StartFrame()
	for( int i = 0; navEditCommands[i].name; ++i )
	{
		if (FStrEq( pcmd, navEditCommands[i].name ))
		{
			if (cv_bot_nav_edit.value == 0.0f)
				CONSOLE_ECHO( "Set nav_edit to 1 to edit the navigation mesh.\n" );
			else
				m_editCmd = navEditCommands[i].cmd;
			return;
		}
	}
}

float g_nextBotAddQuota = 0.0f;
#define BOT_QUOTA_DELAY 1.0f
void Quota_Think( int count, CBot* kicked = NULL )
{
	if (g_nextBotAddQuota > gpGlobals->time + (BOT_QUOTA_DELAY * 2.0f))
	{
		g_nextBotAddQuota = gpGlobals->time + BOT_QUOTA_DELAY;
		return;
		// this should only happen when the value is brought from another map
		// there is probably a better way to do this
	}

	if (g_nextBotAddQuota > gpGlobals->time)
		return;

	int quotaCount = (int)cv_bot_quota.value;
	if (quotaCount > count)
		CBot::CreateBot(NULL, true);
	else if (quotaCount < count)
	{
		if (kicked && kicked->m_bQuotaManaged)
		{
			kicked->m_kickStatus = GOING_TO_BE_KICKED;
		}
	}
	g_nextBotAddQuota = gpGlobals->time + RANDOM_FLOAT((BOT_QUOTA_DELAY > 0.1f) ? 0.1f : BOT_QUOTA_DELAY, BOT_QUOTA_DELAY);
}

void CHLBotManager::AddServerCommand( const char *cmd )
{
	(*g_engfuncs.pfnAddServerCommand)( (char *)cmd, Bot_ServerCommand );
}

void CHLBotManager::AddServerCommands( void )
{
	AddServerCommand( "bot_add" );
	AddServerCommand( "nav_generate" );
	AddServerCommand( "nav_save" );
	AddServerCommand( "nav_load" );
	AddServerCommand( "nav_check" );
	AddServerCommand( "nav_place_name" );

	for( int i = 0; navEditCommands[i].name; ++i )
		AddServerCommand( navEditCommands[i].name );
}

void CHLBotManager::StartFrame( void )
{
	// think all bots

	m_AmountOfUpdates = 0;
	m_maxUpdates = (int)cv_bot_updatelimit.value;
	if (m_maxUpdates <= 0)
		m_maxUpdates = 100; // basically turns this off


	int quotaAmount = 0;
	CBot* lastbot = NULL;
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBasePlayer* player = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(i));

		if (player == NULL || FNullEnt(player->pev))
			continue;

		if (!player->IsBot())	//(!(player->pev->flags & FL_FAKECLIENT))
		// only bots can play
			continue;

		// jakulo: This Should Be Fine...
		// Try not to make the bots use any other class other than CBot
		// otherwise.... Shit Might Get Weird....
		CBot* bot = static_cast<CBot*>(player);
		if (bot)
		{
			switch (bot->m_kickStatus)
			{
				case GOING_TO_BE_KICKED:
				{
					bot->m_kickStatus = KICKED;
					// UTIL_ClientPrintAll(HUD_PRINTNOTIFY, "#Game_disconnected", STRING(bot->pev->netname)); this doesnt work for some reason
					SERVER_COMMAND(UTIL_VarArgs("kick \"%s\"\n", STRING(bot->pev->netname)));
					break;
				}
				case NOT_KICKED:
				{
					if (bot->m_bQuotaManaged)
						quotaAmount++;

					bot->Think();
					lastbot = bot;
					break;
				}
			}
		}
	}
//	if(m_AmountOfUpdates)
	//	CONSOLE_ECHO("Bots not gated this tick: %i", m_AmountOfUpdates);
	Quota_Think(quotaAmount, lastbot);

	if (m_isGenerating)
		UpdateNavGeneration();

	// (listen server only)
	if (cv_bot_nav_edit.value != 0.0f)
	{
		EditNavAreas( m_editCmd );
		m_editCmd = EDIT_NONE;
	}
	if (cv_bot_show_danger.value != 0.0f)
		DrawDanger();
}

//--------------------------------------------------------------------------------------------------------------
// generation stuff

void CHLBotManager::BeginNavGeneration( void )
{
	DestroyNavigationMap();
	m_navLoaded = false;

	m_walkableSeeds.clear();
	m_seedIndex = 0;

	static const char *seedClassnames[] = { "info_player_deathmatch", "info_player_start", NULL};
	for( int i = 0; seedClassnames[i]; ++i )
	{
		CBaseEntity *spot = NULL;
		while( (spot = UTIL_FindEntityByClassname( spot, seedClassnames[i] )) != NULL )
			m_walkableSeeds.push_back( spot->pev->origin );
	}

	static const char* deleteClassnames[] = { "func_door", "func_door_rotating", "func_pushable", "func_resupplycase", "func_wall", NULL};
	for (int i = 0; deleteClassnames[i]; ++i)
	{
		CBaseEntity* spot = NULL;
		while ((spot = UTIL_FindEntityByClassname(spot, deleteClassnames[i])) != NULL)
		{
			m_walkableSeeds.push_back(spot->pev->origin);
			UTIL_Remove(spot);
		}
		
		// some entities will block the traces done during generation
		// this deletes them
		// also adds it as a seed, considering that it may be a spawn door
		// the server reloads the map so we good
	}

	CBaseEntity* item = NULL;
	while ((item = UTIL_FindEntityByClassname(item, "func_ladder")) != NULL)
	{
		Vector flLadder;

		// compute top & bottom of ladder
		flLadder.x = (item->pev->absmin.x + item->pev->absmax.x) / 2.0f;
		flLadder.y = (item->pev->absmin.y + item->pev->absmax.y) / 2.0f;
		flLadder.z = item->pev->absmax.z;
		m_walkableSeeds.push_back(flLadder); // top of the ladder

		flLadder.z = item->pev->absmin.z;
		m_walkableSeeds.push_back(flLadder); // bottom of the ladder
	}

	if (m_walkableSeeds.empty())
	{
		CONSOLE_ECHO( "ERROR: No seed spots found - cannot generate a navigation mesh.\n" );
		return;
	}

	CONSOLE_ECHO( "Generating navigation mesh from %d seed spots...\n", (int)m_walkableSeeds.size() );

	m_currentNode = NULL;
	m_isGenerating = true;
}

void CHLBotManager::UpdateNavGeneration( void )
{
	const int samplesPerFrame = 300;

	for( int i = 0; i < samplesPerFrame; ++i )
	{
		if (SampleStep())
			continue;

		// sampling is finished, so build nav areas from the nodes and save
		CONSOLE_ECHO( "Sampling complete (%d nodes). Building nav areas...\n", CNavNode::GetListLength() );

		GenerateNavigationAreaMesh();
		CONSOLE_ECHO( "Created %d nav areas.\n", (int)TheNavAreaList.size() );

		BuildLadders();
		CONSOLE_ECHO( "Built %d nav ladders.\n", (int)TheNavLadderList.size() );

		// do hiding spots first before everything else cuz sniper spots and spot encounters rely on them
		for (NavAreaList::iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it)
			(*it)->ComputeHidingSpots();

		int spotCount = 0;
		for (NavAreaList::iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it)
			spotCount += (int)(*it)->GetHidingSpotList()->size();
		CONSOLE_ECHO("Found %d hiding spots.\n", spotCount);

		for (NavAreaList::iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it)
			(*it)->ComputeSniperSpots();
		CONSOLE_ECHO("Classified sniper spots.\n");

		for (NavAreaList::iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it)
			(*it)->ComputeApproachAreas();
		CONSOLE_ECHO("Computed approach areas.\n");

		/*
		int approachCount = 0;
		for (NavAreaList::iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it)
			approachCount += (int)(*it)->GetApproachInfoCount();
		CONSOLE_ECHO("Found %d approach spots.\n", approachCount);
		*/

		for (NavAreaList::iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it)
			(*it)->ComputeSpotEncounters();
		CONSOLE_ECHO("Computed spot encounters.\n");


		if (SaveNavigationMap( GetNavMapFilename() ))
			CONSOLE_ECHO( "Navigation map saved to '%s'.\n", GetNavMapFilename() );
		else
			CONSOLE_ECHO( "ERROR: Could not save navigation map to '%s'.\n", GetNavMapFilename() );

		m_isGenerating = false;
		m_navLoaded = true;
		SERVER_COMMAND("reload\n");
		return;
	}
}

bool CHLBotManager::SampleStep( void )
{
	// find a node with an unexplored direction
	while( true )
	{
		if (m_currentNode == NULL)
		{
			m_currentNode = GetNextWalkableSeedNode();

			if (m_currentNode == NULL)
				return false;	// all seeds explored. no point in continuing!!!!!!!
		}

		bool foundDir = false;
		for( int dir = NORTH; dir < NUM_DIRECTIONS; ++dir )
		{
			if (!m_currentNode->HasVisited( (NavDirType)dir ))
			{
				m_generationDir = (NavDirType)dir;
				foundDir = true;
				break;
			}
		}

		if (foundDir)
			break;

		m_currentNode = m_currentNode->GetParent();
	}

	m_currentNode->MarkAsVisited( m_generationDir );

	const Vector &from = *m_currentNode->GetPosition();

	Vector to = from;
	AddDirectionVector( &to, m_generationDir, GenerationStepSize );

	Vector probe( to.x, to.y, from.z + JumpCrouchHeight );

	float ground;
	Vector normal;
	if (!GetGroundHeight( &probe, &ground, &normal ))
		return true;	// no ground that way

	// too steep to stand on?
	if (normal.z < MaxUnitZSlope)
		return true;

	float deltaZ = ground - from.z;

	if (deltaZ > JumpCrouchHeight)	// too high to reach, even with a crouch-jump
		return true;

	if (deltaZ < -DeathDrop)		// falling this far would kill us
		return true;

	// check that the path between the two spots is clear at torso height
	TraceResult result;
	Vector traceFrom( from.x, from.y, from.z + HalfHumanHeight );
	Vector traceTo( to.x, to.y, ground + HalfHumanHeight );
	UTIL_TraceLine( traceFrom, traceTo, ignore_monsters, NULL, &result );

	if (result.flFraction != 1.0f || 0 != result.fStartSolid)
		return true;

	to.z = ground;
	AddSampledNode( &to, &normal, m_generationDir, m_currentNode );

	return true;
}

CNavNode *CHLBotManager::AddSampledNode( const Vector *destPos, const Vector *normal, NavDirType dir, CNavNode *source )
{
	CNavNode *node = const_cast<CNavNode *>( CNavNode::GetNode( destPos ) );

	bool isNew = false;
	if (node == NULL)
	{
		node = new CNavNode( destPos, normal, source );
		isNew = true;
	}

	source->ConnectTo( node, dir );

	// if the ground is nearly level, assume we can walk back the other way too
	if (fabs( source->GetPosition()->z - destPos->z ) < StepHeight)
	{
		node->ConnectTo( source, OppositeDirection( dir ) );
		node->MarkAsVisited( OppositeDirection( dir ) );
	}

	if (isNew)
	{
		// if a standing player doesn't fit here, mark the spot as crouch only
		TraceResult result;
		Vector floorPos( destPos->x, destPos->y, destPos->z + 1.0f );
		Vector ceiling( destPos->x, destPos->y, destPos->z + HumanHeight - 1.0f );
		UTIL_TraceLine( floorPos, ceiling, ignore_monsters, NULL, &result );

		if (result.flFraction != 1.0f)
			node->SetAttributes( NAV_CROUCH );

		// new nodes become the sampling frontier
		m_currentNode = node;
	}

	return node;
}

CNavNode *CHLBotManager::GetNextWalkableSeedNode( void )
{
	while( m_seedIndex < (int)m_walkableSeeds.size() )
	{
		Vector pos = m_walkableSeeds[ m_seedIndex ];
		++m_seedIndex;

		// align to the sampling grid and drop onto the ground
		SnapToGrid( &pos );

		Vector probe( pos.x, pos.y, pos.z + HalfHumanHeight );

		float ground;
		Vector normal;
		if (!GetGroundHeight( &probe, &ground, &normal ))
			continue;

		pos.z = ground;

		// skip seeds that sampling has already reached
		if (CNavNode::GetNode( &pos ))
			continue;

		return new CNavNode( &pos, &normal );
	}

	return NULL;
}
