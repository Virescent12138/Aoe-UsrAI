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

int dx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
int dy[8] = {1, 0, -1, 0, 1, -1, -1, 1};

int Mgr::s_nextSiteID = 0;

Mgr mgr;

void UsrAI::processData() { mgr.update(getInfo()); }

enum
{
    AT_FARMER = -1
};

const int PLACE_NEAR_BASE = 80;      // 紧贴基地
const int PLACE_ADJACENT = 80;       // 紧贴其它建筑
const int PLACE_PER_RING = 1;        // 每远离基地一环
const int PLACE_BAND_BONUS = -60;    // 落在该建筑理想距离带内
const int PLACE_BAND_FAIL = 60;      // 贴太近
const int PLACE_RES_BONUS = -80;     // 靠近本建筑关心的资源
const int PLACE_IMPOSSIBLE = 10000;  // 不可能值
const int PLACE_FAILED = 400;        // 之前建造失败过的地基, 按次数累加
const int PLACE_NEAR_RES = 100;      // 紧贴资源

const int PLACE_SAVE_SCALE = 16;  // 多少搬运帧折算一点选址代价

const int STOCK_PAYBACK = 4;         // 省下的帧数要够建造成本的几倍才动工
const int STOCK_PLAN_GAP = 125;      // 仓库规划多少帧算一次
const int STOCK_SERVE = 6;           // 认为每种资源能使用多少
const int STOCK_DISCOUNT_OTHER = 6;  // 非食物除以这个数值

const int SCOUT_VIEW = 12;

const int SCOUT_MIN_GAIN = 8;
const int SCOUT_STUCK = 75;
const int SCOUT_COOLDOWN = 375;
const int SCOUT_HOME_STAY = 25 * 90;
const int SCOUT_WAVE[3] = {25 * 240, 25 * 540, 25 * 840};

const int BUILD_CREW = 2;  // 一个工地派几个人
const int LION_CREW = 1;   // 打一只狮子派几个人

const int FIX_CREW = 2;       // 修箭塔派几个人
const int PRIEST_MARGIN = 2;  // 祭司站位比敌人射程多留几格

const int ARMY_ALL_IN = 25 * 60 * 16;
const int PRIEST_ALL_IN = 25 * 60 * 18;

int STOCK_MAX = 2;    // 最多几个仓库
int LION_KEEP = 4;    // 狮子视野3
int ENEMY_KEEP = 10;  // 离敌方单位

double Mgr::disSq(const Pos& a, const Pos& b)
{
    double ddr = a.dr - b.dr, dur = a.ur - b.ur;
    return ddr * ddr + dur * dur;
}

double Mgr::disSq(const FloatPos& a, const FloatPos& b)
{
    double ddr = a.dr - b.dr, dur = a.ur - b.ur;
    return ddr * ddr + dur * dur;
}

double Mgr::dis(const FloatPos& a, const FloatPos& b) { return std::sqrt(disSq(a, b)); }

void Mgr::formHunts(const std::vector<const tagResource*>& sor, const int threshold)
{
    hunts.clear();
    if (sor.size())
    {
        const Double radiusSq = threshold * BLOCKSIDELENGTH * threshold * BLOCKSIDELENGTH;
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
                const tagResource* c = sor[q[h]];
                const FloatPos cp = {c->DR, c->UR};
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

            const int n = site.members.size();
            site.center = {sumDR / n, sumUR / n};

            for (int sn : site.members)
            {
                auto it = huntOfSN.find(sn);
                if (it == huntOfSN.end() || taken.count(it->second)) continue;
                if (site.id < 0 || it->second < site.id) site.id = it->second;
            }
            if (site.id < 0) site.id = s_nextSiteID++;
            taken.insert(site.id);

            hunts.push_back(site);
        }

        hunts.erase(remove_if(hunts.begin(), hunts.end(), [&](const HuntSite& s) { return s.members.size() <= 2; }),
                    hunts.end());

        std::sort(hunts.begin(), hunts.end(), [&](const HuntSite& a, const HuntSite& b)
        { return disSq(baseFloatPos, a.center) < disSq(baseFloatPos, b.center); });
    }

    huntByID.clear();
    huntOfSN.clear();
    for (HuntSite& s : hunts)
    {
        huntByID[s.id] = &s;
        for (int sn : s.members) huntOfSN[sn] = s.id;
    }
}

HuntSite* Mgr::huntOf(int siteID)
{
    auto it = huntByID.find(siteID);
    return it == huntByID.end() ? nullptr : it->second;
}

int Mgr::nearestOf(const std::vector<int>& cand, const FloatPos& at) const
{
    int best = -1;
    double bestDis = 0;
    for (int sn : cand)
    {
        const tagFarmer* f = farmer(sn);
        const double d = disSq(at, FloatPos(f->DR, f->UR));
        if (best < 0 || d < bestDis)
        {
            bestDis = d;
            best = sn;
        }
    }
    return best;
}

int Mgr::takeNearestFree(const FloatPos& at, bool allowSteal)
{
    int best = nearestOf(freeFarmers, at);
    if (best >= 0)
    {
        auto it = std::find(freeFarmers.begin(), freeFarmers.end(), best);
        if (it != freeFarmers.end())
        {
            *it = freeFarmers.back();
            freeFarmers.pop_back();
        }
        return best;
    }
    if (!allowSteal) return -1;

    HuntSite* from = nullptr;
    for (auto& it : huntByID)
        if (it.second->staff.size() && (!from || it.second->staff.size() > from->staff.size())) from = it.second;

    if (from)
    {
        best = nearestOf(from->staff, at);
        if (best >= 0)
        {
            auto& v = from->staff;
            auto it = std::find(v.begin(), v.end(), best);
            if (it != v.end())
            {
                *it = v.back();
                v.pop_back();
            }
            staffHunt.erase(best);
            return best;
        }
    }

    for (int k = RK_COUNT - 1; k >= 0; k--)
    {
        std::vector<int> _crew;
        for (const GatherSpot& s : pools[k].spots)
        {
            auto it = workerOfSpot.find(s.sn);
            if (it != workerOfSpot.end()) _crew.push_back(it->second);
        }
        best = nearestOf(_crew, at);
        if (best < 0) continue;

        dropSpot(best, false);
        return best;
    }
    return -1;
}

void Mgr::assign(int x, HuntSite& site)
{
    while (x-- > 0)
    {
        const int sn = takeNearestFree(site.center, false);
        if (sn < 0) return;
        site.staff.push_back(sn);
        staffHunt[sn] = site.id;
    }
}

void Mgr::release(int x, HuntSite& site)
{
    while (x > 0 && site.staff.size())
    {
        const int sn = site.staff.back();
        site.staff.pop_back();
        staffHunt.erase(sn);
        freeFarmers.push_back(sn);
        x--;
    }
}

void Mgr::toHunt(int& from, int num, HuntSite& site)
{
    const int t = max(0, min(from, num));
    from -= t;
    site.desired += t;
}

