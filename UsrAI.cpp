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

Mgr mgr;

static long long placeFailKey(int type, int dr, int ur)
{
    return ((long long)(type + 1) << 32) | (unsigned int)cellIdx(dr, ur);
}

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

double unitSpeed(int sort)  // 兵种移动速度(像素/帧), 用于总攻错峰出发
{
    switch (sort)
    {
        case AT_SCOUT: return (double)SPEED_SCOUT;
        case AT_CAVALRY: return (double)SPEED_CAVALRY;
        case AT_CHARIOT: return (double)SPEED_CHARIOT;
        case AT_CHARIOT_ARCHER: return (double)SPEED_CHARIOT_ARCHER;
        case AT_STONE_THROWER: return (double)SPEED_STONE_THROWER;
        case AT_PRIEST: return (double)SPEED_PRIEST;
        case AT_HOPLITE: return (double)SPEED_HOPLITE;
        case AT_BOWMAN: return (double)SPEED_BOWMAN;
        case AT_IMPROVED: return (double)SPEED_IMPROVEDBOWMAN1;
        case AT_COMPOSITE_BOWMAN: return (double)SPEED_COMPOSITE_BOWMAN;
        case AT_SLINGER: return (double)SPEED_SLINGER;
        case AT_BROADSWORDSMAN: return (double)SPEED_BROADSWORDSMAN;
        case AT_SWORDSMAN: return (double)SPEED_SHORTSWORDSMAN1;
        case AT_CLUBMAN: return (double)SPEED_CLUBMAN1;
        case AT_SHIP: return (double)SPEED_SHIP;
        default: return (double)HUMAN_SPEED;
    }
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

bool Mgr::valid(int dr, int ur) const
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
            if (!valid(i, j) || cell(i, j).height != h || blockCell(i, j)) return false;
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

void Mgr::mark(const tagBuilding& b)
{
    const int s = buildingSize(b.Type);
    for (int i = b.BlockDR; i < b.BlockDR + s; i++)
        for (int j = b.BlockUR; j < b.BlockUR + s; j++)
            if (inMap(i, j)) blockCell(i, j) = 1;
}

void Mgr::makeFrame(const tagInfo& info)
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
        mark(b);

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
        mark(b);
    }
}

void Mgr::navBuild()
{
    nav.reset(-1);
    if (!navUsed.ready()) navUsed.reset(0);
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
    if (!navUsed.ready()) navUsed.reset(0);
    navUsed.fill(0);

    std::queue<Pos> q;
    for (int i = around.dr; i < around.dr + size; i++)
        for (int j = around.ur; j < around.ur + size; j++)
        {
            if (!inMap(i, j) || navUsed(i, j)) continue;
            q.push({i, j});
            navUsed(i, j) = 1;
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
                if (!inMap(next.dr, next.ur) || navUsed(next) || blocked(next.dr, next.ur)) continue;
                q.push(next);
                navUsed(next) = 1;
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

void Mgr::threatBuild()
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

void Mgr::sendAction(int workerSN, int targetSN)
{
    auto ph = lureHold.find(workerSN);
    if (ph != lureHold.end() && gameFrame < ph->second) return;

    const tagFarmer* f = farmer(workerSN);
    if (!f) return;
    if (f->WorkObjectSN == targetSN && f->NowState != HUMAN_STATE_IDLE) return;
    HumanAction(workerSN, targetSN);
}

void Mgr::laborBuild()
{
    laborPool.clear();
    claimed.clear();

    for (const auto& it : farmerMap)
        if (!workerBusy(it.first)) laborPool.push_back(it.first);
}

void Mgr::laborRelease()
{
    // 农田只在人数确实要下降时释放；同类农田 rate 排名变化不会触发换岗。
    for (auto it = farmToWorker.begin(); it != farmToWorker.end();)
    {
        const tagBuilding* b = building(it->first);
        if (b && b->Percent >= 100 && farmer(it->second))
        {
            ++it;
            continue;
        }
        auto cur = it++;
        workerJobSince.erase(cur->second);
        unbind(cur);
    }

    int farmExcess = (int)farmToWorker.size() - min(farmDesired, (int)farmList.size());
    for (int i = (int)farmList.size() - 1; i >= 0 && farmExcess > 0; i--)
    {
        auto it = farmToWorker.find(farmList[i]);
        if (it == farmToWorker.end() || jobHeld(it->second, ECON_FARM)) continue;
        const int sn = it->second;
        workerJobSince.erase(sn);
        unbind(it);
        farmExcess--;
    }

    for (HuntSite& s : hunts)
        if ((int)s.staff.size() > s.desired) huntFree((int)s.staff.size() - s.desired, s);

    // 采集点同样只按“当前在岗人数 > committed target”释放最差的已有岗位。
    // 不再因为 spots 的边缘 rate 排名变化，把仍然有效的工人先踢掉再重新分配。
    for (int k = 0; k < RK_COUNT; k++)
    {
        GatherPool& p = pools[k];
        int assigned = 0;
        for (const GatherSpot& s : p.spots)
            if (workerOfSpot.count(s.sn)) assigned++;

        int excess = assigned - min(p.desired, (int)p.spots.size());
        for (int i = (int)p.spots.size() - 1; i >= 0 && excess > 0; i--)
        {
            auto it = workerOfSpot.find(p.spots[i].sn);
            if (it == workerOfSpot.end() || jobHeld(it->second, k)) continue;

            const int sn = it->second;
            workerJobSince.erase(sn);
            dropSpot(sn, true);
            excess--;
        }
    }
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

int Mgr::takeNearest(const FloatPos& at, bool steal)
{
    std::vector<int> cand;
    cand.reserve(laborPool.size());
    for (int sn : laborPool)
        if (!claimed.count(sn)) cand.push_back(sn);

    int best = nearestOf(cand, at);
    if (best >= 0)
    {
        auto it = std::find(laborPool.begin(), laborPool.end(), best);
        if (it != laborPool.end())
        {
            *it = laborPool.back();
            laborPool.pop_back();
        }
        claimed.insert(best);
        return best;
    }
    if (!steal) return -1;

    cand.clear();
    for (const auto& it : farmerMap)
    {
        const int sn = it.first;
        if (claimed.count(sn) || workerPinned(sn)) continue;
        cand.push_back(sn);
    }

    best = nearestOf(cand, at);
    if (best < 0) return -1;

    workerDrop(best);  // 防止 sn 同时属于多个岗位
    claimed.insert(best);
    return best;
}

void Mgr::freeWorker(int sn)
{
    if (farmer(sn)) laborPool.push_back(sn);
}

void Mgr::buildDepots()
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

double Mgr::depotCost(const FloatPos& at, const std::vector<FloatPos>& depots) const
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

bool Mgr::standCell(const tagResource* r, Pos& out) const
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

void Mgr::arrangeGather()
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
        workerJobSince.erase(it->first);
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
        s.rate = gatherRate(x.k, x.cost);
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
        workerJobSince.erase(it->first);
        it = spotOfWorker.erase(it);
    }
}

void Mgr::gatherReset()
{
    for (int k = 0; k < RK_COUNT; k++) pools[k].desired = 0;
}

void Mgr::dropSpot(int workerSN, bool toFree)
{
    auto it = spotOfWorker.find(workerSN);
    if (it == spotOfWorker.end()) return;
    workerOfSpot.erase(it->second);
    spotOfWorker.erase(it);
    if (toFree)
    {
        workerJobSince.erase(workerSN);
        freeWorker(workerSN);
    }
}


double Mgr::marginalGatherRate(ResKind k, int assigned) const
{
    if (k < 0 || k >= RK_COUNT || assigned < 0) return 0.0;
    const GatherPool& p = pools[k];
    if (assigned >= (int)p.spots.size()) return 0.0;
    return p.spots[assigned].rate;
}

void Mgr::runGather()
{
    for (int k = 0; k < RK_COUNT; k++)
    {
        GatherPool& p = pools[k];
        const int target = min(p.desired, (int)p.spots.size());

        int assigned = 0;
        for (const GatherSpot& s : p.spots)
        {
            auto it = workerOfSpot.find(s.sn);
            if (it == workerOfSpot.end()) continue;
            assigned++;
            sendAction(it->second, s.sn);
        }

        for (const GatherSpot& s : p.spots)
        {
            if (assigned >= target) break;
            if (workerOfSpot.count(s.sn)) continue;

            const int sn = takeNearest(FloatPos(s.stand));
            if (sn < 0) break;

            workerOfSpot[s.sn] = sn;
            spotOfWorker[sn] = s.sn;
            workerJobSince[sn] = gameFrame;
            assigned++;
            sendAction(sn, s.sn);
        }
    }
}

bool Mgr::huntable(const tagResource* r) const
{ return r->Type == RESOURCE_GAZELLE && r->Blood > 0 && !nearLion(r->BlockDR, r->BlockUR, LION_KEEP); }

HuntSite* Mgr::huntOf(int siteID)
{
    auto it = huntByID.find(siteID);
    return it == huntByID.end() ? nullptr : it->second;
}

void Mgr::formHunts(const std::vector<const tagResource*>& sor, int threshold)
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

void Mgr::restoreStaff()
{
    for (auto it = staffHunt.begin(); it != staffHunt.end();)
    {
        const int sn = it->first;
        HuntSite* s = farmer(sn) ? huntOf(it->second) : nullptr;
        if (!s)
        {
            it = staffHunt.erase(it);
            continue;
        }
        s->staff.push_back(sn);
        it++;
    }
}

void Mgr::huntFrame()
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

void Mgr::huntReset()
{
    for (HuntSite& s : hunts) s.desired = 0;
}

void Mgr::huntDetach(int sn)
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

void Mgr::huntAssign(int x, HuntSite& site)
{
    while (x-- > 0)
    {
        const int sn = takeNearest(site.center);
        if (sn < 0) return;
        site.staff.push_back(sn);
        staffHunt[sn] = site.id;
    }
}

void Mgr::huntFree(int x, HuntSite& site)
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

void Mgr::arrangeHunt()
{
    for (HuntSite& s : hunts)
    {
        if ((int)s.staff.size() < s.desired) huntAssign(s.desired - (int)s.staff.size(), s);
        if (s.staff.size()) runHunt(s);
    }
}

void Mgr::runHunt(HuntSite& site)
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

    for (int sn : site.staff) sendAction(sn, prey->SN);
}

