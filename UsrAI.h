#ifndef USRAI_H
#define USRAI_H

#include <unordered_map>

#include "ai.h"

extern tagGame tagUsrGame;
extern ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/

class UsrAI : public AI
{
   public:
    UsrAI() { this->id = 0; }
    ~UsrAI() {}

   private:
    void processData() override;
    tagInfo getInfo() { return tagUsrGame.getInfo(); }
    int AddToIns(instruction ins) override
    {
        UsrIns.lock.lock();
        ins.id = UsrIns.g_id;
        UsrIns.g_id++;
        UsrIns.instructions.push(ins);
        UsrIns.lock.unlock();
        return ins.id;
    }
    void clearInsRet() override { tagUsrGame.clearInsRet(); }
    /*##########DO NOT MODIFY THE CODE IN THE CLASS##########*/
};

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>

const double EPS = 1e-5;

enum
{
    AT_FARMER = -1
};

// 建筑参数
const int PLACE_ADJACENT = 100;  // 紧贴其它建筑
const int PLACE_BONUS = -60;     // 落在该建筑理想距离带内
const int PLACE_FAILED = 400;    // 之前建造失败过的地基, 按次数累加
const int DEPOT_FAR = 12;        // 工作点离最近存放点超过这么多格产生智能仓储需求
const int CREW_BUILD = 2;        // 一个工地派几个人
const int CREW_FIX = 2;          // 修箭塔派几个人

// 侦察
const int SCOUT_VIEW = 12;                                 // 侦察视野
const int SCOUT_MIN_GAIN = 8;                              // 至少探明这么多格才有价值
const int SCOUT_COOLDOWN = 375;                            // 确认走不到的路径点, 冷却这么久
const int SCOUT_FLEE_R = 16;                               // 避险时扫描安全格的窗口半径
const int SCOUT_DONE = 2;                                  // 离路径点这么多格内就算站到了
const int SCOUT_HOME_DONE = 5;                             // 离集合点这么多格内就算回到了
const double SCOUT_DETOUR = 1.5;                           // 直线距离折算成实际路程的系数
const int SCOUT_HOME_STAY = 25 * 90;                       // 回避时间
const int SCOUT_WAVE[3] = {25 * 240, 25 * 540, 25 * 840};  // 波次
const int ENEMY_KEEP = 10;                                 // 敌方单位的警戒圈
const int SCOUT_RETRY = 25;
const int MOVE_RETRY = 25;
const double MOVE_GAIN = 0.5;  // 两次 IDLE 之间至少靠近这么多格才算有进展

// 总攻
const int ASSAULT_FRAME = 25 * 60 * 16;    // 发动进攻
const double RETREAT_BOW = 4.0;            // 敌人进到这个格距就后撤
const double RETREAT_STONE = 6.0;          // 敌人进到这个格距就后撤
const int RETREAT_STEP = 3;                // 后撤跨这么多格合并下令
const int SLOT_COOLDOWN = 50;              // 子位冷却
const double MOVE_DONE = 0.2;              // 距目标子位小于这个格距即视为到位
const int HOME_KEEP = 10;                  // 出动前, 基地附近至少留这么多复合弓守家
const int HOME_RANGE = 40;                 // 算作"基地附近"的格距
const int BELONG_CORNER = 50;              // 分隔攻守判据
const int DEF_ALERT = 45;                  // 进到这个距离才算来袭波次
const int TOWER_ALERT = 55;                // 提前点名范围
const int FIX_TOWER_UNTIL = 25 * 60 * 15;  // 这之后不再修塔
const int WAIT_BAND_IN = 22;               // 待命部队散开到离基地此距离以外
const int WAIT_BAND_OUT = 26;              // 待命部队散开到离基地此距离以内
const int PRIEST_STAY = 30;                // 祭司跟大部队时到攻城厂保持的格距
const int PRIEST_STAY_BLIND = 45;          // 攻城厂尚未定位
const int PRIEST_STAY_BAND = 5;            // 落在 [STAY, STAY+BAND] 里就不再动

