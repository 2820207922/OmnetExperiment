#!/user/bin/env python3
# -*- coding: utf-8 -*-

import copy
import pandas as pd
import numpy as np
import matplotlib
import matplotlib.pyplot as plt

print(matplotlib.matplotlib_fname())
font_name = "simhei"
matplotlib.rcParams['font.family']= font_name # 指定字体，实际上相当于修改 matplotlibrc 文件　只不过这样做是暂时的　下次失效
matplotlib.rcParams['axes.unicode_minus']=False # 正确显示负号，防止变成方框

RESULTS_DIR = "/home/user/omnetpp-6.0.2/workspace/MinHopCountAlgo/simulations/VN/results/"

style_config = {
    "Dijkstra": {
        "color": "#cc0000", 
        "alpha": 0.75,
        "hatch": "///", 
        "marker": "o", 
        "linestyle": ":"
    },
    "DRA": {
        "color": "#0000cc", 
        "alpha": 0.75,
        "hatch": "..", 
        "marker": "s", 
        "linestyle": "-."
    },
    "FBCCM": {
        "color": "#00aa00", 
        "alpha": 0.75,
        "hatch": "xx", 
        "marker": "^", 
        "linestyle": "-"
    }
}

# style_config = {
#     "Dijkstra": {"color": "#ff404f", "hatch": "///"},
#     "DRA": {"color": "#1f77b4", "hatch": ".."},
#     "FBCCM": {"color": "#2ca02c", "hatch": "xx"}
# }

def getFlowDatas(dir, prefix, startTime=0.25, endTime=0.5):
    flow_datas = {
        "num_sent_datas": 0,
        "num_recv_datas": 0,
        "arrvialTime": [],
        "creationTime": [],
        "meanBitLifeTimePerPacket": [],
        "packetDelayDifferenceToMean": [],
        "packetDelayVariation": [],
        "packetJitter": [],
        "sourceData": [],
    }

    names_source = [
        "arrvialTime", 
        "creationTime",
    ]
    names_sink = [
        "arrvialTime1",
        "creationTime",

        "arrvialTime2",
        "meanBitLifeTimePerPacket",

        "arrvialTime3",
        "packetDelayDifferenceToMean",

        "arrvialTime4",
        "packetDelayVariation",

        "arrvialTime5",
        "packetJitter",
    ]

    path_flow_source = dir + prefix + "source.csv" 
    list_flow_source = pd.read_csv(path_flow_source, header=0, names=names_source).to_dict(orient='list')
    # print(list_flow_source["arrvialTime"])

    path_flow_sink = dir + prefix + "sink.csv" 
    list_flow_sink = pd.read_csv(path_flow_sink, header=0, names=names_sink).to_dict(orient='list')
    # print(list_flow_sink["arrvialTime"])

    num_recv_datas = 0
    minCreationTime = 0.5
    maxCreationTime = 0.25
    for i in range(0, len(list_flow_sink["creationTime"])):

        arrvialTime = list_flow_sink["arrvialTime1"][i]
        if arrvialTime < startTime or arrvialTime > endTime:
            continue
        num_recv_datas += 1
        flow_datas["arrvialTime"].append(arrvialTime - startTime)

        creationTime = list_flow_sink["creationTime"][i]
        flow_datas["creationTime"].append(creationTime - startTime)
        if minCreationTime > creationTime:
            minCreationTime = creationTime
        if maxCreationTime < creationTime:
            maxCreationTime = creationTime
            

        arrvialTime = list_flow_sink["arrvialTime1"][i]
        flow_datas["arrvialTime"].append(arrvialTime - startTime)

        meanBitLifeTimePerPacket = list_flow_sink["meanBitLifeTimePerPacket"][i]
        flow_datas["meanBitLifeTimePerPacket"].append(meanBitLifeTimePerPacket)

        packetDelayDifferenceToMean = list_flow_sink["packetDelayDifferenceToMean"][i]
        flow_datas["packetDelayDifferenceToMean"].append(packetDelayDifferenceToMean)

        packetDelayVariation = list_flow_sink["packetDelayVariation"][i]
        flow_datas["packetDelayVariation"].append(packetDelayVariation)

        packetJitter = list_flow_sink["packetJitter"][i]
        flow_datas["packetJitter"].append(packetJitter)

    num_sent_datas = 0

    for i in range(0, len(list_flow_source["creationTime"])):
        creationTime = list_flow_source["creationTime"][i]
        if creationTime < minCreationTime or creationTime > maxCreationTime:
            continue
        num_sent_datas += 1

    flow_datas["sourceData"] = list_flow_source["creationTime"]
    flow_datas["num_sent_datas"] = num_sent_datas
    flow_datas["num_recv_datas"] = num_recv_datas
    print(f"{prefix}: num_sent_datas={num_sent_datas}, num_recv_datas={num_recv_datas}, droppedRate={100.0 * (num_sent_datas - num_recv_datas) / num_sent_datas}%")

    return flow_datas

