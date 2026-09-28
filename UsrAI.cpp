#include "UsrAI.h"

using namespace std;
tagGame tagUsrGame;
ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/

static const int dx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
static const int dy[8] = {1, 0, -1, 0, 1, -1, -1, 1};

static long long hashKey(int type, int dr, int ur)  // 哈希
{ return ((long long)(type + 1) << 32) | (unsigned int)cellIdx(dr, ur); }

void UsrAI::processData()
{
    const tagInfo info = getInfo();
    static Mgr mgr;
    mgr.update(info);
}

void Mgr::update(const tagInfo& info)
{
    if (info.GameFrame == lastUpdateFrame) return;

    const int prevFrame = lastUpdateFrame;
    catchUpFrame = prevFrame >= 0 && info.GameFrame > prevFrame + 1;
    lastUpdateFrame = info.GameFrame;

    makeFrame(info);

    defence();
    runHunt();

    if (!combat && !assaultOn) runScout();

    if (!catchUpFrame && !combat && !assaultOn) clearRoad();

    offense();

    strategy();

    runProd();
    runBuild();
    runEconomy();
    runDestroy();

    CommitInstruction();
}

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

static ActionInfo actionRow(int key, bool byUnit)
{
    const ActionInfo table[] = {
        {BUILDING_CENTER_CREATEFARMER, BUILDING_CENTER, AT_FARMER, {0, (int)BUILDING_CENTER_CREATEFARMER_FOOD, 0, 0}},
        {BUILDING_CENTER_UPGRADE, BUILDING_CENTER, AT_NONE, {0, (int)BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD, 0, 0}},
        {BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN,
         BUILDING_RANGE,
         AT_COMPOSITE_BOWMAN,
         {0, (int)BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_FOOD, 0, (int)BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_GOLD}},
        {BUILDING_RANGE_UPGRADE_COMPOSITE_BOW,
         BUILDING_RANGE,
         AT_NONE,
         {(int)BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD, (int)BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_FOOD, 0, 0}},
        {BUILDING_MARKET_WOOD_UPGRADE,
         BUILDING_MARKET,
         AT_NONE,
         {(int)BUILDING_MARKET_WOOD_UPGRADE_WOOD, (int)BUILDING_MARKET_WOOD_UPGRADE_FOOD, 0, 0}}};
    for (const ActionInfo& a : table)
        if ((byUnit ? a.unit : a.action) == key) return a;
    return {-1, -1, AT_NONE, Stock()};
}

ActionInfo actionInfo(int action) { return actionRow(action, false); }
ActionInfo unitAction(int unitType) { return actionRow(unitType, true); }

static int buildCrew(int type)  // 仓库/谷仓最多 4 人, 其余功能性建筑 1 人
{ return type == BUILDING_GRANARY || type == BUILDING_STOCK ? CREW_DEPOT : CREW_BUILD; }

static int siteKey(const BuildSite& s) { return (s.type + 1) * MAP_L * MAP_U + cellIdx(s.site.dr, s.site.ur); }

static int costVariant(int type)  // 建筑类型 -> 附加代价层
{
    if (type == BUILDING_FARM) return 1;
    if (type == BUILDING_ARMYCAMP || type == BUILDING_RANGE || type == BUILDING_HOME || type == BUILDING_MARKET)
        return 2;
    return 0;
}

static bool wantFirst(const Want& a, const Want& b)
{ return a.priority != b.priority ? a.priority > b.priority : a.id > b.id; }

const vector<int>& Mgr::buildingsOf(int type) const
{
    static const vector<int> kEmpty;
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
    return blockStamp[cellIdx(dr, ur)] != blockEpoch;
}

bool Mgr::canPlace(int dr, int ur, int size) const
{
    if (dr < 0 || ur < 0 || dr + size - 1 >= MAP_L || ur + size - 1 >= MAP_U) return false;

    const int h = cell(dr, ur).height;
    for (int i = dr; i < dr + size; i++)
        for (int j = ur; j < ur + size; j++)
            if (!valid(i, j) || cell(i, j).height != h || blockStamp[cellIdx(i, j)] == blockEpoch) return false;
    return true;
}

void Mgr::mark(const tagBuilding& b)
{
    const int s = buildingSize(b.Type);
    for (int i = b.BlockDR; i < b.BlockDR + s; i++)
        for (int j = b.BlockUR; j < b.BlockUR + s; j++)
            if (inMap(i, j)) blockStamp[cellIdx(i, j)] = blockEpoch;
}

void Mgr::makeFrame(const tagInfo& info)
{
    farmerMap.clear();
    armyMap.clear();
    buildingMap.clear();
    resourceMap.clear();
    eArmyMap.clear();
    eBuildingMap.clear();
    for (auto& kv : byType) kv.second.clear();  // 保留桶, 只清列表, 避免每帧重建哈希表

    fill(unitCnt.begin(), unitCnt.end(), 0);
    fill(bldCnt.begin(), bldCnt.end(), 0);
    fill(bldDoneCnt.begin(), bldDoneCnt.end(), 0);

    // 占格用 epoch，不再每帧清整张表。
    if (++blockEpoch == 0)
    {
        fill(blockStamp.begin(), blockStamp.end(), 0);
        blockEpoch = 1;
    }

    granaryDrops.clear();
    stockDrops.clear();
    held = Stock();

    gameFrame = info.GameFrame;

    for (const auto& f : info.farmers)
    {
        farmerMap.emplace(f.SN, &f);
        unitCnt[AT_FARMER + 1]++;
    }

    for (const auto& a : info.armies)
    {
        armyMap.emplace(a.SN, &a);
        unitCnt[a.Sort + 1]++;
        if (priest == -1 && a.Sort == AT_PRIEST) priest = a.SN;
    }

    for (const auto& a : info.enemy_armies) { eArmyMap.emplace(a.SN, &a); }

    res.wood = info.Wood;
    res.meat = info.Meat;
    res.stone = info.Stone;
    res.gold = info.Gold;
    stage = info.civilizationStage;
    theMap = info.theMap;

    for (const auto& r : info.resources)
    {
        resourceMap.emplace(r.SN, &r);

        if (r.Type == RESOURCE_GAZELLE && r.Blood > 0) continue;

        const int len = resourceSize(r.Type);
        const Pos anchor = resourceCell(&r);
        for (int a = anchor.dr; a < anchor.dr + len; a++)
            for (int b = anchor.ur; b < anchor.ur + len; b++)
                if (inMap(a, b)) blockStamp[cellIdx(a, b)] = blockEpoch;
    }

    for (const auto& b : info.buildings)
    {
        buildingMap.emplace(b.SN, &b);
        bldCnt[b.Type]++;
        if (b.Percent >= 100) bldDoneCnt[b.Type]++;
        byType[b.Type].push_back(b.SN);
        mark(b);

        if (base.dr == -1 && b.Type == BUILDING_CENTER)
        {
            base.dr = b.BlockDR, base.ur = b.BlockUR;
            baseF = FloatPos(base);
        }

        if (b.Percent >= 100)
        {
            const FloatPos c = centerOf({b.BlockDR, b.BlockUR}, b.Type);
            if (b.Type == BUILDING_CENTER || b.Type == BUILDING_GRANARY) granaryDrops.push_back(c);
            if (b.Type == BUILDING_CENTER || b.Type == BUILDING_STOCK) stockDrops.push_back(c);
        }
    }

    for (const auto& b : info.enemy_buildings)
    {
        eBuildingMap.emplace(b.SN, &b);
        mark(b);
    }

    if (base.dr >= 0 && corner.dr == -1)
    {
        corner.dr = (base.dr * 2 / MAP_L) ? 0 : MAP_L - 1;
        corner.ur = (base.ur * 2 / MAP_U) ? 0 : MAP_U - 1;
    }

    // 敌军视图: 本快照内所有战斗/进攻选择器共享, 避免反复 map 查找
    ev.clear();
    const double alert2 = (DEF_ALERT * (double)BLOCKSIDELENGTH) * (DEF_ALERT * (double)BLOCKSIDELENGTH);
    for (const auto& a : info.enemy_armies)
    {
        const tagArmy* e = &a;
        EnemyView v;
        v.e = e;
        v.stone = e->Sort == AT_STONE_THROWER;
        v.objSN = e->WorkObjectSN;
        v.locked = v.objSN != priest && (farmer(v.objSN) || army(v.objSN) || building(v.objSN));
        v.hostile = dis2(FloatPos(e->DR, e->UR), baseF) < alert2;
        v.inTars = corner.dr >= 0 && dis2(corner, Pos(e->BlockDR, e->BlockUR)) <= (double)BELONG_CORNER * BELONG_CORNER;
        ev.push_back(v);
    }

    // 整图拓扑检查本身也是 O(MAP_L*MAP_U)，不必跟着主线程每帧跑。
    // nav 为空时强制首建；之后最多每 NAV_CHECK_INTERVAL 帧检查一次。
    const size_t cells = (size_t)MAP_L * MAP_U;
    const int navAge = gameFrame - lastNavCheckFrame;
    const bool navDue = nav.empty() || navAge >= NAV_CHECK_INTERVAL;
    const bool navForced = nav.empty() || navAge >= NAV_FORCE_INTERVAL;

    if (navDue && (!catchUpFrame || navForced))
    {
        lastNavCheckFrame = gameFrame;
        bool navDirty = nav.size() != cells;

        for (int i = 0; i < MAP_L; i++)
            for (int j = 0; j < MAP_U; j++)
            {
                const int idx = cellIdx(i, j);
                const int t = cell(i, j).type;
                const unsigned char now =
                    ((t == MAPPATTERN_GRASS || t == MAPPATTERN_DESERT) && blockStamp[idx] != blockEpoch) ? 1 : 0;

                if (navWalk[idx] != now)
                {
                    navWalk[idx] = now;
                    navDirty = true;
                }
            }

        if (navDirty)
        {
            fieldBuild(nav, base, buildingSize(BUILDING_CENTER));
            ++navRevision;

            waitPoints.clear();
            for (int i = 0; i < MAP_L; i++)
                for (int j = 0; j < MAP_U; j++)
                {
                    const int idx = cellIdx(i, j);
                    const int d = nav[idx];
                    if (d >= WAIT_BAND_IN && d <= WAIT_BAND_OUT && navWalk[idx]) waitPoints.push_back({i, j});
                }
        }
    }

    // 命令/岗位生命周期必须逐帧维护；资源/猎物池允许短暂缓存。
    dutyFrame();
    orderFrame();

    const int resourceAge = gameFrame - lastResourceRefreshFrame;
    if (resourceAge >= RESOURCE_REFRESH_INTERVAL && (!catchUpFrame || resourceAge >= RESOURCE_FORCE_INTERVAL))
    {
        lastResourceRefreshFrame = gameFrame;
        huntFrame();
        gatherFrame();
    }

    buildFrame();
    prodFrame();
}

