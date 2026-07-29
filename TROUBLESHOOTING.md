# 常见问题排查 (Troubleshooting)

## 驱动加载失败：`no symbol version for module_layout`

### 错误现象

在开发板上执行 `insmod dht11_drv.ko` 时反复出现：

```
dht11_drv: no symbol version for module_layout
insmod: ERROR: could not insert module dht11_drv.ko: Invalid module format
```

即使 `vermagic` 与开发板 `uname -r` 完全一致，依然报错。

### 根本原因

**1. 内核符号版本校验（CONFIG_MODVERSIONS）**

开发板内核启用了 `CONFIG_MODVERSIONS=y`，要求模块携带正确的符号 CRC 校验值。主机编译驱动时，没有生成或使用正确的 `Module.symvers`（符号版本描述文件），导致模块的 `__versions` 段缺失或内容不匹配。

**2. 编译环境的版本不匹配**

内核源码树带有 `.git` 目录且 `CONFIG_LOCALVERSION_AUTO` 开启，导致编译生成的 `kernel.release` 自动追加了 `-dirty` 后缀，与开发板 `4.1.15-ge48931b1` 不符。即使去掉 `-dirty`，若 `Module.symvers` 缺失或不完整，仍无法通过符号校验。

**3. 开发板加载了旧的模块文件**

多次尝试中，SD 卡或 `/root` 下的 `.ko` 文件是早期编译的旧文件（大小仅 8.2KB），未更新为新编译的完整模块（10KB），导致重复报错。

### 解决步骤

#### 步骤 1：消除版本后缀 `-dirty`

```bash
cd ~/Desktop/zhengdian

# 删除 .git 文件夹，避免 git 检测到未跟踪文件
rm -rf .git

# 禁用自动版本检测
scripts/config --disable CONFIG_LOCALVERSION_AUTO

# 手动设置本地版本以匹配开发板
scripts/config --set-str CONFIG_LOCALVERSION "-ge48931b1"

# 更新配置并准备模块编译
make olddefconfig
make modules_prepare
```

此时 `kernel.release` 应变为 `4.1.15-ge48931b1`（无 dirty）。

#### 步骤 2：生成完整的 Module.symvers

编译完整内核，生成包含所有符号及其 CRC 校验的 `Module.symvers`：

```bash
make zImage -j$(nproc)
make modules -j$(nproc)
```

完成后检查 `Module.symvers` 大小，应从 **23KB** 增至 **~470KB**。

#### 步骤 3：重新编译驱动

```bash
cd ~/Desktop/02_dht11_drv_imx6ull
make clean
make
```

检查新生成的 `dht11_drv.ko`：

```bash
modinfo dht11_drv.ko
# 确认 vermagic 中包含 modversions 且版本号匹配
ls -l dht11_drv.ko  # 应为 ~10KB
```

#### 步骤 4：更新开发板上的模块文件

将新编译的 `.ko` 通过 `scp` 直接传到开发板 `/root/`：

```bash
scp dht11_drv.ko root@<开发板IP>:/root/
```

**不要**经过旧 SD 卡文件，确保加载的是最新模块。

#### 步骤 5：加载与测试

```bash
# 开发板端
insmod /root/dht11_drv.ko

# 查看加载日志
dmesg | tail

# 创建设备节点（主设备号以实际为准）
cat /proc/devices | grep dht11
mknod /dev/mydht11 c <主设备号> 0

# 运行测试程序
./dht11_test /dev/mydht11
```

### 关键命令回顾

```bash
# ---------- 主机端 ----------
cd ~/Desktop/zhengdian
rm -rf .git
scripts/config --disable CONFIG_LOCALVERSION_AUTO
scripts/config --set-str CONFIG_LOCALVERSION "-ge48931b1"
make olddefconfig
make modules_prepare
make zImage -j$(nproc)          # 生成完整 Module.symvers
make modules -j$(nproc)

cd ~/Desktop/02_dht11_drv_imx6ull
make clean
make
scp dht11_drv.ko root@开发板IP:/root/

# ---------- 开发板端 ----------
insmod /root/dht11_drv.ko
cat /proc/devices | grep dht11
mknod /dev/mydht11 c <主设备号> 0
/root/dht11_test /dev/mydht11
```

### 经验教训

- 嵌入式驱动开发中，模块与内核的版本一致性**不仅看 `uname -r`**，还要关注符号版本校验（`CONFIG_MODVERSIONS`）和 `Module.symvers` 的生成
- 源码树中的 `.git` 目录会干扰版本检测，若无需版本控制可删除
- 每次更新驱动后，务必确认开发板加载的是**最新编译**的 `.ko` 文件，可通过 `ls -l` 对比大小或时间戳
