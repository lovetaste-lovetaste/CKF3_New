#ifndef TFBOTIDLE
#define TFBOTIDLE

#include "bot/hl_bot_action_interface.h"

// this does nothing
// LEAVE THIS BE!!!!
class CTFBotIdle : public CTFAction
{
public:
	virtual void OnEnter(CBot* me);
	virtual void Update(CBot* me);
	virtual void OnExit(CBot* me);
	virtual void OnResume(CBot* me);
	virtual const char* GetName() const { return "Idle"; }
	virtual bool ShouldHurry(CBot* me)
	{
		return false;
	}
	virtual bool ShouldRetreat(CBot* me)
	{
		return false;
	}
};

#endif