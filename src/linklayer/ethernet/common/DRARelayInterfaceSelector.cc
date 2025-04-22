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

#include "DRARelayInterfaceSelector.h"

#include "../../../queueing/queue/PacketQueueDisplayParser.h"
#include "inet/common/DirectionTag_m.h"
#include "inet/common/ProtocolUtils.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/common/VlanTag_m.h"
#include "inet/common/ProtocolGroup.h"
#include "inet/linklayer/ethernet/common/EthernetMacHeader_m.h"
#include "inet/linklayer/ieee8022/Ieee8022SnapHeader_m.h"
#include "inet/linklayer/ieee8022/Ieee8022LlcHeader_m.h"
#include "inet/linklayer/ethernet/common/Ethernet.h" // 包含 ETHERTYPE_SNAP 定义


namespace inet {

const double EARTH_RADIUS_KM = 6371.0; // 平均地球半径，单位为公里
const double PI = acos(-1);    // 圆周率

Define_Module(DRARelayInterfaceSelector);   // 使用Define_Module定义模块，这样ned文件才能找到对应的C++类

// 初始化函数
void DRARelayInterfaceSelector::initialize(int stage)
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
// 初始化处理逻辑
void DRARelayInterfaceSelector::handleMessage(cMessage* msg) {
    if (msg == startupMsg) {
        // 防止重复发送
        if (!isStartupPacketSent) {
            // 获取自身虚拟节点对象
            host = getParentModule()->getParentModule()->getParentModule();
            cModule *vn0 = host->getParentModule()->getSubmodule("vn", 0);
            std::string name = host->getName();
            // 获取自身经纬度
            selfLongitude = host->par("longitude").doubleValue();
            selfLatitude = host->par("latitude").doubleValue();
            if (name == "vn") {
                // 自身为虚拟节点
                // 获取自身节点ID
                selfId = host->getId() - vn0->getId();
                int sta_per_plane = host->getParentModule()->getSubmodule("manager")->par("sta_per_plane").intValue();
                // 获取自身所在轨道索引和自身在轨道内的索引
                selfN = selfId / sta_per_plane;
                selfM = selfId % sta_per_plane;
                // 获取自身接口对象
                for (int i = 0; i < 5; ++i) {
                    cGate *ethg = host->gate("ethg$o", i);
                    if (ethg->isConnected()) {
                        ethgInterface[i] = interfaceTable->getInterface(i);
                    }
                    else {
                        ethgInterface[i] = nullptr;
                    }
                }
            }
            else {
                // 自身为地面站
                selfId = host->getId() - vn0->getId();
                selfN = -1;
                selfM = -1;
                for (int i = 0; i < 5; ++i) {
                    ethgInterface[i] = nullptr;
                }
                ethgInterface[0] = interfaceTable->getInterface(1);
            }
            // 完成初始化
            isStartupPacketSent = true;
        }
        delete msg;
        startupMsg = nullptr;
        return;
    }
    // 原有消息处理保持不变
    PacketPusherBase::handleMessage(msg);
}

cGate *DRARelayInterfaceSelector::getRegistrationForwardingGate(cGate *gate)
{
    if (gate == outputGate)
        return inputGate;
    else if (gate == inputGate)
        return outputGate;
    else
        throw cRuntimeError("Unknown gate");
}

void DRARelayInterfaceSelector::pushPacket(Packet *packet, cGate *gates)
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
                    // 解析数据包
                    std::string packetFullName = packet->getFullName();
                    // 获取数据包下一跳的输出接口
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
                broadcastPacket(packet, destinationAddress, incomingInterface);
            }
        }
    }
    numProcessedFrames++;
    updateDisplayString();
}

// 与实验无关
void DRARelayInterfaceSelector::broadcastPacket(Packet *outgoingPacket, const MacAddress& destinationAddress, NetworkInterface *incomingInterface)
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

