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
    return !blockCell[cellIdx(dr, ur)];
}

bool Mgr::canPlace(int dr, int ur, int size) const
{
    if (dr < 0 || ur < 0 || dr + size - 1 >= MAP_L || ur + size - 1 >= MAP_U) return false;

    const int h = cell(dr, ur).height;
    for (int i = dr; i < dr + size; i++)
        for (int j = ur; j < ur + size; j++)
            if (!valid(i, j) || cell(i, j).height != h || blockCell[cellIdx(i, j)]) return false;
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
            if (inMap(i, j)) blockCell[cellIdx(i, j)] = 1;
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
    blockCell.assign((size_t)MAP_L * MAP_U, 0);

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
        if (resourceSize(r.Type) == 1) blockCell[cellIdx(r.BlockDR, r.BlockUR)] = 1;
        else
        {
            const int a = (int)(r.DR / BLOCKSIDELENGTH + 0.5);
            const int b = (int)(r.UR / BLOCKSIDELENGTH + 0.5);
            blockCell[cellIdx(a - 1, b - 1)] = 1;
            blockCell[cellIdx(a, b - 1)] = 1;
            blockCell[cellIdx(a - 1, b)] = 1;
            blockCell[cellIdx(a, b)] = 1;
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
    nav.assign((size_t)MAP_L * MAP_U, -1);
    if (base.dr < 0) return;

    std::queue<Pos> q;
    const int baseLen = buildingSize(BUILDING_CENTER);
    for (int i = base.dr; i < base.dr + baseLen; i++)
        for (int j = base.ur; j < base.ur + baseLen; j++)
        {
            nav[cellIdx(i, j)] = 0;
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
                if (!walkable(n.dr, n.ur) || nav[cellIdx(n.dr, n.ur)] != -1) continue;
                if (dx[k] && dy[k] && (!walkable(c.dr + dx[k], c.ur) || !walkable(c.dr, c.ur + dy[k]))) continue;
                nav[cellIdx(n.dr, n.ur)] = dist;
                q.push(n);
            }
        }
    }
}

void Mgr::civilDangerBuild()
{
    for (const auto& it : eArmyMap)
    {
        const tagArmy& e = *it.second;
        const Pos now = {e.BlockDR, e.BlockUR};

        if (mobileEnemies.count(e.SN))
        {
            guardEnemies.erase(e.SN);
            continue;
        }

        auto old = guardEnemies.find(e.SN);
        const bool moved = e.NowState == HUMAN_STATE_WALKING ||
                           (old != guardEnemies.end() && (old->second.dr != now.dr || old->second.ur != now.ur));
        const bool incoming = dis(FloatPos(e.DR, e.UR), baseF) < DEF_ALERT * BLOCKSIDELENGTH;

        if (moved || incoming)
        {
            guardEnemies.erase(e.SN);
            mobileEnemies.insert(e.SN);
            continue;
        }

        // 第一次在防御圈外看到且没有移动：记成驻守。
        guardEnemies[e.SN] = now;
    }

    civilDanger.assign((size_t)MAP_L * MAP_U, 0);
    const int rr = ENEMY_KEEP * ENEMY_KEEP;
    for (const auto& it : guardEnemies)
    {
        const Pos& p = it.second;
        for (int i = max(0, p.dr - ENEMY_KEEP); i <= min(MAP_L - 1, p.dr + ENEMY_KEEP); i++)
            for (int j = max(0, p.ur - ENEMY_KEEP); j <= min(MAP_U - 1, p.ur + ENEMY_KEEP); j++)
            {
                const int dd = i - p.dr, du = j - p.ur;
                if (dd * dd + du * du <= rr) civilDanger[cellIdx(i, j)] = 1;
            }
    }
}

void Mgr::civilNavBuild()
{
    civilNav.assign((size_t)MAP_L * MAP_U, -1);
    if (base.dr < 0) return;

    std::queue<Pos> q;
    const int baseLen = buildingSize(BUILDING_CENTER);
    for (int i = base.dr; i < base.dr + baseLen; i++)
        for (int j = base.ur; j < base.ur + baseLen; j++)
        {
            civilNav[cellIdx(i, j)] = 0;
            q.push({i, j});
        }

    while (!q.empty())
    {
        const Pos cur = q.front();
        q.pop();
        const int nd = civilNav[cellIdx(cur.dr, cur.ur)] + 1;

        for (int k = 0; k < 8; k++)
        {
            const Pos n = {cur.dr + dx[k], cur.ur + dy[k]};
            if (!walkable(n.dr, n.ur) || civilDanger[cellIdx(n.dr, n.ur)]) continue;
            if (civilNav[cellIdx(n.dr, n.ur)] >= 0) continue;
            if (dx[k] && dy[k] &&
                (!walkable(cur.dr + dx[k], cur.ur) || !walkable(cur.dr, cur.ur + dy[k]) ||
                 civilDanger[cellIdx(cur.dr + dx[k], cur.ur)] || civilDanger[cellIdx(cur.dr, cur.ur + dy[k])]))
                continue;

            civilNav[cellIdx(n.dr, n.ur)] = nd;
            q.push(n);
        }
    }
}

bool Mgr::civilSafeSite(const Pos& p, int size) const
{
    // 地基本身和外围一格都不能落入驻守敌人的警戒圈。只看格点危险状态,
    // 不要求存在不穿危险区的到达路径 —— 绕路的判断交给寻路本身。
    for (int i = p.dr - 1; i <= p.dr + size; i++)
        for (int j = p.ur - 1; j <= p.ur + size; j++)
            if (inMap(i, j) && civilDanger[cellIdx(i, j)]) return false;

    return true;
}

