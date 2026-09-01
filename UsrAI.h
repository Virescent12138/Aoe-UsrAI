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

/*##########YOUR CODE BEGINS HERE##########*/

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>

const double EPS = 1e-5;

enum
{
    AT_FARMER = -1
};

// 可调参数

const int PLACE_BASE = 100;          // 紧贴基地
const int PLACE_ADJACENT = 80;       // 紧贴其它建筑
const int PLACE_BONUS = -60;         // 落在该建筑理想距离带内
const int PLACE_IMPOSSIBLE = 10000;  // 不可能值
const int PLACE_FAILED = 400;        // 之前建造失败过的地基, 按次数累加
const int PLACE_FAIL_CAP = 4;        // 单一建筑类型在同一位置的失败惩罚上限

const int DEPOT_FAR = 10;            // 实际采集/耕作点离最近存放点超过这么多格才产生智能仓储需求
const int DEPOT_BENEFIT = 40;        // 每节省一格搬运距离给仓储候选位置的奖励

const int SCOUT_VIEW = 12;                                 // 侦察视野
const int SCOUT_MIN_GAIN = 8;                              // 至少探明这么多格才有价值
const int SCOUT_STUCK = 75;                                // 卡住
const int SCOUT_COOLDOWN = 375;                            // 暂时不去了
const int SCOUT_HOME_STAY = 25 * 90;                       // 回避时间
const int SCOUT_ANCHOR_GAP = 250;                          // 锚点多少帧重算一次
const int SCOUT_WAVE[3] = {25 * 240, 25 * 540, 25 * 840};  // 波次

const int CREW_BUILD = 2;  // 一个工地派几个人
const int CREW_LION = 1;   // 打一只狮子派几个人
const int CREW_FIX = 2;    // 修箭塔派几个人

const int PRIEST_MARGIN = 2;  // 祭司站位比敌人射程多留几格

// 总攻
const int ASSAULT_FRAME = 25 * 60 * 23;  // 全军同时抵达集结点的帧号
const double PATH_FACTOR = 1.35;         // 绕路系数
const int GO_STRIDE = 5;                 // nextToGo 目标点的采样步长
const int GO_MIN_GAIN = 8;               // 视野内未知格少于这么多就不值得跑一趟
const int PRIEST_KEEP = 24;              // 祭司待命点离攻城厂的格数

const int LION_KEEP = 4;    // 狮子视野3
const int ENEMY_KEEP = 10;  // 离敌方单位

const int FIX_TOWER_UNTIL = 25 * 60 * 15;  // 这之后不再修塔
const int LION_HUNT_FROM = 25 * 4 * 60;    // 这之前不主动打狮子

// enemyai 进攻相关数据复刻
const int BELONG_DEF = 40;  // 分隔攻守判据
const int LURE_HOLD = 2;    // 点名后保护几帧

// 实测经济参数
const int CARRY_LIMIT = 10;                 // 村民荷载
const double BASE_RATE_BUSH = 0.5;          // 浆果, 个/秒
const double BASE_RATE_CORPSE = 1.0;        // 猎物尸体, 个/秒
const double BASE_RATE_FARM = 0.5;          // 农田, 个/秒
const double BASE_RATE_GOLD = 1.0;          // 金矿, 个/秒
const double BASE_RATE_STONE = 1.0;         // 石矿, 个/秒
const double BASE_RATE_WOOD = 1.0;          // 木材, 个/秒
const double HUNT_DPS = 5.0;                // 村民对活猎物的每秒伤害
const int CREW_HUNT = 2;                    // 食物不足且尸体少时派几个人去杀猎物
const int FARM_PRIORITY = 96;               // 农田在建造队列里的优先级

