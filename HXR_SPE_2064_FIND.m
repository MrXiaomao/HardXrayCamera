clear all;
% close all;

%% TITLE Energy_Spectrum_Plot
%适用场合：能谱数据解析（基于原示波器代码修改）
%更新日期＿2026.01.30
%功能：①解析能谱数据包 ②16通道能谱数据叠加汇总

%% step1 参数设置[道址数：分析能谱小包个数]
sample_deepth=512;  % 每个能谱小包的道址数（替换原256采样点）
data_start=3;       % 能谱数据在每一帧的第4个32位整数开始（跳过包头/时刻/通道号）
packet_length = (sample_deepth+4)*4; 
test_time =10; %s

%% step2 读取文件并解析
initialPath = 'C:\Users\12074\Desktop\QT_software\build_HardXrayCamera\x64\数据保存路径\'; % 替换为你的起始路径
[FileName1,PathName1]=uigetfile('*.dat','OpenFile',initialPath); % 弹窗选择要解析的.dat文件
FileName_1=[PathName1,FileName1];
fid = fopen(FileName_1, 'r','b');  % 以二进制只读模式打开文件
rawData1 = fread(fid, Inf, 'uint8'); % 读取所有数据，按8位无符号整数解析
fclose(fid);

%% 数据筛选，筛选掉异常数据包
% 筛选规则：搜索包头（0xAA,0xBB）和包尾（0xCC,0xDD）
%rawData1=rawData1 (2064:end);%去掉第一个不完整的能谱数据
rawData = uint8(rawData1);
n = length(rawData);
% 向量化查找所有包头（170=0xAA, 187=0xBB）和包尾（204=0xCC, 221=0xDD）
headerPositions = find(rawData(1:end-1) == 170 & rawData(2:end) == 187);
footerPositions = find(rawData(1:end-1) == 204 & rawData(2:end) == 221);
cnt_CCDD0000AABB = find(rawData(1:end-5) == 204 &rawData(2:end-4) == 221&rawData(3:end-3) == 0&rawData(4:end-2) == 0&rawData(5:end-1) == 170&rawData(6:end) == 187);%包尾接包头的数量
cnt_CCDDAABB = find(rawData(1:end-5) == 204 &rawData(2:end-4) == 221&rawData(5:end-1) == 170&rawData(6:end) == 187);%包尾接包头的数量
% 匹配包头和包尾，提取完整数据包
maxPossiblePackets = min(length(headerPositions), length(footerPositions));
extractedPackets = cell(maxPossiblePackets, 1);
packetCount = 0;
for i = 1:length(headerPositions)
    headerIdx = headerPositions(i);
    nextFooter = footerPositions(find(footerPositions > headerIdx, 1)); % 找当前包头后的第一个包尾
    if ~isempty(nextFooter)
        packetEnd = nextFooter + 1;
        packetCount = packetCount + 1;
        extractedPackets{packetCount} = rawData(headerIdx:packetEnd+2); % 提取完整数据包
    end
end          
extractedPackets = extractedPackets(1:packetCount); % 只保留有效数据包
fprintf('找到有效能谱数据包数：%d\n', packetCount);

% 新增：只保留长度为2064字节的数据包
validIdx = cellfun(@(x) length(x)==packet_length, extractedPackets); % 找到长度为2064的完整包
extractedPackets = extractedPackets(validIdx);
packetCount = sum(validIdx); % 更新有效包数量
fprintf('筛选后，长度为2064字节的数据包数：%d\n', packetCount);
zeroIndices = find(validIdx == 0);

return;

