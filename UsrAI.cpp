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
{ return ((long long)(type + 1) << 32) | (unsigned int)cellIdx(dr, ur); }

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

    for (const auto& it : eArmyMap) threatStamp(it.second->BlockDR, it.second->BlockUR, ENEMY_KEEP);

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
    const tagFarmer* f = farmer(workerSN);
    if (!f) return;
    if (f->WorkObjectSN == targetSN && f->NowState != HUMAN_STATE_IDLE) return;
    HumanAction(workerSN, targetSN);
}

// 只重建空闲池, 不动 claimed —— claimed 必须整帧有效, 否则前半帧派给修塔/打狮子的人
// 到了后半帧又会被建造系统抢走, 下一帧再被抢回来, 两边都到不了目的地
void Mgr::laborBuild()
{
    laborPool.clear();
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

            // 活猎物会跑, 它的 cost 每帧都在变, spots 排序跟着抖. 正在追杀的这一条
            // 绑定不能因为排名波动被撤掉, 否则击杀永远完不成
            const tagResource* r = resource(p.spots[i].sn);
            const tagFarmer* f = farmer(sn);
            if (r && r->Blood > 0 && f && f->WorkObjectSN == r->SN) continue;

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

    // 抢人只能抢采集工. 工地/修塔/打狮子/农田这些专职岗位彼此不许互挖 ——
    // 那正是"两个工地反复对着同一个人下令"的来源
    cand.clear();
    for (const auto& it : farmerMap)
    {
        const int sn = it.first;
        if (claimed.count(sn) || workerReserved(sn)) continue;
        cand.push_back(sn);
    }

    best = nearestOf(cand, at);
    if (best < 0) return -1;

    workerDrop(best);  // 防止 sn 同时属于多个岗位
    claimed.insert(best);
    return best;
}

