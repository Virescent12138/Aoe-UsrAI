# README

## 项目说明

该项目是njust的c++课程作业，项目内容是帝国时代（AoE）的简化版本，开局时处在工具时代，最多可以升级到铜器时代。玩家在100x100地图上发展，抵御定时的三波进攻和摧毁地图上一处敌方建筑群，敌人不会在游戏进行时建造建筑，采集资源，补充兵力或探索地图。胜利条件为使用祭司转换敌人攻城武器厂，一旦祭司死亡直接失败。

本人只书写了UsrAI相关代码，其余代码作者详见软件内部说明以及LISENCE。因考虑到使用便捷性，仓库包括了编译软件所需的最少代码而不包括原始说明文档。

## 项目构架

该项目是使用qt5, cpp14标准，cmake的windows桌面应用程序

## 设计思路

### 游戏信息源

函数`UsrAI::tagInfo getInfo()`提供游戏当前帧的数据，`processData()`的第一句话获得当前帧的游戏状态，进而使用自定的`Mgr`类进行处理，分析，记录等工作，最后使用预设的函数发送指令等待下一帧去执行，保证下一次的info是执行完后返回的。

```
struct tagInfo
{
    vector<tagBuilding> buildings; // 我方建筑列表
    vector<tagFarmer> farmers; // 我方农民列表，包括运输船和渔船
    vector<tagArmy> armies; // 我方军队列表，包括战船
    vector<tagBuilding> enemy_buildings; // 敌方建筑列表
    vector<tagFarmer> enemy_farmers; // 敌方农民列表
    vector<tagArmy> enemy_armies; // 敌方军队列表
    vector<tagResource> resources; // 资源列表
    map<int, int> ins_ret; // 指令返回值，map<id, ret>
            TerrainData* theMap; 
// 地形高度和类型信息，访问方式为(*theMap)[BlockDR][BlockUR]
    int GameFrame; // 当前帧数
    int civilizationStage; // 文明阶段
    int Wood; // 木材数量
    int Meat; // 肉类数量
    int Stone; // 石头数量
    int Gold; // 黄金数量
    int Human_MaxNum; // 最大人口数量
};

struct tagTerrain {
    int height; // 如果type是Ocean，height是-1，如果是Land，则为0，1，2，3或-1（表示斜坡）
    int type; // 枚举类型：Unknown,Ocean,Land
};

struct tagBuilding : tagObj
{
   int Type; // 建筑类型
            int Blood; // 当前血量
            int MaxBlood; // 最大血量
            int Percent; // 完成百分比
            int Project; // 当前项目；箭塔中可表示当前攻击目标SN
            int ProjectPercent; // 项目完成百分比
            int Cnt; // 剩余资源量，仅农田等资源建筑有意义
};

struct tagResource
{
    double DR,UR; //细节坐标
    int BlockDR,BlockUR; //区块坐标
    int Type; // 资源类型
    int SN; // 序列号
    int ProductSort; // 产品种类
    int Cnt; // 剩余资源数量
    int Blood; // 当前血量
};

struct tagHuman
{
    double DR,UR; //细节坐标
    int BlockDR,BlockUR; //区块坐标
    double DR0,UR0; // 目的地坐标
    int NowState; // 当前状态
    int WorkObjectSN; // 工作对象序列号
    int Blood; // 当前血量
    int SN; // 序列号
    int attack; // 攻击力
    int rangedDefense; // 远程防御
    int meleeDefense; // 近战防御
};
struct tagFarmer: public tagHuman
{
    int ResourceSort; // 手持资源种类
    int Resource; // 手持资源数量；如果是运输船，则代表单位数量
    int FarmerSort; // 村民的类型： 0表示陆地生产单位，1表示渔船，2表示运输船
};
struct tagArmy:public tagHuman
{
int Sort;   //军队种类
            int ConvertCooldown; //转换冷却时间（仅限巫师）
};

```

关于可见性需要特别注意：enemy_armies和enemy_farmers主要记录当前视野内可见的敌方单位；敌军离开视野后通常不会继续出现在列表中。enemy_buildings和resources按已探索信息保留得更多一些，已探索过的持续记录。
列表做内部打乱，因此代码不要假设列表中的第n个对象每帧都是同一个对象。

游戏操作接口：

 `HumanBuild(SN, BUILDING_TYPE, x, y);`指定SN前去在(x, y)建造BUILDING_TYPE

 `HumanMove(SN, x * BLOCKSIDELENGTH, y * BLOCKSIDELENGTH);`移动使用的是浮点坐标

`HumanAction(SN, enemy_SN);`进攻

`BuildingAction(building.SN, BUILDING_CENTER_CREATEFARMER);`制造村民等行为

### `Mgr`类设计

