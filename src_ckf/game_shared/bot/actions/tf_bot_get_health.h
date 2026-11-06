#ifndef TFBOTGETHEALTH
#define TFBOTGETHEALTH

#include "bot/hl_bot_action_interface.h"

class CTFBotGetHealth : public CTFAction
{
public:
	virtual void OnEnter(CBot* me);
	virtual void Update(CBot* me);
	virtual void OnExit(CBot* me);
	virtual void OnResume(CBot* me);
	virtual const char* GetName() const { return "GetHealth"; }
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