void Mgr::ringAdd(std::vector<int>& g, const Pos& around, int size, int cost, int inner, int outer, int delta)
{
    if (outer < inner || around.dr < 0) return;
    std::vector<unsigned char> used((size_t)MAP_L * MAP_U, 0);

    std::queue<Pos> q;
    for (int i = around.dr; i < around.dr + size; i++)
        for (int j = around.ur; j < around.ur + size; j++)
        {
            if (!inMap(i, j) || used[cellIdx(i, j)]) continue;
            q.push({i, j});
            used[cellIdx(i, j)] = 1;
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
            if (r >= inner) g[cellIdx(crt.dr, crt.ur)] += val;
            for (int k = 0; k < 8; k++)
            {
                const Pos next = {crt.dr + dx[k], crt.ur + dy[k]};
                if (!inMap(next.dr, next.ur) || used[cellIdx(next.dr, next.ur)] || blocked(next.dr, next.ur)) continue;
                q.push(next);
                used[cellIdx(next.dr, next.ur)] = 1;
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
            if (val > 0) threat[cellIdx(i, j)] += val;
        }
}

void Mgr::threatBuild()
{
    threat.assign((size_t)MAP_L * MAP_U, 0);

    for (const Pos& l : lionCells) threatStamp(l.dr, l.ur, LION_KEEP);

    for (const auto& it : eArmyMap) threatStamp(it.second->BlockDR, it.second->BlockUR, ENEMY_KEEP);

    for (const auto& it : eBuildingMap)
        if (it.second->Type == BUILDING_ARROWTOWER) threatStamp(it.second->BlockDR, it.second->BlockUR, ENEMY_KEEP);
}

int Mgr::threatAt(int dr, int ur) const
{
    if (threat.empty()) return 0;
    if (!inMap(dr, ur)) return 0;
    return threat[cellIdx(dr, ur)];
}

void Mgr::sendAction(int workerSN, int targetSN)
{
    const tagFarmer* f = farmer(workerSN);
    if (!f) return;
    if (f->WorkObjectSN == targetSN && f->NowState != HUMAN_STATE_IDLE) return;
    HumanAction(workerSN, targetSN);
}

bool Mgr::civilWorkerSafe(int sn) const
{
    const tagFarmer* f = farmer(sn);
    if (!f || !inMap(f->BlockDR, f->BlockUR)) return false;
    const int idx = cellIdx(f->BlockDR, f->BlockUR);
    return civilNav[idx] >= 0 && !civilDanger[idx];
}

void Mgr::laborBuild()
{
    laborPool.clear();
    for (const auto& it : farmerMap)
        if (!workerBusy(it.first) && civilWorkerSafe(it.first)) laborPool.push_back(it.first);
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
        unbind(cur);
    }

    int farmExcess = (int)farmToWorker.size() - min(farmDesired, (int)farmList.size());
    for (int i = (int)farmList.size() - 1; i >= 0 && farmExcess > 0; i--)
    {
        auto it = farmToWorker.find(farmList[i]);
        if (it == farmToWorker.end()) continue;
        const int sn = it->second;
        unbind(it);
        farmExcess--;
    }

    // 采集点只在“当前在岗人数 > 本帧目标”时释放最差的已有岗位。
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
            if (it == workerOfSpot.end()) continue;

            const int sn = it->second;

            // 活猎物会跑, 它的 cost 每帧都在变, spots 排序跟着抖. 正在追杀的这一条
            // 绑定不能因为排名波动被撤掉, 否则击杀永远完不成
            const tagResource* r = resource(p.spots[i].sn);
            const tagFarmer* f = farmer(sn);
            if (r && r->Blood > 0 && f && f->WorkObjectSN == r->SN) continue;
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
        const double d = dis(at, FloatPos(f->DR, f->UR));
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
        if (claimed.count(sn) || workerReserved(sn) || !civilWorkerSafe(sn)) continue;
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
            const int idx = cellIdx(i, j);
            if (nav[idx] == -1 || civilNav[idx] == -1 || civilDanger[idx] || standTaken[idx]) continue;

            out = {i, j};
            return true;
        }
    return false;
}

void Mgr::arrangeGather()
{
    buildDepots();

    for (int k = 0; k < RK_COUNT; k++) pools[k].spots.clear();
    standTaken.assign((size_t)MAP_L * MAP_U, 0);

    auto meatOk = [&](const tagResource* r)
    {
        if (kindOf(r->Type) != RK_CORPSE) return false;
        return r->Blood <= 0 || r->Type == RESOURCE_GAZELLE;
    };

    std::vector<const tagResource*> meats;
    for (const auto& it : resourceMap)
        if (meatOk(it.second)) meats.push_back(it.second);

    std::unordered_set<int> grouped;
    for (int i = 0; i < meats.size(); i++)
        for (int j = i + 1; j < meats.size(); j++)
            if (dis(FloatPos(meats[i]->DR, meats[i]->UR), FloatPos(meats[j]->DR, meats[j]->UR)) <=
                CORPSE_GROUP_GAP * BLOCKSIDELENGTH)
            {
                grouped.insert(meats[i]->SN);
                grouped.insert(meats[j]->SN);
            }

    for (auto it = spotOfWorker.begin(); it != spotOfWorker.end();)
    {
        const tagResource* r = resource(it->second);
        const bool gone =
            !farmer(it->first) || !r || kindOf(r->Type) == RK_COUNT || (kindOf(r->Type) == RK_CORPSE && !meatOk(r));
        if (!gone)
        {
            it++;
            continue;
        }

        const int workerSN = it->first;
        const tagFarmer* f = farmer(workerSN);
        if (f) HumanMove(workerSN, f->DR, f->UR);
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
        standTaken[cellIdx(stand.dr, stand.ur)] = 1;

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
        const int workerSN = it->first;
        const tagFarmer* f = farmer(workerSN);
        if (f) HumanMove(workerSN, f->DR, f->UR);
        workerOfSpot.erase(it->second);
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
    if (toFree) { freeWorker(workerSN); }
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
            assigned++;
            sendAction(sn, s.sn);
        }
    }
}

void Mgr::farmFrame()
{
    farmList.clear();

    std::unordered_set<int> safe;
    std::vector<std::pair<double, int>> v;
    for (int sn : buildingsOf(BUILDING_FARM))
    {
        const tagBuilding* b = building(sn);
        if (!b || b->Percent < 100) continue;
        if (!civilSafeSite({b->BlockDR, b->BlockUR}, buildingSize(BUILDING_FARM))) continue;

        safe.insert(sn);
        const double half = buildingSize(BUILDING_FARM) * 0.5;
        const FloatPos at((b->BlockDR + half) * BLOCKSIDELENGTH, (b->BlockUR + half) * BLOCKSIDELENGTH);
        v.push_back({transportRate(BASE_RATE_FARM, depotCost(at, foodDepots)), sn});
    }

    // 驻守敌人使农田暂时不可用时，立即取消旧耕作命令。
    for (auto it = farmToWorker.begin(); it != farmToWorker.end();)
    {
        if (safe.count(it->first))
        {
            ++it;
            continue;
        }

        const int workerSN = it->second;
        const tagFarmer* f = farmer(workerSN);
        if (f) HumanMove(workerSN, f->DR, f->UR);

        auto cur = it++;
        unbind(cur);
    }

    std::sort(v.begin(), v.end(), [](const std::pair<double, int>& a, const std::pair<double, int>& b)
    { return a.first != b.first ? a.first > b.first : a.second < b.second; });

    for (const auto& p : v) farmList.push_back(p.second);
}

void Mgr::unbind(std::unordered_map<int, int>::iterator it)
{
    const int sn = it->second;
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
        assigned++;
        sendAction(sn, farmSN);
    }
}

