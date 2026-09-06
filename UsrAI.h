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
const int SCOUT_STUCK = 75;                                // 卡住
const int SCOUT_COOLDOWN = 375;                            // 暂时不去了
const int SCOUT_HOME_STAY = 25 * 90;                       // 回避时间
const int SCOUT_WAVE[3] = {25 * 240, 25 * 540, 25 * 840};  // 波次

// 总攻
const int ASSAULT_FRAME = 25 * 60 * 16;    // 复合弓 + 投石车统一出动
const double RETREAT_BOW = 4.0;            // 敌人进到这个格距就后撤, 退完仍在射程内可继续输出
const double RETREAT_STONE = 6.0;          // 投石车体积大且射程 11, 提前退
const int MOVE_STUCK = 12;                 // 有移动目标却这么多帧没靠近目标, 判定卡住
const int MARCH_STEP = 5;                  // 一道推进令跨这么多格, 一格一令会走一格停一格
const int RETREAT_STEP = 3;                // 后撤令跨这么多格; 比推进短, 退完要仍在射程内
const int SLOT_BLACK = 50;                 // 卡住过的子位拉黑这么多帧
const double MOVE_DONE = 0.2;              // 距目标子位小于这个格距即视为到位
const double MOVE_GAIN = 0.05;             // 一帧至少靠近这么多格才算有进展
const int HOME_KEEP = 10;                  // 大部队出动前, 基地附近至少留这么多复合弓守家
const int HOME_RANGE = 40;                 // 算作"基地附近"的格距
const int BELONG_CORNER = 50;              // 分隔攻守判据
const int LION_KEEP = 4;                   // 狮子视野3
const int ENEMY_KEEP = 10;                 // 敌方单位的警戒圈
const int DEF_ALERT = 45;                  // 进到这个距离才算来袭波次
const int TOWER_ALERT = 55;                // 提前点名范围
const int FIX_TOWER_UNTIL = 25 * 60 * 15;  // 这之后不再修塔
const int WAIT_BAND_IN = 22;               // 待命部队散开到离基地这一圈, 免得堵住基地门口
const int WAIT_BAND_OUT = 26;
const int PRIEST_STAY = 30;               // 祭司跟大部队时到攻城厂保持的格距
const int PRIEST_STAY_BLIND = 45;         // 攻城厂尚未定位, 按对角估算, 退得更远
const int PRIEST_STAY_BAND = 5;           // 落在 [STAY, STAY+BAND] 里就不再挪动
const int LION_NEAR = 40;                 // 离基地这么多格内的活狮子随时清理
const int LION_HUNT_FROM = 25 * 60 * 15;  // 15 分钟起派一个人清场

// 经济参数
const int CARRY_LIMIT = 10;               // 村民荷载
const double BASE_RATE_BUSH = 0.5;        // 浆果, 个/秒
const double BASE_RATE_CORPSE = 1.0;      // 猎物尸体, 个/秒
const double BASE_RATE_FARM = 0.5;        // 农田, 个/秒
const double BASE_RATE_GOLD = 1.0;        // 金矿, 个/秒
const double BASE_RATE_WOOD = 1.0;        // 木材, 个/秒
const double HUNT_DPS = 5.0;              // 村民对活猎物的每秒伤害
const int CORPSE_GROUP_GAP = 6;           // 排除零星猎物
const int FARM_PRIORITY = 96;             // 农田在建造队列里的优先级
const int BUILD_WAIT = 25;                // 下了建造令之后, 等地基出现的宽限帧数
const int FARM_MAX = 6;                   // 农田数量硬上限
const int POP_CAP = 50;                   // 人口上限
const int FARMER_MIN = 10;                // 村民数下限
const int FARMER_MAX = 20;                // 村民数上限
const int FOOD_RANGE = 60;                // 食物点离基地超过这个格距就不派人, 赶路不值

// 各战略阶段的固定人员比例, 顺序同 EconRes: 木 食 金
const int ECON_WEIGHT[3][3] = {{4, 6, 0}, {5, 4, 2}, {1, 4, 4}};

// 基础数据结构
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

// 直接采集的资源种类, 石矿本局不采, 按纯障碍物处理
enum ResKind
{
    RK_WOOD,
    RK_GOLD,
    RK_BUSH,
    RK_CORPSE,
    RK_COUNT
};

// 人口分配的战略维度, 食物由 RK_BUSH + RK_CORPSE + 农田共同承担
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