主要任务包括进行详细的资源选点，任务实施和路径规划，有如下系统。主要流程是在每帧获得信息构建哈希表支持查找，然后清理之前留下的死亡单位的残留数据，然后对于地图进行更新，包括更新每个资源点的位置和索引。随后根据决策层安排的人口分布进行工作站点间调整调度，并且执行每个命令。其中建筑和耕田独立于采集系统，有自己的人口占据和分配规则。侦察系统安排大祭司寻路，探索地图。

### `frame`

每一帧构建所有SN和具体对象的哈希表，结合info信息实现高效的正反查找，创建一个空闲人口的队列用来支持人员调度。所有指针只有当前帧的生命周期。

##### 资源

对于非活动物，每个作为资源点贪心的采集，有筛选机制。对于动物，使用聚类后击杀，然后当作一般资源处理。

##### `scout`

在非敌人波次（第4，9，14分钟）时间段，派遣我方唯一的大祭司探索地图，使用流场躲避威胁，`bfs`确定可行可以到达的安全目标，`route`实现精确的走位指导。

##### `farm`

相对独立的子系统，固定每块田由它的建造者任一来耕作，死亡后由闲置人口补充，不抢人。

##### `findSpot`

使用评分机制，区分建筑生成分数表。

## 常量表

特别的，认为村民的type编码是-1，使用枚举`AT_FARMER`，方便和其他人员并列存储

