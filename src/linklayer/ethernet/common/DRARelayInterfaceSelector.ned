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

package OmnetExperiment.linklayer.ethernet.common;

import inet.queueing.base.PacketPusherBase;
import inet.queueing.contract.IPacketFlow;

simple DRARelayInterfaceSelector extends PacketPusherBase like IPacketFlow
{
    parameters:
        string macTableModule; 							// mac表，无用
        string interfaceTableModule; 					// 接口表，用于查询接口信息，包括输出缓存
        @class(inet::DRARelayInterfaceSelector);   // 绑定C++类
        @display("i=block/fork");
        @signal[packetDropped](type=inet::Packet);  	// 信号声明，OMNeT++通过该标识统计数据包信息
        // 声明需要记录的数据包信息，包括包数量、包大小等
        @statistic[packetDropped](title="packets dropped"; record=count,sum(packetBytes),vector(packetBytes); interpolationmode=none);  
}
