#include "UsrAI.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <unordered_map>

using namespace std;
tagGame tagUsrGame;
ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/

static const int dx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
static const int dy[8] = {1, 0, -1, 0, 1, -1, -1, 1};

Brain mgr;

void UsrAI::processData() { mgr.update(getInfo()); }

int buildingSize(int type)
{
    if (type == BUILDING_HOME || type == BUILDING_ARROWTOWER) return 2;
    return 3;
}

int resourceSize(int type)
{
    if (type == RESOURCE_STONE || type == RESOURCE_GOLD || type == RESOURCE_FISH) return 2;
    return 1;
}

int buildWoodCost(int type)
{
    switch (type)
    {
        case BUILDING_HOME: return BUILD_HOUSE_WOOD;
        case BUILDING_GRANARY: return BUILD_GRANARY_WOOD;
        case BUILDING_STOCK: return BUILD_STOCK_WOOD;
        case BUILDING_ARMYCAMP: return BUILD_ARMYCAMP_WOOD;
        case BUILDING_MARKET: return BUILD_MARKET_WOOD;
        case BUILDING_FARM: return BUILD_FARM_WOOD;
        case BUILDING_STABLE: return BUILD_STABLE_WOOD;
        case BUILDING_RANGE: return BUILD_RANGE_WOOD;
        case BUILDING_COLLAGE: return BUILD_COLLAGE_WOOD;
        case BUILDING_SIEGE: return BUILD_SIEGE_WOOD;
        default: return 0;
    }
}

int buildStoneCost(int type)
{
    if (type == BUILDING_ARROWTOWER) return BUILD_ARROWTOWER_STONE;
    return 0;
}

ResKind kindOf(int resourceType)
{
    switch (resourceType)
    {
        case RESOURCE_TREE: return RK_WOOD;
        case RESOURCE_STONE: return RK_STONE;
        case RESOURCE_GOLD: return RK_GOLD;
        case RESOURCE_BUSH: return RK_BUSH;
        case RESOURCE_GAZELLE:
        case RESOURCE_ELEPHANT:
        case RESOURCE_LION: return RK_CORPSE;  // 只在 Blood <= 0 时才能当尸体, 此处没有合法检测
        default: return RK_COUNT;
    }
}

int atkRange(int sort)
{
    switch (sort)
    {
        case AT_SLINGER: return (int)DIS_SLINGER;
        case AT_BOWMAN: return (int)DIS_BOWMAN;
        case AT_IMPROVED: return (int)DIS_IMPROVEDBOWMAN2;
        case AT_COMPOSITE_BOWMAN: return (int)DIS_COMPOSITE_BOWMAN;
        case AT_CHARIOT_ARCHER: return (int)DIS_CHARIOT_ARCHER;
        case AT_STONE_THROWER: return (int)DIS_STONE_THROWER;
        case AT_PRIEST: return (int)DIS_PRIEST;
        default: return 0;  // 近战
    }
}

double dpsOf(const tagArmy& e)  // 兔头
{
    double interval;
    switch (e.Sort)
    {
        case AT_SCOUT: interval = (double)INTERVAL_SCOUT; break;
        case AT_CAVALRY: interval = (double)INTERVAL_CAVALRY; break;
        case AT_BOWMAN:
        case AT_IMPROVED:
        case AT_CHARIOT:
        case AT_COMPOSITE_BOWMAN: interval = (double)INTERVAL_BOWMAN; break;
        case AT_STONE_THROWER: interval = (double)INTERVAL_STONE_THROWER; break;
        default: interval = (double)INTERVAL_CLUBMAN1; break;
    }
    return e.attack / interval;
}

Stock actionCost(int action)
{
    Stock c;
    switch (action)
    {
        case BUILDING_CENTER_CREATEFARMER: c.meat = BUILDING_CENTER_CREATEFARMER_FOOD; break;
        case BUILDING_ARMYCAMP_CREATE_CLUBMAN: c.meat = BUILDING_ARMYCAMP_CREATE_CLUBMAN_FOOD; break;
        case BUILDING_ARMYCAMP_CREATE_SLINGER:
            c.meat = BUILDING_ARMYCAMP_CREATE_SLINGER_FOOD;
            c.stone = BUILDING_ARMYCAMP_CREATE_SLINGER_STONE;
            break;
        case BUILDING_ARMYCAMP_CREATE_BROADSWORD:
            c.meat = BUILDING_ARMYCAMP_CREATE_BROADSWORD_FOOD;
            c.gold = BUILDING_ARMYCAMP_CREATE_BROADSWORD_GOLD;
            break;
        case BUILDING_RANGE_CREATE_BOWMAN:
            c.meat = BUILDING_RANGE_CREATE_BOWMAN_FOOD;
            c.wood = BUILDING_RANGE_CREATE_BOWMAN_WOOD;
            break;
        case BUILDING_RANGE_CREATE_CHARIOT_ARCHER:
            c.meat = BUILDING_RANGE_CREATE_CHARIOT_ARCHER_FOOD;
            c.wood = BUILDING_RANGE_CREATE_CHARIOT_ARCHER_WOOD;
            break;
        case BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN:
            c.meat = BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_FOOD;
            c.gold = BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_GOLD;
            break;
        case BUILDING_STABLE_CREATE_SCOUT: c.meat = BUILDING_STABLE_CREATE_SCOUT_FOOD; break;
        case BUILDING_STABLE_CREATE_CHARIOT:
            c.meat = BUILDING_STABLE_CREATE_CHARIOT_FOOD;
            c.wood = BUILDING_STABLE_CREATE_CHARIOT_WOOD;
            break;
        case BUILDING_STABLE_CREATE_CAVALRY:
            c.meat = BUILDING_STABLE_CREATE_CAVALRY_FOOD;
            c.gold = BUILDING_STABLE_CREATE_CAVALRY_GOLD;
            break;
        case BUILDING_COLLAGE_CREATE_HOPLITE:
            c.meat = BUILDING_COLLAGE_CREATE_HOPLITE_FOOD;
            c.gold = BUILDING_COLLAGE_CREATE_HOPLITE_GOLD;
            break;
        case BUILDING_SIEGE_CREATE_STONE_THROWER:
            c.wood = BUILDING_SIEGE_CREATE_STONE_THROWER_WOOD;
            c.gold = BUILDING_SIEGE_CREATE_STONE_THROWER_GOLD;
            break;
        case BUILDING_DOCK_CREATE_SAILING: c.wood = BUILDING_DOCK_CREATE_SAILING_WOOD; break;
        case BUILDING_DOCK_CREATE_WOOD_BOAT: c.wood = BUILDING_DOCK_CREATE_WOOD_BOAT_WOOD; break;
        case BUILDING_DOCK_CREATE_SHIP: c.wood = BUILDING_DOCK_CREATE_SHIP_WOOD; break;
        case BUILDING_CENTER_UPGRADE: c.meat = BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD; break;
        case BUILDING_GRANARY_ARROWTOWER: c.meat = BUILDING_GRANARY_ARROWTOWER_FOOD; break;
        case BUILDING_GRANARY_ARROWTOWE_UPGRADE:
            c.meat = BUILDING_GRANARY_UPGRADE_ARROWTOWER_FOOD;
            c.stone = BUILDING_GRANARY_UPGRADE_ARROWTOWER_STONE;
            break;
        case BUILDING_ARMYCAMP_UPGRADE_CLUBMAN: c.meat = BUILDING_ARMYCAMP_UPGRADE_CLUBMAN_FOOD; break;
        case BUILDING_ARMYCAMP_UPGRADE_BROADSWORD:
            c.meat = BUILDING_ARMYCAMP_UPGRADE_BROADSWORD_FOOD;
            c.gold = BUILDING_ARMYCAMP_UPGRADE_BROADSWORD_GOLD;
            break;
        case BUILDING_RANGE_UPGRADE_COMPOSITE_BOW:
            c.meat = BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_FOOD;
            c.wood = BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD;
            break;
        case BUILDING_MARKET_WOOD_UPGRADE:
            c.meat = BUILDING_MARKET_WOOD_UPGRADE_FOOD;
            c.wood = BUILDING_MARKET_WOOD_UPGRADE_WOOD;
            break;
        case BUILDING_MARKET_CRAFT_UPGRADE:
            c.meat = BUILDING_MARKET_CRAFT_UPGRADE_FOOD;
            c.wood = BUILDING_MARKET_CRAFT_UPGRADE_WOOD;
            break;
        case BUILDING_MARKET_STONE_UPGRADE:
            c.meat = BUILDING_MARKET_STONE_UPGRADE_FOOD;
            c.stone = BUILDING_MARKET_STONE_UPGRADE_STONE;
            break;
        case BUILDING_MARKET_GOLD_UPGRADE:
            c.meat = BUILDING_MARKET_GOLD_UPGRADE_FOOD;
            c.wood = BUILDING_MARKET_GOLD_UPGRADE_WOOD;
            break;
        case BUILDING_MARKET_FARM_UPGRADE:
            c.meat = BUILDING_MARKET_FARM_UPGRADE_FOOD;
            c.wood = BUILDING_MARKET_FARM_UPGRADE_WOOD;
            break;
        case BUILDING_MARKET_PLOW_UPGRADE:
            c.meat = BUILDING_MARKET_PLOW_UPGRADE_FOOD;
            c.wood = BUILDING_MARKET_PLOW_UPGRADE_WOOD;
            break;
        case BUILDING_MARKET_WHEEL_UPGRADE:
            c.meat = BUILDING_MARKET_WHEEL_UPGRADE_FOOD;
            c.wood = BUILDING_MARKET_WHEEL_UPGRADE_WOOD;
            break;
        case BUILDING_STOCK_UPGRADE_USETOOL: c.meat = BUILDING_STOCK_UPGRADE_CLOSER_ATTACK_FOOD; break;
        case BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY: c.meat = BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY_FOOD; break;
        case BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER: c.meat = BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER_FOOD; break;
        case BUILDING_STOCK_UPGRADE_DEFENSE_RIDER: c.meat = BUILDING_STOCK_UPGRADE_DEFENSE_RIDER_FOOD; break;
        case BUILDING_STOCK_UPGRADE_MISSILE_DEFENSE_INFANTRY:
            c.meat = BUILDING_STOCK_UPGRADE_MISSILE_DEFENSE_INFANTRY_FOOD;
            c.gold = BUILDING_STOCK_UPGRADE_MISSILE_DEFENSE_INFANTRY_GOLD;
            break;
        case BUILDING_ARMYCAMP_RESEARCH_LOGISTICS:
            c.meat = BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_FOOD;
            c.gold = BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_GOLD;
            break;
        default: break;
    }
    return c;
}

