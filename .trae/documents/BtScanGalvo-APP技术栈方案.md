# BtScanGalvo Android APP 技术栈方案

## Summary

为 `BtScanGalvo` 平台的蓝牙控制上位机确定技术栈，并给出从零搭建工程到可联调的落地步骤。决策已与用户确认：**Android 原生 + Kotlin + Jetpack Compose（Material 3）**，需要**实时形状预览**。通信沿用固件已定义的 HC-05 经典蓝牙 SPP 二进制帧协议，预览图形复刻固件 `Shape_Generate` 的算法确保一致性。

## 技术栈决策

| 环节 | 选型 | 理由 |
|------|------|------|
| 语言 | Kotlin | 官方一级语言，协程/Flow 支持好 |
| UI | Jetpack Compose (Material 3) | 声明式、代码量小，适合小工具 |
| 通信 | Android 经典蓝牙 SPP（`BluetoothSocket` + UUID `00001101-0000-1000-8000-00805F9B34FB`） | HC-05 即经典蓝牙透传串口，原生 API 最简 |
| 异步 | Kotlin Coroutines + Flow + StateFlow | 连接/IO 放后台，UI 响应式 |
| 架构 | MVVM（ViewModel + Repository + 单例连接管理器） | 轻量解耦，避免过度设计 |
| DI | 不用 Hilt，手动单例 | 功能少，避免引入额外依赖 |
| 预览 | Compose `Canvas` 复刻固件 `Shape_Generate` 算法 | 与固件点表一致，随形状/幅度实时变化 |
| SDK | minSdk 24 / target & compile 35 | 覆盖 Android 7.0+，兼容经典蓝牙 |

**不采用的方案**：iOS（HC-05 SPP 需 MFi，普通 App 不可用）；Flutter/Web（经典 SPP 支持受限）；Xml+View（放弃，Compose 已确定）。

## Current State Analysis

- 固件已完成：USART6 + HC-05(115200) 帧协议与扫描逻辑均已实现，协议见 `Firmware/README.md`；帧格式 `[SOF 0xAA][LEN][CMD][Payload][CRC8]`，CRC8 多项式 0x07。
- 命令集（`bt_protocol.h`）：0x01 切形状、0x02 幅度、0x03 启停/暂停、0x04 点间隔、0xF0 ACK；0x80 以上保留。
- 形状点表算法（`scan.c` `Shape_Generate`）：圆 256 点（角度均匀）、正方/长方各 256 点（每边 64），半摆幅 = 32767*amp/100，以 32768 为中心；点间隔默认 30us、下限 5us。
- `APP/` 目前仅有 `README.md`，源码未创建，git 已纳入管理。

## Proposed Changes

工程在 `APP/` 目录直接作为 Android Gradle 工程根（Kotlin DSL 单模块），保留并更新 `APP/README.md`。

1. **工程脚手架** `APP/`：`settings.gradle.kts`、根+app 的 `build.gradle.kts`（AGP 8.x + Compose compiler）、`gradle.properties`、`.gitignore`。app 模块 `minSdk 24 / targetSdk 35`。
2. **协议层** `app/src/main/java/com/bt/scangalvo/bluetooth/`：
   - `BtFrame.kt`：SOF/命令/ACK 常量。
   - `BtProtocol.kt`：CRC8(0x07)、`buildFrame`、`parseIncoming`（逐字节状态机，复刻 `BT_ParseByte`）。含 JVM 单元测试。
3. **连接层** `BtConnectionManager.kt`：经典 SPP `BluetoothSocket` 读写；Android 12+ 运行时权限（BLUETOOTH_SCAN/CONNECT）与旧版(ACCESS_FINE_LOCATION)兼容；连接/断开/IO 走协程，连接状态用 StateFlow 暴露；已配对设备列表 + 可选设备发现。
4. **数据层** `BtRepository.kt` + `model/ScanState.kt`：发送四条命令并等待对应 ACK（含超时/未知命令错误解析）。
5. **UI 层** `viewmodel/ScanViewModel.kt` + `ui/`：连接页（扫描/连接/断开）、控制面板（形状三选、幅度滑条、启停/暂停、点间隔输入）、`ShapePreview` Canvas（按当前形状+幅度绘制 256 点，可选动画高亮当前扫描点）。
6. **文档** `APP/README.md`：补充构建（Windows/Android Studio 环境要求与命令）、运行、打包步骤。

## Assumptions & Decisions