void Mgr::fieldBuild(vector<int>& out, const Pos& src, int size)
{
    out.assign((size_t)MAP_L * MAP_U, -1);
    if (!inMap(src.dr, src.ur)) return;

    navQueue.clear();
    for (int i = src.dr; i < src.dr + size; i++)
        for (int j = src.ur; j < src.ur + size; j++)
            if (inMap(i, j))
            {
                out[cellIdx(i, j)] = 0;
                navQueue.push_back({i, j});
            }

    for (size_t head = 0; head < navQueue.size(); head++)
    {
        const Pos c = navQueue[head];
        const int nd = out[cellIdx(c.dr, c.ur)] + 1;

        for (int k = 0; k < 8; k++)
        {
            const int nr = c.dr + dx[k], nu = c.ur + dy[k];
            if (!inMap(nr, nu)) continue;

            const int ni = cellIdx(nr, nu);
            if (!navWalk[ni] || out[ni] >= 0) continue;

            if (dx[k] && dy[k])
            {
                const int side1 = cellIdx(c.dr + dx[k], c.ur);
                const int side2 = cellIdx(c.dr, c.ur + dy[k]);
                if (!navWalk[side1] || !navWalk[side2]) continue;
            }

            out[ni] = nd;
            navQueue.push_back({nr, nu});
        }
    }
}

