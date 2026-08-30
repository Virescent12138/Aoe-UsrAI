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
#include <iterator>
#include <map>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>

const double EPS = 1e-5;

enum
{
    AT_FARMER = -1
};

// 支持结构体

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

namespace geo  // 几何计算
{
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
inline double len(const Pos& p) { return std::sqrt((double)p.dr * p.dr + (double)p.ur * p.ur); }
inline double dot(const Pos& a, const Pos& b) { return (double)a.dr * b.dr + (double)a.ur * b.ur; }
inline double angle(const Pos& a, const Pos& b)
{ return std::acos(dot(a, b) / len(a) / len(b)) * 180 / 3.14159265358979323846; }
}  // namespace geo

template <class T>  // bool 应使用 unsigned char 代替
class Grid
{
   public:
    void reset(const T& init) { v.assign((size_t)MAP_L * MAP_U, init); }
    bool ready() const { return !v.empty(); }

    static int index(int dr, int ur) { return dr * MAP_U + ur; }
    static Pos of(int idx) { return Pos(idx / MAP_U, idx % MAP_U); }
    static bool inside(int dr, int ur) { return dr >= 0 && ur >= 0 && dr < MAP_L && ur < MAP_U; }

    T& operator()(int dr, int ur) { return v[(size_t)dr * MAP_U + ur]; }
    const T& operator()(int dr, int ur) const { return v[(size_t)dr * MAP_U + ur]; }
    T& operator()(const Pos& p) { return (*this)(p.dr, p.ur); }
    const T& operator()(const Pos& p) const { return (*this)(p.dr, p.ur); }
    T& raw(int idx) { return v[idx]; }
    const T& raw(int idx) const { return v[idx]; }

    void fill(const T& val) { std::fill(v.begin(), v.end(), val); }

   private:
    std::vector<T> v;
};

class PrefixSum2D
{
   public:
    void build(const Grid<int>& src);
    long long rect(int dr, int ur, int size) const;  // [dr, dr+size) x [ur, ur+size)

   private:
    int W = 0;
    std::vector<long long> s;
};

namespace tune
{
const int PLACE_NEAR_BASE = 80;      // 紧贴基地
const int PLACE_ADJACENT = 80;       // 紧贴其它建筑
const int PLACE_PER_RING = 1;        // 每远离基地一环(当前未接入 findSpot, 保留)
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
const int STOCK_MAX_BASE = 2;        // 仓库上限 = BASE + 已过分钟数 / 10

const int SCOUT_VIEW = 12;

const int SCOUT_MIN_GAIN = 8;
const int SCOUT_STUCK = 75;
const int SCOUT_COOLDOWN = 375;
const int SCOUT_HOME_STAY = 25 * 90;
const int SCOUT_ANCHOR_GAP = 250;  // 锚点多少帧重算一次
const int SCOUT_WAVE[3] = {25 * 240, 25 * 540, 25 * 840};

const int BUILD_CREW = 2;  // 一个工地派几个人
const int LION_CREW = 1;   // 打一只狮子派几个人

const int FIX_CREW = 2;       // 修箭塔派几个人
const int PRIEST_MARGIN = 2;  // 祭司站位比敌人射程多留几格

const int ARMY_ALL_IN = 25 * 60 * 16;
const int PRIEST_ALL_IN = (int)(25 * 60 * 18.5);

const int LION_KEEP = 4;    // 狮子视野3
const int ENEMY_KEEP = 10;  // 离敌方单位

const int FIX_TOWER_UNTIL = 25 * 60 * 15;  // 这之后不再修塔
const int LION_HUNT_FROM = 25 * 4 * 60;    // 这之前不主动打狮子

const int POP_INF = 2000000000;
}  // namespace tune

// 资源

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

// 村民不够时是否允许把在岗的人抢过来
enum class Steal
{
    No,
    Allow
};

// 世界信息

class World
{
   public:
    void rebuild(const tagInfo& info);

    int frame() const { return gameFrame; }
    const Stock& stock() const { return res; }
    int humanMax() const { return maxHuman; }
    int civStage() const { return stage; }

