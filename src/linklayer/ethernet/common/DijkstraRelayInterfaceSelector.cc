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

#include "DijkstraRelayInterfaceSelector.h"

#include "inet/common/DirectionTag_m.h"
#include "inet/common/ProtocolUtils.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/common/VlanTag_m.h"
#include "inet/common/ProtocolGroup.h"
#include "inet/linklayer/ethernet/common/EthernetMacHeader_m.h"
#include "inet/linklayer/ieee8022/Ieee8022SnapHeader_m.h"
#include "inet/linklayer/ieee8022/Ieee8022LlcHeader_m.h"
#include "inet/linklayer/ethernet/common/Ethernet.h"


namespace inet {

Define_Module(DijkstraRelayInterfaceSelector);  // 定义模块，用于ned文件绑定

void DijkstraRelayInterfaceSelector::initialize(int stage)
{
    // 初始化阶段
    PacketPusherBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        macForwardingTable.reference(this, "macTableModule", true);
        interfaceTable.reference(this, "interfaceTableModule", true);
        WATCH(numProcessedFrames);
        WATCH(numDroppedFrames);

        // 设置泛洪启动消息，发送给自身
        startupMsg = new cMessage("StartupTrigger");
        // 在仿真开始时就发送
        scheduleAt(simTime(), startupMsg);
    }
}
// 泛洪启动消息处理逻辑
void DijkstraRelayInterfaceSelector::handleMessage(cMessage* msg) {
    if (msg == startupMsg) {
        // 防止重复发送
        if (!isStartupPacketSent) {
            // 获取自身虚拟节点对象
            cModule *host = getParentModule()->getParentModule()->getParentModule();
            // 获取自身邻接信息，包括节点名称、ID、相邻节点ID和对应链路时延
            NeighborInfo neighborInfo = {};
            // 获取自身节点名称和ID
            neighborInfo.hostName = host->getFullName();
            neighborInfo.hostId = host->getId();
            // 遍历所有连接的网卡，获取相邻节点ID和链路时延
            int ethgSize = host->gateSize("ethg");
            for (int i = 0; i < ethgSize; ++i) {
                cGate *ethg = host->gate("ethg$o", i);
                if (!ethg->isConnected())
                    continue;
                cChannel* channel = ethg->getChannel();
                double delay = channel->par("delay").doubleValue();
                // 获取链路时延
                neighborInfo.connectedDelay.push_back(delay);
                cGate* remoteGate = ethg->getNextGate();
                if (remoteGate != nullptr) {
                    cModule* remoteModule = remoteGate->getOwnerModule();
                    int remoteId = remoteModule->getId();
                    // 获取相邻节点ID
                    neighborInfo.connectedIds.push_back(remoteId);
                }
            }
            // Dijkstra算法初始化
            for (int i = 0; i < 500; ++i) {
                dis[i] = 1e9;
                vis[i] = false;
            }
            // 设置自身节点为源节点
            nextId = host->getId();
            dis[nextId] = 0;
            father[nextId] = -1;

            // 初始化邻接表，将自身邻接信息加入
            neighborTable = {};
            neighborTable.push_back(neighborInfo);
            // 根据已有邻接信息计算最短路径
            calcDijkstra();
            // 发送邻接信息数据包
            sendNeighborAwarePacket(&neighborInfo);
            // 设置标志位，防止重复发送
            isStartupPacketSent = true;
        }
        delete msg;
        startupMsg = nullptr;
        return;
    }
    // 原有消息处理保持不变
    PacketPusherBase::handleMessage(msg);
}

// 与实验不相干
cGate *DijkstraRelayInterfaceSelector::getRegistrationForwardingGate(cGate *gate)
{
    if (gate == outputGate)
        return inputGate;
    else if (gate == inputGate)
        return outputGate;
    else
        throw cRuntimeError("Unknown gate");
}

