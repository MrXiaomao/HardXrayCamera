clear;
clc;
close all;

% =========================================================
% 网口能谱数据丢包检查
% 功能：
% 1. 解析保存的 .dat 原始数据；
% 2. 校验包头 0xAABB、包尾 0xCCDD；
% 3. 按通道分别提取时间；
% 4. 由于各通道能谱时间间隔相同，只估计一个全局能谱周期；
% 5. 根据各通道能谱序号差分判断是否丢包；
% 6. 最后统一打印 16 个通道的丢包统计表。
% =========================================================

% ==============================
% 参数设置
% ==============================
basePath = 'C:\Users\12074\Desktop\temp\硬X射线测试数据';   % 你的基准文件夹路径
% 如果基准路径不存在，则退回到桌面路径
if ~exist(basePath, 'dir')
    basePath = fullfile(getenv('USERPROFILE'), 'Desktop');
end
[FileName1, PathName1] = uigetfile('*.dat', 'OpenFile', basePath);

if isequal(FileName1, 0)
    error('未选择文件，程序结束。');
end
filename = fullfile(PathName1, FileName1);

specLen = 512;                                % 512道；如果是16道，改为16
packetLen = 2 + 2 + 2 + specLen*2 + 2;         % 512道为1032字节，16道为40字节

headerValue = hex2dec('AABB');
tailValue   = hex2dec('CCDD');

% 如果已知能谱周期，例如10ms或50ms，可直接填写；
% 如果不确定，保持 []，程序会从一个有效通道的时间差众数估计。
knownSpecPeriod = [];   % 单位 ms

% 是否绘制每个通道的序号差分图
showDiffFigure = true;

% 16通道通道标志位
% 注意：如果你的协议中 CH9~CH16 的通道号定义不是下面这种标准位标志，
% 需要按实际协议修改 chFlagList。
chFlagList = uint16([ ...
    hex2dec('0001'), hex2dec('0002'), hex2dec('0004'), hex2dec('0008'), ...
    hex2dec('0010'), hex2dec('0020'), hex2dec('0040'), hex2dec('0080'), ...
    hex2dec('0100'), hex2dec('0200'), hex2dec('0400'), hex2dec('0800'), ...
    hex2dec('1000'), hex2dec('2000'), hex2dec('4000'), hex2dec('8000')]);

% ==============================
% 读取原始字节
% ==============================
fid = fopen(filename, 'rb');
if fid < 0
    error('无法打开文件：%s', filename);
end

raw = fread(fid, inf, 'uint8');
fclose(fid);

raw = uint8(raw);
nBytes = length(raw);

% ==============================
% 大端 uint16 解析函数
% ==============================
read_u16_be = @(buf, idx) double(buf(idx)) * 256 + double(buf(idx+1));

% ==============================
% 搜索并解析数据包
% ==============================
packetIndex = [];
chList = [];
timeList = [];

i = 1;
count = 0;

while i <= nBytes - packetLen + 1

    head = read_u16_be(raw, i);

    if head == headerValue
        tailPos = i + packetLen - 2;
        tail = read_u16_be(raw, tailPos);

        if tail == tailValue
            count = count + 1;

            ch   = read_u16_be(raw, i + 2);
            t_ms = read_u16_be(raw, i + 4);

            packetIndex(count, 1) = i;
            chList(count, 1) = ch;
            timeList(count, 1) = t_ms;

            % 找到完整包后，跳到下一个包
            i = i + packetLen;
        else
            % 包头对了但包尾不对，说明可能错位或中间数据异常
            i = i + 1;
        end
    else
        i = i + 1;
    end
end

fprintf('共解析到 %d 个完整能谱包。\n', count);
fprintf('当前设置：specLen = %d，道数；packetLen = %d 字节。\n', specLen, packetLen);

% =========================================================
% 估计统一能谱周期
% 说明：各通道能谱时间间隔一定相同，所以只估计一个 specPeriod。
% 但不要直接对混合后的 timeList 做 diff，因为不同通道可能有相同时间，
% 混合差分会出现0或异常值。这里从第一个有效通道估计周期。
% =========================================================
if ~isempty(knownSpecPeriod)
    specPeriod = knownSpecPeriod;
    specPeriodSource = '手动指定';
