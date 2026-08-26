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
#include <iterator>
#include <map>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>

const double EPS = 1e-5;

struct Pos
{
    int dr = -1, ur = -1;
    Pos() = default;
    Pos(int a, int b) : dr(a), ur(b) {}

    bool operator<(const Pos& b) const { return dr != b.dr ? dr < b.dr : ur < b.ur; }
};

struct Wave  // bfs 的参数
{
    int cost = 0;         // 每一环的基础代价
    int inner = 0;        // 从这一环起开始记
    int outer = 1 << 29;  // 记到这一环为止(含)
    int delta = 0;        // 每远离一环, 代价的增量

    Wave(int c) : cost(c) {}
    Wave& band(int i, int o) { return inner = i, outer = o, *this; }  // 只记 [i, o] 这几环
    Wave& upto(int o) { return outer = o, *this; }                    // 记到第 o 环
    Wave& grad(int d) { return delta = d, *this; }
    int at(int r) const { return cost + r * delta; }
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
};

class Mgr : public UsrAI
{
   private:
    /* 每帧基础数据 */
    std::unordered_map<int, const tagFarmer*> farmers;
    std::unordered_map<int, const tagArmy*> armies;
    std::unordered_map<int, const tagBuilding*> buildings;
    std::unordered_map<int, const tagResource*> resources;
    std::unordered_map<int, const tagArmy*> enemyArmies;
    std::unordered_map<int, const tagBuilding*> enemyBuildings;
    std::unordered_set<const tagResource*> lions;
    std::unordered_set<int> ally;
    std::vector<int> freeFarmers;  // 空闲人口

    int gameFrame = 0;
    Stock stock;
    int humanMax = 0;
    int civStage = 0;
    Stock reserved;

    const std::vector<std::vector<tagTerrain>>* theMap = nullptr;
    const tagTerrain& cell(int dr, int ur) const { return (*theMap)[dr][ur]; }

    std::vector<bool> blockCell;  // 被资源或建筑占住的格子(索引dr*MAP_U+ur)
    std::vector<Pos> lionsPos;    // 避让

    /* 初始化后固定 */
    Pos basePos = {-1, -1};
    FloatPos baseFloatPos = {-1, -1, -1};
    int priestSN = -1;

    /* 采集 */
    GatherPool pools[RK_COUNT];

    std::unordered_map<int, int> spotOfWorker;  // 村民SN -> 资源SN
    std::unordered_map<int, int> workerOfSpot;  // 资源SN -> 村民SN

    std::vector<int> distMap;   // 基地距离(一维, 索引 dr*MAP_U+ur)
    std::vector<bool> claimed;  // 已被某个资源点占用的落脚格

    std::vector<FloatPos> foodDepots;  // 基地 + 谷仓
    std::vector<FloatPos> resDepots;   // 基地 + 仓库

    static ResKind kindOf(int resourceType);               // 资源类型 -> 池
    void floodReach();                                     // 基地距离
    bool standCell(const tagResource* r, Pos& out) const;  // 采集占地分配
    double depotCost(const FloatPos& at, const std::vector<FloatPos>& depots) const;

    void buildGather();                        // 每帧重建全部池, 清理
    void runGather();                          // 按 desired 调整人口并下令
    void dropSpot(int workerSN, bool toFree);  // 解开一条绑定

    int poolRoom(ResKind k) const;
    int spotsWithin(ResKind k, double limit) const;
    void toPool(int& from, int num, ResKind k, double limit = -1);

    /* 打活猎物 */
    static int s_nextSiteID;

    std::vector<HuntSite> hunts;

    std::unordered_map<int, HuntSite*> huntByID;  // 片区id -> 本帧片区
    std::unordered_map<int, int> huntOfSN;        // 猎物SN -> 片区id
    std::unordered_map<int, int> staffHunt;       // 村民SN -> 片区id
    std::unordered_map<int, int> huntPrey;        // 片区id -> 正在打的猎物SN

    HuntSite* huntOf(int siteID);  // 按id查本帧片区
    void formHunts(const std::vector<const tagResource*>& sor, const int threshold);
    void restoreStaff();  // 继承人口