// 封装邻接信息数据包，并发送
void DijkstraRelayInterfaceSelector::sendNeighborAwarePacket(const NeighborInfo *neighborInfo, const NetworkInterface* incomingInterface) {
    cModule *host = getParentModule()->getParentModule()->getParentModule();
    // 构造自定义数据
    std::string hostName = neighborInfo->hostName;
    int hostId = neighborInfo->hostId;
    std::vector<int> connectedIds = neighborInfo->connectedIds;
    std::vector<double> connectedDelay = neighborInfo->connectedDelay;

    EV << "Send Packet: " << hostName.c_str() << " " << hostId << " neighbors: [";
    for (auto it = connectedIds.begin(); it != connectedIds.end(); ++it) {
        EV_INFO << *it << " ";
    }
    EV_INFO << "]\n";

    // 序列化数据到字节流
    std::vector<uint8_t> byteStream;
    uint16_t nameLen = hostName.size();
    byteStream.insert(byteStream.end(),
                    reinterpret_cast<uint8_t*>(&nameLen),
                    reinterpret_cast<uint8_t*>(&nameLen)+sizeof(nameLen));
    byteStream.insert(byteStream.end(),
                    hostName.begin(), hostName.end());

    byteStream.insert(byteStream.end(),
                    reinterpret_cast<uint8_t*>(&hostId),
                    reinterpret_cast<uint8_t*>(&hostId)+sizeof(hostId));

    uint16_t count = connectedIds.size();
    byteStream.insert(byteStream.end(),
                    reinterpret_cast<uint8_t*>(&count),
                    reinterpret_cast<uint8_t*>(&count)+sizeof(count));

    for (int id : connectedIds) {
        byteStream.insert(byteStream.end(),
                        reinterpret_cast<uint8_t*>(&id),
                        reinterpret_cast<uint8_t*>(&id)+sizeof(id));
    }

    for (double delay : connectedDelay) {
        byteStream.insert(byteStream.end(),
                        reinterpret_cast<uint8_t*>(&delay),
                        reinterpret_cast<uint8_t*>(&delay)+sizeof(delay));
    }

    int ethgSize = host->gateSize("ethg");
    for (int i = 0; i < ethgSize; ++i)
    {
        auto interface = interfaceTable->getInterface(i);
        std::string name = host->getName();
        // 地面站自带一个本地环回接口，其输出接口下标需要加1
        if (name == "gs") {
            interface = interfaceTable->getInterface(i + 1);
        }
        cGate *ethg = host->gate("ethg$o", i);
        // 过滤掉环回接口、未连接的接口和输入接口
        if (interface->isLoopback() || !ethg->isConnected() || (incomingInterface != nullptr && interface == incomingInterface))
            continue;

        sendRawPacket("NeighborAwarePacket", 0x1234, byteStream, interface);
    }
}

// 发送封装后的数据包
void DijkstraRelayInterfaceSelector::sendRawPacket(const char* dataName, uint32_t dataType, const std::vector<uint8_t>& byteStream, NetworkInterface *interface) {
    // ==== 以太网头关键配置 ====
    auto ethHeader = makeShared<EthernetMacHeader>();
    ethHeader->setSrc(interface->getMacAddress());
    ethHeader->setDest(MacAddress::UNSPECIFIED_ADDRESS); // 目标地址
    ethHeader->setTypeOrLength(byteStream.size()); // 设置为有效载荷长度（必须<=1500）
    ethHeader->setChunkLength(B(14));  // 6+6+2字节

    // ==== LLC头合法封装 ====
    auto llcHeader = makeShared<Ieee8022LlcHeader>();
    llcHeader->setDsap(0xAA);       // SNAP标识
    llcHeader->setSsap(0xAA);
    llcHeader->setControl(0x03);    // 无连接
    llcHeader->setChunkLength(B(3));// 强制长度设置

    // ==== SNAP头协议标识 ====
    auto snapHeader = makeShared<Ieee8022SnapHeader>();
    snapHeader->setOui(0x000000);   // 通用OUI
    snapHeader->setProtocolId(dataType); // 自定义协议ID
    snapHeader->setChunkLength(B(5));

    // ==== 数据包组装 ====
    Packet *packet = new Packet(dataName);
    packet->insertAtFront(ethHeader);
    packet->insertAtBack(llcHeader);
    packet->insertAtBack(snapHeader);
    packet->insertAtBack(makeShared<BytesChunk>(byteStream));

    // ==== 协议标签强制设置 ====
    packet->addTag<PacketProtocolTag>()->setProtocol(&Protocol::ieee8022llc);
    packet->addTag<DispatchProtocolReq>()->setProtocol(&Protocol::ieee8022llc);

    // ==== 接口绑定 ====
    packet->addTag<InterfaceReq>()->setInterfaceId(interface->getInterfaceId());
    auto macReq = packet->addTag<MacAddressReq>();
    macReq->setSrcAddress(interface->getMacAddress());
    macReq->setDestAddress(MacAddress::UNSPECIFIED_ADDRESS);

    // ==== 发送数据包 ====
    sendPacket(packet, MacAddress::UNSPECIFIED_ADDRESS, interface);
}

