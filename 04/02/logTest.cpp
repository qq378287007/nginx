#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //size_t write(int fd,const void *buf,size_t nbytes);

#define NGX_MAX_ERROR_STR 2048

// 类似memcpy
// memcpy返回dst指针
// ngx_cpymem返回dst+n位置
#define ngx_cpymem(dst, src, n) (((u_char *)memcpy(dst, src, n)) + (n))

// sizeof含'\0'，strlen不含'\0'
#define NGX_UINT64_LEN (strlen("18446744073709551615")) // 2^64-1

// 数值ui64转为字符串，并写入buf中，右对齐（左填充）
static u_char *ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64, u_char zero, uintptr_t hexadecimal, uintptr_t width)
{
    static u_char hex[] = "0123456789abcdef"; //%xd格式符有关
    static u_char HEX[] = "0123456789ABCDEF"; //%Xd格式符有关
    u_char temp[NGX_UINT64_LEN + 1];
    u_char *p;
    u_char *temp_last;
    size_t len;

    temp_last = temp + NGX_UINT64_LEN; // temp_last指向temp最后一个元素位置(不使用)

    p = temp_last;
    // 十进制数1234567对应十六进制数12D687
    if (hexadecimal == 0)
    { // 十进制%d
        do
        {
            *--p = (u_char)(ui64 % 10 + '0'); // ui64 % 10 + '0'分别得到10进制数7,6,5,4,3,2,1
        } while (ui64 /= 10);
    }
    else if (hexadecimal == 1)
    { // 十六进制%xd
        do
        {
            *--p = hex[(uint32_t)(ui64 & 0xf)]; // ui64 & 0xf分别得到16进制数7,8,6,D,2,1
        } while (ui64 >>= 4);                   // 右移最末尾的4个二进制位
    }
    else
    { // hexadecimal == 2    //十六进制%Xd
        do
        {
            *--p = HEX[(uint32_t)(ui64 & 0XF)];
        } while (ui64 >>= 4);
    }

    // 得到数字宽度，比如strlen("7654321") = 7
    len = temp_last - p;

    // 若%12d，而实际7654321，需要填充5个zero
    // 若width小于实际数字宽度，width失效
    while (buf < last && len++ < width)
        *buf++ = zero; // 填充zero到buffer中

    // 还原数字实际宽度len
    len = temp_last - p;

    if (last - buf < len)
        len = last - buf; // len取剩余空间和数字宽度间最小值

    // 返回最新buf
    return ngx_cpymem(buf, p, len);
}