    void runHunt();
    void driveHunt(HuntSite& site);
    bool huntable(const tagResource* r) const;

    void assign(int x, HuntSite& site);   // 指派x名闲置人员到片区
    void release(int x, HuntSite& site);  // 释放x名人员进闲置队列
    void toHunt(int& from, int num, HuntSite& site);

    /* 建筑管理 */
    void wantBuilding(int buildingType, int total, int priority);  // 该类总数补到 total

    void runBuild();
    int queuedBuild(int type) const;

    std::multiset<std::pair<int, int>> builds;  // 等待建造队列, pair<priority, buildingType>

    std::set<int> builders;
    int constructType = -1;
    Pos constructSite = {-1, -1};
    int constructSN = -1;  // 地基SN, 出现前为 -1

    std::vector<int> costMap;
    std::vector<long long> sumMap;  // costMap 的二维前缀和
    std::unordered_map<int, int> failedSpots;

    std::vector<bool> placeOk;  // (索引dr*MAP_U+ur)
    void buildPlaceMask(int type, int size);

    std::vector<int> saveMap;  // 建立仓库效率提升量, 索引是占地
    int bestSave = 0;          // saveMap 里放得下的位置中的最大值
    int savePlanned = -1;      // 上次算 saveMap 的帧号
    void buildSaveMap();
    void wantStock(int priority);

    Pos findSpot(int type);

    // 第 r 环(0 表示种子自己)加 cost + r * delta, 只记 [inner, outer] 这几环.
    void bfs(std::vector<int>& Map, const std::vector<Pos>& seeds, const Wave& w);
    void bfs(std::vector<int>& Map, const Pos& around, int size, const Wave& w);  // 以 size*size 的方块为种子

    /* 探图系统 */
    int scoutSN = -1;
    std::vector<int> threatMap, scoutDist, scoutPrev;
    std::vector<unsigned char> bfsUsed;  // bfs 复用(一维, 索引 dr*MAP_U+ur)
    std::vector<Pos> route;              // 逐格的路径, 不含起点
    int routeAt = 0;                     // 还没走到的第一格
    int routeSent = -1;                  // 上次下令去的那格在route里的下标
    bool routeFlee = false;              // 当前route是逃跑路线还是探图路线

    // 每轴 MAP_L / SCOUT_VIEW + 1 个点, 下标 idx = i * 每轴点数 + j 对应 Pos(i, j) * SCOUT_VIEW
    std::vector<int> unknownRow;  // 每行未知格的前缀和
    std::vector<int> wpCooldown;  // 卡住过的路径点的冷却到期帧, 过期自动解除
    std::vector<bool> wpDone;     // 已经站到过的路径点
    int goalWp = -1;              // 目标路径点下标, -1 表示还没选
    Pos goalStand = {-1, -1};     // 探图时是goalWp, 回家时是基地旁
    bool homeBound = false;       // 正在为波次回家

    int lastCell = -1;      // 上次记录的祭司所在格
    int lastMoveFrame = 0;  // 上次换格的帧号

    void scout();

    void buildThreat();
    bool enemyCorner(int dr, int ur) const;
    int threatAt(int dr, int ur) const;
    void floodThreat(const Pos& from, bool avoidThreat);  // 逐格bfs; avoidThreat 表示不许穿威胁格

    void buildUnknown();
    int wpGain(const Pos& c) const;                          // c 为圆心半径 SCOUT_VIEW 范围内的未知格数
    bool nearestStand(const Pos& c, int r, Pos& out) const;  // c 附近 r 格内最近的可达格
    int pickWaypoint(Pos& stand) const;                      // 最近的还有收益的路径点, 返回其下标
    int homeETA(const Pos& here, Pos& home) const;           // 回家还要几帧
    int scoutState(int eta) const;

    void buildRoute(const Pos& goal);
    bool routeSafe() const;                        // 剩下的格子还安全
    bool routeWalkable() const;                    // 剩下的格子还走得通
    bool followRoute(const Pos& here, bool idle);  // 沿route推进一段, 走完或走不动返回false