int actionHost(int action)
{
    switch (action)
    {
        case BUILDING_CENTER_CREATEFARMER:
        case BUILDING_CENTER_UPGRADE: return BUILDING_CENTER;

        case BUILDING_GRANARY_ARROWTOWER:
        case BUILDING_GRANARY_WALL:
        case BUILDING_GRANARY_ARROWTOWE_UPGRADE: return BUILDING_GRANARY;

        case BUILDING_MARKET_WOOD_UPGRADE:
        case BUILDING_MARKET_STONE_UPGRADE:
        case BUILDING_MARKET_FARM_UPGRADE:
        case BUILDING_MARKET_GOLD_UPGRADE:
        case BUILDING_MARKET_WHEEL_UPGRADE:
        case BUILDING_MARKET_CRAFT_UPGRADE:
        case BUILDING_MARKET_PLOW_UPGRADE: return BUILDING_MARKET;

        case BUILDING_STOCK_UPGRADE_USETOOL:
        case BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY:
        case BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER:
        case BUILDING_STOCK_UPGRADE_DEFENSE_RIDER:
        case BUILDING_STOCK_UPGRADE_MISSILE_DEFENSE_INFANTRY: return BUILDING_STOCK;

        case BUILDING_ARMYCAMP_CREATE_CLUBMAN:
        case BUILDING_ARMYCAMP_CREATE_SLINGER:
        case BUILDING_ARMYCAMP_UPGRADE_CLUBMAN:
        case BUILDING_ARMYCAMP_CREATE_BROADSWORD:
        case BUILDING_ARMYCAMP_RESEARCH_LOGISTICS:
        case BUILDING_ARMYCAMP_UPGRADE_BROADSWORD: return BUILDING_ARMYCAMP;

        case BUILDING_RANGE_CREATE_BOWMAN:
        case BUILDING_RANGE_CREATE_CHARIOT_ARCHER:
        case BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN:
        case BUILDING_RANGE_UPGRADE_COMPOSITE_BOW: return BUILDING_RANGE;

        case BUILDING_STABLE_CREATE_SCOUT:
        case BUILDING_STABLE_CREATE_CHARIOT:
        case BUILDING_STABLE_CREATE_CAVALRY: return BUILDING_STABLE;

        case BUILDING_DOCK_CREATE_SAILING:
        case BUILDING_DOCK_CREATE_WOOD_BOAT:
        case BUILDING_DOCK_CREATE_SHIP: return BUILDING_DOCK;

        case BUILDING_SIEGE_CREATE_STONE_THROWER: return BUILDING_SIEGE;
        case BUILDING_COLLAGE_CREATE_HOPLITE: return BUILDING_COLLAGE;
        default: return -1;
    }
}

int typeToAction(int type)
{
    switch (type)
    {
        case AT_FARMER: return BUILDING_CENTER_CREATEFARMER;
        case AT_CLUBMAN: return BUILDING_ARMYCAMP_CREATE_CLUBMAN;
        case AT_SLINGER: return BUILDING_ARMYCAMP_CREATE_SLINGER;
        case AT_BOWMAN: return BUILDING_RANGE_CREATE_BOWMAN;
        case AT_SCOUT: return BUILDING_STABLE_CREATE_SCOUT;
        case AT_SWORDSMAN: return BUILDING_ARMYCAMP_CREATE_CLUBMAN;  // 战斧兵由棍棒兵升级而来
        case AT_IMPROVED: return -1;                                 // 敌方单位，不可训练
        case AT_CAVALRY: return BUILDING_STABLE_CREATE_CAVALRY;
        case AT_SHIP: return BUILDING_DOCK_CREATE_SHIP;
        case AT_STONE_THROWER: return BUILDING_SIEGE_CREATE_STONE_THROWER;
        case AT_PRIEST: return -1;  // 唯一单位，不可训练
        case AT_HOPLITE: return BUILDING_COLLAGE_CREATE_HOPLITE;
        case AT_CHARIOT: return BUILDING_STABLE_CREATE_CHARIOT;
        case AT_CHARIOT_ARCHER: return BUILDING_RANGE_CREATE_CHARIOT_ARCHER;
        case AT_BROADSWORDSMAN: return BUILDING_ARMYCAMP_CREATE_BROADSWORD;
        case AT_COMPOSITE_BOWMAN: return BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN;
        default: return -1;
    }
}

const std::vector<int>& Mgr::buildingsOf(int type) const
{
    static const std::vector<int> kEmpty;
    auto it = byType.find(type);
    return it == byType.end() ? kEmpty : it->second;
}

bool Mgr::canBuild(int dr, int ur) const
{
    if (!inMap(dr, ur)) return false;
    const tagTerrain& t = cell(dr, ur);
    return t.height != -1 && (t.type == MAPPATTERN_DESERT || t.type == MAPPATTERN_GRASS);
}

bool Mgr::walkable(int dr, int ur) const
{
    if (!inMap(dr, ur)) return false;
    const int t = cell(dr, ur).type;
    if (t != MAPPATTERN_GRASS && t != MAPPATTERN_DESERT) return false;
    return !blockCell(dr, ur);
}

bool Mgr::canPlace(int dr, int ur, int size) const
{
    if (dr < 0 || ur < 0 || dr + size - 1 >= MAP_L || ur + size - 1 >= MAP_U) return false;

    const int h = cell(dr, ur).height;
    for (int i = dr; i < dr + size; i++)
        for (int j = ur; j < ur + size; j++)
            if (!canBuild(i, j) || cell(i, j).height != h || blockCell(i, j)) return false;
    return true;
}

bool Mgr::nearLion(int dr, int ur, int radius) const
{
    for (const Pos& l : lionCells)
        if (std::abs(l.dr - dr) <= radius && std::abs(l.ur - ur) <= radius) return true;
    return false;
}

bool Mgr::enemyCorner(int dr, int ur) const
{
    const bool sameD = (dr < MAP_L / 2) == (base.dr < MAP_L / 2);
    const bool sameU = (ur < MAP_U / 2) == (base.ur < MAP_U / 2);
    return !sameD && !sameU;
}

int Mgr::lockOf(int enemySN) const
{
    const tagArmy* e = enemyArmy(enemySN);
    if (!e) return -1;

    const int sn = e->WorkObjectSN;
    if (sn == priest) return -1;  // 锁着祭司不算有诱饵
    if (allySet.count(sn)) return sn;

    const tagBuilding* b = building(sn);
    return b && b->Type == BUILDING_ARROWTOWER ? sn : -1;
}

void Mgr::markFootprint(const tagBuilding& b)
{
    const int s = buildingSize(b.Type);
    for (int i = b.BlockDR; i < b.BlockDR + s; i++)
        for (int j = b.BlockUR; j < b.BlockUR + s; j++)
            if (inMap(i, j)) blockCell(i, j) = 1;
}

void Mgr::worldRebuild(const tagInfo& info)
{
    farmerMap.clear();
    armyMap.clear();
    buildingMap.clear();
    resourceMap.clear();
    eArmyMap.clear();
    eBuildingMap.clear();
    lionSet.clear();
    allySet.clear();
    armySNs.clear();
    lionCells.clear();
    byType.clear();

    unitCnt.assign(32, 0);
    bldCnt.assign(32, 0);
    bldDoneCnt.assign(32, 0);
    blockCell.reset(0);

    gameFrame = info.GameFrame;

    // 构建反查
    for (const auto& f : info.farmers)
    {
        farmerMap[f.SN] = &f;
        unitCnt[AT_FARMER + 1]++;
        allySet.insert(f.SN);
    }

    for (const auto& a : info.armies)
    {
        armyMap[a.SN] = &a;
        armySNs.push_back(a.SN);
        unitCnt[a.Sort + 1]++;

        if (priest == -1 && a.Sort == AT_PRIEST) priest = a.SN;
        allySet.insert(a.SN);
    }

    for (const auto& a : info.enemy_armies) eArmyMap[a.SN] = &a;

    // 更新信息
    res.wood = info.Wood;
    res.meat = info.Meat;
    res.stone = info.Stone;
    res.gold = info.Gold;
    maxHuman = info.Human_MaxNum;
    stage = info.civilizationStage;
    theMap = info.theMap;

    for (const auto& r : info.resources)
    {
        resourceMap[r.SN] = &r;

        if (r.Type == RESOURCE_LION && r.Blood > 0)
        {
            lionCells.push_back({r.BlockDR, r.BlockUR});
            lionSet.insert(&r);
        }

        if (r.Type == RESOURCE_GAZELLE && r.Blood > 0) continue;
        if (resourceSize(r.Type) == 1) blockCell(r.BlockDR, r.BlockUR) = 1;
        else
        {
            const int a = (int)(r.DR / BLOCKSIDELENGTH + 0.5);
            const int b = (int)(r.UR / BLOCKSIDELENGTH + 0.5);
            blockCell(a - 1, b - 1) = 1;
            blockCell(a, b - 1) = 1;
            blockCell(a - 1, b) = 1;
            blockCell(a, b) = 1;
        }
    }

    for (const auto& b : info.buildings)
    {
        allySet.insert(b.SN);
        buildingMap[b.SN] = &b;
        bldCnt[b.Type]++;
        if (b.Percent >= 100) bldDoneCnt[b.Type]++;
        byType[b.Type].push_back(b.SN);
        markFootprint(b);

        if (base.dr == -1 && b.Type == BUILDING_CENTER)
        {
            base.dr = b.BlockDR, base.ur = b.BlockUR;
            baseF = FloatPos(base);
            baseF.sn = b.SN;
        }
    }

    for (const auto& b : info.enemy_buildings)
    {
        eBuildingMap[b.SN] = &b;
        markFootprint(b);
    }
}

void Mgr::navRebuild()
{
    nav.reset(-1);
    if (!navMark.ready()) navMark.reset(0);
    if (base.dr < 0) return;

    std::queue<Pos> q;
    const int baseLen = buildingSize(BUILDING_CENTER);
    for (int i = base.dr; i < base.dr + baseLen; i++)
        for (int j = base.ur; j < base.ur + baseLen; j++)
        {
            nav(i, j) = 0;
            q.push({i, j});
        }

    for (int dist = 1; q.size(); dist++)
    {
        int u = (int)q.size();
        for (int t = 0; t < u; t++)
        {
            const Pos c = q.front();
            q.pop();
            for (int k = 0; k < 8; k++)
            {
                const Pos n = {c.dr + dx[k], c.ur + dy[k]};
                if (!walkable(n.dr, n.ur) || nav(n) != -1) continue;
                if (dx[k] && dy[k] && (!walkable(c.dr + dx[k], c.ur) || !walkable(c.dr, c.ur + dy[k]))) continue;
                nav(n) = dist;
                q.push(n);
            }
        }
    }
}