void Mgr::farmFrame()
{
    farmList.clear();
    farmRates.clear();

    std::vector<std::pair<double, int>> v;
    for (int sn : buildingsOf(BUILDING_FARM))
    {
        const tagBuilding* b = building(sn);
        if (!b || b->Percent < 100) continue;

        const double half = buildingSize(BUILDING_FARM) * 0.5;
        const FloatPos at((b->BlockDR + half) * BLOCKSIDELENGTH, (b->BlockUR + half) * BLOCKSIDELENGTH);
        v.push_back({transportRate(BASE_RATE_FARM, depotCost(at, foodDepots)), sn});
    }

    std::sort(v.begin(), v.end(), [](const std::pair<double, int>& a, const std::pair<double, int>& b)
    { return a.first != b.first ? a.first > b.first : a.second < b.second; });

    for (const auto& p : v)
    {
        farmList.push_back(p.second);
        farmRates.push_back(p.first);
    }
}

void Mgr::unbind(std::unordered_map<int, int>::iterator it)
{
    workerJobSince.erase(it->second);
    workerToFarm.erase(it->second);
    freeWorker(it->second);
    farmToWorker.erase(it);
}

void Mgr::runFarm()
{
    const int target = min(farmDesired, (int)farmList.size());

    int assigned = 0;
    for (const auto& it : farmToWorker)
    {
        if (!building(it.first) || !farmer(it.second)) continue;
        assigned++;
        sendAction(it.second, it.first);
    }

    for (int farmSN : farmList)
    {
        if (assigned >= target) break;
        if (farmToWorker.count(farmSN)) continue;

        const tagBuilding* farm = building(farmSN);
        if (!farm) continue;

        const int sn = takeNearest(FloatPos(Pos(farm->BlockDR, farm->BlockUR)));
        if (sn < 0) break;

        workerToFarm[sn] = farmSN;
        farmToWorker[farmSN] = sn;
        workerJobSince[sn] = gameFrame;
        assigned++;
        sendAction(sn, farmSN);
    }
}

bool Mgr::jobHeld(int sn, int group) const
{
    if (group < 0 || group >= ECON_GROUP_COUNT || econIdeal[group] == 0) return false;
    auto it = workerJobSince.find(sn);
    return it != workerJobSince.end() && gameFrame - it->second < ECON_JOB_HOLD;
}

int Mgr::econPhase() const
{
    if (stage == CIVILIZATION_TOOLAGE) return 0;

    const bool stage2Done =
        hasTech(BUILDING_MARKET_WHEEL_UPGRADE) &&
        hasTech(BUILDING_GRANARY_ARROWTOWER) &&
        buildingCount(BUILDING_ARROWTOWER, true) >= 3;
    if (!stage2Done) return 1;

    const bool stage3Done =
        buildingCount(BUILDING_COLLAGE, true) > 0 &&
        hasTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW);
    return stage3Done ? 3 : 2;
}

