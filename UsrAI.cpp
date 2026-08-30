#include "UsrAI.h"

#include <cstdlib>
#include <iostream>
#include <list>
#include <set>
#include <unordered_map>

using namespace std;
tagGame tagUsrGame;
ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/

static const int dx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
static const int dy[8] = {1, 0, -1, 0, 1, -1, -1, 1};

Mgr mgr;

void UsrAI::processData() { mgr.update(getInfo()); }

// 支持结构体

void PrefixSum2D::build(const Grid<int>& src)
{
    W = MAP_U + 1;
    s.assign((size_t)(MAP_L + 1) * W, 0);
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            s[(i + 1) * W + (j + 1)] =
                s[i * W + (j + 1)] + s[(i + 1) * W + j] - s[i * W + j] + src(i, j);
}

long long PrefixSum2D::rect(int dr, int ur, int size) const
{
    return s[(dr + size) * W + (ur + size)] - s[dr * W + (ur + size)] - s[(dr + size) * W + ur] +
           s[dr * W + ur];
}

// 世界信息

const std::vector<int>& World::buildingsOf(int type) const
{
    static const std::vector<int> kEmpty;
    auto it = byType.find(type);
    return it == byType.end() ? kEmpty : it->second;
}

int World::buildingSize(int type)
{
    if (type == BUILDING_HOME || type == BUILDING_ARROWTOWER) return 2;
    return 3;
}

int World::resourceSize(int type)
{
    if (type == RESOURCE_STONE || type == RESOURCE_GOLD || type == RESOURCE_FISH) return 2;
    return 1;
}

int World::buildWoodCost(int type)
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

int World::buildStoneCost(int type)
{
    if (type == BUILDING_ARROWTOWER) return BUILD_ARROWTOWER_STONE;
    return 0;
}

ResKind World::kindOf(int resourceType)
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

int World::atkRange(int sort)
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

double World::dpsOf(const tagArmy& e) // 兔头
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

Stock World::actionCost(int action)
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

int World::actionHost(int action)
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

int World::typeToAction(int type)
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

bool World::valid(int dr, int ur) const
{
    if (!Grid<int>::inside(dr, ur)) return false;
    const tagTerrain& t = cell(dr, ur);
    return t.height != -1 && (t.type == MAPPATTERN_DESERT || t.type == MAPPATTERN_GRASS);
}

bool World::walkable(int dr, int ur) const
{
    if (!Grid<int>::inside(dr, ur)) return false;
    const int t = cell(dr, ur).type;
    if (t != MAPPATTERN_GRASS && t != MAPPATTERN_DESERT) return false;
    return !blockCell(dr, ur);
}

bool World::canPlace(int dr, int ur, int size) const
{
    if (dr < 0 || ur < 0 || dr + size - 1 >= MAP_L || ur + size - 1 >= MAP_U) return false;

    const int h = cell(dr, ur).height;
    for (int i = dr; i < dr + size; i++)
        for (int j = ur; j < ur + size; j++)
            if (!valid(i, j) || cell(i, j).height != h || blockCell(i, j)) return false;
    return true;
}

bool World::nearLion(int dr, int ur, int radius) const
{
    for (const Pos& l : lionCells)
        if (std::abs(l.dr - dr) <= radius && std::abs(l.ur - ur) <= radius) return true;
    return false;
}

bool World::enemyCorner(int dr, int ur) const
{
    const bool sameD = (dr < MAP_L / 2) == (base.dr < MAP_L / 2);
    const bool sameU = (ur < MAP_U / 2) == (base.ur < MAP_U / 2);
    return !sameD && !sameU;
}

int World::lockOf(int enemySN) const
{
    const tagArmy* e = enemyArmy(enemySN);
    if (!e) return -1;

    const int sn = e->WorkObjectSN;
    if (sn == priest) return -1;  // 锁着祭司不算有诱饵
    if (allySet.count(sn)) return sn;

    const tagBuilding* b = building(sn);
    return b && b->Type == BUILDING_ARROWTOWER ? sn : -1;
}

void World::markFootprint(const tagBuilding& b)
{
    const int s = buildingSize(b.Type);
    for (int i = b.BlockDR; i < b.BlockDR + s; i++)
        for (int j = b.BlockUR; j < b.BlockUR + s; j++)
            if (Grid<int>::inside(i, j)) blockCell(i, j) = 1;
}

void World::rebuild(const tagInfo& info)
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

// 导航

void NavGrid::rebuild()
{
    d.reset(-1);
    if (!scratch.ready()) scratch.reset(0);
    if (w.basePos().dr < 0) return;

    std::queue<Pos> q;
    const Pos base = w.basePos();
    const int baseLen = World::buildingSize(BUILDING_CENTER);
    for (int i = base.dr; i < base.dr + baseLen; i++)
        for (int j = base.ur; j < base.ur + baseLen; j++)
        {
            d(i, j) = 0;
            q.push({i, j});
        }

    for (int dist = 1; q.size(); dist++)
    {
        int u = q.size();
        for (int t = 0; t < u; t++)
        {
            const Pos c = q.front();
            q.pop();
            for (int k = 0; k < 8; k++)
            {
                const Pos n = {c.dr + dx[k], c.ur + dy[k]};
                if (!w.walkable(n.dr, n.ur) || d(n) != -1) continue;
                if (dx[k] && dy[k] && (!w.walkable(c.dr + dx[k], c.ur) || !w.walkable(c.dr, c.ur + dy[k])))
                    continue;
                d(n) = dist;
                q.push(n);
            }
        }
    }
}

void NavGrid::bfs(Grid<int>& map, const std::vector<Pos>& seeds, const Wave& wv)
{
    if (wv.outer < wv.inner || seeds.empty()) return;
    if (!scratch.ready()) scratch.reset(0);
    scratch.fill(0);

    std::queue<Pos> q;
    for (const Pos& s : seeds)
    {
        if (!Grid<int>::inside(s.dr, s.ur)) continue;
        if (scratch(s)) continue;
        q.push(s);
        scratch(s) = 1;
    }

    for (int r = 0; q.size(); r++)
    {
        if (r > wv.outer) return;
        const int cost = wv.at(r);
        int u = q.size();
        for (int i = 0; i < u; i++)
        {
            const Pos crt = q.front();
            q.pop();
            if (r >= wv.inner) map(crt) += cost;
            for (int k = 0; k < 8; k++)
            {
                const Pos next = {crt.dr + dx[k], crt.ur + dy[k]};
                if (!Grid<int>::inside(next.dr, next.ur) || scratch(next) || w.blocked(next.dr, next.ur))
                    continue;
                q.push(next);
                scratch(next) = 1;
            }
        }
    }
}

void NavGrid::bfs(Grid<int>& map, const Pos& around, int size, const Wave& wv)
{
    if (around.dr < 0) return;
    std::vector<Pos> seeds;
    seeds.reserve(size * size);
    for (int i = around.dr; i < around.dr + size; i++)
        for (int j = around.ur; j < around.ur + size; j++) seeds.push_back({i, j});
    bfs(map, seeds, wv);
}

// 威胁场

void ThreatField::stamp(int td, int tu, int r)
{
    if (r < 0 || r >= (int)(sizeof(tbl) / sizeof(tbl[0]))) return;
    const int s = 2 * r + 1;
    if (tbl[r].empty())
    {
        tbl[r].assign(s * s, 0);
        for (int a = -r; a <= r; a++)
            for (int b = -r; b <= r; b++)
            {
                const double dd = std::sqrt((double)(a * a + b * b));
                if (dd <= r + EPS) tbl[r][(a + r) * s + (b + r)] = int(r - dd + 1);
            }
    }

    const std::vector<int>& t = tbl[r];
    const int lo_dr = std::max(0, td - r), hi_dr = std::min(MAP_L - 1, td + r);
    const int lo_ur = std::max(0, tu - r), hi_ur = std::min(MAP_U - 1, tu + r);
    for (int i = lo_dr; i <= hi_dr; i++)
        for (int j = lo_ur; j <= hi_ur; j++)
        {
            const int val = t[(i - td + r) * s + (j - tu + r)];
            if (val > 0) map(i, j) += val;
        }
}

void ThreatField::rebuild()
{
    map.reset(0);

    for (const Pos& l : w.lionsPos()) stamp(l.dr, l.ur, tune::LION_KEEP);

    for (const auto& it : w.enemyArmies())
    {
        const tagArmy& e = *it.second;
        const int r = w.lockOf(e.SN) >= 0 ? World::atkRange(e.Sort) + tune::PRIEST_MARGIN : tune::ENEMY_KEEP;
        stamp(e.BlockDR, e.BlockUR, r);
    }

    for (const auto& it : w.enemyBuildings())
        if (it.second->Type == BUILDING_ARROWTOWER)
            stamp(it.second->BlockDR, it.second->BlockUR, tune::ENEMY_KEEP);
}

