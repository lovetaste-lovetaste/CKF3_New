#ifndef TFBOTRETREAT
#define TFBOTRETREAT

#include "bot/hl_bot_action_interface.h"

class CTFBotRetreat : public CTFAction
{
public:
	virtual void OnEnter(CBot* me);
	virtual void Update(CBot* me);
	virtual void OnExit(CBot* me);
	virtual void OnResume(CBot* me);
	virtual const char* GetName() const { return "Retreat"; }
	virtual bool ShouldHurry(CBot* me)
	{
		return false;
	}
	virtual bool ShouldRetreat(CBot* me)
	{
		return false;
		// kinda on the nose LOLOL
	}
	CountdownTimer m_recomputeRetreatSpot;
};

#endif