// 按照格式fmt，解析可变参数args，并生成字符串写入buf
u_char *ngx_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args)
{
    u_char zero; // 填充字符'0'或' '

    // 临时变量
    uintptr_t width;      // 数值宽度
    uintptr_t sign;       // 符号位
    uintptr_t hex;        // 16进制
    uintptr_t frac_width; // 小数点后数字宽度
    uintptr_t scale;      // 小数部分缩放倍数
    uintptr_t n;          // 小数部分循环计数

    int64_t i64;   // 保存%d对应的可变参
    uint64_t ui64; // 保存%ud对应的可变参，和%f对应可变参的整数部分
    u_char *p;     // 保存%s对应的可变参
    double f;      // 保存%f对应的可变参
    uint64_t frac; // 保存%f对应的可变参的小数部分

    while (*fmt && buf < last)
    { // 单字符循环
        if (*fmt == '%')
        { //%开头字符串需要被可变参数替换

            //-----------------变量初始化工作开始-----------------
            zero = (u_char)((*++fmt == '0') ? '0' : ' '); //%0
            width = 0;                                    // 对%d，%f有效，%6d
            sign = 1;                                     //%d，有符号
            hex = 0;                                      // 0：10进制，1：16进制小写字母，2：16进制大写字母
            frac_width = 0;                               //%.10f
            i64 = 0;                                      //%d解析可变参得到数值
            ui64 = 0;                                     //%ud解析可变参得到数值
            //-----------------变量初始化工作结束-----------------

            while (*fmt >= '0' && *fmt <= '9') //%后整数部分，比如%16
                width = width * 10 + (*fmt++ - '0');

            while (1)
            {
                switch (*fmt)
                {             // 处理%后特殊字符
                case 'u':     // 无符号
                    sign = 0; // 标记无符号数
                    fmt++;    // 往后走一个字符
                    continue;
                case 'X':    //%X，大写十六进制，一般是%Xd
                    hex = 2; // 标记十六进制大写字母显示
                    sign = 0;
                    fmt++;
                    continue;
                case 'x':    //%x，小写十六进制，一般是%Xd
                    hex = 1; // 标记十六进制小写字母显示
                    sign = 0;
                    fmt++;
                    continue;
                case '.': // 其后边必须跟个数字，必须与%f配合使用，形如%.10f
                    fmt++;
                    // 得到%.后小数部分数字，比如%.16
                    while (*fmt >= '0' && *fmt <= '9')
                        frac_width = frac_width * 10 + (*fmt++ - '0');
                    break;
                default:
                    break;
                }
                break;
            }

            switch (*fmt)
            {
            case '%': //%%
                *buf++ = '%';
                fmt++;
                continue;
            case 'd':     // 显示整型数据，如果和u配合使用，也就是%ud,则是显示无符号整型数据
                if (sign) // 如果是有符号数
                    i64 = (int64_t)va_arg(args, int);
                // va_arg():遍历可变参数，var_arg的第二个参数表示遍历的这个可变的参数的类型
                //( (int)((args += _INTSIZEOF(int)) - _INTSIZEOF(int)) )
                // 取出当前args指向的值，并使args指向下一个参数。
                else //%ud，%xd，%Xd
                    ui64 = (uint64_t)va_arg(args, u_int);

                // 显示数字保存ui64
                if (sign)
                { // 有符号数
                    if (i64 < 0)
                    {                          // 这可能是和%d格式对应的要显示的数字
                        *buf++ = '-';          // 小于0，自然要把负号先显示出来
                        ui64 = (uint64_t)-i64; // 变成无符号数（正数）
                    }
                    else // 显示正数
                        ui64 = (uint64_t)i64;
                }
                // 把一个数字 比如“1234567”弄到buffer中显示，如果是要求10位，则前边会填充3个空格比如“   1234567”
                // 注意第5个参数hex，是否以16进制显示，比如如果你是想以16进制显示一个数字则可以%Xd或者%xd，此时hex = 2或者1
                buf = ngx_sprintf_num(buf, last, ui64, zero, hex, width); // 存在bug，可能显示-   1234567
                fmt++;
                continue;
            case 's': // 显示字符串
                p = va_arg(args, u_char *);
                while (*p && buf < last)
                    *buf++ = *p++;
                fmt++;
                continue;
            case 'P':
                i64 = (int64_t)va_arg(args, pid_t);
                if (i64 < 0)
                {
                    *buf++ = '-';
                    ui64 = (uint64_t)-i64;
                }
                else
                    ui64 = (uint64_t)i64;
                buf = ngx_sprintf_num(buf, last, ui64, zero, hex, width); // 存在bug，可能显示-   1234567
                fmt++;

                break; // 此处break和continue等价，break跳出switch，continue继续下一次while循环
            case 'f':  // 显示double类型数据，如果要显示小数部分，则要形如 %.5f
                f = va_arg(args, double);
                if (f < 0)
                {                 // 负数的处理
                    *buf++ = '-'; // 单独搞个负号出来
                    f = -f;       // 那这里f应该是正数了!
                }
                // f >= 0
                ui64 = (int64_t)f; // 整数部分给到ui64里
                frac = 0;

                // 如果要求小数点后显示多少位小数
                if (frac_width)
                {              // 如果是%d.2f，那么frac_width就会是这里的2
                    scale = 1; // 缩放从1开始
                    for (n = 0; n < frac_width; n++)
                        scale *= 10; // 这可能溢出哦

                    // 取出小数部分，若%.2f，对应参数12.537
                    //(uint64_t) ((12.537 - (double) 12) * 100 + 0.5);
                    //= (uint64_t) (0.537 * 100 + 0.5)
                    //= (uint64_t) (53.7 + 0.5)
                    //= (uint64_t) (54.2)
                    //= 54
                    frac = (uint64_t)((f - (double)ui64) * scale + 0.5); // 取得保留的小数位数

                    //%.2f，对应参数12.999
                    //(uint64_t) (0.999 * 100 + 0.5)
                    //= (uint64_t) (99.9 + 0.5)
                    //= (uint64_t) (100.4)
                    //= 100
                    // 此时scale == 100
                    if (frac == scale)
                    {             // 进位
                        ui64++;   // 正整数部分进位
                        frac = 0; // 小数部分归0
                    }
                }

                // 正整数部分
                buf = ngx_sprintf_num(buf, last, ui64, zero, 0, width); // 把一个数字 比如“1234567”弄到buffer中显示

                if (frac_width)
                { // 小数部分
                    if (buf < last)
                        *buf++ = '.';
                    buf = ngx_sprintf_num(buf, last, frac, '0', 0, frac_width); // frac是小数部分，前边填充'0'字符，bug
                }
                fmt++;
                continue;
            default:
                *buf++ = *fmt++; // 往下移动一个字符
                continue;
            }
        }
        else
        {
            *buf++ = *fmt++; // 非'%'字符赋值
        }
    }

    return buf;
}