    const tagFarmer* farmer(int sn) const { return look(farmerMap, sn); }
    const tagArmy* army(int sn) const { return look(armyMap, sn); }
    const tagBuilding* building(int sn) const { return look(buildingMap, sn); }
    const tagResource* resource(int sn) const { return look(resourceMap, sn); }
    const tagArmy* enemyArmy(int sn) const { return look(eArmyMap, sn); }
    const tagBuilding* enemyBuilding(int sn) const { return look(eBuildingMap, sn); }

    const std::unordered_map<int, const tagFarmer*>& farmers() const { return farmerMap; }
    const std::unordered_map<int, const tagArmy*>& armies() const { return armyMap; }
    const std::unordered_map<int, const tagBuilding*>& buildings() const { return buildingMap; }
    const std::unordered_map<int, const tagResource*>& resources() const { return resourceMap; }
    const std::unordered_map<int, const tagArmy*>& enemyArmies() const { return eArmyMap; }
    const std::unordered_map<int, const tagBuilding*>& enemyBuildings() const { return eBuildingMap; }
    const std::unordered_set<const tagResource*>& lions() const { return lionSet; }
    const std::vector<Pos>& lionsPos() const { return lionCells; }
    const std::vector<int>& armyOrder() const { return armySNs; }  // 保持引擎给出的顺序
    bool isAlly(int sn) const { return allySet.count(sn) > 0; }

    const std::vector<int>& buildingsOf(int type) const;
    int unitCount(int type) const { return unitCnt[type + 1]; }  // AT_FARMER = -1, 偏移 1
    int buildingCount(int type, bool doneOnly = false) const { return doneOnly ? bldDoneCnt[type] : bldCnt[type]; }

    const tagTerrain& cell(int dr, int ur) const { return (*theMap)[dr][ur]; }
    bool blocked(int dr, int ur) const { return blockCell(dr, ur) != 0; }
    bool valid(int dr, int ur) const;                 // 是否可以建造
    bool walkable(int dr, int ur) const;              // 是否可以行走
    bool canPlace(int dr, int ur, int size) const;    // size*size 的地基是否放得下
    bool nearLion(int dr, int ur, int radius) const;  // 有狮子

    Pos basePos() const { return base; }
    const FloatPos& baseAt() const { return baseF; }
    int priestSN() const { return priest; }
    bool enemyCorner(int dr, int ur) const;  // 与基地对角的那一象限

    static int buildingSize(int type);
    static int resourceSize(int type);
    static int buildWoodCost(int type);
    static int buildStoneCost(int type);
    static ResKind kindOf(int resourceType);
    static Stock actionCost(int action);
    static int actionHost(int action);
    static int typeToAction(int type);
    static int atkRange(int sort);          // 兵种攻击距离(格)
    static double dpsOf(const tagArmy& e);  // 兔头

    int lockOf(int enemySN) const;  // 该敌人锁着的我方SN, 没锁到我方返回 -1

    void clearReserved() { held = Stock(); }
    void reserve(const Stock& c) { held += c; }
    const Stock& reserved() const { return held; }
    Stock available() const { return res - held; }
    bool afford(const Stock& c) const { return available().covers(c); }

   private:
    template <class T>
    static const T* look(const std::unordered_map<int, const T*>& m, int sn)
    {
        auto it = m.find(sn);
        return it == m.end() ? nullptr : it->second;
    }
    void markFootprint(const tagBuilding& b);

    std::unordered_map<int, const tagFarmer*> farmerMap;
    std::unordered_map<int, const tagArmy*> armyMap;
    std::unordered_map<int, const tagBuilding*> buildingMap;
    std::unordered_map<int, const tagResource*> resourceMap;
    std::unordered_map<int, const tagArmy*> eArmyMap;
    std::unordered_map<int, const tagBuilding*> eBuildingMap;
    std::unordered_set<const tagResource*> lionSet;
    std::unordered_set<int> allySet;
    std::vector<int> armySNs;
    std::vector<Pos> lionCells;

    std::unordered_map<int, std::vector<int>> byType;  // 建筑类型 -> SN 列表(引擎顺序)
    std::vector<int> unitCnt, bldCnt, bldDoneCnt;