// 接收数据包，解析以太网头、LLC头和SNAP头
std::vector<uint8_t> DijkstraRelayInterfaceSelector::recvRawPacket(Packet* packet, uint32_t *dataType) {
    std::vector<uint8_t> byteStream;
    // ==== 协议验证 ====
    // 检查协议标签是否匹配LLC协议
    auto protocolTag = packet->findTag<PacketProtocolTag>();
    if (!protocolTag || protocolTag->getProtocol() != &Protocol::ieee8022llc) {
        EV_ERROR << "Received packet with invalid protocol type" << std::endl;
        throw cRuntimeError("Received packet with invalid protocol type");
        return byteStream; // 返回空数据表示错误
    }

    // ==== 以太网头解析 ====
    auto ethHeader = packet->peekAtFront<EthernetMacHeader>();
    if (!ethHeader) {
        EV_ERROR << "Malformed Ethernet header" << std::endl;
        throw cRuntimeError("Malformed Ethernet header");
        return byteStream;
    }
    packet->popAtFront(ethHeader->getChunkLength()); // 移除已解析的头部

    // ==== LLC头验证 ====
    auto llcHeader = packet->popAtFront<Ieee8022LlcHeader>();
    if (llcHeader->getDsap() != 0xAA ||
        llcHeader->getSsap() != 0xAA ||
        llcHeader->getControl() != 0x03) {
        EV_ERROR << "Invalid LLC header values" << std::endl;
        throw cRuntimeError("Invalid LLC header values");
        return byteStream;
    }

    // ==== SNAP头验证 ====
    auto snapHeader = packet->popAtFront<Ieee8022SnapHeader>();
    if (snapHeader->getOui() != 0x000000) {
        EV_ERROR << "Unexpected SNAP OUI value" << std::endl;
        throw cRuntimeError("Unexpected SNAP OUI value");
        return byteStream;
    }
    *dataType = snapHeader->getProtocolId();

    // ==== 负载提取 ====
    if (auto payloadChunk = packet->peekDataAsBytes(); payloadChunk) {
        byteStream = payloadChunk->getBytes();  // 直接获取字节流
    } else {
        EV_ERROR << "No payload data found" << std::endl;
        throw cRuntimeError("No payload data found");
    }
    // ==== 数据包清理 ====
//    throw cRuntimeError("I am OK!");
    return byteStream;
}

// 解析字节流，获取邻接信息
NeighborInfo DijkstraRelayInterfaceSelector::getNeighborInfoByByteStream(const std::vector<uint8_t>& byteStream) {
    NeighborInfo neighborInfo;
    size_t pos = 0;

    // 解析节点名长度
    if (pos + sizeof(uint16_t) > byteStream.size()) {
        throw cRuntimeError("字节流不完整：无法读取节点名长度");
    }
    uint16_t nameLen;
    std::memcpy(&nameLen, byteStream.data() + pos, sizeof(nameLen));
    pos += sizeof(nameLen);

    // 解析节点名字符串
    if (pos + nameLen > byteStream.size()) {
        throw cRuntimeError("字节流不完整：无法读取节点名");
    }
    neighborInfo.hostName.assign(
        reinterpret_cast<const char*>(byteStream.data() + pos),
        nameLen
    );
    pos += nameLen;

    // 解析节点ID
    if (pos + sizeof(int) > byteStream.size()) {
        throw cRuntimeError("字节流不完整：无法读取节点ID");
    }
    std::memcpy(&neighborInfo.hostId, byteStream.data() + pos, sizeof(int));
    pos += sizeof(int);

    // 解析连接的模块数量
    if (pos + sizeof(uint16_t) > byteStream.size()) {
        throw cRuntimeError("字节流不完整：无法读取连接数量");
    }
    uint16_t count;
    std::memcpy(&count, byteStream.data() + pos, sizeof(count));
    pos += sizeof(count);

    // 解析连接的模块ID列表
    neighborInfo.connectedIds.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        if (pos + sizeof(int) > byteStream.size()) {
            throw cRuntimeError("字节流不完整：无法读取模块ID");
        }
        int id;
        std::memcpy(&id, byteStream.data() + pos, sizeof(id));
        neighborInfo.connectedIds.push_back(id);
        pos += sizeof(int);
    }
    neighborInfo.connectedDelay.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        if (pos + sizeof(double) > byteStream.size()) {
            throw cRuntimeError("字节流不完整：无法读取模块ID");
        }
        double delay;
        std::memcpy(&delay, byteStream.data() + pos, sizeof(delay));
        neighborInfo.connectedDelay.push_back(delay);
        pos += sizeof(double);
    }
    return neighborInfo;
}