void Mgr::update(const tagInfo& info)
{
    makeFrame(info);

    STOCK_MAX = 2 + gameFrame / 25 / 60 / 10;

    armyAllIn = gameFrame > ARMY_ALL_IN;
    priestAllIn = gameFrame > PRIEST_ALL_IN;

    buildThreat();
    defence();
    if (!inCombat) scout();
    if (!inCombat && !armyAllIn) clearRoad();

    killLions();

    attack();

    manageFarms();
    int b_prio = 100;
    int e_prio = 100;

    // strategy begins here
    const int population = max(0, (int)farmers.size() - (int)farmList.size());

    const int homeCnt = min(12, (int)(farmers.size() + armies.size()) / 4 + 1);
    wantBuilding(BUILDING_HOME, homeCnt, b_prio--);

    //wantStock(b_prio--);

    /*
    int armyPop2 = 0;
    const bool logistics = doneTech.count(BUILDING_ARMYCAMP_RESEARCH_LOGISTICS) > 0;
    for (const auto& it : armies)
    {
        const int s = it.second->Sort;
        const bool fromCamp =
            (s == AT_CLUBMAN || s == AT_SLINGER || s == AT_SWORDSMAN || s == AT_BROADSWORDSMAN);
        armyPop2 += (fromCamp && logistics) ? 1 : 2;
    }
    const int armyPop = (armyPop2 + 1) / 2;

    const int MIN_FARMER = 8;
    const int HEADROOM = 6;
    const int farmerTarget = std::max(MIN_FARMER, std::min(20, humanMax - armyPop - HEADROOM));
    wantUnit(AT_FARMER, farmerTarget, 80);
    */

    const int farmerTarget = civStage == CIVILIZATION_TOOLAGE ? 10 : 24;

    if (civStage == CIVILIZATION_TOOLAGE)
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
        if (doneTech.count(BUILDING_GRANARY_ARROWTOWER)) wantBuilding(BUILDING_ARROWTOWER, 3, b_prio--);
        wantBuilding(BUILDING_RANGE, 3, b_prio--);

        wantTech(BUILDING_MARKET_WHEEL_UPGRADE, e_prio--);
        wantUnit(AT_FARMER, farmerTarget, e_prio--);
        wantTech(BUILDING_GRANARY_ARROWTOWER, e_prio--);
        if (doneTech.count(BUILDING_MARKET_WHEEL_UPGRADE) && cntBuilding(BUILDING_RANGE) >= 2)
            wantUnit(AT_CHARIOT_ARCHER, cntUnit(AT_CHARIOT_ARCHER) + 4, e_prio--);
    }

    rebalancePop(population, computeDemand());

    int foodPop = poolPop[0];
    int woodPop = poolPop[1];
    int stonePop = poolPop[2];

    //if (gameFrame > 2 * 25 * 60 && hunts.size() && spotsWithin(RK_CORPSE, 1e9) < 5) toHunt(foodPop, 2, hunts[0]);
    //toPool(foodPop, 2000000000, RK_CORPSE);
    toPool(foodPop, 2000000000, RK_BUSH, 60 * BLOCKSIDELENGTH);
    toPool(stonePop, 2000000000, RK_STONE);
    woodPop += foodPop + stonePop;
    toPool(woodPop, 2000000000, RK_WOOD);

    // strategy ends here

    reserved = Stock();

    runProduction();
    runBuild();

    runHunt();
    runGather();

    destroyLater();
}