void Mgr::ringAdd(Grid<int>& g, const Pos& around, int size, int cost, int inner, int outer, int delta)
{
    if (outer < inner || around.dr < 0) return;
    if (!navMark.ready()) navMark.reset(0);
    navMark.fill(0);

    std::queue<Pos> q;
    for (int i = around.dr; i < around.dr + size; i++)
        for (int j = around.ur; j < around.ur + size; j++)
        {
            if (!inMap(i, j) || navMark(i, j)) continue;
            q.push({i, j});
            navMark(i, j) = 1;
        }

    for (int r = 0; q.size(); r++)
    {
        if (r > outer) return;
        const int val = cost + r * delta;
        int u = (int)q.size();
        for (int i = 0; i < u; i++)
        {
            const Pos crt = q.front();
            q.pop();
            if (r >= inner) g(crt) += val;
            for (int k = 0; k < 8; k++)
            {
                const Pos next = {crt.dr + dx[k], crt.ur + dy[k]};
                if (!inMap(next.dr, next.ur) || navMark(next) || blocked(next.dr, next.ur)) continue;
                q.push(next);
                navMark(next) = 1;
            }
        }
    }
}

void Mgr::threatStamp(int td, int tu, int r)
{
    if (r < 0 || r >= (int)(sizeof(threatTbl) / sizeof(threatTbl[0]))) return;
    const int s = 2 * r + 1;
    if (threatTbl[r].empty())
    {
        threatTbl[r].assign(s * s, 0);
        for (int a = -r; a <= r; a++)
            for (int b = -r; b <= r; b++)
            {
                const double dd = std::sqrt((double)(a * a + b * b));
                if (dd <= r + EPS) threatTbl[r][(a + r) * s + (b + r)] = int(r - dd + 1);
            }
    }

    const std::vector<int>& t = threatTbl[r];
    const int lo_dr = std::max(0, td - r), hi_dr = std::min(MAP_L - 1, td + r);
    const int lo_ur = std::max(0, tu - r), hi_ur = std::min(MAP_U - 1, tu + r);
    for (int i = lo_dr; i <= hi_dr; i++)
        for (int j = lo_ur; j <= hi_ur; j++)
        {
            const int val = t[(i - td + r) * s + (j - tu + r)];
            if (val > 0) threat(i, j) += val;
        }
}

void Mgr::threatRebuild()
{
    threat.reset(0);

    for (const Pos& l : lionCells) threatStamp(l.dr, l.ur, LION_KEEP);

    for (const auto& it : eArmyMap)
    {
        const tagArmy& e = *it.second;
        const int r = lockOf(e.SN) >= 0 ? atkRange(e.Sort) + PRIEST_MARGIN : ENEMY_KEEP;
        threatStamp(e.BlockDR, e.BlockUR, r);
    }

    for (const auto& it : eBuildingMap)
        if (it.second->Type == BUILDING_ARROWTOWER) threatStamp(it.second->BlockDR, it.second->BlockUR, ENEMY_KEEP);
}

int Mgr::threatAt(int dr, int ur) const
{
    if (!threat.ready()) return 0;
    if (!inMap(dr, ur)) return 0;
    return threat(dr, ur);
}

void Mgr::phaseUpdate()
{
    maxStock = STOCK_MAX_BASE + gameFrame / 25 / 60 / 10;
    allInArmy = gameFrame > ARMY_ALL_IN;
    allInPriest = gameFrame > PRIEST_ALL_IN;
}

void Mgr::workerTask(int workerSN, int targetSN)
{
    const tagFarmer* f = farmer(workerSN);
    if (!f) return;
    if (f->WorkObjectSN == targetSN && f->NowState != HUMAN_STATE_IDLE) return;
    HumanAction(workerSN, targetSN);
}

void Mgr::laborRebuild()
{
    laborPool.clear();
    claimedThisFrame.clear();

    for (const auto& it : farmerMap)
        if (!workerBusy(it.first)) laborPool.push_back(it.first);
}

int Mgr::nearestOf(const std::vector<int>& cand, const FloatPos& at) const
{
    int best = -1;
    double bestDis = 0;
    for (int sn : cand)
    {
        const tagFarmer* f = farmer(sn);
        if (!f) continue;
        const double d = disSq(at, FloatPos(f->DR, f->UR));
        if (best < 0 || d < bestDis)
        {
            bestDis = d;
            best = sn;
        }
    }
    return best;
}

int Mgr::claimWorker(const FloatPos& at, bool steal)
{
    std::vector<int> cand;
    cand.reserve(laborPool.size());
    for (int sn : laborPool)
        if (!claimedThisFrame.count(sn)) cand.push_back(sn);

    int best = nearestOf(cand, at);
    if (best >= 0)
    {
        auto it = std::find(laborPool.begin(), laborPool.end(), best);
        if (it != laborPool.end())
        {
            *it = laborPool.back();
            laborPool.pop_back();
        }
        claimedThisFrame.insert(best);
        return best;
    }
    if (!steal) return -1;

    cand.clear();
    for (const auto& it : farmerMap)
    {
        const int sn = it.first;
        if (claimedThisFrame.count(sn) || workerPinned(sn)) continue;
        cand.push_back(sn);
    }

    best = nearestOf(cand, at);
    if (best < 0) return -1;

    workerDrop(best);  // 防止 sn 同时属于多个岗位
    claimedThisFrame.insert(best);
    return best;
}

void Mgr::freeWorker(int sn)
{
    if (farmer(sn)) laborPool.push_back(sn);
}

void Economy::buildDepots()
{
    foodDepots.clear();
    resDepots.clear();
    for (const auto& it : buildingMap)
    {
        const tagBuilding& b = *it.second;
        if (b.Percent < 100) continue;

        const double half = buildingSize(b.Type) * 0.5;
        const FloatPos at((b.BlockDR + half) * BLOCKSIDELENGTH, (b.BlockUR + half) * BLOCKSIDELENGTH);
        if (b.Type == BUILDING_CENTER) foodDepots.push_back(at), resDepots.push_back(at);
        else if (b.Type == BUILDING_GRANARY) foodDepots.push_back(at);
        else if (b.Type == BUILDING_STOCK) resDepots.push_back(at);
    }
}

double Economy::depotCost(const FloatPos& at, const std::vector<FloatPos>& depots) const
{
    double best = -1;
    for (const FloatPos& d : depots)
    {
        const double v = dis(at, d);
        if (best < 0 || v < best) best = v;
    }
    if (best < 0) return dis(at, baseF);
    return best;
}

bool Economy::standCell(const tagResource* r, Pos& out) const
{
    const int size = resourceSize(r->Type);
    int _dr, _ur;
    if (size == 1) _dr = r->BlockDR, _ur = r->BlockUR;
    else
    {
        _dr = (int)(r->DR / BLOCKSIDELENGTH + 0.5) - 1;
        _ur = (int)(r->UR / BLOCKSIDELENGTH + 0.5) - 1;
    }

    out = {-1, -1};
    for (int i = _dr - 1; i <= _dr + size; i++)
        for (int j = _ur - 1; j <= _ur + size; j++)
        {
            if (i >= _dr && i < _dr + size && j >= _ur && j < _ur + size) continue;
            if (!inMap(i, j)) continue;
            if (nav(i, j) == -1 || standTaken(i, j)) continue;

            out = {i, j};
            return true;
        }
    return false;
}

void Economy::gatherFrame()
{
    buildDepots();

    for (int k = 0; k < RK_COUNT; k++) pools[k].spots.clear();
    standTaken.reset(0);

    // 解绑
    for (auto it = spotOfWorker.begin(); it != spotOfWorker.end();)
    {
        const tagResource* r = resource(it->second);
        const bool gone =
            !farmer(it->first) || !r || kindOf(r->Type) == RK_COUNT || (kindOf(r->Type) == RK_CORPSE && r->Blood > 0);
        if (!gone)
        {
            it++;
            continue;
        }
        workerOfSpot.erase(it->second);
        it = spotOfWorker.erase(it);
    }

    // 候选
    struct Cand
    {
        const tagResource* r;
        ResKind k;
        double cost;
        bool held;
    };
    std::vector<Cand> cand;
    cand.reserve(resourceMap.size());

    for (const auto& it : resourceMap)
    {
        const tagResource* r = it.second;
        const ResKind k = kindOf(r->Type);
        if (k == RK_COUNT) continue;
        if (k == RK_CORPSE)
        {
            if (r->Blood > 0) continue;
            if (!workerOfSpot.count(r->SN) && nearLion(r->BlockDR, r->BlockUR, LION_KEEP)) continue;
        }

        const FloatPos at(r->DR, r->UR);
        const double cost = depotCost(at, k == RK_BUSH ? foodDepots : resDepots);
        cand.push_back({r, k, cost, workerOfSpot.count(r->SN) > 0});
    }

    std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b)
    {
        if (a.held != b.held) return a.held;
        if (a.cost != b.cost) return a.cost < b.cost;
        return a.r->SN < b.r->SN;
    });

    // 选中
    std::unordered_set<int> alive;
    alive.reserve(cand.size());
    for (const Cand& x : cand)
    {
        Pos stand;
        if (!standCell(x.r, stand)) continue;
        standTaken(stand) = 1;

        GatherSpot s;
        s.sn = x.r->SN;
        s.at = FloatPos(x.r->DR, x.r->UR);
        s.stand = stand;
        s.cost = x.cost;
        pools[x.k].spots.push_back(s);
        alive.insert(x.r->SN);
    }

    for (int k = 0; k < RK_COUNT; k++)
        std::sort(pools[k].spots.begin(), pools[k].spots.end(), [](const GatherSpot& a, const GatherSpot& b)
        { return a.cost != b.cost ? a.cost < b.cost : a.sn < b.sn; });

    // 资源在但是没空地的, 放人
    for (auto it = spotOfWorker.begin(); it != spotOfWorker.end();)
    {
        if (alive.count(it->second))
        {
            it++;
            continue;
        }
        workerOfSpot.erase(it->second);
        it = spotOfWorker.erase(it);
    }
}

void Economy::gatherReset()
{
    for (int k = 0; k < RK_COUNT; k++) pools[k].desired = 0;
}

void Economy::dropSpot(int workerSN, bool toFree)
{
    auto it = spotOfWorker.find(workerSN);
    if (it == spotOfWorker.end()) return;
    workerOfSpot.erase(it->second);
    spotOfWorker.erase(it);
    if (toFree) freeWorker(workerSN);
}

int Economy::poolRoom(ResKind k) const { return (int)pools[k].spots.size() - pools[k].desired; }

int Economy::spotsWithin(ResKind k, double limit) const
{
    int cnt = 0;
    for (const GatherSpot& s : pools[k].spots)
        if (s.cost <= limit) cnt++;
        else break;
    return cnt;
}

void Economy::toPool(int& from, int num, ResKind k, double limit)
{
    int room = poolRoom(k);
    if (limit >= 0) room = min(room, spotsWithin(k, limit) - pools[k].desired);
    const int t = min(max(0, min(from, num)), max(0, room));
    from -= t;
    pools[k].desired += t;
}

