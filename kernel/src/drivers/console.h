#pragma once
#include <stdint.h>
#include "../limine.h"

// 初始化显卡
void console_init(struct limine_framebuffer* fb);

// 核心接口
void kprint(const char* str);       // 打印字符串
void kprintln(const char* str);     // 打印并换行
void kprint_int(int val);           // 打印整数
void kclear();                      // 清屏
void kprint_char(char c);           // 打印单个字符
void kprint_uint64(uint64_t val);   // 打印64位整数