```
1、建筑类型BuildingNum常量表：
BUILDING_HOME = 0 //房屋
BUILDING_GRANARY = 1 //谷仓
BUILDING_CENTER = 2 //市镇中心
BUILDING_STOCK = 3 //仓库
BUILDING_FARM = 4 //农田
BUILDING_MARKET = 5 //市场
BUILDING_ARROWTOWER = 6 //箭塔
BUILDING_ARMYCAMP = 7 //兵营
BUILDING_STABLE = 8 //马厩
BUILDING_RANGE = 9 //靶场
BUILDING_DOCK = 10 //船坞
BUILDING_SIEGE = 11 //攻城武器厂
BUILDING_COLLAGE = 12 //学院
2、HumanAction函数的Action常量：
BUILDING_CENTER_CREATEFARMER = 1 //市镇中心生产村民
BUILDING_CENTER_UPGRADE = 2 //市镇中心升级时代
BUILDING_GRANARY_ARROWTOWER = 3 //谷仓研发建造箭塔
BUILDING_GRANARY_WALL = 4 //谷仓研发建造城墙，当前主要为枚举保留
BUILDING_GRANARY_ARROWTOWE_UPGRADE = 5 //谷仓升级箭塔
BUILDING_MARKET_WOOD_UPGRADE = 6 //市场研发木材加工
BUILDING_MARKET_STONE_UPGRADE = 7 //市场研发石矿开采
BUILDING_MARKET_FARM_UPGRADE = 8 //市场研发驯养动物
BUILDING_MARKET_GOLD_UPGRADE = 9 //市场研发金矿开采
BUILDING_MARKET_WHEEL_UPGRADE = 10 //市场研发车轮
BUILDING_MARKET_CRAFT_UPGRADE = 11 //市场研发工艺
BUILDING_MARKET_PLOW_UPGRADE = 12 //市场研发犁
BUILDING_STOCK_UPGRADE_USETOOL = 13 //仓库研发工具使用；二级为金属加工
BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY = 14 //仓库研发步兵护甲；二级为步兵鳞甲
BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER = 15 //仓库研发弓兵护甲；二级为弓兵鳞甲
BUILDING_STOCK_UPGRADE_DEFENSE_RIDER = 16 //仓库研发骑兵护甲；二级为骑兵鳞甲
BUILDING_STOCK_UPGRADE_MISSILE_DEFENSE_INFANTRY = 17 //仓库研发青铜盾
BUILDING_ARMYCAMP_CREATE_CLUBMAN = 18 //兵营训练棍棒兵
BUILDING_ARMYCAMP_CREATE_SLINGER = 19 //兵营训练投石兵
BUILDING_ARMYCAMP_UPGRADE_CLUBMAN = 20 //兵营升级棍棒兵为战斧兵
BUILDING_ARMYCAMP_CREATE_BROADSWORD = 21 //兵营训练阔剑兵
BUILDING_ARMYCAMP_UPGRADE_BROADSWORD = 22 //兵营研发阔剑科技
BUILDING_RANGE_CREATE_BOWMAN = 23 //靶场训练弓箭手
BUILDING_RANGE_CREATE_CHARIOT_ARCHER = 24 //靶场训练战车弓兵
BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN = 25 //靶场训练复合弓兵
BUILDING_RANGE_UPGRADE_COMPOSITE_BOW = 26 //靶场研发复合弓科技
BUILDING_STABLE_CREATE_SCOUT = 27 //马厩训练侦察骑兵
BUILDING_STABLE_CREATE_CHARIOT = 28 //马厩训练四马战车
BUILDING_STABLE_CREATE_CAVALRY = 29 //马厩训练骑兵
BUILDING_DOCK_CREATE_SAILING = 30 //船坞建造渔船
BUILDING_DOCK_CREATE_WOOD_BOAT = 31 //船坞建造运输船
BUILDING_DOCK_CREATE_SHIP = 32 //船坞建造战船
BUILDING_SIEGE_CREATE_STONE_THROWER = 33 //攻城武器厂制造投石车
BUILDING_COLLAGE_CREATE_HOPLITE = 34 //学院训练方阵兵
3、建筑信息及建筑行动所需资源，可以在调用行动命令之前检查资源是否足够
/**市镇中心**/
#define BLOOD_BUILD_CENTER 600 //血量
#define VISION_CENTER 4 //视野
#define BUILD_CENTER_WOOD 200 //建造所需木头
#define TIME_BUILD_CENTER 60 //建造时间（秒）
#define BUILDING_CENTER_CREATEFARMER_FOOD 50 //生产村民所需食物
#define TIME_BUILDING_CENTER_CREATEFARMER 20 //生产村民所需时间
#define BUILDING_CENTER_UPGRADE_TOOLAGE_FOOD 500 //升级工具时代所需食物
#define BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD 800 //升级铜器时代所需食物
#define BUILDING_CENTER_UPGRADE_BRONZEAGE_GOLD 0 //升级铜器时代所需黄金
#define TIME_BUILDING_CENTER_UPGRADE 60 //升级时代所需时间
/**房屋**/
#define BLOOD_BUILD_HOUSE 75 //血量
#define VISION_HOME 4 //视野
#define BUILD_HOUSE_WOOD 30 //建造所需木头
#define TIME_BUILD_HOME 20 //建造时间（秒）
#define HOUSE_HUMAN_NUM 4 //每个房屋提供人口上限
/**仓库**/
#define BLOOD_BUILD_STOCK 350 //血量
#define VISION_STOCK 4 //视野
#define BUILD_STOCK_WOOD 120 //建造所需木头
#define TIME_BUILD_STOCK 30 //建造时间（秒）
//升级工具使用（1级）
#define BUILDING_STOCK_UPGRADE_CLOSER_ATTACK_FOOD 100 //工具使用所需食物
#define TIME_BUILDING_STOCK_UPGRADE_CLOSER_ATTACK 40 //工具使用所需时间
#define BUILDING_STOCK_UPGRADE_CLOSER_ATTACK_ADDITION_ATTACK 2 //工具使用：近战攻击 +2
//升级金属加工（2级）
#define BUILDING_STOCK_UPGRADE_CLOSER_ATTACK_2_FOOD 200 //金属加工所需食物
#define BUILDING_STOCK_UPGRADE_CLOSER_ATTACK_2_GOLD 120 //金属加工所需黄金
#define TIME_BUILDING_STOCK_UPGRADE_CLOSER_ATTACK_2 40 //金属加工所需时间
#define BUILDING_STOCK_UPGRADE_CLOSER_ATTACK_2_ADDITION_ATTACK 2 //金属加工：近战攻击再 +2
//升级步兵护甲（1级）
#define BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY_FOOD 75 //步兵护甲所需食物
#define TIME_BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY 40 //步兵护甲所需时间
#define BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY_ADDITION_DEFENSE_INFANTRY 2 //步兵近战防御 +2
//升级步兵鳞甲（2级）
#define BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY_2_FOOD 100 //步兵鳞甲所需食物
#define BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY_2_GOLD 50 //步兵鳞甲所需黄金
#define TIME_BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY_2 40 //步兵鳞甲所需时间
#define BUILDING_STOCK_UPGRADE_DEFENSE_INFANTRY_2_ADDITION_DEFENSE_INFANTRY 2 //步兵近战防御再 +2
//升级弓兵护甲（1级）
#define BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER_FOOD 100 //弓兵护甲所需食物
#define TIME_BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER 40 //弓兵护甲所需时间
#define BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER_ADDITION_DEFENSE_ARCHER 2 //弓兵近战防御 +2
//升级弓兵鳞甲（2级）
#define BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER_2_FOOD 125 //弓兵鳞甲所需食物
#define BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER_2_GOLD 50 //弓兵鳞甲所需黄金
#define TIME_BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER_2 40 //弓兵鳞甲所需时间
#define BUILDING_STOCK_UPGRADE_DEFENSE_ARCHER_2_ADDITION_DEFENSE_ARCHER 2 //弓兵近战防御再 +2
//升级骑兵护甲（1级）
#define BUILDING_STOCK_UPGRADE_DEFENSE_RIDER_FOOD 125 //骑兵护甲所需食物
#define TIME_BUILDING_STOCK_UPGRADE_DEFENSE_RIDER 30 //骑兵护甲所需时间
#define BUILDING_STOCK_UPGRADE_DEFENSE_RIDER_ADDITION_DEFENSE_RIDER 2 //骑兵近战防御 +2
//升级骑兵鳞甲（2级）
#define BUILDING_STOCK_UPGRADE_DEFENSE_RIDER_2_FOOD 150 //骑兵鳞甲所需食物
#define BUILDING_STOCK_UPGRADE_DEFENSE_RIDER_2_GOLD 50 //骑兵鳞甲所需黄金
#define TIME_BUILDING_STOCK_UPGRADE_DEFENSE_RIDER_2 30 //骑兵鳞甲所需时间
#define BUILDING_STOCK_UPGRADE_DEFENSE_RIDER_2_ADDITION_DEFENSE_RIDER 2 //骑兵近战防御再 +2
//升级青铜盾
#define BUILDING_STOCK_UPGRADE_MISSILE_DEFENSE_INFANTRY_FOOD 150 //青铜盾所需食物
#define BUILDING_STOCK_UPGRADE_MISSILE_DEFENSE_INFANTRY_GOLD 80 //青铜盾所需黄金
#define TIME_BUILDING_STOCK_UPGRADE_MISSILE_DEFENSE_INFANTRY 40 //青铜盾所需时间
#define BUILDING_STOCK_UPGRADE_MISSILE_DEFENSE_INFANTRY_ADDITION_DEFENSE_INFANTRY 1 //青铜盾：步兵远程防御 +1
/**谷仓**/
#define BLOOD_BUILD_GRANARY 350 //血量
#define VISION_GRANARY 4 //视野
#define BUILD_GRANARY_WOOD 120 //建造所需木头
#define TIME_BUILD_GRANARY 30 //建造时间（秒）
//升级箭塔研发（1级）
#define BUILDING_GRANARY_ARROWTOWER_FOOD 50 //研发建造箭塔所需食物
#define TIME_BUILDING_GRANARY_RESEARCH_ARROWTOWER 10 //研发建造箭塔所需时间
//升级二级箭塔（2级）
#define BUILDING_GRANARY_UPGRADE_ARROWTOWER_FOOD 120 //升级箭塔所需食物
#define BUILDING_GRANARY_UPGRADE_ARROWTOWER_STONE 50 //升级箭塔所需石头
#define TIME_BUILDING_GRANARY_UPGRADE_ARROWTOWER 40 //升级箭塔所需时间
#define BUILDING_GRANARY_UPGRADE_ARROWTOWER_ADDITION_ATK 1 //升级箭塔：箭塔攻击 +1
#define BUILDING_GRANARY_UPGRADE_ARROWTOWER_ADDITION_DIS 1 //升级箭塔：箭塔射程 +1
#define THROWMISSION_ARROWTOWER_UPGRADED 17 //升级后箭塔投射帧配置
/**兵营**/
#define BLOOD_BUILD_ARMYCAMP 350 //血量
#define VISION_ARMYCAMP 4 //视野
#define BUILD_ARMYCAMP_WOOD 125 //建造所需木头
#define TIME_BUILD_ARMYCAMP 30 //建造时间（秒）
//训练棍棒兵
#define BUILDING_ARMYCAMP_CREATE_CLUBMAN_FOOD 50 //训练棍棒兵所需食物
#define TIME_BUILDING_ARMYCAMP_CREATE_CLUBMAN 26 //训练棍棒兵所需时间
//升级战斧兵
#define BUILDING_ARMYCAMP_UPGRADE_CLUBMAN_FOOD 100 //升级战斧所需食物
#define TIME_BUILDING_ARMYCAMP_UPGRADE_CLUBMAN 40 //升级战斧所需时间
//训练投石兵
#define BUILDING_ARMYCAMP_CREATE_SLINGER_FOOD 40 //训练投石兵所需食物
#define BUILDING_ARMYCAMP_CREATE_SLINGER_STONE 10 //训练投石兵所需石头
#define TIME_BUILDING_ARMYCAMP_CREATE_SLINGER 24 //训练投石兵所需时间
//升级阔剑科技
#define BUILDING_ARMYCAMP_UPGRADE_BROADSWORD_FOOD 140 //研发阔剑科技所需食物
#define BUILDING_ARMYCAMP_UPGRADE_BROADSWORD_GOLD 50 //研发阔剑科技所需黄金
#define TIME_BUILDING_ARMYCAMP_UPGRADE_BROADSWORD 40 //研发阔剑科技所需时间
//训练阔剑兵（需先升级阔剑科技）
#define BUILDING_ARMYCAMP_CREATE_BROADSWORD_FOOD 35 //训练阔剑兵所需食物，前置：阔剑科技
#define BUILDING_ARMYCAMP_CREATE_BROADSWORD_GOLD 15 //训练阔剑兵所需黄金
#define TIME_BUILDING_ARMYCAMP_CREATE_BROADSWORD 30 //训练阔剑兵所需时间
/**靶场**/
#define BLOOD_BUILD_RANGE 350 //血量
#define VISION_RANGE 4 //视野
#define BUILD_RANGE_WOOD 150 //建造所需木头
#define TIME_BUILD_RANGE 40 //建造时间（秒）
//训练弓箭手
#define BUILDING_RANGE_CREATE_BOWMAN_FOOD 40 //训练弓箭手所需食物
#define BUILDING_RANGE_CREATE_BOWMAN_WOOD 20 //训练弓箭手所需木头
#define TIME_BUILDING_RANGE_CREATE_BOWMAN 30 //训练弓箭手所需时间
//训练战车弓兵（需先升级车轮）
#define BUILDING_RANGE_CREATE_CHARIOT_ARCHER_FOOD 40 //训练战车弓兵所需食物，前置：车轮
#define BUILDING_RANGE_CREATE_CHARIOT_ARCHER_WOOD 70 //训练战车弓兵所需木头
#define TIME_BUILDING_RANGE_CREATE_CHARIOT_ARCHER 30 //训练战车弓兵所需时间
//升级复合弓科技
#define BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_FOOD 180 //研发复合弓科技所需食物
#define BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD 100 //研发复合弓科技所需木头
#define TIME_BUILDING_RANGE_UPGRADE_COMPOSITE_BOW 40 //研发复合弓科技所需时间
//训练复合弓兵（需先升级复合弓科技）
#define BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_FOOD 40 //训练复合弓兵所需食物，前置：复合弓科技
#define BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_GOLD 20 //训练复合弓兵所需黄金
#define TIME_BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN 30 //训练复合弓兵所需时间
/**马厩**/
#define BLOOD_BUILD_STABLE 350 //血量
#define VISION_STABLE 4 //视野
#define BUILD_STABLE_WOOD 150 //建造所需木头
#define TIME_BUILD_STABLE 40 //建造时间（秒）
//训练侦察骑兵
#define BUILDING_STABLE_CREATE_SCOUT_FOOD 60 //训练侦察骑兵所需食物
#define TIME_BUILDING_STABLE_CREATE_SCOUT 30 //训练侦察骑兵所需时间
//训练驷马战车（需先升级轮子科技）
#define BUILDING_STABLE_CREATE_CHARIOT_FOOD 40 //训练四马战车所需食物，前置：车轮
#define BUILDING_STABLE_CREATE_CHARIOT_WOOD 60 //训练四马战车所需木头
#define TIME_BUILDING_STABLE_CREATE_CHARIOT 30 //训练四马战车所需时间
//训练骑兵
#define BUILDING_STABLE_CREATE_CAVALRY_FOOD 70 //训练骑兵所需食物
#define BUILDING_STABLE_CREATE_CAVALRY_GOLD 80 //训练骑兵所需黄金
#define TIME_BUILDING_STABLE_CREATE_CAVALRY 30 //训练骑兵所需时间
/**市场**/
#define BLOOD_BUILD_MARKET 350 //血量
#define VISION_MARKET 4 //视野
#define BUILD_MARKET_WOOD 150 //建造所需木头
#define TIME_BUILD_MARKET 40 //建造时间（秒）
//升级伐木（1级）
#define BUILDING_MARKET_WOOD_UPGRADE_FOOD 120 //木材加工所需食物
#define BUILDING_MARKET_WOOD_UPGRADE_WOOD 75 //木材加工所需木头
#define TIME_BUILDING_MARKET_UPGRADE_CUTTING 40 //木材加工所需时间
#define BUILDING_MARKET_WOOD_UPGRADE_ADDITION_CARRY 2 //木材加工：木材携带 +2
#define BUILDING_MARKET_WOOD_UPGRADE_ADDITION_GATHERRATE 0.2 //木材加工：伐木采集速率配置值 +0.2
#define BUILDING_MARKET_WOOD_UPGRADE_ADDITION_DISSHOOT 1 //木材加工：远程射程 +1
//升级伐木（2级）
#define BUILDING_MARKET_CRAFT_UPGRADE_FOOD 170 //工艺所需食物，前置：木材加工
#define BUILDING_MARKET_CRAFT_UPGRADE_WOOD 150 //工艺所需木头
#define TIME_BUILDING_MARKET_CRAFT_UPGRADE 40 //工艺所需时间
#define BUILDING_MARKET_CRAFT_UPGRADE_ADDITION_DISSHOOT 1 //工艺：远程射程再 +1
#define BUILDING_MARKET_CRAFT_UPGRADE_ADDITION_GATHERRATE 2 //工艺：伐木加成
//升级采石（1级）
#define BUILDING_MARKET_STONE_UPGRADE_FOOD 100 //石矿开采所需食物
#define BUILDING_MARKET_STONE_UPGRADE_STONE 50 //石矿开采所需石头
#define TIME_BUILDING_MARKET_UPGRADE_DIGGINGSOTNE 60 //石矿开采所需时间
#define BUILDING_MARKET_STONE_UPGRADE_ADDITION_CARRY 3 //石矿开采：采石携带 +3
#define BUILDING_MARKET_STONE_UPGRADE_ADDITION_GATHERRATE 0.2 //石矿开采：采石速率配置值 +0.2
#define BUILDING_MARKET_STONE_UPGRADE_ADDITION_SILNGERATK 1 //石矿开采：投石兵攻击 +1
#define BUILDING_MARKET_STONE_UPGRADE_ADDITION_SILNGERDIS 1 //石矿开采：投石兵射程 +1
//升级金矿开采
#define BUILDING_MARKET_GOLD_UPGRADE_FOOD 120 //金矿开采所需食物
#define BUILDING_MARKET_GOLD_UPGRADE_WOOD 100 //金矿开采所需木头
#define TIME_BUILDING_MARKET_UPGRADE_GOLD 60 //金矿开采所需时间
#define BUILDING_MARKET_GOLD_UPGRADE_ADDITION_CARRY 3 //金矿开采：采金携带 +3
#define BUILDING_MARKET_GOLD_UPGRADE_ADDITION_GATHERRATE 0.2 //金矿开采：采金速率配置值 +0.2
//升级畜牧（1级）
#define BUILDING_MARKET_FARM_UPGRADE_FOOD 200 //驯养动物所需食物
#define BUILDING_MARKET_FARM_UPGRADE_WOOD 50 //驯养动物所需木头
#define TIME_BUILDING_MARKET_UPGRADE_FARM 60 //驯养动物所需时间
#define BUILDING_MARKET_FARM_UPGRADE_ADDITION_FOOD 75 //驯养动物：农田容量 +75
//升级畜牧（2级）
#define BUILDING_MARKET_PLOW_UPGRADE_FOOD 250 //犁所需食物，前置：驯养动物
#define BUILDING_MARKET_PLOW_UPGRADE_WOOD 75 //犁所需木头
#define TIME_BUILDING_MARKET_PLOW_UPGRADE 40 //犁所需时间
#define BUILDING_MARKET_PLOW_UPGRADE_ADDITION_FOOD 75 //犁：农田容量再 +75
//升级车轮
#define BUILDING_MARKET_WHEEL_UPGRADE_FOOD 150 //车轮所需食物
#define BUILDING_MARKET_WHEEL_UPGRADE_WOOD 100 //车轮所需木头
#define TIME_BUILDING_MARKET_WHEEL_UPGRADE 40 //车轮所需时间
// 车轮效果：+30%，村民移动速度 +30%；解锁四马战车、战车弓兵
/**农田**/
#define BLOOD_BUILD_FARM 50 //血量
#define CNT_BUILD_FARM 250 //基础食物量
#define VISION_FARM 4 //视野
#define BUILD_FARM_WOOD 75 //建造所需木头，前置：市场
#define TIME_BUILD_FARM 30 //建造时间（秒）
// 科技后容量：325 / 400，驯养动物后 325；再研发犁后 400
/**箭塔**/
#define BLOOD_BUILD_ARROWTOWER 125 //血量
#define VISION_ARROWTOWER 10 //视野
#define BUILD_ARROWTOWER_STONE 150 //建造所需石头，前置：谷仓研发建造箭塔
#define TIME_BUILD_ARROWTOWER 80 //建造时间（秒）
#define ATK_BUILD_ARROWTOWER 3 //基础攻击力
#define DIS_ARROWTOWER 7 //基础攻击距离
#define DIS_BUILD_ARROWTOWER 5 //对近战防御力
#define DEFSHOOT_BUILD_ARROWTOWER 2 //对箭矢类远程防御力
// 升级后攻击/射程：4 / 8，即升级箭塔后攻击 +1、射程 +1
/**船坞**/
#define BLOOD_BUILD_DOCK 350 //血量
#define VISION_DOCK 4 //视野
#define BUILD_DOCK_WOOD 100 //建造所需木头
#define TIME_BUILD_DOCK 40 //建造时间（秒）
#define BUILDING_DOCK_CREATE_SAILING_WOOD 60 //建造渔船所需木头
#define TIME_BUILDING_DOCK_CREATE_SAILING 30 //建造渔船所需时间
#define BUILDING_DOCK_CREATE_WOOD_BOAT_WOOD 150 //建造运输船所需木头
#define TIME_BUILDING_DOCK_CREATE_WOOD_BOAT 30 //建造运输船所需时间
#define BUILDING_DOCK_CREATE_SHIP_WOOD 135 //建造战船所需木头
#define TIME_BUILDING_DOCK_CREATE_SHIP 30 //建造战船所需时间
/**攻城武器厂**/
#该建筑禁止建造
#define BLOOD_BUILD_SIEGE 350 //血量
#define VISION_SIEGE 4 //视野
#define BUILD_SIEGE_WOOD 180 //建造所需木头，配置中玩家禁用
#define TIME_BUILD_SIEGE 40 //建造时间（秒）
#define BUILDING_SIEGE_CREATE_STONE_THROWER_WOOD 180 //制造投石车所需木头
#define BUILDING_SIEGE_CREATE_STONE_THROWER_GOLD 80 //制造投石车所需黄金
#define TIME_BUILDING_SIEGE_CREATE_STONE_THROWER 30 //制造投石车所需时间
/**学院**/
#define BLOOD_BUILD_COLLAGE 350 //血量
#define VISION_COLLAGE 4 //视野
#define BUILD_COLLAGE_WOOD 180 //建造所需木头
#define TIME_BUILD_COLLAGE 40 //建造时间（秒）
#define BUILDING_COLLAGE_CREATE_HOPLITE_FOOD 60 //训练方阵兵所需食物
#define BUILDING_COLLAGE_CREATE_HOPLITE_GOLD 40 //训练方阵兵所需黄金
#define TIME_BUILDING_COLLAGE_CREATE_HOPLITE 30 //训练方阵兵所需时间
4、军队种类（tagArmy::sort 存储的信息） 
AT_CLUBMAN = 0 //棍棒兵
AT_SLINGER = 1 //投石兵
AT_BOWMAN = 2 //弓箭手
AT_SCOUT = 3 //侦察骑兵
AT_SWORDSMAN = 4 //战斧兵或敌方近战单位
AT_IMPROVED = 5 //改进型单位或敌方单位
AT_CAVALRY = 6 //骑兵
AT_SHIP = 7 //战船
AT_STONE_THROWER = 8 //投石车
AT_PRIEST = 9 //祭司
AT_HOPLITE = 10 //方阵兵
AT_CHARIOT = 11 //四马战车
AT_CHARIOT_ARCHER = 12 //战车弓兵
AT_BROADSWORDSMAN = 13 //阔剑兵
AT_COMPOSITE_BOWMAN = 14 //复合弓兵
（注意 使用double类常量时 需使用--(double)常量名--对其进行强制转换后使用）

5、AI 控制函数错误码 
Action 成员函数会返回一个大于0的ID，作为该命令的唯一标识，供用户在下一帧 检查该命令是否执行成功。在发出命令的下一帧，即再次进入 processData()时，可获 取Info 直接用Info.ins_ret[id]查询，得到该命令的执行结果数值获取详细的错误码信息。 错误码仅适合在调试时检查，实际编写代码时应该避免依赖错误码来处理人物逻辑。 
错误码常量表如下： 
#define ACTION_INVALID_SN -1 //SN不存在或非法控制SN
#define ACTION_INVALID_ACTION -2 //Action不存在
#define ACTION_INVALID_LOCATION -3 //指定位置超界
#define ACTION_INVALID_OBSN -4 //obSN不存在或obSN非可执行行动对象
#define ACTION_INVALID_BUILDINGNUM -5 //BuildingNum不存在
#define ACTION_INVALID_RESOURCE -6 //资源不足
#define ACTION_INVALID_NULLWORKER -80 //控制对象被删除
#define ACTION_INVALID_NULLGOALOBJECT -81 //目标对象已被删除
#define ACTION_INVALID_ISNTFREE -82 //对象已有必须手动取消的任务，不空闲
#define ACTION_INVALID_BUILDACT_NEEDBUILT -11 //建筑还在建造过程中
#define ACTION_INVALID_BUILDACT_LOCK -13 //建筑行动未解锁，或该行动只能进行有限次且已达上限
#define ACTION_INVALID_BUILDACT_MAXHUMAN -14 //造单位行动，已达人口上限
#define ACTION_INVALID_HUMANBUILD_DIFFERENTHIGH -41 //建筑位置有高度差
#define ACTION_INVALID_HUMANBUILD_OVERBORDER -42 //建筑位置加上建筑宽度，超出边界
#define ACTION_INVALID_HUMANBUILD_UNEXPLORE -43 //建筑位置未被探索
#define ACTION_INVALID_HUMANBUILD_OVERLAP -44 //建筑位置与其他物体有重叠冲突
#define ACTION_INVALID_HUMANBUILD_LOCK -45 //建筑未解锁，未达成建筑条件

6、资源类型常量，tagResource::Type存储的信息：
RESOURCE_EMPTY = 0 //空资源
RESOURCE_BUSH = 1 //浆果丛
RESOURCE_TREE = 2 //树木或森林
RESOURCE_STONE = 3 //石矿
RESOURCE_GAZELLE = 4 //瞪羚或瞪羚尸体
RESOURCE_ELEPHANT = 5 //大象或大象尸体
RESOURCE_LION = 6 //狮子或狮子尸体
RESOURCE_GOLD = 7 //金矿
RESOURCE_FISH = 8 //鱼
7、Farmer采集或手持的资源类型常量：
HUMAN_UNKNOWN = 0 //未知
HUMAN_WOOD = 1 //木头
HUMAN_STOCKFOOD = 2 //可放入仓库或市镇中心的食物
HUMAN_STONE = 3 //石头
HUMAN_GOLD = 4 //黄金
HUMAN_GRANARYFOOD = 5 //可放入谷仓或市镇中心的食物
HUMAN_DOCKFOOD = 6 //鱼肉，可放入船坞或市镇中心

8、FarmerSort农民类型常量：
FARMERTYPE_FARMER = 0 //普通陆地村民
FARMERTYPE_WOOD_BOAT = 1 //运输船
FARMERTYPE_SAILING = 2 //渔船

9、文明阶段常量：
CIVILIZATION_UNKNOWN = 0 //未知
CIVILIZATION_STONEAGE = 1 //石器时代
CIVILIZATION_TOOLAGE = 2 //工具时代
CIVILIZATION_BRONZEAGE = 3 //铜器时代
CIVILIZATION_IRONAGE = 4 //铁器时代

10、其他可能用到的资源常量
//血量
BLOOD_TREE 25
BLOOD_GAZELLE 8
BLOOD_ELEPHANT 45
BLOOD_LION 20
BLOOD_FOREST 100
//资源总量
CNT_TREE 75
CNT_GAZELLE 150
CNT_ELEPHANT 300
CNT_LION 100
CNT_UPGRADEFARM 325
CNT_BUSH 150
CNT_STONE 250
CNT_GOLDORE 200
CNT_FOREST 300
CNT_FISH 200

11、村民的nowState状态常量
#define HUMAN_STATE_IDLE 0 //空闲状态 
#define HUMAN_STATE_WALKING 1 //正在移动状态（无目标对象） 
#define HUMAN_STATE_WORKING 2 //正在工作状态（包括打猎） 
#define HUMAN_STATE_ATTACKING 3 //正在攻击敌军状态

```

