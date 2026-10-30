# BombLab 实验分析与验证报告（AI 辅助）

## 分析来源与实验环境

本报告是 Codex 对本人领取的个人炸弹进行反汇编、恢复计算关系、实现解法并实际运行测试后生成的 AI 辅助分析记录。六关和 Honor 的实现由 AI 生成，以下推理也由 AI 整理，不能视为学生独立完成的分析。学生应依据课程公告核对这种辅助范围是否允许，并自行理解和复核代码及报告；本记录不声称学生已经完成该复核。

- 日期：2026-10-30。
- 环境：Ubuntu WSL 2，`uname -sm` 输出 `Linux x86_64`。
- 编译器：GCC 13.3.0；GDB 15.0.50；Python 3.12.3。
- 仓库：<https://github.com/hswang24/ics-2026-bomblab>。
- 个人包编号：`b6fbb7e988092e6a4d9b8a0b`。
- 原始 `bomb` SHA-256：`52d9c8bc7f02a7ee9c8b61d5c4af2d7c8da3794e1020adcd80aa10a10772d587`。
- 求解实现仅修改 `defuser.c`；接口、驱动、测试和工作流保持模板原样，个人炸弹保持下载时的内容。

本次依据 `objdump` 的 AT&T 语法汇编分析个人炸弹。以下地址来自本包，其他学生的地址、常量和表项可能不同。约定 `u32(x)` 为保留低 32 位，所有 32 位加、乘、异或和循环移位均按无符号语义执行，溢出按模 2^32 回绕。

## 第 1 关：调用关系与参数传递

`phase_01` 在 `0x40209d` 设置 `esi=1` 后调用 `valid_u32_fields`，并要求 `sscanf` 返回 1，因此本关口令是一个整数。`0x4020db` 调用 `p1_compute`，随后用 `cmp %eax,-0x14(%rbp)` 比较返回值与输入。

`p1_compute` 在 `0x40205b`、`0x402064` 分别右移 32 位和 16 位，随后依次调用 `p1_step1`、`p1_step0` 和 `p1_step2`。关键辅助函数是：

```asm
p1_step0: lea (%rdi,%rdi,4),%eax; add %esi,%eax; xor $0x33b0,%eax
p1_step1: lea (%rdi,%rsi,2),%eax; xor $0x3dab,%eax
p1_step2: lea (%rdi,%rdi,4),%eax; add $0xdeb0,%esi; xor %esi,%eax
```

令 `L=u32(C)`，`M=u32(C>>16)`，`H=u32(C>>32)`。跟踪每次调用前的 `edi`、`esi`，得到：

```text
b = (H + 2*M) XOR 0x3dab
 a = (5*L + H) XOR 0x33b0
answer = (5*a) XOR (b + 0xdeb0)
```

这里的 `M` 是右移 16 位后的低 32 位，不是仅保留 16 位。`lea` 的地址表达式在此用于整数运算，没有读取那个地址对应的内存。实际调用顺序不能仅凭辅助函数的名字猜测。

非公开 seed `20261030`：输出 `822866662`，原始炸弹返回 `Phase 1 defused.`。

## 第 2 关：六元素数组与循环递推

本关设置整数数量为 6，并要求 `sscanf` 返回 6。数组从 `rbp-0x40` 开始，每个元素 4 字节。首项计算与循环主体的关键指令：

```asm
402160: shr $0x18,%rax
402164: and $0x3f,%eax
402167: lea (%rax,%rax,2),%eax
40216a: cmp %eax,%esi
40216e: mov $0x6,%ecx
402173: mov $0x1,%edx
402178: mov %r13,%rax
40217b: shr %cl,%rax
40217e: and $0xf,%eax
402181: add %edx,%eax
402183: xor %esi,%eax
402189: lea (%rax,%rax,4),%eax
402190: add $0x1,%rdx
402194: add $0x4,%ecx
```

`esi` 起初保存首项，随后每轮从数组读取下一项并与计算值比较，因此循环使用的是前一项，而不是固定的首项。

```text
v[0] = 3 * ((C >> 24) & 63)
对 i = 1..5：
    nibble = (C >> (6 + 4*(i-1))) & 15
    v[i] = 5 * ((nibble + i) XOR v[i-1])
```