// 数据包接收，由OMNeT++自动触发调用
void DijkstraRelayInterfaceSelector::pushPacket(Packet *packet, cGate *gates)
{
    Enter_Method("pushPacket");
    take(packet);
    auto interfaceReq = packet->findTag<InterfaceReq>();
    auto macAddressReq = packet->getTag<MacAddressReq>();
    auto destinationAddress = macAddressReq->getDestAddress();
    if (interfaceReq != nullptr) {
        auto networkInterface = interfaceTable->getInterfaceById(interfaceReq->getInterfaceId());
//        if (isStartupPacketSent) {
//            throw cRuntimeError("I get 1!");
//        }
        sendPacket(packet, destinationAddress, networkInterface);
    }
    else {
        auto interfaceInd = packet->findTag<InterfaceInd>();
        auto incomingInterface = interfaceInd != nullptr ? interfaceTable->getInterfaceById(interfaceInd->getInterfaceId()) : nullptr;
        auto vlanReq = packet->findTag<VlanReq>();
        int vlanId = vlanReq != nullptr ? vlanReq->getVlanId() : 0;
        if (destinationAddress.isBroadcast()) {
//            if (isStartupPacketSent) {
//                throw cRuntimeError("I get 2!");
//            }
            broadcastPacket(packet, destinationAddress, incomingInterface);
        }
        else if (destinationAddress.isMulticast()) {
            auto outgoingInterfaceIds = macForwardingTable->getMulticastAddressForwardingInterfaces(destinationAddress, vlanId);
            if (outgoingInterfaceIds.size() == 0) {
//                if (isStartupPacketSent) {
//                    throw cRuntimeError("I get 3!");
//                }
                broadcastPacket(packet, destinationAddress, incomingInterface);
            }
            else {
                for (auto outgoingInterfaceId : outgoingInterfaceIds) {
                    if (interfaceInd != nullptr && outgoingInterfaceId == interfaceInd->getInterfaceId())
                        EV_WARN << "Ignoring outgoing interface because it is the same as incoming interface" << EV_FIELD(destinationAddress) << EV_FIELD(incomingInterface) << EV_FIELD(packet) << EV_ENDL;
                    else {
                        auto outgoingInterface = interfaceTable->getInterfaceById(outgoingInterfaceId);
//                        if (isStartupPacketSent) {
//                            throw cRuntimeError("I get 4!");
//                        }
                        sendPacket(packet->dup(), destinationAddress, outgoingInterface);
                    }
                }
                delete packet;
            }
        }
        else {
            // Find output interface of destination address and send packet to output interface
            // if not found then broadcasts to all other interfaces instead
            int outgoingInterfaceId = macForwardingTable->getUnicastAddressForwardingInterface(destinationAddress, vlanId);
            // should not send out the same packet on the same interface
            // (although wireless interfaces are ok to receive the same message)
            if (outgoingInterfaceId != -1) {
                if (isStartupPacketSent) {
                    // 实验发送的数据包最终会来到这里
                    // 获取数据包的完整名称
                    std::string packetFullName = packet->getFullName();
                    // 获取数据包的输出接口
                    auto outgoingInterface = getOutputInterface(packetFullName);
                    EV_INFO << "packetFullName is " << packetFullName << std::endl;
                    EV_INFO << "outgoingInterface is " << outgoingInterface << std::endl;
//                    throw cRuntimeError("I get 5!");
                    // 发送数据包
                    sendPacket(packet, destinationAddress, outgoingInterface);
                }
                else {
                    auto outgoingInterface = interfaceTable->getInterfaceById(outgoingInterfaceId);
                    sendPacket(packet, destinationAddress, outgoingInterface);
                }
            }
            else {
//                throw cRuntimeError("I get 6!");
                if (isStartupPacketSent && packet->getTag<PacketProtocolTag>()->getProtocol() == &Protocol::ieee8022llc) {
                    // 泛洪接收到的邻接信息数据包最终会来到这里
                    uint32_t dataType = 0;
                    std::vector<uint8_t> byteStream;
                    // 解析数据包类型，获取字节流
                    byteStream = recvRawPacket(packet, &dataType);
                    EV_INFO << "Received packet with protocol type " << dataType << std::endl;
                    if (dataType == 0x1234) {
                        // 数据包类型正确，解析字节流，获取邻接信息
                        NeighborInfo recvNeighborInfo = getNeighborInfoByByteStream(byteStream);
                        EV_INFO << "Received recvNeighborInfo:  " << recvNeighborInfo.hostName << " " << recvNeighborInfo.hostId << " neighbors: ";
                        for (auto it = recvNeighborInfo.connectedIds.begin(); it != recvNeighborInfo.connectedIds.end(); ++it) {
                            EV_INFO << *it << " ";
                        }
                        EV_INFO << "Delay: ";
                        for (auto it = recvNeighborInfo.connectedDelay.begin(); it != recvNeighborInfo.connectedDelay.end(); ++it) {
                            EV_INFO << *it << " ";
                        }
                        EV_INFO << "\n";
                        // 检查邻接信息是否已经存在，避免邻接信息消息风暴
                        bool isExist = false;
                        for (auto it = neighborTable.begin(); it != neighborTable.end(); ++it) {
                            if (it->hostId == recvNeighborInfo.hostId) {
                                isExist = true;
                                break;
                            }
                        }
                        // 如果不存在，则加入邻接信息表
                        // 每次获取新的邻接信息，都尝试计算最短路径
                        // 并传递给其他节点
                        if (!isExist) {
                            neighborTable.push_back(recvNeighborInfo);
                            calcDijkstra();
                            sendNeighborAwarePacket(&recvNeighborInfo, incomingInterface);
                        }
                    }
//                    throw cRuntimeError("OK!");
                    delete packet;
                }
                else {
                    broadcastPacket(packet, destinationAddress, incomingInterface);
                }
            }
        }
    }
    numProcessedFrames++;
    updateDisplayString();
}