| 建筑名称   | 建造时代 | 建筑大小 | 建造花费 | 建造时间 | 可执行的行动                   | 备注                                    |
| ---------- | -------- | -------- | -------- | -------- | ------------------------------ | --------------------------------------- |
| 市镇中心   | 开局拥有 | 3x3      | -        | -        | 生产村民、升级时代             | 核心建筑；可存放全部资源；提供初始人口  |
| 房屋       | 石器时代 | 2x2      | 30木头   | 20s      | -                              | 每座增加4人口上限                       |
| 谷仓       | 石器时代 | 3x3      | 120木头  | 30s      | 研究箭塔、升级箭塔             | 存放农场/浆果类食物                     |
| 仓库       | 石器时代 | 3x3      | 120木头  | 30s      | 研究攻防科技                   | 存放木头、石头、黄金和狩猎食物          |
| 兵营       | 石器时代 | 3x3      | 125木头  | 40s      | 训练棍棒兵、投石兵、阔剑兵     | 新增阔剑兵科技链                        |
| 市场       | 工具时代 | 3x3      | 150木头  | 40s      | 采集科技、车轮、工艺、犁       | 需先建造谷仓                            |
| 靶场       | 工具时代 | 3x3      | 150木头  | 40s      | 训练弓箭手、战车弓兵、复合弓兵 | 需先建造谷仓                            |
| 马厩       | 工具时代 | 3x3      | 150木头  | 40s      | 训练侦察骑兵、四马战车、骑兵   | 需先建造兵营；战车需车轮科技            |
| 农场       | 工具时代 | 3x3      | 75木头   | 30s      | -                              | 需要先建造市场；基础250食物，科技后提高 |
| 箭塔       | 工具时代 | 2x2      | 150石头  | 80s      | 自动攻击敌军或建筑             | 需先建造谷仓                            |
| 学院       | 铜器时代 | 3x3      | 180木头  | 40s      | 训练方阵兵                     | 需要先建造马厩                          |
| 攻城武器厂 | 铜器时代 | 3x3      | 180木头  | 40s      | 生产投石车                     | 禁止建造                                |
