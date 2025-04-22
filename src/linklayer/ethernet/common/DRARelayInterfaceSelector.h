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

#ifndef LINKLAYER_ETHERNET_COMMON_DRARELAYINTERFACESELECTOR_H_
#define LINKLAYER_ETHERNET_COMMON_DRARELAYINTERFACESELECTOR_H_

#include "inet/common/IProtocolRegistrationListener.h"
#include "inet/common/ModuleRefByPar.h"
#include "inet/linklayer/ethernet/contract/IMacForwardingTable.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/queueing/base/PacketPusherBase.h"
#include "inet/common/INETDefs.h"

namespace inet {

using namespace inet::queueing;
// 定义消息优先级
enum PriorityLevel {
    Normal = 0,
    BestEffort,
    TimeTriggered,
};

class INET_API DRARelayInterfaceSelector : public PacketPusherBase, public TransparentProtocolRegistrationListener
{
  protected:
    ModuleRefByPar<IInterfaceTable> interfaceTable;   // 接口表
    ModuleRefByPar<IMacForwardingTable> macForwardingTable; // MAC转发表，无用

    long numProcessedFrames = 0;  // 处理的帧数
    long numDroppedFrames = 0;    // 丢弃的帧数

    cMessage* startupMsg = nullptr;   // 仅用于判断节点启动
    bool isStartupPacketSent = false;

    cModule *host;                        // 记录自身节点模块
    int selfId, selfN, selfM;             // 记录自身节点Id、所在轨道索引和自身在轨道内的索引
    double selfLongitude, selfLatitude;   // 记录自身节点经纬度
    NetworkInterface  *ethgInterface[5];  // 记录自身节点的5个接口
    double congestedPacketRate = 0.5;     // 记录拥塞阈值（比率，取值0-1）

  protected:
    // 这三个函数由OMNeT++自动调用，
    // 主要用于初始化模块、处理消息和接收数据包
    // 其中，initialize()函数在模块创建时调用，handleMessage()函数用于处理启动信号，触发泛洪，pushPacket()函数用于接收数据包
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage* msg) override;
    virtual void pushPacket(Packet *packet, cGate *gate) override;

    // 根据数据包的完整名称获取源节点索引
    int getSourceIndexByPacketFullName(const std::string& packetFullName);
    // 根据数据包的完整名称获取目标节点索引
    int getDestIndexByPacketFullName(const std::string& packetFullName);
    // 根据数据包的完整名称获取优先级
    PriorityLevel getPriorityLevelByPacketFullName(const std::string& packetFullName);
    // 根据数据包的完整名称获取目标节点的输出接口
    NetworkInterface* getOutputInterface(const std::string& packetFullName);
    // 判断输出接口是否拥塞
    bool isCongestedByOutputIndex(int outputIndex, PriorityLevel pri);

    // 以下函数与实验无关
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

#endif /* LINKLAYER_ETHERNET_COMMON_DRARELAYINTERFACESELECTOR_H_ */
