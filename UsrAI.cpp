#include "UsrAI.h"

using namespace std;
tagGame tagUsrGame;
ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/

static const int dx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
static const int dy[8] = {1, 0, -1, 0, 1, -1, -1, 1};

Mgr mgr;

static long long hashKey(int type, int dr, int ur) // 哈希
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
        case BUILDING_RANGE: return BUILD_RANGE_WOOD;
        default: return 0;
    }
}

ResKind kindOf(int resourceType)
{
    switch (resourceType)
    {
        case RESOURCE_TREE: return RK_WOOD;
        case RESOURCE_GOLD: return RK_GOLD;
        case RESOURCE_BUSH: return RK_BUSH;
        case RESOURCE_GAZELLE: return RK_GAZELLE;
        default: return RK_COUNT;
    }
}

Stock actionCost(int action)
{
    Stock c;
    switch (action)
    {
        case BUILDING_CENTER_CREATEFARMER: c.meat = BUILDING_CENTER_CREATEFARMER_FOOD; break;
        case BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN:
            c.meat = BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_FOOD;
            c.gold = BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_GOLD;
            break;
        case BUILDING_CENTER_UPGRADE: c.meat = BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD; break;
        case BUILDING_RANGE_UPGRADE_COMPOSITE_BOW:
            c.meat = BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_FOOD;
            c.wood = BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD;
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
        case BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN:
        case BUILDING_RANGE_UPGRADE_COMPOSITE_BOW: return BUILDING_RANGE;
        default: return -1;
    }
}