void Mgr::makeFrame(const tagInfo& info)
{
    destroyCnt = -1;
    farmers.clear();
    armies.clear();
    buildings.clear();
    resources.clear();
    enemyArmies.clear();
    enemyBuildings.clear();
    freeFarmers.clear();
    _unit.clear();
    _building.clear();
    __building.clear();
    _unit.resize(32, 0);
    _building.resize(32, 0);
    __building.resize(32, 0);
    lionsPos.clear();
    lions.clear();
    farmList.clear();
    buildingsByType.clear();
    foodDepots.clear();
    resDepots.clear();
    blockCell.assign(MAP_L * MAP_U, 0);
    bfsUsed.assign(MAP_L * MAP_U, 0);
    costMap.assign(MAP_L * MAP_U, 0);
    prods.clear();
    builds.clear();
    ally.clear();

    gameFrame = info.GameFrame;

    // 构建反查
    for (const auto& f : info.farmers)
    {
        farmers[f.SN] = &f;
        _unit[0]++;  // AT_FARMER = -1, offset + 1
        ally.insert(f.SN);
    }

    for (const auto& a : info.armies)
    {
        armies[a.SN] = &a;
        _unit[a.Sort + 1]++;

        if (priestSN == -1 && a.Sort == AT_PRIEST) priestSN = a.SN;
        ally.insert(a.SN);
    }

    for (const auto& a : info.enemy_armies) enemyArmies[a.SN] = &a;

    // 更新信息
    stock.wood = info.Wood;
    stock.meat = info.Meat;
    stock.stone = info.Stone;
    stock.gold = info.Gold;
    humanMax = info.Human_MaxNum;
    civStage = info.civilizationStage;
    theMap = info.theMap;

    std::vector<const tagResource*> prey;  // 聚类输入
    for (const auto& r : info.resources)
    {
        resources[r.SN] = &r;

        if (r.Type == RESOURCE_LION && r.Blood > 0)
        {
            lionsPos.push_back({r.BlockDR, r.BlockUR});
            lions.insert(&r);
        }

        if (r.Type == RESOURCE_GAZELLE && r.Blood > 0) continue;
        if (resourceSize(r.Type) == 1) blockCell[r.BlockDR * MAP_U + r.BlockUR] = 1;
        else
        {
            int a = r.DR / BLOCKSIDELENGTH + 0.5;
            int b = r.UR / BLOCKSIDELENGTH + 0.5;
            blockCell[(a - 1) * MAP_U + (b - 1)] = 1;
            blockCell[a * MAP_U + (b - 1)] = 1;
            blockCell[(a - 1) * MAP_U + b] = 1;
            blockCell[a * MAP_U + b] = 1;
        }
    }

    auto mark = [&](const tagBuilding& b)
    {
        const int s = buildingSize(b.Type);
        for (int i = b.BlockDR; i < b.BlockDR + s; i++)
            for (int j = b.BlockUR; j < b.BlockUR + s; j++)
                if (i >= 0 && j >= 0 && i < MAP_L && j < MAP_U) blockCell[i * MAP_U + j] = 1;
    };

    for (const auto& b : info.buildings)
    {
        ally.insert(b.SN);
        buildings[b.SN] = &b;
        _building[b.Type]++;
        if (b.Percent >= 100) __building[b.Type]++;
        buildingsByType[b.Type].push_back(b.SN);
        mark(b);

        if (basePos.dr == -1 && b.Type == BUILDING_CENTER)
        {
            basePos.dr = b.BlockDR, basePos.ur = b.BlockUR;
            baseFloatPos = FloatPos(basePos);
            baseFloatPos.sn = b.SN;
        }
        if (b.Type == BUILDING_FARM && b.Percent >= 100) farmList.push_back(b.SN);

        if (b.Percent < 100) continue;
        const double half = buildingSize(b.Type) * 0.5;
        const FloatPos c((b.BlockDR + half) * BLOCKSIDELENGTH, (b.BlockUR + half) * BLOCKSIDELENGTH);
        if (b.Type == BUILDING_CENTER) foodDepots.push_back(c), resDepots.push_back(c);
        else if (b.Type == BUILDING_GRANARY) foodDepots.push_back(c);
        else if (b.Type == BUILDING_STOCK) resDepots.push_back(c);
    }

    for (const auto& b : info.enemy_buildings)
    {
        enemyBuildings[b.SN] = &b;
        mark(b);
    }

    // 聚类打猎
    for (const auto& r : info.resources)
        if (huntable(&r)) prey.push_back(&r);

    buildGather();

    if (savePlanned < 0 || gameFrame - savePlanned >= STOCK_PLAN_GAP)
    {
        buildSaveMap();
        savePlanned = gameFrame;
    }

    formHunts(prey, 6);
    restoreStaff();

    // 空闲池
    for (const auto& it : farmers)
    {
        const int sn = it.first;
        if (workerToFarm.count(sn) || isBuilder(sn) || staffHunt.count(sn) || spotOfWorker.count(sn) ||
            lionCrew.count(sn) || fixCrew.count(sn))
            continue;
        freeFarmers.push_back(sn);
    }

    // 人口分配
    for (int k = 0; k < RK_COUNT; k++) pools[k].desired = 0;
    for (HuntSite& s : hunts) s.desired = 0;

    if (!army(scoutSN))
    {
        scoutSN = priestSN;
        for (const auto& a : info.armies)
            if (a.Sort == AT_SCOUT)
            {
                scoutSN = a.SN;
                break;
            }
    }

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

bool Mgr::nearLion(const std::vector<Pos>& src, int dr, int ur, int radius) const
{
    for (const Pos& l : src)
        if (std::abs(l.dr - dr) <= radius && std::abs(l.ur - ur) <= radius) return true;
    return false;
}

int Mgr::buildingSize(int type) const
{
    if (type == BUILDING_HOME || type == BUILDING_ARROWTOWER) return 2;
    return 3;
}

int Mgr::buildWoodCost(int type) const
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

int Mgr::buildStoneCost(int type) const
{
    if (type == BUILDING_ARROWTOWER) return BUILD_ARROWTOWER_STONE;
    return 0;
}

int Mgr::resourceSize(int type) const
{
    if (type == RESOURCE_STONE || type == RESOURCE_GOLD || type == RESOURCE_FISH) return 2;
    return 1;
}

void Mgr::unbindFarm(std::unordered_map<int, int>::iterator it)
{
    workerToFarm.erase(it->second);
    if (farmers.count(it->second)) freeFarmers.push_back(it->second);
    farmToWorker.erase(it);
}

void Mgr::manageFarms()
{
    unordered_set<int> _farms;
    _farms.insert(farmList.begin(), farmList.end());

    for (auto it = farmToWorker.begin(); it != farmToWorker.end();)
    {
        auto cur = it++;
        if (!_farms.count(cur->first) || !farmers.count(cur->second)) unbindFarm(cur);
    }

    for (int farmSN : farmList)
    {
        auto it = farmToWorker.find(farmSN);
        if (it == farmToWorker.end())
        {
            const tagBuilding* farm = building(farmSN);
            const Pos p(farm->BlockDR, farm->BlockUR);
            const int sn = nearestOf(freeFarmers, FloatPos(p));
            if (sn < 0) continue;

            auto it2 = std::find(freeFarmers.begin(), freeFarmers.end(), sn);
            if (it2 != freeFarmers.end())
            {
                *it2 = freeFarmers.back();
                freeFarmers.pop_back();
            }
            workerToFarm[sn] = farmSN;
            it = farmToWorker.emplace(farmSN, sn).first;
        }

        const int sn = it->second;
        sendAction(sn, farmSN);
    }
}

bool Mgr::canPlace(int dr, int ur, int size) const
{
    if (dr < 0 || ur < 0 || dr + size - 1 >= MAP_L || ur + size - 1 >= MAP_U) return false;

    int h = cell(dr, ur).height;
    for (int i = dr; i < dr + size; i++)
        for (int j = ur; j < ur + size; j++)
            if (!valid(i, j) || cell(i, j).height != h || blockCell[i * MAP_U + j]) return false;
    return true;
}

void Mgr::buildPlaceMask(int type, int size)
{
    placeOk.assign(MAP_L * MAP_U, false);

    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
        {
            if (!canPlace(i, j, size)) continue;

            bool reach = false;
            for (int a = i; a < i + size && !reach; a++)
                for (int b = j; b < j + size && !reach; b++)
                    if (distMap[a * MAP_U + b] != -1) reach = true;
            if (!reach) continue;

            placeOk[i * MAP_U + j] = true;
        }
}

Pos Mgr::findSpot(int type)
{
    fill(costMap.begin(), costMap.end(), 0);
    const int size = buildingSize(type);
    const int baseLen = buildingSize(BUILDING_CENTER);

    buildPlaceMask(type, size);

    // 通用距离惩罚
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (distMap[i * MAP_U + j] == -1) costMap[i * MAP_U + j] += MAP_L + MAP_U;
            else costMap[i * MAP_U + j] += distMap[i * MAP_U + j];

    // 靠近基地额外惩罚
    bfs(costMap, basePos, baseLen, Wave(0).upto(2).grad(PLACE_NEAR_BASE));

    // 通用靠近建筑惩罚
    for (auto& it : buildings)
    {
        int len = buildingSize(it.second->Type);
        int dr = it.second->BlockDR, ur = it.second->BlockUR;
        bfs(costMap, {dr, ur}, len, Wave(0).upto(1).grad(PLACE_ADJACENT));
    }

    // 通用靠近资源惩罚
    for (const auto& it : resources)
    {
        const tagResource* r = it.second;
        if (r->Type == RESOURCE_GAZELLE && r->Blood > 0) continue;  // 活羚羊会跑, 不占地(同 makeFrame)

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
                if (a < 0 || b < 0 || a >= MAP_L || b >= MAP_U) continue;
                if (a >= dr && a < dr + len && b >= ur && b < ur + len) continue;
                costMap[a * MAP_U + b] += PLACE_NEAR_RES;
            }
    }

    // 根据建筑类型特化
    switch (type)
    {
        case BUILDING_FARM:  // 靠近谷仓, 基地排布
            for (auto& it : buildings)
            {
                if (it.second->Type == BUILDING_GRANARY)
                    bfs(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_GRANARY),
                        Wave(PLACE_BAND_BONUS).band(1, 4));

                if (it.second->Type == BUILDING_CENTER)
                    bfs(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_CENTER),
                        Wave(PLACE_BAND_BONUS).band(2, 4));
            }
            break;

        case BUILDING_GRANARY:  // 和基地以及其他谷仓保持6-8格左右的距离, 靠近浆果丛
            bfs(costMap, basePos, baseLen, Wave(PLACE_BAND_BONUS).band(6, 8));
            bfs(costMap, basePos, baseLen, Wave(PLACE_BAND_FAIL).upto(5));
            for (auto& it : buildings)
                if (it.second->Type == BUILDING_GRANARY)
                {
                    Pos p = {it.second->BlockDR, it.second->BlockUR};
                    int len = buildingSize(BUILDING_GRANARY);
                    bfs(costMap, p, len, Wave(PLACE_BAND_BONUS).band(6, 8));
                    bfs(costMap, p, len, Wave(PLACE_BAND_FAIL).upto(5));
                }
            for (const GatherSpot& s : pools[RK_BUSH].spots)
                bfs(costMap, {(int)(s.at.dr / BLOCKSIDELENGTH), (int)(s.at.ur / BLOCKSIDELENGTH)}, 1,
                    Wave(PLACE_RES_BONUS).upto(4));
            break;

        case BUILDING_ARMYCAMP:  // 和基地保持6-9格的距离
        case BUILDING_COLLAGE:   // 同上
            bfs(costMap, basePos, baseLen, Wave(PLACE_BAND_BONUS).band(6, 9));
            bfs(costMap, basePos, baseLen, Wave(PLACE_BAND_FAIL).upto(5));
            break;

        case BUILDING_HOME:  // 紧靠其他房屋建造(形成大矩形占地), 和基地保持5格的距离
            for (auto& it : buildings)
                if (it.second->Type == BUILDING_HOME)
                    bfs(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_HOME),
                        Wave(-PLACE_ADJACENT * 3).band(1, 1));
            bfs(costMap, basePos, baseLen, Wave(PLACE_BAND_FAIL).upto(5));
            break;

        case BUILDING_STOCK:
            bfs(costMap, basePos, buildingSize(BUILDING_CENTER), Wave(PLACE_IMPOSSIBLE).band(60, 1000));
            break;

        case BUILDING_ARROWTOWER:
            for (auto& it : buildings)
                if (it.second->Type == BUILDING_ARROWTOWER)
                    bfs(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(BUILDING_ARROWTOWER),
                        Wave(PLACE_ADJACENT * 3).upto(8));
            break;
    }

    // 每格代价先摊平成均值
    const int area = size * size;

    // 前缀和
    sumMap.assign((MAP_L + 1) * (MAP_U + 1), 0);
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            sumMap[(i + 1) * (MAP_U + 1) + (j + 1)] = sumMap[i * (MAP_U + 1) + (j + 1)] +
                                                      sumMap[(i + 1) * (MAP_U + 1) + j] - sumMap[i * (MAP_U + 1) + j] +
                                                      costMap[i * MAP_U + j];

    Pos best = {-1, -1};
    long long bestCost = 0;
    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
        {
            const int at = i * MAP_U + j;
            if (!placeOk[at]) continue;

            const int W = MAP_U + 1;
            long long sum = sumMap[(i + size) * W + (j + size)] - sumMap[i * W + (j + size)] -
                            sumMap[(i + size) * W + j] + sumMap[i * W + j];
            sum /= area;

            auto fit = failedSpots.find(at);
            if (fit != failedSpots.end()) sum += (long long)PLACE_FAILED * fit->second;

            if (type == BUILDING_STOCK) sum -= saveMap[at] / PLACE_SAVE_SCALE;  // 智能建造

            if (best.dr < 0 || sum < bestCost)
            {
                bestCost = sum;
                best = {i, j};
            }
        }
    return best;
}

