#ifndef CKFECONMANAGER
#define CKFECONMANAGER

// economy shit

class CBasePlayerWeapon;

enum
{
    ITEM_NULL, // dont use this

    // stock weapons
    ITEM_SCATTERGUN,
    ITEM_PISTOL,
    ITEM_BAT,
    ITEM_ROCKETLAUNCHER,
    ITEM_SHOTGUN,
    ITEM_SHOVEL,
    ITEM_FLAMETHROWER,
    ITEM_FIREAXE,
    ITEM_GRENADELAUNCHER,
    ITEM_STICKYBOMBLAUNCHER,
    ITEM_BOTTLE,
    ITEM_MINIGUN,
    ITEM_FISTS,
    ITEM_WRENCH,
    ITEM_BUILDPDA,
    ITEM_DESTROYPDA,
    ITEM_SYRINGEGUN,
    ITEM_MEDIGUN,
    ITEM_BONESAW,
    ITEM_SNIPERRIFLE,
    ITEM_SMG,
    ITEM_KUKRI,
    ITEM_REVOLVER,
    ITEM_SAPPER,
    ITEM_BUTTERFLY,
    ITEM_DISGUISEKIT,

    // unlocks
    ITEM_BLACKBOX,
    ITEM_AXTINGUISHER,
    ITEM_ETERNAL_REWARD,

    ITEM_COUNT  // ESPECIALLY dont use this
};

enum AllowedClasses
{
    ALLOWED_SCOUT       = (1 << 0),
    ALLOWED_SOLDIER     = (1 << 1),
    ALLOWED_PYRO        = (1 << 2),
    ALLOWED_DEMOMAN     = (1 << 3),
    ALLOWED_HEAVY       = (1 << 4),
    ALLOWED_ENGINEER    = (1 << 5),
    ALLOWED_MEDIC       = (1 << 6),
    ALLOWED_SNIPER      = (1 << 7),
    ALLOWED_SPY         = (1 << 8),
};

inline int ClassToAllowedBit(int iClass)
{
    switch (iClass)
    {
    case CLASS_SCOUT:    return ALLOWED_SCOUT;
    case CLASS_SOLDIER:  return ALLOWED_SOLDIER;
    case CLASS_PYRO:     return ALLOWED_PYRO;
    case CLASS_DEMOMAN:  return ALLOWED_DEMOMAN;
    case CLASS_HEAVY:    return ALLOWED_HEAVY;
    case CLASS_ENGINEER: return ALLOWED_ENGINEER;
    case CLASS_MEDIC:    return ALLOWED_MEDIC;
    case CLASS_SNIPER:   return ALLOWED_SNIPER;
    case CLASS_SPY:      return ALLOWED_SPY;
    }
    return 0;
}

enum
{
    ATK_NULL = 0,   // DO NOT USE THIS

    NO_RANDOM_CRITS, // TODO

    //--------------------------/
    // attributes that usually change base stats in the weapon itself
    ATK_CLIP,
    ATK_DAMAGE,
    ATK_FIRE_RATE,
    ATK_RELOAD_RATE,
    ATK_VELOCITY,   // projectiles
    ATK_RANGE,      // melee weapons
    ATK_SHOT_AMOUNT,// multi-shot weapons ( shotguns, minigun, etc ). this is the EXTRA AMOUNT. the weapons shoot one extra!!!!
    //--------------------------/



    //--------------------------/
    // on kill
    // these are scanned on KillReward()
    KILL_START, // dont use this
    KILL_CRITS,
    KILL_HEAL,
    KILL_DISGUISE,
    KILL_NO_SCREAM, // TODO
    KILL_END,   // dont use this
    //--------------------------/



    //--------------------------/
    // on hit
    // these are in the TakeDamage thingy
    HIT_START,  // dont use this
    HIT_HEAL,
    HIT_END,    // dont use this
    //--------------------------/

    //--------------------------/
    // on hit checks
    // this is specifically for things that require the target to be under specific conditions
    // basically just a boolean
    HITCOND_START,  // dont use this
    HITCOND_CRIT_BURNING,
    HITCOND_CRIT_MINICRIT,  // TODO
    HITCOND_END,    // dont use this

    //--------------------------/


    ATTRIB_COUNT
};

struct WeaponAttr
{
    int iAttrID;
    float flValue;
};

struct ItemDef
{
    int iIndex;                 // unique id. i will eventually make another way of doing this
    const char* szBaseClass;    // base class that the weapon is based on. eg: "weapon_rocketlauncher" for rocket launcher unlocks  
    WeaponAttr attrs[8];        // eg: {ATK_DAMAGE, 1.2}, {ATK_CLIP, -2}, {ATK_FIRE_RATE, 0.1}
    int iAllowedClasses;        // eg: ALLOWED_SCOUT | ALLOWED_ENGINEER for a weapon that is for both scout and engineer

    // custom model shit. these get precached automatically, so just put the model here!!!
    char* szViewmodel;          // custom viewmodel model path
    char* szPlayermodel;        // custom player model path
};

extern ItemDef   g_ItemDefs[ITEM_COUNT];
extern const int g_StockLoadout[CLASS_SPY + 1][MAX_WEAPON_SLOTS];

class CEconAttributeMgr
{
public:
    float GetAttributeValue(const CBasePlayerWeapon* pWeapon, int iAttrID, float flDefault);
    int GetAttributeInt(const CBasePlayerWeapon* pWeapon, int iAttrID, int iDefault);
    bool GetAttributeBool(const CBasePlayerWeapon* pWeapon, int iAttrID);
    const ItemDef* GetItemDefFor(const CBasePlayerWeapon* pWeapon);
    const ItemDef* GetItemDef(int iIndex);
    char* GetViewmodel(const CBasePlayerWeapon* pWeapon, char* szDefaultViewmodel);
    char* GetPlayermodel(const CBasePlayerWeapon* pWeapon, char* szDefaultPlayermodel);
};

void InstallEconomyManager(void);
void ValidateItemDefs(void);

extern CEconAttributeMgr* TheEconManager;

#endif // !CKFECONMANAGER
