#ifndef TFBOTENGINEER
#define TFBOTENGINEER

#include "bot/hl_bot_action_interface.h"

class CTFBotEngineerAction : public CTFAction
{
public:
	virtual void OnEnter(CBot* me);
	virtual void Update(CBot* me);
	virtual void OnExit(CBot* me);
	virtual void OnResume(CBot* me);
	virtual const char* GetName() const { return "EngineerAction"; }
	virtual bool ShouldHurry(CBot* me)
	{
		return false;
	}
	virtual bool ShouldRetreat(CBot* me)
	{
		return false;
	}

	bool CanPlaceOnArea(CBot* me, CNavArea* area);
};

#endif