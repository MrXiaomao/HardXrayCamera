clear all;
close all;


%% TITLE General_Wave_Plot
%适用场合：HXR示波器数据解析
%更新日期＿2025.09.15
%编写：沈晖寅 1305188374@qq.com
%功能：①观测一定数量的波形

%% step1 参数设置[采样点数：分析波形个数]
sample_deepth=1024;  %56 每个波形的采样深度（256个采样点）
data_start=3;  % 波形数据在每一帧的第3个16位整数开始（跳过帧头）
win_10us=625;
win_100us=6250;
%% step2 读取文件并绘刿
initialPath = 'C:\Users\12074\Desktop\QT_software\build_HardXrayCamera\x64\波形测试\'; % 替换为你的起始路径
[FileName1,PathName1]=uigetfile('*.dat','OpenFile',initialPath); % 弹窗选择要解析的.dat文件
FileName_1=[PathName1,FileName1];
fid = fopen(FileName_1, 'r','b');  % 以二进制只读模式打开文件
rawData = fread(fid, Inf, 'uint8'); % 读取所有数据，按8位无符号整数解析
fclose(fid);

% fileID1 = fopen(FileName_1, 'r','b');
% FindDataHead=fread(fileID1,8192*4,'uint8');
% for i=1:1:length(FindDataHead)%寻找第一帧数据帧夿
%    if(FindDataHead(i)==170 &&FindDataHead(i+1)==187 )%&& HeadPostion>2)std(double(wave))
%            HeadPostion=i-1;
%            % if( HeadPostion>2)
%            %     break;
%            %end
%            break;
%    end
% end
% fclose(fileID1);
% 
% D = dir(FileName_1);
% fsize = D.bytes-HeadPostion;
% wave_cnt=fix(fsize/Total_bytes);
% 
% precision = 'uint16';
% fileID = fopen(FileName_1, 'r','b');
% if HeadPostion > 0  %将文件包读取起点移动到第丿帧有效数据包夿
%    B=fread(fileID,HeadPostion,'uint8');
% end
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
temp = log2(double(combinedValue(2,:))); % 转化序号
combinedValue(2,:) = uint16(temp);

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
    case 32
        Wave_CH6(:,CH6_CNT)=data_part(data_start:end);
        Time_PACK6(:,CH6_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
        Peak_CH6(CH6_CNT)=max(Wave_CH6(:,CH6_CNT));
        CH6_CNT=CH6_CNT+1;  
    case 64
        Wave_CH7(:,CH7_CNT)=data_part(data_start:end);
        Time_PACK7(:,CH7_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
        Peak_CH7(CH7_CNT)=max(Wave_CH7(:,CH7_CNT));
        CH7_CNT=CH7_CNT+1; 
    case 128
        Wave_CH8(:,CH8_CNT)=data_part(data_start:end);
        Time_PACK8(:,CH8_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
        Peak_CH8(CH8_CNT)=max(Wave_CH8(:,CH8_CNT));
        CH8_CNT=CH8_CNT+1;
    case 256
        Wave_CH9(:,CH9_CNT)=data_part(data_start:end);
        Time_PACK9(:,CH9_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
        Peak_CH9(CH9_CNT)=max(Wave_CH9(:,CH9_CNT));
        CH9_CNT=CH9_CNT+1;
    case 512
        Wave_CH10(:,CH10_CNT)=data_part(data_start:end);
        Time_PACK10(:,CH10_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
        Peak_CH10(CH10_CNT)=max(Wave_CH10(:,CH10_CNT));
        CH10_CNT=CH10_CNT+1;
    case 1024
        Wave_CH11(:,CH11_CNT)=data_part(data_start:end);
        Time_PACK11(:,CH11_CNT) = data_part(2)*2^32+data_part(3)*2^16+data_part(4);
        Peak_CH11(CH11_CNT)=max(Wave_CH11(:,CH11_CNT));
        CH11_CNT=CH11_CNT+1;
    case 2048
        Wave_CH12(:,CH12_CNT)=data_part(data_start:end);
        Time_PACK12(:,CH12_CNT) = data_part(2)*2^32+data_part(3)*2^16+data_part(4);
        Peak_CH12(CH12_CNT)=max(Wave_CH12(:,CH12_CNT));
        CH12_CNT=CH12_CNT+1;
    case 4096
        Wave_CH13(:,CH13_CNT)=data_part(data_start:end);
        Time_PACK13(:,CH13_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
        Peak_CH13(CH13_CNT)=max(Wave_CH13(:,CH13_CNT));
        CH13_CNT=CH13_CNT+1;
    case 8192
        Wave_CH14(:,CH14_CNT)=data_part(data_start:end);
        Time_PACK14(:,CH14_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
        Peak_CH14(CH14_CNT)=max(Wave_CH14(:,CH14_CNT));
        CH14_CNT=CH14_CNT+1;
    case 16384
        Wave_CH15(:,CH15_CNT)=data_part(data_start:end);
        Time_PACK15(:,CH15_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
        Peak_CH15(CH15_CNT)=max(Wave_CH15(:,CH15_CNT));
        CH15_CNT=CH15_CNT+1;
    case 32768
        Wave_CH16(:,CH16_CNT)=data_part(data_start:end);
        Time_PACK16(:,CH16_CNT) = data_part(2)+data_part(3)*2^16+data_part(4)*2^32;
      %  base_CH16(CH16_CNT)=mean(Wave_CH16(100:200,CH16_CNT));
        base_CH16(CH16_CNT)=mean(Wave_CH16(20:40,CH16_CNT));
        Peak_CH16(CH16_CNT)=max(Wave_CH16(:,CH16_CNT));
        REAL_peak_CH16(CH16_CNT)=Peak_CH16(CH16_CNT)-base_CH16(CH16_CNT);
        CH16_CNT=CH16_CNT+1;
end
end

waveform=reshape(Wave_CH1,1,[]);
for i=1:1:fix(length(waveform)/625-1)
window_10us(i,:)=waveform(((i-1)*625+1):(i*625));
end

for j=1:1:fix(length((waveform)/6250)-1)
window_100us(j,:)=waveform(((j-1)*6250+1):(j*6250));
end

%window_100us=waveform(1:6250);
%% °Ѳ¨ЎвÁл¯
%total_wave_ch16=reshape(Wave_CH16,1,[]);
%real_wave=

%subplot(2,1,1)
% plot(Wave_CH16.*660/16000);
% title("CH16 wave")
% xlabel("Time(*16ns)")
% ylabel("·ù¶Ȗµ£¨mV£©")
% 
% plot(REAL_peak_CH16.*660/16000);
% title("CH16 peak")
% xlabel("Time(*16ns)")
% ylabel("·ù¶Ȗµ£¨mV£©")
% a=max(REAL_peak_CH1);
% y=hist(double(REAL_peak_CH1),300);
% plot(y)
% subplot(2,1,2)
% plot(Wave_CH16(:,1));
% title("CH16 -330~330mV")
% xlabel("Time(*16ns)")
% ylabel("幅度值LSB")
%result = any(Wave_CH16(:) > 10000)
%fclose(fileID);
% %%
%     rawData = uint8(rawData);
%     n = length(rawData);
%     % 向量化查找所有可能的包头和包尾位罿
%     headerPositions = find(rawData(1:end-1) == 170 & rawData(2:end) == 187);
%     footerPositions = find(rawData(1:end-1) == 204 & rawData(2:end) == 221);
% 
%     % 预分配细胞数组（估计朿大可能的数据包数量）
%     maxPossiblePackets = min(length(headerPositions), length(footerPositions));
%     extractedPackets = cell(maxPossiblePackets, 1);
%     packetCount = 0;
% 
%     % 匹配包头和包尿
%     for i = 1:length(headerPositions)
%         headerIdx = headerPositions(i);
%         % 查找在此包头之后的第丿个包尿
%         nextFooter = footerPositions(find(footerPositions > headerIdx, 1));
%         if ~isempty(nextFooter)
%             packetEnd = nextFooter + 1;
%             packetCount = packetCount + 1;
%             extractedPackets{packetCount} = rawData(headerIdx:packetEnd);
%         end
%     end
% 
%     % 调整细胞数组大小以匹配实际数据包数量
%     extractedPackets = extractedPackets(1:packetCount);
% 
%     % 预分配combinedValue矩阵并处理数据拼掿
%     if packetCount > 0
%         % 计算朿大可能的16位忼数釿
%         maxValues = 0;
%         for i = 1:packetCount
%             maxValues = max(maxValues,floor(length(extractedPackets{i}) / 2));
%         end
% 
%         combinedValue = zeros(packetCount, maxValues, 'uint16');
% 
%         for i = 1:packetCount
%             packetData = extractedPackets{i};
%             packetLength = length(packetData);
%             numValues = floor(packetLength / 2);
% 
%             % 丿次濧提取所有高位和低位字节
%             highBytes = uint16(packetData(1:2:2*numValues));
%             lowBytes = uint16(packetData(2:2:2*numValues));
% 
%             % 向量化拼接操使
%             combinedValue(i, 1:numValues) = bitor(bitshift(highBytes, 8), lowBytes);
%         end
%     else
%         combinedValue = [];
%     end
%     combinedValue=combinedValue';
% 
% 
%     %WAVE=combinedValue;
%     %WAVE(1:3,:)=[];
% 
%     data_part = combinedValue(:, combinedValue(1028, :) == 52445);
%     data_part(1028:end,:)=[];
% 
%     profile off;
%     profile viewer;
% 
%     val_16=find(combinedValue(2,:)==32768);

%% 噪声水平计算
MEAN=mean(double(Wave_CH16(:,1)))
STD=std(double(Wave_CH16(:,1)))

NOISE = 660*STD./16384


h=double(waveform);
BinWidth=1
histogram(h,BinWidth);
xlabel("幅度值LSB")
ylabel("count")
title("基线信号统计分布")

%% 时间戳差分，丢包情况评估
time_inter=diff(Time);plot(time_inter);hold on;
time_inter1=diff(Time_PACK1);plot(time_inter1);hold on;
time_inter2=diff(Time_PACK2);plot(time_inter2);hold on;
time_inter3=diff(Time_PACK3);plot(time_inter3);hold on;

% %% 通道增益丿致濧测诿
% % 叿512个点中多个峰峰忼，求平均得到一个鿚道的PKPK
% % 16个鿚道进行归一化处琿
% 
% % 查找极大倿
% [pks_max_CH15, locs_max_C15] = findpeaks(double(Wave_CH15(:,1)), 'MinPeakHeight', 0.5, 'MinPeakDistance', 0.1);
% % 查找极小值（通过对信号取反）
% [pks_min_CH15, locs_min_CH15] = findpeaks(-double(Wave_CH15(:,1)), 'MinPeakDistance', 0.1);
% pks_min_CH15 = -pks_min_CH15; % 恢复实际倿
% 
% pk_pk_CH15=mean(pks_max_CH15-pks_min_CH15);
% 
% 
% [pks_max_CH16, locs_max_C16] = findpeaks(double(Wave_CH16(:,1)), 'MinPeakHeight', 0.5, 'MinPeakDistance', 0.1);
% % 查找极小值（通过对信号取反）
% [pks_min_CH16, locs_min_CH16] = findpeaks(-double(Wave_CH16(:,1)), 'MinPeakDistance', 0.1);
% pks_min_CH16 = -pks_min_CH16; % 恢复实际倿
% 
% pk_pk_CH16=mean(pks_max_CH16-pks_min_CH16);