void Mgr::ringAdd(vector<int>& g, const Pos& around, int size, int cost, int inner, int outer)
{
    const int dr1 = around.dr + size - 1, ur1 = around.ur + size - 1;
    for (int i = around.dr - outer; i <= dr1 + outer; i++)
        for (int j = around.ur - outer; j <= ur1 + outer; j++)
        {
            if (!inMap(i, j)) continue;
            const int d =
                max(max(around.dr - i, i - dr1), max(max(around.ur - j, j - ur1), 0));  // 到矩形的切比雪夫距离
            if (d >= inner) g[cellIdx(i, j)] += cost;
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

const EnemyView* Mgr::viewOf(int sn) const
{
    for (const EnemyView& v : ev)
        if (v.e->SN == sn) return &v;
    return nullptr;
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

        // halt 只是把当前战斗/动作打断一次，不是“去某个地点”的寻路命令。
        // 等单位真正空闲或原目标消失后直接销毁，避免零距离移动进入 stuck/retry 循环。
        if (o.halt)
        {
            if (!a || a->NowState == HUMAN_STATE_IDLE || !enemyArmy(a->WorkObjectSN))
            {
                it = orders.erase(it);
                continue;
            }
            ++it;
            continue;
        }

        const FloatPos here = f ? FloatPos(f->DR, f->UR) : FloatPos(a->DR, a->UR);
        const double d = dis(here, o.at) / cellLen;

        if (f)  // 村民: 带着资源、在干别的活或位置有变化都算有进展
        {
            const int obj = f->WorkObjectSN;
            if (f->Resource > 0 || (obj != o.target && locate(obj)) || fabs(d - o.best) >= GATHER_MOVE)
            {
                o.best = d;
                o.idle = 0;
            }
            else
            {
                o.idle += gameFrame - o.lastCheck;  // 跳帧时按引擎帧差累计
                if (o.idle >= GATHER_STUCK) o.stuck = true;
            }
        }
        else if (a->NowState == HUMAN_STATE_IDLE)
        {
            if (o.back)  // 后撤走完
            {
                it = orders.erase(it);
                continue;
            }

            if (o.target < 0)  // 纯移动令
            {
                if (d < o.best - MOVE_GAIN)
                {
                    o.best = d;
                    o.idle = 0;
                    o.stuck = false;
                }
                else
                {
                    o.idle += gameFrame - o.lastCheck;
                    if (o.idle >= MOVE_RETRY) o.stuck = true;
                }
            }
            else if (a->WorkObjectSN == o.target)
            {
                // 已经锁到正确动作目标，不把短暂 IDLE 当成寻路失败。
                o.best = d;
                o.idle = 0;
                o.stuck = false;
                o.stuckAt = -1;
            }
            else
            {
                // 军队/祭司对目标 SN 的动作同样会隐式寻路。
                // 原实现这里完全不判卡，orderAction 又会每帧重发，最容易形成高频死循环。
                if (d < o.best - MOVE_GAIN)
                {
                    o.best = d;
                    o.idle = 0;
                    o.stuck = false;
                    o.stuckAt = -1;
                }
                else
                {
                    o.idle += gameFrame - o.lastCheck;
                    if (o.idle >= MOVE_RETRY && !o.stuck)
                    {
                        o.stuck = true;
                        o.stuckAt = gameFrame;
                    }
                }
            }
        }
        o.lastCheck = gameFrame;
        ++it;
    }
}

void Mgr::orderMove(int sn, const FloatPos& at, bool back)
{
    const tagArmy* a = army(sn);  // 移动令只下给军队和祭司
    if (!a) return;

    auto it = orders.find(sn);
    if (it != orders.end() && !it->second.halt && it->second.target < 0 && it->second.back == back &&
        dis(it->second.at, at) < (double)BLOCKSIDELENGTH * 0.5)
    {
        // 同一个终点不再每个 IDLE 帧重发。重发节流与 idle/stuck 计数相互独立，
        // 因此补发不会把“无进展”计数清零，也就不会妨碍最终判卡。
        if (!it->second.stuck && a->NowState == HUMAN_STATE_IDLE && gameFrame - it->second.issued >= MOVE_REISSUE)
        {
            HumanMove(sn, it->second.at.dr, it->second.at.ur);
            it->second.issued = gameFrame;
        }
        return;
    }

    Order o;
    o.at = at;
    o.back = back;
    o.best = dis(FloatPos(a->DR, a->UR), at) / (double)BLOCKSIDELENGTH;
    o.issued = gameFrame;
    o.lastCheck = gameFrame;
    orders[sn] = o;
    HumanMove(sn, at.dr, at.ur);
}

void Mgr::orderHalt(int sn)
{
    const tagArmy* a = army(sn);
    if (!a) return;

    if (a->NowState == HUMAN_STATE_IDLE)
    {
        orders.erase(sn);
        return;
    }

    auto it = orders.find(sn);
    if (it != orders.end() && it->second.halt) return;  // 已经发过一次停步令

    Order o;
    o.halt = true;
    o.at = FloatPos(Pos(a->BlockDR, a->BlockUR));
    o.issued = gameFrame;
    o.lastCheck = gameFrame;
    orders[sn] = o;
    HumanMove(sn, o.at.dr, o.at.ur);
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
    const tagFarmer* f = farmer(sn);
    const tagArmy* a = nullptr;
    if (f) here = FloatPos(f->DR, f->UR), obj = f->WorkObjectSN, state = f->NowState;
    else if ((a = army(sn))) here = FloatPos(a->DR, a->UR), obj = a->WorkObjectSN, state = a->NowState;
    else return;

    auto it = orders.find(sn);
    if (it == orders.end() || it->second.target != target)
    {
        Order o;
        o.target = target;
        if (!locate(target, &o.at)) return;
        o.best = dis(here, o.at) / (double)BLOCKSIDELENGTH;
        o.lastCheck = gameFrame;
        orders[sn] = o;
        it = orders.find(sn);
    }

    if (f)
    {
        // 村民行为保持原 UsrAI：采集/施工自己的 GATHER_* 卡死逻辑不改。
        if (obj != target || state == HUMAN_STATE_IDLE) HumanAction(sn, target);
        return;
    }

    Order& o = it->second;
    if (o.stuck)
    {
        // 不永久封死目标：先冷却；若目标期间主动靠近，orderFrame 会提前清掉 stuck。
        if (o.stuckAt < 0 || gameFrame - o.stuckAt < ACTION_STUCK_HOLD) return;

        o.stuck = false;
        o.stuckAt = -1;
        o.idle = 0;
        o.lastCheck = gameFrame;
        o.best = dis(here, o.at) / (double)BLOCKSIDELENGTH;
        o.issued = -1000000000;  // 冷却结束后允许立即低频重试一次
    }

    if (obj == target && state != HUMAN_STATE_IDLE) return;  // 已经在正确执行
    if (gameFrame - o.issued < ACTION_REISSUE) return;       // 同目标动作节流

    HumanAction(sn, target);
    o.issued = gameFrame;
}

bool Mgr::orderStuck(int sn) const
{
    auto it = orders.find(sn);
    return it != orders.end() && it->second.stuck;
}

void Mgr::dutyFrame()
{
    deadWorkers.clear();
    for (const auto& it : duty)
        if (!farmer(it.first)) deadWorkers.push_back(it.first);
    for (int sn : deadWorkers) dropDuty(sn);
}

double Mgr::workerCost(int sn, const FloatPos& at) const
{
    const tagFarmer* f = farmer(sn);
    if (!f || workerReserved(sn)) return -1.0;

    double cost = dis(at, FloatPos(f->DR, f->UR)) / (double)BLOCKSIDELENGTH;
    if (duty.count(sn)) cost += WORK_SWITCH_COST + (f->Resource > 0 ? WORK_CARRY_COST : 0.0);  // 非专职岗位只剩采集
    return cost;
}

int Mgr::pickWorker(const FloatPos& at, double* outCost) const
{
    int best = -1;
    double bestCost = 0.0;
    for (const auto& it : farmerMap)
    {
        const double cost = workerCost(it.first, at);
        if (cost < 0) continue;
        if (best < 0 || cost < bestCost - EPS || (fabs(cost - bestCost) <= EPS && it.first < best))
        {
            best = it.first;
            bestCost = cost;
        }
    }
    if (outCost) *outCost = best < 0 ? -1.0 : bestCost;
    return best;
}

void Mgr::setDuty(int sn, int kind, int target)
{
    dropDuty(sn);
    duty[sn] = {kind, target};
    if (kind == D_GATHER || kind == D_FARM) holder[target] = sn;
}

void Mgr::dropDuty(int sn)
{
    auto it = duty.find(sn);
    if (it == duty.end()) return;

    if (it->second.kind == D_GATHER || it->second.kind == D_FARM) holder.erase(it->second.target);
    duty.erase(it);
    orders.erase(sn);
}

vector<int> Mgr::crewOf(int kind, int target) const
{
    vector<int> out;
    for (const auto& it : duty)
        if (it.second.kind == kind && it.second.target == target) out.push_back(it.first);
    sort(out.begin(), out.end());
    return out;
}

bool Mgr::workerReserved(int sn) const
{
    auto it = duty.find(sn);
    return it != duty.end() && it->second.kind != D_GATHER;
}

double Mgr::depotCost(const FloatPos& at, int depotType) const
{
    const vector<FloatPos>& drops = depotType == BUILDING_GRANARY ? granaryDrops : stockDrops;

    double best = -1.0;
    for (const FloatPos& p : drops)
    {
        const double v = dis(at, p);
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
    for (auto bit = huntBatches.begin(); bit != huntBatches.end();)  // 已稳定的批次只保留仍存在的尸体
    {
        vector<int>& batch = *bit;
        batch.erase(remove_if(batch.begin(), batch.end(),
                              [&](int sn)
        {
            const tagResource* r = resource(sn);
            return !r || r->Type != RESOURCE_GAZELLE || r->Blood > 0;
        }),
                    batch.end());

        if (batch.empty()) bit = huntBatches.erase(bit);
        else ++bit;
    }

    if (!huntTargets.empty())  // 此刻尸体位置已经稳定，整批转入仓库规划
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
            vector<int> batch;
            for (int sn : huntTargets)
            {
                const tagResource* r = resource(sn);
                if (r && r->Type == RESOURCE_GAZELLE && r->Blood <= 0) batch.push_back(sn);
            }
            if (!batch.empty()) huntBatches.push_back(batch);

            huntTargets.clear();
            for (int sn : crewOf(D_HUNT, -1)) dropDuty(sn);
        }
    }

    if (!huntTargets.empty()) return;

    const tagResource* seed = nullptr;  // 找离基地最近的一只可达活羚羊
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
        if (!seed || d < seedDis - EPS || (fabs(d - seedDis) <= EPS && r->SN < seed->SN))
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
    sort(huntTargets.begin(), huntTargets.end());
}

void Mgr::runHunt()
{
    if (huntTargets.empty()) return;

    vector<int> crew = crewOf(D_HUNT, -1);
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
        if (!target || d < best - EPS || (fabs(d - best) <= EPS && r->SN < target->SN))
        {
            target = r;
            best = d;
        }
    }
    if (!target) return;

    while ((int)crew.size() < CREW_HUNT)
    {
        const int sn = pickWorker(FloatPos(target->DR, target->UR));
        if (sn < 0) break;
        setDuty(sn, D_HUNT, -1);
        crew.push_back(sn);
    }

    for (int sn : crew) orderAction(sn, target->SN);
}

