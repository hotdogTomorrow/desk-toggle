# DeskToggle

一个轻量的 Windows 11 桌面图标显隐工具：**双击桌面空白处隐藏图标，再双击恢复**。

C++ / Win32 原生程序，无安装器，无额外运行时依赖，无联网、遥测、广告和自动更新。

## 下载

**普通用户下载后即可运行，无需编译或安装开发工具。**

| 电脑类型 | 直接运行 | 含说明的便携包 |
| --- | --- | --- |
| 大多数 Intel／AMD Windows 11 电脑 | [下载 x64 EXE](https://github.com/hotdogTomorrow/desk-toggle/releases/latest/download/DeskToggle-x64.exe) | [下载 x64 ZIP](https://github.com/hotdogTomorrow/desk-toggle/releases/latest/download/DeskToggle-x64.zip) |
| Windows ARM 电脑 | [下载 ARM64 EXE](https://github.com/hotdogTomorrow/desk-toggle/releases/latest/download/DeskToggle-ARM64.exe) | [下载 ARM64 ZIP](https://github.com/hotdogTomorrow/desk-toggle/releases/latest/download/DeskToggle-ARM64.zip) |

下载 EXE 后放在固定文件夹中，双击运行；ZIP 则先解压，再运行里面的 `DeskToggle.exe`。无需管理员权限或额外运行库。

[查看全部版本与更新说明](https://github.com/hotdogTomorrow/desk-toggle/releases)。Release 页自动附带的 `Source code` 是源码包，普通用户请选择 `DeskToggle-*.exe` 或 `DeskToggle-*.zip`。

## 使用

1. 将下载的 EXE（或 ZIP 中的 `DeskToggle.exe`）放在一个固定文件夹中，双击运行。无需管理员权限。
2. 双击桌面空白处，隐藏或显示桌面上的全部图标。双击图标本身仍由 Windows 正常处理。
3. 右键任务栏通知区域中的蓝色显示器图标，可以切换显隐、恢复图标、开启／关闭开机自启或退出。

托盘图标可能被 Windows 放在通知区域的折叠菜单中。左键点击图标也会打开菜单。

- 自启默认关闭，勾选后在**当前用户登录后**启动；不会注册系统服务或计划任务。
- 修改自启后无需保持设置窗口开启。移动程序文件后，请关闭再开启自启以更新路径。
- Windows「设置 → 应用 → 启动」和任务管理器可以另外禁用启动项；本程序的勾选状态表示注册表中是否存在自己的启动项。系统禁用的项目需要在系统设置中重新允许。
- 双击间隔和位置容差遵循系统设置。拖动框选、右键、滚轮及按住 Ctrl／Shift／Alt／Win 的点击不会触发切换。
- 隐藏只改变桌面的显示状态，不移动、删除或修改文件。
- 正常退出会恢复**本程序在本次运行中隐藏的图标**；启动时不会主动更改原有显隐状态。
- 重复运行会提示已有实例，不重复常驻。

如异常终止后图标仍隐藏，可以在桌面右键 → 查看 → 显示桌面图标中恢复，或者执行：

```powershell
.\DeskToggle.exe --show
```

另有 `--toggle`（切换系统图标显隐）和 `--quit`（退出常驻实例）命令。

## 资源占用设计

- 使用 `GetMessage` 阻塞等待事件，没有定时轮询、忙循环、动画或内存清理定时器。
- 使用异步 Raw Input，不注入 Explorer，不使用会同步阻塞系统鼠标输入的全局鼠标钩子。
- 只注册鼠标，不注册键盘输入。鼠标移动且未按住候选点击时，直接返回，不查桌面、不读光标、不调用 COM、不分配内存。
- 只有点击时判定是否命中桌面空白；图标显隐使用 Shell 的 `IFolderView2` 接口，只修改 `FWF_NOICONS` 标志。
- 不长期缓存 Explorer 的 COM 代理；切换时重新取得视图，避免 Explorer 重启后的旧引用。托盘响应 `TaskbarCreated` 重建。
- Release 静态链接 C/C++ 运行库，程序仅依赖 Windows 系统组件；无网页运行环境和自建后台线程。

Raw Input 仍会收到鼠标移动消息，所以持续移动高回报率鼠标时不应承诺绝对零 CPU。工作集含 Windows 共享 DLL 页面，和私有内存是不同指标；首次打开菜单或调用 Shell 后数值也会变化。

运行后可采样 CPU 增量、工作集和私有内存：

```powershell
.\scripts\Measure-Idle.ps1 -Seconds 15
```

`CpuPercentOfOneCore` 以一个逻辑核心为 100%，任务管理器通常按整机核心数归一化。单次零增量不等于永远零占用。采样时长、交互阶段和硬件条件需要一起比较。

## 构建

需要 Visual Studio 2022 Build Tools 的「使用 C++ 的桌面开发」、Windows SDK、CMake 3.24 或更新版本。运行无需安装 Visual Studio 或 VC++ Redistributable。

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

产物：`build/Release/DeskToggle.exe`。ARM64 可将配置命令改成 `-A ARM64`，并安装相应的 MSVC ARM64 构建工具。

运行 `scripts/Package.ps1 -Architecture x64` 可生成 `dist/DeskToggle-x64.exe`、`dist/DeskToggle-x64.zip` 和各自的 SHA-256 校验文件；脚本会核对 EXE 的实际架构，避免把不同架构误打包。

GitHub Actions 会编译 x64 与 ARM64，在 x64 上运行手势规则测试，分别生成独立 EXE 和只包含 EXE、README、许可证的便携 ZIP。开发者测试程序不会打包，Release 不包含 PDB 或本机调试路径。推送 `v*` 版本标签后，只有两种架构都构建成功且文件校验通过，才会自动创建带下载附件的 GitHub Release；版本说明放在 `docs/releases/<标签>.md`。

图标是项目自己的代码绘制资源，修改后可用 `scripts/New-Icon.ps1` 重建，无第三方图片依赖。

## 验证

自动手势测试覆盖正常双击、单击、连续点击、慢速点击、图标点击、拖拽后返回原位、负坐标、不同鼠标设备、时间戳回绕和状态重置。

以下开发者探针会短暂切换系统状态，随后恢复原状态；请仅在有交互式桌面的本地环境运行：

```powershell
.\build\Release\shell_probe.exe --roundtrip
.\build\Release\shell_probe.exe --startup-roundtrip
```

人工验证清单见 [docs/verification.md](docs/verification.md)。CI 的纯手势测试不能代替真实鼠标、实际登录启动、多显示器与 Explorer 重启测试。

目标环境为 Windows 11 的标准 Explorer 桌面。替代桌面、动态壁纸、远程桌面和第三方 Shell 的窗口结构或输入来源可能不同，需要单独验证。

## 隐私与开源

应用不联网，不创建日志，不保存鼠标轨迹，不读取桌面文件内容。点击命中测试仅通过系统辅助功能接口区分桌面空白与图标，不请求文件名称。

开启自启时，程序将带引号的自身完整路径保存到本机当前用户的 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 下的 `DeskToggle` 值；关闭时只删除该值。这一本机设置不会写入仓库。

`.gitignore` 排除了构建目录、调试符号、本机配置、日志、转储和验证产物。发布前还应检查待提交文件、二进制字符串和 Git 作者邮箱，避免提交个人路径、桌面截图、凭据或非公开邮箱。

MIT License，见 [LICENSE](LICENSE)。