// 与实验不相干
void DijkstraRelayInterfaceSelector::broadcastPacket(Packet *outgoingPacket, const MacAddress& destinationAddress, NetworkInterface *incomingInterface)
{
    if (incomingInterface == nullptr)
        EV_INFO << "Broadcasting packet to all interfaces" << EV_FIELD(destinationAddress) << EV_FIELD(outgoingPacket) << EV_ENDL;
    else
        EV_INFO << "Broadcasting packet to all interfaces except incoming interface" << EV_FIELD(destinationAddress) << EV_FIELD(incomingInterface) << EV_FIELD(outgoingPacket) << EV_ENDL;
    for (int i = 0; i < interfaceTable->getNumInterfaces(); i++) {
        auto outgoingInterface = interfaceTable->getInterface(i);
        if (incomingInterface != outgoingInterface && isForwardingInterface(outgoingInterface))
            sendPacket(outgoingPacket->dup(), destinationAddress, outgoingInterface);
    }
    delete outgoingPacket;
}

// 与实验不相干
void DijkstraRelayInterfaceSelector::sendPacket(Packet *packet, const MacAddress& destinationAddress, NetworkInterface *outgoingInterface)
{
    EV_INFO << "Sending packet to peer" << EV_FIELD(destinationAddress) << EV_FIELD(outgoingInterface) << EV_FIELD(packet) << EV_ENDL;
    packet->addTagIfAbsent<DirectionTag>()->setDirection(DIRECTION_OUTBOUND);
    packet->addTagIfAbsent<InterfaceReq>()->setInterfaceId(outgoingInterface->getInterfaceId());
    if (auto outgoingInterfaceProtocol = outgoingInterface->getProtocol())
        ensureEncapsulationProtocolReq(packet, outgoingInterfaceProtocol, true, false);
    setDispatchProtocol(packet);
    pushOrSendPacket(packet, outputGate, consumer);
}