// 经济不再追逐瞬时缺口：阶段预设负责长期结构，库存带只调少量机动人口。
const int ECON_REPLAN = 25 * 15;            // 每 15 秒最多进行一次跨资源换岗
const int ECON_FLEX_MAX = 4;                // 最多约 20% 人口作为机动人口
const int ECON_FLEX_PER_RES = 2;            // 单一资源最多额外吸收两个机动工，防止极端分配
const int ECON_JOB_HOLD = ECON_REPLAN;       // 新岗位至少稳定一个经济周期
const int DEPOT_MIN_WORKERS = 2;             // 远端真实采集至少两人才值得新建仓储

// 基础数据结构

struct Pos
{
    int dr = -1, ur = -1;
    Pos() = default;
    Pos(int a, int b) : dr(a), ur(b) {}

    bool operator==(const Pos& b) { return dr == b.dr && ur == b.ur; }
};

struct FloatPos
{
    double dr = -1.0, ur = -1.0;
    int sn = -1;  // 如果只表示点坐标, 不需要修改其值

    FloatPos() = default;
    FloatPos(const Pos& p)
    {
        dr = p.dr * BLOCKSIDELENGTH + 0.5 * BLOCKSIDELENGTH;
        ur = p.ur * BLOCKSIDELENGTH + 0.5 * BLOCKSIDELENGTH;
    }
    FloatPos(double a, double b, int s = -1)
    {
        dr = a;
        ur = b;
        sn = s;
    }
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
    friend Stock operator+(Stock a, const Stock& b) { return a += b; }
    friend Stock operator-(Stock a, const Stock& b) { return a -= b; }

    bool covers(const Stock& c) const { return wood >= c.wood && meat >= c.meat && stone >= c.stone && gold >= c.gold; }
};

enum ResKind
{
    RK_WOOD,
    RK_STONE,
    RK_GOLD,
    RK_BUSH,
    RK_CORPSE,
    RK_COUNT
};

const int ECON_FARM = RK_COUNT;
const int ECON_HUNT = RK_COUNT + 1;
const int ECON_GROUP_COUNT = RK_COUNT + 2;

struct EconProfile
{
    int weight[4];  // 木 食 石 金：阶段人口分布预设
    int low[4];     // 低于 low 打开补库状态
    int high[4];    // 补到 high 才关闭，形成天然滞回
};

// 库存带按 config 的主要阶段开销设计：升铜 800 食物；三箭塔 450 石头；
// 学院阶段约 430 木材；军队阶段持续消耗食物和黄金。石头用 120~180 留作维修缓冲。
const EconProfile ECON_PROFILE[4] = {
    {{4, 6, 0, 0}, {260, 700, 120,   0}, {450, 900, 180,   0}},
    {{3, 4, 3, 0}, {180, 220, 350,   0}, {300, 380, 550,   0}},
    {{5, 5, 0, 0}, {320, 220, 120,   0}, {520, 380, 180,   0}},
    {{2, 5, 0, 4}, {180, 320, 120, 250}, {320, 550, 180, 450}},
};

struct GatherSpot
{
    int sn = -1;
    FloatPos at = {-1, -1};
    Pos stand = {-1, -1};
    double cost = 0;  // 落脚点到最近存放点的像素距离
    double rate = 0;  // 综合搬运距离后的每秒产出
};

struct GatherPool
{
    std::vector<GatherSpot> spots;  // 按 cost 升序
    int desired = 0;
};

struct HuntSite  // 活猎物聚类, 每帧重建
{
    int id = -1;
    FloatPos center = {-1, -1};
    std::vector<int> members;  // 猎物SN
    std::vector<int> staff;    // 村民SN
    int desired = 0;
};

struct BuildSite
{
    int type = -1;
    Pos site = {-1, -1};
    int sn = -1;  // 地基SN, 出现前为 -1
    std::set<int> workers;
};

inline bool inMap(int dr, int ur) { return dr >= 0 && ur >= 0 && dr < MAP_L && ur < MAP_U; }
inline int cellIdx(int dr, int ur) { return dr * MAP_U + ur; }
inline Pos cellPos(int idx) { return Pos(idx / MAP_U, idx % MAP_U); }