    Grid<unsigned char> blockCell;  // 被资源或建筑占住的格子
    const std::vector<std::vector<tagTerrain>>* theMap = nullptr;

    int gameFrame = 0;
    Stock res;
    Stock held;
    int maxHuman = 0;
    int stage = 0;

    /* 初始化后固定 */
    Pos base = {-1, -1};
    FloatPos baseF = {-1, -1, -1};
    int priest = -1;
};

// 通用导航

class NavGrid
{
   public:
    explicit NavGrid(const World& w) : w(w) {}

    void rebuild();  // 以基地占地为源bfs
    int dist(int dr, int ur) const { return d(dr, ur); }
    int dist(const Pos& p) const { return d(p); }
    bool reachable(int dr, int ur) const { return d(dr, ur) != -1; }

    // 第 r 环(0 表示种子自己)加 cost + r * delta, 只记 [inner, outer] 这几环
    void bfs(Grid<int>& map, const std::vector<Pos>& seeds, const Wave& wv);
    void bfs(Grid<int>& map, const Pos& around, int size, const Wave& wv);  // size*size 方块为种子

   private:
    const World& w;
    Grid<int> d;                  // 基地距离, -1 表示不可达
    Grid<unsigned char> scratch;  // bfs 私有标记
};

// 威胁场

class ThreatField
{
   public:
    explicit ThreatField(const World& w) : w(w) {}
    void rebuild();
    int at(int dr, int ur) const;

   private:
    // tbl[r][(dx+r)*(2r+1)+(dy+r)] = r - floor(sqrt(dx*dx + dy*dy)) + 1
    void stamp(int td, int tu, int r);

    const World& w;
    Grid<int> map;
    std::vector<int> tbl[17];
};

// 阶段分割

class GamePhase
{
   public:
    explicit GamePhase(const World& w) : w(w) {}
    void update();

    int stockMax() const { return maxStock; }
    bool armyAllIn() const { return army; }
    bool priestAllIn() const { return priest; }

   private:
    const World& w;
    int maxStock = tune::STOCK_MAX_BASE;
    bool army = false;
    bool priest = false;
};

// 命令接口封装

class Mgr;

class Orders
{
   public:
    explicit Orders(const World& w) : w(w) {}
    void bind(Mgr* m) { owner = m; }

    void action(int sn, int targetSN);
    void move(int sn, double dr, double ur);
    void moveToCell(int sn, const Pos& p);  // 走到该格中心
    void build(int workerSN, int type, int dr, int ur);
    void buildingAction(int hostSN, int action);

    // 目标没变且不是发呆就不重复下令
    void workerTask(int workerSN, int targetSN);

   private:
    const World& w;
    Mgr* owner = nullptr;
};

// 人力资源

class Dispatch;

class Labor
{
   public:
    explicit Labor(const World& w) : w(w) {}

    void registerSystem(Dispatch* s) { systems.push_back(s); }
    void rebuild();  // 重建空闲池

    const std::vector<int>& idle() const { return pool; }
    int nearestOf(const std::vector<int>& cand, const FloatPos& at) const;

    // 认领离 at 最近的一个村民; Steal::Allow 时可以从在岗的人里抢(农田工除外)
    int claim(const FloatPos& at, Steal steal);
    void release(int sn);  // 交还空闲池(该村民已阵亡则丢弃)
    bool isPinned(int sn) const;  // 有系统声明该村民不可被抢(目前只有农田)

   private:
    const World& w;
    std::vector<Dispatch*> systems;
    std::vector<int> pool;                     // 空闲人口
    std::unordered_set<int> claimedThisFrame;  // 本帧被认领过的 SN, 防止一帧内反复抢同一人
};

struct DataPack  // 所有子系统共享的数据
{
    World& world;
    NavGrid& nav;
    ThreatField& threat;
    GamePhase& phase;
    Labor& labor;
    Orders& orders;
};

class Dispatch
{
   public:
    explicit Dispatch(DataPack& datapack) : c(datapack) {}
    virtual ~Dispatch() = default;

