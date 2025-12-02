#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

/**
 * @brief 声明limine版本号
 *
 */
__attribute__((used, section(".limine_requests"))) static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

/**
 * @brief 请求framebuffer显存
 *
 */

__attribute__((used, section(".limine_requests"))) static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0};

/**
 * @brief  声明limine请求区头尾，使得limine在内核运行前处理请求区的所有请求
 *
 */

__attribute__((used, section(".limine_requests_start"))) static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

/**
 * @brief 将src开始的n个字节复制到dest处
 *
 * @param dest
 * @param src
 * @param n
 * @return void*
 */
void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++)
    {
        pdest[i] = psrc[i];
    }
    return dest;
}

/**
 * @brief 将src开始的n个字节全都赋值成c
 *
 * @param src
 * @param c
 * @param n
 * @return void*
 */
void *memset(void *src, int c, size_t n)
{
    uint8_t *p = (uint8_t *)src;
    for (size_t i = 0; i < n; i++)
    {
        p[i] = (uint8_t)c;
    }
    return src;
}

/**
 * @brief 将src开始的n个字节移动到dest处，允许内存重叠
 *
 * @param dest
 * @param src
 * @param n
 * @return void*
 */
void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
    if (dest < src)
    {
        for (size_t i = 0; i < n; i++)
        {
            pdest[i] = psrc[i];
        }
    }
    else if (dest > src)
    {
        for (size_t i = n; i > 0; i--)
        {
            pdest[i-1] = psrc[i-1];
        }
    }
    return dest;
}

/**
 * @brief 逐字节比较两块内存区域s1,s2开始的n个字节的内容。
 *
 * @param s1
 * @param s2
 * @param n
 * @return int :0 (s1==s2)；-1 (s1<s2)；1 (s1>s2)
 *
 */
int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *ps1 = (const uint8_t *)s1;
    const uint8_t *ps2 = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++)
    {
        if (ps1[i] != ps2[i])
        {
            return ps1[i] < ps2[i] ? -1 : 1;
        }
    }
    return 0;
}


/**
 * @brief  让 CPU 完全停止
 * 
 */
static void hcf(void)
{
    for(;;) {
        asm("hlt");
    }
}

/**
 * @brief 入口
 * 
 */
void kmain(void) {
    
    // 确保版本正确
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    // 确保获取到显存
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // 获取到第一个显存信息
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    // 随便画点东西
    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }

    // We're done, just hang...
    hcf();

}