inline double disSq(const Pos& a, const Pos& b)
{
    const double ddr = a.dr - b.dr, dur = a.ur - b.ur;
    return ddr * ddr + dur * dur;
}
inline double disSq(const FloatPos& a, const FloatPos& b)
{
    const double ddr = a.dr - b.dr, dur = a.ur - b.ur;
    return ddr * ddr + dur * dur;
}
inline double dis(const FloatPos& a, const FloatPos& b) { return std::sqrt(disSq(a, b)); }
inline int dis(const Pos& a, const Pos& b) { return std::sqrt(disSq(a, b)); }
inline double len(const Pos& p) { return std::sqrt((double)p.dr * p.dr + (double)p.ur * p.ur); }
inline double dot(const Pos& a, const Pos& b) { return (double)a.dr * b.dr + (double)a.ur * b.ur; }
inline double angle(const Pos& a, const Pos& b)
{ return std::acos(dot(a, b) / len(a) / len(b)) * 180 / 3.14159265358979323846; }

inline double baseGatherRate(ResKind k)
{
    switch (k)
    {
        case RK_WOOD: return BASE_RATE_WOOD;
        case RK_STONE: return BASE_RATE_STONE;
        case RK_GOLD: return BASE_RATE_GOLD;
        case RK_BUSH: return BASE_RATE_BUSH;
        case RK_CORPSE: return BASE_RATE_CORPSE;
        default: return 0.0;
    }
}

// 基础采集速率是“资源/秒”；dropDis 是浮点坐标距离。
// HUMAN_SPEED 为“浮点坐标/帧”，游戏 25 帧/秒。
// 满载周期 = 采满 CARRY_LIMIT + 往返最近存货点。
inline double transportRate(double baseRate, double dropDis)
{
    if (baseRate <= EPS) return 0.0;
    const double gatherSec = CARRY_LIMIT / baseRate;
    const double walkSec = 2.0 * dropDis / (HUMAN_SPEED * 25.0);
    return CARRY_LIMIT / (gatherSec + walkSec);
}

inline double gatherRate(ResKind k, double dropDis)
{ return transportRate(baseGatherRate(k), dropDis); }

template <class T>  // bool -> unsigned char
class Grid          // 二维压一维的优化内存分布
{
   public:
    void reset(const T& init) { v.assign((size_t)MAP_L * MAP_U, init); }
    bool ready() const { return !v.empty(); }
    void fill(const T& val) { std::fill(v.begin(), v.end(), val); }

    T& operator()(int dr, int ur) { return v[(size_t)dr * MAP_U + ur]; }
    const T& operator()(int dr, int ur) const { return v[(size_t)dr * MAP_U + ur]; }
    T& operator()(const Pos& p) { return (*this)(p.dr, p.ur); }
    const T& operator()(const Pos& p) const { return (*this)(p.dr, p.ur); }

   private:
    std::vector<T> v;
};

int buildingSize(int type);
int resourceSize(int type);
int buildWoodCost(int type);
int buildStoneCost(int type);
ResKind kindOf(int resourceType);
Stock actionCost(int action);
int actionHost(int action);
int typeToAction(int type);
int atkRange(int sort);          // 兵种攻击距离(格)
double dpsOf(const tagArmy& e);  // 兔头
double unitSpeed(int sort);      // 兵种移动速度(像素/帧)

class Mgr : public UsrAI
{
   public:
    virtual ~Mgr() = default;

    void update(const tagInfo& info);  // 每帧主循环

    // 下文是一些测试中可能不会真的启用的功能
    bool usePopModel = false;    // 是否销毁多余村民

   private:
    // 每帧信息
    void makeFrame(const tagInfo& info);
    void mark(const tagBuilding& b);

