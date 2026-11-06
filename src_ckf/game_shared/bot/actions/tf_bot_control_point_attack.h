#ifndef TFBOTCONTROLPOINT_ATTACK
#define TFBOTCONTROLPOINT_ATTACK

#include "bot/hl_bot_action_interface.h"

class CTFBotControlPointAttack : public CTFAction
{
public:
	virtual void OnEnter(CBot* me);		// when getting changed/suspended into, OR when the initial action
	virtual void Update(CBot* me);		// CONTINUE status
	virtual void OnExit(CBot* me);		// CHANGE_TO or DONE status
	virtual void OnResume(CBot* me);	// action that was suspended into is now finished, so this action is now the active one
	virtual const char* GetName() const { return "ControlPointAttack"; };
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