// 与实验无关
void DRARelayInterfaceSelector::sendPacket(Packet *packet, const MacAddress& destinationAddress, NetworkInterface *outgoingInterface)
{
    EV_INFO << "Sending packet to peer" << EV_FIELD(destinationAddress) << EV_FIELD(outgoingInterface) << EV_FIELD(packet) << EV_ENDL;
    packet->addTagIfAbsent<DirectionTag>()->setDirection(DIRECTION_OUTBOUND);
    packet->addTagIfAbsent<InterfaceReq>()->setInterfaceId(outgoingInterface->getInterfaceId());
    if (auto outgoingInterfaceProtocol = outgoingInterface->getProtocol())
        ensureEncapsulationProtocolReq(packet, outgoingInterfaceProtocol, true, false);
    setDispatchProtocol(packet);
    pushOrSendPacket(packet, outputGate, consumer);
}

// 根据数据包的完整名称获取源节点索引
int DRARelayInterfaceSelector::getSourceIndexByPacketFullName(const std::string& packetFullName) {
    size_t lastGtPos = packetFullName.find("gs[");
    if (lastGtPos == std::string::npos) {
        throw cRuntimeError("Can not find gs[ ");
        return -1;
    }

    size_t colonPos = packetFullName.find("]->", lastGtPos + 3);
    if (colonPos == std::string::npos) {
        throw cRuntimeError("Can not find ]->");
        return -1;
    }

    std::string sourceIndexStr = packetFullName.substr(lastGtPos + 3, colonPos - (lastGtPos + 3));
    if (sourceIndexStr.empty()) {
        throw cRuntimeError("空字符串");
        return -1; // 空字符串直接返回失败
    }

    for (size_t i = 0; i < sourceIndexStr.size(); i++) {
        if (!std::isdigit(sourceIndexStr[i])) {
            EV_INFO << "destIndexStr is " << sourceIndexStr << endl;
            throw cRuntimeError("发现非数字字符");
            return -1; // 发现非数字字符
        }
    }

    int sourceIndex = std::stol(sourceIndexStr);
    return sourceIndex;
}

// 根据数据包的完整名称获取目标节点索引
int DRARelayInterfaceSelector::getDestIndexByPacketFullName(const std::string& packetFullName) {
    size_t lastGtPos = packetFullName.rfind(">gs[");
    if (lastGtPos == std::string::npos) {
        throw cRuntimeError("Can not find >gs[ ");
        return -1;
    }

    size_t colonPos = packetFullName.find("]:", lastGtPos + 4);
    if (colonPos == std::string::npos) {
        throw cRuntimeError("Can not find ]: ");
        return -1;
    }

    std::string destIndexStr = packetFullName.substr(lastGtPos + 4, colonPos - (lastGtPos + 4));
    if (destIndexStr.empty()) {
        throw cRuntimeError("空字符串");
        return -1; // 空字符串直接返回失败
    }

    for (size_t i = 0; i < destIndexStr.size(); i++) {
        if (!std::isdigit(destIndexStr[i])) {
            EV_INFO << "destIndexStr is " << destIndexStr << endl;
            throw cRuntimeError("发现非数字字符");
            return -1; // 发现非数字字符
        }
    }

    int destIndex = std::stol(destIndexStr);
    return destIndex;
}

// 根据数据包的完整名称获取优先级
PriorityLevel DRARelayInterfaceSelector::getPriorityLevelByPacketFullName(const std::string& packetFullName) {
    size_t pos = packetFullName.find("TT");
    if (pos != std::string::npos) {
//        throw cRuntimeError("get TT");
        return TimeTriggered;
    }
    pos = packetFullName.find("BE");
    if (pos != std::string::npos) {
//        throw cRuntimeError("get BE");
        return BestEffort;
    }
//    throw cRuntimeError("get N");
    return Normal;
}