void Economy::gatherRun()
{
    for (int k = 0; k < RK_COUNT; k++)
    {
        GatherPool& p = pools[k];
        const int active = min(p.desired, (int)p.spots.size());

        for (int i = active; i < (int)p.spots.size(); i++)
        {
            auto it = workerOfSpot.find(p.spots[i].sn);
            if (it != workerOfSpot.end()) dropSpot(it->second, true);
        }
    }

    for (int k = 0; k < RK_COUNT; k++)
    {
        GatherPool& p = pools[k];
        const int active = min(p.desired, (int)p.spots.size());

        for (int i = 0; i < active; i++)
        {
            const GatherSpot& s = p.spots[i];

            if (!workerOfSpot.count(s.sn))
            {
                const int sn = claimWorker(FloatPos(s.stand));
                if (sn < 0) continue;
                workerOfSpot[s.sn] = sn;
                spotOfWorker[sn] = s.sn;
            }
            workerTask(workerOfSpot[s.sn], s.sn);
        }
    }
}

bool Economy::huntable(const tagResource* r) const
{ return r->Type == RESOURCE_GAZELLE && r->Blood > 0 && !nearLion(r->BlockDR, r->BlockUR, LION_KEEP); }

HuntSite* Economy::siteOf(int siteID)
{
    auto it = huntByID.find(siteID);
    return it == huntByID.end() ? nullptr : it->second;
}

void Economy::formHunts(const std::vector<const tagResource*>& sor, int threshold)
{
    hunts.clear();
    if (sor.size())
    {
        const double radiusSq = (double)threshold * BLOCKSIDELENGTH * threshold * BLOCKSIDELENGTH;
        std::vector<bool> used(sor.size(), false);
        std::unordered_set<int> taken;  // 已经被占用的siteID, 新出现的site不要使用

        for (int i = 0; i < (int)sor.size(); i++)
        {
            if (used[i]) continue;

            used[i] = true;
            HuntSite site;
            site.members.push_back(sor[i]->SN);
            double sumDR = sor[i]->DR, sumUR = sor[i]->UR;

            std::vector<int> q = {i};
            for (int h = 0; h < (int)q.size(); h++)
            {
                const tagResource* cur = sor[q[h]];
                const FloatPos cp = {cur->DR, cur->UR};
                for (int j = 0; j < (int)sor.size(); j++)
                {
                    if (used[j]) continue;
                    if (disSq(FloatPos(sor[j]->DR, sor[j]->UR), cp) > radiusSq) continue;

                    used[j] = true;
                    site.members.push_back(sor[j]->SN);
                    sumDR += sor[j]->DR;
                    sumUR += sor[j]->UR;
                    q.push_back(j);
                }
            }

            const int n = (int)site.members.size();
            site.center = {sumDR / n, sumUR / n};

            for (int sn : site.members)
            {
                auto it = huntOfSN.find(sn);
                if (it == huntOfSN.end() || taken.count(it->second)) continue;
                if (site.id < 0 || it->second < site.id) site.id = it->second;
            }
            if (site.id < 0) site.id = nextSiteID++;
            taken.insert(site.id);

            hunts.push_back(site);
        }

        hunts.erase(remove_if(hunts.begin(), hunts.end(), [&](const HuntSite& s) { return s.members.size() <= 2; }),
                    hunts.end());

        std::sort(hunts.begin(), hunts.end(), [&](const HuntSite& a, const HuntSite& b)
        { return disSq(baseF, a.center) < disSq(baseF, b.center); });
    }

    huntByID.clear();
    huntOfSN.clear();
    for (HuntSite& s : hunts)
    {
        huntByID[s.id] = &s;
        for (int sn : s.members) huntOfSN[sn] = s.id;
    }
}

void Economy::restoreStaff()
{
    for (auto it = staffHunt.begin(); it != staffHunt.end();)
    {
        const int sn = it->first;
        HuntSite* s = farmer(sn) ? siteOf(it->second) : nullptr;
        if (!s)
        {
            it = staffHunt.erase(it);
            continue;
        }
        s->staff.push_back(sn);
        it++;
    }
}

void Economy::huntFrame()
{
    std::vector<const tagResource*> prey;  // 聚类输入
    for (const auto& it : resourceMap)
        if (huntable(it.second)) prey.push_back(it.second);

    formHunts(prey, 6);
    restoreStaff();

    // 过期数据
    for (auto it = huntPrey.begin(); it != huntPrey.end();)
    {
        const tagResource* r = resource(it->second);
        if (r && r->Blood > 0) it++;
        else it = huntPrey.erase(it);
    }

    for (auto it = huntPrey.begin(); it != huntPrey.end();)
        if (huntByID.count(it->first)) it++;
        else it = huntPrey.erase(it);
}

void Economy::huntReset()
{
    for (HuntSite& s : hunts) s.desired = 0;
}

void Economy::huntDetach(int sn)
{
    auto sh = staffHunt.find(sn);
    if (sh == staffHunt.end()) return;

    auto hb = huntByID.find(sh->second);
    if (hb != huntByID.end())
    {
        auto& v = hb->second->staff;
        auto vit = std::find(v.begin(), v.end(), sn);
        if (vit != v.end())
        {
            *vit = v.back();
            v.pop_back();
        }
    }
    staffHunt.erase(sh);
}

void Economy::huntAssign(int x, HuntSite& site)
{
    while (x-- > 0)
    {
        const int sn = claimWorker(site.center);
        if (sn < 0) return;
        site.staff.push_back(sn);
        staffHunt[sn] = site.id;
    }
}

void Economy::huntFree(int x, HuntSite& site)
{
    while (x > 0 && site.staff.size())
    {
        const int sn = site.staff.back();
        site.staff.pop_back();
        staffHunt.erase(sn);
        freeWorker(sn);
        x--;
    }
}

void Economy::toHunt(int& from, int num, int siteIdx)
{
    if (siteIdx < 0 || siteIdx >= (int)hunts.size()) return;
    const int t = max(0, min(from, num));
    from -= t;
    hunts[siteIdx].desired += t;
}

void Economy::huntRun()
{
    for (HuntSite& s : hunts)
        if ((int)s.staff.size() > s.desired) huntFree((int)s.staff.size() - s.desired, s);

    for (HuntSite& s : hunts)
    {
        if ((int)s.staff.size() < s.desired) huntAssign(s.desired - (int)s.staff.size(), s);
        if (s.staff.size()) driveHunt(s);
    }
}

void Economy::driveHunt(HuntSite& site)
{
    const tagResource* prey = nullptr;

    auto cur = huntPrey.find(site.id);
    if (cur != huntPrey.end())
    {
        const tagResource* r = resource(cur->second);
        if (r && huntable(r)) prey = r;
    }

    if (!prey)  // 挑最近的一只
    {
        double bestDis = 0;
        for (int sn : site.members)
        {
            const tagResource* r = resource(sn);
            if (!r || !huntable(r)) continue;

            const double d = disSq(baseF, FloatPos(r->DR, r->UR));
            if (!prey || d < bestDis)
            {
                bestDis = d;
                prey = r;
            }
        }
        if (!prey)  // 打光, 交给采集系统
        {
            huntPrey.erase(site.id);
            huntFree((int)site.staff.size(), site);
            return;
        }
        huntPrey[site.id] = prey->SN;
    }

    for (int sn : site.staff) workerTask(sn, prey->SN);
}

void Economy::farmFrame()
{
    farmList.clear();
    for (int sn : buildingsOf(BUILDING_FARM))
    {
        const tagBuilding* b = building(sn);
        if (b && b->Percent >= 100) farmList.push_back(sn);
    }
}

void Economy::farmUnbind(std::unordered_map<int, int>::iterator it)
{
    workerToFarm.erase(it->second);
    freeWorker(it->second);
    farmToWorker.erase(it);
}

void Economy::farmRun()
{
    std::unordered_set<int> live(farmList.begin(), farmList.end());

    for (auto it = farmToWorker.begin(); it != farmToWorker.end();)
    {
        auto cur = it++;
        if (!live.count(cur->first) || !farmer(cur->second)) farmUnbind(cur);
    }

    for (int farmSN : farmList)
    {
        auto it = farmToWorker.find(farmSN);
        if (it == farmToWorker.end())
        {
            const tagBuilding* farm = building(farmSN);
            const Pos p(farm->BlockDR, farm->BlockUR);
            const int sn = claimWorker(FloatPos(p));
            if (sn < 0) continue;

            workerToFarm[sn] = farmSN;
            it = farmToWorker.emplace(farmSN, sn).first;
        }

        workerTask(it->second, farmSN);
    }
}

void Economy::rebalance(int population, const Stock& need)
{
    const int stoneWant = (phaseChanged && res.stone < 600) ? 2 : 0;
    popStone = min(stoneWant, population);
    const int balancePop = max(0, population - popStone);

    if (!phaseChanged && stage == CIVILIZATION_TOOLAGE)
    {
        popFood = balancePop * 3 / 5;
        popWood = balancePop - popFood;
        return;
    }
    else if (!phaseChanged && stage == CIVILIZATION_BRONZEAGE)
    {
        phaseChanged = true;
        lastSnapFrame = gameFrame;  // 进入新阶段, 从下次 60s 才平衡
        prevStock = res;
    }

    const int sumFW = popFood + popWood;
    if (sumFW < balancePop) popWood += balancePop - sumFW;  // 默认进树木
    else if (sumFW > balancePop)
    {
        int excess = sumFW - balancePop;
        while (excess > 0)
        {
            if (popFood >= popWood && popFood > 0)
            {
                popFood--;
                excess--;
            }
            else if (popWood > 0)
            {
                popWood--;
                excess--;
            }
            else break;
        }
    }

    if (!phaseChanged || gameFrame - lastSnapFrame < ECON_SNAP || balancePop == 0) return;

    // 每人每分钟多少资源
    const double rateFood = (res.meat - prevStock.meat) / (double)ECON_SNAP;
    const double rateWood = (res.wood - prevStock.wood) / (double)ECON_SNAP;
    const double per0 = popFood > 0 ? rateFood / popFood : 0.0;
    const double per1 = popWood > 0 ? rateWood / popWood : 0.0;

    // 缺口
    const double gap0 = max(0.0, (double)need.meat - res.meat);
    const double gap1 = max(0.0, (double)need.wood - res.wood);

    // 缺口补全(任意比例不超过4 : 1)
    if (per0 < EPS || per1 < EPS || (gap0 < EPS && gap1 < EPS))
    {
        popFood = balancePop / 2;
        popWood = balancePop - popFood;
    }
    else if (gap0 < EPS)
    {
        popFood = balancePop / 5;
        popWood = balancePop - popFood;
    }
    else if (gap1 < EPS)
    {
        popWood = balancePop / 5;
        popFood = balancePop - popWood;
    }
    else
    {
        double ratio = gap0 * per1 / gap1 / per0;
        ratio = min(ratio, 4.0);
        ratio = max(ratio, 0.25);

        popFood = (int)(ratio / (1.0 + ratio) * balancePop);
        popWood = balancePop - popFood;
    }

    lastSnapFrame = gameFrame;
    prevStock = res;
}

