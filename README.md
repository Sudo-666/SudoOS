# 进度
用 C 编写的一个符合 Limine 标准的小型 x86-64 内核，并使用 Limine 引导加载程序启动

- Limine 本质只是引导程序（bootloader），它的工作是：
    - 加载你的内核 ELF 文件（bin/sudoOS）
    - 切换到你指定的 CPU 状态（x86_64 long mode）
    - 为你准备好一些必要的信息（内存地图、帧缓冲、SMBIOS...）
    - 跳转到你的内核入口函数 kmain()

- 内核地址空间：0xffffffff80000000 ~ 0xffffffffffffffff


---
# 运行

```bash
make run
```