def getAllFlowDatas(res_dir, algo_name, prefixs):
    algo_dir = res_dir + algo_name + "/"

    all_flow_datas = {}
    

    print(algo_name, ":")

    for prefix in prefixs:
        all_flow_datas[prefix] = getFlowDatas(algo_dir, prefix)

    return all_flow_datas

def drawRecvRate(datas_Dijkstra, datas_DRA, datas_FBCCM, prefixs, pic_name):
    """
    按时间进程绘制三种算法的到达率折线图
    """
    algo_name = ["Dijkstra", "DRA", "FBCCM"]
    num_time_points = 5  # 每种算法取5个时间点
    
    # 定义时间点标签
    time_labels = [f"{(i+1)*50}" for i in range(num_time_points)]
    x_positions = np.arange(num_time_points)
    
    plt.figure(figsize=(14, 7))
    plt.subplots_adjust(right=0.75)

    # 显式收集图例句柄和标签
    legend_handles = []
    legend_labels = []

    for algo in algo_name:
        # 获取对应算法的数据字典
        if algo == "Dijkstra":
            data_dict = datas_Dijkstra
        elif algo == "DRA":
            data_dict = datas_DRA
        else:
            data_dict = datas_FBCCM
            
        # 准备累积数据点
        time_series_data = []
        
        # 按时间点累积计算
        for time_idx in range(num_time_points):
            # 确定当前时间点要处理的数据流数量
            prefixes_to_process = (time_idx + 1) / num_time_points
            
            # 累积计算到当前时间点的发送和接收总量
            total_recv = 0
            total_sent = 0
            for prefix in prefixs:
                total_recv += int(data_dict[prefix]["num_recv_datas"] * prefixes_to_process)
                minCreationTime = 0.25
                maxCreationTime = 0.0
                for creationTime in data_dict[prefix]["creationTime"][0:total_recv]:
                    if minCreationTime > creationTime:
                        minCreationTime = creationTime
                    if maxCreationTime < creationTime:
                        maxCreationTime = creationTime
                for i in range(0, len(data_dict[prefix]["sourceData"])):
                    creationTime = data_dict[prefix]["sourceData"][i] - 0.25
                    if creationTime < minCreationTime or creationTime > maxCreationTime:
                        continue
                    total_sent += 1
            
            # print(f"({total_recv}, {total_sent})")
            # 计算该时间点的累积抵达率
            rate = 100.0 * total_recv / total_sent if total_sent > 0 else 0
            time_series_data.append(rate)
        
        # 绘制折线图
        line, = plt.plot(
            x_positions, time_series_data,
            color=style_config[algo]["color"],
            alpha=style_config[algo]["alpha"],
            marker=style_config[algo]["marker"],
            markersize=15,
            linestyle=style_config[algo]["linestyle"],
            linewidth=2.5,
            label=algo
        )
        
        # 记录有效图例句柄
        legend_handles.append(line)
        legend_labels.append(algo)
        
        # 添加数据标签
        for xi, value in zip(x_positions, time_series_data):
            value_text = value + 1
            if algo == "Dijkstra":
                if "TT" in pic_name :
                    if xi == 0:
                        value_text += 12
                    if xi == 1:
                        value_text -= 10
                    else:
                        value_text -= 8
                else:
                    if xi == 0:
                        value_text += 1
                    else:
                        value_text -= 8

            if algo == "DRA":
                value_text -= 6
            if algo == "FBCCM":
                value_text += 1
            plt.text(
                xi, 
                value_text,
                f'{value:.2f}%', 
                ha='center', 
                va='bottom', 
                fontsize=20,
                color=style_config[algo]["color"],
                weight='bold',
                bbox=dict(facecolor='white', alpha=0.7, edgecolor='none', pad=1)
            )

    # 坐标轴设置
    plt.xlabel("时间 (ms)", fontsize=20)
    plt.ylabel("到达率 (%)", fontsize=20)
    plt.title(pic_name, fontsize=25)
    plt.xticks(x_positions, time_labels, fontsize=20)
    plt.xlim(-0.25, 4.25)
    # 自动计算适当的Y轴刻度范围
    y_values = [val for algo in algo_name for val in plt.gca().get_lines()[algo_name.index(algo)].get_ydata()]
    y_max = max(y_values) * 1.1 if y_values else 10
    y_min = min(y_values) * 0.8 if min(y_values) > 1 else -3
    y_tick = np.linspace(0, 100, 6)
    plt.yticks(y_tick, fontsize=20)
    plt.ylim(20, y_max)
    
    # 创建图例（使用显式句柄和标签）
    legend = plt.legend(
        legend_handles,
        legend_labels,
        title="算法",
        bbox_to_anchor=(1.02, 0.9),
        loc='upper left',
        borderaxespad=0.,
        frameon=True,
        fontsize=20,
        title_fontsize=20
    )
    
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.tight_layout()
    plt.savefig(RESULTS_DIR + f"{pic_name}.png", bbox_inches='tight')
    plt.close()

