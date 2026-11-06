// contains all the economy shit that the weapons use
// this initially was based off the tf2 one, but it kinda branched off LOL
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

CEconAttributeMgr* TheEconManager = NULL;

const int g_StockLoadout[CLASS_SPY + 1][MAX_WEAPON_SLOTS] =
{
    { 0 },
    { ITEM_SCATTERGUN, ITEM_PISTOL, ITEM_BAT },
    { ITEM_MINIGUN, ITEM_SHOTGUN, ITEM_FISTS },
    { ITEM_ROCKETLAUNCHER, ITEM_SHOTGUN, ITEM_SHOVEL },
    { ITEM_FLAMETHROWER, ITEM_SHOTGUN, ITEM_FIREAXE },
    { ITEM_SNIPERRIFLE, ITEM_SMG, ITEM_KUKRI },
    { ITEM_SYRINGEGUN, ITEM_MEDIGUN, ITEM_BONESAW },
    { ITEM_SHOTGUN, ITEM_PISTOL, ITEM_WRENCH, ITEM_BUILDPDA, ITEM_DESTROYPDA },
    { ITEM_GRENADELAUNCHER, ITEM_STICKYBOMBLAUNCHER, ITEM_BOTTLE },
    { ITEM_REVOLVER, ITEM_SAPPER, ITEM_BUTTERFLY, ITEM_DISGUISEKIT },
    // these are based off the class enums. the ordering may look weird but trust me its fine
};

ItemDef g_ItemDefs[ITEM_COUNT] =
{
    { ITEM_NULL,              "",                        {}, 0 },
    { ITEM_SCATTERGUN,        "weapon_scattergun",       {}, ALLOWED_SCOUT },
    { ITEM_PISTOL,            "weapon_pistol",           {}, ALLOWED_SCOUT | ALLOWED_ENGINEER },
    { ITEM_BAT,               "weapon_bat",              {}, ALLOWED_SCOUT },
    { ITEM_ROCKETLAUNCHER,    "weapon_rocketlauncher",   {}, ALLOWED_SOLDIER },
    { ITEM_SHOTGUN,           "weapon_shotgun",          {}, ALLOWED_SOLDIER | ALLOWED_PYRO | ALLOWED_HEAVY | ALLOWED_ENGINEER },
    { ITEM_SHOVEL,            "weapon_shovel",           {}, ALLOWED_SOLDIER },
    { ITEM_FLAMETHROWER,      "weapon_flamethrower",     {}, ALLOWED_PYRO },
    { ITEM_FIREAXE,           "weapon_fireaxe",          {}, ALLOWED_PYRO },
    { ITEM_GRENADELAUNCHER,   "weapon_grenadelauncher",  {}, ALLOWED_DEMOMAN },
    { ITEM_STICKYBOMBLAUNCHER,"weapon_stickylauncher",   {}, ALLOWED_DEMOMAN },
    { ITEM_BOTTLE,            "weapon_bottle",           {}, ALLOWED_DEMOMAN },
    { ITEM_MINIGUN,           "weapon_minigun",          {}, ALLOWED_HEAVY },
    { ITEM_FISTS,             "weapon_fist",             {}, ALLOWED_HEAVY },
    { ITEM_WRENCH,            "weapon_wrench",           {}, ALLOWED_ENGINEER },
    { ITEM_BUILDPDA,          "weapon_buildpda",         {}, ALLOWED_ENGINEER },
    { ITEM_DESTROYPDA,        "weapon_destroypda",       {}, ALLOWED_ENGINEER },
    { ITEM_SYRINGEGUN,        "weapon_syringegun",       {}, ALLOWED_MEDIC },
    { ITEM_MEDIGUN,           "weapon_medigun",          {}, ALLOWED_MEDIC },
    { ITEM_BONESAW,           "weapon_bonesaw",          {}, ALLOWED_MEDIC },
    { ITEM_SNIPERRIFLE,       "weapon_sniperifle",       {}, ALLOWED_SNIPER },
    { ITEM_SMG,               "weapon_smg",              {}, ALLOWED_SNIPER },
    { ITEM_KUKRI,             "weapon_kukri",            {}, ALLOWED_SNIPER },
    { ITEM_REVOLVER,          "weapon_revolver",         {}, ALLOWED_SPY },
    { ITEM_SAPPER,            "weapon_sapper",           {}, ALLOWED_SPY },
    { ITEM_BUTTERFLY,         "weapon_butterfly",        {}, ALLOWED_SPY },
    { ITEM_DISGUISEKIT,       "weapon_disguisekit",      {}, ALLOWED_SPY },

    { ITEM_BLACKBOX,          
    "weapon_rocketlauncher",   
    { {ATK_CLIP, 3}, {HIT_HEAL, 20.0} }, 
    ALLOWED_SOLDIER, 
    "models/chicken_fortress_2/v_blackbox.mdl",
    "models/ckf_III/w_blackbox.mdl" },
    
    { ITEM_AXTINGUISHER,      
    "weapon_fireaxe",          
    { {HITCOND_CRIT_BURNING, 1.0}, {ATK_DAMAGE, 44.0}, {NO_RANDOM_CRITS, 1.0} },
    ALLOWED_PYRO, 
    "models/chicken_fortress_2/v_extinguisher.mdl", 
    "models/ckf_III/w_axetinguisher.mdl" },

    { ITEM_ETERNAL_REWARD,    
    "weapon_butterfly",        
    { {KILL_DISGUISE, 1.0} }, 
    ALLOWED_SPY, 
    "models/chicken_fortress_2/v_ereward.mdl" },

    // unlock table :3
    // this was fun to make. it didnt take long either! the main thing is the attributes LOL
    // 
    // example of a unlockable weapon
    // { example_index,    "weapon_rocketlauncher",   { {ATK_CLIP, 2}, {ATK_FIRE_RATE, 0.1} }, ALLOWED_SOLDIER },
    // this would give you a rocket launcher with a max clip of 2 and a firing rate of 0.1, and it would only be allowed on soldier
};
static_assert(ARRAYSIZE(g_ItemDefs) == ITEM_COUNT, "g_ItemDefs out of sync with enum");
// this is equal to items_game.txt in tf2
// i WOULD make a txt file, but im too stupid to know how to work that
// only reason nav file reading works is bcuz of valve doing it for me LOLOLOL i suck with file shit
// downside is that this hardcodes all the files. however, this is probably for the best anyway