- 预览仅复刻形状几何（相对坐标），不映射 DAC 电压/物理尺寸；固件侧以实际 DAC 为准，预览仅做直观示意。
- ACK 只在并发单命令前提下做简单匹配（最近一条指令），固件当前无命令 ID，不做请求-响应配对。
- 经典蓝牙需先配对 HC-05；默认监听 PIN 1234 由系统配对处理，App 不做密码注入。
- 沿用 `APP/` 作为 Android 工程根目录，避免嵌套多层；README 保留在根。
- 遵循仓库规则：git 管理 + README 维护。

## 技能(Skill) 调用推荐

本任务与以下可用技能匹配，按阶段调用，避免重复探索、保证实现与项目规范一致：

| Skill | 触发阶段 | 用途 |
|-------|---------|------|
| `android-app-build-workflow` | 工程脚手架 → 协议层 → UI 层 → 构建全流程（**本任务主体技能**） | 初始化 Android 工程、配置 Gradle(AGP/Kotlin DSL)，编写 Kotlin/Compose 代码并编译出 APK，覆盖从零到可运行完整的 APK 构建链路 |
| `TRAE-code-review` | 代码就地完成后，若用户要求对 APP 的 MR/PR、commit 或差异做通用代码审查 | 审查 Kotlin/Compose 代码质量、正确性、可维护性、性能最佳实践 |
| `TRAE-security-review` | 若用户要求对蓝牙权限、SPP 收发做安全扫描/审计时 | 聚焦经典蓝牙权限声明、数据帧收发是否引入可利用风险 |

说明：`android-app-build-workflow` 在整个实现阶段都会被反复使用，每次进入「搭建/编写/编译」子步骤时触发；后两个审查类技能仅在用户明确要求时调用，不作为默认流程引入。

### 网络搜寻到的（外部）技能包推荐

以下技能包来自网络检索（Claude Code Skill / Agent Skill 格式，存放在 `.claude/skills/<name>/SKILL.md`，Windows 安装目录 `%USERPROFILE%\.claude\skills\`），可作为内置技能的补充，按实现阶段选择性引入：

| 技能包 | 来源/作者 | 覆盖内容 | 适配点 | 建议优先级 |
|--------|-----------|----------|--------|-----------|
| **claude-android-ninja** | Drjacky（MIT，27★，持续维护） | 模块化架构、Compose 状态/动画、Gradle 约定、数据同步、测试、性能 | 一套覆盖 Compose 与 Gradle 全流程 | 高 |
| **Chris Banes skills** | Google Android 组 Chris Banes | `compose-state-hoisting`、`kotlin-coroutines-structured-concurrency`、`compose-recomposition-performance` | 对应 MVVM+Flow 状态管理与协程连接层 | 高 |
| **compose-performance-skills** | Skydoves | Compose 重组性能、state 优化 | 形状预览性能 | 中 |
| **jetpack-compose-skills** | anhvt52（94★） | 编写/审查 Compose 代码、过时 API、Material3、navigation | UI 层编码基准 | 高 |
| **android-development** | dpconde（基于 NowInAndroid） | Google 官方架构、MVVM+UDF、Hilt、多模块 | 偏大型架构，本项目略重 | 中 |
| **android-claude-code-skills** | aihip（npm 1.9.0） | Android 变更审查、APK/项目架构分析 | 联调后的健壮性检查 | 低 |

推荐落地：以 **Chris Banes skills**（compose-state-hoisting + kotlin-coroutines-structured-concurrency，命中状态管理与蓝牙协程 IO 两大痛点）与 **claude-android-ninja**（整体架构/UI 基准）为主体；预览性能需要时再加 **Skydoves compose-performance-skills**。

安装方式（任选其一）：`npx skills add <git-url>`（推荐，免手动建目录），或 `git clone <git-url>` 到 `.claude/skills/`。安装后回填验证，若有命中再更新本节。

> 说明：外部技能为可选增强，不阻塞内置技能 `android-app-build-workflow` 的主流程；是否安装由工程负责人确认，安装前建议先审阅其 SKILL.md 与 license。

## Verification

1. 每次协议层改动跑 `./gradlew test`（覆盖 CRC8/组帧/解析，含 URL 900us 小端等边界）。
2. `./gradlew :app:assembleDebug` 出 APK 无错误。
3. 真机安装，与 HC-05 配对连接成功，连接状态正确。
4. 逐条下发 0x01~0x04，固件回 ACK 0（err=0），UI 状态与固件一致（形状/幅度/启停/间隔）。
5. 发送未知命令验证 err=1 错误提示。
6. 形状预览与固件实际扫描包络一致（圆为圆，方为方，幅度变化时预览同步缩放）。