// 交还空闲池. 调用前必须已经解除绑定, 否则 workerBusy 会把它挡在池外;
// 同时防重复入池, 免得一个人在池里出现两次
void Mgr::freeWorker(int sn)
{
    if (!farmer(sn) || workerBusy(sn)) return;
    if (std::find(laborPool.begin(), laborPool.end(), sn) != laborPool.end()) return;
    laborPool.push_back(sn);
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

    auto meatOk = [&](const tagResource* r)
    {
        if (kindOf(r->Type) != RK_CORPSE) return false;
        return r->Blood <= 0 || r->Type == RESOURCE_GAZELLE;
    };

    std::vector<const tagResource*> meats;
    for (const auto& it : resourceMap)
        if (meatOk(it.second)) meats.push_back(it.second);

    std::unordered_set<int> grouped;
    const double meatGapSq = (double)CORPSE_GROUP_GAP * BLOCKSIDELENGTH * CORPSE_GROUP_GAP * BLOCKSIDELENGTH;
    for (int i = 0; i < (int)meats.size(); i++)
        for (int j = i + 1; j < (int)meats.size(); j++)
            if (disSq(FloatPos(meats[i]->DR, meats[i]->UR), FloatPos(meats[j]->DR, meats[j]->UR)) <= meatGapSq)
            {
                grouped.insert(meats[i]->SN);
                grouped.insert(meats[j]->SN);
            }

    std::vector<int> fled;
    for (auto it = spotOfWorker.begin(); it != spotOfWorker.end();)
    {
        const tagResource* r = resource(it->second);
        const bool unsafe = r && threatAt(r->BlockDR, r->BlockUR) && dis({r->DR,r->UR}, baseF) > 40 * BLOCKSIDELENGTH;
        const bool gone = !farmer(it->first) || !r || kindOf(r->Type) == RK_COUNT || unsafe ||
                          (kindOf(r->Type) == RK_CORPSE && !meatOk(r));
        if (!gone)
        {
            it++;
            continue;
        }
        if (unsafe) fled.push_back(it->first);

        workerOfSpot.erase(it->second);
        workerJobSince.erase(it->first);
        it = spotOfWorker.erase(it);
    }

    for (int sn : fled)
        if (farmer(sn)) moveToCell(sn, base);

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

        if (threatAt(r->BlockDR, r->BlockUR) && dis({r->DR,r->UR}, baseF) > 40 * BLOCKSIDELENGTH) continue;

        if (k == RK_CORPSE)
        {
            if (!meatOk(r)) continue;
            if (!workerOfSpot.count(r->SN))  // 已经绑上的豁免落单与狮子判定
            {
                if (!grouped.count(r->SN)) continue;
                if (nearLion(r->BlockDR, r->BlockUR, LION_KEEP)) continue;
            }
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
        if (x.k == RK_CORPSE && x.r->Blood > 0 && s.rate > EPS)
        {
            const double yield = (double)CNT_GAZELLE;
            s.rate = yield / (yield / s.rate + x.r->Blood / HUNT_DPS);
        }
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

void Mgr::farmFrame()
{
    farmList.clear();
    farmRates.clear();

    std::vector<std::pair<double, int>> v;
    for (int sn : buildingsOf(BUILDING_FARM))
    {
        const tagBuilding* b = building(sn);
        if (!b || b->Percent < 100) continue;
        if (threatAt(b->BlockDR, b->BlockUR) && dis({b->BlockDR,b->BlockUR}, base) > 40) continue;  // 敌人跟前不耕地

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
    const int sn = it->second;
    workerJobSince.erase(sn);
    workerToFarm.erase(sn);
    farmToWorker.erase(it);
    freeWorker(sn);  // 必须在两张表都清掉之后, 否则 workerBusy 会挡住入池
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

int Mgr::econPhase()
{
    if (!towersDone && buildingCount(BUILDING_ARROWTOWER, true) >= 3) towersDone = true;

    int phase = -1;
    if (hasTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW) && hasTech(BUILDING_MARKET_WOOD_UPGRADE)) phase = 3;
    else if (hasTech(BUILDING_MARKET_WHEEL_UPGRADE) && towersDone) phase = 2;
    else if (stage != CIVILIZATION_TOOLAGE) phase = 1;
    else phase = 0;

    econStage = max(econStage, phase);
    return econStage;
}

void Mgr::econCommit()
{
    int ideal[ECON_GROUP_COUNT] = {};
    for (int k = 0; k < RK_COUNT; k++) ideal[k] = pools[k].desired;
    ideal[ECON_FARM] = farmDesired;
    for (int g = 0; g < ECON_GROUP_COUNT; g++) econIdeal[g] = ideal[g];

    auto apply = [&]()
    {
        for (int k = 0; k < RK_COUNT; k++) pools[k].desired = econCommitted[k];
        farmDesired = econCommitted[ECON_FARM];
    };

    if (!econInitialized)
    {
        for (int g = 0; g < ECON_GROUP_COUNT; g++) econCommitted[g] = ideal[g];
        econInitialized = true;
        econLastFrame = gameFrame;
        apply();
        return;
    }

    // 资源/农田消失
    for (int k = 0; k < RK_COUNT; k++) econCommitted[k] = min(econCommitted[k], (int)pools[k].spots.size());
    econCommitted[ECON_FARM] = min(econCommitted[ECON_FARM], (int)farmList.size());

    // 新造出的村民只填补已有目标的空缺
    int committedTotal = 0;
    int idealTotal = 0;
    for (int g = 0; g < ECON_GROUP_COUNT; g++)
    {
        committedTotal += econCommitted[g];
        idealTotal += ideal[g];
    }
    const int usable = min(econPop, idealTotal);
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

    // 每 15 秒最多迁移一个已有岗位
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

    // 人口突然死亡时不保留不可能实现的岗位总量
    int total = 0;
    for (int g = 0; g < ECON_GROUP_COUNT; g++) total += econCommitted[g];
    while (total > econPop)
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
    farmDesired = 0;
    wantFarm = 0;

    // 建造工、修塔工、打狮子的人不归经济系统调度. 不把他们扣掉的话目标总量永远填不满,
    // takeNearest 每帧返回 -1, 表现出来就是"分配混乱"
    const int reserved = CREW_BUILD * (int)sites.size() + (int)fixCrew.size() + (int)lionCrew.size();
    const int pop = max(0, (int)farmerMap.size() - reserved);
    econPop = pop;
    if (pop <= 0)
    {
        econCommit();
        return;
    }

    const EconProfile& profile = ECON_PROFILE[econPhase()];
    const int have[4] = {res.wood, res.meat, res.stone, res.gold};

    for (int r = 0; r < 4; r++)
    {
        if (have[r] < profile.low[r]) econBoost[r] = true;
        else if (have[r] >= profile.high[r]) econBoost[r] = false;
    }

    const int meatCap = (int)pools[RK_CORPSE].spots.size();  // 活猎物 + 尸体
    const int bushCap = (int)pools[RK_BUSH].spots.size();
    const int farmCap = (int)farmList.size();
    const int naturalFoodCap = meatCap + bushCap;

    int currentCap[4] = {(int)pools[RK_WOOD].spots.size(), naturalFoodCap + farmCap, (int)pools[RK_STONE].spots.size(),
                         (int)pools[RK_GOLD].spots.size()};
    // 农田可以现建, 所以规划阶段食物不设上限: 差多少由 farmShortage 反推出来
    int planCap[4] = {currentCap[0], buildAvailable(BUILDING_FARM) ? pop : currentCap[1], currentCap[2], currentCap[3]};

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

    // 约 80% 人口保持阶段预设；最多 ECON_FLEX_MAX 人承担库存带的动态修正。
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

    // 食物内部是一条固定优先级的队列: 打猎/尸体 -> 浆果 -> 农田.
    // 池内本来就按 cost 升序, 农田按 rate 降序, 所以"取前 n 个"就是最优解.
    // 三者之间不再互相比边际效率 —— 每帧重排名次正是之前行为抖动的根源
    int left = raw[1];
    pools[RK_CORPSE].desired = min(left, meatCap);
    left -= pools[RK_CORPSE].desired;
    pools[RK_BUSH].desired = min(left, bushCap);
    left -= pools[RK_BUSH].desired;
    farmDesired = min(left, farmCap);
    left -= farmDesired;

    const int farmShortage = left;  // 有食物人口, 但没有食物岗位

    pools[RK_WOOD].desired = min(raw[0], currentCap[0]);
    pools[RK_STONE].desired = min(raw[2], currentCap[2]);
    pools[RK_GOLD].desired = min(raw[3], currentCap[3]);

    // 缺口不预测, 直接测量: 每帧最多批一块, 建成后 farmCap 加一, 下一帧缺口自动减一.
    // pendingFarm 挡住在建期间重复批
    const int pendingFarm =
        buildingCount(BUILDING_FARM) - buildingCount(BUILDING_FARM, true) + queuedBuild(BUILDING_FARM);
    if (buildAvailable(BUILDING_FARM) && farmShortage > pendingFarm && farmCap + pendingFarm < FARM_MAX) wantFarm = 1;

    // 等农田的人去砍木头: 农田的瓶颈本来就是木料, 这是个闭环而不是浪费
    const int woodRoom = max(0, currentCap[0] - pools[RK_WOOD].desired);
    pools[RK_WOOD].desired += min(farmShortage, woodRoom);

    // 还有人闲着就填到仍有空位的岗位上
    auto assignedNow = [&]()
    {
        int n = farmDesired;
        for (int k = 0; k < RK_COUNT; k++) n += pools[k].desired;
        return n;
    };
    auto foodRoom = [&]()
    { return (meatCap - pools[RK_CORPSE].desired) + (bushCap - pools[RK_BUSH].desired) + (farmCap - farmDesired); };

    while (assignedNow() < pop)
    {
        int pick = -1;
        double best = -1;
        for (int r = 0; r < 4; r++)
        {
            int cnt = 0;
            if (r == 1)
            {
                if (foodRoom() <= 0) continue;
                cnt = pools[RK_CORPSE].desired + pools[RK_BUSH].desired + farmDesired;
            }
            else
            {
                const ResKind k = r == 0 ? RK_WOOD : (r == 2 ? RK_STONE : RK_GOLD);
                if (pools[k].desired >= currentCap[r]) continue;
                cnt = pools[k].desired;
            }

            double score = profile.weight[r] > 0 ? (double)profile.weight[r] / (cnt + 1) : 0.01;
            if (econBoost[r]) score += 10.0;
            if (score > best) best = score, pick = r;
        }
        if (pick < 0) break;

        if (pick == 1)
        {
            if (pools[RK_CORPSE].desired < meatCap) pools[RK_CORPSE].desired++;
            else if (pools[RK_BUSH].desired < bushCap) pools[RK_BUSH].desired++;
            else farmDesired++;
        }
        else pools[pick == 0 ? RK_WOOD : (pick == 2 ? RK_STONE : RK_GOLD)].desired++;
    }

    econCommit();
}

bool Mgr::buildAvailable(int type) const
{
    switch (type)
    {
        case BUILDING_RANGE:
        case BUILDING_STABLE: return buildingCount(BUILDING_ARMYCAMP, true) > 0;

        case BUILDING_COLLAGE: return buildingCount(BUILDING_STABLE, true) > 0;

        case BUILDING_FARM: return buildingCount(BUILDING_MARKET, true) > 0;

        case BUILDING_ARROWTOWER: return hasTech(BUILDING_GRANARY_ARROWTOWER);

        case BUILDING_SIEGE: return false;  // 本局规则禁止建造；投石车只来自祭司转化

        default: return true;
    }
}

void Mgr::buildFrame()
{
    builds.clear();
    if (!costMap.ready()) costMap.reset(0);

    // 智能建造
    granaryPendings.clear();
    depotWant(RK_BUSH, granaryPendings);

    stockPendings.clear();
    depotWant(RK_CORPSE, stockPendings);
    depotWant(RK_GOLD, stockPendings);

    // 历史农田只有仍在实际耕作时才保留仓储需求
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

// 追加而不是覆盖: 一座仓库同时服务尸体和金矿, 两边的远端锚点要能并在一张清单里
void Mgr::depotWant(ResKind k, std::vector<Pos>& out) const
{
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

void Mgr::buildPlaceMask(int size)
{
    placeOk.reset(0);

    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
        {
            if (!canPlace(i, j, size)) continue;
            if (threatAt(i, j) && dis({i, j}, base) > 40) continue;  // 敌人跟前不盖房

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

    buildPlaceMask(size);

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
        case BUILDING_HOME:
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

        case BUILDING_ARROWTOWER:
            for (const auto& it : buildingMap)
                if (it.second->Type == BUILDING_ARROWTOWER)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_ARROWTOWER),
                            PLACE_ADJACENT * 3, 0, 9);
            break;

        default: break;  // 谷仓/仓库走通用布局 + 下方智能运输收益，不在 switch 中另设固定环
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

            if (s.sn < 0 && gameFrame - s.born < BUILD_WAIT)
            {
                ++it;
                continue;
            }

            if (s.sn < 0)
            {
                if (!s.workers.empty())
                {
                    int& fail = failedSpots[placeFailKey(s.type, s.site.dr, s.site.ur)];
                    fail = min(fail + 1, PLACE_FAIL_CAP);
                }
                const std::set<int> crew = s.workers;
                it = sites.erase(it);
                for (int sn : crew) freeWorker(sn);
                continue;
            }
        }

        const tagBuilding* b = building(s.sn);
        if (!b || b->Percent >= 100)
        {
            const std::set<int> crew = s.workers;
            it = sites.erase(it);
            for (int sn : crew) freeWorker(sn);
            continue;
        }
        ++it;
    }

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
        s.born = gameFrame;
        sites.push_back(s);
    }

    for (BuildSite& s : sites)
    {
        if (s.sn < 0) continue;  // 地基还没出现, 既不加人也不下令(否则会对 -1 发指令)

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
        s.born = gameFrame;
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
}

bool Mgr::techAvailable(int action) const
{
    const int host = actionHost(action);
    if (host < 0 || buildingCount(host, true) <= 0) return false;

    switch (action)
    {
        case BUILDING_CENTER_UPGRADE:
            return stage == CIVILIZATION_TOOLAGE && buildingCount(BUILDING_MARKET, true) > 0 &&
                   buildingCount(BUILDING_ARMYCAMP, true) > 0 && buildingCount(BUILDING_RANGE, true) > 0;

        case BUILDING_MARKET_WHEEL_UPGRADE: return stage == CIVILIZATION_BRONZEAGE;

        case BUILDING_RANGE_UPGRADE_COMPOSITE_BOW: return stage == CIVILIZATION_BRONZEAGE;

        default: return true;
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

    // 只补不拆: 军事单位超编不该去拆村民, 那是 runDestroy 按人口上限单独管的事
    int diff = total - unitCount(type) - queuedProd(action);
    for (; diff > 0; diff--) prods.insert({priority, action});
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

// 人口逼近上限时拆掉少量村民, 把人口让给军队. 只拆闲人和采集工, 而且必须先解绑:
// 直接自毁会在各张岗位表里留下指向死人的记录, 那个岗位到下一帧才会被回收
void Mgr::runDestroy()
{
    int excess = (int)farmerMap.size() - farmerTarget();
    if (excess <= 0) return;

    std::vector<int> cand;
    cand.reserve(farmerMap.size());

    for (int sn : laborPool)  // 先拆真正闲着的
        if (farmer(sn) && !workerReserved(sn)) cand.push_back(sn);

    for (const auto& it : farmerMap)  // 不够再拆采集工
    {
        const int sn = it.first;
        if (workerReserved(sn)) continue;
        if (std::find(cand.begin(), cand.end(), sn) != cand.end()) continue;
        cand.push_back(sn);
    }

    for (int sn : cand)
    {
        if (excess-- <= 0) break;
        workerDrop(sn);
        HumanAction(sn, sn);
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
        if (d < TOWER_ALERT * BLOCKSIDELENGTH) towerAtk.push_back(it.first);
        if (d < DEF_ALERT * BLOCKSIDELENGTH) hostiles.push_back(it.first);
    }

    fixTower();

    combat = hostiles.size() > 0;
    if (!combat) return;

    std::sort(hostiles.begin(), hostiles.end());

    runTower();
    runPriest();
    runArmy();
}

void Mgr::fixTower()
{
    if (res.stone <= 0 || gameFrame > FIX_TOWER_UNTIL)
    {
        const std::set<int> crew = fixCrew;
        fixCrew.clear();
        for (int sn : crew) freeWorker(sn);
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
        const std::set<int> crew = fixCrew;
        fixCrew.clear();
        for (int sn : crew) freeWorker(sn);
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

int Mgr::defenceSelector(const tagArmy& u) const
{
    const Pos me = {u.BlockDR, u.BlockUR};

    auto isHostile = [&](int sn) { return enemyArmy(sn) && std::binary_search(hostiles.begin(), hostiles.end(), sn); };

    auto closer = [&](int a, int b)
    {
        const tagArmy* A = enemyArmy(a);
        const tagArmy* B = enemyArmy(b);
        const double da = disSq(Pos(A->BlockDR, A->BlockUR), me);
        const double db = disSq(Pos(B->BlockDR, B->BlockUR), me);
        return da != db ? da < db : a < b;
    };

    auto nearest = [&](bool attracted, bool allowStone)
    {
        int pick = -1;
        for (int sn : hostiles)
        {
            const tagArmy* e = enemyArmy(sn);
            if (!e || (!allowStone && e->Sort == AT_STONE_THROWER)) continue;
            if ((lockOf(sn) >= 0) != attracted) continue;
            if (pick < 0 || closer(sn, pick)) pick = sn;
        }
        return pick;
    };

    const tagArmy* cur = enemyArmy(u.WorkObjectSN);

    if (u.Sort == AT_PRIEST)
    {
        if (cur && isHostile(cur->SN) && cur->Sort == AT_STONE_THROWER && lockOf(cur->SN) >= 0) return cur->SN;

        int pick = -1;
        for (int sn : hostiles)
        {
            const tagArmy* e = enemyArmy(sn);
            if (!e || e->Sort != AT_STONE_THROWER || lockOf(sn) < 0) continue;
            if (pick < 0 || closer(sn, pick)) pick = sn;
        }
        if (pick >= 0) return pick;

        if (cur && isHostile(cur->SN) && lockOf(cur->SN) >= 0) return cur->SN;
        return nearest(true, true);
    }

    // 防守时敌投石车只留给祭司转化。
    if (cur && isHostile(cur->SN) && cur->Sort != AT_STONE_THROWER)
    {
        if (u.Sort != AT_STONE_THROWER || lockOf(cur->SN) >= 0) return cur->SN;
    }

    if (u.Sort == AT_STONE_THROWER) return nearest(true, false);

    const int attracted = nearest(true, false);
    return attracted >= 0 ? attracted : nearest(false, false);
}

void Mgr::runArmy()
{
    for (const auto& it : armyMap)
    {
        const tagArmy& u = *it.second;
        if (u.SN == priest || onMove.count(u.SN)) continue;

        const int tar = defenceSelector(u);
        if (tar >= 0)
        {
            if (u.WorkObjectSN != tar) HumanAction(u.SN, tar);
            continue;
        }

        if (enemyArmy(u.WorkObjectSN)) moveToCell(u.SN, {u.BlockDR, u.BlockUR});
    }
}

void Mgr::runPriest()
{
    if (assaultOn) return;

    const tagArmy* p = army(priest);
    if (!p) return;

    const int tar = defenceSelector(*p);
    if (tar >= 0)
    {
        if (p->WorkObjectSN != tar) HumanAction(priest, tar);
        return;
    }

    if (enemyArmy(p->WorkObjectSN)) moveToCell(priest, {p->BlockDR, p->BlockUR});
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

        if (e.Sort != AT_STONE_THROWER) lureList.push_back(e.SN);
        tars.push_back(e.SN);
    }

    for (const auto& it : eBuildingMap)
        if (it.second->Type != BUILDING_SIEGE) tars.push_back(it.second->SN);
}

void Mgr::DispatchMove()
{
    if (gameFrame < ASSAULT_RANGED) return;
    assaultOn = true;

    for (const auto& it : armyMap)
    {
        const tagArmy& u = *it.second;
        if (u.SN == priest || onMove.count(u.SN)) continue;

        const bool ranged = u.Sort == AT_COMPOSITE_BOWMAN || u.Sort == AT_STONE_THROWER || u.Sort == AT_SCOUT;
        const bool special = !ranged;

        if (ranged && gameFrame >= ASSAULT_RANGED) onMove.insert(u.SN);
        if (special && gameFrame >= ASSAULT_SPECIAL) onMove.insert(u.SN);
    }
}

void Mgr::nextToGoBuild()
{
    goPoint = {-1, -1};
    if (corner.dr < 0 || base.dr < 0) return;

    std::vector<Pos> sources;
    for (const auto& it : eBuildingMap)
    {
        const tagBuilding& b = *it.second;
        if (b.Type != BUILDING_CENTER) continue;

        const int s = buildingSize(BUILDING_CENTER);
        for (int i = b.BlockDR; i < b.BlockDR + s; i++)
            for (int j = b.BlockUR; j < b.BlockUR + s; j++)
                if (inMap(i, j)) sources.push_back({i, j});
        break;
    }
    if (sources.empty()) sources.push_back(corner);

    auto assumedWalkable = [&](int dr, int ur)
    {
        if (!inMap(dr, ur)) return false;
        const int type = cell(dr, ur).type;
        if (type == MAPPATTERN_UNKNOWN) return true;
        if (type != MAPPATTERN_GRASS && type != MAPPATTERN_DESERT) return false;
        return !blockCell(dr, ur);
    };

    std::vector<unsigned char> used((size_t)MAP_L * MAP_U, 0);
    std::queue<Pos> q;
    for (const Pos& s : sources)
    {
        const int idx = cellIdx(s.dr, s.ur);
        if (used[idx]) continue;
        used[idx] = 1;
        q.push(s);
    }

    while (!q.empty())
    {
        const int layer = (int)q.size();
        Pos best = {-1, -1};
        int bestNav = 0;

        for (int t = 0; t < layer; t++)
        {
            const Pos cur = q.front();
            q.pop();

            if (nav(cur) >= 0)
            {
                if (best.dr < 0 || nav(cur) < bestNav ||
                    (nav(cur) == bestNav && cellIdx(cur.dr, cur.ur) < cellIdx(best.dr, best.ur)))
                {
                    best = cur;
                    bestNav = nav(cur);
                }
                continue;
            }

            for (int d = 0; d < 8; d++)
            {
                const Pos nxt = {cur.dr + dx[d], cur.ur + dy[d]};
                if (!inMap(nxt.dr, nxt.ur)) continue;

                const int idx = cellIdx(nxt.dr, nxt.ur);
                if (used[idx] || !assumedWalkable(nxt.dr, nxt.ur)) continue;

                if (dx[d] && dy[d] &&
                    (!assumedWalkable(cur.dr + dx[d], cur.ur) || !assumedWalkable(cur.dr, cur.ur + dy[d])))
                    continue;

                used[idx] = 1;
                q.push(nxt);
            }
        }

        if (best.dr >= 0)
        {
            goPoint = best;
            return;
        }
    }
}

Pos Mgr::nextToGo(const tagArmy& u) const
{
    (void)u;
    return goPoint;
}

 int Mgr::selector(const tagArmy& u)
{
    const int type = u.Sort;
    if (type != AT_CAVALRY && tars.empty()) return -1;

    auto closer = [&](const int a, const int b)
    {
        Pos A, B, U = {u.BlockDR, u.BlockUR};
        if (enemyArmy(a)) A = {enemyArmy(a)->BlockDR, enemyArmy(a)->BlockUR};
        else A = {enemyBuilding(a)->BlockDR, enemyBuilding(a)->BlockUR};

        if (enemyArmy(b)) B = {enemyArmy(b)->BlockDR, enemyArmy(b)->BlockUR};
        else B = {enemyBuilding(b)->BlockDR, enemyBuilding(b)->BlockUR};

        const int da = disSq(A, U), db = disSq(B, U);
        return da != db ? da < db : a < b;
    };

    auto validTar = [&](const int t)
    {
        if (enemyArmy(t)) return true;
        const tagBuilding* b = enemyBuilding(t);
        return b && b->Type != BUILDING_SIEGE;
    };

    auto lured = [&](const int sn) { /*return lockOf(sn) >= 0; */ return true;};

    auto pickLured = [&](std::vector<int> cand) -> int
    {
        cand.erase(std::remove_if(cand.begin(), cand.end(), [&](const int sn) { return !lured(sn); }), cand.end());
        if (cand.empty()) return -1;
        std::sort(cand.begin(), cand.end(), closer);
        return cand[0];
    };

    switch (type)
    {
        case AT_PRIEST:
            if (enemyArmy(u.WorkObjectSN)) return u.WorkObjectSN;
            else
            {
                auto cand = tars;
                cand.erase(std::remove_if(cand.begin(), cand.end(), [&](const int sn) { return enemyBuilding(sn); }),
                           cand.end());
                if (cand.empty()) return -1;

                const int l = pickLured(cand);
                if (l >= 0) return l;

                std::sort(cand.begin(), cand.end(), closer);
                return cand[0];
            }

        case AT_CAVALRY:
        {
            const tagArmy* cur = enemyArmy(u.WorkObjectSN);
            if (cur && cur->Sort == AT_STONE_THROWER) return u.WorkObjectSN;

            std::vector<int> stoneThrowers;
            for (const auto& it : eArmyMap)
                if (it.second->Sort == AT_STONE_THROWER) stoneThrowers.push_back(it.first);

            if (!stoneThrowers.empty())
            {
                std::sort(stoneThrowers.begin(), stoneThrowers.end(), closer);
                return stoneThrowers[0];
            }

            // 还离推进边界较远时不被普通目标拖住，继续向 goPoint 推进以扩大视野
            const Pos p = nextToGo(u);
            if (p.dr >= 0 && dis(p, {u.BlockDR, u.BlockUR}) > CAVALRY_SEARCH_GAP) return -1;

            // 已经推进到前沿仍没找到投石车，再退回原来的普通索敌
            if (enemyArmy(u.WorkObjectSN)) return u.WorkObjectSN;
            auto cand = tars;
            std::sort(cand.begin(), cand.end(), closer);
            if (cand.empty()) return -1;
            return cand[0];
        }

        default:
        {
            if (validTar(u.WorkObjectSN) && lured(u.WorkObjectSN)) return u.WorkObjectSN;

            const int l = pickLured(tars);
            if (l >= 0) return l;

            if (validTar(u.WorkObjectSN)) return u.WorkObjectSN;

            auto cand = tars;
            std::sort(cand.begin(), cand.end(), closer);
            if (cand.empty()) return -1;
            return cand[0];
        }
    }
}

bool Mgr::kite(const tagArmy& u, int radius)
{
    const Pos here = {u.BlockDR, u.BlockUR};

    bool close = false;
    for (const auto& it : eArmyMap)
    {
        const tagArmy& e = *it.second;
        if (dis(here, {e.BlockDR, e.BlockUR}) <= radius)
        {
            close = true;
            break;
        }
    }
    if (!close) return false;

    if (nav(here) <= 0) return true;

    Pos cur = here;
    for (int step = 0; step < 2; step++) // 默认采用一次退2格, 没必要改
    {
        Pos best = {-1, -1};
        int bestNav = nav(cur);

        for (int d = 0; d < 8; d++)
        {
            const Pos n = {cur.dr + dx[d], cur.ur + dy[d]};
            if (!walkable(n.dr, n.ur)) continue;

            const int v = nav(n);
            if (v < 0 || v >= bestNav) continue;
            bestNav = v;
            best = n;
        }
        if (best.dr < 0) break;
        cur = best;
    }

    if (cur.dr == here.dr && cur.ur == here.ur) return true;

    if (record[u.SN].dr == cur.dr && record[u.SN].ur == cur.ur) return true;

    moveToCell(u.SN, cur);
    record[u.SN] = cur;
    return true;
}

void Mgr::runLure()
{
    if (!assaultOn || combat || lureList.empty()) return;

    for (auto it = lureCrew.begin(); it != lureCrew.end();)
        if (!farmer(*it)) it = lureCrew.erase(it);
        else ++it;

    if ((int)lureCrew.size() < LURE_CREW)
    {
        std::vector<int> cand;
        cand.reserve(farmerMap.size());
        for (const auto& it : farmerMap)
            if (!lureCrew.count(it.first)) cand.push_back(it.first);
        std::sort(cand.begin(), cand.end());

        for (int sn : cand)
        {
            lureCrew.insert(sn);
            if ((int)lureCrew.size() >= LURE_CREW) break;
        }
    }

    for (int sn : lureCrew)
    {
        lureCursor %= (int)lureList.size();
        HumanAction(sn, lureList[lureCursor++]);
    }
}

void Mgr::runAssult()
{
    if (!assaultOn) return;

    for (const auto& it : armyMap)
    {
        const tagArmy& u = *it.second;
        if (u.SN == priest || !onMove.count(u.SN)) continue;

        const Pos p = nextToGo(u);

        int keep = 0;
        if (u.Sort == AT_STONE_THROWER) keep = KITE_STONE;
        if (u.Sort == AT_COMPOSITE_BOWMAN) keep = KITE_BOW;

        if (keep > 0 && kite(u, keep)) continue;

        const int t = selector(u);
        if (t >= 0 && u.WorkObjectSN == t) continue;
        if (t >= 0)
        {
            HumanAction(u.SN, t);
            continue;
        }

        // 什么都没有
        if (p.dr >= 0 && (u.NowState != HUMAN_STATE_WALKING || (u.Sort == AT_CAVALRY && u.WorkObjectSN >= 0)))
            moveToCell(u.SN, p);
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

    int threshold = siegeSN < 0 ? 60 : 40;
    if (siegeDis({p->BlockDR, p->BlockUR}) < threshold)  // 尽快撤离
    {
        Pos best = {-1, -1};
        int bestDis = 0;
        for (int i = 0; i < MAP_L; i++)
            for (int j = 0; j < MAP_U; j++)
            {
                if (!walkable(i, j) || nav(i, j) < 0) continue;
                if (siegeDis({i, j}) < threshold) continue;

                const int d = dis({i, j}, here);
                if (best.dr >= 0 && d >= bestDis) continue;
                bestDis = d;
                best = {i, j};
            }
        if (p->NowState != HUMAN_STATE_WALKING && best.dr >= 0) moveToCell(p->SN, best);
    }
    else if (siegeDis({p->BlockDR, p->BlockUR}) > threshold + 5)
    {
        Pos best = {-1, -1};
        int bestDis = 0;
        for (int i = 0; i < MAP_L; i++)
            for (int j = 0; j < MAP_U; j++)
            {
                if (!walkable(i, j) || nav(i, j) < 0) continue;
                if (siegeDis({i, j}) < threshold) continue;

                const int d = siegeDis({i, j});
                if (best.dr >= 0 && d >= bestDis) continue;
                bestDis = d;
                best = {i, j};
            }
        if (p->NowState != HUMAN_STATE_WALKING && best.dr >= 0) moveToCell(p->SN, best);
    }
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
    const tagResource* tar = nullptr;
    double best = 0;
    for (const tagResource* l : lionSet)
    {
        if (threatAt(l->BlockDR, l->BlockUR) && dis({l->DR, l->UR}, baseF) > 40 * BLOCKSIDELENGTH) continue;

        const double d = dis({l->DR, l->UR}, baseF);
        if (!tar || d < best || (d == best && l->SN < tar->SN))
        {
            best = d;
            tar = l;
        }
    }

    if (tar && best > 40 * BLOCKSIDELENGTH && gameFrame < LION_HUNT_FROM) return;

    for (auto it = lionCrew.begin(); it != lionCrew.end();)
        if (farmer(*it)) it++;
        else it = lionCrew.erase(it);

    if (!tar)
    {
        const std::set<int> crew = lionCrew;
        lionCrew.clear();
        for (int sn : crew) freeWorker(sn);
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
    if (spotOfWorker.count(sn)) return true;
    return workerReserved(sn);
}

bool Mgr::workerReserved(int sn) const
{
    if (workerToFarm.count(sn) || lionCrew.count(sn) || fixCrew.count(sn)) return true;
    for (const BuildSite& s : sites)
        if (s.workers.count(sn)) return true;
    return false;
}

void Mgr::workerDrop(int sn)
{
    workerJobSince.erase(sn);
    dropSpot(sn, false);
    for (BuildSite& s : sites) s.workers.erase(sn);
    lionCrew.erase(sn);
    fixCrew.erase(sn);

    auto it = workerToFarm.find(sn);  // 兜底: 正常路径由 workerReserved 挡住
    if (it != workerToFarm.end())
    {
        farmToWorker.erase(it->second);
        workerToFarm.erase(it);
    }
}

// 不点军营的后勤科技, 所以每个军事单位一律占 1 人口, 不用再分兵种讨论
int Mgr::farmerTargetCnt() const
{
    const int armyPop = (int)armyMap.size();

    const int MIN_FARMER = 10;
    const int HEADROOM = 2;
    return std::max(MIN_FARMER, std::min(24, 50 - armyPop - HEADROOM));
}

// 生产和自毁共用同一个口径, 免得一边在造一边在拆
int Mgr::farmerTarget() const
{
    if (usePopModel) return farmerTargetCnt();
    return stage == CIVILIZATION_TOOLAGE ? 10 : 24;
}

void Mgr::strategy()
{
    int b_prio = 100;
    int e_prio = 100;

    const int homeCnt = min(12, (int)(farmerMap.size() + armyMap.size()) / 4 + 1);
    wantBuilding(BUILDING_HOME, homeCnt, b_prio--);

    wantStock(b_prio--);
    wantGranary(b_prio--);

    wantUnit(AT_FARMER, farmerTarget(), e_prio--);

    if (stage == CIVILIZATION_TOOLAGE)
    {
        // 0
        wantBuilding(BUILDING_ARMYCAMP, 1, b_prio--);
        wantBuilding(BUILDING_RANGE, 1, b_prio--);
        wantBuilding(BUILDING_MARKET, 1, b_prio--);
        wantTech(BUILDING_CENTER_UPGRADE, e_prio--);
    }
    else
    {
        const bool wheelDone = hasTech(BUILDING_MARKET_WHEEL_UPGRADE);

        if (!wheelDone || !towersDone)
        {
            // 1
            wantTech(BUILDING_MARKET_WHEEL_UPGRADE, e_prio--);
            wantTech(BUILDING_GRANARY_ARROWTOWER, e_prio--);
            wantTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW, e_prio--);
            wantTech(BUILDING_MARKET_WOOD_UPGRADE, e_prio--);

            wantBuilding(BUILDING_ARROWTOWER, 3, b_prio--);
        }
        else
        {
            const bool compositeDone = hasTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW);
            const bool woodDone = hasTech(BUILDING_MARKET_WOOD_UPGRADE);

            if (!compositeDone || !woodDone)
            {
                // 2
                wantBuilding(BUILDING_STABLE, 1, b_prio--);
                wantTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW, e_prio--);
                wantTech(BUILDING_MARKET_WOOD_UPGRADE, e_prio--);
            }
            else
            {
                // 3
                wantBuilding(BUILDING_RANGE, 2, b_prio--);
                wantBuilding(BUILDING_STABLE, 2, b_prio--);

                wantTech(BUILDING_STOCK_UPGRADE_USETOOL, e_prio--);
                wantTech(BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER, e_prio--);

                const bool batch1 =
                    unitCount(AT_COMPOSITE_BOWMAN) >= 8 && unitCount(AT_CAVALRY) >= 2;
                const bool batch2 =
                    unitCount(AT_COMPOSITE_BOWMAN) >= 14 && unitCount(AT_CAVALRY) >= 6;

                if (!batch1)
                {
                    wantUnit(AT_COMPOSITE_BOWMAN, 8, e_prio--);
                    wantUnit(AT_CAVALRY, 2, e_prio--);
                }
                else if (!batch2)
                {
                    wantUnit(AT_COMPOSITE_BOWMAN, 14, e_prio--);
                    wantUnit(AT_CAVALRY, 6, e_prio--);
                }
                else
                {
                    wantUnit(AT_COMPOSITE_BOWMAN, 22, e_prio--);
                    wantUnit(AT_CAVALRY, 10, e_prio--);
                }
            }
        }
    }

    econPlan();

    if (wantFarm > 0) wantBuilding(BUILDING_FARM, buildingCount(BUILDING_FARM) + wantFarm, FARM_PRIORITY);
}

void Mgr::update(const tagInfo& info)
{
    makeFrame(info);
    claimed.clear();  // 认领记录整帧有效, 只在帧首清零
    navBuild();
    threatBuild();  // 采集/建造的选址都要读它, 必须先于 arrangeGather
    arrangeGather();
    buildFrame();
    farmFrame();
    prodFrame();
    laborBuild();
    gatherReset();
    scoutFrame();

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
    runGather();

    runDestroy();

    runLure();
}