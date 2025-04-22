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

package OmnetExperiment.topologymanager;	// 声明数据包

module TopologyManager	// 定义模块
{
    parameters:
        int num_of_planes = default(1);				// 定义轨道数
    	int sta_per_plane = default(1);				// 定义轨道内卫星数
    	double width = default(360);				// 背景宽度
    	double height = default(180);				// 背景高度
    	double propagation_bitrate = default(1e9);	// 通道数据率
        @display("i=block/cogwheel");
        @class(OmnetExperiment::TopologyManager); 	// 绑定C++类
}