// 经济参数
const int CARRY_LIMIT = 10;           // 村民荷载
const double BASE_RATE_BUSH = 0.5;    // 浆果, 个/秒
const double BASE_RATE_CORPSE = 1.0;  // 猎物尸体, 个/秒
const double BASE_RATE_FARM = 0.5;    // 农田, 个/秒
const double BASE_RATE_GOLD = 1.0;    // 金矿, 个/秒
const double BASE_RATE_WOOD = 1.0;    // 木材, 个/秒
const int FARM_PRIORITY = 90;         // 农田在建造队列里的优先级
const int BUILD_WAIT = 25;            // 等地基出现的帧数
const int POP_CAP = 50;               // 人口上限
const int FARMER_MIN = 15;            // 村民数下限
const int FARMER_MAX = 26;            // 村民数上限
const int RES_RANGE = 60;             // 排除太远的食物位点

// 各阶段人员比例, 顺序 木 食 金
const int ECON_WEIGHT[3][3] = {{3, 7, 0}, {6, 3, 3}, {1, 4, 4}};

// 辅助结构
struct Pos
{
    int dr = -1, ur = -1;
    Pos() = default;
    Pos(int a, int b) : dr(a), ur(b) {}

    bool operator==(const Pos& b) const { return dr == b.dr && ur == b.ur; }
};

struct FloatPos
{
    double dr = -1.0, ur = -1.0;
    FloatPos() = default;
    FloatPos(const Pos& p) : dr((p.dr + 0.5) * BLOCKSIDELENGTH), ur((p.ur + 0.5) * BLOCKSIDELENGTH) {}
    FloatPos(double a, double b) : dr(a), ur(b) {}
};

struct Stock
{
    int wood = 0, meat = 0, stone = 0, gold = 0;

    Stock& operator+=(const Stock& o)
    {
        wood += o.wood;
        meat += o.meat;
        stone += o.stone;
        gold += o.gold;
        return *this;
    }
    Stock& operator-=(const Stock& o)
    {
        wood -= o.wood;
        meat -= o.meat;
        stone -= o.stone;
        gold -= o.gold;
        return *this;
    }
    friend Stock operator-(Stock a, const Stock& b) { return a -= b; }

    bool covers(const Stock& c) const { return wood >= c.wood && meat >= c.meat && stone >= c.stone && gold >= c.gold; }
};

// 资源种类
enum ResKind
{
    RK_WOOD,
    RK_GOLD,
    RK_BUSH,
    RK_GAZELLE,  // 特指羚羊
    RK_COUNT     // 计算效率时指农田
};

// 人口分配
enum EconRes
{
    E_WOOD,
    E_FOOD,
    E_GOLD,
    E_COUNT
};

enum FoodKind
{
    F_CORPSE,
    F_BUSH,
    F_FARM
};

struct GatherSpot
{
    int sn = -1;
    Pos stand = {-1, -1};
    double cost = 0;  // 落脚点到最近存放点的像素距离
    double rate = 0;  // 综合搬运距离后的每秒产出
};

struct GatherPool
{
    std::vector<GatherSpot> spots;  // 默认按运输距离；食物规划时重排
    int desired = 0;
};

struct BuildSite
{
    int type = -1;
    Pos site = {-1, -1};
    int sn = -1;   // 地基SN, 出现前为 -1
    int born = 0;  // 下达建造令的帧号, 用来给地基出现留宽限
    std::set<int> workers;
};

struct FoodPlan
{
    std::vector<int> jobs;  // 按收益从高到低排序的 FoodKind
    int cursor = 0;         // 前 cursor 个岗位已经派了人
};

struct ProdOrder
{
    int priority;
    int action;
    bool tech;
};

struct MoveOrder
{
    FloatPos at;         // 目标点
    int slot = -1;       // 占用的子位
    bool back = false;   // 后撤令不可被打断
    bool stuck = false;  // 行军令已确认过不去, 不再对同一目标重复下令
    double best = 0;     // 至今最接近目标的格距
    int idle = 0;        // 连续没有进展的 IDLE 帧数
};