void Mgr::econCommit()
{
    int ideal[ECON_GROUP_COUNT] = {};
    for (int k = 0; k < RK_COUNT; k++) ideal[k] = pools[k].desired;
    ideal[ECON_FARM] = farmDesired;
    for (const HuntSite& s : hunts) ideal[ECON_HUNT] += s.desired;
    for (int g = 0; g < ECON_GROUP_COUNT; g++) econIdeal[g] = ideal[g];

    auto apply = [&]()
    {
        for (int k = 0; k < RK_COUNT; k++) pools[k].desired = econCommitted[k];
        farmDesired = econCommitted[ECON_FARM];

        for (HuntSite& s : hunts) s.desired = 0;
        if (!hunts.empty()) hunts[0].desired = econCommitted[ECON_HUNT];
    };

    if (!econInitialized)
    {
        for (int g = 0; g < ECON_GROUP_COUNT; g++) econCommitted[g] = ideal[g];
        econInitialized = true;
        econLastFrame = gameFrame;
        apply();
        return;
    }

    // 资源/农田消失是硬约束，立即夹紧；这不是经济优化换岗。
    for (int k = 0; k < RK_COUNT; k++)
        econCommitted[k] = min(econCommitted[k], (int)pools[k].spots.size());
    econCommitted[ECON_FARM] = min(econCommitted[ECON_FARM], (int)farmList.size());
    if (hunts.empty()) econCommitted[ECON_HUNT] = 0;

    // 新造出的村民不需要等 15 秒：只填补已有目标的空缺，不从在岗人员中抢人。
    int committedTotal = 0;
    int idealTotal = 0;
    for (int g = 0; g < ECON_GROUP_COUNT; g++)
    {
        committedTotal += econCommitted[g];
        idealTotal += ideal[g];
    }
    const int usable = min((int)farmerMap.size(), idealTotal);
    while (committedTotal < usable)
    {
        int add = -1, best = 0;
        for (int g = 0; g < ECON_GROUP_COUNT; g++)
        {
            const int d = ideal[g] - econCommitted[g];
            if (d > best) best = d, add = g;
        }
        if (add < 0) break;
        econCommitted[add]++;
        committedTotal++;
    }

    if (gameFrame - econLastFrame < ECON_REPLAN)
    {
        apply();
        return;
    }
    econLastFrame = gameFrame;

    // 每 15 秒最多迁移一个已有岗位。
    int excess = -1, deficit = -1;
    int excessAmt = 0, deficitAmt = 0;
    for (int g = 0; g < ECON_GROUP_COUNT; g++)
    {
        const int ex = econCommitted[g] - ideal[g];
        const int de = ideal[g] - econCommitted[g];
        if (ex > excessAmt) excessAmt = ex, excess = g;
        if (de > deficitAmt) deficitAmt = de, deficit = g;
    }

    if (excess >= 0) econCommitted[excess]--;
    if (deficit >= 0) econCommitted[deficit]++;

    // 人口突然死亡时不保留不可能实现的岗位总量。
    int total = 0;
    for (int g = 0; g < ECON_GROUP_COUNT; g++) total += econCommitted[g];
    while (total > (int)farmerMap.size())
    {
        int pick = -1, best = -1;
        for (int g = 0; g < ECON_GROUP_COUNT; g++)
        {
            const int ex = econCommitted[g] - ideal[g];
            if (econCommitted[g] > 0 && ex > best) best = ex, pick = g;
        }
        if (pick < 0) break;
        econCommitted[pick]--;
        total--;
    }

    apply();
}

void Mgr::econPlan()
{
    for (int k = 0; k < RK_COUNT; k++) pools[k].desired = 0;
    for (HuntSite& s : hunts) s.desired = 0;
    farmDesired = 0;
    wantFarm = 0;

    const int pop = (int)farmerMap.size();
    if (pop <= 0)
    {
        econCommit();
        return;
    }

    const EconProfile& profile = ECON_PROFILE[econPhase()];
    const int have[4] = {res.wood, res.meat, res.stone, res.gold};

    // 库存带本身提供滞回：低于 low 才开始补，达到 high 才停止补。
    for (int r = 0; r < 4; r++)
    {
        if (have[r] < profile.low[r]) econBoost[r] = true;
        else if (have[r] >= profile.high[r]) econBoost[r] = false;
    }

    const int stableFoodCap =
        (int)pools[RK_BUSH].spots.size() +
        (int)pools[RK_CORPSE].spots.size() +
        (int)farmList.size();
    const bool useHunt =
        econBoost[1] && !hunts.empty() &&
        gameFrame > 2 * 25 * 60 &&
        (int)pools[RK_CORPSE].spots.size() < 3;
    const int currentFoodCap = stableFoodCap + (useHunt ? CREW_HUNT : 0);

    int currentCap[4] = {
        (int)pools[RK_WOOD].spots.size(),
        currentFoodCap,
        (int)pools[RK_STONE].spots.size(),
        (int)pools[RK_GOLD].spots.size()
    };
    int planCap[4] = {
        currentCap[0],
        buildAvailable(BUILDING_FARM) ? pop : currentCap[1],
        currentCap[2],
        currentCap[3]
    };

    auto addByWeight = [&](int count, int target[4], const int cap[4])
    {
        for (int n = 0; n < count; n++)
        {
            int pick = -1;
            double best = -1;
            for (int r = 0; r < 4; r++)
            {
                if (target[r] >= cap[r] || profile.weight[r] <= 0) continue;
                const double score = (double)profile.weight[r] / (target[r] + 1);
                if (score > best) best = score, pick = r;
            }
            if (pick < 0) break;
            target[pick]++;
        }
    };

    // 约 80% 人口保持阶段预设；最多 4 人承担库存带的动态修正。
    const int flex = min(ECON_FLEX_MAX, max(1, pop / 5));
    const int core = max(0, pop - flex);
    int raw[4] = {};
    addByWeight(core, raw, planCap);

    int boostAdded[4] = {};
    for (int n = 0; n < flex; n++)
    {
        int pick = -1;
        double best = -1;
        for (int r = 0; r < 4; r++)
        {
            if (!econBoost[r] || raw[r] >= planCap[r] || boostAdded[r] >= ECON_FLEX_PER_RES) continue;
            const int width = max(1, profile.high[r] - profile.low[r]);
            const double severity = max(0, profile.high[r] - have[r]) / (double)width;
            const double score = severity / (boostAdded[r] + 1);
            if (score > best) best = score, pick = r;
        }

        if (pick >= 0)
        {
            raw[pick]++;
            boostAdded[pick]++;
        }
        else
        {
            const int before = raw[0] + raw[1] + raw[2] + raw[3];
            addByWeight(1, raw, planCap);
            if (raw[0] + raw[1] + raw[2] + raw[3] == before) break;
        }
    }

    // 稳定食物岗位不够时只补一块农田；当前多出来的人先去其它岗位。
    const int pendingFarm = buildingCount(BUILDING_FARM) - buildingCount(BUILDING_FARM, true);
    if (buildAvailable(BUILDING_FARM) && raw[1] > stableFoodCap + pendingFarm)
        wantFarm = 1;

    int target[4] = {
        min(raw[0], currentCap[0]),
        min(raw[1], currentCap[1]),
        min(raw[2], currentCap[2]),
        min(raw[3], currentCap[3])
    };
    int assignedMacro = target[0] + target[1] + target[2] + target[3];

    // 当前某类资源没位置时，把剩余人口放到其它可用岗位，避免站着不干活。
    while (assignedMacro < pop)
    {
        int pick = -1;
        double best = -1;
        for (int r = 0; r < 4; r++)
        {
            if (target[r] >= currentCap[r]) continue;
            double score = profile.weight[r] > 0 ? (double)profile.weight[r] / (target[r] + 1) : 0.01;
            if (econBoost[r]) score += 10.0;
            if (score > best) best = score, pick = r;
        }
        if (pick < 0) break;
        target[pick]++;
        assignedMacro++;
    }

    pools[RK_WOOD].desired = target[0];
    pools[RK_STONE].desired = target[2];
    pools[RK_GOLD].desired = target[3];

    // 食物内部先保留正在工作的来源，只在总食物人数需要增减时，
    // 才用运输后的实际效率决定新增/释放来自尸体、浆果还是农田。
    int huntPop = 0;
    if (target[1] > 0 && useHunt) huntPop = min(CREW_HUNT, target[1]);
    if (!hunts.empty()) hunts[0].desired = huntPop;

    const int foodTarget = max(0, target[1] - huntPop);

    auto assignedPool = [&](ResKind k)
    {
        int n = 0;
        for (const GatherSpot& s : pools[k].spots)
            if (workerOfSpot.count(s.sn)) n++;
        return n;
    };

    pools[RK_BUSH].desired = min(assignedPool(RK_BUSH), (int)pools[RK_BUSH].spots.size());
    pools[RK_CORPSE].desired = min(assignedPool(RK_CORPSE), (int)pools[RK_CORPSE].spots.size());
    farmDesired = min((int)farmToWorker.size(), (int)farmList.size());

    auto foodNow = [&]()
    { return pools[RK_BUSH].desired + pools[RK_CORPSE].desired + farmDesired; };

    while (foodNow() > foodTarget)
    {
        int src = -1;
        double worst = 1e100;

        if (pools[RK_BUSH].desired > 0)
        {
            const double e = marginalGatherRate(RK_BUSH, pools[RK_BUSH].desired - 1);
            if (e < worst) worst = e, src = 0;
        }
        if (pools[RK_CORPSE].desired > 0)
        {
            const double e = marginalGatherRate(RK_CORPSE, pools[RK_CORPSE].desired - 1);
            if (e < worst) worst = e, src = 1;
        }
        if (farmDesired > 0)
        {
            const double e = farmRates[farmDesired - 1];
            if (e < worst) worst = e, src = 2;
        }

        if (src == 0) pools[RK_BUSH].desired--;
        else if (src == 1) pools[RK_CORPSE].desired--;
        else if (src == 2) farmDesired--;
        else break;
    }

    while (foodNow() < foodTarget)
    {
        int src = -1;
        double best = -1;

        const double bush = marginalGatherRate(RK_BUSH, pools[RK_BUSH].desired);
        if (bush > best) best = bush, src = 0;

        const double corpse = marginalGatherRate(RK_CORPSE, pools[RK_CORPSE].desired);
        if (corpse > best) best = corpse, src = 1;

        if (farmDesired < (int)farmRates.size() && farmRates[farmDesired] > best)
            best = farmRates[farmDesired], src = 2;

        if (src == 0) pools[RK_BUSH].desired++;
        else if (src == 1) pools[RK_CORPSE].desired++;
        else if (src == 2) farmDesired++;
        else break;
    }

    econCommit();
}

