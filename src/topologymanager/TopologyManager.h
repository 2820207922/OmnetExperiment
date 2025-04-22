#ifndef TOPOLOGYMANAGER_TOPOLOGYMANAGER_H_
#define TOPOLOGYMANAGER_TOPOLOGYMANAGER_H_

#include <omnetpp.h>    // 导入OMNeT++的头文件
#include <omnetpp/csimplemodule.h>  // 导入OMNeT++的简单模块头文件
#include "inet/common/InitStages.h" // 导入OMNeT++的初始化阶段头文件

using namespace inet;   // 使用inet命名空间

namespace OmnetExperiment {

class TopologyManager: public omnetpp::cSimpleModule {
public:
    TopologyManager();  // 构造函数，可忽略
    virtual ~TopologyManager(); // 析构函数，可忽略

protected:
    virtual void initialize(int stage) override;    // 初始化函数，OMNet++在仿真开始时调用，通过该函数触发拓扑生成功能

private:
    void initializeParameters();    // 初始化参数函数
    bool validateNetwork() const;   // 判断网络是否有效
    void setupNetworkTopology();    // 设置网络拓扑
    void setupVN(int i, int j);     // 设置虚拟节点
    void connectVN(int i, int j);   // 连接虚拟节点
    void disconnectVN(int i, int j);    // 断开虚拟节点
    void setupGS(cModule *gs);   // 设置地面站
    cModule* getCloestVN(cModule *gs);  // 获取离地面站最近的虚拟节点
    void connectGStoVN(cModule* gs, cModule* vn);   // 连接地面站和虚拟节点
    void setupIndices(int i, int j, int* indices) const;    // 设置索引
    void connectModules(cModule* source, cModule* dest, int outIndex, int inIndex); // 连接模块
    void disconnectModules(cModule* source, cModule* dest, int outIndex, int inIndex);  // 断开模块连接
    void setModulePosition(cModule* vn, int i, int j);  // 设置节点经纬度
    void setDisplayPosition(cModule* node, double longitude, double latitude);  // 设置节点显示位置
    double deg2rad(double deg); // 将角度转换为弧度
    double calculateDistance(cModule* source, cModule* dest);   // 计算节点之间的距离

    // Member variables
    int num_of_planes;  // 轨道数量
    int sta_per_plane;  // 每个轨道上的卫星数量
    double width;   // 背景宽度
    double height;  // 背景高度
    double propagation_bitrate; // 传播比特率
};

}

#endif /* TOPOLOGYMANAGER_TOPOLOGYMANAGER_H_ */
