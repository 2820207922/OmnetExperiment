#include <cmath>
#include "TopologyManager.h"

using namespace std;
using namespace inet;

namespace OmnetExperiment {

Define_Module(TopologyManager); // 使用Define_Module定义模块，这样ned文件才能找到对应的C++类

const double PI = acos(-1);     // 圆周率
const double EARTH_RADIUS_KM = 6371.0; // 平均地球半径，单位为公里
const double CONSTANT = 299792458;  // 光速，单位为米/秒

TopologyManager::TopologyManager() {
    // Constructor
}

TopologyManager::~TopologyManager() {
    // Destructor
}

void TopologyManager::initialize(int stage) {
    EV << "Initializing topology manager, stage: " << stage << endl;
    // 拓扑生成入口
    if (stage == INITSTAGE_LOCAL) {
        initializeParameters(); // 初始化参数
        if (!validateNetwork()) {   // 验证网络
            EV << "Error: The number of VNs does not match!" << endl;
            return;
        }
        setupNetworkTopology(); // 开始生成网络拓扑
    }
}

void TopologyManager::initializeParameters() {
    // 获取ned文件中的参数
    num_of_planes = par("num_of_planes").intValue();
    sta_per_plane = par("sta_per_plane").intValue();
    width = par("width").doubleValue();
    height = par("height").doubleValue();
    propagation_bitrate = par("propagation_bitrate").doubleValue();
}

bool TopologyManager::validateNetwork() const {
    // 判断网络是否有效，即虚拟节点的总数量是否与轨道和卫星数量匹配
    int numOfVN = getParentModule()->par("numOfVN").intValue();
    return numOfVN == num_of_planes * sta_per_plane;
}

void TopologyManager::setupNetworkTopology() {
    // 网络拓扑生成分为四个部分：
    // 1. 设置虚拟节点及其相关参数
    // 2. 连接虚拟节点，一星四链结构
    // 3. 断开极地区域虚拟节点经度方向上的连接
    // 4. 设置地面站及其相关参数，并连接到离它最近的虚拟节点
    for (int i = 0; i < num_of_planes; ++i) {
        for (int j = 0; j < sta_per_plane; ++j) {
            // 设置虚拟节点
            // 这里的i和j分别表示轨道和卫星的索引
            // 例如，i=0,j=0表示第一个轨道上的第一个卫星
            setupVN(i, j);
        }
    }
    for (int i = 0; i < num_of_planes; ++i) {
        for (int j = 0; j < sta_per_plane; ++j) {
            // 连接虚拟节点
            connectVN(i, j);
        }
    }
    for (int i = 0; i < num_of_planes; ++i) {
        for (int j = 0; j < sta_per_plane; ++j) {
            // 断开极地区域虚拟节点经度方向上的连接
            disconnectVN(i, j);
        }
    }
    int numOfGS = getParentModule()->par("numOfGS").intValue();
    for (int i = 0; i < numOfGS; ++i) {
        // 设置地面站及其相关参数
        cModule *gs = getParentModule()->getSubmodule("gs", i);
        setupGS(gs);
        // 连接到离它最近的虚拟节点
        cModule* vn = getCloestVN(gs);
        connectGStoVN(gs, vn);
    }
}

void TopologyManager::connectGStoVN(cModule* gs, cModule* vn) {
    // 连接地面站和虚拟节点，需要虚拟节点预留一个端口
    connectModules(gs, vn, 0, 4);
    connectModules(vn, gs, 4, 0);
}

cModule* TopologyManager::getCloestVN(cModule *gs) {
    // 获取离地面站最近的虚拟节点
    int numOfVN = getParentModule()->par("numOfVN").intValue();
    double dist = 1e9;
    int index = -1;
    // 遍历所有虚拟节点
    for (int i = 0; i < numOfVN; ++i) {
        cModule *vn = getParentModule()->getSubmodule("vn", i);
        double distance = calculateDistance(gs, vn);
        // 获取最近虚拟节点
        if (dist > distance) {
            dist = distance;
            index = i;
        }
    }
    // 返回虚拟节点对象
    cModule* res = getParentModule()->getSubmodule("vn", index);
    return res;
}

void TopologyManager::setupGS(cModule *gs) {
    // 设置地面站的经纬度和显示位置
    double longitude = gs->par("longitude").doubleValue();
    double latitude = gs->par("latitude").doubleValue();
    setDisplayPosition(gs, longitude, latitude);
}

void TopologyManager::setupVN(int i, int j) {
    int indices[5];
    setupIndices(i, j, indices);

    // 根据下标获取节点对象
    cModule *vn = getParentModule()->getSubmodule("vn", indices[0]);
    cModule *vn_up = getParentModule()->getSubmodule("vn", indices[1]);
    cModule *vn_down = getParentModule()->getSubmodule("vn", indices[2]);
    cModule *vn_left = getParentModule()->getSubmodule("vn", indices[3]);
    cModule *vn_right = getParentModule()->getSubmodule("vn", indices[4]);

    if (!vn || !vn_up || !vn_down || !vn_left || !vn_right) {
        EV << "Error: Couldn't find module!" << endl;
        return;
    }
    // 设置虚拟节点的经纬度和显示位置
    setModulePosition(vn, i, j);
}

void TopologyManager::connectVN(int i, int j) {
    // 根据轨道和卫星的索引设置虚拟节点
    // 这里的i和j分别表示轨道和卫星的索引
    // indices[0]表示当前虚拟节点的下标
    // indices[1]表示上方虚拟节点的下标
    // indices[2]表示下方虚拟节点的下标
    // indices[3]表示左方虚拟节点的下标
    // indices[4]表示右方虚拟节点的下标
    int indices[5];
    setupIndices(i, j, indices);

    cModule *vn = getParentModule()->getSubmodule("vn", indices[0]);
    cModule *vn_up = getParentModule()->getSubmodule("vn", indices[1]);
    cModule *vn_down = getParentModule()->getSubmodule("vn", indices[2]);
    cModule *vn_left = getParentModule()->getSubmodule("vn", indices[3]);
    cModule *vn_right = getParentModule()->getSubmodule("vn", indices[4]);

    if (!vn) {
        EV << "Error: Couldn't find module!" << endl;
        return;
    }
    // 连接虚拟节点，一星四链结构
    connectModules(vn, vn_up, 0, 2);
    connectModules(vn, vn_down, 2, 0);
    connectModules(vn, vn_left, 3, 1);
    connectModules(vn, vn_right, 1, 3);
}
void TopologyManager::disconnectVN(int i, int j) {
    // 根据轨道和卫星的索引设置虚拟节点
    // 这里的i和j分别表示轨道和卫星的索引
    // indices[0]表示当前虚拟节点的下标
    // indices[1]表示上方虚拟节点的下标
    // indices[2]表示下方虚拟节点的下标
    // indices[3]表示左方虚拟节点的下标
    // indices[4]表示右方虚拟节点的下标
    int indices[5];
    setupIndices(i, j, indices);

    cModule *vn = getParentModule()->getSubmodule("vn", indices[0]);
    cModule *vn_up = getParentModule()->getSubmodule("vn", indices[1]);
    cModule *vn_down = getParentModule()->getSubmodule("vn", indices[2]);
    cModule *vn_left = getParentModule()->getSubmodule("vn", indices[3]);
    cModule *vn_right = getParentModule()->getSubmodule("vn", indices[4]);

    if (!vn) {
        EV << "Error: Couldn't find module!" << endl;
        return;
    }

    // 断开极地区域虚拟节点经度方向上的连接
    if (abs(vn->par("latitude").doubleValue()) > 75.0) {
//        disconnectModules(vn, vn_up, 0, 2);
//        disconnectModules(vn, vn_down, 2, 0);
        disconnectModules(vn, vn_left, 3, 1);
        disconnectModules(vn, vn_right, 1, 3);
    }
}

void TopologyManager::setupIndices(int i, int j, int* indices) const {
    // 根据轨道和卫星的索引计算虚拟节点下标
    indices[0] = i * sta_per_plane + j;
    // 上方虚拟节点,下方虚拟节点
    indices[1] = i * sta_per_plane + (j - 1 + sta_per_plane) % sta_per_plane;
    indices[2] = i * sta_per_plane + (j + 1) % sta_per_plane;
    // 左方虚拟节点,右方虚拟节点
    if (i > 0) {
        indices[3] = ((i - 1) % num_of_planes) * sta_per_plane + j;
    }
    else {
        // 第0个轨道的左方虚拟节点连接到第num_of_planes-1个轨道的虚拟节点
        int jj = (sta_per_plane - j - 1) % sta_per_plane;
        indices[3] = ((i - 1 + num_of_planes) % num_of_planes) * sta_per_plane + jj;
    }

    if (i < num_of_planes - 1) {
        indices[4] = ((i + 1) % num_of_planes) * sta_per_plane + j;
    }
    else {
        // 第num_of_planes-1个轨道的右方虚拟节点连接到第0个轨道的虚拟节点
        int jj = (sta_per_plane - j - 1) % sta_per_plane;
        indices[4] = ((i + 1) % num_of_planes) * sta_per_plane + jj;
    }
}

void TopologyManager::connectModules(cModule* source, cModule* dest, int outIndex, int inIndex) {
    // 连接模块，创建由source到dest的单向连接
    cChannelType *chType = cChannelType::get("inet.node.ethernet.EthernetLink");
    cDatarateChannel* ch = dynamic_cast<cDatarateChannel*>(chType->create("vn_channel"));
    // 计算两个节点之间的距离、传播时延
    double distance = calculateDistance(source, dest);
    ch->par("length").setDoubleValue(distance * 1000);
    ch->par("delay").setDoubleValue(distance * 1000 / CONSTANT);
    // 设置传播比特率
    ch->par("datarate").setDoubleValue(propagation_bitrate);
    // 确保通道参数已固化
    source->gate("ethg$o", outIndex)->connectTo(dest->gate("ethg$i", inIndex), ch);
    ch->finalizeParameters();  // 关键步骤：处理参数依赖
    ch->callInitialize();     // 必须显式初始化动态创建的通道！
}

void TopologyManager::disconnectModules(cModule* source, cModule* dest, int outIndex, int inIndex) {
    // 断开模块连接
    cGate *outGate = source->gate("ethg$o", outIndex);
    if (outGate->isConnected()) {
        outGate->disconnect();
    }
    cGate *inGate = dest->gate("ethg$o", inIndex);
    if (inGate->isConnected()) {
        inGate->disconnect();
    }
}

void TopologyManager::setModulePosition(cModule* vn, int i, int j) {
    // 设置节点经纬度
    double longitude = 180.0 * i / num_of_planes + 90.0 / num_of_planes;
    double latitude = 360.0 / sta_per_plane * j + 180.0 / sta_per_plane;

    if (latitude < 180.0) {
        latitude = 90.0 - latitude;
    }
    else {
        longitude -= 180.0;
        latitude = latitude - 270.0;
    }

    vn->par("longitude").setDoubleValue(longitude);
    vn->par("latitude").setDoubleValue(latitude);

    // 设置节点显示位置
    setDisplayPosition(vn, longitude, latitude);
}

void TopologyManager::setDisplayPosition(cModule* node, double longitude, double latitude) {
    // 设置节点显示位置
    // 将经纬度转换为显示位置
    // 这里背景宽度和高度分别为width和height
    // 经纬度范围：经度[-180, 180]，纬度[-90, 90]
    // 显示位置范围：x[0, width]，y[0, height]
    // 计算显示位置
    // 这里的px和py是显示位置的坐标
    double px = width * (longitude + 180.0) / 360.0;
    double py = height * (-latitude + 90.0) / 180.0;

    node->getDisplayString().setTagArg("p", 0, px);
    node->getDisplayString().setTagArg("p", 1, py);
}

// 将角度转换为弧度
double TopologyManager::deg2rad(double deg) {
    return deg * (M_PI / 180.0);
}

// 计算两个地理坐标之间的距离
double TopologyManager::calculateDistance(cModule* source, cModule* dest) {
    // 从模块参数获取经度、纬度和海拔
    double lon1 = source->par("longitude").doubleValue();
    double lat1 = source->par("latitude").doubleValue();
    double alt1 = source->par("altitude").doubleValue();

    double lon2 = dest->par("longitude").doubleValue();
    double lat2 = dest->par("latitude").doubleValue();
    double alt2 = dest->par("altitude").doubleValue();

    // 将角度转换为弧度
    double radLon1 = deg2rad(lon1);
    double radLat1 = deg2rad(lat1);
    double radLon2 = deg2rad(lon2);
    double radLat2 = deg2rad(lat2);

    double r1 = EARTH_RADIUS_KM + alt1; // 地球半径加上海拔
    double x1 = r1 * cos(radLat1) * cos(radLon1);
    double y1 = r1 * cos(radLat1) * sin(radLon1);
    double z1 = r1 * sin(radLat1);

    double r2 = EARTH_RADIUS_KM + alt2; // 地球半径加上海拔
    double x2 = r2 * cos(radLat2) * cos(radLon2);
    double y2 = r2 * cos(radLat2) * sin(radLon2);
    double z2 = r2 * sin(radLat2);

    double dx = x2 - x1;
    double dy = y2 - y1;
    double dz = z2 - z1;

    double distance = sqrt(dx*dx + dy*dy + dz*dz);

//    EV << "lon1=" << lon1 << "  lat1=" << lat1 << "  lon2=" << lon2 << "  lat2=" << lat2 << "  distance=" << distance << endl;

    return distance;
}

}