% 重构数据解析逻辑：从16位→32位（适配能谱4字节道址数据）
if packetCount > 0%之前统计的有效数据包数量。
    maxValues = 0;
    for i = 1:packetCount
        maxValues = max(maxValues,floor(length(extractedPackets{i}) / 4)); % 4字节为一个能谱数据单元
    end
    % 预分配32位数据矩阵（替换原uint16）
    combinedValue = zeros(packetCount, maxValues, 'uint32'); %
    % combinedValue = zeros(5, maxValues, 'uint32'); 
    for i = 1:packetCount
        packetData = extractedPackets{i};
        packetLength = length(packetData);
        numValues = floor(packetLength / 4);
        % 提取4字节拼接为32位整数（适配能谱数据格式）
        byte1 = uint32(packetData(1:4:4*numValues));
        byte2 = uint32(packetData(2:4:4*numValues));
        byte3 = uint32(packetData(3:4:4*numValues));
        byte4 = uint32(packetData(4:4:4*numValues));
        % 拼接4字节为32位整数（大端序）
        combinedValue(i, 1:numValues) = bitor(bitor(bitshift(byte1, 24), bitshift(byte2, 16)), ...
                                             bitor(bitshift(byte3, 8), byte4));%拼接为32bit的数据
    end
else
    combinedValue = [];
end
combinedValue=combinedValue'; % 转置为“道址点×数据包数”的矩阵

% 过滤掉长度不符合要求的数据包（包尾标识为0xCCDD，对应32位值52445→调整为能谱校验值）
%data = combinedValue(:, combinedValue(sample_deepth+4, :) == 52445);
%3437035520
data = combinedValue(:, combinedValue(sample_deepth+4, :) ==3437035520);
data(sample_deepth+4:end,:)=[]; % 去掉包尾数据
data(1,:)=[]; % 去掉包头数据

%% 解析各通道能谱数据（核心修改：从波形提取→能谱叠加）
wave_cnt=size(data,2); % 有效数据包总数

% 初始化16通道能谱累加矩阵，指定类型为uint64
Energy_CH1=zeros  (sample_deepth,1,  'uint64');  
Energy_CH2=zeros  (sample_deepth,1,  'uint64');
Energy_CH3=zeros  (sample_deepth,1,  'uint64');
Energy_CH4=zeros  (sample_deepth,1,  'uint64');
Energy_CH5=zeros  (sample_deepth,1,  'uint64');
Energy_CH6=zeros  (sample_deepth,1,  'uint64');
Energy_CH7=zeros  (sample_deepth,1,  'uint64');
Energy_CH8=zeros  (sample_deepth,1,  'uint64');
Energy_CH9=zeros  (sample_deepth,1,  'uint64');
Energy_CH10=zeros(sample_deepth,1,'uint64');
Energy_CH11=zeros(sample_deepth,1,'uint64');
Energy_CH12=zeros(sample_deepth,1,'uint64');
Energy_CH13=zeros(sample_deepth,1,'uint64');
Energy_CH14=zeros(sample_deepth,1,'uint64');
Energy_CH15=zeros(sample_deepth,1,'uint64');
Energy_CH16=zeros(sample_deepth,1,'uint64');


spe_ch1 = []; spe_ch2 = []; spe_ch3 = []; spe_ch4 = [];
spe_ch5 = []; spe_ch6 = []; spe_ch7 = []; spe_ch8 = [];
spe_ch9 = []; spe_ch10 = []; spe_ch11 = []; spe_ch12 = [];
spe_ch13 = []; spe_ch14 = []; spe_ch15 = []; spe_ch16 = [];


% 初始化各通道计数器（保留原逻辑）
CH1_CNT=0;
CH2_CNT=0;
CH3_CNT=0;
CH4_CNT=0;
CH5_CNT=0;
CH6_CNT=0;
CH7_CNT=0;
CH8_CNT=0;
CH9_CNT=0;
CH10_CNT=0;
CH11_CNT=0;
CH12_CNT=0;
CH13_CNT=0;
CH14_CNT=0;
CH15_CNT=0;
CH16_CNT=0;

