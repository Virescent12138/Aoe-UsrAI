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

const int PLACE_NEAR_BASE = 80;      // 紧贴基地
const int PLACE_ADJACENT = 80;       // 紧贴其它建筑
const int PLACE_BAND_BONUS = -60;    // 落在该建筑理想距离带内
const int PLACE_BAND_FAIL = 60;      // 贴太近
const int PLACE_RES_BONUS = -80;     // 靠近本建筑关心的资源
const int PLACE_IMPOSSIBLE = 10000;  // 不可能值
const int PLACE_FAILED = 400;        // 之前建造失败过的地基, 按次数累加
const int PLACE_NEAR_RES = 100;      // 紧贴资源
const int PLACE_SAVE_SCALE = 16;     // 多少搬运帧折算一点选址代价

const int STOCK_PAYBACK = 4;         // 省下的帧数要够建造成本的几倍才动工
const int STOCK_PLAN_GAP = 125;      // 仓库规划多少帧算一次
const int STOCK_SERVE = 6;           // 认为每种资源能使用多少
const int STOCK_DISCOUNT_OTHER = 6;  // 非食物除以这个数值
const int STOCK_MAX_BASE = 2;        // 仓库上限 = BASE + 已过分钟数 / 10

const int SCOUT_VIEW = 12;
const int SCOUT_MIN_GAIN = 8;
const int SCOUT_STUCK = 75;
const int SCOUT_COOLDOWN = 375;
const int SCOUT_HOME_STAY = 25 * 90;
const int SCOUT_ANCHOR_GAP = 250;  // 锚点多少帧重算一次
const int SCOUT_WAVE[3] = {25 * 240, 25 * 540, 25 * 840};

const int CREW_BUILD = 2;  // 一个工地派几个人
const int CREW_LION = 1;   // 打一只狮子派几个人
const int CREW_FIX = 2;    // 修箭塔派几个人

const int PRIEST_MARGIN = 2;  // 祭司站位比敌人射程多留几格
const int ARMY_ALL_IN = 25 * 60 * 16;
const int PRIEST_ALL_IN = (int)(25 * 60 * 18.5);

const int LION_KEEP = 4;    // 狮子视野3
const int ENEMY_KEEP = 10;  // 离敌方单位

const int FIX_TOWER_UNTIL = 25 * 60 * 15;  // 这之后不再修塔
const int LION_HUNT_FROM = 25 * 4 * 60;    // 这之前不主动打狮子

const int ECON_SNAP = 60 * 25;  // 每隔此帧数快照一次库存
const int POP_INF = 2000000000;

// 基础数据结构

struct Pos
{
    int dr = -1, ur = -1;
    Pos() = default;
    Pos(int a, int b) : dr(a), ur(b) {}
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

struct GatherSpot
{
    int sn = -1;
    FloatPos at = {-1, -1};
    Pos stand = {-1, -1};
    double cost = 0;
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
inline double vecLen(const Pos& p) { return std::sqrt((double)p.dr * p.dr + (double)p.ur * p.ur); }
inline double dot(const Pos& a, const Pos& b) { return (double)a.dr * b.dr + (double)a.ur * b.ur; }
inline double angleDeg(const Pos& a, const Pos& b)
{ return std::acos(dot(a, b) / vecLen(a) / vecLen(b)) * 180 / 3.14159265358979323846; }

template <class T>  // bool 应使用 unsigned char
class Grid
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

class Mgr : public UsrAI
{
   public:
    virtual ~Mgr() = default;

   protected:
    void worldRebuild(const tagInfo& info);
    void markFootprint(const tagBuilding& b);

    const tagFarmer* farmer(int sn) const { return look(farmerMap, sn); }
    const tagArmy* army(int sn) const { return look(armyMap, sn); }
    const tagBuilding* building(int sn) const { return look(buildingMap, sn); }
    const tagResource* resource(int sn) const { return look(resourceMap, sn); }
    const tagArmy* enemyArmy(int sn) const { return look(eArmyMap, sn); }
    const tagBuilding* enemyBuilding(int sn) const { return look(eBuildingMap, sn); }

    const std::vector<int>& buildingsOf(int type) const;
    int unitCount(int type) const { return unitCnt[type + 1]; }  // AT_FARMER = -1, 偏移 1
    int buildingCount(int type, bool doneOnly = false) const { return doneOnly ? bldDoneCnt[type] : bldCnt[type]; }

