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

package OmnetExperiment.simulations.VN; // 声明数据包
import inet.networks.base.TsnNetworkBase;   // 导入已有网络模块
import OmnetExperiment.virtualnode.VirtualNode;         // 导入虚拟节点模块
import OmnetExperiment.groundstation.GroundStation;     // 导入地面站模块
import OmnetExperiment.topologymanager.TopologyManager; // 导入拓扑管理器
network Demo extends TsnNetworkBase // 继承已有网络模块
{
    parameters:
        int numOfVN = default(1); // 卫星数量
        int numOfGS = default(1); // 地面站数量
        @display("bgi=background_earth,s;bgb=2160,1080");   // 显示世界地图，并设置宽高
    submodules:
        // 定义网络拓扑管理器
        manager: TopologyManager {  
            parameters:
                @display("p=50,50");
        }
        // 定义卫星节点及数量，一星四链结构，预留一接口连接地面站，总计5个接口
        vn[numOfVN]: VirtualNode {  
            gates:
                ethg[5];
        }
        // 定义地面站及数量，预留一接口连接就近卫星
        gs[numOfGS]: GroundStation {
            gates:
                ethg[1];
        }
    connections allowunconnected:   // 允许接口不连接
}