int typeToAction(int type)
{
    switch (type)
    {
        case AT_FARMER: return BUILDING_CENTER_CREATEFARMER;
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

bool Mgr::enemyCorner(int dr, int ur) const
{
    const bool sameD = (dr < MAP_L / 2) == (base.dr < MAP_L / 2);
    const bool sameU = (ur < MAP_U / 2) == (base.ur < MAP_U / 2);
    return !sameD && !sameU;
}

int Mgr::lockOf(int enemySN) const
{
    const tagArmy* e = enemyArmy(enemySN);
    if (!e || e->WorkObjectSN == priest) return -1;

    const int sn = e->WorkObjectSN;
    return farmer(sn) || army(sn) || building(sn) ? sn : -1;
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
    byType.clear();
    unitCnt.assign(32, 0);
    bldCnt.assign(32, 0);
    bldDoneCnt.assign(32, 0);
    blockCell.assign((size_t)MAP_L * MAP_U, 0);
    held = Stock();

    gameFrame = info.GameFrame;

    // 构建反查
    for (const auto& f : info.farmers)
    {
        farmerMap[f.SN] = &f;
        unitCnt[AT_FARMER + 1]++;
    }

    for (const auto& a : info.armies)
    {
        armyMap[a.SN] = &a;
        unitCnt[a.Sort + 1]++;

        if (priest == -1 && a.Sort == AT_PRIEST) priest = a.SN;
    }

    for (const auto& a : info.enemy_armies) eArmyMap[a.SN] = &a;

    // 更新信息
    res.wood = info.Wood;
    res.meat = info.Meat;
    res.stone = info.Stone;
    res.gold = info.Gold;
    stage = info.civilizationStage;
    theMap = info.theMap;

    for (const auto& r : info.resources)
    {
        resourceMap[r.SN] = &r;

        if (r.Type == RESOURCE_GAZELLE && r.Blood > 0) continue;

        const int len = resourceSize(r.Type);
        const Pos anchor = resourceCell(&r);
        for (int a = anchor.dr; a < anchor.dr + len; a++)
            for (int b = anchor.ur; b < anchor.ur + len; b++)
                if (inMap(a, b)) blockCell[cellIdx(a, b)] = 1;  // 2x2 资源贴边时 anchor 会越界
    }

    for (const auto& b : info.buildings)
    {
        buildingMap[b.SN] = &b;
        bldCnt[b.Type]++;
        if (b.Percent >= 100) bldDoneCnt[b.Type]++;
        byType[b.Type].push_back(b.SN);
        mark(b);

        if (base.dr == -1 && b.Type == BUILDING_CENTER)
        {
            base.dr = b.BlockDR, base.ur = b.BlockUR;
            baseF = FloatPos(base);
        }
    }

    for (const auto& b : info.enemy_buildings)
    {
        eBuildingMap[b.SN] = &b;
        mark(b);
    }

    fieldBuild(nav, base, buildingSize(BUILDING_CENTER));  // 子系统构建
    gatherFrame();
    buildFrame();
    farmFrame();
    prodFrame();
}

// 现在只剩 nav 一个使用者: 采集落脚点、建筑选址、后撤方向都靠它。
// 探图与行军的路径已经全部交给引擎, 不再需要威胁场和进攻场。
void Mgr::fieldBuild(std::vector<int>& out, const Pos& src, int size)
{
    out.assign((size_t)MAP_L * MAP_U, -1);
    if (!inMap(src.dr, src.ur)) return;

    std::queue<Pos> q;
    for (int i = src.dr; i < src.dr + size; i++)
        for (int j = src.ur; j < src.ur + size; j++)
            if (inMap(i, j))
            {
                out[cellIdx(i, j)] = 0;
                q.push({i, j});
            }

    while (!q.empty())
    {
        const Pos c = q.front();
        q.pop();
        const int nd = out[cellIdx(c.dr, c.ur)] + 1;

        for (int k = 0; k < 8; k++)
        {
            const Pos n = {c.dr + dx[k], c.ur + dy[k]};
            if (!walkable(n.dr, n.ur) || out[cellIdx(n.dr, n.ur)] >= 0) continue;
            if (dx[k] && dy[k] && (!walkable(c.dr + dx[k], c.ur) || !walkable(c.dr, c.ur + dy[k]))) continue;

            out[cellIdx(n.dr, n.ur)] = nd;
            q.push(n);
        }
    }
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

int Mgr::threatAt(int dr, int ur) const
{
    if (!inMap(dr, ur)) return 0;

    int sum = 0;
    auto add = [&](int td, int tu, int r)
    {
        const int dd = dr - td, du = ur - tu;
        const double d = std::sqrt((double)(dd * dd + du * du));
        if (d <= r + EPS) sum += int(r - d + 1);
    };

    for (const auto& it : eArmyMap) add(it.second->BlockDR, it.second->BlockUR, ENEMY_KEEP);
    for (const auto& it : eBuildingMap)
        if (it.second->Type == BUILDING_ARROWTOWER) add(it.second->BlockDR, it.second->BlockUR, ENEMY_KEEP);

    return sum;
}

void Mgr::sendAction(int workerSN, int targetSN)
{
    const tagFarmer* f = farmer(workerSN);
    if (!f) return;
    if (f->WorkObjectSN == targetSN && f->NowState != HUMAN_STATE_IDLE) return;
    HumanAction(workerSN, targetSN);
}

void Mgr::laborFrame()
{
    laborPool.clear();
    for (const auto& it : farmerMap)
        if (!workerBusy(it.first)) laborPool.push_back(it.first);
}

void Mgr::laborRelease()
{
    int farmExcess = (int)farmToWorker.size() - min(farmDesired, (int)farmList.size());
    for (int i = (int)farmList.size() - 1; i >= 0 && farmExcess > 0; i--)
    {
        auto it = farmToWorker.find(farmList[i]);
        if (it == farmToWorker.end()) continue;

        unbind(it);
        farmExcess--;
    }

    for (int k = 0; k < RK_COUNT; k++)
    {
        GatherPool& p = pools[k];
        int assigned = 0;
        for (const GatherSpot& s : p.spots)
            if (workerOfSpot.count(s.sn)) assigned++;

        int excess = assigned - min(p.desired, (int)p.spots.size());
        for (int i = p.spots.size() - 1; i >= 0 && excess > 0; i--)
        {
            auto it = workerOfSpot.find(p.spots[i].sn);
            if (it == workerOfSpot.end()) continue;

            const int sn = it->second;

            // 猎物不撤绑定
            const tagResource* r = resource(p.spots[i].sn);
            const tagFarmer* f = farmer(sn);
            if (r && r->Blood > 0 && f && f->WorkObjectSN == r->SN) continue;

            dropSpot(sn, true);
            excess--;
        }
    }
}

int Mgr::takeNearest(const FloatPos& at, bool steal)
{
    int best = -1;
    double bestDis = 0;

    auto consider = [&](int sn)
    {
        const tagFarmer* f = farmer(sn);
        if (!f) return;
        const double d = dis(at, FloatPos(f->DR, f->UR));
        if (best < 0 || d < bestDis) best = sn, bestDis = d;
    };

    for (int sn : laborPool) consider(sn);

    if (best >= 0)
    {
        auto it = std::find(laborPool.begin(), laborPool.end(), best);
        *it = laborPool.back();
        laborPool.pop_back();
        return best;
    }

    if (!steal) return -1;

    for (const auto& it : farmerMap)
        if (!workerReserved(it.first)) consider(it.first);

    if (best < 0) return -1;
    workerDrop(best);
    return best;
}

void Mgr::freeWorker(int sn)
{
    if (!farmer(sn) || workerBusy(sn)) return;
    if (std::find(laborPool.begin(), laborPool.end(), sn) != laborPool.end()) return;
    laborPool.push_back(sn);
}

int Mgr::targetOf(const std::unordered_map<int, int>& jobs, int workerSN)
{
    for (const auto& it : jobs)
        if (it.second == workerSN) return it.first;
    return -1;
}

bool Mgr::workerBusy(int sn) const { return targetOf(workerOfSpot, sn) >= 0 || workerReserved(sn); }

bool Mgr::workerReserved(int sn) const
{
    if (targetOf(farmToWorker, sn) >= 0 || fixCrew.count(sn)) return true;
    for (const BuildSite& s : sites)
        if (s.workers.count(sn)) return true;
    return false;
}

void Mgr::workerDrop(int sn)
{
    dropSpot(sn, false);
    for (BuildSite& s : sites) s.workers.erase(sn);
    fixCrew.erase(sn);

    const int farm = targetOf(farmToWorker, sn);
    if (farm >= 0) farmToWorker.erase(farm);
}

double Mgr::depotCost(const FloatPos& at, int depotType) const
{
    double best = -1;
    for (const auto& it : buildingMap)
    {
        const tagBuilding& b = *it.second;
        if (b.Percent < 100 || (b.Type != BUILDING_CENTER && b.Type != depotType)) continue;

        const double v = dis(at, centerOf({b.BlockDR, b.BlockUR}, b.Type));
        if (best < 0 || v < best) best = v;
    }
    return best < 0 ? dis(at, baseF) : best;
}

bool Mgr::standCell(const tagResource* r, Pos& out) const
{
    const int size = resourceSize(r->Type);
    const Pos anchor = resourceCell(r);
    const int _dr = anchor.dr, _ur = anchor.ur;

    out = {-1, -1};
    for (int i = _dr - 1; i <= _dr + size; i++)
        for (int j = _ur - 1; j <= _ur + size; j++)
        {
            if (i >= _dr && i < _dr + size && j >= _ur && j < _ur + size) continue;
            if (!inMap(i, j)) continue;
            const int idx = cellIdx(i, j);
            if (nav[idx] == -1 || standTaken[idx] || !walkable(i, j)) continue;

            out = {i, j};
            return true;
        }
    return false;
}

void Mgr::gatherFrame()
{
    for (int k = 0; k < RK_COUNT; k++) pools[k].spots.clear();
    standTaken.assign((size_t)MAP_L * MAP_U, 0);

    // 第一阶段: 只做类型与运距筛选, 不碰落脚格。
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

        auto bind = workerOfSpot.find(r->SN);
        const bool held = bind != workerOfSpot.end() && farmer(bind->second);

        const double cost = depotCost(FloatPos(r->DR, r->UR), k == RK_BUSH ? BUILDING_GRANARY : BUILDING_STOCK);
        cand.push_back({r, k, cost, held});
    }

    // 已经有人在采的排最前, 保证它在抢落脚格时不会输给新岗位。
    std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b)
    {
        if (a.held != b.held) return a.held;
        if (a.cost != b.cost) return a.cost < b.cost;
        return a.r->SN < b.r->SN;
    });

    // 第二阶段: 按上面的确定顺序分配落脚格, 顺带用落脚格判运距。
    std::unordered_set<int> selected;
    selected.reserve(cand.size());

    for (const Cand& x : cand)
    {
        Pos stand;
        if (!standCell(x.r, stand)) continue;  // 没有站立点的

        // 资源格自身被 blockCell 占住, nav 恒为 -1, 必须拿落脚格来判范围
        const int step = nav[cellIdx(stand.dr, stand.ur)];
        if (step < 0 || step > RES_RANGE) continue;

        standTaken[cellIdx(stand.dr, stand.ur)] = 1;

        GatherSpot s;
        s.sn = x.r->SN;
        s.stand = stand;
        s.cost = x.cost;
        s.rate = gatherRate(x.k, x.cost);

        pools[x.k].spots.push_back(s);
        selected.insert(s.sn);
    }

    // 池内统一按运输距离升序, laborRelease 从尾部退人才是退最差的那个。
    for (int k = 0; k < RK_COUNT; k++)
        std::sort(pools[k].spots.begin(), pools[k].spots.end(), [](const GatherSpot& a, const GatherSpot& b)
        { return a.cost != b.cost ? a.cost < b.cost : a.sn < b.sn; });

    // 资源消失、工人死亡或本帧没有落脚点时解除旧绑定; 不额外下令打断, 交给下一帧重新指派。
    for (auto it = workerOfSpot.begin(); it != workerOfSpot.end();)
    {
        if (farmer(it->second) && selected.count(it->first))
        {
            ++it;
            continue;
        }
        it = workerOfSpot.erase(it);
    }
}

void Mgr::dropSpot(int workerSN, bool toFree)
{
    const int spot = targetOf(workerOfSpot, workerSN);
    if (spot < 0) return;
    workerOfSpot.erase(spot);
    if (toFree) freeWorker(workerSN);
}

void Mgr::runGather()
{
    for (int k = 0; k < RK_COUNT; k++)
    {
        GatherPool& p = pools[k];
        int target = min(p.desired, (int)p.spots.size());

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
            assigned++;
            sendAction(sn, s.sn);
        }
    }
}