char* CEconAttributeMgr::GetPlayermodel(const CBasePlayerWeapon* pWeapon, char* szDefaultPlayermodel)
{
    const ItemDef* pDef = GetItemDef(pWeapon->m_iItemIndex);
    if (!pDef || !pDef->szPlayermodel || !pDef->szPlayermodel[0])
        return szDefaultPlayermodel;

    return pDef->szPlayermodel;
}

char* CEconAttributeMgr::GetViewmodel(const CBasePlayerWeapon* pWeapon, char* szDefaultViewmodel)
{
    const ItemDef* pDef = GetItemDef(pWeapon->m_iItemIndex);
    if (!pDef || !pDef->szViewmodel || !pDef->szViewmodel[0])
        return szDefaultViewmodel;

    return pDef->szViewmodel;
}

float CEconAttributeMgr::GetAttributeValue(const CBasePlayerWeapon* pWeapon, int iAttrID, float flDefault)
{
    const ItemDef* pDef = GetItemDef(pWeapon->m_iItemIndex);
    if (!pDef)
        return flDefault;

    int attribLength = ARRAYSIZE(pDef->attrs);
    if (!attribLength)
        return flDefault;

    for (int i = 0; i < attribLength; i++)
    {
        if (pDef->attrs[i].iAttrID == iAttrID)
            return pDef->attrs[i].flValue;
    }
    return flDefault;
}

int CEconAttributeMgr::GetAttributeInt(const CBasePlayerWeapon* pWeapon, int iAttrID, int iDefault)
{
    const ItemDef* pDef = GetItemDef(pWeapon->m_iItemIndex);
    if (!pDef)
        return iDefault;

    int attribLength = ARRAYSIZE(pDef->attrs);
    if (!attribLength)
        return iDefault;

    for (int i = 0; i < attribLength; i++)
    {
        if (pDef->attrs[i].iAttrID == iAttrID)
            return (int)pDef->attrs[i].flValue;
    }
    return iDefault;
}