bool Industry::buildAvailable(int type) const
{
    switch (type)
    {
        case BUILDING_FARM: return buildingCount(BUILDING_MARKET, true) > 0;
        default: return true;
    }
}

void Industry::buildFrame()
{
    builds.clear();
    if (!costMap.ready()) costMap.reset(0);

    if (savePlanned < 0 || gameFrame - savePlanned >= STOCK_PLAN_GAP)
    {
        saveMapBuild();
        savePlanned = gameFrame;
    }
}

void Industry::placeMask(int type, int size)
{
    placeOk.reset(0);

    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
        {
            if (!canPlace(i, j, size)) continue;

            bool reach = false;
            for (int a = i; a < i + size && !reach; a++)
                for (int b = j; b < j + size && !reach; b++)
                    if (nav(a, b) != -1) reach = true;
            if (!reach) continue;

            placeOk(i, j) = 1;
        }
}

Pos Industry::findSpot(int type)
{
    costMap.reset(0);
    const int size = buildingSize(type);
    const int baseLen = buildingSize(BUILDING_CENTER);

    placeMask(type, size);

    // 通用距离惩罚
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (nav(i, j) == -1) costMap(i, j) += MAP_L + MAP_U;
            else costMap(i, j) += nav(i, j);

    // 靠近基地额外惩罚
    ringAdd(costMap, base, baseLen, 0, 0, 2, PLACE_NEAR_BASE);

    // 通用靠近建筑惩罚
    for (const auto& it : buildingMap)
        ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(it.second->Type), 0, 0, 1,
                PLACE_ADJACENT);

    // 通用靠近资源惩罚
    for (const auto& it : resourceMap)
    {
        const tagResource* r = it.second;
        if (r->Type == RESOURCE_GAZELLE && r->Blood > 0) continue;

        const int len = resourceSize(r->Type);
        int dr, ur;
        if (len == 1) dr = r->BlockDR, ur = r->BlockUR;
        else
        {
            dr = (int)(r->DR / BLOCKSIDELENGTH + 0.5) - 1;
            ur = (int)(r->UR / BLOCKSIDELENGTH + 0.5) - 1;
        }

        for (int a = dr - 1; a <= dr + len; a++)
            for (int b = ur - 1; b <= ur + len; b++)
            {
                if (!inMap(a, b)) continue;
                if (a >= dr && a < dr + len && b >= ur && b < ur + len) continue;
                costMap(a, b) += PLACE_NEAR_RES;
            }
    }

    // 根据建筑类型特化
    switch (type)
    {
        case BUILDING_FARM:  // 靠近谷仓, 基地排布
            for (const auto& it : buildingMap)
            {
                if (it.second->Type == BUILDING_GRANARY)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_GRANARY),
                            PLACE_BAND_BONUS, 2, 5);

                if (it.second->Type == BUILDING_CENTER)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_CENTER),
                            PLACE_BAND_BONUS, 2, 5);
            }
            break;

        case BUILDING_GRANARY:  // 和基地以及其他谷仓保持6-8格左右的距离, 靠近浆果丛
            ringAdd(costMap, base, baseLen, PLACE_BAND_BONUS, 6, 8);
            ringAdd(costMap, base, baseLen, PLACE_BAND_FAIL, 0, 5);
            for (const auto& it : buildingMap)
                if (it.second->Type == BUILDING_GRANARY)
                {
                    const Pos p = {it.second->BlockDR, it.second->BlockUR};
                    const int len = buildingSize(BUILDING_GRANARY);
                    ringAdd(costMap, p, len, PLACE_BAND_BONUS, 6, 8);
                    ringAdd(costMap, p, len, PLACE_BAND_FAIL, 0, 5);
                }
            for (const GatherSpot& s : pools[RK_BUSH].spots)
                ringAdd(costMap, {(int)(s.at.dr / BLOCKSIDELENGTH), (int)(s.at.ur / BLOCKSIDELENGTH)}, 1,
                        PLACE_RES_BONUS, 0, 4);
            break;

        case BUILDING_ARMYCAMP:
        case BUILDING_COLLAGE:
        case BUILDING_RANGE:
            ringAdd(costMap, base, baseLen, PLACE_BAND_FAIL, 0, 6);

            for (const auto& it : buildingMap)
                if (it.second->Type == BUILDING_GRANARY)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_GRANARY),
                            PLACE_BAND_FAIL, 0, 5);

            for (const auto& it : farmerMap)
            {
                const tagFarmer& f = *(it.second);
                if (workerPinned(f.SN)) continue;

                ringAdd(costMap, {f.BlockDR, f.BlockUR}, 1, PLACE_BAND_BONUS, 0, 4);
            }
            break;

        case BUILDING_HOME:  // 紧靠其他房屋建造(形成大矩形占地), 和基地保持5格的距离
            for (const auto& it : buildingMap)
                if (it.second->Type == BUILDING_HOME)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_HOME),
                            -PLACE_ADJACENT * 3, 1, 1);
            ringAdd(costMap, base, baseLen, PLACE_BAND_FAIL, 0, 5);
            break;

        case BUILDING_STOCK: ringAdd(costMap, base, buildingSize(BUILDING_CENTER), PLACE_IMPOSSIBLE, 60, 1000); break;

        case BUILDING_ARROWTOWER:
            for (const auto& it : buildingMap)
                if (it.second->Type == BUILDING_ARROWTOWER)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_ARROWTOWER),
                            PLACE_ADJACENT * 3, 0, 9);
            break;
    }

    const int area = size * size;
    const int W = MAP_U + 1;
    psum.assign((size_t)(MAP_L + 1) * W, 0);
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            psum[(i + 1) * W + (j + 1)] =
                psum[i * W + (j + 1)] + psum[(i + 1) * W + j] - psum[i * W + j] + costMap(i, j);

    Pos best = {-1, -1};
    long long bestCost = 0;
    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
        {
            if (!placeOk(i, j)) continue;

            const long long block = psum[(i + size) * W + (j + size)] - psum[i * W + (j + size)] -
                                    psum[(i + size) * W + j] + psum[i * W + j];
            long long v = block / area;

            auto fit = failedSpots.find(cellIdx(i, j));
            if (fit != failedSpots.end()) v += (long long)PLACE_FAILED * fit->second;

            if (type == BUILDING_STOCK) v -= saveMap(i, j) / PLACE_SAVE_SCALE;  // 智能建造

            if (best.dr < 0 || v < bestCost)
            {
                bestCost = v;
                best = {i, j};
            }
        }
    return best;
}

void Industry::saveMapBuild()
{
    saveMap.reset(0);
    bestSave = 0;

    const int size = buildingSize(BUILDING_STOCK);
    const double off = (size - 1) * 0.5;  // 左上角到占地中心

    static const ResKind kServed[] = {RK_WOOD, RK_CORPSE};  // 尚未加入黄金, 如果加入要手动同步调整开销倍率和折算值

    for (const ResKind k : kServed)
    {
        const int carry = (k == RK_GOLD) ? FARMER_CARRYLIMIT_GOLD : FARMER_CARRYLIMIT_WOOD;
        const int discount = (k == RK_CORPSE) ? 1 : STOCK_DISCOUNT_OTHER;

        struct Serve
        {
            double worth;
            int trips;
            const GatherSpot* s;
        };
        std::vector<Serve> pick;
        pick.reserve(pools[k].spots.size());
        for (const GatherSpot& s : pools[k].spots)
        {
            const tagResource* r = resource(s.sn);
            if (!r) continue;
            const int trips = (r->Cnt + carry - 1) / carry;
            pick.push_back({trips * s.cost, trips, &s});
        }
        std::sort(pick.begin(), pick.end(), [](const Serve& a, const Serve& b)
        { return a.worth != b.worth ? a.worth > b.worth : a.s->sn < b.s->sn; });
        if ((int)pick.size() > STOCK_SERVE) pick.resize(STOCK_SERVE);

        for (const Serve& p : pick)
        {
            const GatherSpot& s = *p.s;
            const int trips = p.trips;

            const double reach = s.cost / BLOCKSIDELENGTH;
            if (reach <= 4) continue;  // 已经很近了

            const int lo_dr = max(0, (int)std::floor(s.at.dr / BLOCKSIDELENGTH - reach - off));
            const int hi_dr = min(MAP_L - size, (int)std::ceil(s.at.dr / BLOCKSIDELENGTH + reach));
            const int lo_ur = max(0, (int)std::floor(s.at.ur / BLOCKSIDELENGTH - reach - off));
            const int hi_ur = min(MAP_U - size, (int)std::ceil(s.at.ur / BLOCKSIDELENGTH + reach));

            for (int i = lo_dr; i <= hi_dr; i++)
                for (int j = lo_ur; j <= hi_ur; j++)
                {
                    const FloatPos ctr((i + off) * BLOCKSIDELENGTH, (j + off) * BLOCKSIDELENGTH);
                    const double gain = s.cost - dis(s.at, ctr);
                    if (gain <= 0) continue;
                    saveMap(i, j) += (int)(trips * 2 * gain / HUMAN_SPEED / discount);
                }
        }
    }

    placeMask(BUILDING_STOCK, size);
    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
            if (placeOk(i, j) && saveMap(i, j) > bestSave) bestSave = saveMap(i, j);
}

void Industry::wantStock(int priority)
{
    const int have = buildingCount(BUILDING_STOCK);
    if (have >= maxStock) return;

    for (int sn : buildingsOf(BUILDING_STOCK))
        if (building(sn)->Percent < 100) return;

    const double cost = 100 / FARMER_CONSTRUCTSPEED + BUILD_STOCK_WOOD / FARMER_GATHERSPEED_WOOD;

    if (bestSave < cost * STOCK_PAYBACK) return;
    wantBuilding(BUILDING_STOCK, have + 1, priority);
}

int Industry::queuedBuild(int type) const
{
    int cnt = 0;
    for (const BuildSite& s : sites)
        if (s.type == type && s.sn < 0) cnt++;
    for (const auto& b : builds)
        if (b.second == type) cnt++;
    return cnt;
}

void Industry::wantBuilding(int buildingType, int total, int priority)
{
    if (!buildAvailable(buildingType)) return;
    int diff = total - buildingCount(buildingType) - queuedBuild(buildingType);
    for (; diff > 0; diff--) builds.insert({priority, buildingType});
}

Stock Industry::buildDemand() const
{
    Stock need;
    for (const auto& p : builds)
    {
        need.wood += buildWoodCost(p.second);
        need.stone += buildStoneCost(p.second);
    }
    return need;
}