// 计算最短路径
void DijkstraRelayInterfaceSelector::calcDijkstra() {
    for (int k = 0; k < 500; ++k) { // 每次计算具有迭代上限，避免死循环
        NeighborInfo neighborInfo;
        // 判断是否含有下一个被访问的节点的邻接信息
        for (auto it = neighborTable.begin(); it != neighborTable.end(); ++it) {
            if (it->hostId == nextId) {
                // 如果当前节点的邻接信息已经被访问过，则跳过
                if (vis[nextId]) {
                    return;
                }
                // 如果当前节点的邻接信息没有被访问过，则将其标记为已访问
                vis[nextId] = true;
                neighborInfo = *it;
                break;
            }
        }
        // 访问当前节点的邻接信息
        if (vis[nextId]) {
            // 遍历当前节点的邻接信息
            int size = neighborInfo.connectedIds.size();
            for (int i = 0; i < size; ++i) {
                int id = neighborInfo.connectedIds[i];
                double delay = neighborInfo.connectedDelay[i];
                if (vis[id]) {
                    continue;
                }
                if (dis[id] > dis[nextId] + delay) {
                    dis[id] = dis[nextId] + delay;
                    father[id] = nextId;
                }
            }
            // 找出下一个节点
            int tmpId = nextId;
            double tmpDelay = 1e9;
            for (int i = 0; i < 500; ++i) {
                if (vis[i]) {
                    continue;
                }
                if (dis[i] < tmpDelay) {
                    tmpDelay = dis[i];
                    tmpId = i;
                }
            }
            nextId = tmpId;

            EV << "nextId: " << nextId << " dis: ";
            for (int i = 0; i < 20; ++i) {
                EV << "(" << i << "," << dis[i] << ") ";
            }
            EV << endl;
        }
        else {
            break;
        }
    }
}

// 查找数据包下一跳的输出接口
NetworkInterface* DijkstraRelayInterfaceSelector::getOutputInterface(const std::string& packetFullName) {
    // 根据数据包的完整名称解析出目标节点的索引
    size_t lastGtPos = packetFullName.rfind(">gs[");
    if (lastGtPos == std::string::npos) {
        throw cRuntimeError("Can not find >gs[ ");
        return nullptr;
    }

    size_t colonPos = packetFullName.find("]:", lastGtPos + 4);
    if (colonPos == std::string::npos) {
        throw cRuntimeError("Can not find ]: ");
        return nullptr;
    }

    std::string destIndexStr = packetFullName.substr(lastGtPos + 4, colonPos - (lastGtPos + 4));
    if (destIndexStr.empty()) {
        throw cRuntimeError("空字符串");
        return nullptr; // 空字符串直接返回失败
    }

    for (size_t i = 0; i < destIndexStr.size(); i++) {
        if (!std::isdigit(destIndexStr[i])) {
            EV_INFO << "destIndexStr is " << destIndexStr << endl;
            throw cRuntimeError("发现非数字字符");
            return nullptr; // 发现非数字字符
        }
    }
    // 获取目标节点的索引
    int destIndex = std::stol(destIndexStr);
    cModule *host = getParentModule()->getParentModule()->getParentModule();
    cModule *dest = host->getParentModule()->getSubmodule("gs", destIndex);
    // 获取目标节点对象
    int hostId = host->getId();
    int destId = dest->getId();
    EV_INFO << "destId: " << destId;
    int k = 0;
    // 路径回溯
    while (father[destId] != hostId) {
        ++k;
        if (k > 500) {  // 防止死循环
            throw cRuntimeError("Father not Found");
        }
        destId = father[destId];
        EV_INFO << " <- " << destId;
    }
    EV_INFO << endl;

    // 遍历自身接口，找出与下一跳节点相连的接口
    int ethgSize = host->gateSize("ethg");
    for (int i = 0; i < ethgSize; ++i) {
        cGate *ethg = host->gate("ethg$o", i);
        if (!ethg->isConnected())
            continue;
        cGate* remoteGate = ethg->getNextGate();
        if (remoteGate != nullptr) {
            cModule* remoteModule = remoteGate->getOwnerModule();
            int remoteId = remoteModule->getId();
            // 找到下一跳节点的ID
            if (destId == remoteId) {
                std::string name = host->getName();
                // 返回对应的输出接口
                if (name == "gs") {
                    // 地面站自带一个本地环回接口，其输出接口下标需要加1
                    return interfaceTable->getInterface(i + 1);
                }
                else {
                    return interfaceTable->getInterface(i);
                }
            }
        }
    }
    // 如果没有找到对应的输出接口，抛出异常
    throw cRuntimeError("Interface not Found");
    return nullptr;
}

} // namespace inet