def drawDirect(datas_Dijkstra, datas_DRA, datas_FBCCM, prefix, param):
    x1 = datas_Dijkstra[prefix]["arrvialTime"]
    y1 = datas_Dijkstra[prefix][param]

    x2 = datas_DRA[prefix]["arrvialTime"]
    y2 = datas_DRA[prefix][param]

    x3 = datas_FBCCM[prefix]["arrvialTime"]
    y3 = datas_FBCCM[prefix][param]

    plt.figure(figsize=(40, 10), dpi=100)

    plt.plot(x1, y1, label='Dijkstra', marker='o', markersize=4, markevery=100, linestyle=None, alpha=0.5, color='red', linewidth=0.5)
    plt.plot(x2, y2, label='DRA', marker='d', markersize=4, markevery=1, linestyle=None, alpha=0.5, color='blue', linewidth=0.5)
    plt.plot(x3, y3, label='FBCCM', marker='^', markersize=4, markevery=1, linestyle=None, alpha=0.5, color='green', linewidth=0.5)

    plt.xlabel("arrvialTime/s")
    plt.ylabel(param + "/s")
    plt.title(prefix + param)

    plt.legend()
    plt.grid(True)
    # plt.show()
    plt.savefig(RESULTS_DIR + prefix + param)

def drawAveParam(datas_Dijkstra, datas_DRA, datas_FBCCM, prefixs, param, pic_name, y_name, absFlag=False):
    """
    按时间进程绘制三种算法的参数平均值折线图
    
    参数:
    datas_Dijkstra, datas_DRA, datas_FBCCM - 三种算法的数据字典
    prefixs - 数据流标识符列表
    param - 要分析的参数名
    pic_name - 图表标题
    y_name - Y轴标签名
    absFlag - 是否取绝对值
    """
    algo_name = ["Dijkstra", "DRA", "FBCCM"]
    num_time_points = 6  # 每种算法取5个时间点
    
    # 定义时间点标签
    time_labels = [f"{(i+1)*50}" for i in range(num_time_points)]
    x_positions = np.arange(num_time_points)
    
    plt.figure(figsize=(14, 7))
    plt.subplots_adjust(right=0.75)

    # 显式收集图例句柄和标签
    legend_handles = []
    legend_labels = []

    for algo in algo_name:
        # 获取对应算法的数据字典
        if algo == "Dijkstra":
            data_dict = datas_Dijkstra
        elif algo == "DRA":
            data_dict = datas_DRA
        else:
            data_dict = datas_FBCCM
            
        # 准备累积数据点
        time_series_data = []
        
        # 按时间点累积计算
        for time_idx in range(num_time_points):
            # 确定当前时间点要处理的数据流数量
            prefixes_to_process = (time_idx + 1) / num_time_points
            
            # 累积所有参数值用于计算平均值
            all_values = []
            for prefix in prefixs:
                try:
                    recv_count = int(data_dict[prefix]["num_recv_datas"] * prefixes_to_process)
                    values = data_dict[prefix][param][0:recv_count]
                    if absFlag:
                        values = np.abs(values)
                    all_values.extend(values)
                except (KeyError, TypeError):
                    pass
            # print(f"len(all_values):{len(all_values)}")
            # 计算平均值并转换为毫秒
            avg = np.mean(all_values) * 1000 if all_values else 0
            time_series_data.append(avg)
            # print(f"len(time_series_data):{len(time_series_data)}")
        
        # 绘制折线图
        line, = plt.plot(
            x_positions, time_series_data,
            color=style_config[algo]["color"],
            alpha=style_config[algo]["alpha"],
            marker=style_config[algo]["marker"],
            markersize=15,
            linestyle=style_config[algo]["linestyle"],
            linewidth=2.5,
            label=algo
        )
        
        # 记录有效图例句柄
        legend_handles.append(line)
        legend_labels.append(algo)
        
        # 添加数据标签
        for xi, value in zip(x_positions, time_series_data):
            if absFlag == False:
                value_text = value + xi / 8
                if algo == "Dijkstra":
                    value_text += (2 - xi) / 8
                if algo == "DRA":
                    value_text += (4 - xi) / 8
                if algo == "FBCCM":
                    value_text -= 1 + xi / 8
            if absFlag == True:
                value_text = value
                if algo == "Dijkstra":
                    value_text -= 0.5
                if algo == "DRA":
                    value_text += 0.15
                if algo == "FBCCM":
                    value_text += 0.15
                
            plt.text(
                xi, 
                value_text,
                f'{value:.4f}ms', 
                ha='center', 
                va='bottom', 
                fontsize=20,
                color=style_config[algo]["color"],
                weight='bold',
                bbox=dict(facecolor='white', alpha=0.7, edgecolor='none', pad=1)
            )

    # 坐标轴设置
    plt.xlabel("时间 (ms)", fontsize=20)
    plt.ylabel(f"{y_name} (ms)", fontsize=20)
    plt.title(pic_name, fontsize=25)
    plt.xticks(x_positions, time_labels, fontsize=20)
    plt.xlim(-0.35, 4.35)
    
    # 自动计算适当的Y轴刻度范围
    y_values = [val for algo in algo_name for val in plt.gca().get_lines()[algo_name.index(algo)].get_ydata()]
    y_max = max(y_values) * 1.05 if y_values else 10
    y_min = min(y_values) * 0.95 if min(y_values) > 1 else -1
    y_tick = np.linspace(y_min, y_max, 6)
    plt.yticks(y_tick, fontsize=20)
    plt.ylim(y_min + 0.1, y_max + 1)
    
    # 创建图例（使用显式句柄和标签）
    legend = plt.legend(
        legend_handles,
        legend_labels,
        title="算法",
        bbox_to_anchor=(1.02, 0.9),
        loc='upper left',
        borderaxespad=0.,
        frameon=True,
        fontsize=20,
        title_fontsize=20
    )
    
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.tight_layout()
    plt.savefig(RESULTS_DIR + f"{pic_name}.png", bbox_inches='tight')
    plt.close()