void Mgr::econPlan(int phase)
{
    for (int k = 0; k < RK_COUNT; k++) pools[k].desired = 0;
    farmDesired = 0;
    wantFarm = 0;

    const int reserved = CREW_BUILD * (int)sites.size() + (int)fixCrew.size() + (lionWorker >= 0 ? 1 : 0);
    const int pop = max(0, (int)farmerMap.size() - reserved);
    if (pop <= 0) return;

    const int* weight = ECON_WEIGHT[phase];

    const int meatCap = (int)pools[RK_CORPSE].spots.size();
    const int bushCap = (int)pools[RK_BUSH].spots.size();
    const int farmCap = (int)farmList.size();
    const int currentCap[4] = {(int)pools[RK_WOOD].spots.size(), meatCap + bushCap + farmCap,
                               (int)pools[RK_STONE].spots.size(), (int)pools[RK_GOLD].spots.size()};
    const int planCap[4] = {currentCap[0], buildAvailable(BUILDING_FARM) ? pop : currentCap[1], currentCap[2],
                            currentCap[3]};

    int raw[4] = {};
    for (int n = 0; n < pop; n++)
    {
        int pick = -1;
        double best = -1;
        for (int r = 0; r < 4; r++)
        {
            if (raw[r] >= planCap[r] || weight[r] <= 0) continue;
            const double score = (double)weight[r] / (raw[r] + 1);
            if (score > best) best = score, pick = r;
        }
        if (pick < 0) break;
        raw[pick]++;
    }

    // 食物岗位不再机械按“猎物 > 浆果 > 农田”。
    // 稳定采集率已经含搬运距离；这里再把“从当前岗位走到新工作点”的一次迁移时间摊入短期收益。
    struct FoodSlot
    {
        int kind;  // 0 corpse, 1 bush, 2 existing farm, 3 pending farm, 4 new farm
        double score;
        bool used;
        FoodSlot(int k, double s) : kind(k), score(s), used(false) {}
    };

    auto travelSec = [&](const FloatPos& at, int boundWorker)
    {
        if (boundWorker >= 0 && farmer(boundWorker))
            return 0.0;  // 已经认领这个岗位，不再把前往途中距离重复当成“换岗成本”

        double best = -1;
        for (const auto& it : farmerMap)
        {
            if (workerReserved(it.first)) continue;
            const tagFarmer& f = *it.second;
            const double d = dis(FloatPos(f.DR, f.UR), at);
            if (best < 0 || d < best) best = d;
        }
        if (best < 0) best = dis(baseF, at);
        return best / (HUMAN_SPEED * 25.0);
    };

    auto shortRate = [&](double rate, double moveSec)
    { return rate * FOOD_TRAVEL_HORIZON / (FOOD_TRAVEL_HORIZON + moveSec); };

    auto spotScore = [&](const GatherSpot& s)
    {
        auto it = workerOfSpot.find(s.sn);
        const int worker = it == workerOfSpot.end() ? -1 : it->second;
        return shortRate(s.rate, travelSec(FloatPos(s.stand), worker));
    };

    // 让“计划选中的岗位”和 runGather 真正派到的岗位保持一致。
    // 已经有人工作的点没有迁移罚分，因此自然形成稳定性；新点只有明显更划算才会挤进前列。
    for (ResKind k : {RK_CORPSE, RK_BUSH})
        std::sort(pools[k].spots.begin(), pools[k].spots.end(), [&](const GatherSpot& a, const GatherSpot& b)
        {
            const double sa = spotScore(a), sb = spotScore(b);
            if (sa != sb) return sa > sb;
            return a.sn < b.sn;
        });

    std::unordered_map<int, double> farmScore;
    for (int farmSN : farmList)
    {
        const tagBuilding* b = building(farmSN);
        if (!b) continue;
        const double half = buildingSize(BUILDING_FARM) * 0.5;
        const FloatPos at((b->BlockDR + half) * BLOCKSIDELENGTH, (b->BlockUR + half) * BLOCKSIDELENGTH);
        auto it = farmToWorker.find(farmSN);
        const int worker = it == farmToWorker.end() ? -1 : it->second;
        const double rate = transportRate(BASE_RATE_FARM, depotCost(at, foodDepots));
        farmScore[farmSN] = shortRate(rate, travelSec(at, worker));
    }
    std::sort(farmList.begin(), farmList.end(), [&](int a, int b)
    {
        const double sa = farmScore[a], sb = farmScore[b];
        if (sa != sb) return sa > sb;
        return a < b;
    });

    std::vector<FoodSlot> food;
    food.reserve(meatCap + bushCap + farmCap + raw[1]);

    for (const GatherSpot& s : pools[RK_CORPSE].spots) food.push_back({0, spotScore(s)});
    for (const GatherSpot& s : pools[RK_BUSH].spots) food.push_back({1, spotScore(s)});
    for (int farmSN : farmList)
        if (farmScore.count(farmSN)) food.push_back({2, farmScore[farmSN]});

    const int pendingFarm =
        buildingCount(BUILDING_FARM) - buildingCount(BUILDING_FARM, true) + queuedBuild(BUILDING_FARM);
    const double newFarmScore = BASE_RATE_FARM * NEW_FARM_EFFICIENCY;

    // 在建农田先作为未来岗位参与比较；之后才是还需要新建的农田。
    for (int i = 0; i < pendingFarm; i++) food.push_back({3, newFarmScore});
    if (buildAvailable(BUILDING_FARM))
    {
        const int room = max(0, FARM_MAX - farmCap - pendingFarm);
        for (int i = 0; i < room; i++) food.push_back({4, newFarmScore});
    }

    std::sort(food.begin(), food.end(), [](const FoodSlot& a, const FoodSlot& b)
    {
        if (a.score != b.score) return a.score > b.score;
        return a.kind < b.kind;  // 同收益优先已有资源/已有农田
    });

    int futureFarmJobs = 0;
    int newFarmJobs = 0;
    int foodAssigned = 0;
    for (FoodSlot& s : food)
    {
        if (foodAssigned >= raw[1]) break;
        s.used = true;
        foodAssigned++;
        if (s.kind == 0) pools[RK_CORPSE].desired++;
        else if (s.kind == 1) pools[RK_BUSH].desired++;
        else if (s.kind == 2) farmDesired++;
        else
        {
            futureFarmJobs++;
            if (s.kind == 4) newFarmJobs++;
        }
    }

    pools[RK_WOOD].desired = min(raw[0], currentCap[0]);
    pools[RK_STONE].desired = min(raw[2], currentCap[2]);
    pools[RK_GOLD].desired = min(raw[3], currentCap[3]);

    // 仍然一次只开一块农田；已有在建农田时先等它完成。
    if (newFarmJobs > 0 && pendingFarm == 0 && farmCap < FARM_MAX) wantFarm = 1;

    // 未来农田岗位尚不能工作，先去砍木，既不闲置也能支付农田木材。
    const int woodRoom = max(0, currentCap[0] - pools[RK_WOOD].desired);
    pools[RK_WOOD].desired += min(futureFarmJobs, woodRoom);

    auto assignedNow = [&]()
    {
        int n = farmDesired;
        for (int k = 0; k < RK_COUNT; k++) n += pools[k].desired;
        return n;
    };

    // 没有 15 秒换岗锁。固定比例塞不满时，把人放到仍有实际岗位的位置；
    // 若补到食物，就继续使用同一份“收益 + 迁移成本”排序，而不是回到固定类型优先级。
    while (assignedNow() < pop)
    {
        int pick = -1;
        double best = -1;
        for (int r = 0; r < 4; r++)
        {
            int cnt = 0, cap = currentCap[r];
            if (r == 1) { cnt = pools[RK_CORPSE].desired + pools[RK_BUSH].desired + farmDesired; }
            else
            {
                const ResKind k = r == 0 ? RK_WOOD : (r == 2 ? RK_STONE : RK_GOLD);
                cnt = pools[k].desired;
            }
            if (cnt >= cap) continue;

            const double score = weight[r] > 0 ? (double)weight[r] / (cnt + 1) : 0.0;
            if (pick < 0 || score > best) best = score, pick = r;
        }
        if (pick < 0) break;

        if (pick == 1)
        {
            int slot = -1;
            for (int i = 0; i < (int)food.size(); i++)
            {
                if (food[i].used || food[i].kind >= 3) continue;
                slot = i;
                break;
            }
            if (slot < 0) break;

            food[slot].used = true;
            if (food[slot].kind == 0) pools[RK_CORPSE].desired++;
            else if (food[slot].kind == 1) pools[RK_BUSH].desired++;
            else farmDesired++;
        }
        else
        {
            pools[pick == 0 ? RK_WOOD : (pick == 2 ? RK_STONE : RK_GOLD)].desired++;
        }
    }
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
    if (costMap.empty()) costMap.assign((size_t)MAP_L * MAP_U, 0);

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
            if (canPlace(a, b, size) && nav[cellIdx(a, b)] >= 0) return true;
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
    placeOk.assign((size_t)MAP_L * MAP_U, 0);

    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
        {
            if (!canPlace(i, j, size)) continue;
            if (!civilSafeSite({i, j}, size)) continue;
            bool reach = false;
            for (int a = i; a < i + size && !reach; a++)
                for (int b = j; b < j + size && !reach; b++)
                    if (nav[cellIdx(a, b)] != -1) reach = true;
            if (!reach) continue;

            placeOk[cellIdx(i, j)] = 1;
        }
}

