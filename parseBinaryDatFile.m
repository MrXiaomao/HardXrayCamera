clear;
close all;

%% TITLE General_Wave_Plot
%适用场合：HXR示波器数据解析
%更新日期＿2025.09.15
%编写：沈晖寅 1305188374@qq.com
%功能：①观测一定数量的波形

%% step1 参数设置[采样点数：分析波形个数]
sample_deepth=580;  %56 每个波形的采样深度（256个采样点）
data_start=3;  % 波形数据在每一帧的第3个16位整数开始（跳过帧头）
win_10us=625;
win_100us=6250;
%% step2 读取文件并绘刿
[FileName1,PathName1]=uigetfile('*.dat','OpenFile'); % 弹窗选择要解析的.dat文件
FileName_1=[PathName1,FileName1];
fid = fopen(FileName_1, 'r','b');  % 以二进制只读模式打开文件
rawData = fread(fid, Inf, 'uint8'); % 读取所有数据，按8位无符号整数解析
fclose(fid);

%% 数据筛鿉，筛鿉掉异常数据匿
% 筛鿉规则：棿索包头包尿
   rawData = uint8(rawData);
n = length(rawData);
% 向量化查找所有包头（170=0xAA, 187=0xBB）和包尾（204=0xCC, 221=0xDD）
headerPositions = find(rawData(1:end-1) == 170 & rawData(2:end) == 187);%包头的数量
footerPositions = find(rawData(1:end-1) == 204 & rawData(2:end) == 221);%包尾的数量

% 匹配包头和包尾，提取完整数据包
maxPossiblePackets = min(length(headerPositions), length(footerPositions));
extractedPackets = cell(maxPossiblePackets, 1);%分成单独通道数的包
packetCount = 0;
for i = 1:length(headerPositions)
    headerIdx = headerPositions(i);
    nextFooter = footerPositions(find(footerPositions > headerIdx, 1)); % 找当前包头后的第一个包尾
    if ~isempty(nextFooter)
        packetEnd = nextFooter + 1;
        packetCount = packetCount + 1;
        extractedPackets{packetCount} = rawData(headerIdx:packetEnd); % 提取完整数据包
    end


end
extractedPackets = extractedPackets(1:packetCount); % 只保留有效数据包


if packetCount > 0
    maxValues = 0;
    for i = 1:packetCount
        maxValues = max(maxValues,floor(length(extractedPackets{i}) / 2));
    end
    combinedValue = zeros(packetCount, maxValues, 'uint16'); % 预分配16位数据矩阵
    
    for i = 1:packetCount
        packetData = extractedPackets{i};
        packetLength = length(packetData);
        numValues = floor(packetLength / 2);
        % 提取高/低字节，拼接为16位整数（高字节左移8位 + 低字节）
        highBytes = uint16(packetData(1:2:2*numValues));
        lowBytes = uint16(packetData(2:2:2*numValues));
        combinedValue(i, 1:numValues) = bitor(bitshift(highBytes, 8), lowBytes);
    end
else
    combinedValue = [];
end
combinedValue=combinedValue'; % 转置为“采样点×数据包数”的矩阵
%% 基于 combinedValue 第3行检测丢包
seq = double(combinedValue(3,:));   % 第3行数据

lost_idx = [];

for i = 1:length(seq)-16

    diff_val = seq(i+16) - seq(i);

    % 正常情况下应该等于1
    if diff_val ~= 1

        lost_idx(end+1) = i;

        fprintf('疑似丢包: index=%d -> %d, value=%d -> %d, diff=%d\n', ...
            i, i+16, seq(i), seq(i+16), diff_val);

    end
end

fprintf('检测完成，异常点数量: %d\n', length(lost_idx));

    %WAVE=combinedValue;
    %WAVE(1:3,:)=[];

    %去掉长度不满足要求的数据匿   1028     260    68    516
    %data = combinedValue;