    const tagFarmer* farmer(int sn) const { return get(farmerMap, sn); }
    const tagArmy* army(int sn) const { return get(armyMap, sn); }
    const tagBuilding* building(int sn) const { return get(buildingMap, sn); }
    const tagResource* resource(int sn) const { return get(resourceMap, sn); }
    const tagArmy* enemyArmy(int sn) const { return get(eArmyMap, sn); }
    const tagBuilding* enemyBuilding(int sn) const { return get(eBuildingMap, sn); }

    const std::vector<int>& buildingsOf(int type) const;
    int unitCount(int type) const { return unitCnt[type + 1]; }  // AT_FARMER = -1, 偏移 1
    int buildingCount(int type, bool doneOnly = false) const { return doneOnly ? bldDoneCnt[type] : bldCnt[type]; }

    const tagTerrain& cell(int dr, int ur) const { return (*theMap)[dr][ur]; }
    bool blocked(int dr, int ur) const { return blockCell(dr, ur) != 0; }
    bool valid(int dr, int ur) const;                 // 地形是否允许建造
    bool walkable(int dr, int ur) const;              // 是否可以行走
    bool canPlace(int dr, int ur, int size) const;    // size*size 的地基是否放得下
    bool nearLion(int dr, int ur, int radius) const;  // 有狮子
    bool enemyCorner(int dr, int ur) const;           // 与基地对角的那一象限
    int lockOf(int enemySN) const;                    // 该敌人锁着的我方SN, 没锁到我方返回 -1, 锁到祭司返回 -1

    Stock available() const { return res - held; }
    bool afford(const Stock& c) const { return available().covers(c); }

    void navBuild();  // 以基地占地为源bfs, nav(dr, ur) 即距离, -1 不可达
    // 以 around 起 size*size 方块为种子, 第 r 环(0 为种子自己)加 cost + r * delta, 只记 [inner, outer] 环
    void ringAdd(Grid<int>& g, const Pos& around, int size, int cost, int inner, int outer, int delta = 0);
    void threatBuild();
    void threatStamp(int td, int tu, int r);
    int threatAt(int dr, int ur) const;

    void moveToCell(int sn, const Pos& p)  // 走到该格中心
    { HumanMove(sn, (0.5 + p.dr) * BLOCKSIDELENGTH, (0.5 + p.ur) * BLOCKSIDELENGTH); }
    void sendAction(int workerSN, int targetSN);  // 智能命令, 兼容 runLure

    void laborBuild();  // 重建空闲池
    void laborRelease();
    int nearestOf(const std::vector<int>& cand, const FloatPos& at) const;
    int takeNearest(const FloatPos& at, bool steal = false);  // steal 时可从在岗的人里抢
    void freeWorker(int sn);                                  // 交还空闲池(该村民已阵亡则丢弃)

    bool workerBusy(int sn) const;  // 已被某个岗位登记
    void workerDrop(int sn);        // 从所有岗位解绑
    bool workerPinned(int sn) const { return workerToFarm.count(sn) > 0; }

    template <class T>
    static const T* get(const std::unordered_map<int, const T*>& m, int sn)
    {
        auto it = m.find(sn);
        return it == m.end() ? nullptr : it->second;
    }

    std::unordered_map<int, const tagFarmer*> farmerMap;
    std::unordered_map<int, const tagArmy*> armyMap;
    std::unordered_map<int, const tagBuilding*> buildingMap;
    std::unordered_map<int, const tagResource*> resourceMap;
    std::unordered_map<int, const tagArmy*> eArmyMap;
    std::unordered_map<int, const tagBuilding*> eBuildingMap;
    std::unordered_set<const tagResource*> lionSet;
    std::unordered_set<int> allySet;
    std::vector<int> armySNs;  // 默认顺序
    std::vector<Pos> lionCells;

    std::unordered_map<int, std::vector<int>> byType;  // 建筑类型 -> SN 列表(默认顺序)
    std::vector<int> unitCnt, bldCnt, bldDoneCnt;