Pos Mgr::findSpot(int type)
{
    costMap.assign((size_t)MAP_L * MAP_U, 0);
    const int size = buildingSize(type);
    const int baseLen = buildingSize(BUILDING_CENTER);

    buildPlaceMask(size);

    // 通用距离惩罚
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (nav[cellIdx(i, j)] == -1) costMap[cellIdx(i, j)] += MAP_L + MAP_U;
            else costMap[cellIdx(i, j)] += nav[cellIdx(i, j)];

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
                costMap[cellIdx(a, b)] += PLACE_ADJACENT;
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
        case BUILDING_MARKET:
            ringAdd(costMap, base, baseLen, PLACE_ADJACENT, 0, 6);

            for (const auto& it : buildingMap)
            {
                if (it.second->Type == BUILDING_GRANARY || it.second->Type == BUILDING_STOCK)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_GRANARY),
                            PLACE_ADJACENT, 0, 5);
            }

            for (const auto& it : farmerMap)
            {
                const tagFarmer& f = *(it.second);
                if (workerToFarm.count(f.SN)) continue;

                ringAdd(costMap, {f.BlockDR, f.BlockUR}, 1, PLACE_BONUS, 0, 6, 10);
            }
            break;

        default: break;  // 谷仓/仓库走通用布局 + 下方智能运输收益，不在 switch 中另设固定环
    }

    const int area = size * size;
    const int W = MAP_U + 1;
    psum.assign((size_t)(MAP_L + 1) * W, 0);
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            psum[(i + 1) * W + (j + 1)] =
                psum[i * W + (j + 1)] + psum[(i + 1) * W + j] - psum[i * W + j] + costMap[cellIdx(i, j)];

    Pos best = {-1, -1};
    long long bestCost = 0;
    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
        {
            if (!placeOk[cellIdx(i, j)]) continue;

            const long long block = psum[(i + size) * W + (j + size)] - psum[i * W + (j + size)] -
                                    psum[(i + size) * W + j] + psum[i * W + j];
            long long v = block / area;

            if (type == BUILDING_GRANARY || type == BUILDING_STOCK)
            {
                const double gain = depotBenefit(type, {i, j});
                const bool hasDemand = type == BUILDING_GRANARY ? !granaryPendings.empty() : !stockPendings.empty();
                if (hasDemand && gain <= EPS) continue;
                v -= (long long)(gain * 40);  // 粗估节约一格带来40帧优势
            }

            auto fit = failedSpots.find(placeFailKey(type, i, j));
            if (fit != failedSpots.end()) v += (long long)PLACE_FAILED * fit->second;

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

        if (!civilSafeSite(s.site, buildingSize(s.type)) && !s.workers.empty())
        {
            const std::set<int> crew = s.workers;
            s.workers.clear();
            for (int sn : crew)
            {
                const tagFarmer* f = farmer(sn);
                if (f) HumanMove(sn, f->DR, f->UR);
                freeWorker(sn);
            }
        }

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
                    fail++;
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
        const bool safe = civilSafeSite(s.site, buildingSize(s.type));
        if (!safe)
        {
            // 危险只是暂态，不算选址失败；立即取消旧施工命令，地基保留等待恢复。
            const std::set<int> crew = s.workers;
            s.workers.clear();
            for (int sn : crew)
            {
                const tagFarmer* f = farmer(sn);
                if (f) HumanMove(sn, f->DR, f->UR);
                freeWorker(sn);
            }
            continue;
        }

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

void Mgr::floodThreat(const Pos& from, bool avoidThreat)
{
    scoutDist.assign((size_t)MAP_L * MAP_U, -1);
    scoutPrev.assign((size_t)MAP_L * MAP_U, -1);
    if (!inMap(from.dr, from.ur)) return;

    std::queue<Pos> q;
    scoutDist[cellIdx(from.dr, from.ur)] = 0;
    q.push(from);

    while (q.size())
    {
        const Pos cur = q.front();
        q.pop();
        for (int d = 0; d < 8; d++)
        {
            const Pos n = {cur.dr + dx[d], cur.ur + dy[d]};
            if (!walkable(n.dr, n.ur)) continue;
            if (scoutDist[cellIdx(n.dr, n.ur)] >= 0) continue;  // used
            if (avoidThreat && threatAt(n.dr, n.ur) > 0) continue;
            if (dx[d] && dy[d] && (!walkable(cur.dr + dx[d], cur.ur) || !walkable(cur.dr, cur.ur + dy[d])))
                continue;  // 对角
            scoutDist[cellIdx(n.dr, n.ur)] = scoutDist[cellIdx(cur.dr, cur.ur)] + 1;
            scoutPrev[cellIdx(n.dr, n.ur)] = cellIdx(cur.dr, cur.ur);
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
            if (scoutDist[cellIdx(i, j)] < 0 || !walkable(i, j)) continue;
            const double d = dis({i, j}, p);
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

            const int step = scoutDist[cellIdx(st.dr, st.ur)];
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
            const double d = dis(tp, base);
            if (d > bestFar)
            {
                bestFar = d;
                anchor = tp;
            }
        }
        if (anchor.dr == -1) anchor = base;
        lastAnchorChanged = gameFrame;
    }

    if (nearestStand(anchor, 4, home)) return scoutDist[cellIdx(home.dr, home.ur)] * 25;
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
    if (goal.dr < 0 || scoutDist[cellIdx(goal.dr, goal.ur)] < 0) return;

    for (Pos p = goal;;)
    {
        route.push_back(p);
        const int prev = scoutPrev[cellIdx(p.dr, p.ur)];
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
    moveToCell(priest, route[end]);
    return true;
}

Pos Mgr::fleeGoal() const
{
    Pos best = {-1, -1};
    int bestThreat = 0, bestStep = 0;
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
        {
            const int step = scoutDist[cellIdx(i, j)];
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
    const tagArmy* unit = army(priest);

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
    if (scoutDist.empty())
    {
        scoutDist.assign((size_t)MAP_L * MAP_U, -1);
        scoutPrev.assign((size_t)MAP_L * MAP_U, -1);
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
        DebugText("scout");
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
        const bool ok = scoutDist[cellIdx(goalStand.dr, goalStand.ur)] >= 0 && wpGain(goalStand) >= SCOUT_MIN_GAIN;
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
        if (!t || t->Percent < 100) continue;

        const Pos here = {t->BlockDR, t->BlockUR};
        auto nearest = [&](const std::vector<int>& cand, bool unlockedOnly)
        {
            int pick = -1;
            double best = 0;
            for (int e : cand)
            {
                const tagArmy* enemy = enemyArmy(e);
                if (!enemy || (unlockedOnly && lockOf(e) >= 0)) continue;

                const double d = dis(here, Pos(enemy->BlockDR, enemy->BlockUR));
                if (pick < 0 || d < best || (d == best && e < pick))
                {
                    best = d;
                    pick = e;
                }
            }
            return pick;
        };

        // 优先点还没锁住诱饵的敌人；没有时再处理最近的来袭敌人。
        int pick = nearest(towerAtk, true);
        if (pick < 0) pick = nearest(hostiles, false);
        if (pick < 0) continue;

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
        const double da = dis(Pos(A->BlockDR, A->BlockUR), me);
        const double db = dis(Pos(B->BlockDR, B->BlockUR), me);
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
    if (assaultOn) return;

    for (const auto& it : armyMap)
    {
        const tagArmy& u = *it.second;
        if (u.SN == priest) continue;
        if (inVanguard(u.SN)) continue;  // 提前批次已在进攻路上, 不召回防守

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

    for (auto it = moveGoal.begin(); it != moveGoal.end();)
        if (army(it->first)) ++it;
        else it = moveGoal.erase(it);

    // 只打敌方老家一侧的敌军. 冲向我方基地的来袭波次交给留家的守军,
    // 否则提前批一见到波次就会掉头当防守用, inVanguard 那道过滤等于白设.
    for (const auto& it : eArmyMap)
    {
        const tagArmy& e = *it.second;
        if (corner.dr >= 0 && dis(corner, Pos(e.BlockDR, e.BlockUR)) > BELONG_CORNER) continue;
        tars.push_back(e.SN);
    }

    for (const auto& it : eBuildingMap)
        if (it.second->Type != BUILDING_SIEGE) tars.push_back(it.second->SN);
}

// 尚未探明的格子按可走处理, 否则总攻初期方向场到不了敌方
bool Mgr::marchable(int dr, int ur) const
{
    if (!inMap(dr, ur)) return false;
    if (cell(dr, ur).type == MAPPATTERN_UNKNOWN) return true;
    return walkable(dr, ur);
}

void Mgr::atkFieldBuild()
{
    atkField.assign((size_t)MAP_L * MAP_U, -1);

    const Pos src = siegePos.dr >= 0 ? siegePos : corner;
    if (src.dr < 0 || !inMap(src.dr, src.ur)) return;

    std::queue<Pos> q;
    atkField[cellIdx(src.dr, src.ur)] = 0;
    q.push(src);

    while (!q.empty())
    {
        const Pos c = q.front();
        q.pop();
        const int nd = atkField[cellIdx(c.dr, c.ur)] + 1;

        for (int k = 0; k < 8; k++)
        {
            const Pos n = {c.dr + dx[k], c.ur + dy[k]};
            if (!marchable(n.dr, n.ur) || atkField[cellIdx(n.dr, n.ur)] != -1) continue;
            if (dx[k] && dy[k] && (!marchable(c.dr + dx[k], c.ur) || !marchable(c.dr, c.ur + dy[k]))) continue;
            atkField[cellIdx(n.dr, n.ur)] = nd;
            q.push(n);
        }
    }
}

int Mgr::selector(const tagArmy& u)
{
    if (tars.empty()) return -1;

    const Pos here = {u.BlockDR, u.BlockUR};

    auto targetPos = [&](int sn, Pos& out)
    {
        const tagArmy* a = enemyArmy(sn);
        if (a)
        {
            out = {a->BlockDR, a->BlockUR};
            return true;
        }

        const tagBuilding* b = enemyBuilding(sn);
        if (b && b->Type != BUILDING_SIEGE)
        {
            out = {b->BlockDR, b->BlockUR};
            return true;
        }
        return false;
    };

    auto allowed = [&](int sn)
    {
        if (std::find(tars.begin(), tars.end(), sn) == tars.end()) return false;
        Pos p;
        return targetPos(sn, p);
    };

    auto nearest = [&](bool army)
    {
        int pick = -1;
        double best = 0;
        for (int sn : tars)
        {
            const tagArmy* a = enemyArmy(sn);
            const tagBuilding* b = enemyBuilding(sn);
            if (army ? !a : (a || !b || b->Type == BUILDING_SIEGE)) continue;

            const Pos p = a ? Pos(a->BlockDR, a->BlockUR) : Pos(b->BlockDR, b->BlockUR);
            const double d = dis(here, p);
            if (pick < 0 || d < best || (d == best && sn < pick))
            {
                pick = sn;
                best = d;
            }
        }
        return std::pair<int, double>(pick, best);
    };

    // 新目标严格先敌军后建筑；已有有效目标尽量保持。
    const auto enemy = nearest(true);
    if (allowed(u.WorkObjectSN))
    {
        if (enemy.first < 0 || enemy.first == u.WorkObjectSN) return u.WorkObjectSN;

        // 当前打的是建筑, 而视野里出现了敌军: 转火.
        // 建筑不还手也不会跑, 晚拆没有代价; 敌军放着不打则是一直在挨打.
        if (!enemyArmy(u.WorkObjectSN)) return enemy.first;

        // 当前打的已经是敌军就一直打完, 不因为出现更近的敌军而改令.
        return u.WorkObjectSN;
    }

    if (enemy.first >= 0) return enemy.first;
    return nearest(false).first;
}

// 复合弓碰撞箱边长约 0.3 格, 四个角子位间距 0.5 格, 因此同格四人互不重叠.
FloatPos Mgr::slotAt(int slot)
{
    static const double off[5][2] = {{0.25, 0.25}, {0.25, 0.75}, {0.75, 0.25}, {0.75, 0.75}, {0.5, 0.5}};
    const Pos c = cellPos(slot / 5);
    const int k = slot % 5;
    return FloatPos((c.dr + off[k][0]) * BLOCKSIDELENGTH, (c.ur + off[k][1]) * BLOCKSIDELENGTH);
}

// 只看 tars 里的敌军, 与索敌用同一批人.
// 若把来袭波次也算进来, 后撤方向(朝家)与波次前进方向一致, 距离拉不开,
// 结果是被波次一路推回家且全程不开火. 无视它反而接触更短.
double Mgr::enemyGap(const FloatPos& at) const
{
    double best = 1e9;
    for (int sn : tars)
    {
        const tagArmy* e = enemyArmy(sn);
        if (!e) continue;
        best = std::min(best, dis(at, FloatPos(e->DR, e->UR)) / BLOCKSIDELENGTH);
    }
    return best;
}

void Mgr::sendTo(const tagArmy& u, int slot, bool back)
{
    slotClaim(u, slot);

    const FloatPos at = slotAt(slot);
    Wait w;
    w.slot = slot;
    w.gap = dis(FloatPos(u.DR, u.UR), at) / BLOCKSIDELENGTH;
    w.idle = 0;
    w.back = back;
    moveGoal[u.SN] = w;

    HumanMove(u.SN, at.dr, at.ur);
}

int Mgr::slotOf(const tagArmy& u) const
{
    const Pos here = {u.BlockDR, u.BlockUR};
    if (!inMap(here.dr, here.ur)) return -1;
    if (u.Sort == AT_STONE_THROWER) return slotIdx(here.dr, here.ur, 4);

    int best = -1;
    double bestGap = 0;
    for (int k = 0; k < 4; k++)
    {
        const int slot = slotIdx(here.dr, here.ur, k);
        const double d = dis(FloatPos(u.DR, u.UR), slotAt(slot));
        if (best < 0 || d < bestGap)
        {
            best = slot;
            bestGap = d;
        }
    }
    return best;
}

// 投石车体积超过一整格, 一个子位表达不了, 所以按"所在格连带周围一圈"处理.
// slotClaim 与 slotFree 必须用同一个范围, 否则会放进去又互相重叠.
bool Mgr::slotFree(int slot, const tagArmy& u) const
{
    if (slot < 0) return false;
    if (slotBlack[slot] > gameFrame) return false;

    if (u.Sort != AT_STONE_THROWER) return slotOwner[slot] < 0 || slotOwner[slot] == u.SN;

    const Pos c = cellPos(slot / 5);
    for (int i = c.dr - 1; i <= c.dr + 1; i++)
        for (int j = c.ur - 1; j <= c.ur + 1; j++)
        {
            if (!inMap(i, j)) continue;
            for (int k = 0; k < 5; k++)
            {
                const int owner = slotOwner[slotIdx(i, j, k)];
                if (owner >= 0 && owner != u.SN) return false;
            }
        }
    return true;
}

void Mgr::slotClaim(const tagArmy& u, int slot)
{
    if (slot < 0) return;

    if (u.Sort != AT_STONE_THROWER)
    {
        if (slotOwner[slot] < 0) slotOwner[slot] = u.SN;
        return;
    }

    const Pos c = cellPos(slot / 5);
    for (int i = c.dr - 1; i <= c.dr + 1; i++)
        for (int j = c.ur - 1; j <= c.ur + 1; j++)
        {
            if (!inMap(i, j)) continue;
            for (int k = 0; k < 5; k++)
                if (slotOwner[slotIdx(i, j, k)] < 0) slotOwner[slotIdx(i, j, k)] = u.SN;
        }
}

// retreat 为真时沿 nav 朝家退一格, 否则沿 atkField 向敌方推进一格.
int Mgr::pickSlot(const tagArmy& u, bool retreat)
{
    const Pos here = {u.BlockDR, u.BlockUR};
    if (!inMap(here.dr, here.ur)) return -1;

    const int hereField = atkField[cellIdx(here.dr, here.ur)];
    const int hereNav = nav[cellIdx(here.dr, here.ur)];

    // 投石车只用格心, 复合弓用四个角
    const int lo = u.Sort == AT_STONE_THROWER ? 4 : 0;
    const int hi = u.Sort == AT_STONE_THROWER ? 5 : 4;

    int best = -1;
    int bestNav = 0;      // 后撤: nav 越小越好, 一律朝家退
    int bestField = 0;    // 推进: atkField, 越小越好
    double bestMove = 0;  // 两者共用: 少走路

    for (int d = 0; d < 9; d++)
    {
        const Pos n = d == 8 ? here : Pos(here.dr + dx[d], here.ur + dy[d]);

        // 后撤必须落在确认可走的格子上; 推进允许踩未探明区域, 走不通会被卡死兜底换掉.
        if (retreat ? !walkable(n.dr, n.ur) : !marchable(n.dr, n.ur)) continue;
        if (d < 8 && dx[d] && dy[d])
        {
            const bool okA = retreat ? walkable(here.dr + dx[d], here.ur) : marchable(here.dr + dx[d], here.ur);
            const bool okB = retreat ? walkable(here.dr, here.ur + dy[d]) : marchable(here.dr, here.ur + dy[d]);
            if (!okA || !okB) continue;
        }

        const int field = atkField[cellIdx(n.dr, n.ur)];
        const int homeNav = nav[cellIdx(n.dr, n.ur)];
        if (retreat)
        {
            // 一律沿 nav 朝家退. 必须真的更靠家, 顺带排掉同格换子位这种无意义抖动.
            if (homeNav < 0) continue;
            if (hereNav >= 0 && homeNav >= hereNav) continue;
        }
        else
        {
            if (field < 0 || (d < 8 && hereField >= 0 && field >= hereField)) continue;
            if (d == 8) continue;  // 推进必须真的换格
        }

        for (int k = lo; k < hi; k++)
        {
            const int slot = slotIdx(n.dr, n.ur, k);
            if (!slotFree(slot, u)) continue;

            const FloatPos at = slotAt(slot);
            const double move = dis(FloatPos(u.DR, u.UR), at) / BLOCKSIDELENGTH;

            if (retreat)
            {
                const bool take = best < 0 || homeNav < bestNav || (homeNav == bestNav && move < bestMove);
                if (!take) continue;
                bestNav = homeNav;
            }
            else
            {
                const bool take = best < 0 || field < bestField || (field == bestField && move < bestMove);
                if (!take) continue;
                bestField = field;
            }

            best = slot;
            bestMove = move;
        }
    }
    return best;
}

// 大部队出动前, 基地附近留 HOME_KEEP 个复合弓守家, 多出来的立刻并入提前批次,
// 不必凑够数量. 进攻逻辑与大部队完全一致, 只是人少.
// assaultOn 之后清空: 这些人自动并入大部队, 不再单独成军.
void Mgr::vanguardPick()
{
    if (assaultOn)
    {
        vanguard.clear();
        return;
    }

    for (auto it = vanguard.begin(); it != vanguard.end();)
    {
        const tagArmy* u = army(*it);
        if (u && u->Sort == AT_COMPOSITE_BOWMAN) ++it;
        else it = vanguard.erase(it);
    }

    // 守家池: 还没编入提前批, 且确实在基地附近的复合弓.
    // 追敌追远了的那些不算在内, 留给防守逻辑, 回来了自然重新参与筛选.
    std::vector<int> home;
    for (const auto& it : armyMap)
    {
        const tagArmy& u = *it.second;
        if (u.Sort != AT_COMPOSITE_BOWMAN || inVanguard(u.SN)) continue;
        if (dis(FloatPos(u.DR, u.UR), baseF) > HOME_RANGE * BLOCKSIDELENGTH) continue;
        home.push_back(u.SN);
    }

    // 按 SN 升序, 保证每帧挑出的守家名单一致
    std::sort(home.begin(), home.end());
    for (size_t i = HOME_KEEP; i < home.size(); i++) vanguard.insert(home[i]);
}

void Mgr::runAssult()
{
    std::vector<const tagArmy*> units;
    for (const auto& it : armyMap)
    {
        const tagArmy* u = it.second;
        if (u->Sort != AT_COMPOSITE_BOWMAN && u->Sort != AT_STONE_THROWER) continue;
        if (!inMap(u->BlockDR, u->BlockUR)) continue;
        if (!assaultOn && !inVanguard(u->SN)) continue;  // 大部队还没出动, 只指挥提前批次
        units.push_back(u);
    }
    if (units.empty()) return;

    slotOwner.assign((size_t)MAP_L * MAP_U * 5, -1);
    if (slotBlack.size() != slotOwner.size()) slotBlack.assign(slotOwner.size(), 0);

    // 先登记真实位置, 再叠上仍在执行的移动目标, 否则别人会把在途目标当空位.
    // 投石车的占位会连带封住周围一圈, 所以放在复合弓之后, 让弓兵先认领自己脚下的子位.
    for (const tagArmy* u : units)
        if (u->Sort != AT_STONE_THROWER) slotClaim(*u, slotOf(*u));
    for (const tagArmy* u : units)
        if (u->Sort == AT_STONE_THROWER) slotClaim(*u, slotOf(*u));
    for (const auto& it : eArmyMap) slotClaim(*it.second, slotOf(*it.second));
    for (const tagArmy* u : units)
    {
        auto w = moveGoal.find(u->SN);
        if (w != moveGoal.end()) slotClaim(*u, w->second.slot);
    }

    // 离敌人越近越先决策: 后撤时最危险的先挑空位, 推进时最靠前的先带路.
    std::sort(units.begin(), units.end(), [&](const tagArmy* a, const tagArmy* b)
    {
        const double ga = enemyGap(FloatPos(a->DR, a->UR)), gb = enemyGap(FloatPos(b->DR, b->UR));
        if (ga != gb) return ga < gb;
        int fa = atkField[cellIdx(a->BlockDR, a->BlockUR)];
        int fb = atkField[cellIdx(b->BlockDR, b->BlockUR)];
        if (fa < 0) fa = 1 << 30;
        if (fb < 0) fb = 1 << 30;
        if (fa != fb) return fa < fb;
        return a->SN < b->SN;
    });

    for (const tagArmy* up : units)
    {
        const tagArmy& u = *up;
        const double gap = enemyGap(FloatPos(u.DR, u.UR));
        const double danger = u.Sort == AT_STONE_THROWER ? RETREAT_STONE : RETREAT_BOW;
        const int tar = selector(u);

        // 一, 正在执行移动令. 后撤令必须走完; 推进令一旦有敌人或有目标就立刻放弃, 免得白走不输出.
        auto w = moveGoal.find(u.SN);
        if (w != moveGoal.end())
        {
            const bool giveUp = !w->second.back && (gap < danger || tar >= 0);
            const double now = dis(FloatPos(u.DR, u.UR), slotAt(w->second.slot)) / BLOCKSIDELENGTH;

            if (giveUp || now <= MOVE_DONE) moveGoal.erase(w);
            else if (now < w->second.gap - MOVE_GAIN)  // 还在靠近, 让它走
            {
                w->second.gap = now;
                w->second.idle = 0;
                continue;
            }
            else if (++w->second.idle < MOVE_STUCK) continue;
            else
            {
                // 走不动的子位短期拉黑, 本帧立刻另作决策, 绝不让单位永久停在 idle.
                slotBlack[w->second.slot] = gameFrame + SLOT_BLACK;
                moveGoal.erase(w);
            }
        }

        // 二, 敌人贴太近就退一步. 复合弓射程 8 而阈值 4, 退完仍在射程内可继续输出.
        if (gap < danger)
        {
            const int slot = pickSlot(u, true);
            if (slot >= 0)
            {
                sendTo(u, slot, true);
                continue;
            }
            // 退无可退就继续打, 至少不空站着.
        }

        // 三, 有目标就交给 HumanAction, 引擎会自动走进射程并射击.
        if (tar >= 0)
        {
            if (u.WorkObjectSN != tar || u.NowState == HUMAN_STATE_IDLE) HumanAction(u.SN, tar);
            continue;
        }

        // 四, 看不见目标就沿方向场向敌方推进一格.
        const int slot = pickSlot(u, false);
        if (slot >= 0) sendTo(u, slot, false);  // 被围住时本帧当预备队, 下一帧重试
    }
}

void Mgr::runAtkPriest()
{
    if (!assaultOn) return;
    const tagArmy* p = army(priest);
    if (!p) return;

    const Pos here = {p->BlockDR, p->BlockUR};

    if (siegeSN >= 0 && eArmyMap.size() <= 2 && eBuildingMap.size() <= 3)
    {
        if (p->WorkObjectSN != siegeSN) HumanAction(p->SN, siegeSN);
        return;
    }

    int threshold = siegeSN < 0 ? 45 : 30;
    if (siegeDis({p->BlockDR, p->BlockUR}) < threshold)  // 尽快撤离
    {
        Pos best = {-1, -1};
        int bestDis = 0;
        for (int i = 0; i < MAP_L; i++)
            for (int j = 0; j < MAP_U; j++)
            {
                if (!walkable(i, j) || nav[cellIdx(i, j)] < 0) continue;
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
                if (!walkable(i, j) || nav[cellIdx(i, j)] < 0) continue;
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
    offenseUpdate();

    if (!assaultOn && gameFrame >= ASSAULT_FRAME) assaultOn = true;
    vanguardPick();
    if (!assaultOn && vanguard.empty()) return;

    atkFieldBuild();
    runAssult();
    if (assaultOn) runAtkPriest();  // 祭司跟大部队走, 不跟提前批次
}

void Mgr::clearRoad()
{
    std::vector<unsigned char> used((size_t)MAP_L * MAP_U, 0);
    std::vector<Pos> points;
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (nav[cellIdx(i, j)] >= 22 && nav[cellIdx(i, j)] <= 26)
            {
                used[cellIdx(i, j)] = 1;
                points.push_back({i, j});
            }

    if (points.empty()) return;

    for (const auto& a : armyMap)
    {
        const tagArmy* u = a.second;
        if (inVanguard(u->SN)) continue;  // 提前批次不参与集结
        if (used[cellIdx(u->BlockDR, u->BlockUR)] || u->NowState != HUMAN_STATE_IDLE) continue;

        const int ran = rand() % points.size();
        moveToCell(u->SN, points[ran]);
    }
}

void Mgr::killLions()
{
    if (lionWorker >= 0 && !farmer(lionWorker)) lionWorker = -1;
    if (lionTarget >= 0)
    {
        const tagResource* cur = resource(lionTarget);
        if (!cur || cur->Type != RESOURCE_LION || cur->Blood <= 0) lionTarget = -1;
    }

    auto lionDis = [&](const tagResource* l) { return dis(FloatPos(l->DR, l->UR), baseF) / BLOCKSIDELENGTH; };

    const tagResource* tar = lionTarget >= 0 ? resource(lionTarget) : nullptr;

    // 基地 40 格内立即处理；正在清远处狮子时，近处目标可以抢占。
    if (!tar || lionDis(tar) > 40.0)
    {
        const tagResource* around = nullptr;
        double best = 0;
        for (const tagResource* l : lionSet)
        {
            const double d = lionDis(l);
            if (d > 40.0) continue;
            if (!around || d < best || (d == best && l->SN < around->SN))
            {
                around = l;
                best = d;
            }
        }
        if (around) tar = around;
    }

    // 40 格外从 20 分钟开始逐个清理；已有目标保持到击杀。
    if (!tar && gameFrame >= LION_HUNT_FROM)
    {
        double best = 0;
        for (const tagResource* l : lionSet)
        {
            const double d = lionDis(l);
            if (!tar || d < best || (d == best && l->SN < tar->SN))
            {
                tar = l;
                best = d;
            }
        }
    }

    if (!tar)
    {
        lionTarget = -1;
        if (lionWorker >= 0)
        {
            const int sn = lionWorker;
            lionWorker = -1;
            freeWorker(sn);
        }
        return;
    }

    lionTarget = tar->SN;
    if (lionWorker < 0) lionWorker = takeNearest({tar->DR, tar->UR}, true);
    if (lionWorker >= 0) sendAction(lionWorker, tar->SN);
}

bool Mgr::workerBusy(int sn) const
{
    if (spotOfWorker.count(sn)) return true;
    return workerReserved(sn);
}

bool Mgr::workerReserved(int sn) const
{
    if (workerToFarm.count(sn) || sn == lionWorker || fixCrew.count(sn)) return true;
    for (const BuildSite& s : sites)
        if (s.workers.count(sn)) return true;
    return false;
}

void Mgr::workerDrop(int sn)
{
    dropSpot(sn, false);
    for (BuildSite& s : sites) s.workers.erase(sn);
    if (lionWorker == sn) lionWorker = -1;
    fixCrew.erase(sn);

    auto it = workerToFarm.find(sn);  // 兜底: 正常路径由 workerReserved 挡住
    if (it != workerToFarm.end())
    {
        farmToWorker.erase(it->second);
        workerToFarm.erase(it);
    }
}

int Mgr::farmerTarget() const { return std::max(10, std::min(20, 50 - (int)armyMap.size() - 2)); }

void Mgr::strategy()
{
    int b_prio = 100;
    int e_prio = 100;
    int phase = 0;

    const int homeCnt = min(12, (int)(farmerMap.size() + armyMap.size()) / 4 + 1);
    wantBuilding(BUILDING_HOME, homeCnt, b_prio--);

    wantStock(b_prio--);
    wantGranary(b_prio--);
    wantUnit(AT_FARMER, farmerTarget(), e_prio--);

    if (stage == CIVILIZATION_TOOLAGE)
    {
        phase = 0;
        wantBuilding(BUILDING_ARMYCAMP, 1, b_prio--);
        wantBuilding(BUILDING_RANGE, 1, b_prio--);
        wantBuilding(BUILDING_MARKET, 1, b_prio--);
        wantTech(BUILDING_CENTER_UPGRADE, e_prio--);
    }
    else
    {
        if (!hasTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW) || buildingCount(BUILDING_RANGE) <= 4) phase = 1;
        else phase = 2;

        wantBuilding(BUILDING_RANGE, 4, b_prio--);

        wantTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW, e_prio--);
        wantUnit(AT_COMPOSITE_BOWMAN, 40, e_prio--);
    }

    econPlan(phase);

    if (wantFarm > 0) wantBuilding(BUILDING_FARM, buildingCount(BUILDING_FARM) + wantFarm, FARM_PRIORITY);
}

void Mgr::update(const tagInfo& info)
{
    makeFrame(info);
    claimed.clear();  // 认领记录整帧有效, 只在帧首清零
    navBuild();
    civilDangerBuild();
    civilNavBuild();
    arrangeGather();
    buildFrame();
    farmFrame();
    prodFrame();
    laborBuild();
    gatherReset();

    threatBuild();  // threatAt 只服务探图避险

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
}