void Mgr::farmFrame()
{
    farmList.clear();
    for (int sn : buildingsOf(BUILDING_FARM))
    {
        const tagBuilding* b = building(sn);
        if (b && b->Percent >= 100) farmList.push_back(sn);
    }

    for (auto it = farmToWorker.begin(); it != farmToWorker.end();)
    {
        if (farmer(it->second) && std::find(farmList.begin(), farmList.end(), it->first) != farmList.end())
        {
            ++it;
            continue;
        }

        auto cur = it++;
        unbind(cur);
    }
}

void Mgr::unbind(std::unordered_map<int, int>::iterator it)
{
    const int sn = it->second;
    farmToWorker.erase(it);
    freeWorker(sn);
}

void Mgr::runFarm()
{
    const int target = min(farmDesired, (int)farmList.size());

    int assigned = 0;
    for (const auto& it : farmToWorker)
    {
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

        farmToWorker[farmSN] = sn;
        assigned++;
        sendAction(sn, farmSN);
    }
}

int Mgr::econPick(int phase, const int count[E_COUNT], const int cap[E_COUNT]) const
{
    int pick = -1;
    double best = -1.0;
    for (int r = 0; r < E_COUNT; r++)
    {
        const int w = ECON_WEIGHT[phase][r];
        if (w <= 0 || count[r] >= cap[r]) continue;
        const double score = (double)w / (count[r] + 1);
        if (score > best) best = score, pick = r;
    }
    return pick;
}

FoodPlan Mgr::planFood()
{
    for (ResKind k : {RK_GAZELLE, RK_BUSH})
        std::sort(pools[k].spots.begin(), pools[k].spots.end(), [](const GatherSpot& a, const GatherSpot& b)
        { return a.rate != b.rate ? a.rate > b.rate : a.sn < b.sn; });

    std::vector<std::pair<double, int>> farms;
    farms.reserve(farmList.size());
    for (int sn : farmList)
    {
        const tagBuilding* b = building(sn);
        if (!b) continue;

        const FloatPos at = centerOf({b->BlockDR, b->BlockUR}, BUILDING_FARM);
        farms.push_back({gatherRate(RK_COUNT, depotCost(at, BUILDING_GRANARY)), sn});
    }
    std::sort(farms.begin(), farms.end(), [](const auto& a, const auto& b)
    { return a.first != b.first ? a.first > b.first : a.second < b.second; });

    // runFarm 按这个顺序派人, 排在前面的农田先有人
    farmList.clear();
    for (const auto& f : farms) farmList.push_back(f.second);

    struct FoodSlot
    {
        int kind;
        double rate;
    };
    std::vector<FoodSlot> slots_;
    slots_.reserve(pools[RK_GAZELLE].spots.size() + pools[RK_BUSH].spots.size() + farms.size());

    for (const GatherSpot& s : pools[RK_GAZELLE].spots) slots_.push_back({F_CORPSE, s.rate});
    for (const GatherSpot& s : pools[RK_BUSH].spots) slots_.push_back({F_BUSH, s.rate});
    for (const auto& f : farms) slots_.push_back({F_FARM, f.first});

    std::sort(slots_.begin(), slots_.end(), [](const FoodSlot& a, const FoodSlot& b)
    { return a.rate != b.rate ? a.rate > b.rate : a.kind < b.kind; });

    FoodPlan plan;
    plan.jobs.reserve(slots_.size());
    for (const FoodSlot& s : slots_) plan.jobs.push_back(s.kind);

    return plan;
}

bool Mgr::takeFood(FoodPlan& plan)
{
    if (plan.cursor >= (int)plan.jobs.size()) return false;

    const int kind = plan.jobs[plan.cursor++];
    if (kind == F_CORPSE) pools[RK_GAZELLE].desired++;
    else if (kind == F_BUSH) pools[RK_BUSH].desired++;
    else farmDesired++;

    return true;
}

void Mgr::econPlan(int phase)
{
    for (int k = 0; k < RK_COUNT; k++) pools[k].desired = 0;
    farmDesired = wantFarm = 0;

    // 按工地上实际站着的人预留, 而不是按 CREW_BUILD * 工地数.
    // 后者会在开局把 8 个村民全部预扣掉, 使所有 desired 停在 0。
    int reserved = (int)fixCrew.size();
    for (const BuildSite& s : sites) reserved += (int)s.workers.size();
    reserved = min(reserved, (int)farmerMap.size() / 2);  // 建造最多占用一半人口

    const int pop = max(0, (int)farmerMap.size() - reserved);
    if (pop <= 0) return;

    FoodPlan food = planFood();

    const int currentCap[E_COUNT] = {(int)pools[RK_WOOD].spots.size(), (int)food.jobs.size(),
                                     (int)pools[RK_GOLD].spots.size()};

    // 有市场时食物按比例先超额算, 超出现有岗位的部分就是该开农田的信号
    const int planCap[E_COUNT] = {currentCap[E_WOOD], buildAvailable(BUILDING_FARM) ? pop : currentCap[E_FOOD],
                                  currentCap[E_GOLD]};

    // 先按阶段比例生成战略目标。容量不足或 0 权重资源都不会被硬塞人口。
    int raw[E_COUNT] = {};
    for (int n = 0; n < pop; n++)
    {
        const int r = econPick(phase, raw, planCap);
        if (r < 0) break;
        raw[r]++;
    }

    pools[RK_WOOD].desired = min(raw[E_WOOD], currentCap[E_WOOD]);
    pools[RK_GOLD].desired = min(raw[E_GOLD], currentCap[E_GOLD]);

    // 一次只开一块农田, 等上一块封顶再开下一块
    const bool farmPending =
        buildingCount(BUILDING_FARM) != buildingCount(BUILDING_FARM, true) || queuedBuild(BUILDING_FARM) > 0;
    if (raw[E_FOOD] > currentCap[E_FOOD] && !farmPending) wantFarm = 1;

    const int foodNow = min(raw[E_FOOD], currentCap[E_FOOD]);
    for (int n = 0; n < foodNow; n++) takeFood(food);

    int now[E_COUNT] = {pools[RK_WOOD].desired, foodNow, pools[RK_GOLD].desired};
    int assigned = now[E_WOOD] + now[E_FOOD] + now[E_GOLD];

    // 食物岗位不够装下战略目标时, 余下的人在本阶段非零权重资源之间补位。
    while (assigned < pop)
    {
        const int r = econPick(phase, now, currentCap);
        if (r < 0) break;

        if (r == E_FOOD)
        {
            if (!takeFood(food)) break;
        }
        else pools[r == E_WOOD ? RK_WOOD : RK_GOLD].desired++;

        now[r]++;
        assigned++;
    }
}

bool Mgr::buildAvailable(int type) const
{
    switch (type)
    {
        case BUILDING_RANGE: return buildingCount(BUILDING_ARMYCAMP, true) > 0;
        case BUILDING_FARM: return buildingCount(BUILDING_MARKET, true) > 0;
        default: return true;
    }
}