int ThreatField::at(int dr, int ur) const
{
    if (!map.ready()) return 0;
    if (!Grid<int>::inside(dr, ur)) return 0;
    return map(dr, ur);
}

// 阶段

void GamePhase::update()
{
    const int f = w.frame();
    maxStock = tune::STOCK_MAX_BASE + f / 25 / 60 / 10;
    army = f > tune::ARMY_ALL_IN;
    priest = f > tune::PRIEST_ALL_IN;
}

// 命令

void Orders::action(int sn, int targetSN) { owner->emitAction(sn, targetSN); }

void Orders::move(int sn, double dr, double ur) { owner->emitMove(sn, dr, ur); }

void Orders::moveToCell(int sn, const Pos& p)
{
    owner->emitMove(sn, (0.5 + p.dr) * BLOCKSIDELENGTH, (0.5 + p.ur) * BLOCKSIDELENGTH);
}

void Orders::build(int workerSN, int type, int dr, int ur) { owner->emitBuild(workerSN, type, dr, ur); }

void Orders::buildingAction(int hostSN, int act) { owner->emitBuildingAction(hostSN, act); }

void Orders::workerTask(int workerSN, int targetSN)
{
    const tagFarmer* f = w.farmer(workerSN);
    if (!f) return;
    if (f->WorkObjectSN == targetSN && f->NowState != HUMAN_STATE_IDLE) return;
    owner->emitAction(workerSN, targetSN);
}

// 人力资源

void Labor::rebuild()
{
    pool.clear();
    claimedThisFrame.clear();

    for (const auto& it : w.farmers())
    {
        const int sn = it.first;
        bool taken = false;
        for (Dispatch* s : systems)
            if (s->ownsWorker(sn))
            {
                taken = true;
                break;
            }
        if (!taken) pool.push_back(sn);
    }
}

int Labor::nearestOf(const std::vector<int>& cand, const FloatPos& at) const
{
    int best = -1;
    double bestDis = 0;
    for (int sn : cand)
    {
        const tagFarmer* f = w.farmer(sn);
        if (!f) continue;
        const double d = geo::disSq(at, FloatPos(f->DR, f->UR));
        if (best < 0 || d < bestDis)
        {
            bestDis = d;
            best = sn;
        }
    }
    return best;
}

int Labor::claim(const FloatPos& at, Steal steal)
{
    std::vector<int> cand;
    cand.reserve(pool.size());
    for (int sn : pool)
        if (!claimedThisFrame.count(sn)) cand.push_back(sn);

    int best = nearestOf(cand, at);
    if (best >= 0)
    {
        auto it = std::find(pool.begin(), pool.end(), best);
        if (it != pool.end())
        {
            *it = pool.back();
            pool.pop_back();
        }
        claimedThisFrame.insert(best);
        return best;
    }
    if (steal == Steal::No) return -1;

    cand.clear();
    for (const auto& it : w.farmers())
    {
        const int sn = it.first;
        if (claimedThisFrame.count(sn)) continue;

        bool pinned = false;
        for (Dispatch* s : systems)
            if (s->pinsWorker(sn))
            {
                pinned = true;
                break;
            }
        if (pinned) continue;

        cand.push_back(sn);
    }

    best = nearestOf(cand, at);
    if (best < 0) return -1;

    // 解绑该村民在所有持久集合里的登记, 防止 sn 同时属于多个岗位
    for (Dispatch* s : systems) s->detachWorker(best);
    claimedThisFrame.insert(best);
    return best;
}

void Labor::release(int sn)
{
    if (w.farmer(sn)) pool.push_back(sn);
}

bool Labor::isPinned(int sn) const
{
    for (const Dispatch* s : systems)
        if (s->pinsWorker(sn)) return true;
    return false;
}

// 收集

void GatherSystem::buildDepots()
{
    foodDepots.clear();
    resDepots.clear();
    for (const auto& it : c.world.buildings())
    {
        const tagBuilding& b = *it.second;
        if (b.Percent < 100) continue;

        const double half = World::buildingSize(b.Type) * 0.5;
        const FloatPos at((b.BlockDR + half) * BLOCKSIDELENGTH, (b.BlockUR + half) * BLOCKSIDELENGTH);
        if (b.Type == BUILDING_CENTER) foodDepots.push_back(at), resDepots.push_back(at);
        else if (b.Type == BUILDING_GRANARY) foodDepots.push_back(at);
        else if (b.Type == BUILDING_STOCK) resDepots.push_back(at);
    }
}

double GatherSystem::depotCost(const FloatPos& at, const std::vector<FloatPos>& depots) const
{
    double best = -1;
    for (const FloatPos& d : depots)
    {
        const double v = geo::dis(at, d);
        if (best < 0 || v < best) best = v;
    }
    if (best < 0) return geo::dis(at, c.world.baseAt());
    return best;
}

bool GatherSystem::standCell(const tagResource* r, Pos& out) const
{
    const int size = World::resourceSize(r->Type);
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
            if (!Grid<int>::inside(i, j)) continue;
            if (c.nav.dist(i, j) == -1 || claimed(i, j)) continue;

            out = {i, j};
            return true;
        }
    return false;
}

void GatherSystem::onFrame()
{
    buildDepots();

    for (int k = 0; k < RK_COUNT; k++) pools[k].spots.clear();
    claimed.reset(0);

    // 解绑
    for (auto it = spotOfWorker.begin(); it != spotOfWorker.end();)
    {
        const tagResource* r = c.world.resource(it->second);
        const bool gone = !c.world.farmer(it->first) || !r || World::kindOf(r->Type) == RK_COUNT ||
                          (World::kindOf(r->Type) == RK_CORPSE && r->Blood > 0);
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
    cand.reserve(c.world.resources().size());

    for (const auto& it : c.world.resources())
    {
        const tagResource* r = it.second;
        const ResKind k = World::kindOf(r->Type);
        if (k == RK_COUNT) continue;
        if (k == RK_CORPSE)
        {
            if (r->Blood > 0) continue;
            if (!workerOfSpot.count(r->SN) && c.world.nearLion(r->BlockDR, r->BlockUR, tune::LION_KEEP)) continue;
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
        claimed(stand) = 1;

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

void GatherSystem::resetDesired()
{
    for (int k = 0; k < RK_COUNT; k++) pools[k].desired = 0;
}

void GatherSystem::dropSpot(int workerSN, bool toFree)
{
    auto it = spotOfWorker.find(workerSN);
    if (it == spotOfWorker.end()) return;
    workerOfSpot.erase(it->second);
    spotOfWorker.erase(it);
    if (toFree) c.labor.release(workerSN);
}

int GatherSystem::poolRoom(ResKind k) const { return (int)pools[k].spots.size() - pools[k].desired; }

int GatherSystem::spotsWithin(ResKind k, double limit) const
{
    int cnt = 0;
    for (const GatherSpot& s : pools[k].spots)
        if (s.cost <= limit) cnt++;
        else break;
    return cnt;
}

void GatherSystem::toPool(int& from, int num, ResKind k, double limit)
{
    int room = poolRoom(k);
    if (limit >= 0) room = min(room, spotsWithin(k, limit) - pools[k].desired);
    const int t = min(max(0, min(from, num)), max(0, room));
    from -= t;
    pools[k].desired += t;
}

void GatherSystem::run()
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
                const int sn = c.labor.claim(FloatPos(s.stand), Steal::No);
                if (sn < 0) continue;
                workerOfSpot[s.sn] = sn;
                spotOfWorker[sn] = s.sn;
            }
            c.orders.workerTask(workerOfSpot[s.sn], s.sn);
        }
    }
}

// 打猎

bool HuntSystem::huntable(const tagResource* r) const
{
    return r->Type == RESOURCE_GAZELLE && r->Blood > 0 &&
           !c.world.nearLion(r->BlockDR, r->BlockUR, tune::LION_KEEP);
}

HuntSite* HuntSystem::huntOf(int siteID)
{
    auto it = huntByID.find(siteID);
    return it == huntByID.end() ? nullptr : it->second;
}

