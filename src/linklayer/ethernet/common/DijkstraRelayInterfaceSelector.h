//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef LINKLAYER_ETHERNET_COMMON_DIJKSTRARELAYINTERFACESELECTOR_H_
#define LINKLAYER_ETHERNET_COMMON_DIJKSTRARELAYINTERFACESELECTOR_H_

#include "inet/common/IProtocolRegistrationListener.h"
#include "inet/common/ModuleRefByPar.h"
#include "inet/linklayer/ethernet/contract/IMacForwardingTable.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/queueing/base/PacketPusherBase.h"
#include "inet/common/INETDefs.h"

namespace inet {

using namespace inet::queueing;

// 定义邻接信息结构体
struct NeighborInfo {
    std::string hostName;
    int hostId;
    std::vector<int> connectedIds;
    std::vector<double> connectedDelay;
};

class INET_API DijkstraRelayInterfaceSelector : public PacketPusherBase, public TransparentProtocolRegistrationListener
{
  protected:
    ModuleRefByPar<IInterfaceTable> interfaceTable;   // 接口表
    ModuleRefByPar<IMacForwardingTable> macForwardingTable; // MAC转发表，无用

    long numProcessedFrames = 0;  // 处理的帧数
    long numDroppedFrames = 0;    // 丢弃的帧数

    // 新增成员变量
    cMessage* startupMsg = nullptr;   // Dijkstra算法需要完整的网络拓扑，为此虚拟节点在启动时通过泛洪获取网络中所有节点的邻接信息
    bool isStartupPacketSent = false; // 判断是否启动，发送自身邻接信息到其他节点

    std::vector<NeighborInfo> neighborTable;  // 记录邻接信息表
    int father[500];                          // 计算最短路径时，记录每个节点的父节点，用于路径回溯，查找对应输出接口
    double dis[500];                          // 计算最短路径时，记录每个节点到源节点的距离
    bool vis[500];                            // 计算最短路径时，记录每个节点是否被访问过
    int nextId;                               // 计算最短路径时，记录当前节点的下一个节点，初始值为自身Id


  protected:
    // 这三个函数由OMNeT++自动调用，
    // 主要用于初始化模块、处理消息和接收数据包
    // 其中，initialize()函数在模块创建时调用，handleMessage()函数用于处理启动信号，触发泛洪，pushPacket()函数用于接收数据包
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage* msg) override;
    virtual void pushPacket(Packet *packet, cGate *gate) override;
    // 发送邻接信息数据包，泛洪时调用
    virtual void sendNeighborAwarePacket(const NeighborInfo *neighborInfo, const NetworkInterface* incomingInterface = nullptr);
    // 发送封装后的数据包
    virtual void sendRawPacket(const char* dataName, uint32_t dataType, const std::vector<uint8_t>& byteStream, NetworkInterface * interface);
    // 解析数据包
    virtual std::vector<uint8_t> recvRawPacket(Packet* packet, uint32_t *dataType);
    // 解析数据包后判断其类型，转化为邻接信息
    virtual NeighborInfo getNeighborInfoByByteStream(const std::vector<uint8_t>& byteStream);
    // 基于已有邻接信息计算最短路径
    virtual void calcDijkstra();
    // 查找数据包下一跳的输出接口
    NetworkInterface* getOutputInterface(const std::string& packetFullName);

    // 以下函数与实验不相干
    virtual bool isForwardingInterface(NetworkInterface *networkInterface) const { return !networkInterface->isLoopback() && networkInterface->isBroadcast(); }
    virtual void broadcastPacket(Packet *packet, const MacAddress& destinationAddress, NetworkInterface *incomingInterface);
    virtual void sendPacket(Packet *packet, const MacAddress& destinationAddress, NetworkInterface *outgoingInterface);

    virtual bool isForwardingService(const Protocol& protocol, cGate *gate, ServicePrimitive servicePrimitive) const override { return servicePrimitive == SP_REQUEST; }
    virtual bool isForwardingServiceGroup(const ProtocolGroup& protocolGroup, cGate *gate, ServicePrimitive servicePrimitive) const override { return servicePrimitive == SP_REQUEST; }
    virtual bool isForwardingAnyService(cGate *gate, ServicePrimitive servicePrimitive) const override { return servicePrimitive == SP_REQUEST; }

    virtual bool isForwardingProtocol(const Protocol& protocol, cGate *gate, ServicePrimitive servicePrimitive) const override { return false; }
    virtual bool isForwardingProtocolGroup(const ProtocolGroup& protocolGroup, cGate *gate, ServicePrimitive servicePrimitive) const override { return false; }
    virtual bool isForwardingAnyProtocol(cGate *gate, ServicePrimitive servicePrimitive) const override { return false; }

    virtual cGate *getRegistrationForwardingGate(cGate *gate) override;
};

} // namespace inet

#endif /* LINKLAYER_ETHERNET_COMMON_DIJKSTRARELAYINTERFACESELECTOR_H_ */
