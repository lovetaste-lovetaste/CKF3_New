#ifndef HLBOTLEARNING
#define HLBOTLEARNING

// the idea behind this is that bots will learn from their deaths and other things, and adjust accordingly
// eg: bot keeps dying at a specific point or while doing a specific thing. bot keeps this info and changes plans so that they do not die again
// this is probably going to be a WIP for a LOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOONG ass time

/*
ideas:

bots, on path success, add the data from that path into here
whenever a bot is pathfinding, the pathfinder uses the data from the learning data to increase/decrease costs depending on it

engineer bots moniter the placements of kills/deaths/weapon fires, and makes an estimate on where the frontlines are. then, it uses this to place sentries accordingly

*/

class CBotLearning
{
public:
	void SetBot(CBot* me) { m_me = me; }

private:
	CBot* m_me;
};

#endif