void Industry::buildRun()
{
    for (auto it = sites.begin(); it != sites.end();)
    {
        BuildSite& s = *it;
        for (auto wit = s.workers.begin(); wit != s.workers.end();)
            if (farmer(*wit)) ++wit;
            else wit = s.workers.erase(wit);

        if (s.sn < 0)
        {
            for (int sn : buildingsOf(s.type))
            {
                const tagBuilding* b = building(sn);
                if (b->BlockDR != s.site.dr || b->BlockUR != s.site.ur) continue;
                s.sn = sn;
                break;
            }
            if (s.sn < 0)
            {
                if (!s.workers.empty()) failedSpots[cellIdx(s.site.dr, s.site.ur)]++;
                for (int sn : s.workers) freeWorker(sn);
                it = sites.erase(it);
                continue;
            }
        }

        const tagBuilding* b = building(s.sn);
        if (!b || b->Percent >= 100)
        {
            for (int sn : s.workers) freeWorker(sn);
            it = sites.erase(it);
            continue;
        }
        ++it;
    }

    for (BuildSite& s : sites)
    {
        while ((int)s.workers.size() < CREW_BUILD)
        {
            const int sn = claimWorker(FloatPos(s.site), true);
            if (sn < 0) break;
            s.workers.insert(sn);
        }
        for (int sn : s.workers) workerTask(sn, s.sn);
    }

    Stock used;
    std::vector<Pos> justPlaced;
    for (auto it = builds.end(); it != builds.begin();)
    {
        --it;
        const int type = it->second;

        Stock probe = used;
        probe.wood += buildWoodCost(type);
        probe.stone += buildStoneCost(type);

        const Stock left = avail();
        if (left.wood < probe.wood) break;
        if (left.stone < probe.stone) break;

        const Pos spot = findSpot(type);
        if (spot.dr < 0)
        {
            it = builds.erase(it);
            continue;
        }

        bool dup = false;
        for (const Pos& p : justPlaced)
            if (p.dr == spot.dr && p.ur == spot.ur)
            {
                dup = true;
                break;
            }
        if (dup)
        {
            it = builds.erase(it);
            continue;
        }

        const int first = claimWorker(FloatPos(spot), true);
        if (first < 0) break;

        BuildSite s;
        s.type = type;
        s.site = spot;
        s.workers.insert(first);
        sites.push_back(s);
        justPlaced.push_back(spot);
        used = probe;
        it = builds.erase(it);

        orderBuild(first, type, spot.dr, spot.ur);
    }
}

void Industry::prodFrame()
{
    prods.clear();
    destroyCnt = -1;
}

bool Industry::techAvailable(int action) const
{
    switch (action)
    {
        case BUILDING_CENTER_UPGRADE: return stage == CIVILIZATION_TOOLAGE && buildingCount(BUILDING_MARKET, true) > 0;
        case BUILDING_MARKET_WHEEL_UPGRADE:
            return buildingCount(BUILDING_MARKET) > 0 && stage == CIVILIZATION_BRONZEAGE;
        default: return true;
    }
}

int Industry::idleHost(int buildingType, const std::set<int>& busy) const
{
    for (int sn : buildingsOf(buildingType))
    {
        const tagBuilding* b = building(sn);
        if (b->Percent >= 100 && b->Project == 0 && !busy.count(sn)) return sn;
    }
    return -1;
}

int Industry::queuedProd(int action) const
{
    const int host = actionHost(action);
    int cnt = 0;
    for (const auto& p : prods)
        if (p.second == action) cnt++;
    for (int sn : buildingsOf(host))
        if (building(sn)->Project == action) cnt++;
    return cnt;
}

void Industry::wantUnit(int type, int total, int priority)
{
    const int action = typeToAction(type);

    int diff = total - unitCount(type) - queuedProd(action);
    for (; diff > 0; diff--) prods.insert({priority, action});
    if (diff < 0) destroyCnt = -diff;
}

void Industry::wantTech(int action, int priority)
{
    if (techAvailable(action) && !doneTech.count(action)) prods.insert({priority, action});
}

Stock Industry::prodDemand() const
{
    Stock need;
    for (const auto& p : prods) need += actionCost(p.second);
    return need;
}

void Industry::prodRun()
{
    std::set<int> busy;  // 本帧已经派活的建筑
    std::set<int> full;  // 这类建筑本帧已经找不出空的了
    for (auto it = prods.end(); it != prods.begin();)
    {
        it--;
        const int action = it->second;
        const int hostType = actionHost(action);
        if (full.count(hostType)) continue;

        const int host = idleHost(hostType, busy);
        if (host < 0)
        {
            full.insert(hostType);
            continue;
        }

        const Stock cost = actionCost(action);
        if (!afford(cost)) continue;

        orderBuilding(host, action);
        doneTech.insert(action);
        busy.insert(host);
        held += cost;
        it = prods.erase(it);
    }
}

void Industry::prodDestroy()
{
    if (destroyCnt <= 0) return;

    int left = destroyCnt;
    for (const auto& it : farmerMap)
    {
        if (left-- <= 0) break;
        order(it.first, it.first);
    }
}

void War::scoutFrame()
{
    if (army(scoutSN)) return;

    scoutSN = priest;
    for (int sn : armySNs)
    {
        const tagArmy* a = army(sn);
        if (a && a->Sort == AT_SCOUT)
        {
            scoutSN = sn;
            break;
        }
    }
}

void War::scoutFlood(const Pos& from, bool avoidThreat)
{
    scoutDist.reset(-1);
    scoutPrev.reset(-1);
    if (!inMap(from.dr, from.ur)) return;

    std::queue<Pos> q;
    scoutDist(from) = 0;
    q.push(from);

    while (q.size())
    {
        const Pos cur = q.front();
        q.pop();
        for (int d = 0; d < 8; d++)
        {
            const Pos n = {cur.dr + dx[d], cur.ur + dy[d]};
            if (!walkable(n.dr, n.ur)) continue;
            if (scoutDist(n) >= 0) continue;  // used
            if (avoidThreat && threatAt(n.dr, n.ur) > 0) continue;
            if (dx[d] && dy[d] && (!walkable(cur.dr + dx[d], cur.ur) || !walkable(cur.dr, cur.ur + dy[d])))
                continue;  // 对角
            scoutDist(n) = scoutDist(cur) + 1;
            scoutPrev(n) = cellIdx(cur.dr, cur.ur);
            q.push(n);
        }
    }
}

void War::unknownRebuild()
{
    // unknownRow[i * (MAP_U + 1) + j] = 第 i 行前 j 格里的未知格数
    const int W = MAP_U + 1;
    unknownRow.assign((size_t)MAP_L * W, 0);
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            unknownRow[i * W + j + 1] = unknownRow[i * W + j] + (cell(i, j).type == MAPPATTERN_UNKNOWN);
}

int War::wpGain(const Pos& p) const
{
    const int W = MAP_U + 1;
    const int r = SCOUT_VIEW;
    int sum = 0;
    for (int ddr = -r; ddr <= r; ddr++)
    {
        const int i = p.dr + ddr;
        if (i < 0 || i >= MAP_L) continue;
        const int half = (int)std::sqrt((double)(r * r - ddr * ddr));
        const int lo = max(0, p.ur - half), hi = min(MAP_U - 1, p.ur + half);
        if (lo > hi) continue;
        sum += unknownRow[i * W + hi + 1] - unknownRow[i * W + lo];
    }
    return sum;
}

bool War::nearestStand(const Pos& p, int r, Pos& out) const
{
    double best = 0;
    out = {-1, -1};
    for (int i = max(0, p.dr - r); i <= min(MAP_L - 1, p.dr + r); i++)
        for (int j = max(0, p.ur - r); j <= min(MAP_U - 1, p.ur + r); j++)
        {
            if (scoutDist(i, j) < 0 || !walkable(i, j)) continue;
            const double d = disSq({i, j}, p);
            if (out.dr >= 0 && d >= best) continue;
            best = d;
            out = {i, j};
        }
    return out.dr >= 0;
}

int War::pickWaypoint(Pos& stand) const
{
    const int wp = MAP_L / SCOUT_VIEW + 1;
    int bestIdx = -1, bestStep = 0;
    stand = {-1, -1};

    for (int i = 0; i < wp; i++)
        for (int j = 0; j < wp; j++)
        {
            const int idx = i * wp + j;
            if (wpDone[idx] || gameFrame < wpCooldown[idx]) continue;

            const Pos cw(min(i * SCOUT_VIEW, MAP_L - 1), min(j * SCOUT_VIEW, MAP_U - 1));
            if (enemyCorner(cw.dr, cw.ur)) continue;

            Pos st;
            if (!nearestStand(cw, 5, st)) continue;  // 这一帧到不了

            if (wpGain(st) < SCOUT_MIN_GAIN) continue;

            const int step = scoutDist(st);
            if (bestIdx >= 0 && step >= bestStep) continue;
            bestStep = step;
            bestIdx = idx;
            stand = st;
        }
    return bestIdx;
}

int War::homeETA(const Pos& here)
{
    if (anchor.dr == -1 || gameFrame - lastAnchorChanged > SCOUT_ANCHOR_GAP)
    {
        double bestFar = -1;
        for (const auto& it : buildingMap)
        {
            if (it.second->Type != BUILDING_ARROWTOWER) continue;
            const Pos tp{it.second->BlockDR, it.second->BlockUR};
            const double d = disSq(tp, base);
            if (d > bestFar)
            {
                bestFar = d;
                anchor = tp;
            }
        }
        if (anchor.dr == -1) anchor = base;
        lastAnchorChanged = gameFrame;
    }

    if (nearestStand(anchor, 4, home)) return scoutDist(home) * 25;
    return max(abs(here.dr - anchor.dr), abs(here.ur - anchor.ur)) * 25;
}

bool War::isExplore(int eta) const
{
    for (int i = 0; i < 3; i++)
    {
        if (gameFrame >= SCOUT_WAVE[i] && gameFrame <= SCOUT_WAVE[i] + SCOUT_HOME_STAY) return false;
        if (gameFrame < SCOUT_WAVE[i]) return gameFrame + eta >= SCOUT_WAVE[i] ? false : true;
    }
    return false;
}

void War::buildRoute(const Pos& goal)
{
    route.clear();
    routeAt = 0;
    routeFlee = false;
    if (goal.dr < 0 || scoutDist(goal) < 0) return;

    for (Pos p = goal;;)
    {
        route.push_back(p);
        const int prev = scoutPrev(p);
        if (prev < 0) break;
        p = cellPos(prev);
    }
    std::reverse(route.begin(), route.end());
}

bool War::routeSafe() const
{
    const int lim = min((int)route.size(), routeAt + SCOUT_VIEW);
    for (int i = routeAt; i < lim; i++)
        if (threatAt(route[i].dr, route[i].ur) > 0 || !walkable(route[i].dr, route[i].ur)) return false;
    return true;
}