static u_char *ngx_log_errno(u_char *buf, u_char *last, int err)
{
    // 左边字符串
    char leftstr[10] = {0};
    sprintf(leftstr, " (%d: ", err);
    size_t leftlen = strlen(leftstr);

    // 中间字符串
    char *perrorinfo = strerror(err); // 根据资料不会返回NULL;
    size_t len = strlen(perrorinfo);

    // 右边字符串
    char rightstr[] = ") ";
    size_t rightlen = strlen(rightstr);

    // 左右的额外宽度
    size_t extralen = leftlen + rightlen;

    // 保证全装下，否则抛弃
    // nginx做法是若位置不够，填充最右边位置【即使覆盖掉以往的有效内容】
    if (buf + len + extralen <= last)
    {
        buf = ngx_cpymem(buf, leftstr, leftlen);
        buf = ngx_cpymem(buf, perrorinfo, len);
        buf = ngx_cpymem(buf, rightstr, rightlen);
    }
    return buf;
}

void ngx_log_stderr(int err, const char *fmt, ...)
{
    va_list args;
    u_char errstr[NGX_MAX_ERROR_STR + 1];
    u_char *p, *last;

    memset(errstr, 0, sizeof(errstr)); // 清空

    // 最末位置，放置'\0'，不允许使用
    // 最后有效位置last-1
    last = errstr + NGX_MAX_ERROR_STR;

    p = ngx_cpymem(errstr, "nginx: ", strlen("nginx: "));

    va_start(args, fmt); // args指向起始的参数地址
    // args = (va_list)&fmt + _INTSIZEOF(fmt)
    // args指向fmt后面参数的地址，即第一个可变参数地址。

    p = ngx_vslprintf(p, last, fmt, args); // 组合字符串写入errstr

    va_end(args); // 释放args

    if (err)                             // 错误发生
        p = ngx_log_errno(p, last, err); // 显示错误代码和错误信息

    // 若位置不够，最后位置插入换行符，哪怕覆盖到其他内容
    if (p >= last)
        p = last - 1;
    *p++ = '\n'; // 增加换行符

    printf("%s", errstr);                     // 输出信息
    write(STDERR_FILENO, errstr, p - errstr); // 标准错误【一般屏幕】输出信息
}

#ifdef TEST_LOGTEST
int main()
{
    ngx_log_stderr(0, "invalid option = %010d", 12);
    ngx_log_stderr(2, "invalid option = %010d", 12);
    return 0;
}
#endif

/*
g++ logTest.cxx -DTEST_LOGTEST -o logTest
./logTest
*/