inline bool inMap(int dr, int ur) { return dr >= 0 && ur >= 0 && dr < MAP_L && ur < MAP_U; }
inline int cellIdx(int dr, int ur) { return dr * MAP_U + ur; }
inline Pos cellPos(int idx) { return Pos(idx / MAP_U, idx % MAP_U); }

template <typename T>
inline double dis(const T& a, const T& b)
{
    const double ddr = a.dr - b.dr, dur = a.ur - b.ur;
    return std::sqrt(ddr * ddr + dur * dur);
}

// 按打分挑一格: score 返回负数表示排除, 否则越小越好。
// radius < 0 扫全图, 否则只扫 around 周围的窗口。
template <class F>
inline Pos bestCell(F score, const Pos& around = Pos(0, 0), int radius = -1)
{
    const int loD = radius < 0 ? 0 : std::max(0, around.dr - radius);
    const int hiD = radius < 0 ? MAP_L - 1 : std::min(MAP_L - 1, around.dr + radius);
    const int loU = radius < 0 ? 0 : std::max(0, around.ur - radius);
    const int hiU = radius < 0 ? MAP_U - 1 : std::min(MAP_U - 1, around.ur + radius);

    Pos best = {-1, -1};
    double bestScore = 0;
    for (int i = loD; i <= hiD; i++)
        for (int j = loU; j <= hiU; j++)
        {
            const double s = score(i, j);
            if (s < 0) continue;
            if (best.dr >= 0 && s >= bestScore) continue;
            best = {i, j};
            bestScore = s;
        }
    return best;
}

inline double gatherRate(ResKind k, double dropDis)  // 农田传入 RK_COUNT
{
    static const double rate[RK_COUNT + 1] = {BASE_RATE_WOOD, BASE_RATE_GOLD, BASE_RATE_BUSH, BASE_RATE_CORPSE,
                                              BASE_RATE_FARM};

    double gatherSec = CARRY_LIMIT / rate[k];
    double walkSec = 2.0 * dropDis / (HUMAN_SPEED * 25.0);
    return CARRY_LIMIT / (gatherSec + walkSec);
}

int buildingSize(int type);
int resourceSize(int type);
int buildWoodCost(int type);
ResKind kindOf(int resourceType);
Stock actionCost(int action);
int actionHost(int action);
int typeToAction(int type);

inline FloatPos centerOf(const Pos& p, int buildingType)  // 建筑的几何中心
{
    const double half = buildingSize(buildingType) * 0.5;
    return FloatPos((p.dr + half) * BLOCKSIDELENGTH, (p.ur + half) * BLOCKSIDELENGTH);
}

inline Pos resourceCell(const tagResource* r)  // 资源格点
{
    if (resourceSize(r->Type) == 1) return Pos(r->BlockDR, r->BlockUR);
    return Pos((int)(r->DR / BLOCKSIDELENGTH + 0.5) - 1, (int)(r->UR / BLOCKSIDELENGTH + 0.5) - 1);
}

class Mgr : public UsrAI
{
   public:
    virtual ~Mgr() = default;
    void update(const tagInfo& info);

   private:
    // 每帧信息
    void makeFrame(const tagInfo& info);
    void mark(const tagBuilding& b);  // 构建建筑的 blockcell

    std::unordered_map<int, const tagFarmer*> farmerMap;
    std::unordered_map<int, const tagArmy*> armyMap;
    std::unordered_map<int, const tagBuilding*> buildingMap;
    std::unordered_map<int, const tagResource*> resourceMap;
    std::unordered_map<int, const tagArmy*> eArmyMap;
    std::unordered_map<int, const tagBuilding*> eBuildingMap;

    std::unordered_map<int, std::vector<int>> byType;  // 建筑类型 -> SN 列表(默认顺序)
    std::vector<int> unitCnt, bldCnt, bldDoneCnt;

