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

package OmnetExperiment.groundstation;	// 声明数据包，供其他ned文件使用

import inet.node.tsn.TsnDevice;			// 导入已有数据包
import inet.linklayer.contract.IMacForwardingTable;	// 可忽略

simple ForwardingEntry {	// 可忽略
    string address;
    string interface;
    int vlan;
}

module GroundStation extends TsnDevice		// 继承交换机设备，在此基础上新增参数
{
    parameters:
        @display("i=device/receiverdish");				// 显示为信关站
        double longitude = default(0deg) @unit(deg);	// 定义经度
        double latitude = default(0deg) @unit(deg);		// 定义纬度
        double altitude = default(0km) @unit(km);		// 定义海拔
        bridging.typename = default("BridgingLayer"); 	// 用于后续部署算法
        bridging.interfaceRelay.typename = default("GSInterfaceRelayLayer");
        **.macTableModule = default(absPath(".macTable"));	// 可忽略
        
   	submodules:
        macTable: <default("MacForwardingTable")> like IMacForwardingTable {
            @display("p=100,100;is=s");	// 可忽略
        }
}