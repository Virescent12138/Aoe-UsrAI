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

// 表必须在调用时现建: 费用参数可能是运行期才确定的全局量, 静态表会在初始化期读到错误的值
static ActionInfo actionRow(int key, bool byUnit)
{
    const ActionInfo table[] = {
        {BUILDING_CENTER_CREATEFARMER, BUILDING_CENTER, AT_FARMER, {0, (int)BUILDING_CENTER_CREATEFARMER_FOOD, 0, 0}},
        {BUILDING_CENTER_UPGRADE, BUILDING_CENTER, AT_NONE, {0, (int)BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD, 0, 0}},
        {BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN, BUILDING_RANGE, AT_COMPOSITE_BOWMAN,
         {0, (int)BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_FOOD, 0, (int)BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_GOLD}},
        {BUILDING_RANGE_UPGRADE_COMPOSITE_BOW, BUILDING_RANGE, AT_NONE,
         {(int)BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD, (int)BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_FOOD, 0, 0}},
        {BUILDING_MARKET_WOOD_UPGRADE, BUILDING_MARKET, AT_NONE,
         {(int)BUILDING_MARKET_WOOD_UPGRADE_WOOD, (int)BUILDING_MARKET_WOOD_UPGRADE_FOOD, 0, 0}},
    };
    for (const ActionInfo& a : table)
        if ((byUnit ? a.unit : a.action) == key) return a;
    return {-1, -1, AT_NONE, Stock()};
}

ActionInfo actionInfo(int action) { return actionRow(action, false); }
ActionInfo unitAction(int unitType) { return actionRow(unitType, true); }

static int siteKey(const BuildSite& s) { return (s.type + 1) * MAP_L * MAP_U + cellIdx(s.site.dr, s.site.ur); }

static bool wantFirst(const Want& a, const Want& b)
{ return a.priority != b.priority ? a.priority > b.priority : a.id > b.id; }

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
                if (inMap(a, b)) blockCell[cellIdx(a, b)] = 1;
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
    dutyFrame();
    orderFrame();
    huntFrame();
    gatherFrame();
    buildFrame();
    prodFrame();
}

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