if __name__ == "__main__": 
    prefixs_TT = [
        "flow_0_1_",
        "flow_2_3_",
        "flow_5_4_",
        "flow_4_6_",
    ]

    prefixs_BE = [
    "flow_6_3_",
    "flow_9_7_",
    "flow_10_8_",
    "flow_12_11_",
    ]

    datas_Dijkstra_TT = getAllFlowDatas(RESULTS_DIR, "Dijkstra/TT", prefixs_TT)
    datas_Dijkstra_BE = getAllFlowDatas(RESULTS_DIR, "Dijkstra/BE", prefixs_BE)
    datas_DRA_TT = getAllFlowDatas(RESULTS_DIR, "DRA/TT", prefixs_TT)
    datas_DRA_BE = getAllFlowDatas(RESULTS_DIR, "DRA/BE", prefixs_BE)
    datas_FBCCM_TT = getAllFlowDatas(RESULTS_DIR, "FBCCM/TT", prefixs_TT)
    datas_FBCCM_BE = getAllFlowDatas(RESULTS_DIR, "FBCCM/BE", prefixs_BE)

    # print(datas_DRA["flow_0_1_"]["meanBitLifeTimePerPacket"])

    params = [
        "meanBitLifeTimePerPacket",
        "packetDelayDifferenceToMean",
        "packetDelayVariation",
        "packetJitter",
    ]

    # for prefix in prefixs_TT:
    #     for param in params:
    #         drawDirect(datas_Dijkstra, datas_DRA_TT, datas_FBCCM_TT, prefix, param)


    drawRecvRate(datas_Dijkstra_TT, datas_DRA_TT, datas_FBCCM_TT, prefixs_TT, "TT流不同方案下数据包到达率性能对比")
    drawAveParam(datas_Dijkstra_TT, datas_DRA_TT, datas_FBCCM_TT, prefixs_TT, "meanBitLifeTimePerPacket", "TT流不同方案下数据包平均延迟性能对比", "平均延迟")
    drawAveParam(datas_Dijkstra_TT, datas_DRA_TT, datas_FBCCM_TT, prefixs_TT, "packetJitter", "TT流不同方案下数据包平均延迟抖动性能对比", "平均延迟抖动", True)

    drawRecvRate(datas_Dijkstra_BE, datas_DRA_BE, datas_FBCCM_BE, prefixs_BE, "BE流不同方案下数据包到达率性能对比")
    drawAveParam(datas_Dijkstra_BE, datas_DRA_BE, datas_FBCCM_BE, prefixs_BE, "meanBitLifeTimePerPacket", "BE流不同方案下数据包平均延迟性能对比", "平均延迟")
    drawAveParam(datas_Dijkstra_BE, datas_DRA_BE, datas_FBCCM_BE, prefixs_BE, "packetJitter", "BE流不同方案下数据包平均延迟抖动性能对比", "平均延迟抖动", True)
    