bool Mgr::valid(int dr, int ur) const
{
    if (dr < 0 || ur < 0 || dr >= MAP_L || ur >= MAP_U) return false;
    const tagTerrain& t = cell(dr, ur);
    return t.height != -1 && (t.type == MAPPATTERN_DESERT || t.type == MAPPATTERN_GRASS);
}

void Mgr::bfs(std::vector<int>& Map, const std::vector<Pos>& seeds, const Wave& w)
{
    if (w.outer < w.inner || seeds.empty()) return;
    fill(bfsUsed.begin(), bfsUsed.end(), 0);

    queue<Pos> q;
    for (const Pos& s : seeds)
    {
        if (s.dr < 0 || s.ur < 0 || s.dr >= MAP_L || s.ur >= MAP_U) continue;
        if (bfsUsed[s.dr * MAP_U + s.ur]) continue;
        q.push(s);
        bfsUsed[s.dr * MAP_U + s.ur] = 1;
    }

    for (int r = 0; q.size(); r++)
    {
        if (r > w.outer) return;
        const int cost = w.at(r);
        int u = q.size();
        for (int i = 0; i < u; i++)
        {
            Pos crt = q.front();
            q.pop();
            if (r >= w.inner) Map[crt.dr * MAP_U + crt.ur] += cost;
            for (int d = 0; d < 8; d++)
            {
                Pos next = {crt.dr + dx[d], crt.ur + dy[d]};
                if (next.dr < 0 || next.dr >= MAP_L || next.ur < 0 || next.ur >= MAP_U ||
                    bfsUsed[next.dr * MAP_U + next.ur] || blockCell[next.dr * MAP_U + next.ur])
                    continue;
                q.push(next);
                bfsUsed[next.dr * MAP_U + next.ur] = 1;
            }
        }
    }
}

void Mgr::bfs(std::vector<int>& Map, const Pos& around, int size, const Wave& w)
{
    if (around.dr < 0) return;
    vector<Pos> seeds;
    seeds.reserve(size * size);
    for (int i = around.dr; i < around.dr + size; i++)
        for (int j = around.ur; j < around.ur + size; j++) seeds.push_back({i, j});
    bfs(Map, seeds, w);
}

bool Mgr::huntable(const tagResource* r) const
{ return r->Type == RESOURCE_GAZELLE && r->Blood > 0 && !nearLion(lionsPos, r->BlockDR, r->BlockUR, LION_KEEP); }

void Mgr::runHunt()
{
    for (HuntSite& s : hunts)
        if (s.staff.size() > s.desired) release(s.staff.size() - s.desired, s);

    for (HuntSite& s : hunts)
    {
        if (s.staff.size() < s.desired) assign(s.desired - s.staff.size(), s);
        if (s.staff.size()) driveHunt(s);
    }
}

void Mgr::driveHunt(HuntSite& site)
{
    const tagResource* prey = nullptr;

    auto valid = huntPrey.find(site.id);
    if (valid != huntPrey.end())
    {
        const tagResource* r = resource(valid->second);
        if (r && huntable(r)) prey = r;
    }

    if (!prey)  // 挑最近的一只
    {
        double bestDis = 0;
        for (int sn : site.members)
        {
            const tagResource* r = resource(sn);
            if (!r || !huntable(r)) continue;

            const double d = disSq(baseFloatPos, FloatPos(r->DR, r->UR));
            if (!prey || d < bestDis)
            {
                bestDis = d;
                prey = r;
            }
        }
        if (!prey)  // 打光, 交给采集系统
        {
            huntPrey.erase(site.id);
            release(site.staff.size(), site);
            return;
        }
        huntPrey[site.id] = prey->SN;
    }

    for (int sn : site.staff) sendAction(sn, prey->SN);
}

ResKind Mgr::kindOf(int resourceType)
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

void Mgr::floodReach()
{
    distMap.assign(MAP_L * MAP_U, -1);
    if (basePos.dr < 0) return;
    std::queue<Pos> q;

    const int baseLen = buildingSize(BUILDING_CENTER);
    for (int i = basePos.dr; i < basePos.dr + baseLen; i++)
        for (int j = basePos.ur; j < basePos.ur + baseLen; j++)
        {
            distMap[i * MAP_U + j] = 0;
            q.push({i, j});
        }

    for (int dist = 1; q.size(); dist++)
    {
        int u = q.size();
        for (int t = 0; t < u; t++)
        {
            const Pos c = q.front();
            q.pop();
            for (int d = 0; d < 8; d++)
            {
                const Pos n = {c.dr + dx[d], c.ur + dy[d]};
                if (!walkable(n.dr, n.ur) || distMap[n.dr * MAP_U + n.ur] != -1) continue;
                if (dx[d] && dy[d] && (!walkable(c.dr + dx[d], c.ur) || !walkable(c.dr, c.ur + dy[d]))) continue;
                distMap[n.dr * MAP_U + n.ur] = dist;
                q.push(n);
            }
        }
    }
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
            if (i < 0 || j < 0 || i >= MAP_L || j >= MAP_U) continue;
            if (distMap[i * MAP_U + j] == -1 || claimed[i * MAP_U + j]) continue;

            out = {i, j};
            return true;
        }
    return false;
}

double Mgr::depotCost(const FloatPos& at, const std::vector<FloatPos>& depots) const
{
    double best = -1;
    for (const FloatPos& d : depots)
    {
        const double v = dis(at, d);
        if (best < 0 || v < best) best = v;
    }
    if (best < 0) return dis(at, baseFloatPos);
    return best;
}

void Mgr::buildGather()
{
    for (int k = 0; k < RK_COUNT; k++) pools[k].spots.clear();

    floodReach();
    claimed.assign(MAP_L * MAP_U, false);

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
    cand.reserve(resources.size());

    for (const auto& it : resources)
    {
        const tagResource* r = it.second;
        const ResKind k = kindOf(r->Type);
        if (k == RK_COUNT) continue;
        if (k == RK_CORPSE)
        {
            if (r->Blood > 0) continue;
            if (!workerOfSpot.count(r->SN) && nearLion(lionsPos, r->BlockDR, r->BlockUR, LION_KEEP)) continue;
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
    for (const Cand& c : cand)
    {
        Pos stand;
        if (!standCell(c.r, stand)) continue;
        claimed[stand.dr * MAP_U + stand.ur] = true;

        GatherSpot s;
        s.sn = c.r->SN;
        s.at = FloatPos(c.r->DR, c.r->UR);
        s.stand = stand;
        s.cost = c.cost;
        pools[c.k].spots.push_back(s);
        alive.insert(c.r->SN);
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

void Mgr::dropSpot(int workerSN, bool toFree)
{
    auto it = spotOfWorker.find(workerSN);
    if (it == spotOfWorker.end()) return;
    workerOfSpot.erase(it->second);
    spotOfWorker.erase(it);
    if (toFree && farmers.count(workerSN)) freeFarmers.push_back(workerSN);
}

int Mgr::poolRoom(ResKind k) const { return (int)pools[k].spots.size() - pools[k].desired; }

int Mgr::spotsWithin(ResKind k, double limit) const
{
    int cnt = 0;
    for (const GatherSpot& s : pools[k].spots)
        if (s.cost <= limit) cnt++;
        else break;
    return cnt;
}

void Mgr::buildSaveMap()
{
    saveMap.assign(MAP_L * MAP_U, 0);
    bestSave = 0;

    const int size = buildingSize(BUILDING_STOCK);
    const double off = (size - 1) * 0.5;  // 左上角到占地中心

    for (const ResKind k : {RK_WOOD /*, RK_GOLD*/, RK_CORPSE})
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
        if (pick.size() > STOCK_SERVE) pick.resize(STOCK_SERVE);

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
                    const FloatPos c((i + off) * BLOCKSIDELENGTH, (j + off) * BLOCKSIDELENGTH);
                    const double gain = s.cost - dis(s.at, c);
                    if (gain <= 0) continue;
                    saveMap[i * MAP_U + j] += trips * 2 * gain / HUMAN_SPEED / discount;
                }
        }
    }

    buildPlaceMask(BUILDING_STOCK, size);
    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
            if (placeOk[i * MAP_U + j] && saveMap[i * MAP_U + j] > bestSave) bestSave = saveMap[i * MAP_U + j];
}

void Mgr::wantStock(int priority)
{
    const int have = cntBuilding(BUILDING_STOCK);
    if (have >= STOCK_MAX) return;

    for (int sn : buildingsByType[BUILDING_STOCK])
        if (building(sn)->Percent < 100) return;

    const double cost = 100 / FARMER_CONSTRUCTSPEED + BUILD_STOCK_WOOD / FARMER_GATHERSPEED_WOOD;

    if (bestSave < cost * STOCK_PAYBACK) return;
    wantBuilding(BUILDING_STOCK, have + 1, priority);
}

void Mgr::toPool(int& from, int num, ResKind k, double limit)
{
    int room = poolRoom(k);
    if (limit >= 0) room = min(room, spotsWithin(k, limit) - pools[k].desired);
    const int t = min(max(0, min(from, num)), max(0, room));
    from -= t;
    pools[k].desired += t;
}

void Mgr::runGather()
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
                const int sn = takeNearestFree(FloatPos(s.stand), false);
                if (sn < 0) continue;
                workerOfSpot[s.sn] = sn;
                spotOfWorker[sn] = s.sn;
            }
            const int sn = workerOfSpot[s.sn];
            sendAction(sn, s.sn);
        }
    }
}