void Mgr::ringAdd(std::vector<int>& g, const Pos& around, int size, int cost, int inner, int outer)
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
        int u = (int)q.size();
        for (int i = 0; i < u; i++)
        {
            const Pos crt = q.front();
            q.pop();
            if (r >= inner) g[cellIdx(crt.dr, crt.ur)] += cost;
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

bool Mgr::locate(int sn, FloatPos* at) const
{
    FloatPos p;
    if (const tagFarmer* f = farmer(sn)) p = FloatPos(f->DR, f->UR);
    else if (const tagArmy* a = army(sn)) p = FloatPos(a->DR, a->UR);
    else if (const tagArmy* e = enemyArmy(sn)) p = FloatPos(e->DR, e->UR);
    else if (const tagResource* r = resource(sn)) p = FloatPos(r->DR, r->UR);
    else if (const tagBuilding* b = building(sn)) p = centerOf({b->BlockDR, b->BlockUR}, b->Type);
    else if (const tagBuilding* eb = enemyBuilding(sn)) p = centerOf({eb->BlockDR, eb->BlockUR}, eb->Type);
    else return false;

    if (at) *at = p;
    return true;
}

void Mgr::orderFrame()
{
    const double cellLen = (double)BLOCKSIDELENGTH;
    for (auto it = orders.begin(); it != orders.end();)
    {
        Order& o = it->second;
        const tagFarmer* f = farmer(it->first);
        const tagArmy* a = army(it->first);
        if ((!f && !a) || (o.target >= 0 && !locate(o.target, &o.at)))
        {
            it = orders.erase(it);
            continue;
        }

        const FloatPos here = f ? FloatPos(f->DR, f->UR) : FloatPos(a->DR, a->UR);
        const double d = dis(here, o.at) / cellLen;

        if (f)  // 村民: 带着资源、在干别的活或位置有变化都算有进展
        {
            const int obj = f->WorkObjectSN;
            if (f->Resource > 0 || (obj != o.target && locate(obj)) || std::fabs(d - o.best) >= GATHER_MOVE)
            {
                o.best = d;
                o.idle = 0;
            }
            else if (++o.idle >= GATHER_STUCK) o.stuck = true;
        }
        else if (a->NowState == HUMAN_STATE_IDLE)
        {
            if (o.back)  // 后撤令走完
            {
                it = orders.erase(it);
                continue;
            }
            if (o.target < 0)  // 军队/祭司移动令: IDLE 帧里靠近不足 MOVE_GAIN 计一次
            {
                if (d < o.best - MOVE_GAIN)
                {
                    o.best = d;
                    o.idle = 0;
                }
                else if (++o.idle >= MOVE_RETRY) o.stuck = true;
            }
        }
        ++it;
    }
}

void Mgr::orderMove(int sn, const FloatPos& at, bool back)
{
    const tagArmy* a = army(sn);  // 移动令只下给军队和祭司
    if (!a) return;

    auto it = orders.find(sn);
    if (it != orders.end() && it->second.target < 0 && it->second.back == back &&
        dis(it->second.at, at) < (double)BLOCKSIDELENGTH * 0.5)
    {
        if (!it->second.stuck && a->NowState == HUMAN_STATE_IDLE) HumanMove(sn, at.dr, at.ur);
        return;
    }

    Order o;
    o.at = at;
    o.back = back;
    o.best = dis(FloatPos(a->DR, a->UR), at) / (double)BLOCKSIDELENGTH;
    orders[sn] = o;
    HumanMove(sn, at.dr, at.ur);
}

void Mgr::orderAction(int sn, int target)
{
    if (const tagBuilding* b = building(sn))
    {
        if (b->Project != target) HumanAction(sn, target);
        return;
    }

    FloatPos here;
    int obj, state;
    if (const tagFarmer* f = farmer(sn)) here = FloatPos(f->DR, f->UR), obj = f->WorkObjectSN, state = f->NowState;
    else if (const tagArmy* a = army(sn)) here = FloatPos(a->DR, a->UR), obj = a->WorkObjectSN, state = a->NowState;
    else return;

    auto it = orders.find(sn);
    if (it == orders.end() || it->second.target != target)
    {
        Order o;
        o.target = target;
        if (!locate(target, &o.at)) return;
        o.best = dis(here, o.at) / (double)BLOCKSIDELENGTH;
        orders[sn] = o;
    }

    if (obj != target || state == HUMAN_STATE_IDLE) HumanAction(sn, target);
}

bool Mgr::orderStuck(int sn) const
{
    auto it = orders.find(sn);
    return it != orders.end() && it->second.stuck;
}

void Mgr::dutyFrame()
{
    std::vector<int> dead;
    for (const auto& it : duty)
        if (!farmer(it.first)) dead.push_back(it.first);
    for (int sn : dead) dropDuty(sn, false);
}

void Mgr::laborFrame()
{
    laborPool.clear();
    for (const auto& it : farmerMap)
        if (!duty.count(it.first)) laborPool.push_back(it.first);
}

void Mgr::laborRelease()
{
    for (int k = 0; k < RK_COUNT; k++)
    {
        GatherPool& p = pools[k];
        int assigned = 0;
        for (const GatherSpot& s : p.spots)
            if (holder.count(s.sn)) assigned++;

        int excess = assigned - min(p.desired, (int)p.spots.size());
        for (int i = p.spots.size() - 1; i >= 0 && excess > 0; i--)
        {
            auto it = holder.find(p.spots[i].sn);
            if (it == holder.end()) continue;

            dropDuty(it->second, true);
            excess--;
        }
    }
}

double Mgr::workerCost(int sn, const FloatPos& at, bool steal) const
{
    const tagFarmer* f = farmer(sn);
    if (!f || workerReserved(sn)) return -1.0;

    const bool gathering = duty.count(sn) > 0;  // 非专职岗位只剩采集
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
    dropDuty(sn, false);

    auto it = std::find(laborPool.begin(), laborPool.end(), sn);
    if (it != laborPool.end())
    {
        *it = laborPool.back();
        laborPool.pop_back();
    }
}

void Mgr::setDuty(int sn, int kind, int target)
{
    claimWorker(sn);
    duty[sn] = {kind, target};
    if (kind == D_GATHER || kind == D_FARM) holder[target] = sn;
}

void Mgr::dropDuty(int sn, bool toFree)
{
    auto it = duty.find(sn);
    if (it == duty.end()) return;

    if (it->second.kind == D_GATHER || it->second.kind == D_FARM) holder.erase(it->second.target);
    duty.erase(it);
    orders.erase(sn);
    if (toFree) freeWorker(sn);
}

void Mgr::freeWorker(int sn)
{
    if (!farmer(sn) || duty.count(sn)) return;
    if (std::find(laborPool.begin(), laborPool.end(), sn) != laborPool.end()) return;
    laborPool.push_back(sn);
}

std::vector<int> Mgr::crewOf(int kind, int target) const
{
    std::vector<int> out;
    for (const auto& it : duty)
        if (it.second.kind == kind && it.second.target == target) out.push_back(it.first);
    std::sort(out.begin(), out.end());
    return out;
}

bool Mgr::workerReserved(int sn) const
{
    auto it = duty.find(sn);
    return it != duty.end() && it->second.kind != D_GATHER;
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

void Mgr::gatherWatch()
{
    for (auto it = resBlack.begin(); it != resBlack.end();)
        if (gameFrame >= it->second) it = resBlack.erase(it);
        else ++it;

    for (const auto& it : duty)  // 卡死的采集点拉黑, 绑定由 gatherFrame 解开
    {
        if (it.second.kind != D_GATHER) continue;
        auto o = orders.find(it.first);
        if (o != orders.end() && o->second.target == it.second.target && o->second.stuck)
            resBlack[it.second.target] = gameFrame + RES_BLACK;
    }
}

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
    for (auto bit = huntBatches.begin(); bit != huntBatches.end();) // 已稳定的批次只保留仍存在的尸体
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

    if (!huntTargets.empty()) // 此刻尸体位置已经稳定，整批转入仓库规划
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
            for (int sn : crewOf(D_HUNT, -1)) dropDuty(sn, false);
        }
    }

    if (!huntTargets.empty()) return;

    const tagResource* seed = nullptr;// 找离基地最近的一只可达活羚羊
    Pos seedAt;
    double seedDis = 0.0;
    for (const auto& it : resourceMap)
    {
        const tagResource* r = it.second;
        if (r->Type != RESOURCE_GAZELLE || r->Blood <= 0) continue;

        const Pos at = resourceCell(r);
        if (!inMap(at.dr, at.ur) || nav[cellIdx(at.dr, at.ur)] < 0) continue;

        const double d = dis(at, base);
        if (d > RES_RANGE) continue;
        if (!seed || d < seedDis - EPS || (std::fabs(d - seedDis) <= EPS && r->SN < seed->SN))
        {
            seed = r;
            seedAt = at;
            seedDis = d;
        }
    }
    if (!seed) return;

    for (const auto& it : resourceMap)
    {
        const tagResource* r = it.second;
        if (r->Type != RESOURCE_GAZELLE || r->Blood <= 0) continue;

        const Pos at = resourceCell(r);
        if (!inMap(at.dr, at.ur) || nav[cellIdx(at.dr, at.ur)] < 0) continue;
        if (dis(at, base) > RES_RANGE || dis(at, seedAt) > HUNT_CLUSTER) continue;

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

    std::vector<int> crew = crewOf(D_HUNT, -1);
    FloatPos ref = baseF;
    if (!crew.empty())
    {
        double dr = 0.0, ur = 0.0;
        int n = 0;
        for (int sn : crew)
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

    while ((int)crew.size() < CREW_HUNT)
    {
        const int sn = pickWorker(FloatPos(target->DR, target->UR), true);
        if (sn < 0) break;
        setDuty(sn, D_HUNT, -1);
        crew.push_back(sn);
    }

    for (int sn : crew) orderAction(sn, target->SN);
}

void Mgr::huntDepotWant(std::vector<Pos>& out) const
{
    const double far_ = DEPOT_FAR * (double)BLOCKSIDELENGTH;

    // 同时存在多个历史批次时，只处理当前采集人数最多的一批
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

            if (holder.count(sn)) workers++;

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

void Mgr::gatherFrame()
{
    gatherWatch();

    for (int k = 0; k < RK_COUNT; k++) pools[k].spots.clear();

    std::unordered_set<int> selected;
    selected.reserve(resourceMap.size());

    for (const auto& it : resourceMap)
    {
        const tagResource* r = it.second;
        const ResKind k = kindOf(r->Type);
        if (k == RK_COUNT || resBlack.count(r->SN)) continue;
        if (k == RK_GAZELLE && r->Blood > 0) continue;  // 活羚羊只归猎人，普通经济只吃尸体

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

    for (int sn : buildingsOf(BUILDING_FARM))  // 已完工农田同样作为采集点
    {
        const tagBuilding* b = building(sn);
        if (!b || b->Percent < 100) continue;

        const int half = buildingSize(BUILDING_FARM) / 2;
        GatherSpot s;
        s.sn = sn;
        s.at = Pos(b->BlockDR + half, b->BlockUR + half);  // FloatPos(at) 即几何中心
        s.cost = depotCost(FloatPos(s.at), BUILDING_GRANARY);
        s.rate = gatherRate(RK_FARM, s.cost);

        pools[RK_FARM].spots.push_back(s);
        selected.insert(s.sn);
    }

    // 池内统一按运输距离升序, 同类资源下即产出降序
    for (int k = 0; k < RK_COUNT; k++)
        std::sort(pools[k].spots.begin(), pools[k].spots.end(), [](const GatherSpot& a, const GatherSpot& b)
        { return a.cost != b.cost ? a.cost < b.cost : a.sn < b.sn; });

    std::vector<int> stale;
    for (const auto& it : holder)
        if (!selected.count(it.first)) stale.push_back(it.second);
    for (int sn : stale) dropDuty(sn, false);
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

std::vector<int> Mgr::planFood() const
{
    struct FoodSlot
    {
        int kind;
        double rate;
    };
    std::vector<FoodSlot> slots_;
    for (ResKind k : {RK_GAZELLE, RK_BUSH, RK_FARM})
        for (const GatherSpot& s : pools[k].spots) slots_.push_back({k, s.rate});

    // 同产出时按 尸体 > 浆果 > 农田
    std::stable_sort(slots_.begin(), slots_.end(), [](const FoodSlot& a, const FoodSlot& b) { return a.rate > b.rate; });

    std::vector<int> jobs;
    jobs.reserve(slots_.size());
    for (const FoodSlot& s : slots_) jobs.push_back(s.kind);
    return jobs;
}

void Mgr::runEconomy()
{
    for (int k = 0; k < RK_COUNT; k++)
        for (const GatherSpot& s : pools[k].spots)
        {
            auto it = holder.find(s.sn);
            if (it != holder.end()) orderAction(it->second, s.sn);
        }

    int need[RK_COUNT] = {};
    for (int k = 0; k < RK_COUNT; k++)
    {
        int assigned = 0;
        for (const GatherSpot& s : pools[k].spots)
            if (holder.count(s.sn)) assigned++;
        need[k] = max(0, min(pools[k].desired, (int)pools[k].spots.size()) - assigned);
    }

    // 贪心: 每轮在所有(空闲村民, 缺人岗位)中取 走路时间 + 单趟周期 最小的一对
    const double speedPerSec = (double)HUMAN_SPEED * 25.0;
    while (!laborPool.empty())
    {
        int bestWorker = -1, bestTarget = -1, bestGroup = -1;
        double bestCost = 0.0;

        for (int k = 0; k < RK_COUNT; k++)
        {
            if (need[k] <= 0) continue;
            for (const GatherSpot& s : pools[k].spots)
            {
                if (holder.count(s.sn)) continue;

                const double cycleSec = CARRY_LIMIT / max(s.rate, EPS);
                for (int sn : laborPool)
                {
                    const tagFarmer* f = farmer(sn);
                    if (!f) continue;
                    const double cost = dis(FloatPos(f->DR, f->UR), FloatPos(s.at)) / speedPerSec + cycleSec;
                    if (bestWorker < 0 || cost < bestCost)
                    {
                        bestWorker = sn;
                        bestTarget = s.sn;
                        bestGroup = k;
                        bestCost = cost;
                    }
                }
            }
        }

        if (bestWorker < 0) break;

        setDuty(bestWorker, bestGroup == RK_FARM ? D_FARM : D_GATHER, bestTarget);
        need[bestGroup]--;
        orderAction(bestWorker, bestTarget);
    }
}

Stock Mgr::phaseNeed() const
{
    Stock need;
    for (const Want& w : builds) need.wood += buildWoodCost(w.id);
    for (const Want& w : prods) need += actionInfo(w.id).cost;
    return need;
}

void Mgr::econPlan(int phase)
{
    for (int k = 0; k < RK_COUNT; k++) pools[k].desired = 0;
    wantFarm = 0;

    int reserved = 0;  // 工地/修塔/打猎不参与经济分配
    for (const auto& it : duty)
        if (it.second.kind != D_GATHER && it.second.kind != D_FARM) reserved++;

    const int pop = max(0, (int)farmerMap.size() - reserved);
    if (pop <= 0) return;

    const std::vector<int> food = planFood();
    size_t cursor = 0;
    auto takeFood = [&]()
    {
        if (cursor >= food.size()) return false;
        pools[food[cursor++]].desired++;
        return true;
    };

    const Stock need = phaseNeed();
    const int left[E_COUNT] = {need.wood, need.meat, need.gold};
    const int have[E_COUNT] = {res.wood, res.meat, res.gold};

    int weight[E_COUNT];
    for (int r = 0; r < E_COUNT; r++)
    {
        if (econSurplus[r])
        {
            // 新需求出现必须立即响应
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

    const int currentCap[E_COUNT] = {(int)pools[RK_WOOD].spots.size(), (int)food.size(),
                                     (int)pools[RK_GOLD].spots.size()};

    // 有市场时食物按比例先超额算, 超出现有岗位的部分就是该开农田的信号
    const int planCap[E_COUNT] = {currentCap[E_WOOD], buildAvailable(BUILDING_FARM) ? pop : currentCap[E_FOOD],
                                  currentCap[E_GOLD]};

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
    for (int n = 0; n < foodNow; n++) takeFood();

    int now[E_COUNT] = {pools[RK_WOOD].desired, foodNow, pools[RK_GOLD].desired};
    int assigned = now[E_WOOD] + now[E_FOOD] + now[E_GOLD];

    // 食物岗位不够装下战略目标时, 余下的人在本阶段非零权重资源之间补位
    while (assigned < pop)
    {
        const int r = econPick(weight, now, currentCap);
        if (r < 0) break;

        if (r == E_FOOD)
        {
            if (!takeFood()) break;
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
    huntDepotWant(stockPendings);  // 整群打完后按整批尸体联合选址
    depotWant(RK_GOLD, stockPendings);

    // 历史农田只有仍在实际耕作时才保留仓储需求
    for (int sn : buildingsOf(BUILDING_FARM))
    {
        const tagBuilding* b = building(sn);
        if (!b) continue;

        const bool planned = b->Percent < 100;
        const bool active = holder.count(sn) > 0;
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
        if (!holder.count(s.sn)) continue;
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
                v -= (long long)(gain * 40);
            }

            auto fit = failedSpots.find(hashKey(type, i, j));
            if (fit != failedSpots.end()) v += (long long)PLACE_FAILED * fit->second;
            if (best.dr >= 0 && v >= bestCost) continue;  // 派人代价非负, 不可能更优

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
        if (order.id == type) cnt++;
    return cnt;
}

void Mgr::wantBuilding(int buildingType, int total, int priority)
{
    if (!buildAvailable(buildingType)) return;

    const int diff = total - buildingCount(buildingType) - queuedBuild(buildingType);
    for (int i = 0; i < diff; i++) builds.push_back({priority, buildingType});
}

void Mgr::releaseBuilders(const BuildSite& s)
{
    for (int sn : crewOf(D_BUILD, siteKey(s))) dropDuty(sn, true);
}

void Mgr::runBuild()
{
    for (auto it = sites.begin(); it != sites.end();)  // 维护已登记工地
    {
        BuildSite& s = *it;

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
                if (!crewOf(D_BUILD, siteKey(s)).empty()) failedSpots[hashKey(s.type, s.site.dr, s.site.ur)]++;

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

    for (const BuildSite& s : sites)
    {
        if (s.sn < 0) continue;

        std::vector<int> crew = crewOf(D_BUILD, siteKey(s));
        while ((int)crew.size() < CREW_BUILD)
        {
            const int sn = pickWorker(centerOf(s.site, s.type), true);
            if (sn < 0) break;
            setDuty(sn, D_BUILD, siteKey(s));
            crew.push_back(sn);
        }
        for (int sn : crew) orderAction(sn, s.sn);
    }

    std::sort(builds.begin(), builds.end(), wantFirst);

    const Stock left = available();
    int usedWood = 0;
    std::unordered_set<int> placed;

    for (const auto& order : builds)
    {
        const int type = order.id;
        const int wood = usedWood + buildWoodCost(type);

        if (left.wood < wood) break;

        int first = -1;
        const Pos spot = findSpot(type, first);
        if (spot.dr < 0 || first < 0) continue;

        const int cell = cellIdx(spot.dr, spot.ur);
        if (placed.count(cell)) continue;

        BuildSite s;
        s.type = type;
        s.site = spot;
        s.born = gameFrame;
        sites.push_back(s);
        setDuty(first, D_BUILD, siteKey(s));

        placed.insert(cell);
        usedWood = wood;
        HumanBuild(first, type, spot.dr, spot.ur);
    }
}

int Mgr::projectCount(int action) const
{
    const int host = actionInfo(action).host;
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
    const int host = actionInfo(action).host;
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
    for (const Want& order : prods)
        if (order.id == action) cnt++;
    return cnt;
}

void Mgr::wantUnit(int type, int total, int priority)
{
    const ActionInfo a = unitAction(type);
    if (a.host < 0 || buildingCount(a.host, true) <= 0) return;

    const int diff = total - unitCount(type) - queuedProd(a.action);
    for (int i = 0; i < diff; i++) prods.push_back({priority, a.action});
}

void Mgr::wantTech(int action, int priority)
{
    if (!techAvailable(action) || doneTech.count(action) || runningTech.count(action)) return;
    prods.push_back({priority, action});
}

void Mgr::runProd()
{
    std::sort(prods.begin(), prods.end(), wantFirst);

    std::set<int> busy;  // 同一建筑本帧只能接一个新命令
    for (const Want& order : prods)
    {
        const ActionInfo a = actionInfo(order.id);
        const int host = idleHost(a.host, busy);
        if (host < 0 || !afford(a.cost)) continue;

        BuildingAction(host, order.id);
        if (a.unit == AT_NONE) runningTech.insert(order.id);
        busy.insert(host);
        held += a.cost;
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
        dropDuty(sn, false);
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

void Mgr::runScout()
{
    const tagArmy* unit = army(priest);
    if (!unit || base.dr < 0) return;

    const tagArmy& u = *unit;
    const Pos here = {u.BlockDR, u.BlockUR};
    const FloatPos Fhere = {u.DR, u.UR};

    const int wp = MAP_L / SCOUT_VIEW + 1;
    if (wpDone.empty()) wpDone.assign(wp * wp, 0);

    const int eta = homeETA(here);

    // 回家决定必须保持到该波次避险结束
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

        if (!matched && gameFrame > SCOUT_WAVE[2] + SCOUT_HOME_STAY) scoutHomeUntil = 1000000000;
    }

    if (scoutHomeUntil > gameFrame)
    {
        goalWp = -1;
        goalStand = {-1, -1};

        // 到家停手; 卡住则撤令, 下帧重新下达
        if (dis(Fhere, FloatPos(home)) < SCOUT_HOME_DONE * (double)BLOCKSIDELENGTH || orderStuck(priest))
            orders.erase(priest);
        else orderMove(priest, FloatPos(home));
        return;
    }

    if (goalWp >= 0)
    {
        const bool reached = dis(Fhere, FloatPos(goalStand)) <= SCOUT_DONE * (double)BLOCKSIDELENGTH;
        const bool blind = wpGain(goalStand) < SCOUT_MIN_GAIN;

        if (reached || blind || orderStuck(priest))
        {
            wpDone[goalWp] = 1;
            goalWp = -1;
            goalStand = {-1, -1};
        }
    }

    if (goalWp < 0)
    {
        goalWp = pickWaypoint(here, goalStand);
        if (goalWp < 0) return;
    }

    orderMove(priest, FloatPos(goalStand));
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

    auto it = orders.find(priest);  // 探图/回家令作废, 战后重新下达
    if (!assaultOn && it != orders.end() && it->second.target < 0) orders.erase(it);

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

    std::vector<int> crew = crewOf(D_FIX, -1);
    if (!tar)
    {
        for (int sn : crew) dropDuty(sn, true);
        return;
    }

    while ((int)crew.size() < CREW_FIX)
    {
        const int sn = pickWorker(centerOf({tar->BlockDR, tar->BlockUR}, BUILDING_ARROWTOWER), true);
        if (sn < 0) break;
        setDuty(sn, D_FIX, -1);
        crew.push_back(sn);
    }

    for (int sn : crew) orderAction(sn, tar->SN);
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

        for (const auto& it : eArmyMap)
        {
            const tagArmy& e = *it.second;
            if (lockOf(e.SN) >= 0) continue;
            if (dis(FloatPos(e.DR, e.UR), baseF) >= TOWER_ALERT * (double)BLOCKSIDELENGTH) continue;

            const double d = dis(here, Pos(e.BlockDR, e.BlockUR));
            if (pick < 0 || d < best || (d == best && e.SN < pick)) best = d, pick = e.SN;
        }

        if (pick < 0)
            for (int eSN : hostiles)
            {
                const tagArmy* e = enemyArmy(eSN);
                if (!e) continue;

                const double d = dis(here, Pos(e->BlockDR, e->BlockUR));
                if (pick < 0 || d < best || (d == best && eSN < pick)) best = d, pick = eSN;
            }

        if (pick >= 0) orderAction(sn, pick);
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
        if (tar >= 0) orderAction(u.SN, tar);
        else if (enemyArmy(u.WorkObjectSN)) orderMove(u.SN, FloatPos(Pos(u.BlockDR, u.BlockUR)));
    }
}

int Mgr::siegeDis(const Pos& p) const { return siegePos.dr < 0 ? dis(corner, p) : dis(siegePos, p); }

void Mgr::offenseUpdate()
{
    tars.clear();

    // 进攻目标只登记敌军
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

    // 触发后撤
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

        // 打断开火/推进, 沿 nav 下坡走 RETREAT_STEP 格
        if (retreat.count(u.SN))
        {
            const Pos rc = retreatCell({u.BlockDR, u.BlockUR});
            if (rc.dr >= 0) orderMove(u.SN, FloatPos(rc), true);
            continue;
        }

        auto it = orders.find(u.SN);
        if (it != orders.end() && it->second.back) continue;  // 后撤令走完前不接新令

        const int tar = attackSelector(u);
        if (tar >= 0) orderAction(u.SN, tar);  // 有目标就打断行军
        else orderMove(u.SN, march);
    }
}

void Mgr::runAtkPriest()
{
    if (!assaultOn) return;
    const tagArmy* p = army(priest);
    if (!p) return;

    if (priestRushOn)
    {
        if (siegeSN >= 0) orderAction(p->SN, siegeSN);
        return;
    }

    const Pos here = {p->BlockDR, p->BlockUR};

    int sum = 0, cnt = 0;
    for (const auto& it : armyMap)
    {
        const tagArmy* u = it.second;
        if (u->Sort != AT_COMPOSITE_BOWMAN || !inMap(u->BlockDR, u->BlockUR)) continue;
        sum += siegeDis({u->BlockDR, u->BlockUR});
        cnt++;
    }

    if (cnt == 0) return;

    const int threshold = sum / cnt + PRIEST_COVER_GAP;
    const int gap = siegeDis(here);
    if (gap >= threshold && gap <= threshold + PRIEST_STAY_BAND) return;

    const bool retreat = gap < threshold;
    Pos best = {-1, -1};  // 后撤时取阈值以外最近格, 否则取环带内最近格
    double bestDis = 0;
    for (int i = 0; i < MAP_L; i++)
        for (int j = 0; j < MAP_U; j++)
        {
            if (!walkable(i, j) || nav[cellIdx(i, j)] < 0) continue;

            const Pos c = {i, j};
            const int d = siegeDis(c);
            if (d < threshold || (!retreat && d > threshold + PRIEST_STAY_BAND)) continue;

            const double v = dis(c, here);
            if (best.dr >= 0 && v >= bestDis) continue;
            best = c;
            bestDis = v;
        }

    if (best.dr < 0) return;
    if (retreat || p->NowState != HUMAN_STATE_WALKING) orderMove(p->SN, FloatPos(best));
}

void Mgr::runTowerBreak()
{
    std::vector<const tagBuilding*> towers;
    for (const auto& it : eBuildingMap)
        if (it.second->Type == BUILDING_ARROWTOWER) towers.push_back(it.second);

    std::sort(towers.begin(), towers.end(), [](const tagBuilding* a, const tagBuilding* b)
    { return a->SN < b->SN; });

    std::vector<const tagArmy*> shields;  // 复合弓与投石车都当肉盾, 不打塔
    for (const auto& it : armyMap)
    {
        const tagArmy* u = it.second;
        if (!inMap(u->BlockDR, u->BlockUR)) continue;
        if (!assaultOn && !inVanguard(u->SN)) continue;

        if (u->Sort == AT_COMPOSITE_BOWMAN || u->Sort == AT_STONE_THROWER) shields.push_back(u);
    }
    std::sort(shields.begin(), shields.end(), [](const tagArmy* a, const tagArmy* b) { return a->SN < b->SN; });

    // 肉盾到箭塔的归属在突破阶段内保持稳定
    std::unordered_map<int, int> towerIndex;
    for (int i = 0; i < (int)towers.size(); i++) towerIndex[towers[i]->SN] = i;

    for (auto it = towerShield.begin(); it != towerShield.end();)
    {
        const tagArmy* u = army(it->first);
        if (!u || (u->Sort != AT_COMPOSITE_BOWMAN && u->Sort != AT_STONE_THROWER) || !towerIndex.count(it->second))
            it = towerShield.erase(it);
        else ++it;
    }

    std::vector<int> groupCnt(towers.size(), 0);
    for (const auto& it : towerShield)
        if (towerIndex.count(it.second)) groupCnt[towerIndex[it.second]]++;

    for (const tagArmy* u : shields)
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

    for (const tagArmy* u : shields)
    {
        auto it = towerShield.find(u->SN);
        if (it == towerShield.end()) continue;
        const tagBuilding* t = enemyBuilding(it->second);
        if (!t) continue;

        // 切阶段时命令已清空, 旧的行军令(可能朝地图角落)在这里被围塔令覆盖
        // 被引擎拉去交战时撤令, 强制改回围塔
        if (u->NowState != HUMAN_STATE_WALKING && u->NowState != HUMAN_STATE_IDLE) orders.erase(u->SN);
        orderMove(u->SN, FloatPos(Pos(t->BlockDR, t->BlockUR)));
    }

    // 主力到场后祭司立即切武器厂
    if (assaultOn && siegeSN >= 0) orderAction(priest, siegeSN);
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

    const bool breakNow = tars.empty() && siegeSN >= 0;
    if (breakNow != towerBreakOn)
    {
        towerBreakOn = breakNow;
        for (const auto& it : armyMap) orders.erase(it.first);  // 切阶段时清掉上一阶段的军队命令
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

    std::vector<Pos> points;  // 有人要挪时才扫图
    for (const auto& a : armyMap)
    {
        const tagArmy* u = a.second;
        if (u->Sort == AT_PRIEST) continue;  // 祭司由 scout/offense 独占控制
        if (inVanguard(u->SN) || u->NowState != HUMAN_STATE_IDLE) continue;
        if (inBand(u->BlockDR, u->BlockUR)) continue;

        auto it = orders.find(u->SN);
        if (it != orders.end() && it->second.target < 0 && !it->second.back && !it->second.stuck)
        {
            const FloatPos to = it->second.at;  // 上一个散开点还没走到, 继续
            orderMove(u->SN, to);
            continue;
        }

        if (points.empty())
            for (int i = 0; i < MAP_L; i++)
                for (int j = 0; j < MAP_U; j++)
                    if (inBand(i, j)) points.push_back({i, j});
        if (points.empty()) return;

        orderMove(u->SN, FloatPos(points[rand() % points.size()]));
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
    laborFrame();

    defence();
    runHunt();  // strategy/econPlan 再规划剩余经济人口
    if (!combat && !assaultOn) runScout();
    if (!combat && !assaultOn) clearRoad();
    offense();

    strategy();  // econPlan 在这里定下各岗位人数

    laborRelease();
    laborFrame();

    runProd();
    runBuild();
    laborFrame();
    runEconomy();

    runDestroy();
    
    CommitInstruction();
}
