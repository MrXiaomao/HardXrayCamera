clear;
clc;
close all;

% =========================================================
% 16通道波形数据丢包检查
% 波形包格式：
% 包头(0xAABB) + 通道号(16bit) + 时刻(16bit) + 584点波形(584*16bit) + 包尾(0xCCDD)
% 总长度：2 + 2 + 2 + 584*2 + 2 = 1176 字节
%
% 时刻单位：10ms，每10ms采集一次波形
% 判断方法：同一通道内，对展开后的时刻序号 diff，若差分 > 1，则认为丢包
% =========================================================

% ==============================
% 参数设置
% ==============================
% basePath = 'C:\Users\12074\Desktop\temp\硬X射线测试数据';   % 你的基准文件夹路径
basePath = 'C:\Users\12074\Desktop\QT_software\build_HardXrayCamera\测试数据';
% 如果基准路径不存在，则退回到桌面路径
if ~exist(basePath, 'dir')
    basePath = fullfile(getenv('USERPROFILE'), 'Desktop');
end
[FileName1, PathName1] = uigetfile('*.dat', 'OpenFile', basePath);

if isequal(FileName1, 0)
    error('未选择文件。');
end
filename = fullfile(PathName1, FileName1);

wavePointNum = 580;
packetLen = 2 + 2 + 2 + wavePointNum*2 + 2;   % 1176字节

headerValue = hex2dec('AABB');
tailValue   = hex2dec('CCDD');

% 固定采集周期：10ms
sampleInterval_ms = 10;

% 是否绘制每个通道的差分图
showDiffFigure = true;

% 16个通道的标准16bit位标志
% 注意：如果你的协议中 CH9~CH16 的通道号定义不是下面这样，需要修改此处。
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

scanPos = 1;
count = 0;
badHeaderTailCount = 0;

while scanPos <= nBytes - packetLen + 1

    head = read_u16_be(raw, scanPos);

    if head == headerValue
        tailPos = scanPos + packetLen - 2;
        tail = read_u16_be(raw, tailPos);

        if tail == tailValue
            count = count + 1;

            ch = read_u16_be(raw, scanPos + 2);
            t10ms = read_u16_be(raw, scanPos + 4);   % 单位：10ms计数

            packetIndex(count, 1) = scanPos;
            chList(count, 1) = ch;
            timeList(count, 1) = t10ms;

            % 找到完整包后，跳到下一个包
            scanPos = scanPos + packetLen;
        else
            % 包头对了但包尾不对，可能错位或中间数据异常
            badHeaderTailCount = badHeaderTailCount + 1;
            scanPos = scanPos + 1;
        end
    else
        scanPos = scanPos + 1;
    end
end

fprintf('文件：%s\n', filename);
fprintf('共解析到 %d 个完整波形包。\n', count);
fprintf('包头匹配但包尾不匹配次数：%d\n', badHeaderTailCount);
fprintf('波形包长度：%d 字节。\n', packetLen);
fprintf('固定采集间隔：%d ms。\n\n', sampleInterval_ms);

% ==============================
% 初始化汇总结果
% ==============================
channelNoList = (1:16)';
packetCountList = zeros(16, 1);
lostFlagList = false(16, 1);
lostCountList = zeros(16, 1);
repeatCountList = zeros(16, 1);
abnormalCountList = zeros(16, 1);

% ==============================
% 按16个通道分别判断丢包
% ==============================
figure;
hold on;
grid on;

xlabel('接收包序号');
ylabel('波形序号差分');
title('16通道序号差分图');

for chNo = 1:16
    chFlag = double(chFlagList(chNo));

    idx = find(chList == chFlag);
    t = timeList(idx);

    packetCountList(chNo) = length(t);

    if length(t) < 2
        continue;
    end

    % 16bit 时刻展开。时刻单位是10ms，最大65535，即655.35秒后回绕。
    t_unwrap = unwrap_time16(t);

    % 时刻字段本身就是10ms采样序号，因此直接差分
    dseq = diff(t_unwrap);

    % 差分 > 1：中间缺少波形包
    lostPos = find(dseq > 1);
    lostNum = dseq(lostPos) - 1;

    % 差分 == 0：重复包或时刻未更新
    repeatPos = find(dseq == 0);

    % 差分 < 0：通常是回绕处理失败、乱序或时刻异常
    abnormalPos = find(dseq < 0);

    lostCountList(chNo) = sum(lostNum);
    lostFlagList(chNo) = lostCountList(chNo) > 0;
    repeatCountList(chNo) = length(repeatPos);
    abnormalCountList(chNo) = length(abnormalPos);

    if showDiffFigure
        plot(dseq, '-o', 'DisplayName', sprintf('CH%d', chNo));
    end
end

% 正常差分参考线
yline(1, '--', '正常差分=1', 'HandleVisibility', 'off');
% 图例
legend('show', 'Location', 'best');

% ==============================
% 最后统一打印16个通道丢包情况
lostStrCell = cell(16, 1);
for chNo = 1:16
    if lostFlagList(chNo)
        lostStrCell{chNo} = '是';
    else
        lostStrCell{chNo} = '否';
    end
end
fprintf('======================================================\n');

% 生成MATLAB表格，方便查看或导出
resultTable = table(channelNoList, packetCountList, string(lostStrCell), lostCountList, ...
    'VariableNames', {'通道编号', '数据包个数', '是否丢包', '丢包个数'});

disp(resultTable);

% 如果需要更详细的诊断，可查看这个表
resultTableDetail = table(channelNoList, packetCountList, string(lostStrCell), lostCountList, ...
    repeatCountList, abnormalCountList, ...
    'VariableNames', {'通道编号', '数据包个数', '是否丢包', '丢包个数', '重复或时刻未更新次数', '乱序或时刻异常次数'});

% ==============================
% 本脚本用到的局部函数
% ==============================
function t_unwrap = unwrap_time16(t)
    t = double(t(:));
    t_unwrap = t;
    offset = 0;

    for k = 2:length(t)
        currentValue = t(k) + offset;

        % 16bit回绕：65535 -> 0
        if currentValue < t_unwrap(k-1)
            offset = offset + 65536;
            currentValue = t(k) + offset;
        end

        t_unwrap(k) = currentValue;
    end
end
