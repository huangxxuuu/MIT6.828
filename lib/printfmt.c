// Stripped-down primitive printf-style formatting routines,
// used in common by printf, sprintf, fprintf, etc.
// This code is also used by both the kernel and user programs.

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/stdarg.h>
#include <inc/error.h>

/*
 * Space or zero padding and a field width are supported for the numeric
 * formats only.
 *
 * The special format %e takes an integer error code
 * and prints a string describing the error.
 * The integer may be positive or negative,
 * so that -E_NO_MEM and E_NO_MEM are equivalent.
 */

static const char * const error_string[MAXERROR] =
{
	[E_UNSPECIFIED]	= "unspecified error",
	[E_BAD_ENV]	= "bad environment",
	[E_INVAL]	= "invalid parameter",
	[E_NO_MEM]	= "out of memory",
	[E_NO_FREE_ENV]	= "out of environments",
	[E_FAULT]	= "segmentation fault",
};

/*
 * Print a number (base <= 16) in reverse order,
 * using specified putch function and associated pointer putdat.
 */
static void
printnum(void (*putch)(int, void*), void *putdat,
	 unsigned long long num, unsigned base, int width, int padc)
{
	// 通过递归调用来先打印高位字符
	// first recursively print all preceding (more significant) digits
	if (num >= base) {
		printnum(putch, putdat, num / base, base, width - 1, padc);
	} else {
		// print any needed pad characters before first digit
		while (--width > 0)
			putch(padc, putdat);
	}

	// then print this (the least significant) digit
	putch("0123456789abcdef"[num % base] | (padc & 0xff00 ), putdat);
}

// Get an unsigned int of various possible sizes from a varargs list,
// depending on the lflag parameter.
static unsigned long long
getuint(va_list *ap, int lflag)
{
	if (lflag >= 2)
		return va_arg(*ap, unsigned long long);
	else if (lflag)
		return va_arg(*ap, unsigned long);
	else
		return va_arg(*ap, unsigned int);
}

// Same as getuint but signed - can't use getuint
// because of sign extension
static long long
getint(va_list *ap, int lflag)
{
	if (lflag >= 2)
		return va_arg(*ap, long long);
	else if (lflag)
		return va_arg(*ap, long);
	else
		return va_arg(*ap, int);
}


// Main function to format and print a string.
void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);