void HuntSystem::formHunts(const std::vector<const tagResource*>& sor, int threshold)
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
                    if (geo::disSq(FloatPos(sor[j]->DR, sor[j]->UR), cp) > radiusSq) continue;

                    used[j] = true;
                    site.members.push_back(sor[j]->SN);
                    sumDR += sor[j]->DR;
                    sumUR += sor[j]->UR;
                    q.push_back(j);
                }
            }

            const int n = site.members.size();
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

        hunts.erase(remove_if(hunts.begin(), hunts.end(),
                              [&](const HuntSite& s) { return s.members.size() <= 2; }),
                    hunts.end());

        const FloatPos base = c.world.baseAt();
        std::sort(hunts.begin(), hunts.end(), [&](const HuntSite& a, const HuntSite& b)
        { return geo::disSq(base, a.center) < geo::disSq(base, b.center); });
    }

    huntByID.clear();
    huntOfSN.clear();
    for (HuntSite& s : hunts)
    {
        huntByID[s.id] = &s;
        for (int sn : s.members) huntOfSN[sn] = s.id;
    }
}

void HuntSystem::restoreStaff()
{
    for (auto it = staffHunt.begin(); it != staffHunt.end();)
    {
        const int sn = it->first;
        HuntSite* s = c.world.farmer(sn) ? huntOf(it->second) : nullptr;
        if (!s)
        {
            it = staffHunt.erase(it);
            continue;
        }
        s->staff.push_back(sn);
        it++;
    }
}

void HuntSystem::onFrame()
{
    std::vector<const tagResource*> prey;  // 聚类输入
    for (const auto& it : c.world.resources())
        if (huntable(it.second)) prey.push_back(it.second);

    formHunts(prey, 6);
    restoreStaff();

    // 过期数据
    for (auto it = huntPrey.begin(); it != huntPrey.end();)
    {
        const tagResource* r = c.world.resource(it->second);
        if (r && r->Blood > 0) it++;
        else it = huntPrey.erase(it);
    }

    for (auto it = huntPrey.begin(); it != huntPrey.end();)
        if (huntByID.count(it->first)) it++;
        else it = huntPrey.erase(it);
}

void HuntSystem::resetDesired()
{
    for (HuntSite& s : hunts) s.desired = 0;
}

void HuntSystem::detachWorker(int sn)
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

void HuntSystem::assign(int x, HuntSite& site)
{
    while (x-- > 0)
    {
        const int sn = c.labor.claim(site.center, Steal::No);
        if (sn < 0) return;
        site.staff.push_back(sn);
        staffHunt[sn] = site.id;
    }
}

void HuntSystem::release(int x, HuntSite& site)
{
    while (x > 0 && site.staff.size())
    {
        const int sn = site.staff.back();
        site.staff.pop_back();
        staffHunt.erase(sn);
        c.labor.release(sn);
        x--;
    }
}

void HuntSystem::toHunt(int& from, int num, int siteIdx)
{
    if (siteIdx < 0 || siteIdx >= (int)hunts.size()) return;
    const int t = max(0, min(from, num));
    from -= t;
    hunts[siteIdx].desired += t;
}

void HuntSystem::run()
{
    for (HuntSite& s : hunts)
        if ((int)s.staff.size() > s.desired) release((int)s.staff.size() - s.desired, s);

    for (HuntSite& s : hunts)
    {
        if ((int)s.staff.size() < s.desired) assign(s.desired - (int)s.staff.size(), s);
        if (s.staff.size()) driveHunt(s);
    }
}

void HuntSystem::driveHunt(HuntSite& site)
{
    const tagResource* prey = nullptr;

    auto cur = huntPrey.find(site.id);
    if (cur != huntPrey.end())
    {
        const tagResource* r = c.world.resource(cur->second);
        if (r && huntable(r)) prey = r;
    }

    if (!prey)  // 挑最近的一只
    {
        double bestDis = 0;
        for (int sn : site.members)
        {
            const tagResource* r = c.world.resource(sn);
            if (!r || !huntable(r)) continue;

            const double d = geo::disSq(c.world.baseAt(), FloatPos(r->DR, r->UR));
            if (!prey || d < bestDis)
            {
                bestDis = d;
                prey = r;
            }
        }
        if (!prey)  // 打光, 交给采集系统
        {
            huntPrey.erase(site.id);
            release((int)site.staff.size(), site);
            return;
        }
        huntPrey[site.id] = prey->SN;
    }

    for (int sn : site.staff) c.orders.workerTask(sn, prey->SN);
}

// 农场

void FarmSystem::onFrame()
{
    farmList.clear();
    for (int sn : c.world.buildingsOf(BUILDING_FARM))
    {
        const tagBuilding* b = c.world.building(sn);
        if (b && b->Percent >= 100) farmList.push_back(sn);
    }
}

void FarmSystem::unbind(std::unordered_map<int, int>::iterator it)
{
    workerToFarm.erase(it->second);
    c.labor.release(it->second);
    farmToWorker.erase(it);
}

void FarmSystem::run()
{
    std::unordered_set<int> live(farmList.begin(), farmList.end());

    for (auto it = farmToWorker.begin(); it != farmToWorker.end();)
    {
        auto cur = it++;
        if (!live.count(cur->first) || !c.world.farmer(cur->second)) unbind(cur);
    }

    for (int farmSN : farmList)
    {
        auto it = farmToWorker.find(farmSN);
        if (it == farmToWorker.end())
        {
            const tagBuilding* farm = c.world.building(farmSN);
            const Pos p(farm->BlockDR, farm->BlockUR);
            const int sn = c.labor.claim(FloatPos(p), Steal::No);
            if (sn < 0) continue;

            workerToFarm[sn] = farmSN;
            it = farmToWorker.emplace(farmSN, sn).first;
        }

        c.orders.workerTask(it->second, farmSN);
    }
}

// 建造

bool BuildSystem::buildAvailable(int type) const
{
    switch (type)
    {
        case BUILDING_FARM: return c.world.buildingCount(BUILDING_MARKET, true) > 0;
        default: return true;
    }
}

bool BuildSystem::ownsWorker(int sn) const
{
    for (const BuildSite& s : sites)
        if (s.workers.count(sn)) return true;
    return false;
}

void BuildSystem::detachWorker(int sn)
{
    for (BuildSite& s : sites) s.workers.erase(sn);
}

void BuildSystem::onFrame()
{
    builds.clear();
    if (!costMap.ready()) costMap.reset(0);

    if (savePlanned < 0 || c.world.frame() - savePlanned >= tune::STOCK_PLAN_GAP)
    {
        buildSaveMap();
        savePlanned = c.world.frame();
    }
}

void BuildSystem::buildPlaceMask(int type, int size)
{
    placeOk.reset(0);

    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
        {
            if (!c.world.canPlace(i, j, size)) continue;

            bool reach = false;
            for (int a = i; a < i + size && !reach; a++)
                for (int b = j; b < j + size && !reach; b++)
                    if (c.nav.dist(a, b) != -1) reach = true;
            if (!reach) continue;

            placeOk(i, j) = 1;
        }
}

