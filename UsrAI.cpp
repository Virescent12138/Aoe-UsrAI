#include "UsrAI.h"

using namespace std;
tagGame tagUsrGame;
ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/

static const int dx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
static const int dy[8] = {1, 0, -1, 0, 1, -1, -1, 1};

Mgr mgr;

static long long hashKey(int type, int dr, int ur)  // 哈希
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
        case BUILDING_HOME: return (int)BUILD_HOUSE_WOOD;
        case BUILDING_GRANARY: return (int)BUILD_GRANARY_WOOD;
        case BUILDING_STOCK: return (int)BUILD_STOCK_WOOD;
        case BUILDING_ARMYCAMP: return (int)BUILD_ARMYCAMP_WOOD;
        case BUILDING_MARKET: return (int)BUILD_MARKET_WOOD;
        case BUILDING_FARM: return (int)BUILD_FARM_WOOD;
        case BUILDING_RANGE: return (int)BUILD_RANGE_WOOD;
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
        case BUILDING_CENTER_CREATEFARMER: c.meat = (int)BUILDING_CENTER_CREATEFARMER_FOOD; break;
        case BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN:
            c.meat = (int)BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_FOOD;
            c.gold = (int)BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_GOLD;
            break;
        case BUILDING_CENTER_UPGRADE: c.meat = (int)BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD; break;
        case BUILDING_RANGE_UPGRADE_COMPOSITE_BOW:
            c.meat = (int)BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_FOOD;
            c.wood = (int)BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD;
            break;
        case BUILDING_MARKET_WOOD_UPGRADE:
            c.meat = (int)BUILDING_MARKET_WOOD_UPGRADE_FOOD;
            c.wood = (int)BUILDING_MARKET_WOOD_UPGRADE_WOOD;
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
        case BUILDING_MARKET_WOOD_UPGRADE: return BUILDING_MARKET;
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
    huntFrame();
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
            dropSpot(sn, true);
            excess--;
        }
    }
}

double Mgr::workerCost(int sn, const FloatPos& at, bool steal) const
{
    const tagFarmer* f = farmer(sn);
    if (!f || workerReserved(sn)) return -1.0;

    const bool gathering = targetOf(workerOfSpot, sn) >= 0;
    if (gathering && !steal) return -1.0;

    double cost = dis(at, FloatPos(f->DR, f->UR)) / (double)BLOCKSIDELENGTH;
    if (gathering)
    {
        cost += WORK_SWITCH_COST;
        if (f->Resource > 0) cost += WORK_CARRY_COST;
    }
    return cost;
}

int Mgr::pickWorker(const FloatPos& at, bool steal, double* outCost) const
{
    int best = -1;
    double bestCost = 0.0;

    auto consider = [&](int sn)
    {
        const double cost = workerCost(sn, at, steal);
        if (cost < 0) return;
        if (best < 0 || cost < bestCost - EPS || (std::fabs(cost - bestCost) <= EPS && sn < best))
        {
            best = sn;
            bestCost = cost;
        }
    };

    if (steal)
        for (const auto& it : farmerMap) consider(it.first);
    else
        for (int sn : laborPool) consider(sn);

    if (outCost) *outCost = best < 0 ? -1.0 : bestCost;
    return best;
}

void Mgr::claimWorker(int sn)
{
    if (!farmer(sn)) return;

    const int spot = targetOf(workerOfSpot, sn);
    if (spot >= 0) workerOfSpot.erase(spot);

    auto it = std::find(laborPool.begin(), laborPool.end(), sn);
    if (it != laborPool.end())
    {
        *it = laborPool.back();
        laborPool.pop_back();
    }
}

int Mgr::takeNearest(const FloatPos& at, bool steal)
{
    const int best = pickWorker(at, steal);
    if (best >= 0) claimWorker(best);
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
    if (targetOf(farmToWorker, sn) >= 0 || fixCrew.count(sn) || huntCrew.count(sn)) return true;
    for (const BuildSite& s : sites)
        if (s.workers.count(sn)) return true;
    return false;
}