void
vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap)
{
	register const char *p;
	register int ch, err;
	unsigned long long num;
	int base, lflag, width, precision, altflag; // lflag反映要打印的数字的类型 int long (long long)
	int attri = 0;
	char padc;

	while (1) {
		// 不是%，意味着不是格式化控制字符，直接输出
		while ((ch = *(unsigned char *) fmt++) != '%') {
			if (ch == '\0')
				return;
			putch(ch | attri, putdat);
		}
		// 格式化控制字符对应的控制格式处理部分。
		// 因为上面有fmt++。所以现在的位置是%的下一个字符
		// Process a %-escape sequence
		padc = ' ';
		width = -1;
		precision = -1;
		lflag = 0;
		altflag = 0;
	reswitch:
		switch (ch = *(unsigned char *) fmt++) {

		// flag to pad on the right
		case '-':
			padc = '-';
			goto reswitch;

		// flag to pad with 0's instead of spaces
		case '0':
			padc = '0';
			goto reswitch;

		// width field
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			for (precision = 0; ; ++fmt) {
				precision = precision * 10 + ch - '0';
				ch = *fmt; // 给ch赋 ++fmt后对应的字符
				if (ch < '0' || ch > '9')
					break; // 这里跳出时ch与fmt是对应的
			}
			goto process_precision;

		case '*':
			precision = va_arg(ap, int);
			goto process_precision;

		case '.':
			if (width < 0)
				width = 0;
			goto reswitch;

		case '#':
			altflag = 1;
			goto reswitch;

		process_precision:
			if (width < 0)
				width = precision, precision = -1;
			goto reswitch;

		// long flag (doubled for long long)
		case 'l':
			lflag++;
			goto reswitch;

		// character
		case 'c':
			putch(va_arg(ap, int) | attri, putdat);
			break;

		// error message
		case 'e':
			err = va_arg(ap, int);
			if (err < 0)
				err = -err;
			if (err >= MAXERROR || (p = error_string[err]) == NULL) // 不在错误列表中有解释字符串的，直接打印错误号
				printfmt(putch, putdat, "error %d", err);
			else
				printfmt(putch, putdat, "%s", p);
			break;

		// string
		case 's':
			if ((p = va_arg(ap, char *)) == NULL)
				p = "(null)";
			if (width > 0 && padc != '-')
				// strnlen 统计字符串p的长度。返回长度与precision的较小值
				for (width -= strnlen(p, precision); width > 0; width--) // 计算左侧的空格在左侧填充padc
					putch(padc | attri, putdat);
			for (; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--)
				if (altflag && (ch < ' ' || ch > '~')) // ascii 可打印字符的第一个和最后一个。超出这个范围打印'?'
					putch('?' | attri, putdat);
				else
					putch(ch | attri, putdat);
			for (; width > 0; width--) // 若字符串打印完width仍有剩余，填充空格
				putch(' ' | attri, putdat);
			break;

		// (signed) decimal
		case 'd':
			num = getint(&ap, lflag);
			if ((long long) num < 0) {
				putch('-' | attri, putdat);
				num = -(long long) num;
			}
			base = 10;
			goto number;

		// unsigned decimal
		case 'u':
			num = getuint(&ap, lflag);
			base = 10;
			goto number;

		// (unsigned) octal
		case 'o':
			// Replace this with your code.
			num = getuint(&ap, lflag);
			base = 8;
			goto number;

		// pointer
		case 'p':
			putch('0' | attri, putdat);
			putch('x' | attri, putdat);
			num = (unsigned long long)
				(uintptr_t) va_arg(ap, void *);
			base = 16;
			goto number;

		// (unsigned) hexadecimal
		case 'x':
			num = getuint(&ap, lflag);
			base = 16;
		number:
			printnum(putch, putdat, num, base, width, padc | attri);
			break;

		// escaped '%' character
		case '%':
			putch(ch | attri, putdat);
			break;
		
		// 设置打印字符属性
		case 'B': // 背景色
			ch = *fmt;
			switch(ch){
				case 'B': attri |= 0x1000; break; // 设置蓝色
				case 'G': attri |= 0x2000; break; // 设置绿色
				case 'R': attri |= 0x4000; break; // 设置红色
				case 'I': attri |= 0x8000; break; // 设置高亮
				case 'b': attri &= ~0x1000; break; // 取消蓝色
				case 'g': attri &= ~0x2000; break; // 取消绿色
				case 'r': attri &= ~0x4000; break; // 取消红色
				case 'i': attri &= ~0x8000; break; // 取消高亮
			}
			fmt++;
			ch = *fmt;
			break;
			
		case 'F': // 前景色
			ch = *fmt;
			switch(ch){
				case 'B': attri |= 0x0100; break; // 设置蓝色
				case 'G': attri |= 0x0200; break; // 设置绿色
				case 'R': attri |= 0x0400; break; // 设置红色
				case 'I': attri |= 0x0800; break; // 设置高亮
				case 'b': attri &= ~0x0100; break; // 取消蓝色
				case 'g': attri &= ~0x0200; break; // 取消绿色
				case 'r': attri &= ~0x0400; break; // 取消红色
				case 'i': attri &= ~0x0800; break; // 取消高亮
			}
			fmt++;
			ch = *fmt;
			break;
		
		case 'C': // 清空格式
			ch = *fmt;
			attri = 0;
			break;
			
			

		// unrecognized escape sequence - just print it literally
		default: // 未识别的格式化控制字符。则不进行解析，普通打印。对于已解析的部分，退回去
			putch('%' | attri, putdat);
			for (fmt--; fmt[-1] != '%'; fmt--)
				/* do nothing */;
			break;
		}
	}
}

void
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintfmt(putch, putdat, fmt, ap);
	va_end(ap);
}

struct sprintbuf {
	char *buf;
	char *ebuf;
	int cnt;
};

static void
sprintputch(int ch, struct sprintbuf *b)
{
	b->cnt++;
	if (b->buf < b->ebuf)
		*b->buf++ = ch;
}

int
vsnprintf(char *buf, int n, const char *fmt, va_list ap)
{
	struct sprintbuf b = {buf, buf+n-1, 0};

	if (buf == NULL || n < 1)
		return -E_INVAL;

	// print the string to the buffer
	vprintfmt((void*)sprintputch, &b, fmt, ap);

	// null terminate the buffer
	*b.buf = '\0';

	return b.cnt;
}

int
snprintf(char *buf, int n, const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = vsnprintf(buf, n, fmt, ap);
	va_end(ap);

	return rc;
}