Pos BuildSystem::findSpot(int type)
{
    costMap.reset(0);
    const int size = World::buildingSize(type);
    const int baseLen = World::buildingSize(BUILDING_CENTER);
    const Pos basePos = c.world.basePos();

    buildPlaceMask(type, size);

    // 通用距离惩罚
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (c.nav.dist(i, j) == -1) costMap(i, j) += MAP_L + MAP_U;
            else costMap(i, j) += c.nav.dist(i, j);

    // 靠近基地额外惩罚
    c.nav.bfs(costMap, basePos, baseLen, Wave(0).upto(2).grad(tune::PLACE_NEAR_BASE));

    // 通用靠近建筑惩罚
    for (const auto& it : c.world.buildings())
    {
        const int len = World::buildingSize(it.second->Type);
        c.nav.bfs(costMap, {it.second->BlockDR, it.second->BlockUR}, len,
                  Wave(0).upto(1).grad(tune::PLACE_ADJACENT));
    }

    // 通用靠近资源惩罚
    for (const auto& it : c.world.resources())
    {
        const tagResource* r = it.second;
        if (r->Type == RESOURCE_GAZELLE && r->Blood > 0) continue;

        const int len = World::resourceSize(r->Type);
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
                if (!Grid<int>::inside(a, b)) continue;
                if (a >= dr && a < dr + len && b >= ur && b < ur + len) continue;
                costMap(a, b) += tune::PLACE_NEAR_RES;
            }
    }

    // 根据建筑类型特化
    switch (type)
    {
        case BUILDING_FARM:  // 靠近谷仓, 基地排布
            for (const auto& it : c.world.buildings())
            {
                if (it.second->Type == BUILDING_GRANARY)
                    c.nav.bfs(costMap, {it.second->BlockDR, it.second->BlockUR},
                              World::buildingSize(BUILDING_GRANARY), Wave(tune::PLACE_BAND_BONUS).band(2, 5));

                if (it.second->Type == BUILDING_CENTER)
                    c.nav.bfs(costMap, {it.second->BlockDR, it.second->BlockUR},
                              World::buildingSize(BUILDING_CENTER), Wave(tune::PLACE_BAND_BONUS).band(2, 5));
            }
            break;

        case BUILDING_GRANARY:  // 和基地以及其他谷仓保持6-8格左右的距离, 靠近浆果丛
            c.nav.bfs(costMap, basePos, baseLen, Wave(tune::PLACE_BAND_BONUS).band(6, 8));
            c.nav.bfs(costMap, basePos, baseLen, Wave(tune::PLACE_BAND_FAIL).upto(5));
            for (const auto& it : c.world.buildings())
                if (it.second->Type == BUILDING_GRANARY)
                {
                    const Pos p = {it.second->BlockDR, it.second->BlockUR};
                    const int len = World::buildingSize(BUILDING_GRANARY);
                    c.nav.bfs(costMap, p, len, Wave(tune::PLACE_BAND_BONUS).band(6, 8));
                    c.nav.bfs(costMap, p, len, Wave(tune::PLACE_BAND_FAIL).upto(5));
                }
            for (const GatherSpot& s : gather.pool(RK_BUSH).spots)
                c.nav.bfs(costMap, {(int)(s.at.dr / BLOCKSIDELENGTH), (int)(s.at.ur / BLOCKSIDELENGTH)}, 1,
                          Wave(tune::PLACE_RES_BONUS).upto(4));
            break;

        case BUILDING_ARMYCAMP:
        case BUILDING_COLLAGE:
        case BUILDING_RANGE:
            c.nav.bfs(costMap, basePos, baseLen, Wave(tune::PLACE_BAND_FAIL).upto(6));

            for (const auto& it : c.world.buildings())
                if (it.second->Type == BUILDING_GRANARY)
                    c.nav.bfs(costMap, {it.second->BlockDR, it.second->BlockUR},
                              World::buildingSize(BUILDING_GRANARY), Wave(tune::PLACE_BAND_FAIL).upto(5));

            for (const auto& it : c.world.farmers())
            {
                const tagFarmer& f = *(it.second);
                if (c.labor.isPinned(f.SN)) continue;

                c.nav.bfs(costMap, {f.BlockDR, f.BlockUR}, 1, Wave(tune::PLACE_BAND_BONUS).upto(4));
            }
            break;

        case BUILDING_HOME:  // 紧靠其他房屋建造(形成大矩形占地), 和基地保持5格的距离
            for (const auto& it : c.world.buildings())
                if (it.second->Type == BUILDING_HOME)
                    c.nav.bfs(costMap, {it.second->BlockDR, it.second->BlockUR},
                              World::buildingSize(BUILDING_HOME), Wave(-tune::PLACE_ADJACENT * 3).band(1, 1));
            c.nav.bfs(costMap, basePos, baseLen, Wave(tune::PLACE_BAND_FAIL).upto(5));
            break;

        case BUILDING_STOCK:
            c.nav.bfs(costMap, basePos, World::buildingSize(BUILDING_CENTER),
                      Wave(tune::PLACE_IMPOSSIBLE).band(60, 1000));
            break;

        case BUILDING_ARROWTOWER:
            for (const auto& it : c.world.buildings())
                if (it.second->Type == BUILDING_ARROWTOWER)
                    c.nav.bfs(costMap, {it.second->BlockDR, it.second->BlockUR},
                              World::buildingSize(BUILDING_ARROWTOWER), Wave(tune::PLACE_ADJACENT * 3).upto(9));
            break;
    }

    // 每格代价先摊平成均值
    const int area = size * size;
    sum.build(costMap);

    Pos best = {-1, -1};
    long long bestCost = 0;
    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
        {
            if (!placeOk(i, j)) continue;

            long long v = sum.rect(i, j, size) / area;

            const int at = Grid<int>::index(i, j);
            auto fit = failedSpots.find(at);
            if (fit != failedSpots.end()) v += (long long)tune::PLACE_FAILED * fit->second;

            if (type == BUILDING_STOCK) v -= saveMap(i, j) / tune::PLACE_SAVE_SCALE;  // 智能建造

            if (best.dr < 0 || v < bestCost)
            {
                bestCost = v;
                best = {i, j};
            }
        }
    return best;
}

void BuildSystem::buildSaveMap()
{
    saveMap.reset(0);
    bestSave = 0;

    const int size = World::buildingSize(BUILDING_STOCK);
    const double off = (size - 1) * 0.5;  // 左上角到占地中心

    static const ResKind kServed[] = {RK_WOOD, RK_CORPSE}; // 尚未加入黄金, 如果加入要手动同步调整开销倍率和折算值

    for (const ResKind k : kServed)
    {
        const int carry = (k == RK_GOLD) ? FARMER_CARRYLIMIT_GOLD : FARMER_CARRYLIMIT_WOOD;
        const int discount = (k == RK_CORPSE) ? 1 : tune::STOCK_DISCOUNT_OTHER;

        struct Serve
        {
            double worth;
            int trips;
            const GatherSpot* s;
        };
        std::vector<Serve> pick;
        pick.reserve(gather.pool(k).spots.size());
        for (const GatherSpot& s : gather.pool(k).spots)
        {
            const tagResource* r = c.world.resource(s.sn);
            if (!r) continue;
            const int trips = (r->Cnt + carry - 1) / carry;
            pick.push_back({trips * s.cost, trips, &s});
        }
        std::sort(pick.begin(), pick.end(), [](const Serve& a, const Serve& b)
        { return a.worth != b.worth ? a.worth > b.worth : a.s->sn < b.s->sn; });
        if ((int)pick.size() > tune::STOCK_SERVE) pick.resize(tune::STOCK_SERVE);

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
                    const double gain = s.cost - geo::dis(s.at, ctr);
                    if (gain <= 0) continue;
                    saveMap(i, j) += (int)(trips * 2 * gain / HUMAN_SPEED / discount);
                }
        }
    }

    buildPlaceMask(BUILDING_STOCK, size);
    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
            if (placeOk(i, j) && saveMap(i, j) > bestSave) bestSave = saveMap(i, j);
}

void BuildSystem::wantStock(int priority)
{
    const int have = c.world.buildingCount(BUILDING_STOCK);
    if (have >= c.phase.stockMax()) return;

    for (int sn : c.world.buildingsOf(BUILDING_STOCK))
        if (c.world.building(sn)->Percent < 100) return;

    const double cost = 100 / FARMER_CONSTRUCTSPEED + BUILD_STOCK_WOOD / FARMER_GATHERSPEED_WOOD;

    if (bestSave < cost * tune::STOCK_PAYBACK) return;
    wantBuilding(BUILDING_STOCK, have + 1, priority);
}

int BuildSystem::queuedBuild(int type) const
{
    int cnt = 0;
    for (const BuildSite& s : sites)
        if (s.type == type && s.sn < 0) cnt++;
    for (const auto& b : builds)
        if (b.second == type) cnt++;
    return cnt;
}

void BuildSystem::wantBuilding(int buildingType, int total, int priority)
{
    if (!buildAvailable(buildingType)) return;
    int diff = total - c.world.buildingCount(buildingType) - queuedBuild(buildingType);
    for (; diff > 0; diff--) builds.insert({priority, buildingType});
}

Stock BuildSystem::demand() const
{
    Stock need;
    for (const auto& p : builds)
    {
        need.wood += World::buildWoodCost(p.second);
        need.stone += World::buildStoneCost(p.second);
    }
    return need;
}