首轮右移 6 位，此后每轮增加 4，依次为 6、10、14、18、22。

非公开 seed `20261030`：输出 `177 880 4460 22265 111295 556380`，返回 `Phase 2 defused.`。

## 第 3 关：switch 跳转表

本关要求两个整数。`0x402311` 的 `and $0x7,%edi` 得出第一个整数 `index=C&7`；`0x40231f` 到 `0x402327` 得出辅助输入 `x=u32(C>>3) XOR u32(C>>32)`。

`p3_dispatch` 的关键跳转是：

```asm
402268: cmp $0x7,%eax
40226d: notrack jmp *0x405820(,%rax,8)
```

表中每项是 8 字节地址，指向的跳转片段再转到具体辅助函数。读取实际表项，得到下面的映射，而不是按 `p3_case_N` 的编号排列：

| index | 跳转地址 | 辅助函数 | 第二个整数 |
|---:|---|---|---|
| 0 | `0x402288` | `p3_case_1` | `(x XOR 0xec93) + (x>>2)` |
| 1 | `0x402290` | `p3_case_3` | `(4*x) XOR 0x3ccf` |
| 2 | `0x402298` | `p3_case_6` | `(4*x) XOR 0x8641` |
| 3 | `0x4022a0` | `p3_case_4` | `5*x + 0xcb43` |
| 4 | `0x4022a8` | `p3_case_0` | `5*x + 0x36899` |
| 5 | `0x4022b0` | `p3_case_5` | `(x XOR 0x31ac) + (x>>1)` |
| 6 | `0x402278` | `p3_case_2` | `3*x + 0x84e9` |
| 7 | `0x402280` | `p3_case_7` | `5*x + 0x740e` |

各分支的 32 位结果即第二个整数，最终由 `0x40232e` 比较。实现使用 C 的 `switch` 表达此映射。

非公开 seed `20261030`：输出 `0 1021097390`，返回 `Phase 3 defused.`。

## 第 4 关：二分递归与返回时组合结果

本关口令只有一个整数。入口在 `0x402403` 到 `0x402415` 构造参数：

```text
target = C & 127
salt = u32(C>>7) XOR u32(C>>32)
answer = walk(target, 0, 127, salt)
```

`p4_walk` 的 `sub %esi,%edi; shr $1,%edi; add %esi,%edi` 计算 `mid=low+((high-low)>>1)`。它先比较目标与中点，相等时直接返回；否则缩小区间，并在递归返回后处理子结果。

```text
walk(t, low, high, salt):
    mid = low + ((high-low)>>1)
    if t == mid:
        return (mid + 0x33) XOR salt
    if t < mid:
        child = walk(t, low, mid-1, salt)
        return (5*child) XOR (mid+salt) XOR 3
    child = walk(t, mid+1, high, salt)
    return (5*child) XOR (mid+salt) XOR 8
```

左分支由 `jb` 进入 `0x402380`，返回时异或 3；右分支在 `0x402362` 把下界设为 `mid+1`，返回时异或 8。它不是对访问中点简单求和，必须保留乘 5、异或和递归返回的顺序。目标始终在 0..127，区间逐步收缩，最大递归层数为 8。

非公开 seed `20261030`：输出 `2600981026`，返回 `Phase 4 defused.`。

## 第 5 关：结构体布局、头指针表与遍历

本关要求两个整数。`p5_traverse` 从 `0x405b60(,%rax,8)` 读头指针，在节点偏移 `0`、`8`、`12` 分别读取下一个节点指针、value 和 salt。因此原始节点布局为 16 字节：8 字节指针加两个 4 字节整数。

为在解法中避免引用炸弹地址，将 `0x405ba0` 开始的 12 个节点按地址顺序编号，指针转换为下标：

| 节点 | next | value（十六进制） | salt（十六进制） |
|---:|---:|---:|---:|
| 0 | 5 | `0x0b4a` | `0xe99d` |
| 1 | 8 | `0x0178` | `0x5372` |
| 2 | 10 | `0x0a70` | `0x8441` |
| 3 | 6 | `0x0976` | `0x491f` |
| 4 | 11 | `0x057d` | `0x7225` |
| 5 | 9 | `0x047e` | `0x4c05` |
| 6 | NULL | `0x030a` | `0x9d21` |
| 7 | 0 | `0x085e` | `0x16af` |
| 8 | 3 | `0x0738` | `0x6e76` |
| 9 | 1 | `0x0632` | `0xf675` |
| 10 | 4 | `0x0024` | `0x4a17` |
| 11 | 7 | `0x0210` | `0x2fee` |