void Mgr::buildFrame()
{
    builds.clear();

    // 智能建造
    granaryPendings.clear();
    depotWant(RK_BUSH, granaryPendings);

    stockPendings.clear();
    depotWant(RK_GAZELLE, stockPendings);
    depotWant(RK_GOLD, stockPendings);

    // 历史农田只有仍在实际耕作时才保留仓储需求
    for (int sn : buildingsOf(BUILDING_FARM))
    {
        const tagBuilding* b = building(sn);
        if (!b) continue;

        const bool planned = b->Percent < 100;
        const bool active = farmToWorker.count(sn) > 0;
        if (!planned && !active) continue;

        const FloatPos at = centerOf({b->BlockDR, b->BlockUR}, BUILDING_FARM);
        if (depotCost(at, BUILDING_GRANARY) <= DEPOT_FAR * BLOCKSIDELENGTH) continue;

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

        if (dis(at, centerOf({b.BlockDR, b.BlockUR}, b.Type)) <= DEPOT_FAR * BLOCKSIDELENGTH) return true;
    }
    return false;
}

double Mgr::depotBenefit(int depotType, const Pos& site) const
{
    const std::vector<Pos>& pending = depotType == BUILDING_GRANARY ? granaryPendings : stockPendings;
    if (pending.empty()) return 0.0;

    const FloatPos candidate = centerOf(site, depotType);

    double saved = 0.0;
    for (const Pos& p : pending)
    {
        const FloatPos at(p);
        saved += max(0.0, depotCost(at, depotType) - dis(at, candidate));
    }
    return saved / BLOCKSIDELENGTH;
}

bool Mgr::depotRoom(const Pos& c) const
{
    for (int a = c.dr - DEPOT_FAR; a <= c.dr + DEPOT_FAR; a++)
        for (int b = c.ur - DEPOT_FAR; b <= c.ur + DEPOT_FAR; b++)
            if (canPlace(a, b, 3) && nav[cellIdx(a, b)] >= 0) return true;
    return false;
}

void Mgr::depotWant(ResKind k, std::vector<Pos>& out) const
{
    const double far_ = DEPOT_FAR * BLOCKSIDELENGTH;
    const int depotType = k == RK_BUSH ? BUILDING_GRANARY : BUILDING_STOCK;

    const GatherSpot* anchor = nullptr;
    for (const GatherSpot& s : pools[k].spots)
    {
        if (!workerOfSpot.count(s.sn)) continue;
        if (s.cost <= far_) continue;
        if (depotCovered(depotType, s.stand) || !depotRoom(s.stand)) continue;
        if (!anchor || s.cost > anchor->cost) anchor = &s;
    }
    if (anchor) out.push_back(anchor->stand);
}

Pos Mgr::findSpot(int type)
{
    std::vector<int> costMap(MAP_L * MAP_U, 0);
    const int size = buildingSize(type);

    auto placeable = [&](int dr, int ur)
    {
        if (!canPlace(dr, ur, size)) return false;
        for (int i = dr; i < dr + size; i++)
            for (int j = ur; j < ur + size; j++)
                if (nav[cellIdx(i, j)] >= 0) return true;
        return false;
    };

    // 通用距离惩罚
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (nav[cellIdx(i, j)] == -1) costMap[cellIdx(i, j)] += MAP_L + MAP_U;
            else costMap[cellIdx(i, j)] += nav[cellIdx(i, j)];

    // 通用靠近建筑惩罚
    for (const auto& it : buildingMap)
        ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, buildingSize(it.second->Type), PLACE_ADJACENT, 0, 1);

    // 通用靠近资源惩罚
    for (const auto& it : resourceMap)
    {
        const tagResource* r = it.second;
        if (r->Type == RESOURCE_GAZELLE && r->Blood > 0) continue;

        const int len = resourceSize(r->Type);
        const Pos anchor = resourceCell(r);
        const int dr = anchor.dr, ur = anchor.ur;

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
                if (it.second->Type == BUILDING_GRANARY || it.second->Type == BUILDING_CENTER)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, 3, PLACE_BONUS, 2, 5);
            }
            break;

        case BUILDING_ARMYCAMP:
        case BUILDING_RANGE:
        case BUILDING_HOME:
        case BUILDING_MARKET:
            for (const auto& it : buildingMap)
            {
                if (it.second->Type == BUILDING_GRANARY || it.second->Type == BUILDING_STOCK ||
                    it.second->Type == BUILDING_CENTER)
                    ringAdd(costMap, {it.second->BlockDR, it.second->BlockUR}, 3, PLACE_ADJACENT, 0, 5);
            }

            for (const auto& it : farmerMap)
            {
                const tagFarmer& f = *(it.second);
                if (targetOf(farmToWorker, f.SN) >= 0) continue;

                ringAdd(costMap, {f.BlockDR, f.BlockUR}, 1, PLACE_BONUS, 0, 6, -PLACE_BONUS / 6);
            }
            break;

        default: break;  // 谷仓/仓库不在
    }

    int area = size * size;
    Pos best = {-1, -1};
    long long bestCost = 0;
    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
        {
            if (!placeable(i, j)) continue;

            long long v = 0;
            for (int a = i; a < i + size; a++)
                for (int b = j; b < j + size; b++) v += costMap[cellIdx(a, b)];
            v /= area;

            if (type == BUILDING_GRANARY || type == BUILDING_STOCK)
            {
                double gain = depotBenefit(type, {i, j});
                bool hasDemand = type == BUILDING_GRANARY ? !granaryPendings.empty() : !stockPendings.empty();
                if (hasDemand && gain <= EPS) continue;
                v -= (long long)(gain * 40);  // 节约一格带来40优势
            }

            auto fit = failedSpots.find(hashKey(type, i, j));
            if (fit != failedSpots.end()) v += (long long)PLACE_FAILED * fit->second;

            if (best.dr < 0 || v < bestCost)
            {
                bestCost = v;
                best = {i, j};
            }
        }
    return best;
}

void Mgr::wantDepot(int depotType, int priority)
{
    bool pending = depotType == BUILDING_GRANARY ? !granaryPendings.empty() : !stockPendings.empty();
    if (!pending || buildingCount(depotType, true) != buildingCount(depotType)) return;
    wantBuilding(depotType, buildingCount(depotType) + 1, priority);
}

int Mgr::queuedBuild(int type) const
{
    int cnt = 0;
    for (const BuildSite& s : sites)
        if (s.type == type && s.sn < 0) cnt++;
    for (const auto& order : builds)
        if (order.second == type) cnt++;
    return cnt;
}

void Mgr::wantBuilding(int buildingType, int total, int priority)
{
    if (!buildAvailable(buildingType)) return;

    const int diff = total - buildingCount(buildingType) - queuedBuild(buildingType);
    for (int i = 0; i < diff; i++) builds.push_back({priority, buildingType});
}

void Mgr::releaseBuilders(BuildSite& s)
{
    const std::set<int> crew = s.workers;
    s.workers.clear();
    for (int sn : crew) freeWorker(sn);
}