void BuildSystem::run()
{
    for (auto it = sites.begin(); it != sites.end();)
    {
        BuildSite& s = *it;
        for (auto wit = s.workers.begin(); wit != s.workers.end();)
            if (c.world.farmer(*wit)) ++wit;
            else wit = s.workers.erase(wit);

        if (s.sn < 0)
        {
            for (int sn : c.world.buildingsOf(s.type))
            {
                const tagBuilding* b = c.world.building(sn);
                if (b->BlockDR != s.site.dr || b->BlockUR != s.site.ur) continue;
                s.sn = sn;
                break;
            }
            if (s.sn < 0)
            {
                if (!s.workers.empty()) failedSpots[Grid<int>::index(s.site.dr, s.site.ur)]++;
                for (int sn : s.workers) c.labor.release(sn);
                it = sites.erase(it);
                continue;
            }
        }

        const tagBuilding* b = c.world.building(s.sn);
        if (!b || b->Percent >= 100)
        {
            for (int sn : s.workers) c.labor.release(sn);
            it = sites.erase(it);
            continue;
        }
        ++it;
    }

    for (BuildSite& s : sites)
    {
        while ((int)s.workers.size() < tune::BUILD_CREW)
        {
            const int sn = c.labor.claim(FloatPos(s.site), Steal::Allow);
            if (sn < 0) break;
            s.workers.insert(sn);
        }
        for (int sn : s.workers) c.orders.workerTask(sn, s.sn);
    }

    Stock used;
    std::vector<Pos> justPlaced;
    for (auto it = builds.end(); it != builds.begin();)
    {
        --it;
        const int type = it->second;

        Stock probe = used;
        probe.wood += World::buildWoodCost(type);
        probe.stone += World::buildStoneCost(type);

        const Stock avail = c.world.available();
        if (avail.wood < probe.wood) break;
        if (avail.stone < probe.stone) break;

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

        const int first = c.labor.claim(FloatPos(spot), Steal::Allow);
        if (first < 0) break;

        BuildSite s;
        s.type = type;
        s.site = spot;
        s.workers.insert(first);
        sites.push_back(s);
        justPlaced.push_back(spot);
        used = probe;
        it = builds.erase(it);

        c.orders.build(first, type, spot.dr, spot.ur);
    }
}

// 生产

void ProductionSystem::onFrame()
{
    prods.clear();
    destroyCnt = -1;
}

bool ProductionSystem::techAvailable(int action) const
{
    switch (action)
    {
        case BUILDING_CENTER_UPGRADE:
            return c.world.civStage() == CIVILIZATION_TOOLAGE &&
                   c.world.buildingCount(BUILDING_MARKET, true) > 0;
        case BUILDING_MARKET_WHEEL_UPGRADE:
            return c.world.buildingCount(BUILDING_MARKET) > 0 &&
                   c.world.civStage() == CIVILIZATION_BRONZEAGE;
        default: return true;
    }
}

int ProductionSystem::idleHost(int buildingType, const std::set<int>& busy) const
{
    for (int sn : c.world.buildingsOf(buildingType))
    {
        const tagBuilding* b = c.world.building(sn);
        if (b->Percent >= 100 && b->Project == 0 && !busy.count(sn)) return sn;
    }
    return -1;
}

int ProductionSystem::queuedProd(int action) const
{
    const int host = World::actionHost(action);
    int cnt = 0;
    for (const auto& p : prods)
        if (p.second == action) cnt++;
    for (int sn : c.world.buildingsOf(host))
        if (c.world.building(sn)->Project == action) cnt++;
    return cnt;
}

void ProductionSystem::wantUnit(int type, int total, int priority)
{
    const int action = World::typeToAction(type);

    int diff = total - c.world.unitCount(type) - queuedProd(action);
    for (; diff > 0; diff--) prods.insert({priority, action});
    if (diff < 0) destroyCnt = -diff;
}

void ProductionSystem::wantTech(int action, int priority)
{
    if (techAvailable(action) && !doneTech.count(action)) prods.insert({priority, action});
}

Stock ProductionSystem::demand() const
{
    Stock need;
    for (const auto& p : prods) need += World::actionCost(p.second);
    return need;
}

void ProductionSystem::run()
{
    std::set<int> busy;  // 本帧已经派活的建筑
    std::set<int> full;  // 这类建筑本帧已经找不出空的了
    for (auto it = prods.end(); it != prods.begin();)
    {
        it--;
        const int action = it->second;
        const int hostType = World::actionHost(action);
        if (full.count(hostType)) continue;

        const int host = idleHost(hostType, busy);
        if (host < 0)
        {
            full.insert(hostType);
            continue;
        }

        const Stock cost = World::actionCost(action);
        if (!c.world.afford(cost)) continue;

        c.orders.buildingAction(host, action);
        doneTech.insert(action);
        busy.insert(host);
        c.world.reserve(cost);
        it = prods.erase(it);
    }
}

void ProductionSystem::runDestroy()
{
    if (destroyCnt <= 0) return;

    int left = destroyCnt;
    for (const auto& it : c.world.farmers())
    {
        if (left-- <= 0) break;
        c.orders.action(it.first, it.first);
    }
}

// 经济规划

void EconomySystem::rateOf(double out[2]) const
{
    out[0] = (c.world.stock().meat - prevStock.meat) / (double)dt;
    out[1] = (c.world.stock().wood - prevStock.wood) / (double)dt;
}

void EconomySystem::rebalance(int population, const Stock& need)
{
    const Stock& stock = c.world.stock();
    const int gameFrame = c.world.frame();

    const int stoneWant = (phaseChanged && stock.stone < 600) ? 2 : 0;
    pop.stone = min(stoneWant, population);
    const int balancePop = max(0, population - pop.stone);

    if (!phaseChanged && c.world.civStage() == CIVILIZATION_TOOLAGE)
    {
        pop.food = balancePop * 3 / 5;
        pop.wood = balancePop - pop.food;
        return;
    }
    else if (!phaseChanged && c.world.civStage() == CIVILIZATION_BRONZEAGE)
    {
        phaseChanged = true;
        lastSnapFrame = gameFrame;  // 进入新阶段, 从下次 60s 才平衡
        prevStock = stock;
    }

    const int sumFW = pop.food + pop.wood;
    if (sumFW < balancePop) pop.wood += balancePop - sumFW;  // 默认进树木
    else if (sumFW > balancePop)
    {
        int excess = sumFW - balancePop;
        while (excess > 0)
        {
            if (pop.food >= pop.wood && pop.food > 0)
            {
                pop.food--;
                excess--;
            }
            else if (pop.wood > 0)
            {
                pop.wood--;
                excess--;
            }
            else break;
        }
    }

    if (!phaseChanged || gameFrame - lastSnapFrame < dt || balancePop == 0) return;

    double rate[2];
    rateOf(rate);

    // 每人每分钟多少资源
    const double per0 = pop.food > 0 ? rate[0] / pop.food : 0.0;
    const double per1 = pop.wood > 0 ? rate[1] / pop.wood : 0.0;

    // 缺口
    const double gap0 = max(0.0, (double)need.meat - stock.meat);
    const double gap1 = max(0.0, (double)need.wood - stock.wood);

    lastSnapFrame = gameFrame;

    // 缺口补全(任意比例不超过4 : 1)
    if (per0 < EPS || per1 < EPS || (gap0 < EPS && gap1 < EPS))
    {
        pop.food = balancePop / 2;
        pop.wood = balancePop - pop.food;
    }
    else if (gap0 < EPS)
    {
        pop.food = balancePop / 5;
        pop.wood = balancePop - pop.food;
    }
    else if (gap1 < EPS)
    {
        pop.wood = balancePop / 5;
        pop.food = balancePop - pop.wood;
    }
    else
    {
        double ratio = gap0 * per1 / gap1 / per0;
        ratio = min(ratio, 4.0);
        ratio = max(ratio, 0.25);

        pop.food = (int)(ratio / (1.0 + ratio) * balancePop);
        pop.wood = balancePop - pop.food;
    }

    lastSnapFrame = gameFrame;
    prevStock = stock;
}

// 侦察

void ScoutSystem::onFrame()
{
    if (c.world.army(scoutSN)) return;

    scoutSN = c.world.priestSN();
    for (int sn : c.world.armyOrder())
    {
        const tagArmy* a = c.world.army(sn);
        if (a && a->Sort == AT_SCOUT)
        {
            scoutSN = sn;
            break;
        }
    }
}

void ScoutSystem::floodThreat(const Pos& from, bool avoidThreat)
{
    scoutDist.reset(-1);
    scoutPrev.reset(-1);
    if (!Grid<int>::inside(from.dr, from.ur)) return;

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
            if (!c.world.walkable(n.dr, n.ur)) continue;
            if (scoutDist(n) >= 0) continue;  // used
            if (avoidThreat && c.threat.at(n.dr, n.ur) > 0) continue;
            if (dx[d] && dy[d] &&
                (!c.world.walkable(cur.dr + dx[d], cur.ur) || !c.world.walkable(cur.dr, cur.ur + dy[d])))
                continue;  // 对角
            scoutDist(n) = scoutDist(cur) + 1;
            scoutPrev(n) = Grid<int>::index(cur.dr, cur.ur);
            q.push(n);
        }
    }
}

void ScoutSystem::buildUnknown()
{
    // unknownRow[i * (MAP_U + 1) + j] = 第 i 行前 j 格里的未知格数
    const int W = MAP_U + 1;
    unknownRow.assign((size_t)MAP_L * W, 0);
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            unknownRow[i * W + j + 1] =
                unknownRow[i * W + j] + (c.world.cell(i, j).type == MAPPATTERN_UNKNOWN);
}