// BFS 距离场的通行判据
enum FieldMode
{
    FIELD_WALK,
    FIELD_CIVIL,
    FIELD_ATTACK,
    FIELD_SAFE
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

struct MoveOrder  // 一条正在执行的移动命令
{
    int slot = -1;
    double gap = 0;     // 上次记录的到目标格距
    int idle = 0;       // 连续没有进展的帧数
    bool back = false;  // 后撤令必须走完; 推进令可被敌人或新目标打断
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

inline double transportRate(double baseRate, double dropDis)
{
    if (baseRate <= EPS) return 0.0;
    const double gatherSec = CARRY_LIMIT / baseRate;
    const double walkSec = 2.0 * dropDis / (HUMAN_SPEED * 25.0);
    return CARRY_LIMIT / (gatherSec + walkSec);
}

inline double gatherRate(ResKind k, double dropDis)
{
    static const double rate[RK_COUNT] = {BASE_RATE_WOOD, BASE_RATE_GOLD, BASE_RATE_BUSH, BASE_RATE_CORPSE};
    return k < RK_COUNT ? transportRate(rate[k], dropDis) : 0.0;
}

int buildingSize(int type);
int resourceSize(int type);
int buildWoodCost(int type);
ResKind kindOf(int resourceType);
Stock actionCost(int action);
int actionHost(int action);
int typeToAction(int type);

// 建筑锚点在左下角, 运输距离要按几何中心算
inline FloatPos centerOf(const Pos& p, int buildingType)
{
    const double half = buildingSize(buildingType) * 0.5;
    return FloatPos((p.dr + half) * BLOCKSIDELENGTH, (p.ur + half) * BLOCKSIDELENGTH);
}

// 2x2 资源只给中心坐标, 反推左下角格; 1x1 的 Block 字段本身就是左下角
inline Pos resourceCell(const tagResource* r)
{
    if (resourceSize(r->Type) == 1) return Pos(r->BlockDR, r->BlockUR);
    return Pos((int)(r->DR / BLOCKSIDELENGTH + 0.5) - 1, (int)(r->UR / BLOCKSIDELENGTH + 0.5) - 1);
}

class Mgr : public UsrAI
{
   public:
    virtual ~Mgr() = default;

    void update(const tagInfo& info);  // 每帧主循环

   private:
    // 每帧信息
    void makeFrame(const tagInfo& info);
    void mark(const tagBuilding& b);

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
    bool valid(int dr, int ur) const;                 // 地形是否允许建造
    bool walkable(int dr, int ur) const;              // 是否可以行走
    bool canPlace(int dr, int ur, int size) const;    // size*size 的地基是否放得下
    bool nearLion(int dr, int ur, int radius) const;  // 有狮子
    bool enemyCorner(int dr, int ur) const;           // 与基地对角的那一象限
    int lockOf(int enemySN) const;                    // 该敌人锁着的我方SN, 没锁到我方返回 -1, 锁到祭司返回 -1

    // 库存
    Stock available() const { return res - held; }
    bool afford(const Stock& c) const { return available().covers(c); }

    // 距离场与代价图
    void fieldBuild(std::vector<int>& out, const Pos& src, int size, FieldMode mode, std::vector<int>* prev = nullptr);
    void civilDangerBuild();  // 只维护驻守敌人记忆
    bool civilDangerAt(int dr, int ur) const;
    bool civilSafeSite(const Pos& p, int size) const;
    void ringAdd(std::vector<int>& g, const Pos& around, int size, int cost, int inner, int outer, int delta = 0);
    int threatAt(int dr, int ur) const;  // 当前敌人/狮子即时计算

    // 下令
    void moveToCell(int sn, const Pos& p)  // 走到该格中心
    { HumanMove(sn, (0.5 + p.dr) * BLOCKSIDELENGTH, (0.5 + p.ur) * BLOCKSIDELENGTH); }
    void sendAction(int workerSN, int targetSN);  // 智能命令

    // 村民调度: 空闲池与岗位登记
    void laborBuild();  // 重建空闲池
    void laborRelease();
    int takeNearest(const FloatPos& at, bool steal = false);  // steal 时可从在岗的人里抢
    void freeWorker(int sn);                                  // 交还空闲池(该村民已阵亡则丢弃)

    static int targetOf(const std::unordered_map<int, int>& jobs, int workerSN);  // target -> worker 的反查
    bool workerBusy(int sn) const;                                                // 已被某个岗位登记
    bool workerReserved(int sn) const;   // 在专职岗位上(农田/工地/修塔/打狮子), 不许被抢
    bool civilWorkerSafe(int sn) const;  // 当前人在驻守禁区的基地安全侧
    void workerDrop(int sn);             // 从所有岗位解绑

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

    std::vector<int> nav;  // 基地距离, -1 表示不可达

    std::unordered_map<int, Pos> guardEnemies;
    std::unordered_set<int> mobileEnemies;
    std::vector<int> civilNav;
    std::vector<int> laborPool;  // 空闲人口