    virtual bool ownsWorker(int sn) const { return false; }  // 重建空闲池时被询问
    virtual void detachWorker(int sn) {}                     // 被 Labor 抢走时解绑
    virtual bool pinsWorker(int sn) const { return false; }  // 返回真表示不可被抢

   protected:
    DataPack& c;
};

// 采集

class GatherSystem : public Dispatch
{
   public:
    explicit GatherSystem(DataPack& datapack) : Dispatch(datapack) {}

    void onFrame();       // 重建仓储点与全部资源池, 清理失效绑定
    void resetDesired();  // 人口分配前清零
    void run();           // 按 desired 调整人口并下令

    int poolRoom(ResKind k) const;
    int spotsWithin(ResKind k, double limit) const;
    void toPool(int& from, int num, ResKind k, double limit = -1);
    const GatherPool& pool(ResKind k) const { return pools[k]; }

    bool ownsWorker(int sn) const override { return spotOfWorker.count(sn) > 0; }
    void detachWorker(int sn) override { dropSpot(sn, false); }

   private:
    void buildDepots();
    bool standCell(const tagResource* r, Pos& out) const;  // 采集占地分配
    double depotCost(const FloatPos& at, const std::vector<FloatPos>& depots) const;
    void dropSpot(int workerSN, bool toFree);  // 解开一条绑定

    GatherPool pools[RK_COUNT];
    std::unordered_map<int, int> spotOfWorker;  // 村民SN -> 资源SN
    std::unordered_map<int, int> workerOfSpot;  // 资源SN -> 村民SN

    Grid<unsigned char> claimed;  // 已被某个资源点占用的落脚格

    std::vector<FloatPos> foodDepots;  // 基地 + 谷仓
    std::vector<FloatPos> resDepots;   // 基地 + 仓库
};

// 打猎

class HuntSystem : public Dispatch
{
   public:
    explicit HuntSystem(DataPack& ctx) : Dispatch(ctx) {}

    void onFrame();  // 聚类重建 + 继承人口 + 清理过期猎物
    void resetDesired();
    void run();

    int siteCount() const { return (int)hunts.size(); }
    void toHunt(int& from, int num, int siteIdx);  // 给第 idx 近的片区派 num 人

    bool ownsWorker(int sn) const override { return staffHunt.count(sn) > 0; }
    void detachWorker(int sn) override;

   private:
    void formHunts(const std::vector<const tagResource*>& sor, int threshold);
    void restoreStaff();  // 继承人口
    void driveHunt(HuntSite& site);
    bool huntable(const tagResource* r) const;
    HuntSite* huntOf(int siteID);         // 按id查本帧片区
    void assign(int x, HuntSite& site);   // 指派x名闲置人员到片区
    void release(int x, HuntSite& site);  // 释放x名人员进闲置队列

    int nextSiteID = 0;
    std::vector<HuntSite> hunts;                  // 按离基地由近及远
    std::unordered_map<int, HuntSite*> huntByID;  // 片区id -> 本帧片区
    std::unordered_map<int, int> huntOfSN;        // 猎物SN -> 片区id
    std::unordered_map<int, int> staffHunt;       // 村民SN -> 片区id
    std::unordered_map<int, int> huntPrey;        // 片区id -> 正在打的猎物SN
};

// 农田

class FarmSystem : public Dispatch
{
   public:
    explicit FarmSystem(DataPack& ctx) : Dispatch(ctx) {}

    void onFrame();  // 刷新已完工农田列表
    void run();      // 维护绑定关系并下耕地令

    int farmCount() const { return (int)farmList.size(); }

    bool ownsWorker(int sn) const override { return workerToFarm.count(sn) > 0; }
    bool pinsWorker(int sn) const override { return workerToFarm.count(sn) > 0; }

   private:
    void unbind(std::unordered_map<int, int>::iterator it);  // 解开 farmToWorker 里的这条绑定

    std::vector<int> farmList;
    std::unordered_map<int, int> farmToWorker;
    std::unordered_map<int, int> workerToFarm;
};

// 建筑

