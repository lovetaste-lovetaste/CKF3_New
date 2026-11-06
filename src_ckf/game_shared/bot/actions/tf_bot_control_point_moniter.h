#ifndef TFBOTCONTROLPOINT_MONITER
#define TFBOTCONTROLPOINT_MONITER

#include "bot/hl_bot_action_interface.h"

class CTFBotControlPointMoniter : public CTFAction
{
public:
	virtual void OnEnter(CBot* me);
	virtual void Update(CBot* me);
	virtual void OnExit(CBot* me);
	virtual void OnResume(CBot* me);
	virtual const char* GetName() const { return "ControlPointMoniter"; };
	virtual bool ShouldHurry(CBot* me);

	virtual bool ShouldRetreat(CBot* me)
	{
		return false;
	}

	bool m_bHurry;
	CountdownTimer m_refreshHurryTimer;
};

#endif