int ScoutSystem::wpGain(const Pos& p) const
{
    const int W = MAP_U + 1;
    const int r = tune::SCOUT_VIEW;
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

bool ScoutSystem::nearestStand(const Pos& p, int r, Pos& out) const
{
    double best = 0;
    out = {-1, -1};
    for (int i = max(0, p.dr - r); i <= min(MAP_L - 1, p.dr + r); i++)
        for (int j = max(0, p.ur - r); j <= min(MAP_U - 1, p.ur + r); j++)
        {
            if (scoutDist(i, j) < 0 || !c.world.walkable(i, j)) continue;
            const double d = geo::disSq({i, j}, p);
            if (out.dr >= 0 && d >= best) continue;
            best = d;
            out = {i, j};
        }
    return out.dr >= 0;
}

int ScoutSystem::pickWaypoint(Pos& stand) const
{
    const int wp = MAP_L / tune::SCOUT_VIEW + 1;
    int bestIdx = -1, bestStep = 0;
    stand = {-1, -1};

    for (int i = 0; i < wp; i++)
        for (int j = 0; j < wp; j++)
        {
            const int idx = i * wp + j;
            if (wpDone[idx] || c.world.frame() < wpCooldown[idx]) continue;

            const Pos w(min(i * tune::SCOUT_VIEW, MAP_L - 1), min(j * tune::SCOUT_VIEW, MAP_U - 1));
            if (c.world.enemyCorner(w.dr, w.ur)) continue;

            Pos st;
            if (!nearestStand(w, 5, st)) continue;  // 这一帧到不了

            if (wpGain(st) < tune::SCOUT_MIN_GAIN) continue;

            const int step = scoutDist(st);
            if (bestIdx >= 0 && step >= bestStep) continue;
            bestStep = step;
            bestIdx = idx;
            stand = st;
        }
    return bestIdx;
}

int ScoutSystem::homeETA(const Pos& here)
{
    const int gameFrame = c.world.frame();
    if (anchor.dr == -1 || gameFrame - lastAnchorChanged > tune::SCOUT_ANCHOR_GAP)
    {
        double bestFar = -1;
        for (const auto& it : c.world.buildings())
        {
            if (it.second->Type != BUILDING_ARROWTOWER) continue;
            const Pos tp{it.second->BlockDR, it.second->BlockUR};
            const double d = geo::disSq(tp, c.world.basePos());
            if (d > bestFar)
            {
                bestFar = d;
                anchor = tp;
            }
        }
        if (anchor.dr == -1) anchor = c.world.basePos();
        lastAnchorChanged = gameFrame;
    }

    if (nearestStand(anchor, 4, home)) return scoutDist(home) * 25;
    return max(abs(here.dr - anchor.dr), abs(here.ur - anchor.ur)) * 25;
}

bool ScoutSystem::isExplore(int eta) const
{
    const int gameFrame = c.world.frame();
    for (int i = 0; i < 3; i++)
    {
        if (gameFrame >= tune::SCOUT_WAVE[i] && gameFrame <= tune::SCOUT_WAVE[i] + tune::SCOUT_HOME_STAY)
            return false;
        if (gameFrame < tune::SCOUT_WAVE[i]) return gameFrame + eta >= tune::SCOUT_WAVE[i] ? false : true;
    }
    return false;
}

void ScoutSystem::buildRoute(const Pos& goal)
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
        p = Grid<int>::of(prev);
    }
    std::reverse(route.begin(), route.end());
}

bool ScoutSystem::routeSafe() const
{
    const int lim = min((int)route.size(), routeAt + tune::SCOUT_VIEW);
    for (int i = routeAt; i < lim; i++)
        if (c.threat.at(route[i].dr, route[i].ur) > 0 || !c.world.walkable(route[i].dr, route[i].ur))
            return false;
    return true;
}

bool ScoutSystem::followRoute(const Pos& here, bool idle)
{
    while (routeAt < (int)route.size() && route[routeAt].dr == here.dr && route[routeAt].ur == here.ur)
        routeAt++;
    if (routeAt >= (int)route.size()) return false;

    const Pos next = route[routeAt];
    const int sd = next.dr - here.dr, su = next.ur - here.ur;  // 方向

    if (sd < -1 || sd > 1 || su < -1 || su > 1) return false;  // 重新规划

    int end = routeAt;
    while (end + 1 < (int)route.size() && route[end + 1].dr - route[end].dr == sd &&
           route[end + 1].ur - route[end].ur == su)
        end++;

    const int endCell = Grid<int>::index(route[end].dr, route[end].ur);
    if (endCell == routeSent)
    {
        if (!idle) return true;
        route.clear();
        return false;
    }

    routeSent = endCell;
    c.orders.moveToCell(scoutSN, route[end]);
    return true;
}

Pos ScoutSystem::fleeGoal() const
{
    Pos best = {-1, -1};
    int bestThreat = 0, bestStep = 0;
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
        {
            const int step = scoutDist(i, j);
            if (step <= 0) continue;

            const int t = c.threat.at(i, j);
            if (best.dr >= 0 && (t > bestThreat || (t == bestThreat && step >= bestStep))) continue;
            bestThreat = t;
            bestStep = step;
            best = {i, j};
            if (!t && step == 1) return best;
        }
    return best;
}

