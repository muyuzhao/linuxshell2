#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <wait.h>
#include <pwd.h>
#include <fcntl.h>  // 新增头文件用于文件操作
#include <sys/stat.h> // 新增头文件
#define ARG_MAX 10
#define MAX_COMMANDS 5  // 新增：最大管道命令数
#define MAX_ARGS_PER_COMMAND 10  // 新增：每命令最大参数数
#define PATH_BIN "/home/stu/quzijie/bash/mybin/"

void printf_info(){
        //用户名，主机名，路径，管理员/普通用户
        char *user_str="$";
        //存储用户信息位置
        int user_id=getuid();
        if(user_id==0){
user_str="#";
        }

        struct passwd *ptr=getpwuid(user_id);
        if(ptr==NULL){
                printf("mybash1.0>> ");
                fflush(stdout);
                return ;
        }

        char hostname[128]={0};
        if(gethostname(hostname,128)==-1){
                printf("mybash1.0>> ");
                fflush(stdout);
                return ;
        }

        char dir[256]={0};
        if(getcwd(dir,256)==NULL){
                printf("mybash1.0>> ");
                fflush(stdout);
                return ;
        }
        printf("\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m%s ",ptr->pw_name,hostname,dir,user_str);
	fflush(stdout);
}

int parse_command(char *buff, char *commands[][MAX_ARGS_PER_COMMAND], int *in_redirect, int *out_redirect, int *append, char **in_file, char **out_file) {
    char *saveptr;
    char *token = strtok_r(buff, " ", &saveptr);
    int cmd_idx = 0, arg_idx = 0;

    *in_redirect = 0;
    *out_redirect = 0;
    *append = 0;
    *in_file = NULL;
    *out_file = NULL;

    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            // 输入重定向
            *in_redirect = 1;
            token = strtok_r(NULL, " ", &saveptr);
            if (token) *in_file = token;
            token = strtok_r(NULL, " ", &saveptr);
            continue;
        } 
        else if (strcmp(token, ">") == 0) {
            // 输出重定向（覆盖）
            *out_redirect = 1;
            token = strtok_r(NULL, " ", &saveptr);
            if (token) *out_file = token;
            token = strtok_r(NULL, " ", &saveptr);
            continue;
        } 
        else if (strcmp(token, ">>") == 0) {
            // 输出重定向（追加）
            *out_redirect = 1;
            *append = 1;
            token = strtok_r(NULL, " ", &saveptr);
            if (token) *out_file = token;
            token = strtok_r(NULL, " ", &saveptr);
            continue;
        } 
        else if (strcmp(token, "|") == 0) {
            // 管道分隔符
            commands[cmd_idx++][arg_idx] = NULL;
            arg_idx = 0;
            token = strtok_r(NULL, " ", &saveptr);
            continue;
        } 
        else {
            // 命令参数
            commands[cmd_idx][arg_idx++] = token;
        }
        token = strtok_r(NULL, " ", &saveptr);
    }
    
    commands[cmd_idx][arg_idx] = NULL;  // 命令结尾标记
    return cmd_idx + 1;  // 返回命令总数
}

void execute_single_command(char **myargv, int in_redirect, char *in_file, int out_redirect, char *out_file, int append) {
    if (myargv == NULL || myargv[0] == NULL) return;
    
    pid_t pid = fork();
    if (pid == -1) return;
    
    if (pid == 0) {
        // 输入重定向处理
        if (in_redirect && in_file) {
            int fd = open(in_file, O_RDONLY);
            if (fd < 0) {
                perror("open input file");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        
        // 输出重定向处理
        if (out_redirect && out_file) {
            int flags = O_WRONLY | O_CREAT;
            flags |= (append) ? O_APPEND : O_TRUNC;
            int fd = open(out_file, flags, 0644);
            if (fd < 0) {
                perror("open output file");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        // 尝试在自定义目录中执行
        char pathname[256];
        snprintf(pathname, sizeof(pathname), "%s%s", PATH_BIN, myargv[0]);
        if (access(pathname, X_OK) == 0) {
            execv(pathname, myargv);
        } else {
            execvp(myargv[0], myargv);
        }
        perror("execvp error");
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
    }
}

void execute_pipeline(char *commands[][MAX_ARGS_PER_COMMAND], int cmd_count) {
    int prev_pipe = -1;
    int fd[2];
    pid_t pid;

    for (int i = 0; i < cmd_count; i++) {
        if (i < cmd_count - 1) pipe(fd);  // 创建管道（最后一个命令不需要）
        
        pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }
        
        if (pid == 0) {  // 子进程
            // 从上一个命令读（如果不是第一个命令）
            if (i > 0) {
                dup2(prev_pipe, STDIN_FILENO);
                close(prev_pipe);
            }
            
            // 输出到下一个命令（如果不是最后一个命令）
            if (i < cmd_count - 1) {
                close(fd[0]);  // 关闭读端
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
            }
            
            // 执行当前命令
            execute_single_command(commands[i], 0, NULL, 0, NULL, 0);
            exit(0);
        } 
        else {  // 父进程
            // 关闭前一个管道（如果有）
            if (i > 0) close(prev_pipe);
            
            // 如果不是最后一个命令，保存当前管道读端
            if (i < cmd_count - 1) {
                close(fd[1]);  // 关闭写端（被子进程使用）
                prev_pipe = fd[0];  // 保存读端给下一个命令
            }
        }
    }
    
    // 父进程等待所有子进程
    for (int i = 0; i < cmd_count; i++) {
        wait(NULL);
    }
}

int main() {
    while (1) {
        printf_info();
        char buff[256] = {0};  // 增加缓冲区大小
        
        if (fgets(buff, 256, stdin) == NULL) break;
        buff[strcspn(buff, "\n")] = '\0';  // 安全去除换行符
        
        // 跳过空命令
        if (strlen(buff) == 0) continue;
        
        // 处理内置命令
        if (strncmp(buff, "cd", 2) == 0) {
            char *path = strtok(buff + 2, " ");
            if (path) {
                if (chdir(path) != 0) perror("cd");
            }
            continue;
        } else if (strcmp(buff, "exit") == 0) {
            exit(0);
        }
        
        // 命令解析
        char *commands[MAX_COMMANDS][MAX_ARGS_PER_COMMAND];
        int in_redirect, out_redirect, append;
        char *in_file = NULL, *out_file = NULL;
        
        int cmd_count = parse_command(buff, commands, &in_redirect, &out_redirect, &append, &in_file, &out_file);
        
        if (cmd_count == 0) continue;  // 无有效命令
        
        // 执行逻辑
        if (cmd_count == 1) {  // 单命令
            execute_single_command(commands[0], 
                                  in_redirect, in_file, 
                                  out_redirect, out_file, 
                                  append);
        } else {  // 多命令（管道）
            execute_pipeline(commands, cmd_count);
        }
    }
    return 0;
}