void Mgr::huntDepotWant(vector<Pos>& out) const
{
    // 同时存在多个历史批次时，只处理当前采集人数最多的一批
    vector<Pos> bestSpots;
    int bestWorkers = 0;

    for (const vector<int>& batch : huntBatches)
    {
        int workers = 0;
        vector<Pos> spots;

        for (int sn : batch)
        {
            const tagResource* r = resource(sn);
            if (!r || r->Type != RESOURCE_GAZELLE || r->Blood > 0) continue;

            if (holder.count(sn)) workers++;

            const Pos at = resourceCell(r);
            if (depotCost(FloatPos(r->DR, r->UR), BUILDING_STOCK) <= DEPOT_FAR * (double)BLOCKSIDELENGTH) continue;
            if (depotCovered(BUILDING_STOCK, at) || !depotRoom(at)) continue;
            spots.push_back(at);
        }

        if (workers <= 0 || spots.empty()) continue;
        if (bestSpots.empty() || workers > bestWorkers || (workers == bestWorkers && spots.size() > bestSpots.size()))
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

    for (int k = 0; k < RK_COUNT; k++) pools[k].clear();
    spotKind.clear();

    auto add = [&](ResKind k, int sn, const Pos& at, const FloatPos& from)
    {
        GatherSpot s;
        s.sn = sn;
        s.at = at;
        s.cost = depotCost(from, k == RK_BUSH || k == RK_FARM ? BUILDING_GRANARY : BUILDING_STOCK);
        s.rate = gatherRate(k, s.cost);
        pools[k].push_back(s);
        spotKind[sn] = k;
    };

    for (const auto& it : resourceMap)
    {
        const tagResource* r = it.second;
        const ResKind k = kindOf(r->Type);
        if (k == RK_COUNT || resBlack.count(r->SN)) continue;
        if (k == RK_GAZELLE && r->Blood > 0) continue;  // 活羚羊只归猎人, 普通经济只吃尸体

        const Pos at = resourceCell(r);
        if (!inMap(at.dr, at.ur) || dis(at, base) > RES_RANGE) continue;
        if ((k == RK_WOOD || k == RK_GOLD) && !reachable(r)) continue;
        add(k, r->SN, at, FloatPos(r->DR, r->UR));
    }

    const int half = buildingSize(BUILDING_FARM) / 2;
    for (int sn : buildingsOf(BUILDING_FARM))  // 已完工农田同样作为采集点, 取中心格
    {
        const tagBuilding* b = building(sn);
        if (!b || b->Percent < 100) continue;
        const Pos at(b->BlockDR + half, b->BlockUR + half);
        add(RK_FARM, sn, at, FloatPos(at));
    }

    for (int k = 0; k < RK_COUNT; k++)
        sort(pools[k].begin(), pools[k].end(),
             [](const GatherSpot& a, const GatherSpot& b) { return a.cost != b.cost ? a.cost < b.cost : a.sn < b.sn; });

    vector<int> stale;  // 采集点已失效的绑定
    for (const auto& it : holder)
        if (!spotKind.count(it.first)) stale.push_back(it.second);
    for (int sn : stale) dropDuty(sn);
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

void Mgr::runEconomy()
{
    const int econAge = gameFrame - lastEconPlanFrame;
    const bool replan = econAge >= ECON_REPLAN_INTERVAL && (!catchUpFrame || econAge >= ECON_FORCE_INTERVAL);

    int cur[E_COUNT] = {};
    if (replan) catOf.clear();

    // 已有岗位的命令维护仍然逐帧进行；这里只把“重新匹配岗位”降频。
    for (const auto& it : duty)
    {
        if (it.second.kind != D_GATHER && it.second.kind != D_FARM) continue;

        auto k = spotKind.find(it.second.target);
        if (k == spotKind.end()) continue;

        const int c = econCat(k->second);
        cur[c]++;
        if (replan) catOf.emplace(it.first, c);
        orderAction(it.first, it.second.target);
    }

    if (!replan) return;
    lastEconPlanFrame = gameFrame;

    const double cellsPerSec = (double)HUMAN_SPEED * 25.0 / (double)BLOCKSIDELENGTH;

    // 每轮在(候选人, 缺口类别的空闲点)中取代价最小的一对。
    while (true)
    {
        bool short_ = false;
        for (int r = 0; r < E_COUNT; r++)
            if (cur[r] < quota[r])
            {
                short_ = true;
                break;
            }
        if (!short_) break;

        int bestWorker = -1, bestSpot = -1, bestKind = -1;
        double bestCost = 0.0;

        for (const auto& fit : farmerMap)
        {
            const int sn = fit.first;
            double extra = 0.0;

            auto dit = duty.find(sn);
            if (dit != duty.end())
            {
                auto c = catOf.find(sn);
                if (c == catOf.end() || cur[c->second] <= quota[c->second]) continue;
                extra = WORK_SWITCH_COST + (fit.second->Resource > 0 ? WORK_CARRY_COST : 0.0);
            }

            const FloatPos at(fit.second->DR, fit.second->UR);

            for (int k = 0; k < RK_COUNT; k++)
            {
                const int cat = econCat(k);
                if (cur[cat] >= quota[cat]) continue;

                int seen = 0;
                for (const GatherSpot& s : pools[k])
                {
                    if (holder.count(s.sn)) continue;

                    // pools 允许缓存几帧，资源已经消失时直接忽略，避免产生短暂脏岗位。
                    if (k == RK_FARM)
                    {
                        if (!building(s.sn)) continue;
                    }
                    else if (!resource(s.sn)) continue;

                    if (++seen > ECON_SPOT_TOP) break;

                    const double cycle = CARRY_LIMIT / max(s.rate, EPS) * cellsPerSec;
                    const double cost = dis(at, FloatPos(s.at)) / (double)BLOCKSIDELENGTH + extra + ECON_TRIPS * cycle;

                    if (bestWorker < 0 || cost < bestCost)
                    {
                        bestWorker = sn;
                        bestSpot = s.sn;
                        bestKind = k;
                        bestCost = cost;
                    }
                }
            }
        }

        if (bestWorker < 0) break;

        auto old = catOf.find(bestWorker);
        if (old != catOf.end()) cur[old->second]--;

        setDuty(bestWorker, bestKind == RK_FARM ? D_FARM : D_GATHER, bestSpot);
        cur[econCat(bestKind)]++;
        catOf[bestWorker] = econCat(bestKind);
        orderAction(bestWorker, bestSpot);
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
    for (int r = 0; r < E_COUNT; r++) quota[r] = 0;
    wantFarm = 0;

    int pop = 0;  // 工地/修塔/打猎不参与经济分配
    for (const auto& it : farmerMap)
    {
        auto d = duty.find(it.first);
        if (d == duty.end() || d->second.kind == D_GATHER || d->second.kind == D_FARM) pop++;
    }
    if (pop <= 0) return;

    const Stock need = phaseNeed();
    const int left[E_COUNT] = {need.wood, need.meat, need.gold};
    const int have[E_COUNT] = {res.wood, res.meat, res.gold};

    int weight[E_COUNT];
    for (int r = 0; r < E_COUNT; r++)
    {
        // 数量带防抖: 超出需求 SURPLUS_BAND 才判富余, 跌回需求以下立刻取消
        if (econSurplus[r] && have[r] < left[r]) econSurplus[r] = false;
        else if (!econSurplus[r] && have[r] >= left[r] + SURPLUS_BAND) econSurplus[r] = true;

        weight[r] = ECON_WEIGHT[phase][r];
        if (weight[r] > 0 && econSurplus[r]) weight[r] = SURPLUS_WEIGHT;
    }

    const int cap[E_COUNT] = {(int)pools[RK_WOOD].size(),
                              (int)(pools[RK_GAZELLE].size() + pools[RK_BUSH].size() + pools[RK_FARM].size()),
                              (int)pools[RK_GOLD].size()};

    // 有市场时食物先按比例超额算, 超出现有岗位的部分就是该开农田的信号
    const int plan[E_COUNT] = {cap[E_WOOD], buildAvailable(BUILDING_FARM) ? pop : cap[E_FOOD], cap[E_GOLD]};
    int raw[E_COUNT] = {};
    for (int n = 0; n < pop; n++)
    {
        const int r = econPick(weight, raw, plan);
        if (r < 0) break;
        raw[r]++;
    }

    int live = 0;  // 还没打下来的羚羊很快会变成食物岗位
    for (int sn : huntTargets)
    {
        const tagResource* r = resource(sn);
        if (r && r->Type == RESOURCE_GAZELLE && r->Blood > 0) live++;
    }
    const bool farmPending =
        buildingCount(BUILDING_FARM) != buildingCount(BUILDING_FARM, true) || queuedBuild(BUILDING_FARM) > 0;
    if (raw[E_FOOD] > cap[E_FOOD] + live && !farmPending) wantFarm = 1;  // 一次只开一块

    // 定额不超过岗位数, 装不下的人在非零权重资源之间补位
    int total = 0;
    for (int r = 0; r < E_COUNT; r++) total += quota[r] = min(raw[r], cap[r]);
    for (; total < pop; total++)
    {
        const int r = econPick(weight, quota, cap);
        if (r < 0) break;
        quota[r]++;
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
    // builds 是本帧需求队列，必须清；仓储锚点则只在资源池刷新后重算。
    builds.clear();

    if (lastDepotRefreshFrame == lastResourceRefreshFrame) return;
    lastDepotRefreshFrame = lastResourceRefreshFrame;

    granaryPendings.clear();
    stockPendings.clear();

    huntDepotWant(stockPendings);  // 整群打完后按整批尸体联合选址

    const double far_ = DEPOT_FAR * (double)BLOCKSIDELENGTH;
    for (ResKind k : {RK_BUSH, RK_FARM, RK_GOLD})
    {
        const int type = k == RK_GOLD ? BUILDING_STOCK : BUILDING_GRANARY;
        for (const GatherSpot& s : pools[k])
        {
            if (!holder.count(s.sn) || s.cost <= far_) continue;
            if (depotCovered(type, s.at) || !depotRoom(s.at)) continue;
            (type == BUILDING_STOCK ? stockPendings : granaryPendings).push_back(s.at);
        }
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

bool Mgr::depotRoom(const Pos& c) const
{
    for (int a = c.dr - DEPOT_FAR; a <= c.dr + DEPOT_FAR; a++)
        for (int b = c.ur - DEPOT_FAR; b <= c.ur + DEPOT_FAR; b++)
            if (canPlace(a, b, 3) && nav[cellIdx(a, b)] >= 0) return true;
    return false;
}

const vector<int>& Mgr::placeCost(int type)
{
    const int v = costVariant(type);
    vector<int>& g = costCache[v];
    if (costAt[v] == navRevision) return g;
    costAt[v] = navRevision;

    g.assign((size_t)MAP_L * MAP_U, 0);
    for (size_t idx = 0; idx < g.size(); idx++) g[idx] = nav[idx] < 0 ? MAP_L + MAP_U : nav[idx];  // 距离

    for (const auto& it : resourceMap)  // 贴资源
    {
        const tagResource* r = it.second;
        if (r->Type == RESOURCE_GAZELLE && r->Blood > 0) continue;
        ringAdd(g, resourceCell(r), resourceSize(r->Type), PLACE_ADJACENT, 1, 1);
    }

    for (const auto& it : buildingMap)
    {
        const tagBuilding& b = *it.second;
        const Pos p(b.BlockDR, b.BlockUR);
        ringAdd(g, p, buildingSize(b.Type), PLACE_ADJACENT, 0, 1);  // 贴建筑

        const bool granary = b.Type == BUILDING_GRANARY || b.Type == BUILDING_CENTER;
        if (v == 1 && granary) ringAdd(g, p, 3, PLACE_BONUS, 2, 5);                                   // 农田靠近谷仓
        if (v == 2 && (granary || b.Type == BUILDING_STOCK)) ringAdd(g, p, 3, PLACE_ADJACENT, 0, 5);  // 别挡存放点
    }
    // 同一个 revision 下 findSpot 会反复查询 2x2/3x3 区域均值；
    // 预先构建二维前缀和，把每个候选的 4/9 次访问降为 O(1)。
    vector<long long>& pref = costPrefix[v];
    const int stride = MAP_U + 1;
    pref.assign((size_t)(MAP_L + 1) * stride, 0);

    for (int i = 0; i < MAP_L; i++)
    {
        long long row = 0;
        for (int j = 0; j < MAP_U; j++)
        {
            row += g[cellIdx(i, j)];
            pref[(size_t)(i + 1) * stride + (j + 1)] = pref[(size_t)i * stride + (j + 1)] + row;
        }
    }

    return g;
}

const vector<Pos>& Mgr::placeSites(int size)
{
    const int slot = size == 2 ? 0 : 1;
    vector<Pos>& out = placeSiteCache[slot];
    if (placeSiteAt[slot] == navRevision) return out;

    placeSiteAt[slot] = navRevision;
    out.clear();

    for (int i = 0; i + size <= MAP_L; i++)
        for (int j = 0; j + size <= MAP_U; j++)
        {
            if (!canPlace(i, j, size)) continue;

            bool reach = false;
            for (int a = i; a < i + size && !reach; a++)
                for (int b = j; b < j + size; b++)
                    if (nav[cellIdx(a, b)] >= 0)
                    {
                        reach = true;
                        break;
                    }

            if (reach) out.push_back({i, j});
        }

    return out;
}

Pos Mgr::findSpot(int type, int& firstWorker)
{
    firstWorker = -1;
    const int size = buildingSize(type);
    const int variant = costVariant(type);
    const vector<int>& g = placeCost(type);
    (void)g;  // placeCost 同时保证 costPrefix[variant] 已构建

    const vector<Pos>& candidates = placeSites(size);
    const vector<long long>& pref = costPrefix[variant];
    const int stride = MAP_U + 1;

    // 存放点收益: 各需求点到现有存放点的运距与候选位置无关, 先算好
    const bool depot = type == BUILDING_GRANARY || type == BUILDING_STOCK;
    spotPend.clear();
    if (depot)
    {
        const vector<Pos>& src = type == BUILDING_GRANARY ? granaryPendings : stockPendings;
        for (const Pos& p : src) spotPend.push_back({FloatPos(p), depotCost(FloatPos(p), type)});
    }

    Pos best = {-1, -1};
    long long bestCost = 0;

    for (const Pos& site : candidates)
    {
        const int i = site.dr, j = site.ur;

        // placeSites 跨帧缓存；当前帧占格可能已变化，选址前做一次廉价最终校验。
        if (!canPlace(i, j, size)) continue;

        const long long sum = pref[(size_t)(i + size) * stride + (j + size)] - pref[(size_t)i * stride + (j + size)] -
                              pref[(size_t)(i + size) * stride + j] + pref[(size_t)i * stride + j];

        long long v = sum / (size * size);

        if (depot)
        {
            const FloatPos cand = centerOf(site, type);
            double saved = 0.0;
            for (const auto& p : spotPend) saved += max(0.0, p.second - dis(p.first, cand));
            if (!spotPend.empty() && saved <= EPS) continue;
            v -= (long long)(saved / (double)BLOCKSIDELENGTH * 40);
        }

        auto fit = failedSpots.find(hashKey(type, i, j));
        if (fit != failedSpots.end()) v += (long long)PLACE_FAILED * fit->second;
        if (best.dr >= 0 && v >= bestCost) continue;  // 派人代价非负, 不可能更优

        double claim = 0.0;
        const int worker = pickWorker(centerOf(site, type), &claim);
        if (worker < 0) continue;
        v += (long long)(claim + 0.5);

        if (best.dr < 0 || v < bestCost)
        {
            bestCost = v;
            best = site;
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

                for (int sn : crewOf(D_BUILD, siteKey(s))) dropDuty(sn);
                it = sites.erase(it);
                continue;
            }
        }
        const tagBuilding* b = building(s.sn);
        if (!b || b->Percent >= 100)
        {
            for (int sn : crewOf(D_BUILD, siteKey(s))) dropDuty(sn);
            it = sites.erase(it);
            continue;
        }
        it++;
    }

    unordered_set<int> owned;
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

        vector<int> crew = crewOf(D_BUILD, siteKey(s));
        int cap = s.type == BUILDING_GRANARY || s.type == BUILDING_STOCK ? CREW_DEPOT : CREW_BUILD;
        while (crew.size() < cap)
        {
            int sn = pickWorker(centerOf(s.site, s.type));
            if (sn < 0) break;
            setDuty(sn, D_BUILD, siteKey(s));
            crew.push_back(sn);
        }
        for (int sn : crew) orderAction(sn, s.sn);
    }

    const int buildPlanAge = gameFrame - lastBuildPlanFrame;
    if (buildPlanAge < BUILD_PLAN_INTERVAL) return;
    if (catchUpFrame && buildPlanAge < BUILD_FORCE_INTERVAL) return;
    lastBuildPlanFrame = gameFrame;

    sort(builds.begin(), builds.end(), wantFirst);

    const Stock left = available();
    int usedWood = 0;
    unordered_set<int> placed;

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

int Mgr::idleHost(int buildingType, const set<int>& busy) const
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
    sort(prods.begin(), prods.end(), wantFirst);

    set<int> busy;  // 同一建筑本帧只能接一个新命令
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

    destroyCand.clear();  // 先拆闲着的, 再拆普通采集工
    for (int pass = 0; pass < 2; pass++)
        for (const auto& it : farmerMap)
        {
            auto d = duty.find(it.first);
            const bool idle = d == duty.end();
            if (pass == 0 ? idle : !idle && d->second.kind == D_GATHER) destroyCand.push_back(it.first);
        }

    for (int sn : destroyCand)
    {
        if (excess-- <= 0) break;
        dropDuty(sn);
        HumanAction(sn, sn);
    }
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

            const bool unknown = cell(cw.dr, cw.ur).type == MAPPATTERN_UNKNOWN;
            if (!unknown && (!walkable(cw.dr, cw.ur) || nav[cellIdx(cw.dr, cw.ur)] < 0)) continue;

            const double d = dis(here, cw);
            if (bestIdx >= 0 && d >= bestDis) continue;

            bestDis = d;
            bestIdx = idx;
            stand = cw;
        }
    return bestIdx;
}

void Mgr::findHome()
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

    if (scoutHomeUntil <= gameFrame)
    {
        scoutHomeUntil = 0;
        scoutAtHome = false;

        for (int wave : SCOUT_WAVE)
        {
            if (gameFrame < wave)
            {
                if (wave - gameFrame <= SCOUT_RETURN_LEAD) scoutHomeUntil = wave + SCOUT_HOME_STAY + 1;
                break;
            }
            if (gameFrame <= wave + SCOUT_HOME_STAY)
            {
                scoutHomeUntil = wave + SCOUT_HOME_STAY + 1;
                break;
            }
        }

        if (scoutHomeUntil == 0 && gameFrame > SCOUT_WAVE[2] + SCOUT_HOME_STAY) scoutHomeUntil = 1000000000;
    }

    if (scoutHomeUntil > gameFrame)
    {
        goalWp = -1;
        goalStand = {-1, -1};

        if (scoutAtHome)
        {
            orders.erase(priest);
            return;
        }

        findHome();
        if (dis(Fhere, FloatPos(home)) <= SCOUT_HOME_DONE * (double)BLOCKSIDELENGTH)
        {
            scoutAtHome = true;
            orders.erase(priest);
            return;
        }

        orderMove(priest, FloatPos(home));
        return;
    }

    if (goalWp >= 0)
    {
        if (gameFrame - lastBlindCheckFrame >= SCOUT_BLIND_INTERVAL) lastBlindCheckFrame = gameFrame;

        if (dis(Fhere, FloatPos(goalStand)) <= SCOUT_DONE * (double)BLOCKSIDELENGTH ||
            (cell(goalStand.dr, goalStand.ur).type != MAPPATTERN_UNKNOWN && !walkable(goalStand.dr, goalStand.ur)) ||
            orderStuck(priest))
        {
            orders.erase(priest);
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
        if (dis2(FloatPos(it.second->DR, it.second->UR), baseF) <
            (DEF_ALERT * (double)BLOCKSIDELENGTH) * (DEF_ALERT * (double)BLOCKSIDELENGTH))
            hostiles.push_back(it.first);

    int cnt = 0;
    for (const auto& it : armyMap)
        if (it.second->Sort == AT_STONE_THROWER) cnt++;

    if (cnt == 2 && hostiles.empty())
    {
        assaultOn = true;
        return;
    }

    fixTower();

    combat = !hostiles.empty();
    if (!combat) return;

    auto it = orders.find(priest);
    if (!assaultOn && it != orders.end() && it->second.target < 0 && !it->second.halt) orders.erase(it);

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

    if (fixer >= 0 && (!farmer(fixer) || !duty.count(fixer))) fixer = -1;  // 阵亡或岗位已被解除

    if (!tar)
    {
        if (fixer >= 0) dropDuty(fixer);
        fixer = -1;
        return;
    }

    if (fixer < 0)
    {
        const int sn = pickWorker(centerOf({tar->BlockDR, tar->BlockUR}, BUILDING_ARROWTOWER));
        if (sn < 0) return;
        setDuty(sn, D_FIX, -1);
        fixer = sn;
    }

    orderAction(fixer, tar->SN);
}

void Mgr::runTower()
{
    const double alert2 = (TOWER_ALERT * (double)BLOCKSIDELENGTH) * (TOWER_ALERT * (double)BLOCKSIDELENGTH);
    for (int sn : buildingsOf(BUILDING_ARROWTOWER))
    {
        const tagBuilding* t = building(sn);
        if (!t || t->Percent < 100) continue;

        // 先点名没锁到我方的来敌, 没有再打来袭波次
        const FloatPos here = centerOf({t->BlockDR, t->BlockUR}, t->Type);
        int pick = nearestView(
            here, [&](const EnemyView& v) { return !v.locked && dis2(FloatPos(v.e->DR, v.e->UR), baseF) < alert2; });
        if (pick < 0) pick = nearestView(here, [&](const EnemyView& v) { return v.hostile; });
        if (pick >= 0) orderAction(sn, pick);
    }
}

int Mgr::defenceSelector(const tagArmy& u) const
{
    const FloatPos me(u.DR, u.UR);

    // stone: -1 任意, 0 排除投石车, 1 只要投石车
    auto nearest = [&](bool attracted, int stone)
    {
        return nearestView(me, [&](const EnemyView& v)
        {
            if (!v.hostile) return false;
            if (v.locked != attracted) return false;
            return stone < 0 || v.stone == (stone == 1);
        });
    };

    const EnemyView* cur = viewOf(u.WorkObjectSN);

    if (u.Sort == AT_PRIEST)
    {
        if (cur && cur->hostile && cur->stone && cur->locked) return cur->e->SN;

        const int stone = nearest(true, 1);
        if (stone >= 0) return stone;

        if (cur && cur->hostile && cur->locked) return cur->e->SN;
        return nearest(true, -1);
    }

    if (cur && cur->hostile && !cur->stone && (u.Sort != AT_STONE_THROWER || cur->locked)) return cur->e->SN;

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
        else if (enemyArmy(u.WorkObjectSN)) orderHalt(u.SN);
    }
}

int Mgr::siegeDis(const Pos& p) const { return siegePos.dr < 0 ? dis(corner, p) : dis(siegePos, p); }

int Mgr::attackSelector(const tagArmy& u) const
{
    const FloatPos here(u.DR, u.UR);

    // 最高优先级: 已索敌祭司的敌人, 可打断当前目标
    if (priest >= 0)
    {
        const int hunter = nearestView(here, [&](const EnemyView& v) { return v.inTars && v.objSN == priest; });
        if (hunter >= 0) return hunter;
    }

    const EnemyView* cur = viewOf(u.WorkObjectSN);
    if (cur && cur->inTars) return u.WorkObjectSN;

    return nearestView(here, [&](const EnemyView& v) { return v.inTars; });
}

bool Mgr::explicitRole(int sort)
{ return sort == AT_COMPOSITE_BOWMAN || sort == AT_STONE_THROWER || sort == AT_PRIEST; }

int Mgr::otherSelector(const tagArmy& u) const
{
    // 已下达且仍有效的目标优先保持
    auto o = orders.find(u.SN);
    if (o != orders.end() && o->second.target >= 0 && enemyArmy(o->second.target)) return o->second.target;
    if (enemyArmy(u.WorkObjectSN)) return u.WorkObjectSN;

    return nearestView(FloatPos(u.DR, u.UR), [](const EnemyView&) { return true; });
}

double Mgr::enemyGap(const FloatPos& at) const
{
    const int sn = nearestView(at, [&](const EnemyView& v) { return v.inTars; });
    const tagArmy* e = enemyArmy(sn);
    return e ? dis(at, FloatPos(e->DR, e->UR)) / (double)BLOCKSIDELENGTH : 1e9;
}

FloatPos Mgr::marchGoal() const { return siegePos.dr >= 0 ? centerOf(siegePos, BUILDING_SIEGE) : FloatPos(corner); }

Pos Mgr::retreatCell(const Pos& from, int steps) const
{
    if (!inMap(from.dr, from.ur)) return {-1, -1};

    Pos cur = from;
    for (int step = 0; step < steps; step++)
    {
        const int hereRank = nav[cellIdx(cur.dr, cur.ur)];
        Pos next = {-1, -1};
        int bestRank = hereRank;

        for (int d = 0; d < 8; d++)
        {
            const Pos n = {cur.dr + dx[d], cur.ur + dy[d]};
            if (!walkable(n.dr, n.ur)) continue;
            if (dx[d] && dy[d] && (!walkable(cur.dr + dx[d], cur.ur) || !walkable(cur.dr, cur.ur + dy[d]))) continue;

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

    vector<int> home;
    for (const auto& it : armyMap)
    {
        const tagArmy& u = *it.second;
        if (u.Sort != AT_COMPOSITE_BOWMAN || inVanguard(u.SN)) continue;
        if (dis(FloatPos(u.DR, u.UR), baseF) > HOME_RANGE * (double)BLOCKSIDELENGTH) continue;
        home.push_back(u.SN);
    }

    sort(home.begin(), home.end());
    for (int i = HOME_KEEP; i < home.size(); i++) vanguard.insert(home[i]);
}

void Mgr::runAssault()
{
    assaultUnits.clear();
    for (const auto& it : armyMap)
    {
        const tagArmy* u = it.second;
        if (!explicitRole(u->Sort)) continue;                // 无明确策略的军队不参与后撤与行军
        if (u->Sort == AT_PRIEST && priestRushOn) continue;  // 冲锋中的祭司只管武器厂
        if (!inMap(u->BlockDR, u->BlockUR)) continue;
        if (!assaultOn && !inVanguard(u->SN)) continue;
        assaultUnits.push_back(u);
    }
    if (assaultUnits.empty()) return;

    sort(assaultUnits.begin(), assaultUnits.end(), [&](const tagArmy* a, const tagArmy* b)
    {
        const int fa = siegeDis({a->BlockDR, a->BlockUR});
        const int fb = siegeDis({b->BlockDR, b->BlockUR});
        return fa != fb ? fa < fb : a->SN < b->SN;
    });

    // 触发后撤
    retreatSet.clear();
    for (const tagArmy* u : assaultUnits)
    {
        const double danger = u->Sort == AT_STONE_THROWER ? RETREAT_STONE
                              : u->Sort == AT_PRIEST      ? RETREAT_PRIEST
                                                          : RETREAT_BOW;
        if (enemyGap(FloatPos(u->DR, u->UR)) < danger) retreatSet.insert(u->SN);
    }
    if (!retreatSet.empty())
    {
        retreatTriggers.clear();
        for (const tagArmy* u : assaultUnits)
            if (retreatSet.count(u->SN)) retreatTriggers.push_back(u);

        for (const tagArmy* u : assaultUnits)
        {
            if (retreatSet.count(u->SN)) continue;
            for (const tagArmy* t : retreatTriggers)
            {
                if (dis(FloatPos(u->DR, u->UR), FloatPos(t->DR, t->UR)) <= RETREAT_GROUP * (double)BLOCKSIDELENGTH)
                {
                    retreatSet.insert(u->SN);
                    break;
                }
            }
        }
    }

    const FloatPos march = marchGoal();

    for (const tagArmy* up : assaultUnits)
    {
        const tagArmy& u = *up;

        // 打断开火/推进, 沿 nav 下坡走 RETREAT_STEP 格
        if (retreatSet.count(u.SN))
        {
            const Pos rc = retreatCell({u.BlockDR, u.BlockUR}, RETREAT_STEP);
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

void Mgr::runTowerBreak()
{
    breakTowers.clear();
    for (const auto& it : eBuildingMap)
        if (it.second->Type == BUILDING_ARROWTOWER) breakTowers.push_back(it.second);

    sort(breakTowers.begin(), breakTowers.end(),
         [](const tagBuilding* a, const tagBuilding* b) { return a->SN < b->SN; });

    breakShields.clear();  // 复合弓与投石车都当肉盾, 不打塔
    for (const auto& it : armyMap)
    {
        const tagArmy* u = it.second;
        if (!inMap(u->BlockDR, u->BlockUR)) continue;
        if (!assaultOn && !inVanguard(u->SN)) continue;

        if (u->Sort == AT_COMPOSITE_BOWMAN || u->Sort == AT_STONE_THROWER) breakShields.push_back(u);
    }
    sort(breakShields.begin(), breakShields.end(), [](const tagArmy* a, const tagArmy* b) { return a->SN < b->SN; });

    // 肉盾到箭塔的归属在突破阶段内保持稳定
    breakTowerIndex.clear();
    for (int i = 0; i < (int)breakTowers.size(); i++) breakTowerIndex[breakTowers[i]->SN] = i;

    for (auto it = towerShield.begin(); it != towerShield.end();)
    {
        const tagArmy* u = army(it->first);
        if (!u || (u->Sort != AT_COMPOSITE_BOWMAN && u->Sort != AT_STONE_THROWER) || !breakTowerIndex.count(it->second))
            it = towerShield.erase(it);
        else ++it;
    }

    breakGroupCnt.assign(breakTowers.size(), 0);
    for (const auto& it : towerShield)
        if (breakTowerIndex.count(it.second)) breakGroupCnt[breakTowerIndex[it.second]]++;

    for (const tagArmy* u : breakShields)
    {
        if (breakTowers.empty() || towerShield.count(u->SN)) continue;

        int pick = -1;
        double bestDis = 0.0;
        for (int i = 0; i < (int)breakTowers.size(); i++)
        {
            const double d = dis(FloatPos(u->DR, u->UR),
                                 centerOf({breakTowers[i]->BlockDR, breakTowers[i]->BlockUR}, breakTowers[i]->Type));
            if (pick < 0 || breakGroupCnt[i] < breakGroupCnt[pick] ||
                (breakGroupCnt[i] == breakGroupCnt[pick] && d < bestDis))
            {
                pick = i;
                bestDis = d;
            }
        }

        towerShield[u->SN] = breakTowers[pick]->SN;
        breakGroupCnt[pick]++;
    }

    for (const tagArmy* u : breakShields)
    {
        auto it = towerShield.find(u->SN);
        if (it == towerShield.end()) continue;
        const tagBuilding* t = enemyBuilding(it->second);
        if (!t) continue;

        if (u->NowState != HUMAN_STATE_WALKING && u->NowState != HUMAN_STATE_IDLE) orders.erase(u->SN);
        orderMove(u->SN, FloatPos(Pos(t->BlockDR, t->BlockUR)));
    }

    // 主力到场后祭司立即切武器厂
    if (assaultOn && siegeSN >= 0) orderAction(priest, siegeSN);
}

void Mgr::offense()
{
    if (base.dr < 0) return;

    if (siegeSN == -1)
        for (const auto& it : eBuildingMap)
            if (it.second->Type == BUILDING_SIEGE)
            {
                siegeSN = it.first;
                siegePos = {it.second->BlockDR, it.second->BlockUR};
                break;
            }

    if (!assaultOn && gameFrame >= ASSAULT_FRAME) assaultOn = true;
    vanguardPick();
    if (!assaultOn && vanguard.empty()) return;

    bool anyTars = false;
    for (const EnemyView& v : ev)
        if (v.inTars)
        {
            anyTars = true;
            break;
        }
    const bool breakNow = !anyTars && siegeSN >= 0;
    if (breakNow != towerBreakOn)
    {
        towerBreakOn = breakNow;
        for (const auto& it : armyMap)  // 切阶段时清掉上一阶段的军队命令, 无明确策略的军队保持原目标
            if (explicitRole(it.second->Sort)) orders.erase(it.first);
        towerShield.clear();  // 下一阶段重新建立稳定的弓兵-箭塔归属
    }

    if (assaultOn)  // 无明确策略的军队: 统一打最近的敌军, 目标有效就不换, 不参与后撤
        for (const auto& it : armyMap)
        {
            const tagArmy& u = *it.second;
            if (explicitRole(u.Sort)) continue;

            const int tar = otherSelector(u);
            if (tar >= 0) orderAction(u.SN, tar);
        }

    if (towerBreakOn)
    {
        priestRushOn = true;
        runTowerBreak();
        return;
    }

    runAssault();
    if (assaultOn && priestRushOn && siegeSN >= 0) orderAction(priest, siegeSN);
}

void Mgr::clearRoad()
{
    if (nav.empty()) return;

    auto inBand = [&](int dr, int ur)
    {
        if (!inMap(dr, ur)) return false;
        const int d = nav[cellIdx(dr, ur)];
        return d >= WAIT_BAND_IN && d <= WAIT_BAND_OUT;
    };

    auto orderCell = [&](const FloatPos& p)
    { return Pos((int)(p.dr / (double)BLOCKSIDELENGTH), (int)(p.ur / (double)BLOCKSIDELENGTH)); };

    roadUnits.clear();
    for (const auto& a : armyMap)
    {
        const tagArmy* u = a.second;
        if (u->Sort == AT_PRIEST) continue;  // 祭司由 scout/offense 独占控制
        if (inVanguard(u->SN) || u->NowState != HUMAN_STATE_IDLE) continue;
        roadUnits.push_back(u);
    }
    sort(roadUnits.begin(), roadUnits.end(), [](const tagArmy* a, const tagArmy* b) { return a->SN < b->SN; });

    // 已经站进等待带
    for (const tagArmy* u : roadUnits)
    {
        if (!inBand(u->BlockDR, u->BlockUR)) continue;
        auto it = orders.find(u->SN);
        if (it != orders.end() && it->second.target < 0 && !it->second.back && !it->second.halt) orders.erase(it);
    }

    roadReserved.clear();

    // 占住当前已在等待带内的军队所在格
    for (const auto& a : armyMap)
    {
        const tagArmy* u = a.second;
        if (u->Sort != AT_PRIEST && inBand(u->BlockDR, u->BlockUR))
            roadReserved.insert(cellIdx(u->BlockDR, u->BlockUR));
    }

    // 占住仍有效的既有等待带目标，避免多个单位拿到同一目的地
    for (const auto& it : orders)
    {
        const Order& o = it.second;
        if (o.target >= 0 || o.back || o.halt || o.stuck) continue;
        if (!army(it.first)) continue;

        const Pos p = orderCell(o.at);
        if (inBand(p.dr, p.ur)) roadReserved.insert(cellIdx(p.dr, p.ur));
    }

    const vector<Pos>& points = waitPoints;  // nav 更新时已构建，不再在 clearRoad 中扫描整图

    for (const tagArmy* u : roadUnits)
    {
        if (inBand(u->BlockDR, u->BlockUR)) continue;

        Pos oldGoal = {-1, -1};
        auto it = orders.find(u->SN);
        if (it != orders.end() && it->second.target < 0 && !it->second.back && !it->second.halt)
        {
            oldGoal = orderCell(it->second.at);

            // 原目标仍有效时继续它；orderMove 自己负责低频补发与 stuck 计数。
            if (!it->second.stuck && inBand(oldGoal.dr, oldGoal.ur))
            {
                orderMove(u->SN, it->second.at);
                continue;
            }
        }

        if (points.empty()) return;

        int pick = -1;
        double best = 0.0;
        const Pos here(u->BlockDR, u->BlockUR);
        for (int i = 0; i < (int)points.size(); i++)
        {
            const Pos& p = points[i];
            const int idx = cellIdx(p.dr, p.ur);
            if (!walkable(p.dr, p.ur)) continue;  // waitPoints 允许短暂跨帧缓存
            if (roadReserved.count(idx)) continue;
            if (oldGoal.dr >= 0 && p == oldGoal) continue;  // stuck 后不立刻抽回同一个点

            const long long dr = (long long)here.dr - p.dr;
            const long long ur = (long long)here.ur - p.ur;
            const double d = (double)(dr * dr + ur * ur);  // 只比较远近，无需 sqrt
            if (pick < 0 || d < best || (d == best && idx < cellIdx(points[pick].dr, points[pick].ur)))
            {
                pick = i;
                best = d;
            }
        }
        if (pick < 0) continue;

        orders.erase(u->SN);  // 新路径必须从干净状态开始，不能继承旧 stuck
        orderMove(u->SN, FloatPos(points[pick]));
        roadReserved.insert(cellIdx(points[pick].dr, points[pick].ur));
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

    if (stage == CIVILIZATION_TOOLAGE)
    {
        phase = 0;
        wantBuilding(BUILDING_MARKET, 1, b_prio--);
        wantTech(BUILDING_MARKET_WOOD_UPGRADE, e_prio--);

        // 伐木科技开始研究后再走兵营/靶场/升级
        const bool woodTech = hasTech(BUILDING_MARKET_WOOD_UPGRADE) || runningTech.count(BUILDING_MARKET_WOOD_UPGRADE);
        if (woodTech)
        {
            wantBuilding(BUILDING_ARMYCAMP, 1, b_prio--);
            wantBuilding(BUILDING_RANGE, 1, b_prio--);
            wantTech(BUILDING_CENTER_UPGRADE, e_prio--);
            wantUnit(AT_FARMER, min(FARMER_MAX, POP_CAP - (int)armyMap.size() - 2), e_prio--);
        }
    }
    else
    {
        if (!hasTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW) || buildingCount(BUILDING_RANGE) < 3) phase = 1;
        else phase = 2;

        wantBuilding(BUILDING_RANGE, 3, b_prio--);

        wantUnit(AT_FARMER, min(FARMER_MAX, POP_CAP - (int)armyMap.size() - 2), e_prio--);

        wantTech(BUILDING_RANGE_UPGRADE_COMPOSITE_BOW, e_prio--);
        wantUnit(AT_COMPOSITE_BOWMAN, 40, e_prio--);
    }

    econPlan(phase);

    if (wantFarm > 0) wantBuilding(BUILDING_FARM, buildingCount(BUILDING_FARM) + wantFarm, FARM_PRIORITY);
}