bool War::followRoute(const Pos& here, bool idle)
{
    while (routeAt < (int)route.size() && route[routeAt].dr == here.dr && route[routeAt].ur == here.ur) routeAt++;
    if (routeAt >= (int)route.size()) return false;

    const Pos next = route[routeAt];
    const int sd = next.dr - here.dr, su = next.ur - here.ur;  // 方向

    if (sd < -1 || sd > 1 || su < -1 || su > 1) return false;  // 重新规划

    int end = routeAt;
    while (end + 1 < (int)route.size() && route[end + 1].dr - route[end].dr == sd &&
           route[end + 1].ur - route[end].ur == su)
        end++;

    const int endCell = cellIdx(route[end].dr, route[end].ur);
    if (endCell == routeSent)
    {
        if (!idle) return true;
        route.clear();
        return false;
    }

    routeSent = endCell;
    orderMoveCell(scoutSN, route[end]);
    return true;
}

Pos War::fleeGoal() const
{
    Pos best = {-1, -1};
    int bestThreat = 0, bestStep = 0;
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
        {
            const int step = scoutDist(i, j);
            if (step <= 0) continue;

            const int t = threatAt(i, j);
            if (best.dr >= 0 && (t > bestThreat || (t == bestThreat && step >= bestStep))) continue;
            bestThreat = t;
            bestStep = step;
            best = {i, j};
            if (!t && step == 1) return best;
        }
    return best;
}

bool War::evade(const Pos& here, bool idle)
{
    if (threatAt(here.dr, here.ur) <= 0)
    {
        if (routeFlee) route.clear();
        return false;
    }

    if (routeFlee && route.size())
    {
        const Pos& tail = route.back();
        const bool goalOk = walkable(tail.dr, tail.ur) && threatAt(tail.dr, tail.ur) <= 0;
        if (goalOk && followRoute(here, idle)) return true;
        route.clear();
    }

    scoutFlood(here, false);
    const Pos goal = fleeGoal();
    if (goal.dr < 0) return true;

    buildRoute(goal);
    routeFlee = true;
    followRoute(here, idle);
    return true;
}

void War::scoutRun()
{
    if (allInPriest) return;

    const tagArmy* unit = army(scoutSN);
    if (!unit) return;

    const tagArmy& u = *unit;
    const Pos here = {u.BlockDR, u.BlockUR};
    const FloatPos Fhere = {u.DR, u.UR};
    const bool idle = u.NowState == HUMAN_STATE_IDLE;

    const int wp = MAP_L / SCOUT_VIEW + 1;
    if (wpCooldown.empty())
    {
        wpCooldown.assign(wp * wp, 0);
        wpDone.assign(wp * wp, 0);
    }
    if (!scoutDist.ready())
    {
        scoutDist.reset(-1);
        scoutPrev.reset(-1);
    }

    if (evade(here, idle)) return;

    if (gameFrame - lastRecordFrame >= SCOUT_STUCK && dis(Fhere, lastPos) >= BLOCKSIDELENGTH)
    {
        lastPos = Fhere;
        lastRecordFrame = gameFrame;
    }
    else if (route.size() && gameFrame - lastRecordFrame >= SCOUT_STUCK)
    {
        if (goalWp >= 0) wpCooldown[goalWp] = gameFrame + SCOUT_COOLDOWN;
        goalWp = -1;
        goalStand = {-1, -1};
        route.clear();
        lastPos = Fhere;
        lastRecordFrame = gameFrame;
    }

    scoutFlood(here, true);

    if (!isExplore(homeETA(here)))
    {
        if (!arrived && dis(Fhere, FloatPos(home)) < 5 * BLOCKSIDELENGTH) arrived = true;
        if (arrived)
        {
            goalWp = -1;
            goalStand = {-1, -1};
            route.clear();
            return;
        }
        goalWp = -1;
        goalStand = home;
        if (route.empty() || route.back().dr != home.dr || route.back().ur != home.ur) buildRoute(home);
        followRoute(here, idle);
        return;
    }

    arrived = false;
    unknownRebuild();

    if (goalWp >= 0)
    {
        const bool reached = dis(Fhere, FloatPos(goalStand)) <= 2 * BLOCKSIDELENGTH;
        const bool ok = scoutDist(goalStand) >= 0 && wpGain(goalStand) >= SCOUT_MIN_GAIN;
        if (reached) wpDone[goalWp] = 1;
        if (reached || !ok)
        {
            goalWp = -1;
            goalStand = {-1, -1};
            route.clear();
        }
    }

    if (goalWp < 0 && (goalWp = pickWaypoint(goalStand)) < 0) return;

    if (route.size())
    {
        if (!routeSafe()) route.clear();
        else if (followRoute(here, idle)) return;
        else route.clear();
    }

    buildRoute(goalStand);
    followRoute(here, idle);
}

void War::defenceRun()
{
    hostiles.clear();
    towerAtk.clear();

    for (const auto& it : eArmyMap)
    {
        const double d = dis({it.second->DR, it.second->UR}, baseF);
        if (d < 65 * BLOCKSIDELENGTH) towerAtk.push_back(it.first);
        if (d < 40 * BLOCKSIDELENGTH) hostiles.push_back(it.first);
    }

    if (gameFrame < FIX_TOWER_UNTIL) fixTower();

    combat = hostiles.size() > 0;
    if (!combat)
    {
        priestTarget = -1;
        return;
    }

    std::sort(hostiles.begin(), hostiles.end());

    towerRun();
    priestRun();
    armyRun();
}

void War::fixTower()
{
    if (res.stone <= 0)
    {
        for (int sn : fixCrew) freeWorker(sn);
        fixCrew.clear();
        return;
    }

    const tagBuilding* tar = nullptr;
    for (int sn : buildingsOf(BUILDING_ARROWTOWER))
    {
        const tagBuilding* t = building(sn);
        if (t->Percent < 100 || t->Blood >= t->MaxBlood) continue;
        if (!tar || t->SN < tar->SN) tar = t;
    }

    for (auto it = fixCrew.begin(); it != fixCrew.end();)
        if (farmer(*it)) it++;
        else it = fixCrew.erase(it);

    if (!tar)
    {
        for (int sn : fixCrew) freeWorker(sn);
        fixCrew.clear();
        return;
    }

    while ((int)fixCrew.size() < CREW_FIX)
    {
        const int sn = claimWorker(FloatPos(Pos(tar->BlockDR, tar->BlockUR)), true);
        if (sn < 0) break;
        fixCrew.insert(sn);
    }

    for (int sn : fixCrew) workerTask(sn, tar->SN);
}

void War::towerRun()
{
    for (int sn : buildingsOf(BUILDING_ARROWTOWER))
    {
        const tagBuilding* t = building(sn);
        if (t->Percent < 100) continue;

        // 优先点还没锁住诱饵的, 都锁住了就打伤害最高的那个
        int pick = -1;
        double bestDps = 0;
        for (int e : towerAtk)
        {
            if (lockOf(e) >= 0) continue;
            const double d = dpsOf(*enemyArmy(e));
            if (pick < 0 || d > bestDps)
            {
                bestDps = d;
                pick = e;
            }
        }
        if (pick < 0)
            for (int e : hostiles)
            {
                const double d = dpsOf(*enemyArmy(e));
                if (pick < 0 || d > bestDps)
                {
                    bestDps = d;
                    pick = e;
                }
            }
        if (pick < 0) return;

        if (t->Project != pick) order(sn, pick);
    }
}

void War::armyRun()
{
    // 还没锁住诱饵的敌人排前面, 同档按伤害从高到低
    std::sort(hostiles.begin(), hostiles.end(), [&](int a, int b)
    {
        const bool la = lockOf(a) < 0, lb = lockOf(b) < 0;
        if (la != lb) return la;
        const double da = dpsOf(*enemyArmy(a)), db = dpsOf(*enemyArmy(b));
        return da != db ? da > db : a < b;
    });

    for (const auto& it : armyMap)
    {
        const tagArmy& u = *it.second;
        if (u.SN == priest || u.Sort == AT_STONE_THROWER || u.Sort == AT_CHARIOT_ARCHER) continue;

        const tagArmy* cur = enemyArmy(u.WorkObjectSN);
        if (cur && cur->Sort != AT_STONE_THROWER && u.WorkObjectSN != priestTarget) continue;

        for (int e : hostiles)
            if (e != priestTarget && enemyArmy(e)->Sort != AT_STONE_THROWER)
            {
                order(u.SN, e);
                break;
            }
    }
}

void War::priestRun()
{
    if (allInPriest) return;

    const tagArmy* self = army(priest);
    if (!self) return;
    const tagArmy& p = *self;

    auto convertible = [&](int e) { return enemyArmy(e) && lockOf(e) >= 0; };

    const tagArmy* t = enemyArmy(p.WorkObjectSN);
    if (t != nullptr && convertible(t->SN) && t->Sort == AT_STONE_THROWER) return;

    int pick = -1;
    double bestDps = 0;
    for (int e : hostiles)
    {
        if (lockOf(e) < 0) continue;
        double d = dpsOf(*enemyArmy(e));
        if (enemyArmy(e)->Sort == AT_STONE_THROWER) d += 10000;
        if (pick < 0 || d > bestDps)
        {
            bestDps = d;
            pick = e;
        }
    }

    t = enemyArmy(pick);
    const bool isS = t && t->Sort == AT_STONE_THROWER;

    if (isS)
    {
        priestTarget = pick;
        order(priest, pick);
    }
    else if (convertible(p.WorkObjectSN)) { priestTarget = p.WorkObjectSN; }
    else if (pick != -1)
    {
        priestTarget = pick;
        order(priest, pick);
    }
}