void Mgr::runBuild()
{
    for (auto it = sites.begin(); it != sites.end();)  // 维护已登记工地
    {
        BuildSite& s = *it;
        for (auto wit = s.workers.begin(); wit != s.workers.end();)
            if (farmer(*wit)) wit++;
            else wit = s.workers.erase(wit);

        if (s.sn < 0)
        {
            for (int sn : buildingsOf(s.type))
            {
                const tagBuilding* b = building(sn);
                if (b->BlockDR == s.site.dr && b->BlockUR == s.site.ur)
                {
                    s.sn = sn;
                    break;
                }
            }
            if (s.sn < 0 && gameFrame - s.born < BUILD_WAIT)
            {
                it++;
                continue;
            }
            if (s.sn < 0)
            {
                if (!s.workers.empty()) failedSpots[hashKey(s.type, s.site.dr, s.site.ur)]++;

                releaseBuilders(s);
                it = sites.erase(it);
                continue;
            }
        }
        const tagBuilding* b = building(s.sn);
        if (!b || b->Percent >= 100)
        {
            releaseBuilders(s);
            it = sites.erase(it);
            continue;
        }
        it++;
    }

    std::unordered_set<int> owned;
    for (const BuildSite& s : sites)
        if (s.sn >= 0) owned.insert(s.sn);

    for (const auto& it : buildingMap)
    {
        const tagBuilding& b = *it.second;
        if (b.Percent >= 100 || owned.count(b.SN)) continue;

        BuildSite s;
        s.type = b.Type;
        s.site = {b.BlockDR, b.BlockUR};
        s.sn = b.SN;
        s.born = gameFrame;
        sites.push_back(s);
    }

    for (BuildSite& s : sites)
    {
        if (s.sn < 0) continue;

        while ((int)s.workers.size() < CREW_BUILD)
        {
            const int sn = takeNearest(FloatPos(s.site), true);
            if (sn < 0) break;
            s.workers.insert(sn);
        }
        for (int sn : s.workers) sendAction(sn, s.sn);
    }

    std::sort(builds.begin(), builds.end(), [](const auto& a, const auto& b)
    { return a.first != b.first ? a.first > b.first : a.second > b.second; });

    const Stock left = available();
    int usedWood = 0;
    std::unordered_set<int> placed;

    for (const auto& order : builds)
    {
        const int type = order.second;
        const int wood = usedWood + buildWoodCost(type);

        // 高优先级建筑付不起时，不再尝试后面的低优先级建筑
        if (left.wood < wood) break;

        const Pos spot = findSpot(type);
        if (spot.dr < 0) continue;

        const int cell = cellIdx(spot.dr, spot.ur);
        if (placed.count(cell)) continue;

        const int first = takeNearest(FloatPos(spot), true);
        if (first < 0) break;

        BuildSite s;
        s.type = type;
        s.site = spot;
        s.born = gameFrame;
        s.workers.insert(first);
        sites.push_back(s);

        placed.insert(cell);
        usedWood = wood;
        HumanBuild(first, type, spot.dr, spot.ur);
    }
}

int Mgr::projectCount(int action) const
{
    const int host = actionHost(action);
    if (host < 0) return 0;

    int cnt = 0;
    for (int sn : buildingsOf(host))
    {
        const tagBuilding* b = building(sn);
        if (b && b->Project == action) cnt++;
    }
    return cnt;
}

void Mgr::prodFrame()
{
    for (auto it = runningTech.begin(); it != runningTech.end();)
        if (projectCount(*it) > 0) ++it;
        else
        {
            doneTech.insert(*it);
            it = runningTech.erase(it);
        }

    prods.clear();
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
    int cnt = projectCount(action);
    for (const ProdOrder& order : prods)
        if (order.action == action) cnt++;
    return cnt;
}

void Mgr::wantUnit(int type, int total, int priority)
{
    const int action = typeToAction(type);
    const int host = actionHost(action);
    if (action < 0 || host < 0 || buildingCount(host, true) <= 0) return;

    // 只补不拆；超编由 runDestroy 按人口上限单独处理。
    const int diff = total - unitCount(type) - queuedProd(action);
    for (int i = 0; i < diff; i++) prods.push_back({priority, action, false});
}

void Mgr::wantTech(int action, int priority)
{
    if (!techAvailable(action) || doneTech.count(action) || runningTech.count(action)) return;
    prods.push_back({priority, action, true});
}

void Mgr::runProd()
{
    // 原 multiset 逆序语义：priority 高优先；同 priority 时 action 大的先。
    std::sort(prods.begin(), prods.end(), [](const ProdOrder& a, const ProdOrder& b)
    {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.action > b.action;
    });

    std::set<int> busy;  // 同一建筑本帧只能接一个新命令
    for (const ProdOrder& order : prods)
    {
        const int host = idleHost(actionHost(order.action), busy);
        if (host < 0) continue;

        const Stock cost = actionCost(order.action);
        if (!afford(cost)) continue;  // 买不起高优先级项目时，仍允许后面的便宜项目使用宿主

        BuildingAction(host, order.action);
        if (order.tech) runningTech.insert(order.action);
        busy.insert(host);
        held += cost;
    }
}

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

int Mgr::wpGain(const Pos& p) const
{
    int sum = 0;
    const int rr = SCOUT_VIEW * SCOUT_VIEW;
    for (int i = max(0, p.dr - SCOUT_VIEW); i <= min(MAP_L - 1, p.dr + SCOUT_VIEW); i++)
        for (int j = max(0, p.ur - SCOUT_VIEW); j <= min(MAP_U - 1, p.ur + SCOUT_VIEW); j++)
        {
            const int dd = i - p.dr, du = j - p.ur;
            if (dd * dd + du * du <= rr && cell(i, j).type == MAPPATTERN_UNKNOWN) sum++;
        }
    return sum;
}

// 目标就是路径点本身, 允许它在迷雾里。旧版要求"附近有已探明的可达落脚格",
// 结果祭司只能沿迷雾边缘一圈圈啃, 这里直接按直线距离挑最近的有收益路径点。
int Mgr::pickWaypoint(const Pos& here, Pos& stand) const
{
    const int wp = MAP_L / SCOUT_VIEW + 1;
    int bestIdx = -1;
    double bestDis = 0;
    stand = {-1, -1};

    for (int i = 0; i < wp; i++)
        for (int j = 0; j < wp; j++)
        {
            const int idx = i * wp + j;
            if (wpDone[idx] || gameFrame < wpCooldown[idx]) continue;

            const Pos cw(min(i * SCOUT_VIEW, MAP_L - 1), min(j * SCOUT_VIEW, MAP_U - 1));
            if (enemyCorner(cw.dr, cw.ur)) continue;
            if (threatAt(cw.dr, cw.ur) > 0) continue;  // 不再有路径安全检查, 只能在选点时避开
            if (wpGain(cw) < SCOUT_MIN_GAIN) continue;

            const double d = dis(here, cw);
            if (bestIdx >= 0 && d >= bestDis) continue;

            bestDis = d;
            bestIdx = idx;
            stand = cw;
        }
    return bestIdx;
}

int Mgr::homeETA(const Pos& here)
{
    Pos anchor = base;
    double far_ = -1;

    for (const auto& it : buildingMap)
        if (it.second->Type == BUILDING_ARROWTOWER)
        {
            const Pos p = {it.second->BlockDR, it.second->BlockUR};
            const double d = dis(p, base);
            if (d > far_) far_ = d, anchor = p;
        }

    // 锚点落在建筑占的格子上没关系: 到不了引擎会停在旁边
    home = anchor;
    return (int)(dis(here, anchor) * SCOUT_DETOUR) * 25;
}