class BuildSystem : public Dispatch
{
   public:
    BuildSystem(DataPack& ctx, const GatherSystem& g) : Dispatch(ctx), gather(g) {}

    void onFrame();  // 清空排队, 按节流重算仓库收益图
    void run();      // 维护工地 / 补人 / 开新工地

    void wantBuilding(int buildingType, int total, int priority);  // 该类总数补到 total
    void wantStock(int priority);                                  // 收益够本才修仓库
    int queuedBuild(int type) const;
    Stock demand() const;  // 排队中的建筑还差多少资源

    bool ownsWorker(int sn) const override;
    void detachWorker(int sn) override;

   private:
    bool buildAvailable(int type) const;
    Pos findSpot(int type);
    void buildPlaceMask(int type, int size);
    void buildSaveMap();

    const GatherSystem& gather;

    std::multiset<std::pair<int, int>> builds;  // 等待建造队列, pair<priority, buildingType>
    std::vector<BuildSite> sites;

    Grid<int> costMap;
    PrefixSum2D sum;
    Grid<unsigned char> placeOk;
    std::unordered_map<int, int> failedSpots;

    Grid<int> saveMap;     // 建立仓库带来的搬运帧节省, 索引是占地左上角
    int bestSave = 0;      // saveMap 里放得下的位置中的最大值
    int savePlanned = -1;  // 上次算 saveMap 的帧号
};

// 生产

class ProductionSystem : public Dispatch
{
   public:
    explicit ProductionSystem(DataPack& ctx) : Dispatch(ctx) {}

    void onFrame();     // 清空本帧队列
    void run();         // 派发生产 / 科技
    void runDestroy();  // 人口超编时拆村民

    void wantUnit(int type, int total, int priority);  // 该类总数补到 total
    void wantTech(int action, int priority);           // 一次性科技
    int queuedProd(int action) const;
    bool hasTech(int action) const { return doneTech.count(action) > 0; }
    Stock demand() const;

   private:
    bool techAvailable(int action) const;
    int idleHost(int buildingType, const std::set<int>& busy) const;

    std::multiset<std::pair<int, int>> prods;  // pair<priority, action>, 从高到低走一遍
    std::unordered_set<int> doneTech;
    int destroyCnt = -1;
};

// 动态经济

struct PopPlan
{
    int food = 0, wood = 0, stone = 0;
};

class EconomySystem : public Dispatch
{
   public:
    explicit EconomySystem(DataPack& ctx) : Dispatch(ctx) {}

    void rebalance(int population, const Stock& need);
    const PopPlan& plan() const { return pop; }

   private:
    void rateOf(double out[2]) const;

    static const int dt = 60 * 25;  // 每隔此帧数快照一次库存
    Stock prevStock;
    int lastSnapFrame = -100;
    bool phaseChanged = false;
    PopPlan pop;
};

// 侦察

class ScoutSystem : public Dispatch
{
   public:
    explicit ScoutSystem(DataPack& ctx) : Dispatch(ctx) {}

    void onFrame();  // 挑一个还活着的侦察单位
    void run();

   private:
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

    int scoutSN = -1;
    Grid<int> scoutDist, scoutPrev;
    std::vector<Pos> route;  // 逐格的路径, 不含起点
    int routeAt = 0;         // 还没走到的第一格
    int routeSent = -1;      // 上次下令去的那格的一维索引, -1 表示尚未发命令
    bool routeFlee = false;  // 当前route是逃跑路线还是探图路线

    // 每轴 MAP_L / SCOUT_VIEW + 1 个点, 下标 idx = i * 每轴点数 + j 对应 Pos(i, j) * SCOUT_VIEW
    std::vector<int> unknownRow;  // 每行未知格的前缀和, 宽 MAP_U + 1
    std::vector<int> wpCooldown;  // 卡住过的路径点的冷却到期帧, 过期自动解除
    std::vector<bool> wpDone;     // 已经站到过的路径点
    int goalWp = -1;              // 目标路径点下标, -1 表示还没选
    Pos goalStand = {-1, -1};     // 探图时是goalWp, 回家时是基地旁
    bool arrived = false;
    Pos anchor;
    Pos home;
    int lastAnchorChanged = -tune::SCOUT_ANCHOR_GAP;

