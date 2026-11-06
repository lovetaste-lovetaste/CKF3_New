#ifndef TFBOTMEDICHEAL
#define TFBOTMEDICHEAL

#include "bot/hl_bot_action_interface.h"

// this does nothing
// LEAVE THIS BE!!!!
class CTFBotMedicHeal : public CTFAction
{
public:
	CTFBotMedicHeal() : m_bRetreat(false) {}

	virtual void OnEnter(CBot* me);
	virtual void Update(CBot* me);
	virtual void OnExit(CBot* me);
	virtual void OnResume(CBot* me);
	virtual const char* GetName() const { return "MedicHeal"; }
	virtual bool ShouldHurry(CBot* me)
	{
		return false;
	}
	virtual bool ShouldRetreat(CBot* me);

private:
	bool m_bRetreat;
};

#endif