    const tagTerrain& cell(int dr, int ur) const { return (*theMap)[dr][ur]; }
    bool blocked(int dr, int ur) const { return blockCell(dr, ur) != 0; }
    bool canBuild(int dr, int ur) const;              // 地形是否允许建造
    bool walkable(int dr, int ur) const;              // 是否可以行走
    bool canPlace(int dr, int ur, int size) const;    // size*size 的地基是否放得下
    bool nearLion(int dr, int ur, int radius) const;  // 有狮子
    bool enemyCorner(int dr, int ur) const;           // 与基地对角的那一象限
    int lockOf(int enemySN) const;                    // 该敌人锁着的我方SN, 没锁到我方返回 -1

    Stock avail() const { return res - held; }
    bool afford(const Stock& c) const { return avail().covers(c); }

    void navRebuild();  // 以基地占地为源bfs, nav(dr, ur) 即距离, -1 不可达
    // 以 around 起 size*size 方块为种子, 第 r 环(0 为种子自己)加 cost + r * delta, 只记 [inner, outer] 环
    void ringAdd(Grid<int>& g, const Pos& around, int size, int cost, int inner, int outer, int delta = 0);
    void threatRebuild();
    void threatStamp(int td, int tu, int r);
    int threatAt(int dr, int ur) const;
    void phaseUpdate();

    void order(int sn, int targetSN) { HumanAction(sn, targetSN); }
    void orderMoveCell(int sn, const Pos& p)  // 走到该格中心
    { HumanMove(sn, (0.5 + p.dr) * BLOCKSIDELENGTH, (0.5 + p.ur) * BLOCKSIDELENGTH); }
    void orderBuild(int workerSN, int type, int dr, int ur) { HumanBuild(workerSN, type, dr, ur); }
    void orderBuilding(int hostSN, int action) { BuildingAction(hostSN, action); }
    void workerTask(int workerSN, int targetSN);  // 目标没变且不是发呆就不重复下令

    void laborRebuild();  // 重建空闲池
    int nearestOf(const std::vector<int>& cand, const FloatPos& at) const;
    int claimWorker(const FloatPos& at, bool steal = false);  // steal 时可从在岗的人里抢
    void freeWorker(int sn);                                  // 交还空闲池(该村民已阵亡则丢弃)

    virtual bool workerBusy(int sn) const = 0;    // 已被某个岗位登记
    virtual void workerDrop(int sn) = 0;          // 从所有岗位解绑
    virtual bool workerPinned(int sn) const = 0;  // 不可被抢(目前只有农田)

    template <class T>
    static const T* look(const std::unordered_map<int, const T*>& m, int sn)
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
    std::vector<int> armySNs;  // 保持引擎给出的顺序
    std::vector<Pos> lionCells;

    std::unordered_map<int, std::vector<int>> byType;  // 建筑类型 -> SN 列表(引擎顺序)
    std::vector<int> unitCnt, bldCnt, bldDoneCnt;

    Grid<unsigned char> blockCell;  // 被资源或建筑占住的格子
    const std::vector<std::vector<tagTerrain>>* theMap = nullptr;

    int gameFrame = 0;
    Stock res;   // 当前库存
    Stock held;  // 本帧已被生产预定
    int maxHuman = 0;
    int stage = 0;

    /* 初始化后固定 */
    Pos base = {-1, -1};
    FloatPos baseF = {-1, -1, -1};
    int priest = -1;

    Grid<int> nav;                   // 基地距离, -1 表示不可达
    Grid<unsigned char> navMark;     // ringAdd 私有标记
    Grid<int> threat;                // 威胁场
    std::vector<int> threatTbl[17];  // tbl[r][(dx+r)*(2r+1)+(dy+r)] = r - floor(sqrt(dx*dx+dy*dy)) + 1

    int maxStock = STOCK_MAX_BASE;
    bool allInArmy = false;
    bool allInPriest = false;

    std::vector<int> laborPool;                // 空闲人口
    std::unordered_set<int> claimedThisFrame;  // 本帧被认领过的 SN, 防止一帧内反复抢同一人
};

class Economy : public Mgr
{
   protected:
    // 采集
    void gatherFrame();  // 重建仓储点与全部资源池, 清理失效绑定
    void gatherReset();  // 人口分配前清零
    void gatherRun();    // 按 desired 调整人口并下令
    int poolRoom(ResKind k) const;
    int spotsWithin(ResKind k, double limit) const;
    void toPool(int& from, int num, ResKind k, double limit = -1);
    void buildDepots();
    bool standCell(const tagResource* r, Pos& out) const;  // 采集占地分配
    double depotCost(const FloatPos& at, const std::vector<FloatPos>& depots) const;
    void dropSpot(int workerSN, bool toFree);  // 解开一条绑定