头指针表转换为下标后为 `{2,10,4,11,7,0,5,9}`。它和节点的地址顺序不同。

```text
node = heads[C & 7]
hops = (C >> 3) & 3
acc = u32(C>>6) XOR u32(C>>32) XOR 0xaeba
先处理起点，再沿 next 移动 hops 次：
    acc = (acc XOR node.salt) + node.value
    last = node.value
输出 acc 和 last
```

关键是访问节点数为 `hops+1`。汇编 `0x402475` 才读 next；起点已在此前处理过。实现中用 12 作为 NULL 的哨兵编号，合法路径最多移动 3 次，不会解引用这个哨兵。

非公开 seed `20261030`：输出 `4169749574 2890`，返回 `Phase 5 defused.`。

## 第 6 关：函数指针表与两次间接调用

本关要求一个整数。`p6_dispatch` 的两次调用是：

```asm
402590: and $0x38,%ebx
402593: and $0x7,%eax
402596: call *0x405e00(,%rax,8)
40259d: xor $0x2485,%eax
4025a4: call *0x405e00(%rbx)
4025af: add $0x259b,%eax
```

第一次索引为 `C&7`。第二次 `%rbx` 保存 `C&0x38`，它是字节偏移，除以每项 8 字节后索引为 `(C>>3)&7`。不能把这个字节偏移直接当作元素下标。

实际函数指针表对应 `{op_0,op_5,op_4,op_3,op_2,op_0,op_3,op_1}`：

| 表下标 | 运算 F(index,x) |
|---:|---|
| 0、5 | `(x XOR 0xa6ab) + 2*x` |
| 1 | `ROL32(x,3) XOR 0xba54` |
| 2 | `(x+0xaf58) XOR (x>>2)` |
| 3、6 | `((x>>1) XOR x) + 0x13a2` |
| 4 | `ROR32(x,5)` |
| 7 | `((x<<7)+0xac0d) XOR (3*x)` |

整体计算为：

```text
x = u32(C>>6) XOR u32(C>>32)
tmp = F(C&7, x) XOR 0x2485
answer = F((C>>3)&7, tmp) + 0x259b
```

`rol`、`ror` 是 32 位循环移位：移出的一端要补回另一端，不能用普通移位代替。实现中的移位量仅为 3 或 5，避免移位 32 位造成未定义行为。

非公开 seed `20261030`：输出 `823386842`，返回 `Phase 6 defused.`。

## Honor：第七行布局与 context 相关计算

主程序通过前六关后，调用 `transcript_context` 计算上下文，再调用 `secret_gate(input, challenge, context)`。驱动已把 challenge 和 context 传给 `solve_secret`，求解函数无需读取文件、调用炸弹或重新计算前六关。

`secret_gate` 的关键指令为：

```asm
40272b: lea -0xd0(%rbp),%r12
402751: sub $0x24,%rax
402755: cmp $0x7f,%rax
402759: jbe 402770
402776: call secret_copy
40277b: cmpl $0x4e45504f,-0xb0(%rbp)
40278d: lea -0xac(%rbp),%rdi
402794: call secret_phase
```

`strlen(input)-36` 按无符号数与 127 比较，所以允许长度为 36..163。复制起点是 `rbp-0xd0`，被比较位置为 `rbp-0xb0`，差值是 32。小端的 `0x4e45504f` 对应 ASCII `OPEN`；数字部分从 `rbp-0xac` 开始，即输入偏移 36。

因此第七行布局为：32 个 `A`，紧跟 `OPEN`，再紧跟三个空格分隔的无符号十进制整数。复制函数会把整行和 NUL 复制进去；本解法最长 59 个字符，满足长度条件，也不涉及返回地址或寄存器的修改。

`secret_phase` 要求恰好三个整数，计算关系如下。令 `C` 为本关 challenge，`K` 为 context：

