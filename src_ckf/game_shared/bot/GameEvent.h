//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003
// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

#ifndef GAME_EVENT_H
#define GAME_EVENT_H

//
// NOTE: This enum is used by both the client and server, so only add events to the
// end of the list to remain maximally compatible with existing clients.
//

// edited for chicken fortress

enum GameEventType
{
	EVENT_INVALID = 0,

	EVENT_WEAPON_FIRED,
	EVENT_WEAPON_FIRED_SECONDARY,

	EVENT_PLAYER_FOOTSTEP,
	EVENT_PLAYER_VOICELINE,
	EVENT_PLAYER_SCREAMED,

	NUM_GAME_EVENTS
};

#ifdef DEFINE_EVENT_NAMES
// NOTE: These must be kept in sync with the GameEventType enum

const char *GameEventName[ NUM_GAME_EVENTS+1 ] =
{
	"EVENT_INVALID",
	"EVENT_WEAPON_FIRED"
	"EVENT_WEAPON_FIRED_SECONDARY"

	"EVENT_PLAYER_FOOTSTEP",
	"EVENT_PLAYER_VOICELINE",
	"EVENT_PLAYER_SCREAMED",

	NULL		// must be NULL-terminated
};

#else
extern const char *GameEventName[ NUM_GAME_EVENTS ];
#endif

// convert name to GameEventType
extern GameEventType NameToGameEvent( const char *name );

#endif // GAME_EVENT_H