bool Mgr::buildAvailable(int type) const
{
    switch (type)
    {
        case BUILDING_RANGE:
        case BUILDING_STABLE:
            return buildingCount(BUILDING_ARMYCAMP, true) > 0;

        case BUILDING_COLLAGE:
            return buildingCount(BUILDING_STABLE, true) > 0;

        case BUILDING_FARM:
            return buildingCount(BUILDING_MARKET, true) > 0;

        case BUILDING_ARROWTOWER:
            return hasTech(BUILDING_GRANARY_ARROWTOWER);

        case BUILDING_SIEGE:
            return false;  // 本局规则禁止建造；投石车只来自祭司转化

        default:
            return true;
    }
}

void Mgr::buildFrame()
{
    builds.clear();
    if (!costMap.ready()) costMap.reset(0);

    // 智能建造
    depotWant(RK_BUSH, granaryPendings);
    depotWant(RK_CORPSE, stockPendings);

    // 历史农田只有仍在实际耕作时才保留仓储需求；废弃农田不会反复催生谷仓。
    for (int sn : buildingsOf(BUILDING_FARM))
    {
        const tagBuilding* b = building(sn);
        if (!b) continue;

        const bool planned = b->Percent < 100;
        const bool active = farmToWorker.count(sn) > 0;
        if (!planned && !active) continue;

        const double half = buildingSize(BUILDING_FARM) * 0.5;
        const FloatPos at((b->BlockDR + half) * BLOCKSIDELENGTH, (b->BlockUR + half) * BLOCKSIDELENGTH);
        if (depotCost(at, foodDepots) <= DEPOT_FAR * BLOCKSIDELENGTH) continue;

        const Pos c(b->BlockDR, b->BlockUR);
        if (!depotCovered(BUILDING_GRANARY, c) && depotRoom(c)) granaryPendings.push_back(c);
    }
}

bool Mgr::depotCovered(int depotType, const Pos& c) const
{
    const FloatPos at(c);
    for (const auto& it : buildingMap)
    {
        const tagBuilding& b = *it.second;
        if (b.Type != BUILDING_CENTER && b.Type != depotType) continue;

        const double half = buildingSize(b.Type) * 0.5;
        const FloatPos d((b.BlockDR + half) * BLOCKSIDELENGTH, (b.BlockUR + half) * BLOCKSIDELENGTH);
        if (dis(at, d) <= DEPOT_FAR * BLOCKSIDELENGTH) return true;
    }
    return false;
}

double Mgr::depotBenefit(int depotType, const Pos& site) const
{
    const std::vector<Pos>& pending = depotType == BUILDING_GRANARY ? granaryPendings : stockPendings;
    const std::vector<FloatPos>& depots = depotType == BUILDING_GRANARY ? foodDepots : resDepots;
    if (pending.empty()) return 0.0;

    const double half = buildingSize(depotType) * 0.5;
    const FloatPos candidate((site.dr + half) * BLOCKSIDELENGTH, (site.ur + half) * BLOCKSIDELENGTH);

    double saved = 0.0;
    for (const Pos& p : pending)
    {
        const FloatPos at(p);
        saved += max(0.0, depotCost(at, depots) - dis(at, candidate));
    }
    return saved / BLOCKSIDELENGTH;
}

bool Mgr::depotRoom(const Pos& c) const
{
    const int size = buildingSize(BUILDING_GRANARY);  // 谷仓和仓库都是 3x3
    int dist = std::max(DEPOT_FAR - 3, 0);
    for (int a = c.dr - dist; a <= c.dr + dist; a++)
        for (int b = c.ur - dist; b <= c.ur + dist; b++)
            if (canPlace(a, b, size) && nav(a, b) >= 0) return true;
    return false;
}

// 仓储只看“现在真的有人在远端采”，不再看 planner 的 desired。
// 同类远端实际工人至少达到 DEPOT_MIN_WORKERS 才发一个请求；每类一次只给一个锚点，
// 避免短期人口峰值把整张图变成仓储建设目标。
void Mgr::depotWant(ResKind k, std::vector<Pos>& out) const
{
    out.clear();
    const double far_ = DEPOT_FAR * BLOCKSIDELENGTH;
    const int depotType = k == RK_BUSH ? BUILDING_GRANARY : BUILDING_STOCK;

    const GatherSpot* anchor = nullptr;
    int workers = 0;
    for (const GatherSpot& s : pools[k].spots)
    {
        if (!workerOfSpot.count(s.sn)) continue;
        if (s.cost <= far_) continue;
        if (depotCovered(depotType, s.stand) || !depotRoom(s.stand)) continue;

        workers++;
        if (!anchor || s.cost > anchor->cost) anchor = &s;
    }

    if (workers >= DEPOT_MIN_WORKERS && anchor) out.push_back(anchor->stand);
}