```text
index = K & 3
a = u32(C) XOR u32(C>>32)
b = u32(K) XOR u32(K>>32)
second = ROR32(a XOR b,3) XOR 0x9f8a
         XOR (0xfd07*index) XOR u32(K>>16)
third = second XOR (b + index + 0x628d + (a>>3))
```

关键证据包括：`0x402695` 对 context 取低 2 位；`secret_mix` 执行 `ror $3,%eax` 后异或 `0x9f8a`；`0x4026a0` 把 index 乘以 `0xfd07`；后续分别比较第二、第三个整数。注意 `a>>3` 是先得到 32 位 a 再右移，不能替换为 64 位 challenge 的移位。

非公开 seed `20261030` 的第七行为：

```text
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAOPEN2 3222370371 3382906523
```

将它接在对应的六行口令之后输入原始炸弹，返回 `Secret phase defused.`。

## 运行结果、复现与边界验证

### 公开测试

在仓库根目录执行：

```bash
make
python3 public_test.py
```

结果为正式六关 `60/60`，Honor `10/10`。教学示例 `make test-tutor` 为 `10/10`，教学结果不计入正式关卡。

### 非公开 seed 的完整验证

先确认 `20261030` 不在 `tests/public.json` 的公开列表中，再执行：

```bash
{ ./defuser --seed 20261030; ./defuser --secret --seed 20261030; } | ./bomb --seed 20261030
```

同一次运行依次输出 `Phase 1 defused.` 到 `Phase 6 defused.`，以及 `Secret phase defused.`，退出码为 0。上面每关列出的数值来自该次实际求解输出。

下图是浏览器展示 WSL 捕获的原始 stdout 后取得的截图；不是手工输入的通关文字，也不是学生手动操作终端的截图。图片涵盖每个已完成关卡及 Honor 的运行结果。

![非公开 seed 20261030 的六关和 Honor 实际输出](images/nonpublic-seed-20261030.jpg)

### 额外验证范围

本机 `.local-lab/verify_extra.py` 使用固定随机种子生成可复现的非公开 seed 与 challenge，并将解法输出交给未修改的原始炸弹检查，不以另一份同公式实现作为正确性判据。

| 检查 | 实测结果 |
|---|---:|
| 第 1 关：边界值及逐位 challenge | 69/69 |
| 第 2 关：全零、全一、符号位、32 位边界和交错位 | 8/8 |
| 第 3 关：边界值及全部 8 个分支 | 16/16 |
| 第 4 关：边界值及全部 128 个 target | 136/136 |
| 第 5 关：边界值及全部 32 种起点/步数组合 | 40/40 |
| 第 6 关：边界值及全部 64 种函数表索引组合 | 72/72 |
| 非公开 seed 的完整六关加 Honor | 128/128 |
| cap=0..256 的输出缓冲区契约，ASan + UBSan | 10794/10794 |

缓冲区检查覆盖七个求解接口、6 个不同的输入和每个容量：空间不足必须返回非零，成功时输出需要 NUL 结尾，输出区前后哨兵字节保持不变。检查使用独立的本机测试可执行文件，未改动课程测试或正式构建选项。以上结果不代表已经执行助教隐藏测试，也不等同于最终得分。

![公开测试和额外验证汇总截图](images/validation.jpg)

完整反汇编、逐项测试日志、验证辅助脚本和教学编译产物留在本机，不作为作业提交内容。

## 资料与工具使用情况

1. [课程实验说明](https://ics-26fall-fdu.github.io/labs/lab2-bomb-lab/)：环境、接口、评分与报告要求。
2. 本仓库 `README.md`、`AGENTS.md`、`tutor/README.md`、`defuser.h`、驱动及公开测试：协议与验证方式。仓库原有 AI 辅助约定不允许代写完整解法；本记录如实披露实际辅助范围，并不据此声称课程已允许完整代写。
3. GNU `objdump`：导出个人炸弹关键函数指令和只读数据表；GDB：验证可在关卡入口断点停止、观察参数，未修改炸弹的变量、寄存器或内存来绕过检查。
4. GCC、GNU Make、Python：编译、调用原始炸弹验证公开与额外输入，以及执行边界检查。
5. Codex：实施环境准备、反汇编分析、六关与 Honor 求解代码、额外测试以及报告撰写。Computer Use/浏览器工具：截取本地结果页中展示的真实运行输出。未将生成内容表述为学生独立完成的工作。