    Grid<unsigned char> blockCell;  // 被资源或建筑占住的格子
    const std::vector<std::vector<tagTerrain>>* theMap = nullptr;

    int gameFrame = 0;
    Stock res;   // 当前库存
    Stock held;  // 本帧已被生产预定
    int maxHuman = 0;
    int stage = 0;

    // 固定信息
    Pos base = {-1, -1};
    FloatPos baseF = {-1, -1, -1};
    int priest = -1;

    Grid<int> nav;  // 基地距离, -1 表示不可达
    Grid<unsigned char> navUsed;
    Grid<int> threat;                // 威胁场
    std::vector<int> threatTbl[17];  // tbl[r][(dx+r)*(2r+1)+(dy+r)] = r - floor(sqrt(dx*dx+dy*dy)) + 1

    std::vector<int> laborPool;       // 空闲人口
    std::unordered_set<int> claimed;  // 本帧被认领过的 SN, 防止一帧内反复抢同一人

    // 采集
    void arrangeGather();  // 重建仓储点与全部资源池, 清理失效绑定
    void gatherReset();    // 人口分配前清零
    void runGather();      // 按 desired 调整人口并下令
    void buildDepots();
    bool standCell(const tagResource* r, Pos& out) const;  // 采集占地分配
    double depotCost(const FloatPos& at, const std::vector<FloatPos>& depots) const;
    void dropSpot(int workerSN, bool toFree);  // 解开一条绑定

    // 打猎
    void huntFrame();  // 聚类重建 + 继承人口 + 清理过期猎物
    void huntReset();
    void arrangeHunt();
    void huntDetach(int sn);
    void formHunts(const std::vector<const tagResource*>& sor, int threshold);
    void restoreStaff();
    void runHunt(HuntSite& site);
    bool huntable(const tagResource* r) const;
    HuntSite* huntOf(int siteID);         // 按id查本帧片区
    void huntAssign(int x, HuntSite& s);  // 指派x名闲置人员到片区
    void huntFree(int x, HuntSite& s);    // 释放x名人员进闲置队列

    // 农田
    void farmFrame();  // 刷新已完工农田列表
    void runFarm();    // 维护绑定关系并下耕地令
    void unbind(std::unordered_map<int, int>::iterator it);

    // 人口分配
    void econPlan();
    int econPhase() const;
    void econCommit();  // ideal target -> 带滞回/换岗预算的 committed target
    bool jobHeld(int sn, int group) const;
    double marginalGatherRate(ResKind k, int assigned) const;  // 第 assigned+1 名工人的实际边际效率

    GatherPool pools[RK_COUNT];
    std::unordered_map<int, int> spotOfWorker;  // 村民SN -> 资源SN
    std::unordered_map<int, int> workerOfSpot;  // 资源SN -> 村民SN
    Grid<unsigned char> standTaken;             // 已被某个资源点占用的落脚格
    std::vector<FloatPos> foodDepots;           // 基地 + 谷仓
    std::vector<FloatPos> resDepots;            // 基地 + 仓库

    int nextSiteID = 0;
    std::vector<HuntSite> hunts;                  // 按离基地由近及远
    std::unordered_map<int, HuntSite*> huntByID;  // 片区id -> 本帧片区
    std::unordered_map<int, int> huntOfSN;        // 猎物SN -> 片区id
    std::unordered_map<int, int> staffHunt;       // 村民SN -> 片区id
    std::unordered_map<int, int> huntPrey;        // 片区id -> 正在打的猎物SN

    std::vector<int> farmList;      // 已完工农田, 按单人产出降序
    std::vector<double> farmRates;  // 与 farmList 同序
    std::unordered_map<int, int> farmToWorker;
    std::unordered_map<int, int> workerToFarm;

    int farmDesired = 0;  // committed 的农田岗位数
    int wantFarm = 0;     // 本帧规划新建几块农田