// 根据输出接口索引判断是否拥塞
bool DRARelayInterfaceSelector::isCongestedByOutputIndex(int outputIndex, PriorityLevel pri) {
    int packetCapacity = 0, numContainsPackets = 0;
    cGate *outputEthg = host->gate("ethg$o", outputIndex);
    // 输出接口存在
    if (outputEthg != nullptr) {
        if (outputEthg->isConnected()) {
            // 获取输出接口的缓存队列
            cModule *cqf_queue = host->getSubmodule("eth", outputIndex)->getSubmodule("macLayer")->getSubmodule("queue");
            int queueSize = cqf_queue->par("numBEQueues").intValue() + cqf_queue->par("num_CQF_Queues").intValue();
            // 判断数据包的优先级
            if (pri == TimeTriggered) {
                // 获取TT队列，TT流数据包读取当前负责接收的队列的缓存
                for (int i = 0; i < queueSize; ++i) {
                    cModule *transmissionGate = cqf_queue->getSubmodule("transmissionGate", i);
                    cDisplayString gateDisplayStr = transmissionGate->getDisplayString();
                    std::string gateText = gateDisplayStr.getTagArg("i", 1);
    //                throw cRuntimeError(gateText);
                    bool isOpen = gateText != "red";
                    // 判断是否为接收队列，关闭即为接收队列
                    if (!isOpen) {
    //                    throw cRuntimeError("is red");
                        cModule *q = cqf_queue->getSubmodule("queue", i);
                        cDisplayString packetDisplayStr = q->getDisplayString();
                        const char* packetText = packetDisplayStr.getTagArg("t", 0);
        //                throw cRuntimeError(rawText);
                        PacketQueueDisplayParser packetInfo(packetText);
                        // 缓存容量
                        packetCapacity = q->par("packetCapacity").intValue();
                        // 读取缓存
                        numContainsPackets = packetInfo.getPacketContains();
                        EV <<"packetCapacity: " << packetCapacity <<  " packetInfo: " << packetInfo.str() << endl;
                        break;
                    }
                }
            }
            else {
                // 获取BE队列，BE流数据包读取直接读取对应队列的缓存
                cModule *q = cqf_queue->getSubmodule("queue", queueSize - 1);
                cDisplayString packetDisplayStr = q->getDisplayString();
                const char* packetText = packetDisplayStr.getTagArg("t", 0);
//                throw cRuntimeError(rawText);
                PacketQueueDisplayParser packetInfo(packetText);
                // 缓存容量
                packetCapacity = q->par("packetCapacity").intValue();
                // 读取缓存
                numContainsPackets = packetInfo.getPacketContains();
                EV <<"packetCapacity: " << packetCapacity <<  " packetInfo: " << packetInfo.str() << endl;
            }
        }
        else {
            throw cRuntimeError("outputEthg is not connected");
            return true;
        }
    }
    else {
        throw cRuntimeError("outputEthg is nullptr");
        return true;
    }
    if (packetCapacity != 0) {
        // 判断是否拥塞
        if (numContainsPackets < congestedPacketRate * packetCapacity) {
            return false;
        }
        else {
//            throw cRuntimeError("too many numContainsPackets");
            return true;
        }
    }
    else {
        throw cRuntimeError("packetCapacity is 0");
        return true;
    }
}