    template <class T>
    static const T* get(const std::unordered_map<int, const T*>& m, int sn)
    {
        auto it = m.find(sn);
        return it == m.end() ? nullptr : it->second;
    }

    const tagFarmer* farmer(int sn) const { return get(farmerMap, sn); }
    const tagArmy* army(int sn) const { return get(armyMap, sn); }
    const tagBuilding* building(int sn) const { return get(buildingMap, sn); }
    const tagResource* resource(int sn) const { return get(resourceMap, sn); }
    const tagArmy* enemyArmy(int sn) const { return get(eArmyMap, sn); }
    const tagBuilding* enemyBuilding(int sn) const { return get(eBuildingMap, sn); }

    const std::vector<int>& buildingsOf(int type) const;
    int unitCount(int type) const { return unitCnt[type + 1]; }  // AT_FARMER = -1, 偏移 1
    int buildingCount(int type, bool doneOnly = false) const { return doneOnly ? bldDoneCnt[type] : bldCnt[type]; }

    // 地形与位置判定
    const tagTerrain& cell(int dr, int ur) const { return (*theMap)[dr][ur]; }
    bool blocked(int dr, int ur) const { return blockCell[cellIdx(dr, ur)] != 0; }
    bool valid(int dr, int ur) const;               // 地形是否允许建造
    bool walkable(int dr, int ur) const;            // 是否可以行走
    bool canPlace(int dr, int ur, int size) const;  // size*size 的地基是否放得下
    bool enemyCorner(int dr, int ur) const;         // 与基地对角的那一象限
    int lockOf(int enemySN) const;                  // 该敌人锁着的我方SN, 没锁到我方返回 -1, 锁到祭司返回 -1

    // 库存
    Stock available() const { return res - held; }
    bool afford(const Stock& c) const { return available().covers(c); }

    // 距离场与代价图
    void fieldBuild(std::vector<int>& out, const Pos& src, int size);  // 可走格的 bfs 距离场
    void ringAdd(std::vector<int>& g, const Pos& around, int size, int cost, int inner, int outer,
                 int delta = 0);         // bfs环带变体
    int threatAt(int dr, int ur) const;  // 威胁查询

    // 下令
    void moveToCell(int sn, const Pos& p)  // 走到该格中心
    { HumanMove(sn, (0.5 + p.dr) * BLOCKSIDELENGTH, (0.5 + p.ur) * BLOCKSIDELENGTH); }
    void sendAction(int workerSN, int targetSN);  // 智能命令

    // 村民调度
    void laborFrame();  // 重建空闲池
    void laborRelease();
    int takeNearest(const FloatPos& at, bool steal = false);  // steal 时可从在岗的人里抢
    void freeWorker(int sn);                                  // 交还空闲池(该村民已阵亡则丢弃)

    static int targetOf(const std::unordered_map<int, int>& jobs, int workerSN);  // target -> worker 的反查
    bool workerBusy(int sn) const;                                                // 已被某个岗位登记
    bool workerReserved(int sn) const;  // 在专职岗位上(农田/工地/修塔/打狮子), 不许被抢
    void workerDrop(int sn);            // 从所有岗位解绑

    // 全局帧状态
    std::vector<unsigned char> blockCell;  // 被资源或建筑占住的格子, cellIdx 索引
    const std::vector<std::vector<tagTerrain>>* theMap = nullptr;

    int gameFrame = 0;
    Stock res;   // 当前库存
    Stock held;  // 本帧已被生产预定
    int stage = 0;

    // 固定信息
    Pos base = {-1, -1};
    FloatPos baseF = {-1, -1};
    int priest = -1;

    std::vector<int> nav;        // 基地距离, -1 表示不可达
    std::vector<int> laborPool;  // 空闲人口