else
    specPeriod = NaN;
    specPeriodSource = '';

    for chNo = 1:16
        chFlag = double(chFlagList(chNo));
        idx = find(chList == chFlag);
        t = timeList(idx);

        if length(t) < 2
            continue;
        end

        t_unwrap = unwrap_time16_ms(t);
        dt = diff(t_unwrap);
        dt_valid = dt(dt > 0);

        if ~isempty(dt_valid)
            specPeriod = mode(dt_valid);
            specPeriodSource = sprintf('CH%d', chNo);
            break;
        end
    end

    if isnan(specPeriod)
        error('无法估计能谱周期：所有通道的数据包数量不足或时间没有正向变化。');
    end
end

fprintf('统一能谱周期：%.3f ms，来源：%s。\n', specPeriod, specPeriodSource);

% ==============================
% 结果变量：统一统计16个通道
% ==============================
channelNoList = (1:16).';
packetCountList = zeros(16, 1);
hasLostList = false(16, 1);
lostCountList = zeros(16, 1);
periodList = repmat(specPeriod, 16, 1);

% ==============================
% 按16个通道分别判断丢包
% ==============================
figure;
hold on;
grid on;

xlabel('接收包序号');
ylabel('能谱序号差分');
title('16通道序号差分图');
for chNo = 1:16
    chFlag = double(chFlagList(chNo));

    idx = find(chList == chFlag);
    t = timeList(idx);

    packetCountList(chNo) = length(t);

    if length(t) < 2
        % 包数量不足，无法判断丢包
        continue;
    end

    % 16bit 时间展开，处理 65535 ms 回绕
    t_unwrap = unwrap_time16_ms(t);

    % 时间换算为能谱序号
    seq = round((t_unwrap - t_unwrap(1)) / specPeriod) + 1;
    dseq = diff(seq);

    % 差分大于1，认为中间有能谱丢失
    lostPos = find(dseq > 1);
    lostNum = dseq(lostPos) - 1;

    lostTotal = sum(lostNum);
    lostCountList(chNo) = lostTotal;
    hasLostList(chNo) = lostTotal > 0;

    % 绘图：每个通道的序号差分
    if showDiffFigure
        plot(dseq, '-o', 'DisplayName', sprintf('CH%d', chNo));
    end
end
% 正常差分参考线
yline(1, '--', '正常差分=1', 'HandleVisibility', 'off');
% 图例
legend('show', 'Location', 'best');

%% ==============================
% 最后统一打印16个通道的丢包情况
% 第一列：通道编号
% 第二列：数据包个数
% 第三列：是否丢包
% 第四列：丢包个数
% ==============================
for chNo = 1:16
    if hasLostList(chNo)
        lostStr = '是';
    else
        lostStr = '否';
    end
end

fprintf('====================================================\n');

% 同时生成 table，方便后续保存或查看
lostStrList = strings(16, 1);
lostStrList(hasLostList) = "是";
lostStrList(~hasLostList) = "否";

resultTable = table(channelNoList, packetCountList, lostStrList, lostCountList, periodList, ...
    'VariableNames', {'通道编号', '数据包个数', '是否丢包', '丢包个数', '能谱周期_ms'});

disp(resultTable);

% 如需保存统计结果，取消下面两行注释
% [~, name, ~] = fileparts(filename);
% writetable(resultTable, fullfile(PathName1, [name, '_lost_summary.csv']));

% =========================================================
% 本地函数：展开16bit毫秒时间，处理 0~65535 ms 回绕
% =========================================================
function t_unwrap = unwrap_time16_ms(t)
    t = double(t(:));
    t_unwrap = t;
    wrapOffset = 0;

    for k = 2:length(t)
        if t(k) + wrapOffset < t_unwrap(k-1)
            wrapOffset = wrapOffset + 65536;
        end
        t_unwrap(k) = t(k) + wrapOffset;
    end
end