void Mgr::buildPlaceMask(int type, int size)
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

Pos Mgr::findSpot(int type)
{
    costMap.reset(0);
    const int size = buildingSize(type);
    const int baseLen = buildingSize(BUILDING_CENTER);

    buildPlaceMask(type, size);

    // 通用距离惩罚
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (nav(i, j) == -1) costMap(i, j) += MAP_L + MAP_U;
            else costMap(i, j) += nav(i, j);

    // 靠近基地额外惩罚
    ringAdd(costMap, base, baseLen, 0, 0, 2, PLACE_BASE);

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

        for (int a = dr - 2; a <= dr + len + 1; a++)
            for (int b = ur - 2; b <= ur + len + 1; b++)
            {
                if (!inMap(a, b)) continue;
                if (a >= dr && a < dr + len && b >= ur && b < ur + len) continue;
                costMap(a, b) += PLACE_ADJACENT;
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
                            PLACE_BONUS, 2, 5);

                if (it.second->Type == BUILDING_CENTER)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_CENTER),
                            PLACE_BONUS, 2, 5);
            }
            break;


        case BUILDING_ARMYCAMP:
        case BUILDING_COLLAGE:
        case BUILDING_RANGE:
            ringAdd(costMap, base, baseLen, PLACE_ADJACENT, 0, 6);

            for (const auto& it : buildingMap)
                if (it.second->Type == BUILDING_GRANARY)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_GRANARY),
                            PLACE_ADJACENT, 0, 5);

            for (const auto& it : farmerMap)
            {
                const tagFarmer& f = *(it.second);
                if (workerPinned(f.SN)) continue;

                ringAdd(costMap, {f.BlockDR, f.BlockUR}, 1, PLACE_BONUS, 0, 4);
            }
            break;

        case BUILDING_HOME:  // 紧靠其他房屋建造(形成大矩形占地), 和基地保持5格的距离
            for (const auto& it : buildingMap)
                if (it.second->Type == BUILDING_HOME)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_HOME),
                            -PLACE_ADJACENT * 3, 1, 1);
            ringAdd(costMap, base, baseLen, PLACE_ADJACENT, 0, 5);
            break;


        case BUILDING_ARROWTOWER:
            for (const auto& it : buildingMap)
                if (it.second->Type == BUILDING_ARROWTOWER)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_ARROWTOWER),
                            PLACE_ADJACENT * 3, 0, 9);
            break;

        default:
            break;  // 谷仓/仓库走通用布局 + 下方智能运输收益，不在 switch 中另设固定环
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

            if (type == BUILDING_GRANARY || type == BUILDING_STOCK)
            {
                const double gain = depotBenefit(type, {i, j});
                const bool hasDemand = type == BUILDING_GRANARY ? !granaryPendings.empty() : !stockPendings.empty();
                if (hasDemand && gain <= EPS) continue;
                v -= (long long)(gain * DEPOT_BENEFIT);
            }

            auto fit = failedSpots.find(placeFailKey(type, i, j));
            if (fit != failedSpots.end()) v += (long long)PLACE_FAILED * min(fit->second, PLACE_FAIL_CAP);

            if (best.dr < 0 || v < bestCost)
            {
                bestCost = v;
                best = {i, j};
            }
        }
    return best;
}

void Mgr::wantStock(int priority)
{
    if (stockPendings.empty()) return;

    for (int sn : buildingsOf(BUILDING_STOCK))
        if (building(sn)->Percent < 100) return;

    wantBuilding(BUILDING_STOCK, buildingCount(BUILDING_STOCK) + 1, priority);
}

void Mgr::wantGranary(int priority)
{
    const int have = buildingCount(BUILDING_GRANARY);
    if (have > 0 && granaryPendings.empty()) return;

    for (int sn : buildingsOf(BUILDING_GRANARY))
        if (building(sn)->Percent < 100) return;

    wantBuilding(BUILDING_GRANARY, have + 1, priority);
}

int Mgr::queuedBuild(int type) const
{
    int cnt = 0;
    for (const BuildSite& s : sites)
        if (s.type == type && s.sn < 0) cnt++;
    for (const auto& b : builds)
        if (b.second == type) cnt++;
    return cnt;
}

void Mgr::wantBuilding(int buildingType, int total, int priority)
{
    if (!buildAvailable(buildingType)) return;
    int diff = total - buildingCount(buildingType) - queuedBuild(buildingType);
    for (; diff > 0; diff--) builds.insert({priority, buildingType});
}

void Mgr::runBuild()
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
                if (!s.workers.empty())
                {
                    int& fail = failedSpots[placeFailKey(s.type, s.site.dr, s.site.ur)];
                    fail = min(fail + 1, PLACE_FAIL_CAP);
                }
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

    // 收养没人管的半成品: 工地在地基出现之前被判定失败删掉过, 或者建造工中途被抢走, 都会
    // 留下一个永远停在 Percent < 100 的建筑, 而它会让 wantStock / wantGranary 一直以为
    // "上一座还没盖完"从而再也不批新的
    for (const auto& it : buildingMap)
    {
        const tagBuilding& b = *it.second;
        if (b.Percent >= 100) continue;

        bool owned = false;
        for (const BuildSite& s : sites)
            if (s.sn == b.SN) owned = true;
        if (owned) continue;

        BuildSite s;
        s.type = b.Type;
        s.site = {b.BlockDR, b.BlockUR};
        s.sn = b.SN;
        sites.push_back(s);
    }

    for (BuildSite& s : sites)
    {
        while ((int)s.workers.size() < CREW_BUILD)
        {
            const int sn = takeNearest(FloatPos(s.site), true);
            if (sn < 0) break;
            s.workers.insert(sn);
        }
        for (int sn : s.workers) sendAction(sn, s.sn);
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

        const Stock left = available();
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

        const int first = takeNearest(FloatPos(spot), true);
        if (first < 0) break;

        BuildSite s;
        s.type = type;
        s.site = spot;
        s.workers.insert(first);
        sites.push_back(s);
        justPlaced.push_back(spot);
        used = probe;
        it = builds.erase(it);

        HumanBuild(first, type, spot.dr, spot.ur);
    }
}

void Mgr::prodFrame()
{
    // 命令只会在 techAvailable 通过后发出；这里不做失败检测或重试，
    // 只把“宿主 Project 已结束”解释为该科技完成。
    for (auto it = runningTech.begin(); it != runningTech.end();)
    {
        const int action = *it;
        const int hostType = actionHost(action);

        bool active = false;
        for (int sn : buildingsOf(hostType))
        {
            const tagBuilding* b = building(sn);
            if (b && b->Project == action)
            {
                active = true;
                break;
            }
        }

        if (active) ++it;
        else
        {
            doneTech.insert(action);
            it = runningTech.erase(it);
        }
    }

    prods.clear();
    techOrders.clear();
    destroyCnt = -1;
}

