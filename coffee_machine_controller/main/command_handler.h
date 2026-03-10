/**
 * @file command_handler.h
 * @brief 命令处理模块 - 解析和执行命令
 */

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

/**
 * @brief 处理命令字符串
 * 
 * @param cmd 命令字符串
 */
void command_process(const char *cmd);

/**
 * @brief 去除字符串末尾的空白字符
 * 
 * @param s 字符串
 */
void str_rstrip(char *s);

#endif /* COMMAND_HANDLER_H */