    // 采集
    void gatherFrame();                                    // 重建仓储点与全部资源池, 清理失效绑定
    void runGather();                                      // 按 desired 调整人口并下令
    bool standCell(const tagResource* r, Pos& out) const;  // 采集占地分配
    double depotCost(const FloatPos& at, int depotType) const;
    void dropSpot(int workerSN, bool toFree);  // 解开一条绑定

    // 农田
    void farmFrame();  // 刷新已完工农田列表
    void runFarm();    // 维护绑定关系并下耕地令
    void unbind(std::unordered_map<int, int>::iterator it);

    // 人口分配
    int econPick(int phase, const int count[E_COUNT], const int cap[E_COUNT]) const;
    FoodPlan planFood();            // 按产出排序食物岗位, 同时重排采集池与农田表
    bool takeFood(FoodPlan& plan);  // 取下一个食物岗位, 没得取了返回 false
    void econPlan(int phase);

    GatherPool pools[RK_COUNT];
    std::unordered_map<int, int> workerOfSpot;  // 资源SN -> 村民SN
    std::vector<unsigned char> standTaken;      // 已被某个资源点占用的落脚格

    std::vector<int> farmList;                  // 已完工农田, 按单人产出降序
    std::unordered_map<int, int> farmToWorker;  // 农田SN -> 村民SN

    int farmDesired = 0;  // 本帧目标农田岗位数
    int wantFarm = 0;     // 本帧规划新建几块农田

    // 建造
    void buildFrame();                                             // 清空排队, 重算仓库收益图
    void runBuild();                                               // 维护建造
    void releaseBuilders(BuildSite& s);                            // 释放工地人员
    void wantBuilding(int buildingType, int total, int priority);  // 该类总数补到 total
    void wantDepot(int depotType, int priority);                   // 有远端需求时补一座(首座谷仓无条件)

    void depotWant(ResKind k, std::vector<Pos>& out) const;  // 远端有人采集的点触发需求, 取最远的当锚点
    bool depotCovered(int depotType, const Pos& c) const;    // 是否已覆盖
    double depotBenefit(int depotType, const Pos& site) const;
    bool depotRoom(const Pos& c) const;  // 该点附近放得下一座存放点
    int queuedBuild(int type) const;
    bool buildAvailable(int type) const;
    Pos findSpot(int type);

    // 生产
    void prodFrame();                                  // 清空本帧队列
    void runProd();                                    // 处理生产
    void runDestroy();                                 // 人口超编时拆村民(先解绑再自毁)
    void wantUnit(int type, int total, int priority);  // 该类总数补到 total
    void wantTech(int action, int priority);           // 一次性科技
    int queuedProd(int action) const;
    int projectCount(int action) const;  // 当前有多少建筑正在执行该 action
    bool hasTech(int action) const { return doneTech.count(action) > 0; }
    bool techAvailable(int action) const;
    int idleHost(int buildingType, const std::set<int>& busy) const;

    std::vector<std::pair<int, int>> builds;  // 本帧建造需求, pair<priority, buildingType>
    std::vector<BuildSite> sites;
    std::unordered_map<long long, int> failedSpots;  // (buildingType, cell) -> fail count

    std::vector<Pos> granaryPendings;  // 谷仓选址的加权点: 远端浆果簇与远端农田
    std::vector<Pos> stockPendings;    // 仓库选址的加权点: 远端猎物簇与远端金矿簇

    std::vector<ProdOrder> prods;         // 本帧生产/科技需求
    std::unordered_set<int> runningTech;  // 已经下令且尚未完成
    std::unordered_set<int> doneTech;     // 仅在 Project 结束后进入

    // 侦察: 目标可以在迷雾里, 到不了引擎会尽量靠近后转 IDLE, 所以不需要自建路径
    void runScout();
    int wpGain(const Pos& c) const;                       // c 为圆心半径 SCOUT_VIEW 内的未知格数
    int pickWaypoint(const Pos& here, Pos& stand) const;  // 最近的还有收益的路径点, 返回其下标
    int homeETA(const Pos& here);                         // 回家还要几帧(顺带更新 home)
    bool isExplore(int eta) const;
    Pos fleeGoal(const Pos& here) const;  // 窗口内最近的零威胁格, 找不到就回家
    // 维护祭司的移动令; 确认目标过不去时返回 true
    bool scoutGoto(const Pos& p, const Pos& here, bool idle);