bool ScoutSystem::evade(const Pos& here, bool idle)
{
    if (c.threat.at(here.dr, here.ur) <= 0)
    {
        if (routeFlee) route.clear();
        return false;
    }

    if (routeFlee && route.size())
    {
        const Pos& tail = route.back();
        const bool goalOk = c.world.walkable(tail.dr, tail.ur) && c.threat.at(tail.dr, tail.ur) <= 0;
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

void ScoutSystem::run()
{
    if (c.phase.priestAllIn()) return;

    const tagArmy* unit = c.world.army(scoutSN);
    if (!unit) return;

    const tagArmy& u = *unit;
    const Pos here = {u.BlockDR, u.BlockUR};
    const FloatPos Fhere = {u.DR, u.UR};
    const bool idle = u.NowState == HUMAN_STATE_IDLE;
    const int gameFrame = c.world.frame();

    const int wp = MAP_L / tune::SCOUT_VIEW + 1;
    if (wpCooldown.empty())
    {
        wpCooldown.assign(wp * wp, 0);
        wpDone.assign(wp * wp, false);
    }
    if (!scoutDist.ready())
    {
        scoutDist.reset(-1);
        scoutPrev.reset(-1);
    }

    if (evade(here, idle)) return;

    if (gameFrame - lastRecordFrame >= tune::SCOUT_STUCK && geo::dis(Fhere, lastPos) >= BLOCKSIDELENGTH)
    {
        lastPos = Fhere;
        lastRecordFrame = gameFrame;
    }
    else if (route.size() && gameFrame - lastRecordFrame >= tune::SCOUT_STUCK)
    {
        if (goalWp >= 0) wpCooldown[goalWp] = gameFrame + tune::SCOUT_COOLDOWN;
        goalWp = -1;
        goalStand = {-1, -1};
        route.clear();
        lastPos = Fhere;
        lastRecordFrame = gameFrame;
    }

    floodThreat(here, true);

    if (!isExplore(homeETA(here)))
    {
        if (!arrived && geo::dis(Fhere, FloatPos(home)) < 5 * BLOCKSIDELENGTH) arrived = true;
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
        const bool reached = geo::dis(Fhere, FloatPos(goalStand)) <= 2 * BLOCKSIDELENGTH;
        const bool ok = scoutDist(goalStand) >= 0 && wpGain(goalStand) >= tune::SCOUT_MIN_GAIN;
        if (reached) wpDone[goalWp] = true;
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

// 防御

void DefenceSystem::run()
{
    hostiles.clear();
    towerAtk.clear();

    const FloatPos base = c.world.baseAt();
    for (const auto& it : c.world.enemyArmies())
    {
        const double d = geo::dis({it.second->DR, it.second->UR}, base);
        if (d < 65 * BLOCKSIDELENGTH) towerAtk.push_back(it.first);
        if (d < 40 * BLOCKSIDELENGTH) hostiles.push_back(it.first);
    }

    if (c.world.frame() < tune::FIX_TOWER_UNTIL) fixTower();

    combat = hostiles.size() > 0;
    if (!combat)
    {
        priestTarget = -1;
        return;
    }

    std::sort(hostiles.begin(), hostiles.end());  // 引擎每帧打乱列表, 定序才能稳定分配

    runTower();
    runPriest();
    runArmy();
}

void DefenceSystem::fixTower()
{
    if (c.world.stock().stone <= 0)
    {
        for (int sn : fixCrew) c.labor.release(sn);
        fixCrew.clear();
        return;
    }

    const tagBuilding* tar = nullptr;
    for (int sn : c.world.buildingsOf(BUILDING_ARROWTOWER))
    {
        const tagBuilding* t = c.world.building(sn);
        if (t->Percent < 100 || t->Blood >= t->MaxBlood) continue;
        if (!tar || t->SN < tar->SN) tar = t;
    }

    for (auto it = fixCrew.begin(); it != fixCrew.end();)
        if (c.world.farmer(*it)) it++;
        else it = fixCrew.erase(it);

    if (!tar)
    {
        for (int sn : fixCrew) c.labor.release(sn);
        fixCrew.clear();
        return;
    }

    while ((int)fixCrew.size() < tune::FIX_CREW)
    {
        const int sn = c.labor.claim(FloatPos(Pos(tar->BlockDR, tar->BlockUR)), Steal::Allow);
        if (sn < 0) break;
        fixCrew.insert(sn);
    }

    for (int sn : fixCrew) c.orders.workerTask(sn, tar->SN);
}

void DefenceSystem::runTower()
{
    for (int sn : c.world.buildingsOf(BUILDING_ARROWTOWER))
    {
        const tagBuilding* t = c.world.building(sn);
        if (t->Percent < 100) continue;

        // 优先点还没锁住诱饵的, 都锁住了就打伤害最高的那个
        int pick = -1;
        double bestDps = 0;
        for (int e : towerAtk)
        {
            if (c.world.lockOf(e) >= 0) continue;
            const double d = World::dpsOf(*c.world.enemyArmy(e));
            if (pick < 0 || d > bestDps)
            {
                bestDps = d;
                pick = e;
            }
        }
        if (pick < 0)
            for (int e : hostiles)
            {
                const double d = World::dpsOf(*c.world.enemyArmy(e));
                if (pick < 0 || d > bestDps)
                {
                    bestDps = d;
                    pick = e;
                }
            }
        if (pick < 0) return;

        if (t->Project != pick) c.orders.action(sn, pick);
    }
}

void DefenceSystem::runArmy()
{
    // 还没锁住诱饵的敌人排前面, 同档按伤害从高到低
    std::sort(hostiles.begin(), hostiles.end(), [&](int a, int b)
    {
        const bool la = c.world.lockOf(a) < 0, lb = c.world.lockOf(b) < 0;
        if (la != lb) return la;
        const double da = World::dpsOf(*c.world.enemyArmy(a)), db = World::dpsOf(*c.world.enemyArmy(b));
        return da != db ? da > db : a < b;
    });

    const int priestSN = c.world.priestSN();
    for (const auto& it : c.world.armies())
    {
        const tagArmy& u = *it.second;
        if (u.SN == priestSN || u.Sort == AT_STONE_THROWER || u.Sort == AT_CHARIOT_ARCHER) continue;

        const tagArmy* cur = c.world.enemyArmy(u.WorkObjectSN);
        if (cur && cur->Sort != AT_STONE_THROWER && u.WorkObjectSN != priestTarget) continue;

        for (int e : hostiles)
            if (e != priestTarget && c.world.enemyArmy(e)->Sort != AT_STONE_THROWER)
            {
                c.orders.action(u.SN, e);
                break;
            }
    }
}

void DefenceSystem::runPriest()
{
    if (c.phase.priestAllIn()) return;

    const int priestSN = c.world.priestSN();
    const tagArmy* self = c.world.army(priestSN);
    if (!self) return;
    const tagArmy& p = *self;

    auto convertible = [&](int e) { return c.world.enemyArmy(e) && c.world.lockOf(e) >= 0; };

    const tagArmy* t = c.world.enemyArmy(p.WorkObjectSN);
    if (t != nullptr && convertible(t->SN) && t->Sort == AT_STONE_THROWER) return;

    int pick = -1;
    double bestDps = 0;
    for (int e : hostiles)
    {
        if (c.world.lockOf(e) < 0) continue;
        double d = World::dpsOf(*c.world.enemyArmy(e));
        if (c.world.enemyArmy(e)->Sort == AT_STONE_THROWER) d += 10000;
        if (pick < 0 || d > bestDps)
        {
            bestDps = d;
            pick = e;
        }
    }

    t = c.world.enemyArmy(pick);
    const bool isS = t && t->Sort == AT_STONE_THROWER;

    if (isS)
    {
        priestTarget = pick;
        c.orders.action(priestSN, pick);
    }
    else if (convertible(p.WorkObjectSN)) { priestTarget = p.WorkObjectSN; }
    else if (pick != -1)
    {
        priestTarget = pick;
        c.orders.action(priestSN, pick);
    }
}

// 进攻
void OffenseSystem::run()
{
    if (!c.phase.armyAllIn()) return;

    const Pos basePos = c.world.basePos();
    const int priestSN = c.world.priestSN();

    if (siegeSN == -1)
        for (const auto& it : c.world.enemyBuildings())
            if (it.second->Type == BUILDING_SIEGE)
            {
                siegeSN = it.first;
                break;
            }

    if (corner.dr == -1 && basePos.dr >= 0)
    {
        corner.dr = (basePos.dr * 2 / MAP_L) ? 0 : MAP_L - 1;
        corner.ur = (basePos.ur * 2 / MAP_U) ? 0 : MAP_U - 1;
    }
    if (corner.dr < 0) return;

    if (!scratch.ready()) scratch.reset(0);
    scratch.fill(0);

    std::queue<Pos> q;
    const int stride = 5;
    const int sectorsU = (MAP_U + stride - 1) / stride;
    std::vector<bool> sectorTaken(((MAP_L + stride - 1) / stride) * sectorsU, false);
    std::vector<Pos> nextToGo;
    q.push(corner);
    scratch(corner) = 1;
    while (q.size())
    {
        const Pos crt = q.front();
        q.pop();
        if (c.world.cell(crt.dr, crt.ur).type != MAPPATTERN_UNKNOWN && c.world.walkable(crt.dr, crt.ur) &&
            c.nav.dist(crt.dr, crt.ur) >= 0)
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
            if (c.world.cell(crt.dr, crt.ur).type != MAPPATTERN_UNKNOWN) continue;
            for (int d = 0; d < 8; d++)
            {
                const Pos next = {crt.dr + dx[d], crt.ur + dy[d]};
                if (!Grid<int>::inside(next.dr, next.ur)) continue;
                if (scratch(next)) continue;
                if (c.world.cell(next.dr, next.ur).type == MAPPATTERN_OCEAN) continue;

                scratch(next) = 1;
                q.push(next);
            }
        }
    }

    nextToGo.erase(remove_if(nextToGo.begin(), nextToGo.end(), [&](const Pos& p)
    {
        return geo::angle({corner.dr - basePos.dr, corner.ur - basePos.ur},
                          {p.dr - basePos.dr, p.ur - basePos.ur}) > 15;
    }), nextToGo.end());

    std::sort(nextToGo.begin(), nextToGo.end(), [&](const Pos& a, const Pos& b)
    { return c.nav.dist(a) > c.nav.dist(b); });
    const size_t frontierK = std::max<size_t>(6, c.world.armies().size());
    if (nextToGo.size() > frontierK) nextToGo.resize(frontierK);

    if (watchPoint.dr == -1 && !nextToGo.empty()) watchPoint = nextToGo.back();
    if (c.phase.priestAllIn() && !been && watchPoint.dr >= 0)
    {
        been = true;
        c.orders.moveToCell(priestSN, watchPoint);
    }

    // 可打的目标: 视野内的敌方军队 + 已探到的敌方建筑, 去掉攻城厂
    std::vector<int> targets;
    for (const auto& it : c.world.enemyArmies())
        if (geo::dis({it.second->DR, it.second->UR}, FloatPos(corner)) <= 50 * BLOCKSIDELENGTH)
            targets.push_back(it.first);
    for (const auto& it : c.world.enemyBuildings())
        if (it.first != siegeSN) targets.push_back(it.first);

    auto posOf = [&](int sn)
    {
        const tagArmy* a = c.world.enemyArmy(sn);
        if (a) return FloatPos(a->DR, a->UR);
        const tagBuilding* b = c.world.enemyBuilding(sn);
        return b ? FloatPos(Pos(b->BlockDR, b->BlockUR)) : FloatPos(-1, -1);
    };

    auto nearestTarget = [&](const FloatPos& from)
    {
        int pick = -1;
        double best = 0;
        for (int t : targets)
        {
            if (t == siegeSN) continue;
            const double d = geo::disSq(posOf(t), from);
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

    for (const auto& it : c.world.armies())
    {
        const tagArmy& u = *it.second;
        if (u.SN == priestSN || (u.Sort != AT_CHARIOT_ARCHER && u.Sort != AT_STONE_THROWER)) continue;

        if (c.world.enemyArmy(u.WorkObjectSN) || c.world.enemyBuilding(u.WorkObjectSN)) continue;

        const int t = nearestTarget({u.DR, u.UR});
        if (t >= 0) c.orders.action(u.SN, t);
        else if (u.NowState == HUMAN_STATE_IDLE && targets.size() == 0)  // 什么都看不见
        {
            const int idx = pickNearest(u.DR, u.UR);
            if (idx >= 0)
            {
                used[idx] = true;
                c.orders.moveToCell(u.SN, nextToGo[idx]);
            }
        }
    }

    if (c.phase.priestAllIn() && siegeSN >= 0 && targets.size() <= 3)
    {
        const tagArmy* p = c.world.army(priestSN);
        if (p && p->WorkObjectSN != siegeSN) c.orders.action(priestSN, siegeSN);
    }
}

void OffenseSystem::clearRoad()
{
    if (!scratch.ready()) scratch.reset(0);
    scratch.fill(0);

    std::vector<Pos> points;
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (c.nav.dist(i, j) >= 22 && c.nav.dist(i, j) <= 25)
            {
                scratch(i, j) = 1;
                points.push_back({i, j});
            }

    if (points.empty()) return;

    for (const auto& a : c.world.armies())
    {
        const tagArmy* u = a.second;
        if (scratch(u->BlockDR, u->BlockUR) || u->NowState != HUMAN_STATE_IDLE) continue;

        const int ran = rand() % points.size();
        c.orders.moveToCell(u->SN, points[ran]);
    }
}

// 雷霆狮子

void LionHuntSystem::run()
{
    if (c.world.frame() < tune::LION_HUNT_FROM) return;

    const Pos basePos = c.world.basePos();
    const Pos& corner = offense.assaultCorner();  // 上一帧的值, 与原实现一致

    const tagResource* tar = nullptr;
    double best = 0;
    for (const tagResource* l : c.world.lions())
    {
        const double d = geo::dis({l->DR, l->UR}, c.world.baseAt());
        const double a = geo::angle({corner.dr - basePos.dr, corner.ur - basePos.ur},
                                    {l->BlockDR - basePos.dr, l->BlockUR - basePos.ur});
        if (d > 40 * BLOCKSIDELENGTH && a > 15) continue;
        if (!tar || d < best || (d == best && l->SN < tar->SN))
        {
            best = d;
            tar = l;
        }
    }

    for (auto it = lionCrew.begin(); it != lionCrew.end();)
        if (c.world.farmer(*it)) it++;
        else it = lionCrew.erase(it);

    if (!tar)
    {
        for (int sn : lionCrew) c.labor.release(sn);
        lionCrew.clear();
        return;
    }

    while ((int)lionCrew.size() < tune::LION_CREW)
    {
        const int sn = c.labor.claim({tar->DR, tar->UR}, Steal::Allow);
        if (sn < 0) break;
        lionCrew.insert(sn);
    }

    for (int sn : lionCrew) c.orders.workerTask(sn, tar->SN);
}

// 策略

int StrategySystem::farmerTargetByPop() const
{
    int armyPop2 = 0;
    const bool logistics = production.hasTech(BUILDING_ARMYCAMP_RESEARCH_LOGISTICS);
    for (const auto& it : c.world.armies())
    {
        const int s = it.second->Sort;
        const bool fromCamp = (s == AT_CLUBMAN || s == AT_SLINGER || s == AT_SWORDSMAN || s == AT_BROADSWORDSMAN);
        armyPop2 += (fromCamp && logistics) ? 1 : 2;
    }
    const int armyPop = (armyPop2 + 1) / 2;

    const int MIN_FARMER = 8;
    const int HEADROOM = 6;
    return std::max(MIN_FARMER, std::min(20, c.world.humanMax() - armyPop - HEADROOM));
}

void StrategySystem::run()
{
    int b_prio = 100;
    int e_prio = 100;

    const int civStage = c.world.civStage();
    const int population = max(0, (int)c.world.farmers().size() - farms.farmCount());

    const int homeCnt = min(12, (int)(c.world.farmers().size() + c.world.armies().size()) / 4 + 1);
    build.wantBuilding(BUILDING_HOME, homeCnt, b_prio--);

    if (opt.useStock) build.wantStock(b_prio--);

    const int farmerTarget =
        opt.usePopModel ? farmerTargetByPop() : (civStage == CIVILIZATION_TOOLAGE ? 10 : 24);

    if (civStage == CIVILIZATION_TOOLAGE)
    {
        build.wantBuilding(BUILDING_ARMYCAMP, 1, b_prio--);
        build.wantBuilding(BUILDING_RANGE, 1, b_prio--);
        build.wantBuilding(BUILDING_MARKET, 1, b_prio--);
        build.wantBuilding(BUILDING_FARM, 2, b_prio--);

        production.wantTech(BUILDING_CENTER_UPGRADE, e_prio--);
        production.wantUnit(AT_FARMER, farmerTarget, e_prio--);
        production.wantTech(BUILDING_GRANARY_ARROWTOWER, e_prio--);
    }
    else
    {
        build.wantBuilding(BUILDING_FARM, 4, b_prio--);
        if (production.hasTech(BUILDING_GRANARY_ARROWTOWER))
            build.wantBuilding(BUILDING_ARROWTOWER, 3, b_prio--);
        build.wantBuilding(BUILDING_RANGE, 3, b_prio--);

        production.wantTech(BUILDING_MARKET_WHEEL_UPGRADE, e_prio--);
        production.wantUnit(AT_FARMER, farmerTarget, e_prio--);
        production.wantTech(BUILDING_GRANARY_ARROWTOWER, e_prio--);
        if (production.hasTech(BUILDING_MARKET_WHEEL_UPGRADE) &&
            c.world.buildingCount(BUILDING_RANGE) >= 2)
            production.wantUnit(AT_CHARIOT_ARCHER, c.world.unitCount(AT_CHARIOT_ARCHER) + 4, e_prio--);
    }

    economy.rebalance(population, build.demand() + production.demand());

    const PopPlan& pop = economy.plan();
    int foodPop = pop.food;
    int woodPop = pop.wood;
    int stonePop = pop.stone;

    if (opt.useLiveHunting)
    {
        if (c.world.frame() > 2 * 25 * 60 && hunt.siteCount() &&
            gather.spotsWithin(RK_CORPSE, 1e9) < 5)
            hunt.toHunt(foodPop, 2, 0);
        gather.toPool(foodPop, tune::POP_INF, RK_CORPSE);
    }

    gather.toPool(foodPop, tune::POP_INF, RK_BUSH, 60 * BLOCKSIDELENGTH);
    gather.toPool(stonePop, tune::POP_INF, RK_STONE);
    woodPop += foodPop + stonePop;
    gather.toPool(woodPop, tune::POP_INF, RK_WOOD);
}

// Mgr

Mgr::Mgr()
    : world(),
      nav(world),
      threat(world),
      phase(world),
      labor(world),
      orders(world),
      datapack{world, nav, threat, phase, labor, orders},
      gather(datapack),
      hunt(datapack),
      farms(datapack),
      build(datapack, gather),
      production(datapack),
      economy(datapack),
      scout(datapack),
      defence(datapack),
      offense(datapack),
      lionHunt(datapack, offense),
      strategy(datapack, build, production, economy, gather, hunt, farms)
{
    orders.bind(this);

    // 注册顺序即 Labor 广播 detachWorker 的顺序
    labor.registerSystem(&farms);
    labor.registerSystem(&hunt);
    labor.registerSystem(&gather);
    labor.registerSystem(&build);
    labor.registerSystem(&lionHunt);
    labor.registerSystem(&defence);
}

void Mgr::update(const tagInfo& info)
{
    world.rebuild(info);
    phase.update();
    nav.rebuild();
    gather.onFrame();
    build.onFrame();
    farms.onFrame();
    production.onFrame();
    hunt.onFrame();
    labor.rebuild();
    gather.resetDesired();
    hunt.resetDesired();
    scout.onFrame();

    threat.rebuild();
    defence.run();
    if (!defence.inCombat()) scout.run();
    if (!defence.inCombat() && !phase.armyAllIn()) offense.clearRoad();

    lionHunt.run();

    offense.run();

    farms.run();

    strategy.run();

    world.clearReserved();

    production.run();
    build.run();

    hunt.run();
    gather.run();

    production.runDestroy();
}