bool Mgr::techAvailable(int action) const
{
    const int host = actionHost(action);
    if (host < 0 || buildingCount(host, true) <= 0) return false;

    switch (action)
    {
        case BUILDING_CENTER_UPGRADE:
            return stage == CIVILIZATION_TOOLAGE &&
                   buildingCount(BUILDING_MARKET, true) > 0 &&
                   buildingCount(BUILDING_ARMYCAMP, true) > 0 &&
                   buildingCount(BUILDING_RANGE, true) > 0;

        case BUILDING_MARKET_WHEEL_UPGRADE:
            return stage == CIVILIZATION_BRONZEAGE;

        case BUILDING_RANGE_UPGRADE_COMPOSITE_BOW:
            return stage == CIVILIZATION_BRONZEAGE;

        default:
            return true;
    }
}

int Mgr::idleHost(int buildingType, const std::set<int>& busy) const
{
    for (int sn : buildingsOf(buildingType))
    {
        const tagBuilding* b = building(sn);
        if (b->Percent >= 100 && b->Project == 0 && !busy.count(sn)) return sn;
    }
    return -1;
}

int Mgr::queuedProd(int action) const
{
    const int host = actionHost(action);
    int cnt = 0;
    for (const auto& p : prods)
        if (p.second == action) cnt++;
    for (int sn : buildingsOf(host))
        if (building(sn)->Project == action) cnt++;
    return cnt;
}

void Mgr::wantUnit(int type, int total, int priority)
{
    const int action = typeToAction(type);
    const int host = actionHost(action);
    if (action < 0 || host < 0 || buildingCount(host, true) <= 0) return;

    int diff = total - unitCount(type) - queuedProd(action);
    for (; diff > 0; diff--) prods.insert({priority, action});
    if (diff < 0) destroyCnt = -diff;
}

void Mgr::wantTech(int action, int priority)
{
    if (!techAvailable(action) || doneTech.count(action) || runningTech.count(action)) return;
    prods.insert({priority, action});
    techOrders.insert(action);
}

void Mgr::runProd()
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

        BuildingAction(host, action);
        if (techOrders.count(action)) runningTech.insert(action);
        busy.insert(host);
        held += cost;
        it = prods.erase(it);
    }
}

void Mgr::runDestroy()
{
    if (destroyCnt <= 0) return;

    int left = destroyCnt;
    for (const auto& it : farmerMap)
    {
        if (left-- <= 0) break;
        HumanAction(it.first, it.first);
    }
}

// 出征的侦察兵不再兼职探图; 总攻后也不再把祭司顶上去
void Mgr::scoutFrame()
{
    if (army(scoutSN) && !onMove.count(scoutSN) && !(assaultOn && scoutSN == priest)) return;

    scoutSN = -1;
    for (int sn : armySNs)
    {
        const tagArmy* a = army(sn);
        if (a && a->Sort == AT_SCOUT && !onMove.count(sn))
        {
            scoutSN = sn;
            break;
        }
    }
    if (scoutSN < 0 && !assaultOn) scoutSN = priest;
}

void Mgr::floodThreat(const Pos& from, bool avoidThreat)
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

void Mgr::buildUnknown()
{
    // unknownRow[i * (MAP_U + 1) + j] = 第 i 行前 j 格里的未知格数
    const int W = MAP_U + 1;
    unknownRow.assign((size_t)MAP_L * W, 0);
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            unknownRow[i * W + j + 1] = unknownRow[i * W + j] + (cell(i, j).type == MAPPATTERN_UNKNOWN);
}

int Mgr::wpGain(const Pos& p) const
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

bool Mgr::nearestStand(const Pos& p, int r, Pos& out) const
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

int Mgr::pickWaypoint(Pos& stand) const
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

int Mgr::homeETA(const Pos& here)
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

bool Mgr::isExplore(int eta) const
{
    for (int i = 0; i < 3; i++)
    {
        if (gameFrame >= SCOUT_WAVE[i] && gameFrame <= SCOUT_WAVE[i] + SCOUT_HOME_STAY) return false;
        if (gameFrame < SCOUT_WAVE[i]) return gameFrame + eta >= SCOUT_WAVE[i] ? false : true;
    }
    return false;
}

void Mgr::buildRoute(const Pos& goal)
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

bool Mgr::routeSafe() const
{
    const int lim = min((int)route.size(), routeAt + SCOUT_VIEW);
    for (int i = routeAt; i < lim; i++)
        if (threatAt(route[i].dr, route[i].ur) > 0 || !walkable(route[i].dr, route[i].ur)) return false;
    return true;
}

bool Mgr::followRoute(const Pos& here, bool idle)
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
    moveToCell(scoutSN, route[end]);
    return true;
}

Pos Mgr::fleeGoal() const
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

bool Mgr::evade(const Pos& here, bool idle)
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

    floodThreat(here, false);
    const Pos goal = fleeGoal();
    if (goal.dr < 0) return true;

    buildRoute(goal);
    routeFlee = true;
    followRoute(here, idle);
    return true;
}

void Mgr::runScout()
{
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

    floodThreat(here, true);

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
    buildUnknown();

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

void Mgr::defence()
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

    runTower();
    runPriest();
    runArmy();
}

void Mgr::fixTower()
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
        const int sn = takeNearest(FloatPos(Pos(tar->BlockDR, tar->BlockUR)), true);
        if (sn < 0) break;
        fixCrew.insert(sn);
    }

    for (int sn : fixCrew) sendAction(sn, tar->SN);
}

void Mgr::runTower()
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

        if (t->Project != pick) HumanAction(sn, pick);
    }
}

void Mgr::runArmy()
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
        if (u.SN == priest || u.Sort == AT_STONE_THROWER || u.Sort == AT_CHARIOT_ARCHER || u.Sort == AT_SCOUT) continue;
        if (onMove.count(u.SN)) continue;  // 出征的人不归防守调度

        const tagArmy* cur = enemyArmy(u.WorkObjectSN);
        if (cur && cur->Sort != AT_STONE_THROWER && u.WorkObjectSN != priestTarget) continue;

        for (int e : hostiles)
            if (e != priestTarget && enemyArmy(e)->Sort != AT_STONE_THROWER)
            {
                HumanAction(u.SN, e);
                break;
            }
    }
}

void Mgr::runPriest()
{
    if (assaultOn) return;  // 总攻之后祭司只服务胜利条件, 不参与防守转化

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
        HumanAction(priest, pick);
    }
    else if (convertible(p.WorkObjectSN)) { priestTarget = p.WorkObjectSN; }
    else if (pick != -1)
    {
        priestTarget = pick;
        HumanAction(priest, pick);
    }
}

void Mgr::offenseInit()
{
    if (base.dr < 0) return;

    if (corner.dr == -1)
    {
        corner.dr = (base.dr * 2 / MAP_L) ? 0 : MAP_L - 1;
        corner.ur = (base.ur * 2 / MAP_U) ? 0 : MAP_U - 1;
    }

    if (siegeSN == -1)
        for (const auto& it : eBuildingMap)
            if (it.second->Type == BUILDING_SIEGE)
            {
                siegeSN = it.first;
                siegePos = {it.second->BlockDR, it.second->BlockUR};
                break;
            }
}

int Mgr::siegeDis(const Pos& p) const { return siegePos.dr < 0 ? dis(corner, p) : dis(siegePos, p); }

