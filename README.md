# Messager 局域网即时通信项目

基于 Qt QML/C++ 的局域网即时通信软件，无需中心服务器即可实现用户发现、私聊、群聊、文件传输和聊天记录持久化。

## 功能特性

- **基础功能**  
  用户登录、用户名读取、修改用户名、局域网用户发现、在线状态检测、用户搜索、私聊消息、历史记录、删除用户

- **群聊功能**  
  创建群聊、选择群成员、群消息、成员列表、退出群聊、解散群聊、保留和删除历史群聊

- **文件传输**  
  发送文件、接受或拒绝文件、图片预览、传输进度、实时速度、剩余时间、传输结果提示

- **文件**
  - 数据库：`Database` 目录
  - 网络通信：`IPMSG` 目录
  - 前端界面：`qml` 目录
  - 图片资源：`source` 目录
  - 需求及开发日志/文档：`work` 目录

## 部署要求

### 第三方依赖库

1. **Qt 6.11+** 框架组件：

   - Core
   - Gui
   - Qml
   - Quick
   - Sql
   - Svg
   - QuickDialogs2

2. **系统依赖**：
   - CMake 4.3.2+
   - 支持 C++23 的编译器
   - SQLite
   - POSIX Socket
   

### 安装 Qt 6.11

如果系统仓库中的 Qt 版本低于 6.11，请从官网下载安装：

```bash
# 下载 Qt 在线安装器
wget https://download.qt.io/official_releases/online_installers/qt-unified-linux-x64-online.run
chmod +x qt-unified-linux-x64-online.run

# 运行安装器（选择安装 Qt 6.11.1）
./qt-unified-linux-x64-online.run
```

### 系统环境要求

- Linux 系统（推荐 Manjaro/Arch）
- 推荐使用 X11 桌面
- 通信设备需要位于同一个局域网（只能在同一个网段）

---

## 安装部署指南（Manjaro/Arch Linux）

### 方案一

### 步骤 1：安装依赖库

```bash
sudo pacman -S base-devel cmake ninja sqlite
sudo pacman -S qt6-base qt6-declarative qt6-svg qt6-shadertools
```

### 步骤 2：编译项目

```bash
git clone https://github.com/zcw861/Messeager.git
cd Messeager

mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
ninja
```

如果系统 Qt 版本低于 6.11，需要使用自己安装的 Qt：

```bash
# 根据实际安装路径修改
export PATH="/opt/Qt/6.11.1/gcc_64/bin:$PATH"
export LD_LIBRARY_PATH="/opt/Qt/6.11.1/gcc_64/lib:$LD_LIBRARY_PATH"
export CMAKE_PREFIX_PATH="/opt/Qt/6.11.1/gcc_64:$CMAKE_PREFIX_PATH"

# 设置环境变量后重新配置和编译
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc)
```

### 步骤 3：安装到系统

```bash
sudo ninja install 
echo "/opt/Messager/lib" | sudo tee /etc/ld.so.conf.d/messager.conf
sudo ldconfig
```

### 步骤 4：更新桌面数据库

```bash
sudo update-desktop-database
sudo gtk-update-icon-cache /usr/share/icons/hicolor
```

### 步骤 5：终端运行
打开终端输入messager就可以启动了

### 方案二（推荐）

### 步骤 1：下载 messager-bin-0.1-3-x86_64.pkg.tar.zst （见Releases）
```bash
# 进入压缩包所在目录，打开终端（确认当前路径是压缩包所在路径）
pacman -U messager-bin-0.1-3-x86_64.pkg.tar.zst 
```
### 步骤二
打开系统菜单栏搜索messager即可看到该项目

## 注意事项

Messager 使用 UDP 广播发现局域网用户，请确保设备位于同一个局域网，并且路由器没有开启 AP 隔离。

文件传输所保存的路径在/root/.local/share/se.qt.messager/se.qt.messager/data（需要打开隐藏按钮才能看到.loacl目录）。其中cache目录缓存发送的图片，download目录是保存接收文件的地方


项目使用以下端口：

- UDP 45454：用户发现和群聊邀请
- TCP 45455：私聊和群聊消息
- TCP 45456：文件传输

如果无法发现用户或发送消息，请检查防火墙是否允许这些端口。

VPN、FlClash、Clash 或其他代理软件可能使程序读取到虚拟网卡 IP，导致局域网通信失败。出现该问题时，可以关闭代理软件的 TUN 模式或调整网络接口优先级。

如果使用自己安装的 Qt，运行程序前需要设置动态库路径：

```bash
export LD_LIBRARY_PATH="/opt/Qt/6.11.1/gcc_64/lib:$LD_LIBRARY_PATH"
messager
```