    int econCommitted[ECON_GROUP_COUNT] = {};
    int econIdeal[ECON_GROUP_COUNT] = {};
    bool econBoost[4] = {};                   // 木 食 石 金是否处于补库状态
    int econLastFrame = -ECON_REPLAN;
    bool econInitialized = false;
    std::unordered_map<int, int> workerJobSince;  // 村民进入当前普通经济岗位的帧号

    // 建造
    void buildFrame();                                             // 清空排队, 重算仓库收益图
    void runBuild();                                               // 维护建造
    void wantBuilding(int buildingType, int total, int priority);  // 该类总数补到 total
    void wantStock(int priority);                                  // 两名以上村民实际在远端采尸体时补仓库
    void wantGranary(int priority);                                // 第一座保底；之后只响应真实远端浆果/农田需求

    void depotWant(ResKind k, std::vector<Pos>& out) const;  // 两名以上真实远端采集工才产生一个仓储锚点
    bool depotCovered(int depotType, const Pos& c) const;    // 已完成/在建仓储是否已覆盖该服务区
    double depotBenefit(int depotType, const Pos& site) const;
    bool depotRoom(const Pos& c) const;                      // 该点附近放得下一座存放点
    int queuedBuild(int type) const;
    bool buildAvailable(int type) const;
    Pos findSpot(int type);
    void buildPlaceMask(int type, int size);

    // 生产
    void prodFrame();                                  // 清空本帧队列
    void runProd();                                    // 处理生产
    void runDestroy();                                 // 人口超编时拆村民
    void wantUnit(int type, int total, int priority);  // 该类总数补到 total
    void wantTech(int action, int priority);           // 一次性科技
    int queuedProd(int action) const;
    bool hasTech(int action) const { return doneTech.count(action) > 0; }
    bool techAvailable(int action) const;
    int idleHost(int buildingType, const std::set<int>& busy) const;

    std::multiset<std::pair<int, int>> builds;  // 等待建造队列, pair<priority, buildingType>
    std::vector<BuildSite> sites;
    Grid<int> costMap;
    Grid<unsigned char> placeOk;
    std::vector<long long> psum;  // costMap 的二维前缀和, 宽 MAP_U + 1
    std::unordered_map<long long, int> failedSpots;  // (buildingType, cell) -> fail count

    std::vector<Pos> granaryPendings;  // 要采但离谷仓太远的浆果
    std::vector<Pos> stockPendings;    // 要采但离仓库太远的猎物尸体

    std::multiset<std::pair<int, int>> prods;  // pair<priority, action>, 从高到低
    std::unordered_set<int> techOrders;   // 本帧 prods 中哪些 action 是科技
    std::unordered_set<int> runningTech;  // 已经下令且尚未完成
    std::unordered_set<int> doneTech;     // 仅在 Project 结束后进入
    int destroyCnt = -1;

    // 侦察
    void scoutFrame();  // 挑一个还活着的侦察单位, 目前默认祭司
    void runScout();
    void floodThreat(const Pos& from, bool avoidThreat);  // 逐格bfs; avoidThreat 表示不许穿威胁格
    void buildUnknown();
    int wpGain(const Pos& c) const;                          // c 为圆心半径 SCOUT_VIEW 内的未知格数
    bool nearestStand(const Pos& c, int r, Pos& out) const;  // c 附近 r 格内最近的可达格
    int pickWaypoint(Pos& stand) const;                      // 最近的还有收益的路径点, 返回其下标
    int homeETA(const Pos& here);                            // 回家还要几帧(顺带更新 home)
    bool isExplore(int eta) const;
    void buildRoute(const Pos& goal);
    bool routeSafe() const;                        // 剩下的格子还安全
    bool followRoute(const Pos& here, bool idle);  // 沿route推进一段, 走完或走不动返回false
    bool evade(const Pos& here, bool idle);        // 站在威胁里就往最近的安全格跑
    Pos fleeGoal() const;                          // 最近的零威胁格; 全是威胁时取最轻的那格