bool Mgr::isExplore(int eta) const
{
    for (int wave : SCOUT_WAVE)
    {
        if (gameFrame < wave) return gameFrame + eta < wave;
        if (gameFrame <= wave + SCOUT_HOME_STAY) return false;
    }
    return false;
}

Pos Mgr::fleeGoal(const Pos& here) const
{
    const Pos p = bestCell([&](int i, int j) -> double
    {
        if (!walkable(i, j) || threatAt(i, j) > 0) return -1.0;
        return dis(here, Pos(i, j));
    }, here, SCOUT_FLEE_R);

    return p.dr >= 0 ? p : home;
}

bool Mgr::scoutGoto(const Pos& p, const Pos& here, bool idle)
{
    if (p.dr < 0) return false;

    const double d = dis(here, p);

    if (!(p == scoutSent))
    {
        scoutSent = p;
        scoutBest = d;
        scoutIdle = 0;
        moveToCell(priest, p);
        return false;
    }

    if (!idle) return false;  // 还在路上, 不要重复下令打断引擎

    if (d < scoutBest - MOVE_GAIN)
    {
        scoutBest = d;
        scoutIdle = 0;
        moveToCell(priest, p);
        return false;
    }

    if (++scoutIdle >= SCOUT_RETRY) return true;

    moveToCell(priest, p);
    return false;
}

void Mgr::runScout()
{
    const tagArmy* unit = army(priest);
    if (!unit || base.dr < 0) return;

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

    const int eta = homeETA(here);  // 顺带把 home 定下来

    // 1. 避险: 站进威胁圈就直奔最近的安全格
    if (threatAt(here.dr, here.ur) > 0)
    {
        goalWp = -1;
        goalStand = {-1, -1};
        if (scoutGoto(fleeGoal(here), here, idle)) scoutSent = {-1, -1};
        return;
    }

    // 2. 回家躲波次。家必须回, 走不到就隔一会儿从头再试, 不能像路径点那样放弃
    if (!isExplore(eta))
    {
        goalWp = -1;
        goalStand = {-1, -1};
        if (dis(Fhere, FloatPos(home)) < SCOUT_HOME_DONE * BLOCKSIDELENGTH) return;
        if (scoutGoto(home, here, idle)) scoutSent = {-1, -1};
        return;
    }

    // 3. 目标作废: 已经站到了, 或者顺路已经把它看光了
    if (goalWp >= 0)
    {
        const bool reached = dis(Fhere, FloatPos(goalStand)) <= SCOUT_DONE * BLOCKSIDELENGTH;
        const bool blind = wpGain(goalStand) < SCOUT_MIN_GAIN;

        if (reached || blind)
        {
            wpDone[goalWp] = 1;
            goalWp = -1;
            goalStand = {-1, -1};
        }
    }

    // 4. 选目标
    if (goalWp < 0)
    {
        goalWp = pickWaypoint(here, goalStand);
        if (goalWp < 0) return;
    }

    // 5. 下令; 确认过不去才冷却掉这个点位
    if (scoutGoto(goalStand, here, idle))
    {
        wpCooldown[goalWp] = gameFrame + SCOUT_COOLDOWN;
        goalWp = -1;
        goalStand = {-1, -1};
        scoutSent = {-1, -1};
    }
}

void Mgr::defence()
{
    hostiles.clear();
    for (const auto& it : eArmyMap)
        if (dis(FloatPos(it.second->DR, it.second->UR), baseF) < DEF_ALERT * BLOCKSIDELENGTH)
            hostiles.push_back(it.first);

    fixTower();

    combat = !hostiles.empty();
    if (!combat) return;

    // 祭司本帧交给 runDefenders 接管, 探图的移动令整条作废, 否则恢复探图时不会重发命令
    scoutSent = {-1, -1};
    scoutIdle = 0;

    runTower();
    runDefenders();
}

void Mgr::fixTower()
{
    const tagBuilding* tar = nullptr;
    if (res.stone > 0 && gameFrame <= FIX_TOWER_UNTIL)
        for (int sn : buildingsOf(BUILDING_ARROWTOWER))
        {
            const tagBuilding* t = building(sn);
            if (!t || t->Percent < 100 || t->Blood >= t->MaxBlood) continue;
            if (!tar || t->SN < tar->SN) tar = t;
        }

    for (auto it = fixCrew.begin(); it != fixCrew.end();)
        if (farmer(*it)) ++it;
        else it = fixCrew.erase(it);

    if (!tar)
    {
        while (!fixCrew.empty())
        {
            const int sn = *fixCrew.begin();
            fixCrew.erase(fixCrew.begin());
            freeWorker(sn);
        }
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
        int pick = -1;
        double best = 0;

        // 第一优先级：TOWER_ALERT 内、还没有锁住诱饵的敌人。
        for (const auto& it : eArmyMap)
        {
            const tagArmy& e = *it.second;
            if (lockOf(e.SN) >= 0) continue;
            if (dis(FloatPos(e.DR, e.UR), baseF) >= TOWER_ALERT * BLOCKSIDELENGTH) continue;

            const double d = dis(here, Pos(e.BlockDR, e.BlockUR));
            if (pick < 0 || d < best || (d == best && e.SN < pick)) best = d, pick = e.SN;
        }

        // 没有需要点名的，再打最近的来袭敌人。
        if (pick < 0)
            for (int eSN : hostiles)
            {
                const tagArmy* e = enemyArmy(eSN);
                if (!e) continue;

                const double d = dis(here, Pos(e->BlockDR, e->BlockUR));
                if (pick < 0 || d < best || (d == best && eSN < pick)) best = d, pick = eSN;
            }

        if (pick >= 0 && t->Project != pick) HumanAction(sn, pick);
    }
}

int Mgr::defenceSelector(const tagArmy& u) const
{
    const Pos me = {u.BlockDR, u.BlockUR};

    auto hostile = [&](int sn)
    { return enemyArmy(sn) && std::find(hostiles.begin(), hostiles.end(), sn) != hostiles.end(); };

    // stone: -1 任意, 0 排除投石车, 1 只要投石车。
    auto nearest = [&](bool attracted, int stone)
    {
        int pick = -1;
        double best = 0;
        for (int sn : hostiles)
        {
            const tagArmy* e = enemyArmy(sn);
            if (!e || (lockOf(sn) >= 0) != attracted) continue;
            if ((stone == 0 && e->Sort == AT_STONE_THROWER) || (stone == 1 && e->Sort != AT_STONE_THROWER)) continue;

            const double d = dis(me, Pos(e->BlockDR, e->BlockUR));
            if (pick < 0 || d < best || (d == best && sn < pick)) best = d, pick = sn;
        }
        return pick;
    };

    const tagArmy* cur = enemyArmy(u.WorkObjectSN);

    if (u.Sort == AT_PRIEST)
    {
        if (cur && hostile(cur->SN) && cur->Sort == AT_STONE_THROWER && lockOf(cur->SN) >= 0) return cur->SN;

        const int stone = nearest(true, 1);
        if (stone >= 0) return stone;

        if (cur && hostile(cur->SN) && lockOf(cur->SN) >= 0) return cur->SN;
        return nearest(true, -1);
    }

    if (cur && hostile(cur->SN) && cur->Sort != AT_STONE_THROWER &&
        (u.Sort != AT_STONE_THROWER || lockOf(cur->SN) >= 0))
        return cur->SN;

    if (u.Sort == AT_STONE_THROWER) return nearest(true, 0);

    const int attracted = nearest(true, 0);
    return attracted >= 0 ? attracted : nearest(false, 0);
}