void Mgr::offenseUpdate()
{
    tars.clear();

    for (auto it = onMove.begin(); it != onMove.end();)
        if (!army(*it)) it = onMove.erase(it);
        else ++it;

    for (const auto& it : eArmyMap)
    {
        const tagArmy& e = *it.second;
        if (siegeDis({e.BlockDR, e.BlockUR}) > BELONG_DEF) continue;

        if (e.Sort != AT_STONE_THROWER && lureSeen.insert(e.SN).second) lureList.push_back(e.SN);
        tars.push_back(e.SN);
    }

    for (const auto& it : eBuildingMap)
        tars.push_back(it.second->SN);
}

int Mgr::assultETA(const tagArmy& u)
{
    const Pos p = nextToGo(u, false);
    if (p.dr < 0) return 0;

    const double sp = max(unitSpeed(u.Sort), EPS);
    const int step = dis(p, {u.BlockDR, u.BlockUR});
    return (int)(step * (double)BLOCKSIDELENGTH *  PATH_FACTOR / sp);
}

void Mgr::DispatchMove()
{
    for (const auto& it : armyMap)
    {
        const tagArmy& u = *it.second;
        if (u.SN == priest || onMove.count(u.SN)) continue;
        if (gameFrame + assultETA(u) < ASSAULT_FRAME) continue;

        onMove.insert(u.SN);
        moveToCell(u.SN, nextToGo(u));
        assaultOn = true;
    }
}

void Mgr::nextToGoBuild()
{
    goList.clear();
    if (corner.dr < 0) return;

    buildUnknown();

    for (int i = 0; i < MAP_L; i += GO_STRIDE)
        for (int j = 0; j < MAP_U; j += GO_STRIDE)
        {
            if (disSq(Pos(i, j), corner) >= (double)BELONG_DEF * BELONG_DEF) continue;
            if (wpGain({i, j}) < GO_MIN_GAIN) continue;

            Pos st = {-1, -1};
            for (int a = i - 2; a <= i + 2 && st.dr < 0; a++)
                for (int b = j - 2; b <= j + 2; b++)
                    if (walkable(a, b) && nav(a, b) >= 0)
                    {
                        st = {a, b};
                        break;
                    }
            if (st.dr >= 0) goList.push_back(st);
        }
    goUsed.resize(goList.size(), 0);
    fill(goUsed.begin(), goUsed.end(), 0);
}

Pos Mgr::nextToGo(const tagArmy& u, bool occupy)
{
    Pos pick = {-1, -1};
    int pickIdx = -1;
    double best = 0;
    const FloatPos me(u.DR, u.UR);
    for (int i = 0; i < (int)goList.size(); i++)
    {
        const double d = disSq(FloatPos(goList[i]), me);
        if ((pick.dr >= 0 && d >= best) || goUsed[i]) continue;
        best = d;
        pick = goList[i];
        pickIdx = i;
    }
    if (pick.dr == -1 && goList.size()) { pick = goList[0]; pickIdx = 0; }
    if (pick.dr != -1 && occupy) goUsed[pickIdx] = true;
    return pick;
}

int Mgr::selector(const tagArmy& u)
{
    if (tars.empty()) return -1;
    int type = u.Sort;

    auto closer = [&](const int a, const int b)
    {
        Pos A, B, U = {u.BlockDR, u.BlockUR};
        if (enemyArmy(a)) A = {enemyArmy(a)->BlockDR, enemyArmy(a)->BlockUR};
        else A = {enemyBuilding(a)->BlockDR, enemyBuilding(a)->BlockUR};

        if (enemyArmy(b)) B = {enemyArmy(b)->BlockDR, enemyArmy(b)->BlockUR};
        else B = {enemyBuilding(b)->BlockDR, enemyBuilding(b)->BlockUR};

        return disSq(A, U) < disSq(B, U);
    };

    auto validTar = [&](const int t)
    {
        return enemyArmy(t) || enemyBuilding(t);
    };

    switch (type)
    {
    case AT_PRIEST:
        if (enemyArmy(u.WorkObjectSN)) return u.WorkObjectSN;
        else
        {
            auto cand = tars;
            cand.erase(std::remove_if(cand.begin(), cand.end(), [&](const int sn){
                return enemyBuilding(sn);
            }), cand.end());
            if (cand.empty()) return -1;
            std::sort(cand.begin(), cand.end(), closer);
            return cand[0];
        }

    case AT_CAVALRY:
    {
        const tagArmy* cur = enemyArmy(u.WorkObjectSN);
        if (cur && cur->Sort == AT_STONE_THROWER) return u.WorkObjectSN;
        else
        {
            int best = -1;
            auto cand = tars;
            cand.erase(std::remove_if(cand.begin(), cand.end(), [&](const int sn){
                return enemyBuilding(sn) || enemyArmy(sn)->Sort != AT_STONE_THROWER;
            }), cand.end());
            if (cand.size())
            {
                std::sort(cand.begin(), cand.end(), closer);
                return cand[0];
            }
            else
            {
                if (enemyArmy(u.WorkObjectSN)) return u.WorkObjectSN;
                cand = tars;
                std::sort(cand.begin(), cand.end(), closer);
                if (cand.empty()) return -1;
                return cand[0];
            }
        }
    }

    default:
        if (validTar(u.WorkObjectSN)) return u.WorkObjectSN;
        else
        {
            auto cand = tars;
            std::sort(cand.begin(), cand.end(), closer);
            if (cand.empty()) return -1;
            return cand[0];
        }
    }
}

void Mgr::runLure()
{
    if (!assaultOn || combat || lureList.empty()) return;

    for (auto it = lureHold.begin(); it != lureHold.end();)
        if (gameFrame >= it->second || !farmer(it->first)) it = lureHold.erase(it);
        else ++it;

    lureCursor %= lureList.size();
    for (const auto& i : farmerMap)
    {
        const int sn = i.second->SN;
        HumanAction(sn, lureList[lureCursor++]);
        lureCursor %= lureList.size();
        lureHold[sn] = gameFrame + LURE_HOLD;
    }
}

void Mgr::runAssult()
{
    if (!assaultOn) return;

    for (const auto& it : armyMap)
    {
        const tagArmy& u = *it.second;
        if (u.SN == priest || !onMove.count(u.SN)) continue;

        const int t = selector(u);
        if (t >= 0 && u.WorkObjectSN == t) continue;
        if (t >= 0 && u.WorkObjectSN != t)
        {
            HumanAction(u.SN, t);
            continue;
        }

        // 什么都没有
        if (u.NowState != HUMAN_STATE_WALKING) moveToCell(u.SN, nextToGo(u));
    }
}

void Mgr::runAtkPriest()
{
    if (!assaultOn) return;
    const tagArmy* p = army(priest);
    const Pos here = {p->BlockDR, p->BlockUR};

    if (siegeSN >= 0 && eArmyMap.size() <= 2)
    {
        if (p->WorkObjectSN != siegeSN) HumanAction(p->SN, siegeSN);
        return;
    }

    int threshold = siegeSN < 0 ? 40 : 25;
    if (siegeDis({p->BlockDR, p->BlockUR}) < threshold) // 尽快撤离
    {
        double dist = 2e9;
        Pos best = {-1, -1};
        for (int i = 0; i < MAP_L; i++)
            for (int j = 0; j < MAP_U; j++)
                if (siegeDis({i, j}) >= threshold && dis(best, here) > dis({i, j}, here))
                {
                    best = {i, j};
                    dist = dis(best, here);
                }
        if (p->NowState != HUMAN_STATE_WALKING && best.dr != -1) moveToCell(p->SN, best);
    }

    const int t = selector(*p);
    if (t >= 0 && p->WorkObjectSN != t) HumanAction(p->SN, t);
}

