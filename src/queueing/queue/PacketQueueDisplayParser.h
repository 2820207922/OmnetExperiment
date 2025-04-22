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

#ifndef QUEUEING_QUEUE_PACKETQUEUEDISPLAYPARSER_H_
#define QUEUEING_QUEUE_PACKETQUEUEDISPLAYPARSER_H_

#include <string>       // 添加
#include <regex>        // 添加
#include <stdexcept>
#include <sstream>

namespace inet {
namespace queueing {

class PacketQueueDisplayParser {
private:
    int packetContains = 0;       // contains %p (X pk)
    int totalBytes = 0;        // (Y B)
    int pushedPackets = 0;     // pushed %u
    int pulledPackets = 0;     // pulled %o
    int removedPackets = 0;    // removed %r
    int droppedPackets = 0;    // dropped %d

public:
    /**
     * 构造函数：直接解析输入字符串
     */
    explicit PacketQueueDisplayParser(const std::string& displayStr) {
        parse(displayStr);
    }

    /**
     * 解析字符串，并填充字段值
     */
    void parse(const std::string& displayStr) {
        // 正则表达式匹配所有数值字段
        std::regex pattern(
            R"(contains\s+(\d+)\s+pk\s+\((\d+)\s+B\)\s+pushed\s+(\d+)\s+pulled\s+(\d+)\s+removed\s+(\d+)\s+dropped\s+(\d+))"
        );

        std::smatch matches;
        if (std::regex_search(displayStr, matches, pattern) && matches.size() == 7) {
            packetContains  = std::stoi(matches[1]);
            totalBytes      = std::stoi(matches[2]);
            pushedPackets   = std::stoi(matches[3]);
            pulledPackets   = std::stoi(matches[4]);
            removedPackets  = std::stoi(matches[5]);
            droppedPackets  = std::stoi(matches[6]);
        } else {
            throw std::invalid_argument("Invalid display string format");
        }
    }

    // Getter 方法
    int getPacketContains() const  { return packetContains; }
    int getTotalBytes() const     { return totalBytes; }
    int getPushedPackets() const  { return pushedPackets; }
    int getPulledPackets() const  { return pulledPackets; }
    int getRemovedPackets() const { return removedPackets; }
    int getDroppedPackets() const { return droppedPackets; }

    /**
     * 调试用：将解析结果转为字符串
     */
    std::string str() const {
        std::ostringstream oss;
        oss << "contains " << packetContains << " pk (" << totalBytes << " B) "
            << "pushed " << pushedPackets << " pulled " << pulledPackets << " "
            << "removed " << removedPackets << " dropped " << droppedPackets;
        return oss.str();
    }
};

}
}

#endif /* QUEUEING_QUEUE_PACKETQUEUEDISPLAYPARSER_H_ */