for i=1:wave_cnt
    %for i=1:10
    X=data(:,i);%去掉包头包尾的数据
    data_part= uint64(X);
    %data_part= double(X);
    CH_NUM(:,i) = data_part(1); % 通道号（2^0~2^15）
    % 时间戳保留原逻辑（可忽略，不影响能谱）
   %Time(:,i) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;  
    current_packet_data = data_part(data_start:end);
    % 核心修改：将单包能谱数据叠加到对应通道（替换原波形存储）
    switch data_part(2)%第二个单元是通道号
           
        case 1    % CH1=2^0
            Energy_CH1 = Energy_CH1 + data_part(data_start:end);
             spe_ch1 = [spe_ch1, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH1_CNT=CH1_CNT+1;

        case 2    % CH2=2^1
            Energy_CH2 = Energy_CH2 + data_part(data_start:end);
             spe_ch2 = [spe_ch2, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH2_CNT=CH2_CNT+1;
        case 4    % CH3=2^2
            Energy_CH3 = Energy_CH3 + data_part(data_start:end);
             spe_ch3 = [spe_ch3, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH3_CNT=CH3_CNT+1;
        case 8    % CH4=2^3
            Energy_CH4 = Energy_CH4 + data_part(data_start:end);
             spe_ch4 = [spe_ch4, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH4_CNT=CH4_CNT+1;
        case 16   % CH5=2^4
            Energy_CH5 = Energy_CH5 + data_part(data_start:end);
             spe_ch5 = [spe_ch5, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH5_CNT=CH5_CNT+1;
        case 32   % CH6=2^5
            Energy_CH6 = Energy_CH6 + data_part(data_start:end);
             spe_ch6 = [spe_ch6, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH6_CNT=CH6_CNT+1;
        case 64   % CH7=2^6
            Energy_CH7 = Energy_CH7 + data_part(data_start:end);
             spe_ch7 = [spe_ch7, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH7_CNT=CH7_CNT+1;
        case 128  % CH8=2^7
            Energy_CH8 = Energy_CH8 + data_part(data_start:end);
             spe_ch8 = [spe_ch8, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH8_CNT=CH8_CNT+1;
        case 256  % CH9=2^8
            Energy_CH9 = Energy_CH9 + data_part(data_start:end);
             spe_ch9 = [spe_ch9, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH9_CNT=CH9_CNT+1;
        case 512  % CH10=2^9
            Energy_CH10 = Energy_CH10 + data_part(data_start:end);
             spe_ch10 = [spe_ch10, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH10_CNT=CH10_CNT+1;
        case 1024 % CH11=2^10
            Energy_CH11 = Energy_CH11 + data_part(data_start:end);
             spe_ch11 = [spe_ch11, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH11_CNT=CH11_CNT+1;
        case 2048 % CH12=2^11
            Energy_CH12 = Energy_CH12 + data_part(data_start:end);
             spe_ch12 = [spe_ch12, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH12_CNT=CH12_CNT+1;
        case 4096 % CH13=2^12
            Energy_CH13 = Energy_CH13 + data_part(data_start:end);
             spe_ch13 = [spe_ch13, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH13_CNT=CH13_CNT+1;
        case 8192 % CH14=2^13
            Energy_CH14 = Energy_CH14 + data_part(data_start:end);
             spe_ch14 = [spe_ch14, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH14_CNT=CH14_CNT+1;
        case 16384% CH15=2^14
            Energy_CH15 = Energy_CH15 + data_part(data_start:end);
             spe_ch15 = [spe_ch15, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH15_CNT=CH15_CNT+1;
        case 32768% CH16=2^15
            Energy_CH16 = Energy_CH16 + data_part(data_start:end);
             spe_ch16 = [spe_ch16, current_packet_data]; % 列扩展，最终为512×CH1包数
            CH16_CNT=CH16_CNT+1;
    end
end

%for i=1:1:size(spe_ch1,2)
%total_cnt_rate(1:i)=sum(spe_ch1(1:512,i))/test_time;
%end

for i=1:1:1
total_cnt_rate(1:i)=sum(Energy_CH1(1:16,i))/test_time;
end
%% 能谱结果可视化（替换原波形绘图）
% 绘制CH16能谱（保留原代码的CH16优先级）
% figure('Name','CH1能谱累加结果');
% plot(1:sample_deepth, Energy_CH1, 'LineWidth',1.2);
% title(sprintf("CH1 能谱（累计数据包数：%d）", CH16_CNT-1));
% xlabel("道址");
% ylabel("累计计数");
% grid on;

%% 可选：绘制所有16通道能谱（4×4子图）
%figure('Name','16通道能谱汇总');
%channel_energy = {Energy_CH1,Energy_CH2,Energy_CH3,Energy_CH4,
%                  Energy_CH5,Energy_CH6,Energy_CH7,Energy_CH8,
%                  Energy_CH9,Energy_CH10,Energy_CH11,Energy_CH12,
%                  Energy_CH13,Energy_CH14,Energy_CH15,Energy_CH16};
%channel_cnt = [CH1_CNT,CH2_CNT,CH3_CNT,CH4_CNT,CH5_CNT,CH6_CNT,CH7_CNT,CH8_CNT,
%               CH9_CNT,CH10_CNT,CH11_CNT,CH12_CNT,CH13_CNT,CH14_CNT,CH15_CNT,CH16_CNT];

% 16通道能谱，分4组绘制，每组4个通道，2x2子图布局

channel_energy = {Energy_CH1,Energy_CH2,Energy_CH3,Energy_CH4,...
                  Energy_CH5,Energy_CH6,Energy_CH7,Energy_CH8,...
                  Energy_CH9,Energy_CH10,Energy_CH11,Energy_CH12,...
                  Energy_CH13,Energy_CH14,Energy_CH15,Energy_CH16};
channel_cnt = [CH1_CNT,CH2_CNT,CH3_CNT,CH4_CNT,...
               CH5_CNT,CH6_CNT,CH7_CNT,CH8_CNT,...
               CH9_CNT,CH10_CNT,CH11_CNT,CH12_CNT,...
               CH13_CNT,CH14_CNT,CH15_CNT,CH16_CNT];

%group_num = 4; % 分成4组
%channels_per_group = 4;
%
%for g = 1:group_num
%    figure('Name', sprintf('通道组 %d 能谱汇总', g));
%    for ch = 1:channels_per_group
%        ch_idx = (g-1)*channels_per_group + ch; % 真实通道编号
%        subplot(2,2,ch);
%        plot(1:sample_deepth, channel_energy{ch_idx}, 'LineWidth', 1);
%        title(sprintf("CH%d（包数：%d）", ch_idx, channel_cnt(ch_idx)-1));
%        xlabel("道址");
%        ylabel("计数");
%        grid on;
%        xlim([1, sample_deepth]);
%    end
%    sgtitle(sprintf('第%d组通道能谱（CH%d~CH%d）', g, (g-1)*channels_per_group+1, g*channels_per_group));
%end







%for ch=1:16
%    subplot(4,4,ch);
%    plot(1:sample_deepth, channel_energy{ch}, 'LineWidth',1);
%    title(sprintf("CH%d（包数：%d）", ch, channel_cnt(ch)-1));
%    xlabel("道址");
%    ylabel("计数");
%    grid on;
%    xlim([0, sample_deepth]);
%end
%sgtitle('16通道能谱累加结果（512道址）');

%% 能谱统计（替换原噪声计算）
% CH16能谱统计
MEAN_CH16=mean(double(Energy_CH16));
STD_CH16=std(double(Energy_CH16));
fprintf('CH16能谱统计：平均值=%.2f，标准差=%.2f\n', MEAN_CH16, STD_CH16);




%% 时间戳差分，丢包情况评估（保留原逻辑）
%time_inter=diff(Time);
%figure('Name','时间戳差分（丢包评估）');
%plot(time_inter);hold on;
%title("时间戳差分（丢包评估）");
%xlabel("数据包序号");
%ylabel("时间差（1ms单位）");
% grid on;
X_Peak=1:1:512;