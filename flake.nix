{
  description = "SudoOS Development Environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        # 开发环境名称
        name = "sudoos-dev-shell";

        # 这里列出所有需要的软件包
        buildInputs = with pkgs; [
          # 基础构建工具
          gnumake
          gcc          # Makefile 默认使用 cc
          binutils     # 包含 ld, objcopy 等
          nasm         # 汇编器

          # 镜像制作工具
          xorriso      # 制作 ISO
          mtools       # 制作 HDD (mcopy, mmd...)
          gptfdisk     # 制作 HDD (sgdisk)

          # 下载与版本控制
          git
          curl
          gnutar
          gzip

          # 仿真器
          qemu         # qemu-system-x86_64
        ];

        # Shell 启动时的 Hook，可以在这里打印欢迎信息或设置环境变量
        shellHook = ''
          echo "==========================================="
          echo "Welcome to SudoOS Development Environment!"
          echo "Tools loaded: GCC, NASM, QEMU, XORRISO..."
          echo "Run 'make' to build."
          echo "Run 'make run' to launch QEMU."
          echo "==========================================="
        '';
      };
    };
}