void War::offenseRun()
{
    if (!allInArmy) return;

    if (siegeSN == -1)
        for (const auto& it : eBuildingMap)
            if (it.second->Type == BUILDING_SIEGE)
            {
                siegeSN = it.first;
                break;
            }

    if (corner.dr == -1 && base.dr >= 0)
    {
        corner.dr = (base.dr * 2 / MAP_L) ? 0 : MAP_L - 1;
        corner.ur = (base.ur * 2 / MAP_U) ? 0 : MAP_U - 1;
    }
    if (corner.dr < 0) return;

    if (!offenseMark.ready()) offenseMark.reset(0);
    offenseMark.fill(0);

    std::queue<Pos> q;
    const int stride = 5;
    const int sectorsU = (MAP_U + stride - 1) / stride;
    std::vector<bool> sectorTaken(((MAP_L + stride - 1) / stride) * sectorsU, false);
    std::vector<Pos> nextToGo;
    q.push(corner);
    offenseMark(corner) = 1;
    while (q.size())
    {
        const Pos crt = q.front();
        q.pop();
        if (cell(crt.dr, crt.ur).type != MAPPATTERN_UNKNOWN && walkable(crt.dr, crt.ur) && nav(crt.dr, crt.ur) >= 0)
        {
            const int sIdx = (crt.dr / stride) * sectorsU + (crt.ur / stride);
            if (!sectorTaken[sIdx])
            {
                sectorTaken[sIdx] = true;
                nextToGo.push_back(crt);
            }
        }
        else
        {
            if (cell(crt.dr, crt.ur).type != MAPPATTERN_UNKNOWN) continue;
            for (int d = 0; d < 8; d++)
            {
                const Pos next = {crt.dr + dx[d], crt.ur + dy[d]};
                if (!inMap(next.dr, next.ur)) continue;
                if (offenseMark(next)) continue;
                if (cell(next.dr, next.ur).type == MAPPATTERN_OCEAN) continue;

                offenseMark(next) = 1;
                q.push(next);
            }
        }
    }

    nextToGo.erase(remove_if(nextToGo.begin(), nextToGo.end(), [&](const Pos& p)
    { return angleDeg({corner.dr - base.dr, corner.ur - base.ur}, {p.dr - base.dr, p.ur - base.ur}) > 15; }),
                   nextToGo.end());

    std::sort(nextToGo.begin(), nextToGo.end(), [&](const Pos& a, const Pos& b) { return nav(a) > nav(b); });
    const size_t frontierK = std::max<size_t>(6, armyMap.size());
    if (nextToGo.size() > frontierK) nextToGo.resize(frontierK);

    if (watchPoint.dr == -1 && !nextToGo.empty()) watchPoint = nextToGo.back();
    if (allInPriest && !been && watchPoint.dr >= 0)
    {
        been = true;
        orderMoveCell(priest, watchPoint);
    }

    // 可打的目标: 视野内的敌方军队 + 已探到的敌方建筑, 去掉攻城厂
    std::vector<int> targets;
    for (const auto& it : eArmyMap)
        if (dis({it.second->DR, it.second->UR}, FloatPos(corner)) <= 50 * BLOCKSIDELENGTH) targets.push_back(it.first);
    for (const auto& it : eBuildingMap)
        if (it.first != siegeSN) targets.push_back(it.first);

    auto whereIs = [&](int sn)
    {
        const tagArmy* a = enemyArmy(sn);
        if (a) return FloatPos(a->DR, a->UR);
        const tagBuilding* b = enemyBuilding(sn);
        return b ? FloatPos(Pos(b->BlockDR, b->BlockUR)) : FloatPos(-1, -1);
    };

    auto nearestTarget = [&](const FloatPos& from)
    {
        int pick = -1;
        double best = 0;
        for (int t : targets)
        {
            if (t == siegeSN) continue;
            const double d = disSq(whereIs(t), from);
            if (pick < 0 || d < best || (d == best && t < pick))
            {
                best = d;
                pick = t;
            }
        }
        return pick;
    };

    // 每个待派单位选 nextToGo 里离自己最近的未占用格
    std::vector<bool> used(nextToGo.size(), false);
    auto pickNearest = [&](double dr, double ur) -> int
    {
        int pick = -1;
        double best = 0;
        for (size_t i = 0; i < nextToGo.size(); i++)
        {
            if (used[i]) continue;
            const double ddr = nextToGo[i].dr * BLOCKSIDELENGTH + 0.5 * BLOCKSIDELENGTH - dr;
            const double dur = nextToGo[i].ur * BLOCKSIDELENGTH + 0.5 * BLOCKSIDELENGTH - ur;
            const double d = ddr * ddr + dur * dur;
            if (pick < 0 || d < best)
            {
                best = d;
                pick = (int)i;
            }
        }
        return pick;
    };

    for (const auto& it : armyMap)
    {
        const tagArmy& u = *it.second;
        if (u.SN == priest || (u.Sort != AT_CHARIOT_ARCHER && u.Sort != AT_STONE_THROWER)) continue;

        if (enemyArmy(u.WorkObjectSN) || enemyBuilding(u.WorkObjectSN)) continue;

        const int t = nearestTarget({u.DR, u.UR});
        if (t >= 0) order(u.SN, t);
        else if (u.NowState == HUMAN_STATE_IDLE && targets.size() == 0)  // 什么都看不见
        {
            const int idx = pickNearest(u.DR, u.UR);
            if (idx >= 0)
            {
                used[idx] = true;
                orderMoveCell(u.SN, nextToGo[idx]);
            }
        }
    }

    if (allInPriest && siegeSN >= 0 && targets.size() <= 3)
    {
        const tagArmy* p = army(priest);
        if (p && p->WorkObjectSN != siegeSN) order(priest, siegeSN);
    }
}

void War::clearRoad()
{
    if (!offenseMark.ready()) offenseMark.reset(0);
    offenseMark.fill(0);

    std::vector<Pos> points;
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (nav(i, j) >= 22 && nav(i, j) <= 25)
            {
                offenseMark(i, j) = 1;
                points.push_back({i, j});
            }

    if (points.empty()) return;

    for (const auto& a : armyMap)
    {
        const tagArmy* u = a.second;
        if (offenseMark(u->BlockDR, u->BlockUR) || u->NowState != HUMAN_STATE_IDLE) continue;

        const int ran = rand() % points.size();
        orderMoveCell(u->SN, points[ran]);
    }
}

void War::lionRun()
{
    if (gameFrame < LION_HUNT_FROM) return;

    const tagResource* tar = nullptr;
    double best = 0;
    for (const tagResource* l : lionSet)
    {
        const double d = dis({l->DR, l->UR}, baseF);
        const double a =
            angleDeg({corner.dr - base.dr, corner.ur - base.ur}, {l->BlockDR - base.dr, l->BlockUR - base.ur});
        if (d > 40 * BLOCKSIDELENGTH && a > 15) continue;
        if (!tar || d < best || (d == best && l->SN < tar->SN))
        {
            best = d;
            tar = l;
        }
    }

    for (auto it = lionCrew.begin(); it != lionCrew.end();)
        if (farmer(*it)) it++;
        else it = lionCrew.erase(it);

    if (!tar)
    {
        for (int sn : lionCrew) freeWorker(sn);
        lionCrew.clear();
        return;
    }

    while ((int)lionCrew.size() < CREW_LION)
    {
        const int sn = claimWorker({tar->DR, tar->UR}, true);
        if (sn < 0) break;
        lionCrew.insert(sn);
    }

    for (int sn : lionCrew) workerTask(sn, tar->SN);
}

bool Brain::workerBusy(int sn) const
{
    if (workerToFarm.count(sn) || staffHunt.count(sn) || spotOfWorker.count(sn)) return true;
    if (lionCrew.count(sn) || fixCrew.count(sn)) return true;
    for (const BuildSite& s : sites)
        if (s.workers.count(sn)) return true;
    return false;
}

void Brain::workerDrop(int sn)
{
    huntDetach(sn);
    dropSpot(sn, false);
    for (BuildSite& s : sites) s.workers.erase(sn);
    lionCrew.erase(sn);
    fixCrew.erase(sn);
}

int Brain::farmerTargetByPop() const
{
    int armyPop2 = 0;
    const bool logistics = hasTech(BUILDING_ARMYCAMP_RESEARCH_LOGISTICS);
    for (const auto& it : armyMap)
    {
        const int s = it.second->Sort;
        const bool fromCamp = (s == AT_CLUBMAN || s == AT_SLINGER || s == AT_SWORDSMAN || s == AT_BROADSWORDSMAN);
        armyPop2 += (fromCamp && logistics) ? 1 : 2;
    }
    const int armyPop = (armyPop2 + 1) / 2;

    const int MIN_FARMER = 8;
    const int HEADROOM = 6;
    return std::max(MIN_FARMER, std::min(20, maxHuman - armyPop - HEADROOM));
}

void Brain::strategyRun()
{
    int b_prio = 100;
    int e_prio = 100;

    const int population = max(0, (int)farmerMap.size() - (int)farmList.size());

    const int homeCnt = min(12, (int)(farmerMap.size() + armyMap.size()) / 4 + 1);
    wantBuilding(BUILDING_HOME, homeCnt, b_prio--);

    if (useStock) wantStock(b_prio--);

    const int farmerTarget = usePopModel ? farmerTargetByPop() : (stage == CIVILIZATION_TOOLAGE ? 10 : 24);

    if (stage == CIVILIZATION_TOOLAGE)
    {
        wantBuilding(BUILDING_ARMYCAMP, 1, b_prio--);
        wantBuilding(BUILDING_RANGE, 1, b_prio--);
        wantBuilding(BUILDING_MARKET, 1, b_prio--);
        wantBuilding(BUILDING_FARM, 2, b_prio--);

        wantTech(BUILDING_CENTER_UPGRADE, e_prio--);
        wantUnit(AT_FARMER, farmerTarget, e_prio--);
        wantTech(BUILDING_GRANARY_ARROWTOWER, e_prio--);
    }
    else
    {
        wantBuilding(BUILDING_FARM, 4, b_prio--);
        if (hasTech(BUILDING_GRANARY_ARROWTOWER)) wantBuilding(BUILDING_ARROWTOWER, 3, b_prio--);
        wantBuilding(BUILDING_RANGE, 3, b_prio--);

        wantTech(BUILDING_MARKET_WHEEL_UPGRADE, e_prio--);
        wantUnit(AT_FARMER, farmerTarget, e_prio--);
        wantTech(BUILDING_GRANARY_ARROWTOWER, e_prio--);
        if (hasTech(BUILDING_MARKET_WHEEL_UPGRADE) && buildingCount(BUILDING_RANGE) >= 2)
            wantUnit(AT_CHARIOT_ARCHER, unitCount(AT_CHARIOT_ARCHER) + 4, e_prio--);
    }

    rebalance(population, buildDemand() + prodDemand());

    int foodPop = popFood;
    int woodPop = popWood;
    int stonePop = popStone;

    if (useLiveHunting)
    {
        if (gameFrame > 2 * 25 * 60 && (int)hunts.size() && spotsWithin(RK_CORPSE, 1e9) < 5) toHunt(foodPop, 2, 0);
        toPool(foodPop, POP_INF, RK_CORPSE);
    }

    toPool(foodPop, POP_INF, RK_BUSH, 60 * BLOCKSIDELENGTH);
    toPool(stonePop, POP_INF, RK_STONE);
    woodPop += foodPop + stonePop;
    toPool(woodPop, POP_INF, RK_WOOD);
}

void Brain::update(const tagInfo& info)
{
    worldRebuild(info);
    phaseUpdate();
    navRebuild();
    gatherFrame();
    buildFrame();
    farmFrame();
    prodFrame();
    huntFrame();
    laborRebuild();
    gatherReset();
    huntReset();
    scoutFrame();

    threatRebuild();
    defenceRun();
    if (!combat) scoutRun();
    if (!combat && !allInArmy) clearRoad();

    lionRun();

    offenseRun();

    farmRun();

    strategyRun();

    held = Stock();  // 生产预定只在本帧有效

    prodRun();
    buildRun();

    huntRun();
    gatherRun();

    prodDestroy();
}