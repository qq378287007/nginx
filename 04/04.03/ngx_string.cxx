#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ngx_string.h"

// 截取字符串尾部空格
void Rtrim(char *str)
{
	if (str == NULL)
		return;

	for (size_t len = strlen(str) - 1; len >= 0 && isspace(str[len]); len--)
		str[len] = 0;
}

// 截取字符串首部空格
void Ltrim(char *str)
{
	if (str == NULL || !isspace(*str)) // 空字符串或首字符不为空白字符
		return;

	// 找第一个不为空格的
	char *p_tmp;
	for (p_tmp = str; *p_tmp != '\0'; p_tmp++)
		if (!isspace(*p_tmp))
			break;

	// 全是空格
	if (*p_tmp == '\0')
	{
		*str = '\0';
		return;
	}

	// 非空格赋值
	char *p_tmp2;
	for (p_tmp2 = str; *p_tmp != '\0'; p_tmp++, p_tmp2++)
		*p_tmp2 = *p_tmp;
	*p_tmp2 = '\0';
}