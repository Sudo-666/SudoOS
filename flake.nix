{
  description = "SudoOS Development Environment";

  inputs = {
    nixpkgs.url = "git+https://mirrors.tuna.tsinghua.edu.cn/git/nixpkgs.git?ref=nixos-25.11&shallow=1";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      
      # 【新增】获取真正的交叉编译工具链
      crossPkgs = pkgs.pkgsCross.x86_64-embedded;
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        name = "sudoos-dev-shell";
        
        # 将 buildInputs 中的 gcc/binutils 替换为交叉编译版本
        buildInputs = with pkgs; [
          gnumake
          # 移除 pkgs.gcc, pkgs.binutils
          
          # 【新增】使用交叉编译器
          crossPkgs.buildPackages.gcc
          crossPkgs.buildPackages.binutils
          crossPkgs.buildPackages.gdb
          
          nasm
          xorriso
          mtools
          gptfdisk
          git
          curl
          gnutar
          gzip
          qemu
        ];

        shellHook = ''
          echo "\n"
          echo "==========================================="
          echo "Welcome to SudoOS Development Environment!"
          echo "Checking toolchain..."
          # 验证是否在使用交叉编译器
          which x86_64-elf-gcc || echo "Warning: x86_64-elf-gcc not found, strictly checking makefile..."
          echo "==========================================="
          
          # 强制设置环境变量，确保 Makefile 能识别
          export TOOLCHAIN=x86_64-elf
          export CC=x86_64-elf-gcc
          export LD=x86_64-elf-ld
        '';
      };
    };
}