    bool evade(const Pos& here, bool idle);  // 站在威胁里就往最近的安全格跑; 返回真表示这帧不探图
    Pos fleeGoal() const;                    // 最近的零威胁格; 全是威胁时取最轻的那格

    /* 农田管理 */
    std::vector<int> farmList;
    std::unordered_map<int, int> farmToWorker;
    std::unordered_map<int, int> workerToFarm;

    void unbindFarm(std::unordered_map<int, int>::iterator it);  // 解开 farmToWorker 里的这条绑定
    void manageFarms();                                          // 每帧维护绑定关系并下耕地令

    /* 生产管理 */
    std::multiset<std::pair<int, int>> prods;          // pair<priority, action>, 从高到低走一遍, 每栋空闲建筑都能接一条
    void wantUnit(int type, int total, int priority);  // 该类总数补到 total
    void wantTech(int action, int priority);           // 一次性科技

    void runProduction();
    int queuedProd(int action) const;

    std::set<int> doneTech;

    int idleHost(int buildingType, const std::set<int>& busy) const;  // 这类里空着的一栋, busy 是本帧已用掉的

    /* 军队应战 */
    void defence();  // 处理来袭波次, 置 inCombat
    bool attack();   // 全军出击, 我咬死你
    void killLions();
    void clearRoad();

    bool allIn = false;
    Pos final = {-1, -1};
    Pos watchPoint = {-1, -1};

    std::set<int> lionCrew;  // 正在打狮子的人
    std::set<int> fixCrew;   // 正在修箭塔的人

    bool inCombat = false;      // 本帧祭司不探图
    std::vector<int> hostiles;  // 本帧要处理的敌人SN, 升序

    int priestTarget = -1;  // 祭司正在转换的敌人SN
    int siegeSN = -1;

    int atkRange(int sort) const;          // 兵种攻击距离(格)
    double dpsOf(const tagArmy& e) const;  // 兔头
    int lockOf(int enemySN) const;         // 该敌人锁着的我方SN, 没锁到我方返回 -1

    void runTower();
    void runArmy();
    void runPriest();
    void fixTower();

    /* 工具 */
    template <class T>
    static const T* get(const std::unordered_map<int, const T*>& m, int sn)
    {
        auto it = m.find(sn);
        return it == m.end() ? nullptr : it->second;
    }
    const tagFarmer* farmer(int sn) const { return get(farmers, sn); }
    const tagArmy* army(int sn) const { return get(armies, sn); }
    const tagBuilding* building(int sn) const { return get(buildings, sn); }
    const tagResource* resource(int sn) const { return get(resources, sn); }

    int nearestOf(const std::vector<int>& cand, const FloatPos& at) const;
    int takeNearestFree(const FloatPos& at, bool allowSteal);

    bool valid(int dr, int ur) const;  // 是否可以建造

    bool afford(const Stock& c) const;  // 扣掉 reserved 后

    Stock actionCost(int action) const;
    static int actionHost(int action);
    static int typeToAction(int type);

    static double disSq(const Pos& a, const Pos& b);
    static double disSq(const FloatPos& a, const FloatPos& b);
    static double dis(const FloatPos& a, const FloatPos& b);
    int buildingSize(int type) const;
    int resourceSize(int type) const;

    bool walkable(int dr, int ur) const;
    bool nearLion(const std::vector<Pos>& src, int dr, int ur, int radius) const;

    int buildWoodCost(int type) const;
    int buildStoneCost(int type) const;
    bool canPlace(int dr, int ur, int size) const;

    std::vector<int> _unit, _building;  // 计数预处理
    int cntUnit(int type) { return _unit[type + 1]; }
    int cntBuilding(int type) { return _building[type]; }
    std::unordered_map<int, std::vector<int>> buildingsByType;  // 建筑类型 -> SN 列表

    void sendAction(int workerSN, int targetSN);  // 发布命令

    int destroyCnt = -1;
    void destroyLater();

   public:
    Mgr() = default;
    virtual ~Mgr() = default;

    void update(const tagInfo& info);
    void makeFrame(const tagInfo& info);
};

/*##########YOUR CODE ENDS HERE##########*/
#endif  // USRAI_H