// 根据数据包的完整名称获取输出接口
NetworkInterface* DRARelayInterfaceSelector::getOutputInterface(const std::string& packetFullName) {
    std::string selfName = host->getName();
    // 自身为地面站则直接输出
    if (selfName == "gs") {
        return ethgInterface[0];
    }
    // 获取源节点索引、目标节点索引和优先级
    int sourceIndex = getSourceIndexByPacketFullName(packetFullName);
    int destIndex = getDestIndexByPacketFullName(packetFullName);
    PriorityLevel level = getPriorityLevelByPacketFullName(packetFullName);

    cModule *gs = host->getParentModule()->getSubmodule("gs", destIndex);
    cGate *gsEthg0 = gs->gate("ethg$o", 0);
    if (!gsEthg0->isConnected()) {
        throw cRuntimeError("gsEthg0 is not isConnected");
        return nullptr;
    }

    cGate* remoteGate = gsEthg0->getNextGate();
    if (remoteGate == nullptr) {
        throw cRuntimeError("remoteGate is null");
        return nullptr;
    }

    cModule *vn0 = host->getParentModule()->getSubmodule("vn", 0);
    cModule* targetVN = remoteGate->getOwnerModule();
    // 获取目标节点Id
    int targetId = targetVN->getId() - vn0->getId();

    if (selfId == targetId) {
        return ethgInterface[4];
    }
    // 获取目标节点Id、所在轨道索引、在轨道内的索引和经纬度
    int N = host->getParentModule()->getSubmodule("manager")->par("num_of_planes").intValue();
    int M = host->getParentModule()->getSubmodule("manager")->par("sta_per_plane").intValue();
    int targetN = targetId / M;
    int targetM = targetId % M;
    double targetLongitude = targetVN->par("longitude").doubleValue();
    double targetLatitude = targetVN->par("latitude").doubleValue();

    // 1、方向估计
    int p_sc = selfN, s_sc = selfM, p_sn = targetN, s_sn = targetM;

    int ph_nv = 0, ph_nh = 0, pv_nv = 0, pv_nh = 0, nv = 0, nh = 0;

    int dv = 0, dh = 0;

    if (selfLongitude * targetLongitude >= 0) {
        EV << "DRA: on the same side of the seam" << endl;
        ph_nv = abs(s_sc - s_sn);
        ph_nh = abs(p_sc - p_sn);
        if (selfLongitude >= 0 && targetLongitude >= 0) {
            EV << "DRA: in the Eastern Hemisphere" << endl;
            pv_nv = std::min(s_sc + s_sn + 1, M - s_sc - s_sn - 1);
        }
        else {
            EV << "DRA: in the Western Hemisphere" << endl;
            pv_nv = std::min(2*M - s_sc - s_sn - 1, s_sc + s_sn + 1 - M);
        }
        pv_nh = N - abs(p_sc - p_sn);
        if (ph_nv + ph_nh <= pv_nv + pv_nh) {
            EV << "DRA: ph with minimum total hop number" << endl;
            nv = ph_nv;
            nh = ph_nh;
            if (p_sc < p_sn) {
                dh = 1;
            }
            else if (p_sc > p_sn) {
                dh = -1;
            }
            else {
                dh = 1;
            }
            if (s_sc < s_sn) {
                dv = -1;
            }
            else if (s_sc > s_sn) {
                dv = 1;
            }
            else {
                dv = 1;
            }
        }
        else {
            EV << "DRA: pv with minimum total hop number" << endl;
            nv = pv_nv;
            nh = pv_nh;
            if (selfLongitude >= 0 && targetLongitude > 0) {
                EV << "DRA: in the Eastern Hemisphere" << endl;
                if (p_sc < p_sn) {
                    dh = 1;
                }
                else if (p_sc >= p_sn) {
                    dh = -1;
                }
                if (2 * (s_sn + s_sc + 1) <= M) {
                    dv = 1;
                }
                else if (2 * (s_sn + s_sc + 1) > M) {
                    dv = -1;
                }
            }
            else {
                EV << "DRA: in the Western Hemisphere" << endl;
                if (p_sc < p_sn) {
                    dh = 1;
                }
                else if (p_sc >= p_sn) {
                    dh = -1;
                }
                if (2 * (s_sn + s_sc + 1) < 3 *M) {
                    dv = 1;
                }
                else if (2 * (s_sn + s_sc + 1) >= 3 * M) {
                    dv = -1;
                }
            }
        }
    }
    else {
        EV << "DRA: on different sides of the seam" << endl;
        ph_nv = abs(M - s_sn - 1 - s_sc);
        ph_nh = std::min(N + p_sc - p_sn, N + p_sn - p_sc);
        pv_nv = std::min(abs(s_sn - s_sc), M - abs(s_sn - s_sc));
        pv_nh = abs(p_sc - p_sn);
        if (ph_nv + ph_nh <= pv_nv + pv_nh) {
            EV << "DRA: ph with minimum total hop number" << endl;
            nv = ph_nv;
            nh = ph_nh;
            if (p_sc < p_sn) {
                dh = -1;
            }
            else if (p_sc > p_sn) {
                dh = 1;
            }
            if (M <= s_sn + s_sc + 1) {
                dv = 1;
            }
            else if (M > s_sn + s_sc + 1) {
                dv = -1;
            }
        }
        else {
            EV << "DRA: pv with minimum total hop number" << endl;
            nv = pv_nv;
            nh = pv_nh;
            if (selfLongitude > 0 && targetLongitude < 0) {
                EV << "DRA: self in the Eastern Hemisphere" << endl;
                if (p_sc < p_sn) {
                    dh = -1;
                }
                else if (p_sc > p_sn) {
                    dh = 1;
                }
                else {
                    dh = 1;
                }
                if (2 * (s_sn - s_sc) <= M) {
                    dv = -1;
                }
                else if (2 * (s_sn - s_sc) > M) {
                    dv = 1;
                }
            }
            else {
                EV << "DRA: target in the Eastern Hemisphere" << endl;
                if (p_sc < p_sn) {
                    dh = -1;
                }
                else if (p_sc > p_sn) {
                    dh = 1;
                }
                else {
                    dh = 1;
                }
                if (2 * (s_sn - s_sc) < M) {
                    dv = 1;
                }
                else if (2 * (s_sn - s_sc) >= M) {
                    dv = -1;
                }
            }
        }
    }

    EV <<"dv=" << dv << " dh=" << dh << " nv=" << nv << " nh=" << nh << endl;
    // 2、方向增强
    int pri = 0, sec = 0;

    if (abs(selfLatitude) > 75.0) {
        EV << "self is in palor, selfLatitude is " << selfLatitude << endl;
        if (dv != 0) {
            pri = 1;
            dh = 0;
        }
        else {
            if (selfLongitude * selfLatitude >= 0) {
                pri = 1;
                dv = -1;
                dh = 0;
            }
            else {
                pri = 1;
                dv = 1;
                dh = 0;
            }
        }
    }
    else if ((selfLatitude + 360.0 / M > 75.0 && selfLatitude - 360.0 / M < 75.0) || (selfLatitude - 360.0 / M < -75.0 && selfLatitude + 360.0 / M > -75.0)) {
        EV << "self is close to palor, selfLatitude is " << selfLatitude << endl;
        double R = EARTH_RADIUS_KM + host->par("altitude").doubleValue();
//        double R = 1.0;
        double Lv = R * sqrt(2 * (1 - cos(2 * PI / M)));
        double a = R * sqrt(2 * (1 - cos(2 * PI / (2 * N))));
        double Dv = double(pv_nh) * a * cos(abs(selfLatitude) * PI / 180.0) + double(pv_nv) * Lv;
        double Dh = double(ph_nh) * a * cos(abs(selfLatitude) * PI / 180.0) + double(ph_nv) * Lv;
        if (Dv < Dh) {
            pri = 1;
            sec = 2;
        }
        else {
            pri = 2;
            sec = 1;
        }
    }
    else {
        EV << "self is normal, selfLatitude is " << selfLatitude << endl;
        double R = EARTH_RADIUS_KM + host->par("altitude").doubleValue();
//        double R = 1.0;
        double Lv = R * sqrt(2 * (1 - cos(2 * PI / M)));
        double a = R * sqrt(2 * (1 - cos(2 * PI / (2 * N))));
        double Lh = a * cos(selfLatitude * PI / 180.0);
        int closestPolarId = 0, A = 0, k = 0;
        double lat_min = 0;
        for (int i = 0; i < M; ++i) {
            double lat = 360.0 / M * i + 180.0 / M;
            if (lat < 180.0) {
                lat = 90.0 - lat;
            }
            else {
                lat = lat - 270.0;
            }
            if (abs(lat) <= 75.0) {
                closestPolarId = i;
                lat_min = lat;
                break;
            }
        }
        if (selfM < M / 4) {
            A = selfM - closestPolarId;
        }
        else if (selfM >= M / 4 && selfM < M / 2) {
            A = M / 2 - closestPolarId - 1 - selfM;
        }
        else if (selfM >= M / 2 && selfM < 3 * M / 4) {
            A = selfM - M / 2 - closestPolarId;
        }
        else {
            A = M - 1 - selfM - closestPolarId ;
        }
        k = A + 1;
        EV << "R=" << R << " Lv=" << Lv << " a=" << a << " Lh=" << Lh << " closestPolarId=" << closestPolarId << " A=" << A << " k=" << k << " lat_min=" << lat_min << endl;
        bool isOK1 = true, isOK2 = true;
        for (int i = 0; i <= A; ++i) {
            double angle = selfLatitude + i * 360.0 / M;
            double Dv = double(pv_nh) * a * cos(abs(selfLatitude) * PI / 180.0) + Lv * (2.0 * k + 1.0);
            double Dha = double(ph_nh) * a * cos(abs(angle) * PI / 180.0) + 2.0 * i * Lv;
//            double value = (N * cos(lat_min) + Lv / a * (2 * (k - i) + 1)) / (cos(selfLatitude + i * 360.0 / M) + cos(lat_min));
            if (Dv >= Dha) {
                EV << "i=" << i << " angle=" << angle << " Dv=" << Dv << " Dha=" << Dha << endl;
                isOK1 = false;
                break;
            }
        }
        if (isOK1) {
            pri = 1;
            if (dh != 0) {
                sec = 2;
            }
        }
        else {
            for (int i = 1; i <= A; ++i) {
                double angle = selfLatitude + i * 360.0 / M;
                double Dh = double(ph_nh) * a * cos(abs(selfLatitude) * PI / 180.0);
                double Dha = double(ph_nh) * a * cos(abs(angle) * PI / 180.0) + 2.0 * i * Lv;
//                double value = 2 * i * Lv / (a * (cos(selfLatitude) - cos(selfLatitude + i * 360.0 / M)));
                if (Dh >= Dha) {
                    EV << "i=" << i << " angle=" << angle << " Dh=" << Dh << " Dha=" << Dha << endl;
                    isOK2 = false;
                    break;
                }
            }
            if (!isOK2) {
                if ((selfM < M / 4) || (selfM >= M / 2 && selfM < 3 * M / 4)) {
                    dv = 1;
                    pri = 1;
                    sec = 2;
                }
                else if ((selfM >= M / 4 && selfM < M / 2) || (selfM >= 3 * M / 4 && selfM < M)) {
                    dv = -1;
                    pri = 1;
                    sec = 2;
                }
            }
            else {
                if (abs(selfLatitude) >= abs(targetLatitude)) {
                    pri = 2;
                    sec = 1;
                }
                else {
                    pri = 1;
                    sec = 2;
                }
                if (dh == 0) {
                    pri = 1;
                    sec = 0;
                }
            }
        }

        EV << "isOK1=" << isOK1 << " isOK2=" << isOK2 << endl;
    }
    if (dv == 0) {
        if (pri == 1) {
            if (dh != 0) {
                pri = 2;
                sec = 0;
            }
            else {
                pri = 0;
                sec = 0;
            }
        }
    }
    if (dh == 0) {
        if (pri == 2) {
            if (dv != 0) {
                pri = 1;
                sec = 0;
            }
            else {
                pri = 0;
                sec = 0;
            }
        }
    }
    EV << "pri=" << pri << " sec=" << sec << " dv=" << dv << " dh=" << dh << endl;
    if (pri != 0 && sec == 0) {
        if (pri == 1) {
            if (dv == 1) {
                return interfaceTable->getInterface(0);
            }
            else if (dv == -1) {
                return interfaceTable->getInterface(2);
            }
        }
        else if (pri == 2) {
            if (dh == 1) {
                return interfaceTable->getInterface(1);
            }
            else if (dh == -1) {
                return interfaceTable->getInterface(3);
            }
        }
    }

    int priEthgIndex = -1, secEthgIndex = -1;
    if (pri == 1) {
        if (dv == 1) {
            priEthgIndex = 0;
        }
        else if (dv == -1) {
            priEthgIndex = 2;
        }
    }
    else if (pri == 2) {
        if (dh == 1) {
            priEthgIndex = 1;
        }
        else if (dh == -1) {
            priEthgIndex = 3;
        }
    }
    if (sec == 1) {
        if (dv == 1) {
            secEthgIndex = 0;
        }
        else if (dv == -1) {
            secEthgIndex = 2;
        }
    }
    else if (sec == 2) {
        if (dh == 1) {
            secEthgIndex = 1;
        }
        else if (dh == -1) {
            secEthgIndex = 3;
        }
    }
    EV << "priEthgIndex=" << priEthgIndex << " secEthgIndex=" << secEthgIndex << endl;
    if (priEthgIndex == -1 && secEthgIndex == -1) {
        throw cRuntimeError("priEthgIndex == -1 && secEthgIndex == -1");
    }

    // 3、拥塞避免
    if (priEthgIndex != -1) {
        if (!isCongestedByOutputIndex(priEthgIndex, level)) {
            return interfaceTable->getInterface(priEthgIndex);
        }
    }
    if (secEthgIndex != -1) {
        if (!isCongestedByOutputIndex(secEthgIndex, level)) {
            return interfaceTable->getInterface(secEthgIndex);
        }
    }
    if (priEthgIndex != -1) {
        return interfaceTable->getInterface(priEthgIndex);
    }
    if (secEthgIndex != -1) {
        return interfaceTable->getInterface(secEthgIndex);
    }

    throw cRuntimeError("Interface not Found");
    return nullptr;
}

} // namespace inet