    FloatPos lastPos = {-1, -1};
    int lastRecordFrame = 0;
};

// 防守

class DefenceSystem : public Dispatch
{
   public:
    explicit DefenceSystem(DataPack& ctx) : Dispatch(ctx) {}

    void run();  // 处理来袭波次, 置 inCombat
    bool inCombat() const { return combat; }

    bool ownsWorker(int sn) const override { return fixCrew.count(sn) > 0; }
    void detachWorker(int sn) override { fixCrew.erase(sn); }

   private:
    void fixTower();
    void runTower();
    void runPriest();
    void runArmy();

    bool combat = false;        // 本帧祭司不探图
    std::vector<int> hostiles;  // 本帧要处理的敌人SN, 升序
    std::vector<int> towerAtk;
    int priestTarget = -1;  // 祭司正在转换的敌人SN
    std::set<int> fixCrew;  // 正在修箭塔的人
};

// 进攻

class OffenseSystem : public Dispatch
{
   public:
    explicit OffenseSystem(DataPack& ctx) : Dispatch(ctx) {}

    void run();        // 全军出击, 我咬死你
    void clearRoad();  // 闲兵散到基地外围, 免得堵路

    const Pos& assaultCorner() const { return corner; }

   private:
    Pos corner = {-1, -1};
    Pos watchPoint = {-1, -1};
    bool been = false;
    int siegeSN = -1;
    Grid<unsigned char> scratch;
};

// 雷霆狮子

class LionHuntSystem : public Dispatch
{
   public:
    LionHuntSystem(DataPack& ctx, const OffenseSystem& o) : Dispatch(ctx), offense(o) {}

    void run();

    bool ownsWorker(int sn) const override { return lionCrew.count(sn) > 0; }
    void detachWorker(int sn) override { lionCrew.erase(sn); }

   private:
    const OffenseSystem& offense;
    std::set<int> lionCrew;  // 正在打狮子的人
};

// 策略

struct StrategyOptions
{
    bool useStock = false;        // 是否修建仓库
    bool useLiveHunting = false;  // 是否打猎
    bool usePopModel = false;     // 是否销毁多余村民
};

class StrategySystem : public Dispatch
{
   public:
    StrategySystem(DataPack& ctx, BuildSystem& b, ProductionSystem& p, EconomySystem& e, GatherSystem& g, HuntSystem& h,
                   const FarmSystem& f)
        : Dispatch(ctx), build(b), production(p), economy(e), gather(g), hunt(h), farms(f)
    {
    }

    void run();
    StrategyOptions& options() { return opt; }

   private:
    int farmerTargetByPop() const;  // usePopModel 时的村民上限

    BuildSystem& build;
    ProductionSystem& production;
    EconomySystem& economy;
    GatherSystem& gather;
    HuntSystem& hunt;
    const FarmSystem& farms;
    StrategyOptions opt;
};

// 综合

class Mgr : public UsrAI
{
   public:
    Mgr();
    virtual ~Mgr() = default;

    void update(const tagInfo& info);

    // 转发接口
    void emitAction(int sn, int targetSN) { HumanAction(sn, targetSN); }
    void emitMove(int sn, double dr, double ur) { HumanMove(sn, dr, ur); }
    void emitBuild(int workerSN, int type, int dr, int ur) { HumanBuild(workerSN, type, dr, ur); }
    void emitBuildingAction(int hostSN, int action) { BuildingAction(hostSN, action); }

   private:
    World world;
    NavGrid nav;
    ThreatField threat;
    GamePhase phase;
    Labor labor;
    Orders orders;
    DataPack datapack;

    GatherSystem gather;
    HuntSystem hunt;
    FarmSystem farms;
    BuildSystem build;
    ProductionSystem production;
    EconomySystem economy;
    ScoutSystem scout;
    DefenceSystem defence;
    OffenseSystem offense;
    LionHuntSystem lionHunt;
    StrategySystem strategy;
};

/*##########YOUR CODE ENDS HERE##########*/
#endif  // USRAI_H