bool CEconAttributeMgr::GetAttributeBool(const CBasePlayerWeapon* pWeapon, int iAttrID)
{
    const ItemDef* pDef = GetItemDef(pWeapon->m_iItemIndex);
    if (!pDef)
        return false;

    int attribLength = ARRAYSIZE(pDef->attrs);
    if (!attribLength)
        return false;

    for (int i = 0; i < attribLength; i++)
    {
        if (pDef->attrs[i].iAttrID == iAttrID)
            return true;
    }

    return false;
}

const ItemDef* CEconAttributeMgr::GetItemDefFor(const CBasePlayerWeapon* pWeapon)
{
	return GetItemDef(pWeapon->m_iItemIndex);
}

const ItemDef* CEconAttributeMgr::GetItemDef(int iIndex)
{
    if (iIndex <= ITEM_NULL || iIndex >= ARRAYSIZE(g_ItemDefs))
        return nullptr;
    const ItemDef* pDef = &g_ItemDefs[iIndex];
    return (pDef->iIndex == iIndex) ? pDef : nullptr;
}

void ValidateItemDefs(void)
{
    // jakulo REALLY wanted to add this for some reason
    // seems kinda pointless JFL
    // wyt: now it does the precaches so this has a purpose now LOLOLOl

    int errors = 0;
    int modelPrecaches = 0;
    for (int i = 0; i < ITEM_COUNT; i++)
    {
        const ItemDef& def = g_ItemDefs[i];

        if (def.iIndex != i)
        {
            ALERT(at_console, "[Econ] g_ItemDefs[%d] has iIndex %d (entries out of order?)\n", i, def.iIndex);
            errors++;
        }

        if (i == ITEM_NULL)
            continue;

        if (!def.szBaseClass || !def.szBaseClass[0])
        {
            ALERT(at_console, "[Econ] item %d has no base class\n", i);
            errors++;
        }

        if (def.szViewmodel)
        {
            PRECACHE_MODEL(def.szViewmodel);
            modelPrecaches++;
        }

        if (def.szPlayermodel)
        {
            PRECACHE_MODEL(def.szPlayermodel);
            modelPrecaches++;
        }

        if (!def.iAllowedClasses)
        {
            if( def.szBaseClass && def.szBaseClass[0])
                ALERT(at_console, "[Econ] item %d (%s) has no allowed classes\n", i, def.szBaseClass);
            else
                ALERT(at_console, "[Econ] item %d (Broken Base Class) has no allowed classes\n", i);
            errors++;
        }
    }
    for (int iClass = CLASS_SCOUT; iClass <= CLASS_SPY; iClass++)
    {
        for (int slot = 0; slot < MAX_WEAPON_SLOTS; slot++)
        {
            int iItem = g_StockLoadout[iClass][slot];
            if (iItem == ITEM_NULL)
                continue;

            if (iItem < 0 || iItem >= ITEM_COUNT)
            {
                ALERT(at_console, "[Econ] stock loadout class %d slot %d has invalid item %d\n", iClass, slot, iItem);
                errors++;
                continue;
            }

            if (!(g_ItemDefs[iItem].iAllowedClasses & ClassToAllowedBit(iClass)))
            {
                ALERT(at_console, "[Econ] stock item %d (%s) is not allowed for class %d\n",
                    iItem, g_ItemDefs[iItem].szBaseClass, iClass);
                errors++;
            }
        }
    }

    if (errors)
        ALERT(at_console, "[Econ] %d item table error(s), fix them before testing loadouts\n", errors);

    if (modelPrecaches)
        ALERT(at_console, "[Econ] %d custom model precaches\n", modelPrecaches);
}

void InstallEconomyManager(void)
{
    if (TheEconManager == NULL)
    {
        TheEconManager = new CEconAttributeMgr;
        // TheEconManager->AddServerCommands();
        // ValidateItemDefs();
    }
}