void Mgr::workerDrop(int sn)
{
    dropSpot(sn, false);
    for (BuildSite& s : sites) s.workers.erase(sn);
    fixCrew.erase(sn);
    huntCrew.erase(sn);

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

// 一条采集绑定只要满足三条之一就算"活着": 刚换人、身上已经攒到东西、到资源的距离有变化。
// 三条都不满足并持续 GATHER_STUCK 帧, 说明引擎把村民卡在半路了(靠近 -> 被挡 -> IDLE -> 重新靠近),
// 这时把资源点拉黑, 让它下一帧退出资源池, 村民自然被解绑回空闲池。
void Mgr::gatherWatch()
{
    for (auto it = resBlack.begin(); it != resBlack.end();)
        if (gameFrame >= it->second) it = resBlack.erase(it);
        else ++it;

    // 资源没了、村民死了、或者这个人被抢去建造/修塔(workerDrop 已解绑)时, 记录一并作废
    for (auto it = resWatch.begin(); it != resWatch.end();)
    {
        auto bind = workerOfSpot.find(it->first);
        if (bind == workerOfSpot.end() || !resource(it->first) || !farmer(bind->second)) it = resWatch.erase(it);
        else ++it;
    }

    for (const auto& bind : workerOfSpot)
    {
        const tagResource* r = resource(bind.first);
        const tagFarmer* f = farmer(bind.second);
        if (!r || !f) continue;

        // 被改派去干别的活了, 这条记录作废。只认"指向另一个有效对象"的情况:
        // 引擎在 IDLE 时可能把 WorkObjectSN 清成无效值, 那恰恰是要监视的卡死态, 不能当改派。
        const int obj = f->WorkObjectSN;
        if (obj != r->SN && (resource(obj) || building(obj) || farmer(obj)))
        {
            resWatch.erase(bind.first);
            continue;
        }

        ResWatch& w = resWatch[bind.first];
        const double d = dis(FloatPos(f->DR, f->UR), FloatPos(r->DR, r->UR)) / (double)BLOCKSIDELENGTH;

        if (w.worker != bind.second || f->Resource > 0 || std::fabs(d - w.ref) >= GATHER_MOVE)
        {
            w.worker = bind.second;
            w.ref = d;
            w.idle = 0;
            continue;
        }

        if (++w.idle < GATHER_STUCK) continue;

        resBlack[bind.first] = gameFrame + RES_BLACK;
        resWatch.erase(bind.first);
    }
}

// 树木与金矿有碰撞箱, 林子/矿脉内部的那些被同类围死, 引擎根本过不去。这里只判"周围一圈有没有
// nav 可达的落脚格", 把最外层挑出来; 但不再给落脚格做独占分配, 谁站哪一格交给引擎。
// 浆果与尸体没有碰撞箱, 走到格子上就能采, 不做这道筛。
bool Mgr::reachable(const tagResource* r) const
{
    const int size = resourceSize(r->Type);
    const Pos a = resourceCell(r);

    for (int i = a.dr - 1; i <= a.dr + size; i++)
        for (int j = a.ur - 1; j <= a.ur + size; j++)
        {
            if (i >= a.dr && i < a.dr + size && j >= a.ur && j < a.ur + size) continue;
            if (!inMap(i, j) || !walkable(i, j)) continue;
            if (nav[cellIdx(i, j)] >= 0) return true;
        }
    return false;
}

void Mgr::huntFrame()
{
    for (auto it = huntCrew.begin(); it != huntCrew.end();)
        if (farmer(*it)) ++it;
        else it = huntCrew.erase(it);

    // 已稳定的批次只保留仍存在的尸体。
    for (auto bit = huntBatches.begin(); bit != huntBatches.end();)
    {
        std::vector<int>& batch = *bit;
        batch.erase(std::remove_if(batch.begin(), batch.end(), [&](int sn)
        {
            const tagResource* r = resource(sn);
            return !r || r->Type != RESOURCE_GAZELLE || r->Blood > 0;
        }), batch.end());

        if (batch.empty()) bit = huntBatches.erase(bit);
        else ++bit;
    }

    // 当前 session 没有活目标了：此刻尸体位置已经稳定，整批转入仓库规划。
    if (!huntTargets.empty())
    {
        bool live = false;
        for (int sn : huntTargets)
        {
            const tagResource* r = resource(sn);
            if (r && r->Type == RESOURCE_GAZELLE && r->Blood > 0)
            {
                live = true;
                break;
            }
        }

        if (!live)
        {
            std::vector<int> batch;
            for (int sn : huntTargets)
            {
                const tagResource* r = resource(sn);
                if (r && r->Type == RESOURCE_GAZELLE && r->Blood <= 0) batch.push_back(sn);
            }
            if (!batch.empty()) huntBatches.push_back(batch);

            huntTargets.clear();
            huntCrew.clear();
        }
    }

    if (!huntTargets.empty()) return;

    // 找离基地最近的一只可达活羚羊作为 seed，只主动处理 50 格内的动物。
    const tagResource* seed = nullptr;
    Pos seedAt;
    double seedDis = 0.0;
    for (const auto& it : resourceMap)
    {
        const tagResource* r = it.second;
        if (r->Type != RESOURCE_GAZELLE || r->Blood <= 0) continue;

        const Pos at = resourceCell(r);
        if (!inMap(at.dr, at.ur) || nav[cellIdx(at.dr, at.ur)] < 0) continue;

        const double d = dis(at, base);
        if (d > HUNT_RANGE) continue;
        if (!seed || d < seedDis - EPS || (std::fabs(d - seedDis) <= EPS && r->SN < seed->SN))
        {
            seed = r;
            seedAt = at;
            seedDis = d;
        }
    }
    if (!seed) return;

    // 羚羊通常 5~6 只紧密成群；直接按 seed 十格内成批，不做更复杂的聚类/驱赶。
    for (const auto& it : resourceMap)
    {
        const tagResource* r = it.second;
        if (r->Type != RESOURCE_GAZELLE || r->Blood <= 0) continue;

        const Pos at = resourceCell(r);
        if (!inMap(at.dr, at.ur) || nav[cellIdx(at.dr, at.ur)] < 0) continue;
        if (dis(at, base) > HUNT_RANGE || dis(at, seedAt) > HUNT_CLUSTER) continue;

        huntTargets.push_back(r->SN);
    }
    std::sort(huntTargets.begin(), huntTargets.end());
}

int Mgr::huntFutureFood() const
{
    int cnt = 0;
    for (int sn : huntTargets)
    {
        const tagResource* r = resource(sn);
        if (r && r->Type == RESOURCE_GAZELLE && r->Blood > 0) cnt++;
    }
    return cnt;
}

void Mgr::runHunt()
{
    if (huntTargets.empty()) return;

    // 已经有猎人时，以猎队中心为参考挑下一只；刚开 session 时从离基地最近的开始。
    FloatPos ref = baseF;
    if (!huntCrew.empty())
    {
        double dr = 0.0, ur = 0.0;
        int n = 0;
        for (int sn : huntCrew)
        {
            const tagFarmer* f = farmer(sn);
            if (!f) continue;
            dr += f->DR;
            ur += f->UR;
            n++;
        }
        if (n > 0) ref = FloatPos(dr / n, ur / n);
    }

    const tagResource* target = nullptr;
    double best = 0.0;
    for (int sn : huntTargets)
    {
        const tagResource* r = resource(sn);
        if (!r || r->Type != RESOURCE_GAZELLE || r->Blood <= 0) continue;

        const double d = dis(ref, FloatPos(r->DR, r->UR));
        if (!target || d < best - EPS || (std::fabs(d - best) <= EPS && r->SN < target->SN))
        {
            target = r;
            best = d;
        }
    }
    if (!target) return;

    while ((int)huntCrew.size() < CREW_HUNT)
    {
        const int sn = takeNearest(FloatPos(target->DR, target->UR), true);
        if (sn < 0) break;
        huntCrew.insert(sn);
    }

    // 两个人始终集火同一只，缩短受击后的逃跑时间，避免主动把一群鹿打散。
    for (int sn : huntCrew) sendAction(sn, target->SN);
}

void Mgr::huntDepotWant(std::vector<Pos>& out) const
{
    const double far_ = DEPOT_FAR * (double)BLOCKSIDELENGTH;

    // 同时存在多个历史批次时，只处理当前采集人数最多的一批，避免不同猎群把仓库位置拉到中间。
    std::vector<Pos> bestSpots;
    int bestWorkers = 0;

    for (const std::vector<int>& batch : huntBatches)
    {
        int workers = 0;
        std::vector<Pos> spots;

        for (int sn : batch)
        {
            const tagResource* r = resource(sn);
            if (!r || r->Type != RESOURCE_GAZELLE || r->Blood > 0) continue;

            if (workerOfSpot.count(sn)) workers++;

            const Pos at = resourceCell(r);
            if (depotCost(FloatPos(r->DR, r->UR), BUILDING_STOCK) <= far_) continue;
            if (depotCovered(BUILDING_STOCK, at) || !depotRoom(at)) continue;
            spots.push_back(at);
        }

        if (workers <= 0 || spots.empty()) continue;
        if (bestSpots.empty() || workers > bestWorkers ||
            (workers == bestWorkers && spots.size() > bestSpots.size()))
        {
            bestWorkers = workers;
            bestSpots = spots;
        }
    }

    out.insert(out.end(), bestSpots.begin(), bestSpots.end());
}

// 类型、拉黑、直线距离三道筛; 有碰撞箱的再加一道可达性。
void Mgr::gatherFrame()
{
    gatherWatch();  // 必须先于建池, 本帧拉黑才能立即生效

    for (int k = 0; k < RK_COUNT; k++) pools[k].spots.clear();

    std::unordered_set<int> selected;
    selected.reserve(resourceMap.size());

    for (const auto& it : resourceMap)
    {
        const tagResource* r = it.second;
        const ResKind k = kindOf(r->Type);
        if (k == RK_COUNT || resBlack.count(r->SN)) continue;
        if (k == RK_GAZELLE && r->Blood > 0) continue;  // 活羚羊只归 huntCrew，普通经济只吃尸体

        const Pos at = resourceCell(r);
        if (!inMap(at.dr, at.ur) || dis(at, base) > RES_RANGE) continue;
        if ((k == RK_WOOD || k == RK_GOLD) && !reachable(r)) continue;

        GatherSpot s;
        s.sn = r->SN;
        s.at = at;
        s.cost = depotCost(FloatPos(r->DR, r->UR), k == RK_BUSH ? BUILDING_GRANARY : BUILDING_STOCK);
        s.rate = gatherRate(k, s.cost);

        pools[k].spots.push_back(s);
        selected.insert(s.sn);
    }

    // 池内统一按运输距离升序, laborRelease 从尾部退人才是退最差的那个。
    for (int k = 0; k < RK_COUNT; k++)
        std::sort(pools[k].spots.begin(), pools[k].spots.end(), [](const GatherSpot& a, const GatherSpot& b)
        { return a.cost != b.cost ? a.cost < b.cost : a.sn < b.sn; });

    // 资源消失、工人死亡或本帧被拉黑时解除旧绑定; 不额外下令打断, 交给下一帧重新指派。
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

int Mgr::econPick(const int weight[E_COUNT], const int count[E_COUNT], const int cap[E_COUNT]) const
{
    int pick = -1;
    double best = -1.0;
    for (int r = 0; r < E_COUNT; r++)
    {
        const int w = weight[r];
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


void Mgr::runEconomy()
{
    // 已有有效绑定就是租约: 只重新给本帧真正缺人的岗位派人。
    for (int k = 0; k < RK_COUNT; k++)
        for (const GatherSpot& s : pools[k].spots)
        {
            auto it = workerOfSpot.find(s.sn);
            if (it != workerOfSpot.end()) sendAction(it->second, s.sn);
        }
    for (const auto& it : farmToWorker) sendAction(it.second, it.first);

    struct EconJob
    {
        int target = -1;
        int group = -1;  // 0..RK_COUNT-1 为资源, RK_COUNT 为农田
        FloatPos at;
        double rate = 0.0;
    };

    const int GROUPS = RK_COUNT + 1;
    const int FARM_GROUP = RK_COUNT;
    int need[RK_COUNT + 1] = {};
    std::vector<EconJob> jobs;

    for (int k = 0; k < RK_COUNT; k++)
    {
        GatherPool& p = pools[k];
        const int target = min(p.desired, (int)p.spots.size());
        int assigned = 0;

        for (const GatherSpot& s : p.spots)
            if (workerOfSpot.count(s.sn)) assigned++;

        need[k] = max(0, target - assigned);
        if (need[k] <= 0) continue;

        for (const GatherSpot& s : p.spots)
            if (!workerOfSpot.count(s.sn)) jobs.push_back({s.sn, k, FloatPos(s.at), s.rate});
    }

    const int farmTarget = min(farmDesired, (int)farmList.size());
    need[FARM_GROUP] = max(0, farmTarget - (int)farmToWorker.size());
    if (need[FARM_GROUP] > 0)
        for (int farmSN : farmList)
        {
            if (farmToWorker.count(farmSN)) continue;
            const tagBuilding* b = building(farmSN);
            if (!b) continue;

            const FloatPos at = centerOf({b->BlockDR, b->BlockUR}, BUILDING_FARM);
            const double rate = gatherRate(RK_COUNT, depotCost(at, BUILDING_GRANARY));
            jobs.push_back({farmSN, FARM_GROUP, at, rate});
        }

    int totalNeed = 0;
    for (int g = 0; g < GROUPS; g++) totalNeed += need[g];
    if (totalNeed <= 0 || laborPool.empty() || jobs.empty()) return;

    struct Edge
    {
        int to, rev, cap, cost;
    };

    const std::vector<int> workers = laborPool;
    const int W = (int)workers.size();
    const int J = (int)jobs.size();
    const int source = 0;
    const int workerBase = 1;
    const int jobBase = workerBase + W;
    const int groupBase = jobBase + J;
    const int sink = groupBase + GROUPS;
    const int N = sink + 1;

    std::vector<std::vector<Edge>> graph(N);
    auto addEdge = [&](int from, int to, int cap, int cost)
    {
        Edge a = {to, (int)graph[to].size(), cap, cost};
        Edge b = {from, (int)graph[from].size(), 0, -cost};
        graph[from].push_back(a);
        graph[to].push_back(b);
    };

    for (int wi = 0; wi < W; wi++) addEdge(source, workerBase + wi, 1, 0);

    std::vector<std::vector<int>> edgeIndex(W, std::vector<int>(J, -1));
    const double speedPerSec = (double)HUMAN_SPEED * 25.0;
    for (int wi = 0; wi < W; wi++)
    {
        const tagFarmer* f = farmer(workers[wi]);
        if (!f) continue;

        const FloatPos from(f->DR, f->UR);
        for (int ji = 0; ji < J; ji++)
        {
            const EconJob& job = jobs[ji];
            const double walkSec = dis(from, job.at) / speedPerSec;
            const double cycleSec = CARRY_LIMIT / max(job.rate, EPS);
            const int cost = (int)((walkSec + cycleSec) * 1000.0 + 0.5);

            edgeIndex[wi][ji] = (int)graph[workerBase + wi].size();
            addEdge(workerBase + wi, jobBase + ji, 1, cost);
        }
    }

    for (int ji = 0; ji < J; ji++) addEdge(jobBase + ji, groupBase + jobs[ji].group, 1, 0);
    for (int g = 0; g < GROUPS; g++)
        if (need[g] > 0) addEdge(groupBase + g, sink, need[g], 0);

    const int wanted = min(totalNeed, W);
    int flow = 0;
    const int INF = 1000000000;

    while (flow < wanted)
    {
        std::vector<int> dist(N, INF), prevNode(N, -1), prevEdge(N, -1);
        std::vector<unsigned char> queued(N, 0);
        std::queue<int> q;

        dist[source] = 0;
        q.push(source);
        queued[source] = 1;

        while (!q.empty())
        {
            const int v = q.front();
            q.pop();
            queued[v] = 0;

            for (int ei = 0; ei < (int)graph[v].size(); ei++)
            {
                const Edge& e = graph[v][ei];
                if (e.cap <= 0 || dist[e.to] <= dist[v] + e.cost) continue;

                dist[e.to] = dist[v] + e.cost;
                prevNode[e.to] = v;
                prevEdge[e.to] = ei;
                if (!queued[e.to])
                {
                    queued[e.to] = 1;
                    q.push(e.to);
                }
            }
        }

        if (prevNode[sink] < 0) break;

        for (int v = sink; v != source; v = prevNode[v])
        {
            Edge& e = graph[prevNode[v]][prevEdge[v]];
            e.cap--;
            graph[v][e.rev].cap++;
        }
        flow++;
    }

    for (int wi = 0; wi < W; wi++)
        for (int ji = 0; ji < J; ji++)
        {
            const int ei = edgeIndex[wi][ji];
            if (ei < 0 || graph[workerBase + wi][ei].cap != 0) continue;

            const int sn = workers[wi];
            const EconJob& job = jobs[ji];
            claimWorker(sn);

            if (job.group == FARM_GROUP) farmToWorker[job.target] = sn;
            else workerOfSpot[job.target] = sn;

            sendAction(sn, job.target);
            break;
        }
}

// 本阶段已经排进队列、但还没花出去的资源。strategy() 里的 wantBuilding / wantUnit / wantTech
// 都是"补到目标值"的差额语义 —— 已建成的、在建的、在产的都已经从差额里扣掉了, 所以这个量
// 随着生产推进自然递减, 比固定的库存目标准确。
Stock Mgr::phaseNeed() const
{
    Stock need;
    for (const auto& order : builds) need.wood += buildWoodCost(order.second);
    for (const ProdOrder& order : prods) need += actionCost(order.action);
    return need;
}

void Mgr::econPlan(int phase)
{
    for (int k = 0; k < RK_COUNT; k++) pools[k].desired = 0;
    farmDesired = wantFarm = 0;

    // 修塔和猎队是硬预留；工地仍沿用“最多按一半人口计入经济预留”的保护。
    int builders = 0;
    for (const BuildSite& s : sites) builders += (int)s.workers.size();
    builders = min(builders, (int)farmerMap.size() / 2);

    const int reserved =
        min((int)farmerMap.size(), (int)fixCrew.size() + (int)huntCrew.size() + builders);
    const int pop = max(0, (int)farmerMap.size() - reserved);
    if (pop <= 0) return;

    FoodPlan food = planFood();

    // 默认沿用阶段固定比例; 二次调整: 库存已经盖住本阶段剩余需求的资源只留最低人手, 空出来的
    // 人口由 econPick 按剩余权重自动均分到还缺的资源上。刚换阶段时队列里全是新需求, 自然回落
    // 到默认比例。迟滞带防止需求在队列里一出一进就把人在岗位之间来回搬。
    const Stock need = phaseNeed();
    const int left[E_COUNT] = {need.wood, need.meat, need.gold};
    const int have[E_COUNT] = {res.wood, res.meat, res.gold};

    int weight[E_COUNT];
    for (int r = 0; r < E_COUNT; r++)
    {
        if (econSurplus[r])
        {
            // 新需求出现必须立即响应；不能为了迟滞把关键升级/生产拖两分钟。
            if (have[r] < left[r])
            {
                econSurplus[r] = false;
                econSwitchAfter[r] = gameFrame + SURPLUS_HOLD;
            }
        }
        else if (gameFrame >= econSwitchAfter[r] && have[r] >= left[r] + SURPLUS_BAND)
        {
            econSurplus[r] = true;
        }

        weight[r] = ECON_WEIGHT[phase][r];
        if (weight[r] > 0 && econSurplus[r]) weight[r] = SURPLUS_WEIGHT;
    }

    const int currentCap[E_COUNT] = {(int)pools[RK_WOOD].spots.size(), (int)food.jobs.size(),
                                     (int)pools[RK_GOLD].spots.size()};

    // 有市场时食物按比例先超额算, 超出现有岗位的部分就是该开农田的信号
    const int planCap[E_COUNT] = {currentCap[E_WOOD], buildAvailable(BUILDING_FARM) ? pop : currentCap[E_FOOD],
                                  currentCap[E_GOLD]};

    // 先按阶段比例生成战略目标。容量不足或 0 权重资源都不会被硬塞人口。
    int raw[E_COUNT] = {};
    for (int n = 0; n < pop; n++)
    {
        const int r = econPick(weight, raw, planCap);
        if (r < 0) break;
        raw[r]++;
    }

    pools[RK_WOOD].desired = min(raw[E_WOOD], currentCap[E_WOOD]);
    pools[RK_GOLD].desired = min(raw[E_GOLD], currentCap[E_GOLD]);

    // 一次只开一块农田, 等上一块封顶再开下一块
    const bool farmPending =
        buildingCount(BUILDING_FARM) != buildingCount(BUILDING_FARM, true) || queuedBuild(BUILDING_FARM) > 0;
    if (raw[E_FOOD] > currentCap[E_FOOD] + huntFutureFood() && !farmPending) wantFarm = 1;

    const int foodNow = min(raw[E_FOOD], currentCap[E_FOOD]);
    for (int n = 0; n < foodNow; n++) takeFood(food);

    int now[E_COUNT] = {pools[RK_WOOD].desired, foodNow, pools[RK_GOLD].desired};
    int assigned = now[E_WOOD] + now[E_FOOD] + now[E_GOLD];

    // 食物岗位不够装下战略目标时, 余下的人在本阶段非零权重资源之间补位。
    while (assigned < pop)
    {
        const int r = econPick(weight, now, currentCap);
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
    huntDepotWant(stockPendings);  // 活跃狩猎不触发仓库；整群打完后按整批尸体联合选址
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
        if (depotCost(at, BUILDING_GRANARY) <= DEPOT_FAR * (double)BLOCKSIDELENGTH) continue;

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

        if (dis(at, centerOf({b.BlockDR, b.BlockUR}, b.Type)) <= DEPOT_FAR * (double)BLOCKSIDELENGTH) return true;
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
    return saved / (double)BLOCKSIDELENGTH;
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
    const double far_ = DEPOT_FAR * (double)BLOCKSIDELENGTH;
    const int depotType = k == RK_BUSH ? BUILDING_GRANARY : BUILDING_STOCK;

    const GatherSpot* anchor = nullptr;
    for (const GatherSpot& s : pools[k].spots)
    {
        if (!workerOfSpot.count(s.sn)) continue;
        if (s.cost <= far_) continue;
        if (depotCovered(depotType, s.at) || !depotRoom(s.at)) continue;
        if (!anchor || s.cost > anchor->cost) anchor = &s;
    }
    if (anchor) out.push_back(anchor->at);
}

Pos Mgr::findSpot(int type, int& firstWorker)
{
    firstWorker = -1;
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

    // 根据建筑类型特化。普通建筑不再把所有村民位置烘进热力图；
    // 施工者距离在候选地基最终打分时直接计算，保证选址与派人使用同一个人。
    switch (type)
    {
        case BUILDING_FARM:
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
            break;

        default: break;
    }

    const int area = size * size;
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
                const double gain = depotBenefit(type, {i, j});
                const bool hasDemand = type == BUILDING_GRANARY ? !granaryPendings.empty() : !stockPendings.empty();
                if (hasDemand && gain <= EPS) continue;
                v -= (long long)(gain * 40);  // 仓储收益仍是仓库/谷仓的主导因素
            }

            auto fit = failedSpots.find(hashKey(type, i, j));
            if (fit != failedSpots.end()) v += (long long)PLACE_FAILED * fit->second;

            double claim = 0.0;
            const int worker = pickWorker(centerOf({i, j}, type), true, &claim);
            if (worker < 0) continue;
            v += (long long)(claim + 0.5);

            if (best.dr < 0 || v < bestCost)
            {
                bestCost = v;
                best = {i, j};
                firstWorker = worker;
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
            const int sn = takeNearest(centerOf(s.site, s.type), true);
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

        if (left.wood < wood) break;

        int first = -1;
        const Pos spot = findSpot(type, first);
        if (spot.dr < 0 || first < 0) continue;

        const int cell = cellIdx(spot.dr, spot.ur);
        if (placed.count(cell)) continue;

        claimWorker(first);

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
    int excess = (int)farmerMap.size() - min(FARMER_MAX, POP_CAP - (int)armyMap.size() - 2);
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
            if (wpDone[idx]) continue;

            const Pos cw(min(i * SCOUT_VIEW, MAP_L - 1), min(j * SCOUT_VIEW, MAP_U - 1));
            // 只探基地直线距离 SCOUT_HOME_RADIUS 以内; 此范围由防守机制保证安全, 不再需要威胁/象限判断。
            if (dis(cw, base) > SCOUT_HOME_RADIUS) continue;
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
    int size = buildingSize(BUILDING_CENTER);
    double far_ = -1;

    for (const auto& it : buildingMap)
        if (it.second->Type == BUILDING_ARROWTOWER)
        {
            const Pos p = {it.second->BlockDR, it.second->BlockUR};
            const double d = dis(p, base);
            if (d > far_)
            {
                far_ = d;
                anchor = p;
                size = buildingSize(BUILDING_ARROWTOWER);
            }
        }

    // 建筑本体是不可走格。固定选择建筑外圈里 nav 最小的一格作为集合点，
    // 这样回家命令本身就是可完成的，不会因为目标落在建筑内部反复 IDLE/重发。
    Pos stand = {-1, -1};
    int bestNav = -1;
    for (int i = anchor.dr - 1; i <= anchor.dr + size; i++)
        for (int j = anchor.ur - 1; j <= anchor.ur + size; j++)
        {
            if (i >= anchor.dr && i < anchor.dr + size && j >= anchor.ur && j < anchor.ur + size) continue;
            if (!inMap(i, j) || !walkable(i, j)) continue;

            const int d = nav[cellIdx(i, j)];
            if (d < 0) continue;
            if (stand.dr < 0 || d < bestNav || (d == bestNav && cellIdx(i, j) < cellIdx(stand.dr, stand.ur)))
            {
                stand = {i, j};
                bestNav = d;
            }
        }

    home = stand.dr >= 0 ? stand : anchor;
    return (int)(dis(here, home) * SCOUT_DETOUR) * 25;
}

bool Mgr::scoutGoto(const Pos& p, const Pos& here, bool idle)
{
    if (p.dr < 0) return false;

    const double d = dis(here, p);

    // 新目标只下一次命令。之后即便引擎偶尔掉回 IDLE，也继续维护同一个持久目标。
    if (!(p == scoutSent))
    {
        scoutSent = p;
        scoutBest = d;
        scoutFails = 0;
        scoutRetryAt = gameFrame + SCOUT_RETRY;
        moveToCell(priest, p);
        return false;
    }

    if (!idle) return false;                 // 引擎还在执行，不打断
    if (gameFrame < scoutRetryAt) return false;  // IDLE 也不逐帧刷 HumanMove

    scoutRetryAt = gameFrame + SCOUT_RETRY;

    // 从上一次“确认有进展”的位置累计判断，而不是按每个 IDLE 帧判断。
    // 小步前进会累积到 MOVE_GAIN 后清零失败次数。
    if (d < scoutBest - MOVE_GAIN)
    {
        scoutBest = d;
        scoutFails = 0;
        moveToCell(priest, p);
        return false;
    }

    if (++scoutFails >= SCOUT_STUCK_RETRY) return true;

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
    if (wpDone.empty()) wpDone.assign(wp * wp, 0);

    const int eta = homeETA(here);

    // 回家决定必须保持到该波次避险结束；不能因为走近后 ETA 变短又重新去探图。
    if (scoutHomeUntil <= gameFrame)
    {
        scoutHomeUntil = 0;
        bool matched = false;

        for (int wave : SCOUT_WAVE)
        {
            if (gameFrame < wave)
            {
                if (gameFrame + eta >= wave) scoutHomeUntil = wave + SCOUT_HOME_STAY + 1;
                matched = true;
                break;
            }

            if (gameFrame <= wave + SCOUT_HOME_STAY)
            {
                scoutHomeUntil = wave + SCOUT_HOME_STAY + 1;
                matched = true;
                break;
            }
        }

        // 保留原行为：最后一波之后祭司永久留家。
        if (!matched && gameFrame > SCOUT_WAVE[2] + SCOUT_HOME_STAY) scoutHomeUntil = 1000000000;
    }

    if (scoutHomeUntil > gameFrame)
    {
        goalWp = -1;
        goalStand = {-1, -1};

        if (dis(Fhere, FloatPos(home)) < SCOUT_HOME_DONE * (double)BLOCKSIDELENGTH)
        {
            scoutSent = {-1, -1};
            scoutFails = 0;
            scoutRetryAt = 0;
            return;
        }

        if (scoutGoto(home, here, idle)) scoutSent = {-1, -1};
        return;
    }

    if (goalWp >= 0)
    {
        const bool reached = dis(Fhere, FloatPos(goalStand)) <= SCOUT_DONE * (double)BLOCKSIDELENGTH;
        const bool blind = wpGain(goalStand) < SCOUT_MIN_GAIN;

        if (reached || blind)
        {
            wpDone[goalWp] = 1;
            goalWp = -1;
            goalStand = {-1, -1};
            scoutSent = {-1, -1};
            scoutFails = 0;
            scoutRetryAt = 0;
        }
    }

    if (goalWp < 0)
    {
        goalWp = pickWaypoint(here, goalStand);
        if (goalWp < 0) return;
    }

    // 走不到的路径点直接标记为已完成, 反正范围内没有威胁, 不需要冷却重试。
    if (scoutGoto(goalStand, here, idle))
    {
        wpDone[goalWp] = 1;
        goalWp = -1;
        goalStand = {-1, -1};
        scoutSent = {-1, -1};
    }
}

void Mgr::defence()
{
    hostiles.clear();
    for (const auto& it : eArmyMap)
        if (dis(FloatPos(it.second->DR, it.second->UR), baseF) < DEF_ALERT * (double)BLOCKSIDELENGTH)
            hostiles.push_back(it.first);

    fixTower();

    combat = !hostiles.empty();
    if (!combat) return;

    scoutSent = {-1, -1};
    scoutFails = 0;
    scoutRetryAt = 0;

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
        const int sn = takeNearest(centerOf({tar->BlockDR, tar->BlockUR}, BUILDING_ARROWTOWER), true);
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
            if (dis(FloatPos(e.DR, e.UR), baseF) >= TOWER_ALERT * (double)BLOCKSIDELENGTH) continue;

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

    // 进攻目标只登记敌军。箭塔/其它建筑不能再打断“清军 -> 找基地”的推进节奏。
    for (const auto& it : eArmyMap)
    {
        const tagArmy& e = *it.second;
        if (corner.dr >= 0 && dis(corner, Pos(e.BlockDR, e.BlockUR)) > BELONG_CORNER) continue;
        tars.push_back(e.SN);
    }
}

int Mgr::attackSelector(const tagArmy& u) const
{
    int pick = -1;
    double best = 0.0;
    const Pos here = {u.BlockDR, u.BlockUR};

    // 当前已经在打一个仍属于 offense 的敌军，就把它打完，避免频繁换目标。
    if (enemyArmy(u.WorkObjectSN) && std::find(tars.begin(), tars.end(), u.WorkObjectSN) != tars.end())
        return u.WorkObjectSN;

    for (int sn : tars)
    {
        const tagArmy* e = enemyArmy(sn);
        if (!e) continue;

        const double d = dis(here, Pos(e->BlockDR, e->BlockUR));
        if (pick < 0 || d < best || (d == best && sn < pick))
        {
            pick = sn;
            best = d;
        }
    }
    return pick;
}

double Mgr::enemyGap(const FloatPos& at) const
{
    double best = 1e9;
    for (int sn : tars)
    {
        const tagArmy* e = enemyArmy(sn);
        if (!e) continue;
        best = std::min(best, dis(at, FloatPos(e->DR, e->UR)) / (double)BLOCKSIDELENGTH);
    }
    return best;
}

FloatPos Mgr::marchGoal() const { return siegePos.dr >= 0 ? centerOf(siegePos, BUILDING_SIEGE) : FloatPos(corner); }

Pos Mgr::retreatCell(const Pos& from) const
{
    if (!inMap(from.dr, from.ur)) return {-1, -1};

    Pos cur = from;
    for (int step = 0; step < RETREAT_STEP; step++)
    {
        const int hereRank = nav[cellIdx(cur.dr, cur.ur)];
        Pos next = {-1, -1};
        int bestRank = hereRank;

        for (int d = 0; d < 8; d++)
        {
            const Pos n = {cur.dr + dx[d], cur.ur + dy[d]};
            if (!walkable(n.dr, n.ur)) continue;
            if (dx[d] && dy[d] && (!walkable(cur.dr + dx[d], cur.ur) || !walkable(cur.dr, cur.ur + dy[d])))
                continue;

            const int rank = nav[cellIdx(n.dr, n.ur)];
            if (rank < 0 || (hereRank >= 0 && rank >= bestRank)) continue;

            next = n;
            bestRank = rank;
        }

        if (next.dr < 0) break;
        cur = next;
    }

    return cur == from ? Pos{-1, -1} : cur;
}

void Mgr::sendMove(const tagArmy& u, const FloatPos& at, bool back)
{
    MoveOrder m;
    m.at = at;
    m.back = back;
    m.best = dis(FloatPos(u.DR, u.UR), at) / (double)BLOCKSIDELENGTH;
    moveGoal[u.SN] = m;

    HumanMove(u.SN, at.dr, at.ur);
}

void Mgr::marchTo(const tagArmy& u, const FloatPos& at)
{
    auto it = moveGoal.find(u.SN);
    if (it != moveGoal.end() && !it->second.back && it->second.stuck && dis(it->second.at, at) < (double)BLOCKSIDELENGTH)
        return;

    sendMove(u, at, false);
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
        if (dis(FloatPos(u.DR, u.UR), baseF) > HOME_RANGE * (double)BLOCKSIDELENGTH) continue;
        home.push_back(u.SN);
    }

    std::sort(home.begin(), home.end());
    for (int i = HOME_KEEP; i < home.size(); i++) vanguard.insert(home[i]);
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

    const double d = dis(FloatPos(u.DR, u.UR), m.at) / (double)BLOCKSIDELENGTH;

    // 后撤令走完(IDLE 且到位) -> 卸载, 让下一帧根据威胁再决定
    if (m.back)
    {
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

    std::sort(units.begin(), units.end(), [&](const tagArmy* a, const tagArmy* b)
    {
        const int fa = siegeDis({a->BlockDR, a->BlockUR});
        const int fb = siegeDis({b->BlockDR, b->BlockUR});
        return fa != fb ? fa < fb : a->SN < b->SN;
    });

    // 触发后撤: 敌人贴脸的单位, 以及它周围 RETREAT_GROUP 格内的所有己方一起走
    std::unordered_set<int> retreat;
    for (const tagArmy* u : units)
    {
        const double danger = u->Sort == AT_STONE_THROWER ? RETREAT_STONE : RETREAT_BOW;
        if (enemyGap(FloatPos(u->DR, u->UR)) < danger) retreat.insert(u->SN);
    }
    if (!retreat.empty())
    {
        std::vector<const tagArmy*> triggers;
        triggers.reserve(retreat.size());
        for (const tagArmy* u : units)
            if (retreat.count(u->SN)) triggers.push_back(u);

        for (const tagArmy* u : units)
        {
            if (retreat.count(u->SN)) continue;
            for (const tagArmy* t : triggers)
            {
                if (std::max(std::abs(u->BlockDR - t->BlockDR), std::abs(u->BlockUR - t->BlockUR)) <= RETREAT_GROUP)
                {
                    retreat.insert(u->SN);
                    break;
                }
            }
        }
    }

    const FloatPos march = marchGoal();

    for (const tagArmy* up : units)
    {
        const tagArmy& u = *up;
        const int tar = attackSelector(u);

        // 后撤最高优先级: 打断开火/推进, 沿 nav 下坡走 RETREAT_STEP 格
        if (retreat.count(u.SN))
        {
            const Pos rc = retreatCell({u.BlockDR, u.BlockUR});
            if (rc.dr < 0) continue;  // 已在 nav 谷底, 无处可退

            const FloatPos to = FloatPos(rc);
            auto it = moveGoal.find(u.SN);
            const bool sameGoal = it != moveGoal.end() && it->second.back &&
                                  dis(it->second.at, to) < (double)BLOCKSIDELENGTH * 0.5;
            if (sameGoal && u.NowState == HUMAN_STATE_WALKING) continue;
            sendMove(u, to, true);
            continue;
        }

        if (keepMove(u, tar >= 0)) continue;

        // 有目标交给引擎自动进入射程
        if (tar >= 0)
        {
            if (u.WorkObjectSN != tar || u.NowState == HUMAN_STATE_IDLE) HumanAction(u.SN, tar);
            continue;
        }
        // 没目标就朝敌方老家行军
        marchTo(u, march);
    }
}

void Mgr::runAtkPriest()
{
    if (!assaultOn) return;
    const tagArmy* p = army(priest);
    if (!p) return;

    if (priestRushOn)
    {
        if (siegeSN >= 0 && (p->WorkObjectSN != siegeSN || p->NowState == HUMAN_STATE_IDLE))
            HumanAction(p->SN, siegeSN);
        return;
    }

    const Pos here = {p->BlockDR, p->BlockUR};

    // 全程跟随复合弓前线：定位到 cover[2] 后方 PRIEST_COVER_GAP 格，随主力进退。
    // 盲区(武器厂未定位)时 siegeDis 自动退化成到角落的距离, 复合弓推进/后撤祭司都会跟着走。
    std::vector<int> cover;
    for (const auto& it : armyMap)
    {
        const tagArmy* u = it.second;
        if (u->Sort != AT_COMPOSITE_BOWMAN || !inMap(u->BlockDR, u->BlockUR)) continue;
        if (!assaultOn && !inVanguard(u->SN)) continue;
        cover.push_back(siegeDis({u->BlockDR, u->BlockUR}));
    }

    if (cover.empty())
    {
        if (home.dr >= 0 && !(here == home)) moveToCell(p->SN, home);
        return;
    }

    std::sort(cover.begin(), cover.end());
    const int idx = std::min(2, (int)cover.size() - 1);
    const int threshold = cover[idx] + PRIEST_COVER_GAP;

    const int gap = siegeDis(here);
    if (gap >= threshold && gap <= threshold + PRIEST_STAY_BAND) return;

    const bool retreat = gap < threshold;
    const Pos best = bestCell([&](int i, int j) -> double
    {
        const Pos c = {i, j};
        if (!walkable(i, j) || nav[cellIdx(i, j)] < 0) return -1.0;

        const int d = siegeDis(c);
        if (retreat)
        {
            if (d < threshold || threatAt(i, j) > 0) return -1.0;
            return dis(c, here);
        }

        if (d < threshold || d > threshold + PRIEST_STAY_BAND || threatAt(i, j) > 0) return -1.0;
        return dis(c, here);
    });

    if (best.dr < 0) return;
    // 后撤必须实时打断当前前进命令; 前进则允许原命令走完, 避免每帧刷 moveToCell 抖动。
    if (retreat || p->NowState != HUMAN_STATE_WALKING) moveToCell(p->SN, best);
}

void Mgr::runTowerBreak()
{
    std::vector<const tagBuilding*> towers;
    for (const auto& it : eBuildingMap)
        if (it.second->Type == BUILDING_ARROWTOWER) towers.push_back(it.second);

    std::sort(towers.begin(), towers.end(), [](const tagBuilding* a, const tagBuilding* b)
    { return a->SN < b->SN; });

    std::vector<const tagArmy*> bows, stones;
    for (const auto& it : armyMap)
    {
        const tagArmy* u = it.second;
        if (!inMap(u->BlockDR, u->BlockUR)) continue;
        if (!assaultOn && !inVanguard(u->SN)) continue;

        if (u->Sort == AT_COMPOSITE_BOWMAN) bows.push_back(u);
        else if (u->Sort == AT_STONE_THROWER) stones.push_back(u);
    }
    std::sort(bows.begin(), bows.end(), [](const tagArmy* a, const tagArmy* b) { return a->SN < b->SN; });
    std::sort(stones.begin(), stones.end(), [](const tagArmy* a, const tagArmy* b) { return a->SN < b->SN; });

    // 复合弓到箭塔的归属在突破阶段内保持稳定; 只在塔死了/新塔出现时清理, 让分组能均衡地扑向各塔。
    std::unordered_map<int, int> towerIndex;
    for (int i = 0; i < (int)towers.size(); i++) towerIndex[towers[i]->SN] = i;

    for (auto it = towerShield.begin(); it != towerShield.end();)
    {
        const tagArmy* u = army(it->first);
        if (!u || u->Sort != AT_COMPOSITE_BOWMAN || !towerIndex.count(it->second)) it = towerShield.erase(it);
        else ++it;
    }

    std::vector<int> groupCnt(towers.size(), 0);
    for (const auto& it : towerShield)
        if (towerIndex.count(it.second)) groupCnt[towerIndex[it.second]]++;

    for (const tagArmy* u : bows)
    {
        if (towers.empty() || towerShield.count(u->SN)) continue;

        int pick = -1;
        double bestDis = 0.0;
        for (int i = 0; i < (int)towers.size(); i++)
        {
            const double d = dis(FloatPos(u->DR, u->UR),
                                 centerOf({towers[i]->BlockDR, towers[i]->BlockUR}, towers[i]->Type));
            if (pick < 0 || groupCnt[i] < groupCnt[pick] ||
                (groupCnt[i] == groupCnt[pick] && d < bestDis))
            {
                pick = i;
                bestDis = d;
            }
        }

        towerShield[u->SN] = towers[pick]->SN;
        groupCnt[pick]++;
    }

    // 复合弓当肉盾, 只贴塔身位, 不主动攻击; 引擎自动路径到相邻可站立格。
    for (const tagArmy* u : bows)
    {
        auto it = towerShield.find(u->SN);
        if (it == towerShield.end()) continue;
        const tagBuilding* t = enemyBuilding(it->second);
        if (!t) continue;
        if (u->NowState != HUMAN_STATE_WALKING) moveToCell(u->SN, {t->BlockDR, t->BlockUR});
    }

    // 投石车直接打最近的塔。
    for (const tagArmy* u : stones)
    {
        int tar = -1;
        double best = 0.0;
        for (const tagBuilding* t : towers)
        {
            const double d = dis(FloatPos(u->DR, u->UR), centerOf({t->BlockDR, t->BlockUR}, t->Type));
            if (tar < 0 || d < best || (d == best && t->SN < tar))
            {
                tar = t->SN;
                best = d;
            }
        }

        if (tar >= 0 && (u->WorkObjectSN != tar || u->NowState == HUMAN_STATE_IDLE)) HumanAction(u->SN, tar);
    }

    // 主力到场后祭司立即切武器厂。弓兵比祭司更贴塔, 承担箭塔火力。
    if (assaultOn && siegeSN >= 0)
    {
        const tagArmy* p = army(priest);
        if (p && (p->WorkObjectSN != siegeSN || p->NowState == HUMAN_STATE_IDLE)) HumanAction(p->SN, siegeSN);
    }
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

    // 三阶段主战斗仍保持:
    // 1) 还有敌军 -> 弓兵/投石车继续处理敌军;
    // 2) 敌军已清但武器厂未定位 -> 无视箭塔继续推进;
    // 3) 敌军已清且武器厂已定位 -> 全军正式突破。祭司全程由 runAtkPriest 跟队, 到点由 priestRushOn 触发切武器厂。
    const bool breakNow = tars.empty() && siegeSN >= 0;
    if (breakNow != towerBreakOn)
    {
        towerBreakOn = breakNow;
        moveGoal.clear();     // 切阶段时清掉上一阶段的行军/后撤令
        towerShield.clear();  // 下一阶段重新建立稳定的弓兵-箭塔归属
    }

    if (towerBreakOn)
    {
        priestRushOn = true;
        runTowerBreak();
        return;
    }

    runAssault();
    if (assaultOn) runAtkPriest();
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
        if (u->Sort == AT_PRIEST) continue;  // 祭司由 scout/offense 独占控制，不能被待命环带反复拉走
        if (inVanguard(u->SN) || u->NowState != HUMAN_STATE_IDLE) continue;
        if (inBand(u->BlockDR, u->BlockUR)) continue;

        moveToCell(u->SN, points[rand() % points.size()]);
    }
}

void Mgr::strategy()
{
    int b_prio = 100;
    int e_prio = 100;
    int phase = 0;

    const int homeCnt = min(12, (int)(farmerMap.size() + armyMap.size()) / 4 + 1);
    wantBuilding(BUILDING_HOME, homeCnt, b_prio--);

    wantDepot(BUILDING_STOCK, b_prio--);
    wantDepot(BUILDING_GRANARY, b_prio--);

    wantUnit(AT_FARMER, min(FARMER_MAX, POP_CAP - (int)armyMap.size() - 2), e_prio--);

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

        wantBuilding(BUILDING_RANGE, 3, b_prio--);

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
    runHunt();  // 先锁定两名猎人，strategy/econPlan 再规划剩余经济人口
    if (!combat && !assaultOn) runScout();
    if (!combat && !assaultOn) clearRoad();
    offense();

    strategy();  // econPlan 在这里定下各岗位人数

    laborRelease();
    laborFrame();  // 不能提前

    runProd();

    // 工地先抢占施工者；随后重建空闲池，再把剩余人口一次性匹配到经济岗位。
    runBuild();
    laborFrame();
    runEconomy();

    runDestroy();

    CommitInstruction();
}