    // 防守
    void defence();  // 处理来袭波次, 置 combat
    void fixTower();
    void runTower();
    void runPriest();
    void runArmy();

    bool combat = false;        // 本帧祭司不探图
    std::vector<int> hostiles;  // 本帧要处理的敌人SN, 升序
    std::vector<int> towerAtk;  // 本帧要骗索敌的敌人SN, 升序
    int priestTarget = -1;      // 祭司正在转换的敌人SN
    std::set<int> fixCrew;      // 正在修箭塔的人

    // 进攻
    void offense();                     // 进攻总调度
    void offenseInit();                 // 定位对角与攻城厂, 刷新集结点; 没定位好返回 false
    void offenseUpdate();               // 清理死人留下的锁, 刷新 runLure , 更新 tars
    int siegeDis(const Pos& p) const;   // 到攻城厂的格距, 未定位返回角落运算
    void runLure();                     // 全员点名, 批量破坏敌方索敌

    int assultETA(const tagArmy& u);               // 按自身速度算, 走到集结点还要几帧
    void DispatchMove();                                 // 按速度错峰出发
    void nextToGoBuild();                                // 重建未探索目标点表
    Pos nextToGo(const tagArmy& u, bool occupy = true);  // 最近的未探索目标点
    int selector(const tagArmy& u);                      // 目标选择器
    void runAssult();                                    // 战斗
    void runAtkPriest();                                 // 祭司
    void clearRoad();                                    // 借过一下

    Pos corner = {-1, -1};  // 与基地对角的地图角
    int siegeSN = -1;
    Pos siegePos = {-1, -1};
    Grid<unsigned char> clearRoadUsed;

    std::vector<Pos> goList;         // 推进点
    std::vector<bool> goUsed;        // 推进点使用状态
    std::unordered_set<int> onMove;  // 已经出发的单位
    bool assaultOn = false;          // 全军出击

    std::vector<int> tars; // 敌人SN列表

    std::vector<int> lureList;
    std::unordered_set<int> lureSeen;
    int lureCursor = 0;                     // 轮转游标
    std::unordered_map<int, int> lureHold;  // 村民 SN -> 保护到哪一帧

    // 雷霆狮子
    void killLions();
    std::set<int> lionCrew;  // 正在打狮子的人

    // 探图
    int scoutSN = -1;
    Grid<int> scoutDist, scoutPrev;
    std::vector<Pos> route;  // 逐格的路径, 不含起点
    int routeAt = 0;         // 还没走到的第一格
    int routeSent = -1;      // 上次下令去的那格的一维索引, -1 表示尚未发命令
    bool routeFlee = false;  // 当前route是逃跑路线还是探图路线

    // 每轴 MAP_L / SCOUT_VIEW + 1 个点, 下标 idx = i * 每轴点数 + j 对应 Pos(i, j) * SCOUT_VIEW
    std::vector<int> unknownRow;        // 每行未知格的前缀和, 宽 MAP_U + 1
    std::vector<int> wpCooldown;        // 卡住过的路径点的冷却到期帧, 过期自动解除
    std::vector<unsigned char> wpDone;  // 已经站到过的路径点
    int goalWp = -1;                    // 目标路径点下标, -1 表示还没选
    Pos goalStand = {-1, -1};           // 探图时是goalWp, 回家时是基地旁
    bool arrived = false;
    Pos anchor;
    Pos home;
    int lastAnchorChanged = -SCOUT_ANCHOR_GAP;
    FloatPos lastPos = {-1, -1};
    int lastRecordFrame = 0;

    // 策略
    void strategy();
    int farmerTargetCnt() const;  // usePopModel 时的村民上限, 支持自毁
};

/*##########YOUR CODE ENDS HERE##########*/
#endif  // USRAI_H