void Mgr::runDefenders()
{
    if (assaultOn) return;

    for (const auto& it : armyMap)
    {
        const tagArmy& u = *it.second;
        if (inVanguard(u.SN)) continue;  // 提前批次已在进攻路上, 不召回防守

        const int tar = defenceSelector(u);
        if (tar >= 0)
        {
            if (u.WorkObjectSN != tar) HumanAction(u.SN, tar);
        }
        else if (enemyArmy(u.WorkObjectSN)) moveToCell(u.SN, {u.BlockDR, u.BlockUR});
    }
}

int Mgr::siegeDis(const Pos& p) const { return siegePos.dr < 0 ? dis(corner, p) : dis(siegePos, p); }

void Mgr::offenseUpdate()
{
    tars.clear();

    for (auto it = moveGoal.begin(); it != moveGoal.end();)
        if (army(it->first)) ++it;
        else it = moveGoal.erase(it);

    for (const auto& it : eArmyMap)
    {
        const tagArmy& e = *it.second;
        if (corner.dr >= 0 && dis(corner, Pos(e.BlockDR, e.BlockUR)) > BELONG_CORNER) continue;
        tars.push_back(e.SN);
    }

    for (const auto& it : eBuildingMap)
        if (it.second->Type != BUILDING_SIEGE) tars.push_back(it.second->SN);
}

int Mgr::attackSelector(const tagArmy& u) const
{
    int armyTar = -1, buildingTar = -1;
    double armyDis = 0, buildingDis = 0;
    const Pos here = {u.BlockDR, u.BlockUR};

    for (int sn : tars)
    {
        if (const tagArmy* e = enemyArmy(sn))
        {
            const double d = dis(here, Pos(e->BlockDR, e->BlockUR));
            if (armyTar < 0 || d < armyDis || (d == armyDis && sn < armyTar)) armyDis = d, armyTar = sn;
        }
        else if (const tagBuilding* b = enemyBuilding(sn))
        {
            if (b->Type == BUILDING_SIEGE) continue;
            const double d = dis(here, Pos(b->BlockDR, b->BlockUR));
            if (buildingTar < 0 || d < buildingDis || (d == buildingDis && sn < buildingTar))
                buildingDis = d, buildingTar = sn;
        }
    }

    const bool current = std::find(tars.begin(), tars.end(), u.WorkObjectSN) != tars.end() &&
                         (enemyArmy(u.WorkObjectSN) ||
                          (enemyBuilding(u.WorkObjectSN) && enemyBuilding(u.WorkObjectSN)->Type != BUILDING_SIEGE));

    // 当前打敌军就一直打完
    if (current)
    {
        if (enemyArmy(u.WorkObjectSN) || armyTar < 0 || armyTar == u.WorkObjectSN) return u.WorkObjectSN;
        return armyTar;
    }

    return armyTar >= 0 ? armyTar : buildingTar;
}

FloatPos Mgr::slotAt(int slot)
{
    static const double off[5][2] = {{0.25, 0.25}, {0.25, 0.75}, {0.75, 0.25}, {0.75, 0.75}, {0.5, 0.5}};
    const Pos c = cellPos(slot / 5);
    const int k = slot % 5;
    return FloatPos((c.dr + off[k][0]) * BLOCKSIDELENGTH, (c.ur + off[k][1]) * BLOCKSIDELENGTH);
}

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

FloatPos Mgr::marchGoal() const
{ return siegePos.dr >= 0 ? centerOf(siegePos, BUILDING_SIEGE) : FloatPos(corner); }

void Mgr::sendMove(const tagArmy& u, const FloatPos& at, int slot, bool back)
{
    if (slot >= 0) slotClaim(u, slot);

    MoveOrder m;
    m.at = at;
    m.slot = slot;
    m.back = back;
    m.best = dis(FloatPos(u.DR, u.UR), at) / BLOCKSIDELENGTH;
    moveGoal[u.SN] = m;

    HumanMove(u.SN, at.dr, at.ur);
}