    // 防守
    void defence();  // 处理来袭波次, 置 combat
    void fixTower();
    void runTower();
    int defenceSelector(const tagArmy& u) const;
    void runDefenders();

    bool combat = false;        // 本帧祭司不探图
    std::vector<int> hostiles;  // 本帧要处理的敌人SN
    std::set<int> fixCrew;      // 正在修箭塔的人

    // 进攻
    void offense();                    // 进攻总调度: 定位对角与攻城厂, 派兵
    void offenseUpdate();              // 清理死人留下的移动令, 更新 tars
    int siegeDis(const Pos& p) const;  // 到攻城厂的格距, 未定位返回角落运算

    int attackSelector(const tagArmy& u) const;
    void vanguardPick();  // 大部队出动前维持一支提前批次
    bool inVanguard(int sn) const { return vanguard.count(sn) > 0; }
    void runAssault();
    void runAtkPriest();  // 祭司
    void clearRoad();     // 借过一下

    static FloatPos slotAt(int slot);                                            // 单位当前实际站的子位
    vector<int> slotOf(const tagArmy& u, const FloatPos& p) const;
    void slotClaim(const tagArmy& u, int seed);                                  // 按单位占地占位
    bool slotFree(int seed, const tagArmy& u) const;                             // 该单位放得下
    int slotStep(const tagArmy& u, const Pos& from, const FloatPos& ref) const;  // 沿 nav 后撤一格
    int pickSlot(const tagArmy& u);                                              // 连撤 RETREAT_STEP 格

    double enemyGap(const FloatPos& at) const;                                 // 到最近敌军的格距, 没有敌军返回很大值
    FloatPos marchGoal() const;                                                // 行军总目标: 攻城厂, 没定位就是对角
    void sendMove(const tagArmy& u, const FloatPos& at, int slot, bool back);  // 占位 + 登记 + 下令
    void marchTo(const tagArmy& u, const FloatPos& at);                        // 长途行军, 全交给引擎寻路
    bool keepMove(const tagArmy& u, bool interrupt);                           // 维护在途命令, 仍应继续走返回 true

    Pos corner = {-1, -1};  // 与基地对角的地图角
    int siegeSN = -1;
    Pos siegePos = {-1, -1};

    bool assaultOn = false;
    std::unordered_set<int> vanguard;  // 提前出动的复合弓; assaultOn 之后清空并入大部队

    std::vector<int> tars;
    std::vector<int> slot;       // 2*MAP_L × 2*MAP_U 的1/4格占位图
    std::vector<int> slotBlack;  // 子位拉黑到期帧

    std::unordered_map<int, MoveOrder> moveGoal;

    // 探图
    // 每轴 MAP_L / SCOUT_VIEW + 1 个点, 下标 idx = i * 每轴点数 + j 对应 Pos(i, j) * SCOUT_VIEW
    std::vector<int> wpCooldown;        // 引擎走不到的路径点的冷却到期帧, 过期自动解除
    std::vector<unsigned char> wpDone;  // 已经站到过或已经看光的路径点
    int goalWp = -1;                    // 目标路径点下标, -1 表示还没选
    Pos goalStand = {-1, -1};           // 当前路径点本身, 允许落在迷雾里
    Pos scoutSent = {-1, -1};           // 上次下令去的格, 用来避免重复下令
    double scoutBest = 0;               // 本条移动令至今最接近目标的格距
    int scoutIdle = 0;                  // 连续没有进展的 IDLE 帧数
    Pos home = {-1, -1};

    // 策略
    void strategy();
    int farmerTarget() const;  // 本帧村民目标数, 生产和自毁共用同一个口径
};

/*##########YOUR CODE ENDS HERE##########*/
#endif  // USRAI_H