    // 打猎
    void huntFrame();  // 聚类重建 + 继承人口 + 清理过期猎物
    void huntReset();
    void huntRun();
    void huntDetach(int sn);
    void formHunts(const std::vector<const tagResource*>& sor, int threshold);
    void restoreStaff();
    void driveHunt(HuntSite& site);
    bool huntable(const tagResource* r) const;
    HuntSite* siteOf(int siteID);         // 按id查本帧片区
    void huntAssign(int x, HuntSite& s);  // 指派x名闲置人员到片区
    void huntFree(int x, HuntSite& s);    // 释放x名人员进闲置队列
    void toHunt(int& from, int num, int siteIdx);

    // 农田
    void farmFrame();  // 刷新已完工农田列表
    void farmRun();    // 维护绑定关系并下耕地令
    void farmUnbind(std::unordered_map<int, int>::iterator it);

    // 人口分配
    void rebalance(int population, const Stock& need);

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

    std::vector<int> farmList;
    std::unordered_map<int, int> farmToWorker;
    std::unordered_map<int, int> workerToFarm;

    Stock prevStock;
    int lastSnapFrame = -100;
    bool phaseChanged = false;
    int popFood = 0, popWood = 0, popStone = 0;
};

class Industry : public Economy
{
   protected:
    // 建造
    void buildFrame();                                             // 清空排队, 按节流重算仓库收益图
    void buildRun();                                               // 维护工地 / 补人 / 开新工地
    void wantBuilding(int buildingType, int total, int priority);  // 该类总数补到 total
    void wantStock(int priority);                                  // 收益够本才修仓库
    int queuedBuild(int type) const;
    Stock buildDemand() const;  // 排队中的建筑还差多少资源
    bool buildAvailable(int type) const;
    Pos findSpot(int type);
    void placeMask(int type, int size);
    void saveMapBuild();

    // 生产
    void prodFrame();                                  // 清空本帧队列
    void prodRun();                                    // 派发生产 / 科技
    void prodDestroy();                                // 人口超编时拆村民
    void wantUnit(int type, int total, int priority);  // 该类总数补到 total
    void wantTech(int action, int priority);           // 一次性科技
    int queuedProd(int action) const;
    bool hasTech(int action) const { return doneTech.count(action) > 0; }
    Stock prodDemand() const;
    bool techAvailable(int action) const;
    int idleHost(int buildingType, const std::set<int>& busy) const;

    std::multiset<std::pair<int, int>> builds;  // 等待建造队列, pair<priority, buildingType>
    std::vector<BuildSite> sites;
    Grid<int> costMap;
    Grid<unsigned char> placeOk;
    std::vector<long long> psum;  // costMap 的二维前缀和, 宽 MAP_U + 1
    std::unordered_map<int, int> failedSpots;

    Grid<int> saveMap;     // 建立仓库带来的搬运帧节省, 索引是占地左上角
    int bestSave = 0;      // saveMap 里放得下的位置中的最大值
    int savePlanned = -1;  // 上次算 saveMap 的帧号

    std::multiset<std::pair<int, int>> prods;  // pair<priority, action>, 从高到低走一遍
    std::unordered_set<int> doneTech;
    int destroyCnt = -1;
};

class War : public Industry
{
   protected:
    // 侦察
    void scoutFrame();  // 挑一个还活着的侦察单位
    void scoutRun();
    void scoutFlood(const Pos& from, bool avoidThreat);  // 逐格bfs; avoidThreat 表示不许穿威胁格
    void unknownRebuild();
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
    void defenceRun();  // 处理来袭波次, 置 combat
    void fixTower();
    void towerRun();
    void priestRun();
    void armyRun();

    // 进攻
    void offenseRun();  // 全军出击, 我咬死你
    void clearRoad();   // 闲兵散到基地外围, 免得堵路

    // 雷霆狮子
    void lionRun();

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

    bool combat = false;        // 本帧祭司不探图
    std::vector<int> hostiles;  // 本帧要处理的敌人SN, 升序
    std::vector<int> towerAtk;
    int priestTarget = -1;  // 祭司正在转换的敌人SN
    std::set<int> fixCrew;  // 正在修箭塔的人

    Pos corner = {-1, -1};
    Pos watchPoint = {-1, -1};
    bool been = false;
    int siegeSN = -1;
    Grid<unsigned char> offenseMark;

    std::set<int> lionCrew;  // 正在打狮子的人
};

class Brain : public War
{
   public:
    void update(const tagInfo& info);

    bool useStock = false;        // 是否修建仓库
    bool useLiveHunting = false;  // 是否打猎
    bool usePopModel = false;     // 是否销毁多余村民

   private:
    void strategyRun();
    int farmerTargetByPop() const;  // usePopModel 时的村民上限

    bool workerBusy(int sn) const override;
    void workerDrop(int sn) override;
    bool workerPinned(int sn) const override { return workerToFarm.count(sn) > 0; }
};

/*##########YOUR CODE ENDS HERE##########*/
#endif  // USRAI_H