    // 采集
    void arrangeGather();                                  // 重建仓储点与全部资源池, 清理失效绑定
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
    FoodPlan planFood();          // 按产出排序食物岗位, 同时重排采集池与农田表
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
    void releaseBuilders(BuildSite& s, bool stop);                 // 释放工地人员；stop 时先取消旧施工令
    void wantBuilding(int buildingType, int total, int priority);  // 该类总数补到 total
    void wantDepot(int depotType, int priority);                   // 有远端需求时补一座(首座谷仓无条件)

    void depotWant(ResKind k, std::vector<Pos>& out) const;  // 远端有人采集的点触发需求, 取最远的当锚点
    bool depotCovered(int depotType, const Pos& c) const;    // 已完成/在建仓储是否已覆盖该服务区
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

    // 侦察
    void runScout();
    void floodThreat(const Pos& from, bool avoidThreat);     // 逐格bfs; avoidThreat 表示不许穿威胁格
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
    int defenceSelector(const tagArmy& u) const;
    void runDefenders();

    bool combat = false;        // 本帧祭司不探图
    std::vector<int> hostiles;  // 本帧要处理的敌人SN
    std::set<int> fixCrew;      // 正在修箭塔的人

    // 进攻
    void offense();                    // 进攻总调度: 定位对角与攻城厂, 建 atkField, 派兵
    void offenseUpdate();              // 清理死人留下的移动令, 更新 tars
    int siegeDis(const Pos& p) const;  // 到攻城厂的格距, 未定位返回角落运算

    int attackSelector(const tagArmy& u) const;
    void vanguardPick();  // 大部队出动前维持一支提前批次
    bool inVanguard(int sn) const { return vanguard.count(sn) > 0; }
    void runAssault();
    void runAtkPriest();  // 祭司
    void clearRoad();     // 借过一下

    bool marchable(int dr, int ur) const;  // 真实可走或尚未探明

    // 每格 5 个子位: 0..3 是 0.25/0.75 的四个角(给复合弓), 4 是格心(给投石车)
    static int slotIdx(int dr, int ur, int k) { return cellIdx(dr, ur) * 5 + k; }
    static FloatPos slotAt(int slot);
    int slotOf(const tagArmy& u) const;               // 单位当前实际站的子位
    void slotClaim(const tagArmy& u, int slot);       // 占位; 大体积单位连带封周围一圈
    bool slotFree(int slot, const tagArmy& u) const;  // 该单位放得下、
    int slotStep(const tagArmy& u, const Pos& from, const FloatPos& ref, bool retreat) const;  // 下坡一格
    int pickSlot(const tagArmy& u, bool retreat);        // 连走 RETREAT_STEP / MARCH_STEP 格
    double enemyGap(const FloatPos& at) const;           // 到最近敌军的格距, 没有敌军返回很大值
    void sendTo(const tagArmy& u, int slot, bool back);  // 占位 + 登记 + 下移动令
    bool keepMove(const tagArmy& u, bool interrupt);     // 维护在途命令；仍应继续走返回 true

    Pos corner = {-1, -1};  // 与基地对角的地图角
    int siegeSN = -1;
    Pos siegePos = {-1, -1};

    bool assaultOn = false;
    std::unordered_set<int> vanguard;  // 提前出动的复合弓; assaultOn 之后清空并入大部队

    std::vector<int> tars;
    std::vector<int> atkField;
    std::vector<int> slotOwner;  // 子位 -> 占用者 SN, -1 为空
    std::vector<int> slotBlack;  // 子位拉黑到期帧

    std::unordered_map<int, MoveOrder> moveGoal;

    // 雷霆狮子
    void killLions();
    int lionWorker = -1;  // 固定一名清狮村民
    int lionTarget = -1;  // 当前逐个击杀的狮子

    // 探图
    std::vector<int> scoutDist, scoutPrev;
    std::vector<Pos> route;  // 逐格的路径, 不含起点
    int routeAt = 0;         // 还没走到的第一格
    int routeSent = -1;      // 上次下令去的那格的一维索引, -1 表示尚未发命令
    bool routeFlee = false;  // 当前route是逃跑路线还是探图路线

    // 每轴 MAP_L / SCOUT_VIEW + 1 个点, 下标 idx = i * 每轴点数 + j 对应 Pos(i, j) * SCOUT_VIEW
    std::vector<int> wpCooldown;        // 卡住过的路径点的冷却到期帧, 过期自动解除
    std::vector<unsigned char> wpDone;  // 已经站到过的路径点
    int goalWp = -1;                    // 目标路径点下标, -1 表示还没选
    Pos goalStand = {-1, -1};           // 探图时是goalWp, 回家时是基地旁
    bool arrived = false;
    Pos home;
    FloatPos lastPos = {-1, -1};
    int lastRecordFrame = 0;

    // 策略
    void strategy();
    int farmerTarget() const;  // 本帧村民目标数, 生产和自毁共用同一个口径
};

/*##########YOUR CODE ENDS HERE##########*/
#endif  // USRAI_H