void Mgr::offense()
{
    offenseInit();
    nextToGoBuild();

    offenseUpdate();
    DispatchMove();

    runAssult();
    runAtkPriest();
}

void Mgr::clearRoad()
{
    if (!clearRoadUsed.ready()) clearRoadUsed.reset(0);
    clearRoadUsed.fill(0);

    std::vector<Pos> points;
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (nav(i, j) >= 22 && nav(i, j) <= 26)
            {
                clearRoadUsed(i, j) = 1;
                points.push_back({i, j});
            }

    if (points.empty()) return;

    for (const auto& a : armyMap)
    {
        const tagArmy* u = a.second;
        if (onMove.count(u->SN)) continue;
        if (clearRoadUsed(u->BlockDR, u->BlockUR) || u->NowState != HUMAN_STATE_IDLE) continue;

        const int ran = rand() % points.size();
        moveToCell(u->SN, points[ran]);
    }
}

void Mgr::killLions()
{
    if (gameFrame < LION_HUNT_FROM) return;

    const tagResource* tar = nullptr;
    double best = 0;
    for (const tagResource* l : lionSet)
    {
        const double d = dis({l->DR, l->UR}, baseF);
        const double a =
            angle({corner.dr - base.dr, corner.ur - base.ur}, {l->BlockDR - base.dr, l->BlockUR - base.ur});
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
        const int sn = takeNearest({tar->DR, tar->UR}, true);
        if (sn < 0) break;
        lionCrew.insert(sn);
    }

    for (int sn : lionCrew) sendAction(sn, tar->SN);
}

bool Mgr::workerBusy(int sn) const
{
    if (workerToFarm.count(sn) || staffHunt.count(sn) || spotOfWorker.count(sn)) return true;
    if (lionCrew.count(sn) || fixCrew.count(sn)) return true;
    for (const BuildSite& s : sites)
        if (s.workers.count(sn)) return true;
    return false;
}

void Mgr::workerDrop(int sn)
{
    workerJobSince.erase(sn);
    huntDetach(sn);
    dropSpot(sn, false);
    for (BuildSite& s : sites) s.workers.erase(sn);
    lionCrew.erase(sn);
    fixCrew.erase(sn);
}

int Mgr::farmerTargetCnt() const
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

void Mgr::strategy()
{
    int b_prio = 100;
    int e_prio = 100;

    const int homeCnt = min(12, (int)(farmerMap.size() + armyMap.size()) / 4 + 1);
    wantBuilding(BUILDING_HOME, homeCnt, b_prio--);

    // 仓储完全由智能需求决定；第一座谷仓仍作为箭塔科技宿主保底。
    wantStock(b_prio--);
    wantGranary(b_prio--);

    const int farmerTarget = usePopModel ? farmerTargetCnt() : (stage == CIVILIZATION_TOOLAGE ? 10 : 24);
    wantUnit(AT_FARMER, farmerTarget, e_prio--);

    if (stage == CIVILIZATION_TOOLAGE)
    {
        // 第一阶段：军营 -> 靶场；市场并行；三者完成后才允许基地升铜器。
        wantBuilding(BUILDING_ARMYCAMP, 1, b_prio--);
        wantBuilding(BUILDING_RANGE, 1, b_prio--);
        wantBuilding(BUILDING_MARKET, 1, b_prio--);
        wantTech(BUILDING_CENTER_UPGRADE, e_prio--);
    }
    else
    {
        const bool wheelDone = hasTech(BUILDING_MARKET_WHEEL_UPGRADE);
        const bool towerTechDone = hasTech(BUILDING_GRANARY_ARROWTOWER);
        const bool towersDone = buildingCount(BUILDING_ARROWTOWER, true) >= 3;

        if (!wheelDone || !towerTechDone || !towersDone)
        {
            // 第二阶段：轮子 + 箭塔科技；箭塔科技完成后 buildAvailable 才会放行三座箭塔。
            wantTech(BUILDING_MARKET_WHEEL_UPGRADE, e_prio--);
            wantTech(BUILDING_GRANARY_ARROWTOWER, e_prio--);
            wantBuilding(BUILDING_ARROWTOWER, 3, b_prio--);
        }
        else
        {
            const bool academyDone = buildingCount(BUILDING_COLLAGE, true) > 0;
            const bool compositeDone = hasTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW);

            if (!academyDone || !compositeDone)
            {
                // 第三阶段：学院依赖马厩，因此先补马厩；复合弓科技可由已完成靶场并行研究。
                wantBuilding(BUILDING_STABLE, 1, b_prio--);
                wantBuilding(BUILDING_COLLAGE, 1, b_prio--);
                wantTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW, e_prio--);
            }
            else
            {
                // 最后阶段：分批扩军；投石车由祭司转化敌军获得，不进入生产需求。

                const bool batch1 =
                    unitCount(AT_HOPLITE) >= 6 &&
                    unitCount(AT_COMPOSITE_BOWMAN) >= 6 &&
                    unitCount(AT_CAVALRY);
                const bool batch2 =
                    unitCount(AT_HOPLITE) >= 9 &&
                    unitCount(AT_COMPOSITE_BOWMAN) >= 9 &&
                    unitCount(AT_CAVALRY) >= 3;

                if (!batch1)
                {
                    wantUnit(AT_HOPLITE, 6, e_prio--);
                    wantUnit(AT_COMPOSITE_BOWMAN, 6, e_prio--);
                    wantUnit(AT_CAVALRY, 2, e_prio--);
                }
                else if (!batch2)
                {
                    wantUnit(AT_HOPLITE, 9, e_prio--);
                    wantUnit(AT_COMPOSITE_BOWMAN, 9, e_prio--);
                    wantUnit(AT_CAVALRY, 3, e_prio--);
                }
                else
                {
                    wantUnit(AT_HOPLITE, 12, e_prio--);
                    wantUnit(AT_COMPOSITE_BOWMAN, 12, e_prio--);
                    wantUnit(AT_CAVALRY, 4, e_prio--);
                }
            }
        }
    }

    econPlan();

    if (wantFarm > 0)
        wantBuilding(BUILDING_FARM, buildingCount(BUILDING_FARM) + wantFarm, FARM_PRIORITY);
}

void Mgr::update(const tagInfo& info)
{
    makeFrame(info);
    navBuild();
    arrangeGather();
    buildFrame();
    farmFrame();
    prodFrame();
    huntFrame();
    laborBuild();
    gatherReset();
    huntReset();
    scoutFrame();

    threatBuild();
    defence();
    if (!combat) runScout();
    if (!combat && !assaultOn) clearRoad();

    killLions();

    offense();

    strategy();  // econPlan 在这里定下各岗位人数
    laborRelease();
    laborBuild();

    held = Stock();  // 生产预定只在本帧有效

    runProd();
    runBuild();

    runFarm();
    arrangeHunt();
    runGather();

    runDestroy();

    runLure();
}