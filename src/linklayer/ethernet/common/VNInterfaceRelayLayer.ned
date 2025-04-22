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
import inet.protocolelement.contract.IProtocolLayer;
import inet.queueing.contract.IPacketFlow;
module VNInterfaceRelayLayer like IProtocolLayer
{
    parameters:
        string macTableModule;
        string interfaceTableModule;
        *.macTableModule = default(absPath(this.macTableModule));
        *.interfaceTableModule = default(absPath(this.interfaceTableModule));
        @display("i=block/layer");
    gates:
        input upperLayerIn;
        output upperLayerOut;
        input lowerLayerIn;
        output lowerLayerOut;
    submodules:
        learner: <default("RelayInterfaceLearner")> like IPacketFlow {
            @display("p=200,150");
        }
        selector: <default("RelayInterfaceSelector")> like IPacketFlow {    // 暂时保持默认，算法的选择在ini文件中配置
            @display("p=500,150");
        }
    connections:
        upperLayerIn --> { @display("m=n"); } --> selector.in;
        selector.out --> { @display("m=s"); } --> lowerLayerOut;
        lowerLayerIn --> { @display("m=s"); } --> learner.in;
        learner.out --> { @display("m=n"); } --> upperLayerOut;
}