void Mgr::sendAction(int workerSN, int targetSN)
{
    const tagFarmer* f = farmer(workerSN);
    if (f->WorkObjectSN == targetSN && f->NowState != HUMAN_STATE_IDLE) return;
    HumanAction(workerSN, targetSN);
}

void Mgr::destroyLater()
{
    while (destroyCnt-- > 0 && farmers.size())
    {
        int sn = farmers.begin()->first;
        HumanAction(sn, sn);
        farmers.erase(sn);
    }
}

bool Mgr::techAvailable(int action)
{
    switch (action)
    {
        case BUILDING_CENTER_UPGRADE: return civStage == CIVILIZATION_TOOLAGE && cntBuilding(BUILDING_MARKET, true);
        case BUILDING_MARKET_WHEEL_UPGRADE: return cntBuilding(BUILDING_MARKET) && civStage == CIVILIZATION_BRONZEAGE;
        default: return true;
    }
}

bool Mgr::buildAvailable(int type)
{
    switch (type)
    {
    case BUILDING_FARM : return cntBuilding(BUILDING_MARKET, true);
    default: return true;
    }
}

bool Mgr::walkable(int dr, int ur) const
{
    if (dr < 0 || ur < 0 || dr >= MAP_L || ur >= MAP_U) return false;
    const int t = cell(dr, ur).type;
    if (t != MAPPATTERN_GRASS && t != MAPPATTERN_DESERT) return false;
    return !blockCell[dr * MAP_U + ur];
}

void Mgr::buildThreat()
{
    threatMap.assign(MAP_L * MAP_U, 0);

    // thTable[r][(dx+r)*(2r+1)+(dy+r)] = r - floor(sqrt(dx * dx + dy * dy)) + 1
    static std::vector<int> thTable[17];

    auto stamp = [&](int td, int tu, int r)
    {
        if (r < 0 || r >= (int)(sizeof(thTable) / sizeof(thTable[0]))) return;
        const int s = 2 * r + 1;
        if (thTable[r].empty())
        {
            thTable[r].assign(s * s, 0);
            for (int dx = -r; dx <= r; dx++)
                for (int dy = -r; dy <= r; dy++)
                {
                    double d = std::sqrt(dx * dx + dy * dy);
                    if (d <= r + EPS) thTable[r][(dx + r) * s + (dy + r)] = int(r - d + 1);
                }
        }

        const std::vector<int>& tbl = thTable[r];
        const int lo_dr = std::max(0, td - r), hi_dr = std::min(MAP_L - 1, td + r);
        const int lo_ur = std::max(0, tu - r), hi_ur = std::min(MAP_U - 1, tu + r);
        for (int i = lo_dr; i <= hi_dr; i++)
            for (int j = lo_ur; j <= hi_ur; j++)
            {
                int val = tbl[(i - td + r) * s + (j - tu + r)];
                if (val > 0) threatMap[i * MAP_U + j] += val;
            }
    };

    for (const Pos& l : lionsPos) stamp(l.dr, l.ur, LION_KEEP);

    for (const auto& it : enemyArmies)
    {
        const tagArmy& e = *it.second;
        const int r = lockOf(e.SN) >= 0 ? atkRange(e.Sort) + PRIEST_MARGIN : ENEMY_KEEP;
        stamp(e.BlockDR, e.BlockUR, r);
    }

    for (const auto& it : enemyBuildings)
        if (it.second->Type == BUILDING_ARROWTOWER) stamp(it.second->BlockDR, it.second->BlockUR, ENEMY_KEEP);
}

bool Mgr::enemyCorner(int dr, int ur) const
{
    const bool sameD = (dr < MAP_L / 2) == (basePos.dr < MAP_L / 2);
    const bool sameU = (ur < MAP_U / 2) == (basePos.ur < MAP_U / 2);
    return !sameD && !sameU;
}

int Mgr::threatAt(int dr, int ur) const
{
    if (threatMap.empty()) return 0;
    if (dr < 0 || ur < 0 || dr >= MAP_L || ur >= MAP_U) return 0;
    return threatMap[dr * MAP_U + ur];
}

void Mgr::floodThreat(const Pos& from, bool avoidThreat)
{
    scoutDist.assign(MAP_L * MAP_U, -1);
    scoutPrev.assign(MAP_L * MAP_U, -1);
    if (from.dr < 0 || from.dr >= MAP_L || from.ur < 0 || from.ur >= MAP_U) return;

    std::queue<Pos> q;
    scoutDist[from.dr * MAP_U + from.ur] = 0;
    q.push(from);

    while (q.size())
    {
        const Pos c = q.front();
        q.pop();
        for (int d = 0; d < 8; d++)
        {
            const Pos n = {c.dr + dx[d], c.ur + dy[d]};
            if (!walkable(n.dr, n.ur)) continue;
            if (scoutDist[n.dr * MAP_U + n.ur] >= 0) continue;  // used
            if (avoidThreat && threatAt(n.dr, n.ur) > 0) continue;
            if (dx[d] && dy[d] && (!walkable(c.dr + dx[d], c.ur) || !walkable(c.dr, c.ur + dy[d])))  // 对角
                continue;
            scoutDist[n.dr * MAP_U + n.ur] = scoutDist[c.dr * MAP_U + c.ur] + 1;
            scoutPrev[n.dr * MAP_U + n.ur] = c.dr * MAP_U + c.ur;
            q.push(n);
        }
    }
}

void Mgr::buildUnknown()
{
    // unknownRow[i * (MAP_U + 1) + j] = 第 i 行前 j 格里的未知格数
    const int W = MAP_U + 1;
    unknownRow.assign(MAP_L * W, 0);
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            unknownRow[i * W + j + 1] = unknownRow[i * W + j] + (cell(i, j).type == MAPPATTERN_UNKNOWN);
}

int Mgr::wpGain(const Pos& c) const
{
    const int W = MAP_U + 1;
    const int r = SCOUT_VIEW;
    int sum = 0;
    for (int ddr = -r; ddr <= r; ddr++)
    {
        const int i = c.dr + ddr;
        if (i < 0 || i >= MAP_L) continue;
        const int half = (int)std::sqrt((double)(r * r - ddr * ddr));
        const int lo = max(0, c.ur - half), hi = min(MAP_U - 1, c.ur + half);
        if (lo > hi) continue;
        sum += unknownRow[i * W + hi + 1] - unknownRow[i * W + lo];
    }
    return sum;
}