void Mgr::marchTo(const tagArmy& u, const FloatPos& at)
{
    auto it = moveGoal.find(u.SN);
    if (it != moveGoal.end() && it->second.slot < 0 && it->second.stuck &&
        dis(it->second.at, at) < BLOCKSIDELENGTH)
        return;

    sendMove(u, at, -1, false);
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

// 从 from 沿 nav 朝基地方向下坡一格, 返回那一格里离 ref 最近的空闲子位; 无路可走返回 -1。
// 只有后撤需要这种逐格控制: 要精确停在"退出危险距离但仍在射程内"的位置。
int Mgr::slotStep(const tagArmy& u, const Pos& from, const FloatPos& ref) const
{
    const int hereRank = nav[cellIdx(from.dr, from.ur)];
    const int lo = u.Sort == AT_STONE_THROWER ? 4 : 0;
    const int hi = u.Sort == AT_STONE_THROWER ? 5 : 4;

    int best = -1, bestRank = 0;
    double bestMove = 0;

    for (int d = 0; d < 8; d++)
    {
        const Pos n = {from.dr + dx[d], from.ur + dy[d]};
        if (!walkable(n.dr, n.ur)) continue;
        if (dx[d] && dy[d] && (!walkable(from.dr + dx[d], from.ur) || !walkable(from.dr, from.ur + dy[d]))) continue;

        const int rank = nav[cellIdx(n.dr, n.ur)];
        if (rank < 0 || (hereRank >= 0 && rank >= hereRank)) continue;

        for (int k = lo; k < hi; k++)
        {
            const int slot = slotIdx(n.dr, n.ur, k);
            if (!slotFree(slot, u)) continue;

            const double move = dis(ref, slotAt(slot)) / BLOCKSIDELENGTH;
            if (best >= 0 && (rank > bestRank || (rank == bestRank && move >= bestMove))) continue;

            best = slot;
            bestRank = rank;
            bestMove = move;
        }
    }
    return best;
}

int Mgr::pickSlot(const tagArmy& u)
{
    const Pos here = {u.BlockDR, u.BlockUR};
    if (!inMap(here.dr, here.ur)) return -1;

    int slot = slotStep(u, here, FloatPos(u.DR, u.UR));
    if (slot < 0) return -1;

    // 继续下坡; 中途某格挤满了就停在已经拿到的最远那个子位
    Pos cur = cellPos(slot / 5);
    for (int n = 1; n < RETREAT_STEP; n++)
    {
        const int next = slotStep(u, cur, slotAt(slot));
        if (next < 0) break;

        slot = next;
        cur = cellPos(slot / 5);
    }
    return slot;
}

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

bool Mgr::keepMove(const tagArmy& u, bool interrupt)
{
    auto it = moveGoal.find(u.SN);
    if (it == moveGoal.end()) return false;

    MoveOrder& m = it->second;
    if (!m.back && interrupt)
    {
        moveGoal.erase(it);
        return false;
    }
    if (u.NowState != HUMAN_STATE_IDLE) return true;

    const double d = dis(FloatPos(u.DR, u.UR), m.at) / BLOCKSIDELENGTH;

    if (m.slot >= 0)
    {
        // 后撤只有 RETREAT_STEP 格, 不值得重试: 差得远说明子位挤不进去, 拉黑后下一帧另选
        if (d > MOVE_DONE) slotBlack[m.slot] = gameFrame + SLOT_COOLDOWN;

        moveGoal.erase(it);
        return false;
    }

    // 行军令: 有进展就重发继续推
    if (d < m.best - MOVE_GAIN)
    {
        m.best = d;
        m.idle = 0;
        HumanMove(u.SN, m.at.dr, m.at.ur);
        return true;
    }

    if (++m.idle < MOVE_RETRY)
    {
        HumanMove(u.SN, m.at.dr, m.at.ur);
        return true;
    }

    m.stuck = true;  // 确认过不去, 交给 marchTo 判断要不要换目标重发
    return false;
}

void Mgr::runAssault()
{
    std::vector<const tagArmy*> units;
    for (const auto& it : armyMap)
    {
        const tagArmy* u = it.second;
        if (u->Sort != AT_COMPOSITE_BOWMAN && u->Sort != AT_STONE_THROWER) continue;
        if (!inMap(u->BlockDR, u->BlockUR)) continue;
        if (!assaultOn && !inVanguard(u->SN)) continue;
        units.push_back(u);
    }
    if (units.empty()) return;

    slotOwner.assign((size_t)MAP_L * MAP_U * 5, -1);
    if (slotBlack.size() != slotOwner.size()) slotBlack.assign(slotOwner.size(), 0);

    // 先占真实位置，再占在途目标
    for (const tagArmy* u : units)
        if (u->Sort != AT_STONE_THROWER) slotClaim(*u, slotOf(*u));
    for (const tagArmy* u : units)
        if (u->Sort == AT_STONE_THROWER) slotClaim(*u, slotOf(*u));
    for (const auto& it : eArmyMap) slotClaim(*it.second, slotOf(*it.second));
    for (const tagArmy* u : units)
    {
        auto it = moveGoal.find(u->SN);
        if (it != moveGoal.end()) slotClaim(*u, it->second.slot);
    }

    // 越危险、越靠前的单位越先抢可用子位。没有进攻距离场之后直接用到攻城厂的直线格距排序
    std::sort(units.begin(), units.end(), [&](const tagArmy* a, const tagArmy* b)
    {
        const double ga = enemyGap(FloatPos(a->DR, a->UR)), gb = enemyGap(FloatPos(b->DR, b->UR));
        if (ga != gb) return ga < gb;

        const int fa = siegeDis({a->BlockDR, a->BlockUR});
        const int fb = siegeDis({b->BlockDR, b->BlockUR});
        return fa != fb ? fa < fb : a->SN < b->SN;
    });

    const FloatPos march = marchGoal();

    for (const tagArmy* up : units)
    {
        const tagArmy& u = *up;
        const double gap = enemyGap(FloatPos(u.DR, u.UR));
        const double danger = u.Sort == AT_STONE_THROWER ? RETREAT_STONE : RETREAT_BOW;
        const int tar = attackSelector(u);

        if (keepMove(u, gap < danger || tar >= 0)) continue;

        // 1. 贴得太近先退, 逐格控制以免退出射程
        if (gap < danger)
        {
            const int slot = pickSlot(u);
            if (slot >= 0)
            {
                sendMove(u, slotAt(slot), slot, true);
                continue;
            }
        }
        // 2. 有目标交给引擎自动进入射程
        if (tar >= 0)
        {
            if (u.WorkObjectSN != tar || u.NowState == HUMAN_STATE_IDLE) HumanAction(u.SN, tar);
            continue;
        }
        // 3. 没目标就朝敌方老家行军, 寻路全部交给引擎
        marchTo(u, march);
    }
}

void Mgr::runAtkPriest()
{
    if (!assaultOn) return;
    const tagArmy* p = army(priest);
    if (!p) return;

    if (siegeSN >= 0 && eArmyMap.size() <= 2 && eBuildingMap.size() <= 3)
    {
        if (p->WorkObjectSN != siegeSN) HumanAction(p->SN, siegeSN);
        return;
    }

    const Pos here = {p->BlockDR, p->BlockUR};
    const int threshold = siegeSN < 0 ? PRIEST_STAY_BLIND : PRIEST_STAY;
    const int gap = siegeDis(here);

    if (gap >= threshold && gap <= threshold + PRIEST_STAY_BAND) return;

    const bool retreat = gap < threshold;
    const Pos best = bestCell([&](int i, int j) -> double
    {
        const Pos c = {i, j};
        if (!walkable(i, j) || nav[cellIdx(i, j)] < 0 || siegeDis(c) < threshold) return -1.0;
        return retreat ? dis(c, here) : siegeDis(c);
    });

    if (p->NowState != HUMAN_STATE_WALKING && best.dr >= 0) moveToCell(p->SN, best);
}

void Mgr::offense()
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

    offenseUpdate();

    if (!assaultOn && gameFrame >= ASSAULT_FRAME) assaultOn = true;
    vanguardPick();
    if (!assaultOn && vanguard.empty()) return;

    runAssault();
    if (assaultOn) runAtkPriest();  // 祭司跟大部队走, 不跟提前批次
}

void Mgr::clearRoad()
{
    auto inBand = [&](int dr, int ur)
    {
        const int d = nav[cellIdx(dr, ur)];
        return d >= WAIT_BAND_IN && d <= WAIT_BAND_OUT;
    };

    std::vector<Pos> points;
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
            if (inBand(i, j)) points.push_back({i, j});

    if (points.empty()) return;

    for (const auto& a : armyMap)
    {
        const tagArmy* u = a.second;
        if (inVanguard(u->SN) || u->NowState != HUMAN_STATE_IDLE) continue;
        if (inBand(u->BlockDR, u->BlockUR)) continue;

        moveToCell(u->SN, points[rand() % points.size()]);
    }
}

int Mgr::farmerTarget() const { return std::max(FARMER_MIN, std::min(FARMER_MAX, POP_CAP - (int)armyMap.size() - 2)); }

void Mgr::strategy()
{
    int b_prio = 100;
    int e_prio = 100;
    int phase = 0;

    const int homeCnt = min(12, (int)(farmerMap.size() + armyMap.size()) / 4 + 1);
    wantBuilding(BUILDING_HOME, homeCnt, b_prio--);

    wantDepot(BUILDING_STOCK, b_prio--);
    wantDepot(BUILDING_GRANARY, b_prio--);
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
        if (!hasTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW) || buildingCount(BUILDING_RANGE) <= 3) phase = 1;
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

    laborFrame();  // fixTower 会取人, 空闲池必须先于 defence 重建

    defence();
    if (!combat) runScout();
    if (!combat && !assaultOn) clearRoad();
    offense();

    strategy();  // econPlan 在这里定下各岗位人数

    laborRelease();
    laborFrame();  // 不能提前

    runProd();

    runFarm();
    runGather();
    runBuild();

    runDestroy();

    CommitInstruction();
}