% 过滤掉长度不符合要求的数据包（包尾标识为52445=0xCCCD，用于校验）
data = combinedValue(:, combinedValue(sample_deepth+4, :) == 52445);
data(sample_deepth+4:end,:)=[]; % 去掉包尾数据
data(1,:)=[]; % 去掉包头数据

    % uniqueValues2 = unique(data(1,:));
    % uniqueValues1 = unique(combinedValue(2,:));
    % counts1 = histcounts(combinedValue(2,:), [uniqueValues1, max(uniqueValues1)+1]); % 注意bins的边界设罿
    % counts2 = histcounts(data(1,:), [uniqueValues2, max(uniqueValues2)+1]); % 注意bins的边界设罿
    % disp(tab)
%% 解析各鿚道数据
wave_cnt=size(data,2); % 有效数据包总数
CH1_CNT=1;
CH2_CNT=1;
CH3_CNT=1;
CH4_CNT=1;
CH5_CNT=1;
CH6_CNT=1;
CH7_CNT=1;
CH8_CNT=1;
CH9_CNT=1;
CH10_CNT=1;
CH11_CNT=1;
CH12_CNT=1;
CH13_CNT=1;
CH14_CNT=1;
CH15_CNT=1;% 初始化各通道计数器
CH16_CNT=1;
sample_windows=50;
%i=1
for i=1:wave_cnt
    X=data(:,i);
    data_part= uint64(X);
    %data = swapbytes(Y);
    CH_NUM(:,i) = data_part(1);
    %wave(:,1)= data(data_start:end-1);
    Time(:,i) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;  %单位0.5ms
    switch data_part(1)
        case 1
            Wave_CH1(:,CH1_CNT)=data_part(data_start:end);% 提取该通道的波形数据（从第3个点开始）
           % base_CH1(CH1_CNT)=mean(Wave_CH1(150:200,CH1_CNT)); % 计算基线（150-200点的平均值）
            base_CH1(CH1_CNT)=mean(Wave_CH1(20:40,CH1_CNT)); % 计算基线（150-200点的平均值）
            Peak_CH1(CH1_CNT)=max(Wave_CH1(:,CH1_CNT));% 计算峰值
            Time_PACK1(:,CH1_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;% 有效峰值（峰值-基线）
                    REAL_peak_CH1(CH1_CNT)=Peak_CH1(CH1_CNT)-base_CH1(CH1_CNT);
            CH1_CNT=CH1_CNT+1;
        case 2
            Wave_CH2(:,CH2_CNT)=data_part(data_start:end);
            Time_PACK2(:,CH2_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
            Peak_CH2(CH2_CNT)=max(Wave_CH2(:,CH2_CNT));
            CH2_CNT=CH2_CNT+1;
        case 4
            Wave_CH3(:,CH3_CNT)=data_part(data_start:end);
            Time_PACK3(:,CH3_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
            Peak_CH3(CH3_CNT)=max(Wave_CH3(:,CH3_CNT));
            CH3_CNT=CH3_CNT+1;
        case 8
            Wave_CH4(:,CH4_CNT)=data_part(data_start:end);
            Time_PACK4(:,CH4_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
            Peak_CH4(CH4_CNT)=max(Wave_CH4(:,CH4_CNT));
            CH4_CNT=CH4_CNT+1;
        case 16
            Wave_CH5(:,CH5_CNT)=data_part(data_start:end);
            Time_PACK5(:,CH5_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
            Peak_CH5(CH5_CNT)=max(Wave_CH5(:,CH5_CNT));
            CH5_CNT=CH5_CNT+1;
        case 512
            Wave_CH10(:,CH10_CNT)=data_part(data_start:end);
            Time_PACK10(:,CH10_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
            Peak_CH10(CH10_CNT)=max(Wave_CH10(:,CH10_CNT));
            CH10_CNT=CH10_CNT+1;
    end
end

%% 时间戳差分，丢包情况评估
time_inter=diff(Time);plot(time_inter);hold on;
time_inter1=diff(Time_PACK1);plot(time_inter1);hold on;
time_inter2=diff(Time_PACK2);plot(time_inter2);hold on;
time_inter3=diff(Time_PACK3);plot(time_inter3);hold on;