bool Mgr::nearestStand(const Pos& c, int r, Pos& out) const
{
    double best = 0;
    out = {-1, -1};
    for (int i = max(0, c.dr - r); i <= min(MAP_L - 1, c.dr + r); i++)
        for (int j = max(0, c.ur - r); j <= min(MAP_U - 1, c.ur + r); j++)
        {
            if (scoutDist[i * MAP_U + j] < 0 || !walkable(i, j)) continue;
            const double d = disSq({i, j}, c);
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

            const Pos w(min(i * SCOUT_VIEW, MAP_L - 1), min(j * SCOUT_VIEW, MAP_U - 1));
            if (enemyCorner(w.dr, w.ur)) continue;

            Pos st;
            if (!nearestStand(w, 5, st)) continue;  // 这一帧到不了

            if (wpGain(st) < SCOUT_MIN_GAIN) continue;

            const int step = scoutDist[st.dr * MAP_U + st.ur];
            if (bestIdx >= 0 && step >= bestStep) continue;
            bestStep = step;
            bestIdx = idx;
            stand = st;
        }
    return bestIdx;
}

int Mgr::homeETA(const Pos& here)
{
    if (anchor.dr == -1 || gameFrame - lastAnchorChanged > 250)
    {
        double bestFar = -1;
        for (const auto& it : buildings)
        {
            if (it.second->Type != BUILDING_ARROWTOWER) continue;
            const Pos tp{it.second->BlockDR, it.second->BlockUR};
            const double d = disSq(tp, basePos);
            if (d > bestFar)
            {
                bestFar = d;
                anchor = tp;
            }
        }
        if (anchor.dr == -1) anchor = basePos;
    }

    if (nearestStand(anchor, 4, home)) return scoutDist[home.dr * MAP_U + home.ur] * 25;
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
    if (goal.dr < 0 || scoutDist[goal.dr * MAP_U + goal.ur] < 0) return;

    for (Pos c = goal;;)
    {
        route.push_back(c);
        const int p = scoutPrev[c.dr * MAP_U + c.ur];
        if (p < 0) break;
        c = {p / MAP_U, p % MAP_U};
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

    const int endCell = route[end].dr * MAP_U + route[end].ur;
    if (endCell == routeSent)
    {
        if (!idle) return true;
        route.clear();
        return false;
    }

    routeSent = endCell;
    HumanMove(scoutSN, ((double)route[end].dr + 0.5) * BLOCKSIDELENGTH,
              ((double)route[end].ur + 0.5) * BLOCKSIDELENGTH);
    return true;
}

Pos Mgr::fleeGoal() const
{
    Pos best = {-1, -1};
    int bestThreat = 0, bestStep = 0;
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
        {
            const int step = scoutDist[i * MAP_U + j];
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

void Mgr::scout()
{
    if (priestAllIn) return;

    const tagArmy& u = *army(scoutSN);
    const Pos here = {u.BlockDR, u.BlockUR};
    const FloatPos Fhere = {u.DR, u.UR};
    const bool idle = u.NowState == HUMAN_STATE_IDLE;
    const int wp = MAP_L / SCOUT_VIEW + 1;
    if (wpCooldown.empty())
    {
        wpCooldown.assign(wp * wp, 0);
        wpDone.assign(wp * wp, false);
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
        if (!arrived && dis(Fhere, home) < 5 * BLOCKSIDELENGTH) arrived = true;
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
        const bool arrived = dis(Fhere, FloatPos(goalStand)) <= 2 * BLOCKSIDELENGTH;
        const bool ok = scoutDist[goalStand.dr * MAP_U + goalStand.ur] >= 0 && wpGain(goalStand) >= SCOUT_MIN_GAIN;
        if (arrived) wpDone[goalWp] = true;
        if (arrived || !ok)
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

Stock Mgr::actionCost(int action) const
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

int Mgr::atkRange(int sort) const
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

double Mgr::dpsOf(const tagArmy& e) const
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

int Mgr::lockOf(int enemySN) const
{
    const tagArmy* e = get(enemyArmies, enemySN);
    if (!e) return -1;

    const int sn = e->WorkObjectSN;
    if (sn == priestSN) return -1;  // 锁着祭司不算有诱饵
    if (ally.count(sn)) return sn;

    const tagBuilding* b = building(sn);
    return b && b->Type == BUILDING_ARROWTOWER ? sn : -1;
}

void Mgr::defence()
{
    hostiles.clear();
    towerAtk.clear();
    for (const auto& it : enemyArmies)
    {
        if (dis({it.second->DR, it.second->UR}, baseFloatPos) < 65 * BLOCKSIDELENGTH) towerAtk.push_back(it.first);
        if (dis({it.second->DR, it.second->UR}, baseFloatPos) < 40 * BLOCKSIDELENGTH) hostiles.push_back(it.first);
    }

    if (gameFrame < 25 * 60 * 15) fixTower();

    inCombat = hostiles.size() > 0;
    if (!inCombat)
    {
        priestTarget = -1;
        return;
    }

    std::sort(hostiles.begin(), hostiles.end());  // 引擎每帧打乱列表, 定序才能稳定分配

    runTower();
    runPriest();
    runArmy();
}

void Mgr::fixTower()
{
    if (stock.stone <= 0)
    {
        for (int sn : fixCrew) freeFarmers.push_back(sn);
        fixCrew.clear();
        return;
    }

    const tagBuilding* tar = nullptr;
    for (int sn : buildingsByType[BUILDING_ARROWTOWER])
    {
        const tagBuilding* t = building(sn);
        if (t->Percent < 100 || t->Blood >= t->MaxBlood) continue;
        if (!tar || t->SN < tar->SN) tar = t;
    }

    for (auto it = fixCrew.begin(); it != fixCrew.end();)
        if (farmers.count(*it)) it++;
        else it = fixCrew.erase(it);

    if (!tar)
    {
        for (int sn : fixCrew) freeFarmers.push_back(sn);
        fixCrew.clear();
        return;
    }

    while (fixCrew.size() < FIX_CREW)
    {
        const int sn = takeNearestFree(FloatPos(Pos(tar->BlockDR, tar->BlockUR)), true);
        if (sn < 0) break;
        fixCrew.insert(sn);
    }

    for (int sn : fixCrew) sendAction(sn, tar->SN);
}

void Mgr::runTower()
{
    for (int sn : buildingsByType[BUILDING_ARROWTOWER])
    {
        const tagBuilding* t = building(sn);
        if (t->Percent < 100) continue;

        // 优先点还没锁住诱饵的, 都锁住了就打伤害最高的那个
        int pick = -1;
        double bestDps = 0;
        for (int e : towerAtk)
        {
            if (lockOf(e) >= 0) continue;
            const double d = dpsOf(*enemyArmies[e]);
            if (pick < 0 || d > bestDps)
            {
                bestDps = d;
                pick = e;
            }
        }
        if (pick < 0)
            for (int e : hostiles)
            {
                const double d = dpsOf(*enemyArmies[e]);
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
        const double da = dpsOf(*enemyArmies[a]), db = dpsOf(*enemyArmies[b]);
        return da != db ? da > db : a < b;
    });

    for (const auto& it : armies)
    {
        const tagArmy& u = *it.second;
        if (u.SN == priestSN || u.Sort == AT_STONE_THROWER || u.Sort == AT_CHARIOT_ARCHER) continue;

        if (enemyArmies.count(u.WorkObjectSN) && get(enemyArmies, u.WorkObjectSN)->Sort != AT_STONE_THROWER &&
            u.WorkObjectSN != priestTarget)
            continue;

        for (int e : hostiles)
            if (e != priestTarget && get(enemyArmies, e)->Sort != AT_STONE_THROWER)
            {
                HumanAction(u.SN, e);
                break;
            }
    }
}

void Mgr::runPriest()
{
    if (priestAllIn) return;
    const tagArmy& p = *army(priestSN);
    auto convertible = [&](int e) { return enemyArmies.count(e) && lockOf(e) >= 0; };

    auto t = get(enemyArmies, p.WorkObjectSN);
    if (t != nullptr && convertible(t->SN) && t->Sort == AT_STONE_THROWER) return;

    bool isS = false;
    int pick = -1;
    double bestDps = 0;
    for (int e : hostiles)
    {
        if (lockOf(e) < 0) continue;
        double d = dpsOf(*enemyArmies[e]);
        if (enemyArmies[e]->Sort == AT_STONE_THROWER) d += 10000;
        if (pick < 0 || d > bestDps)
        {
            bestDps = d;
            pick = e;
        }
    }

    t = get(enemyArmies, pick);
    if (t && t->Sort == AT_STONE_THROWER) isS = true;

    if (isS)
    {
        priestTarget = pick;
        HumanAction(priestSN, pick);
    }
    else if (convertible(p.WorkObjectSN)) { priestTarget = p.WorkObjectSN; }
    else if (pick != -1)
    {
        priestTarget = pick;
        HumanAction(priestSN, pick);
    }
}

void Mgr::attack()
{
    if (!armyAllIn) return;

    if (siegeSN == -1)
        for (const auto& it : enemyBuildings)
            if (it.second->Type == BUILDING_SIEGE)
            {
                siegeSN = it.first;
                break;
            }

    if (final.dr == -1 && basePos.dr >= 0)
    {
        final.dr = (basePos.dr * 2 / MAP_L) ? 0 : MAP_L - 1;
        final.ur = (basePos.ur * 2 / MAP_U) ? 0 : MAP_U - 1;
    }

    fill(bfsUsed.begin(), bfsUsed.end(), false);
    std::queue<Pos> q;
    const int stride = 5;
    const int sectorsU = (MAP_U + stride - 1) / stride;
    std::vector<bool> sectorTaken(((MAP_L + stride - 1) / stride) * sectorsU, false);
    std::vector<Pos> nextToGo;
    q.push(final);
    bfsUsed[final.dr * MAP_U + final.ur] = true;
    while (q.size())
    {
        auto crt = q.front();
        q.pop();
        if (cell(crt.dr, crt.ur).type != MAPPATTERN_UNKNOWN && walkable(crt.dr, crt.ur) &&
            distMap[crt.dr * MAP_U + crt.ur] >= 0)
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
                Pos next = {crt.dr + dx[d], crt.ur + dy[d]};
                if (next.dr < 0 || next.dr >= MAP_L || next.ur < 0 || next.ur >= MAP_U) continue;
                if (bfsUsed[next.dr * MAP_U + next.ur]) continue;
                if (cell(next.dr, next.ur).type == MAPPATTERN_OCEAN) continue;

                bfsUsed[next.dr * MAP_U + next.ur] = true;
                q.push(next);
            }
        }
    }

    nextToGo.erase(remove_if(nextToGo.begin(), nextToGo.end(), [&](const Pos& p) {
        return angle({final.dr - basePos.dr, final.ur - basePos.ur}, {p.dr - basePos.dr, p.ur - basePos.ur}) > 15;
    }), nextToGo.end());

    std::sort(nextToGo.begin(), nextToGo.end(),
              [&](const Pos& a, const Pos& b) { return distMap[a.dr * MAP_U + a.ur] > distMap[b.dr * MAP_U + b.ur]; });
    const size_t frontierK = std::max<size_t>(6, armies.size());
    if (nextToGo.size() > frontierK) nextToGo.resize(frontierK);

    if (watchPoint.dr == -1) watchPoint = nextToGo.back();
    if (priestAllIn && !been)
    {
        been = true;
        HumanMove(priestSN, (0.5 + watchPoint.dr) * BLOCKSIDELENGTH, (0.5 + watchPoint.ur) * BLOCKSIDELENGTH);
    }

    // 可打的目标: 视野内的敌方军队 + 已探到的敌方建筑, 去掉攻城厂
    std::vector<int> targets;
    for (const auto& it : enemyArmies)
        if (dis({it.second->DR, it.second->UR}, FloatPos(final)) <= 50 * BLOCKSIDELENGTH) targets.push_back(it.first);
    for (const auto& it : enemyBuildings)
        if (it.first != siegeSN) targets.push_back(it.first);

    auto posOf = [&](int sn)
    {
        const tagArmy* a = get(enemyArmies, sn);
        if (a) return FloatPos(a->DR, a->UR);
        const tagBuilding* b = get(enemyBuildings, sn);
        return FloatPos(Pos(b->BlockDR, b->BlockUR));
    };

    auto nearestTarget = [&](const FloatPos& from)
    {
        int pick = -1;
        double best = 0;
        for (int t : targets)
        {
            if (t == siegeSN) continue;
            const double d = disSq(posOf(t), from);
            if (pick < 0 || d < best || (d == best && t < pick))
            {
                best = d;
                pick = t;
            }
        }
        return pick;
    };

    // 贪心分发: 每个待派单位选 nextToGo 里离自己最近的未占用格
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
    auto sendTo = [&](int sn, int idx)
    {
        used[idx] = true;
        HumanMove(sn, (0.5 + nextToGo[idx].dr) * BLOCKSIDELENGTH, (0.5 + nextToGo[idx].ur) * BLOCKSIDELENGTH);
    };

    for (const auto& it : armies)
    {
        const tagArmy& u = *it.second;
        if (u.SN == priestSN || (u.Sort != AT_CHARIOT_ARCHER && u.Sort != AT_STONE_THROWER)) continue;

        if (enemyArmies.count(u.WorkObjectSN) || enemyBuildings.count(u.WorkObjectSN)) continue;

        const int t = nearestTarget({u.DR, u.UR});
        if (t >= 0) HumanAction(u.SN, t);
        else if (u.NowState == HUMAN_STATE_IDLE && targets.size() == 0)  // 什么都看不见
        {
            const int idx = pickNearest(u.DR, u.UR);
            if (idx >= 0) sendTo(u.SN, idx);
        }
    }

    if (priestAllIn && siegeSN >= 0 && targets.size() <= 3)
    {
        const tagArmy& p = *army(priestSN);
        if (p.WorkObjectSN != siegeSN) HumanAction(priestSN, siegeSN);
    }
}

void Mgr::killLions()
{
    if (gameFrame < 25 * 4 * 60) return;

    const tagResource* tar = nullptr;
    double best = 0;
    for (const tagResource* l : lions)
    {
        const double d = dis({l->DR, l->UR}, baseFloatPos);
        const double a = angle({final.dr - basePos.dr, final.ur - basePos.ur}, {l->BlockDR - basePos.dr, l->BlockUR - basePos.ur});
        if (d > 40 * BLOCKSIDELENGTH && a > 15) continue;
        if (!tar || d < best || (d == best && l->SN < tar->SN))
        {
            best = d;
            tar = l;
        }
    }

    for (auto it = lionCrew.begin(); it != lionCrew.end();)
        if (farmers.count(*it)) it++;
        else it = lionCrew.erase(it);

    if (!tar)
    {
        for (int sn : lionCrew) freeFarmers.push_back(sn);
        lionCrew.clear();
        return;
    }

    while (lionCrew.size() < LION_CREW)
    {
        const int sn = takeNearestFree({tar->DR, tar->UR}, true);
        if (sn < 0) break;
        lionCrew.insert(sn);
    }

    for (int sn : lionCrew) sendAction(sn, tar->SN);
}

void Mgr::clearRoad()
{
    std::vector<Pos> points;
    fill(bfsUsed.begin(), bfsUsed.end(), false);
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (distMap[i * MAP_U + j] >= 22 && distMap[i * MAP_U + j] <= 25)
            {
                bfsUsed[i * MAP_U + j] = true;
                points.push_back({i, j});
            }

    if (points.empty()) return;

    for (auto& a : armies)
    {
        auto& u = a.second;
        if (bfsUsed[u->BlockDR * MAP_U + u->BlockUR] || u->NowState != HUMAN_STATE_IDLE) continue;

        int ran = rand() % points.size();
        HumanMove(u->SN, (0.5 + points[ran].dr) * BLOCKSIDELENGTH, (0.5 + points[ran].ur) * BLOCKSIDELENGTH);
    }
}

int Mgr::idleHost(int type, const std::set<int>& busy) const
{
    auto it = buildingsByType.find(type);
    if (it == buildingsByType.end()) return -1;
    for (int sn : it->second)
    {
        const tagBuilding* b = building(sn);
        if (b->Percent >= 100 && b->Project == 0 && !busy.count(sn)) return sn;
    }
    return -1;
}

void Mgr::runProduction()
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

        const Stock c = actionCost(action);
        if (!afford(c)) continue;

        BuildingAction(host, action);
        doneTech.insert(action);
        busy.insert(host);
        reserved += c;
        it = prods.erase(it);
    }
}

int Mgr::actionHost(int action)
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

bool Mgr::afford(const Stock& c) const
{
    return stock.wood - reserved.wood >= c.wood && stock.meat - reserved.meat >= c.meat &&
           stock.stone - reserved.stone >= c.stone && stock.gold - reserved.gold >= c.gold;
}

bool Mgr::isBuilder(int sn) const
{
    for (const BuildSite& s : sites)
        if (s.workers.count(sn)) return true;
    return false;
}

void Mgr::runBuild()
{
    // 1. 更新已有工地: 清死人, 解析地基, 撤完工
    for (auto it = sites.begin(); it != sites.end();)
    {
        BuildSite& s = *it;
        for (auto wit = s.workers.begin(); wit != s.workers.end();)
            if (farmers.count(*wit)) ++wit;
            else wit = s.workers.erase(wit);

        if (s.sn < 0)
        {
            for (int sn : buildingsByType[s.type])
            {
                const tagBuilding* b = building(sn);
                if (b->BlockDR != s.site.dr || b->BlockUR != s.site.ur) continue;
                s.sn = sn;
                break;
            }
            if (s.sn < 0)
            {
                if (!s.workers.empty()) failedSpots[s.site.dr * MAP_U + s.site.ur]++;
                for (int sn : s.workers) freeFarmers.push_back(sn);
                it = sites.erase(it);
                continue;
            }
        }

        const tagBuilding* b = building(s.sn);
        if (!b || b->Percent >= 100)
        {
            for (int sn : s.workers) freeFarmers.push_back(sn);
            it = sites.erase(it);
            continue;
        }
        ++it;
    }

    // 2. 每个工地补齐 crew, 派活
    for (BuildSite& s : sites)
    {
        while ((int)s.workers.size() < BUILD_CREW)
        {
            const int sn = takeNearestFree(FloatPos(s.site), true);
            if (sn < 0) break;
            s.workers.insert(sn);
        }
        for (int sn : s.workers) sendAction(sn, s.sn);
    }

    // 3. 从队列开新工地, 累计本帧已用资源避免超支
    Stock used;
    std::vector<Pos> justPlaced;
    for (auto it = builds.end(); it != builds.begin();)
    {
        --it;
        const int type = it->second;

        Stock c;
        c.wood = buildWoodCost(type);
        c.stone = buildStoneCost(type);
        Stock probe = used;
        probe.wood += c.wood;
        probe.stone += c.stone;
        if (stock.wood - reserved.wood < probe.wood) break;  // 优先序: 更贵的钱不够就停
        if (stock.stone - reserved.stone < probe.stone) break;

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

        const int first = takeNearestFree(FloatPos(spot), true);
        if (first < 0) break;  // 没人建了

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

int Mgr::queuedProd(int action) const
{
    const int host = actionHost(action);
    int cnt = 0;
    for (const auto& p : prods)
        if (p.second == action) cnt++;
    auto it = buildingsByType.find(host);
    if (it != buildingsByType.end())
        for (int sn : it->second)
            if (building(sn)->Project == action) cnt++;
    return cnt;
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
    int diff = total - cntBuilding(buildingType) - queuedBuild(buildingType);
    for (; diff > 0; diff--) builds.insert({priority, buildingType});
}

void Mgr::wantUnit(int type, int total, int priority)
{
    const int action = typeToAction(type);

    int diff = total - cntUnit(type) - queuedProd(action);
    for (; diff > 0; diff--) prods.insert({priority, action});
    if (diff < 0) destroyCnt = -diff;
}

void Mgr::wantTech(int action, int priority)
{
    if (techAvailable(action) && !doneTech.count(action)) prods.insert({priority, action});
}

int Mgr::typeToAction(int type)
{
    switch (type)
    {
        case -1: return BUILDING_CENTER_CREATEFARMER;
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

void Mgr::rateOf(double out[2]) const
{
    out[0] = (stock.meat - prevStock.meat) / dt;
    out[1] = (stock.wood - prevStock.wood) / dt;
}

Stock Mgr::computeDemand()
{
    Stock need;
    for (const auto& p : builds)
    {
        need.wood += buildWoodCost(p.second);
        need.stone += buildStoneCost(p.second);
    }
    for (const auto& p : prods)
    {
        const Stock c = actionCost(p.second);
        need.meat += c.meat;
        need.wood += c.wood;
        need.gold += c.gold;
        need.stone += c.stone;
    }
    return need;
}

void Mgr::rebalancePop(int population, const Stock& need)
{
    const int stoneWant = (PhaseChanged && stock.stone < 600) ? 2 : 0;
    poolPop[2] = min(stoneWant, population);
    const int balancePop = max(0, population - poolPop[2]);

    if (!PhaseChanged && civStage == CIVILIZATION_TOOLAGE)
    {
        poolPop[0] = balancePop * 3 / 5;
        poolPop[1] = balancePop - poolPop[0];
        return;
    }
    else if (!PhaseChanged && civStage == CIVILIZATION_BRONZEAGE)
    {
        PhaseChanged = true;
        lastSnapFrame = gameFrame;  // 进入新阶段, 从下次 60s 才平衡
        prevStock = stock;
    }

    int sumFW = poolPop[0] + poolPop[1];
    if (sumFW < balancePop) poolPop[1] += balancePop - sumFW;  // 默认进树木
    else if (sumFW > balancePop)
    {
        int excess = sumFW - balancePop;
        while (excess > 0)
        {
            if (poolPop[0] >= poolPop[1] && poolPop[0] > 0)
            {
                poolPop[0]--;
                excess--;
            }
            else if (poolPop[1] > 0)
            {
                poolPop[1]--;
                excess--;
            }
            else break;
        }
    }

    if (!PhaseChanged || gameFrame - lastSnapFrame < dt || balancePop == 0) return;

    double rate[2];
    rateOf(rate);

    // 每人每分钟多少资源
    const double per0 = poolPop[0] > 0 ? rate[0] / poolPop[0] : 0.0;
    const double per1 = poolPop[1] > 0 ? rate[1] / poolPop[1] : 0.0;

    // 缺口
    const double gap0 = max(0.0, (double)need.meat - stock.meat);
    const double gap1 = max(0.0, (double)need.wood - stock.wood);

    lastSnapFrame = gameFrame;

    // 缺口补全(任意比例不超过4 : 1)
    if (per0 < EPS || per1 < EPS || (gap0 < EPS && gap1 < EPS))
    {
        poolPop[0] = balancePop / 2;
        poolPop[1] = balancePop - poolPop[0];
    }
    else if (gap0 < EPS)
    {
        poolPop[0] = balancePop / 5;
        poolPop[1] = balancePop - poolPop[0];
    }
    else if (gap1 < EPS)
    {
        poolPop[1] = balancePop / 5;
        poolPop[0] = balancePop - poolPop[1];
    }
    else
    {
        double ratio = gap0 * per1 / gap1 / per0;
        ratio = min(ratio, 4.0);
        ratio = max(ratio, 0.25);

        poolPop[0] = ratio / (1.0 + ratio) * balancePop;
        poolPop[1] = balancePop - poolPop[0];
    }

    lastSnapFrame = gameFrame;
    prevStock = stock;
}
