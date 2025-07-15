#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <wait.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include <termios.h>
#include <errno.h>

#define ARG_MAX 10
#define MAX_COMMANDS 5
#define MAX_ARGS_PER_COMMAND 10
#define MAX_JOBS 20
#define PATH_BIN "/home/stu/quzijie/bash/mybin/"

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} JobStatus;

typedef struct {
    int id;           // 作业ID
    pid_t pgid;       // 进程组ID
    JobStatus status; // 作业状态
    char *command;    // 命令字符串
    int is_pipeline;  // 是否为管道命令
} Job;

// 全局变量
Job jobs[MAX_JOBS];             // 作业列表
int current_job_id = 1;         // 下一个可用的作业ID
pid_t shell_pgid;               // shell进程组ID
int shell_is_interactive;       // shell是否交互式运行

/**********************************************************************
 * 作业管理函数
 **********************************************************************/

/**
 * @brief 初始化作业列表和shell环境
 */
void init_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        jobs[i].id = -1;
        jobs[i].command = NULL;
    }
    
    // 设置shell进程组
    shell_pgid = getpgrp();
    shell_is_interactive = isatty(STDIN_FILENO);
    
    if (shell_is_interactive) {
        // 确保shell在前台
        while (tcgetpgrp(STDIN_FILENO) != shell_pgid) {
            kill(-shell_pgid, SIGTTIN);
        }
        
        // 忽略作业控制信号
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
    }
}

/**
 * @brief 添加新作业到列表
 */
int add_job(pid_t pgid, JobStatus status, const char *command, int is_pipeline) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].id == -1) {
            jobs[i].id = current_job_id++;
            jobs[i].pgid = pgid;
            jobs[i].status = status;
            jobs[i].command = strdup(command);
            jobs[i].is_pipeline = is_pipeline;
            if (!jobs[i].command) {
                jobs[i].id = -1;
                return -1;
            }
            return jobs[i].id;
        }
    }
    return -1;
}

/**
 * @brief 根据ID查找作业
 */
Job* find_job(int id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].id == id) {
            return &jobs[i];
        }
    }
    return NULL;
}

/**
 * @brief 根据进程组ID查找作业
 */
Job* find_job_by_pgid(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].id != -1 && jobs[i].pgid == pgid) {
            return &jobs[i];
        }
    }
    return NULL;
}

/**
 * @brief 更新作业状态
 */
void update_job_status(pid_t pgid, JobStatus status) {
    Job* job = find_job_by_pgid(pgid);
    if (job) {
        job->status = status;
        // 作业完成时释放资源
        if (status == JOB_DONE) {
            printf("[%d]+\tDone\t\t%s\n", job->id, job->command);
            free(job->command);
            job->command = NULL;
            job->id = -1;
        }
    }
}

/**
 * @brief 清理已完成的作业
 */
void cleanup_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].id != -1 && jobs[i].status == JOB_DONE) {
            free(jobs[i].command);
            jobs[i].command = NULL;
            jobs[i].id = -1;
        }
    }
}

/**********************************************************************
 * 信号处理函数
 **********************************************************************/

/**
 * @brief SIGCHLD信号处理程序
 */
void handle_sigchld(int sig) {
    int status;
    pid_t pid;
    
    // 处理所有状态变化的子进程
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        // 获取进程组ID
        pid_t pgid = getpgid(pid);
        if (pgid == -1) {
            // 如果getpgid失败，可能进程已经终止，尝试用pid作为pgid
            pgid = pid;
        }
        
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            // 进程正常退出或被信号终止
            update_job_status(pgid, JOB_DONE);
        } else if (WIFSTOPPED(status)) {
            // 进程被信号停止
            update_job_status(pgid, JOB_STOPPED);
            Job* job = find_job_by_pgid(pgid);
            if (job) {
                printf("\n[%d]+\tStopped\t\t%s\n", job->id, job->command);
            }
        } else if (WIFCONTINUED(status)) {
            // 进程被继续执行
            update_job_status(pgid, JOB_RUNNING);
        }
    }
}

/**********************************************************************
 * 内置命令实现
 **********************************************************************/

/**
 * @brief 打印作业列表
 */
void cmd_jobs() {
    cleanup_jobs(); // 清理已完成的作业
    
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].id != -1) {
            printf("[%d]\t", jobs[i].id);
            switch (jobs[i].status) {
                case JOB_RUNNING: printf("Running"); break;
                case JOB_STOPPED: printf("Stopped"); break;
                case JOB_DONE: printf("Done"); break;
            }
            printf("\t\t%s\n", jobs[i].command);
        }
    }
}

/**
 * @brief 将后台作业切换到前台运行
 */
void cmd_fg(int job_id) {
    Job *job = NULL;
    
    // 如果没有指定job_id，使用最近的作业
    if (job_id == -1) {
        int max_id = 0;
        for (int i = 0; i < MAX_JOBS; i++) {
            if (jobs[i].id != -1 && jobs[i].id > max_id) {
                max_id = jobs[i].id;
                job = &jobs[i];
            }
        }
    } else {
        job = find_job(job_id);
    }
    
    if (!job) {
        printf("fg: %d: no such job\n", job_id);
        return;
    }
    
    // 更新作业状态
    job->status = JOB_RUNNING;
    
    if (shell_is_interactive) {
        // 设置前台进程组
        tcsetpgrp(STDIN_FILENO, job->pgid);
    }
    
    // 发送SIGCONT信号继续运行作业
    kill(-job->pgid, SIGCONT);
    
    // 等待作业完成或暂停
    int status;
    pid_t result = waitpid(-job->pgid, &status, WUNTRACED);
    
    if (shell_is_interactive) {
        // 重新获取终端控制权
        tcsetpgrp(STDIN_FILENO, shell_pgid);
    }
    
    // 检查作业状态
    if (result > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            update_job_status(job->pgid, JOB_DONE);
        } else if (WIFSTOPPED(status)) {
            update_job_status(job->pgid, JOB_STOPPED);
        }
    }
}

/**
 * @brief 将暂停的作业切换到后台运行
 */
void cmd_bg(int job_id) {
    Job *job = NULL;
    
    // 如果没有指定job_id，使用最近的暂停作业
    if (job_id == -1) {
        int max_id = 0;
        for (int i = 0; i < MAX_JOBS; i++) {
            if (jobs[i].id != -1 && jobs[i].status == JOB_STOPPED && jobs[i].id > max_id) {
                max_id = jobs[i].id;
                job = &jobs[i];
            }
        }
    } else {
        job = find_job(job_id);
    }
    
    if (!job) {
        printf("bg: %d: no such job\n", job_id);
        return;
    }
    
    if (job->status != JOB_STOPPED) {
        printf("bg: job %d already in background\n", job_id);
        return;
    }
    
    // 更新作业状态
    job->status = JOB_RUNNING;
    
    // 发送SIGCONT信号继续运行作业
    kill(-job->pgid, SIGCONT);
    printf("[%d]+\tContinued\t%s\n", job_id, job->command);
}

/**
 * @brief 执行cd命令
 */
void cmd_cd(const char *path) {
    if (!path || strcmp(path, "") == 0) {
        // 默认切换到HOME目录
        const char *home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
        path = home;
    }
    
    if (chdir(path) != 0) {
        perror("cd");
    }
}

/**
 * @brief 处理内置命令
 */
int handle_builtin_commands(char *buff, char *original_cmd) {
    // 创建一个buff的副本进行操作，避免修改原始buff
    char *temp_buff = strdup(buff);
    if (!temp_buff) {
        perror("strdup");
        return 0;
    }

    char *cmd = strtok(temp_buff, " ");
    if (!cmd) {
        free(temp_buff);
        return 0;
    }
    
    // 解析命令参数
    char *arg = strtok(NULL, " ");
    
    int is_builtin = 0; // 标记是否为内置命令

    if (strcmp(cmd, "cd") == 0) {
        cmd_cd(arg);
        is_builtin = 1;
    }
    else if (strcmp(cmd, "exit") == 0) {
        free(temp_buff);
        free(original_cmd);
        exit(0);
    }
    else if (strcmp(cmd, "jobs") == 0) {
        cmd_jobs();
        is_builtin = 1;
    }
    else if (strcmp(cmd, "fg") == 0) {
        int job_id = -1;
        if (arg) {
            // 支持%前缀
            if (arg[0] == '%') job_id = atoi(arg + 1);
            else job_id = atoi(arg);
        }
        cmd_fg(job_id);
        is_builtin = 1;
    }
    else if (strcmp(cmd, "bg") == 0) {
        int job_id = -1;
        if (arg) {
            // 支持%前缀
            if (arg[0] == '%') job_id = atoi(arg + 1);
            else job_id = atoi(arg);
        }
        cmd_bg(job_id);
        is_builtin = 1;
    }

    free(temp_buff); // 释放副本

    if (is_builtin) {
        free(original_cmd); // 如果是内置命令，释放原始命令字符串
        return 1;
    }
    
    return 0; // 不是内置命令
}


/**********************************************************************
 * 命令提示符与解析
 **********************************************************************/

/**
 * @brief 打印命令提示符
 */
void print_prompt() {
    char *user_str = "$";
    int user_id = getuid();
    
    // 检查是否是root用户
    if (user_id == 0) {
        user_str = "#";
    }
    
    // 获取用户信息
    struct passwd *ptr = getpwuid(user_id);
    if (!ptr) {
        printf("mybash1.0>> ");
        fflush(stdout);
        return;
    }
    
    // 获取主机名
    char hostname[128] = {0};
    if (gethostname(hostname, sizeof(hostname) - 1) == -1) {
        printf("mybash1.0>> ");
        fflush(stdout);
        return;
    }
    
    // 获取当前工作目录
    char dir[256] = {0};
    if (getcwd(dir, sizeof(dir) - 1) == NULL) {
        printf("mybash1.0>> ");
        fflush(stdout);
        return;
    }
    
    // 彩色打印提示符
    printf("\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m%s ",
           ptr->pw_name, hostname, dir, user_str);
    fflush(stdout);
}

/**
 * @brief 解析命令字符串
 */
int parse_command(char *buff, char *commands[][MAX_ARGS_PER_COMMAND],
                  int *in_redirect, int *out_redirect, int *append,
                  char **in_file, char **out_file, int *background) {
    *background = 0;
    
    // 检查是否有后台运行标记 &
    size_t len = strlen(buff);
    if (len > 0 && buff[len - 1] == '&') {
        *background = 1;
        buff[len - 1] = '\0'; // 移除 &
        
        // 移除&前的空格
        while (len > 1 && buff[len - 2] == ' ') {
            buff[len - 2] = '\0';
            len--;
        }
    }
    
    char *saveptr;
    char *token = strtok_r(buff, " ", &saveptr);
    int cmd_idx = 0, arg_idx = 0;
    
    *in_redirect = 0;
    *out_redirect = 0;
    *append = 0;
    *in_file = NULL;
    *out_file = NULL;
    
    // 解析命令和重定向符号
    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            *in_redirect = 1;
            token = strtok_r(NULL, " ", &saveptr);
            if (token) *in_file = token;
            token = strtok_r(NULL, " ", &saveptr);
            continue;
        }
        else if (strcmp(token, ">") == 0) {
            *out_redirect = 1;
            token = strtok_r(NULL, " ", &saveptr);
            if (token) *out_file = token;
            token = strtok_r(NULL, " ", &saveptr);
            continue;
        }
        else if (strcmp(token, ">>") == 0) {
            *out_redirect = 1;
            *append = 1;
            token = strtok_r(NULL, " ", &saveptr);
            if (token) *out_file = token;
            token = strtok_r(NULL, " ", &saveptr);
            continue;
        }
        else if (strcmp(token, "|") == 0) {
            commands[cmd_idx++][arg_idx] = NULL;
            arg_idx = 0;
            token = strtok_r(NULL, " ", &saveptr);
            continue;
        }
        else {
            commands[cmd_idx][arg_idx++] = token;
        }
        token = strtok_r(NULL, " ", &saveptr);
    }
    
    commands[cmd_idx][arg_idx] = NULL;
    return cmd_idx + 1; // 返回命令数量
}

/**********************************************************************
 * 命令执行函数
 **********************************************************************/

/**
 * @brief 执行单条命令
 */
void execute_single_command(char **myargv, int in_redirect, char *in_file,
                           int out_redirect, char *out_file, int append,
                           int background, const char *command_str) {
    if (!myargv || !myargv[0]) return;
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    
    if (pid == 0) { // 子进程
        // 创建新进程组
        if (setpgid(0, 0) == -1) {
            perror("setpgid");
            exit(1);
        }
        
        // 如果不是后台作业且shell是交互式的，设置为前台进程组
        if (!background && shell_is_interactive) {
            tcsetpgrp(STDIN_FILENO, getpgrp());
        }
        
        // 恢复默认信号处理
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        
        // 输入重定向处理
        if (in_redirect && in_file) {
            int fd = open(in_file, O_RDONLY);
            if (fd < 0) {
                perror("open input file");
                exit(1);
            }
            if (dup2(fd, STDIN_FILENO) == -1) {
                perror("dup2 input");
                exit(1);
            }
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
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2 output");
                exit(1);
            }
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
    }
    else { // 父进程
        // 设置进程组
        if (setpgid(pid, pid) == -1 && errno != EACCES) {
            perror("setpgid");
        }
        
        if (background) {
            // 后台作业：添加到作业列表
            int job_id = add_job(pid, JOB_RUNNING, command_str, 0);
            if (job_id != -1) {
                printf("[%d] %d\n", job_id, pid);
            }
        } else {
            // 前台作业：等待完成
            if (shell_is_interactive) {
                tcsetpgrp(STDIN_FILENO, pid);
            }
            
            int status;
            pid_t result = waitpid(pid, &status, WUNTRACED);
            
            if (shell_is_interactive) {
                tcsetpgrp(STDIN_FILENO, shell_pgid);
            }
            
            // 检查作业是否被暂停
            if (result > 0 && WIFSTOPPED(status)) {
                // 添加到作业列表
                int job_id = add_job(pid, JOB_STOPPED, command_str, 0);
                if (job_id != -1) {
                    printf("\n[%d]+\tStopped\t\t%s\n", job_id, command_str);
                }
            }
        }
    }
}

/**
 * @brief 执行管道命令
 */
void execute_pipeline(char *commands[][MAX_ARGS_PER_COMMAND], int cmd_count,
                     int background, const char *command_str) {
    pid_t pgid = 0;
    int prev_pipe = -1;
    int fd[2];
    pid_t pids[MAX_COMMANDS];
    
    for (int i = 0; i < cmd_count; i++) {
        // 创建管道（最后一个命令不需要输出管道）
        if (i < cmd_count - 1) {
            if (pipe(fd) == -1) {
                perror("pipe");
                return;
            }
        }
        
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return;
        }
        
        if (pid == 0) { // 子进程
            // 设置进程组
            if (i == 0) {
                if (setpgid(0, 0) == -1) {
                    perror("setpgid");
                    exit(1);
                }
            } else {
                if (setpgid(0, pids[0]) == -1) {
                    perror("setpgid");
                    exit(1);
                }
            }
            
            // 如果不是后台作业且shell是交互式的，设置为前台进程组
            if (!background && shell_is_interactive && i == 0) {
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }
            
            // 恢复默认信号处理
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            
            // 从上一个命令读（如果不是第一个命令）
            if (i > 0) {
                if (dup2(prev_pipe, STDIN_FILENO) == -1) {
                    perror("dup2 input");
                    exit(1);
                }
                close(prev_pipe);
            }
            
            // 输出到下一个命令（如果不是最后一个命令）
            if (i < cmd_count - 1) {
                close(fd[0]); // 关闭读端
                if (dup2(fd[1], STDOUT_FILENO) == -1) {
                    perror("dup2 output");
                    exit(1);
                }
                close(fd[1]);
            }
            
            // 执行当前命令
            char *args[MAX_ARGS_PER_COMMAND];
            int arg_idx = 0;
            while (commands[i][arg_idx] != NULL) {
                args[arg_idx] = commands[i][arg_idx];
                arg_idx++;
            }
            args[arg_idx] = NULL;
            
            // 尝试在自定义目录中执行
            char pathname[256];
            snprintf(pathname, sizeof(pathname), "%s%s", PATH_BIN, args[0]);
            if (access(pathname, X_OK) == 0) {
                execv(pathname, args);
            } else {
                execvp(args[0], args);
            }
            
            perror("execvp error");
            exit(1);
        }
        else { // 父进程
            pids[i] = pid;
            
            // 设置进程组
            if (i == 0) {
                if (setpgid(pid, pid) == -1 && errno != EACCES) {
                    perror("setpgid");
                }
                pgid = pid;
            } else {
                if (setpgid(pid, pgid) == -1 && errno != EACCES) {
                    perror("setpgid");
                }
            }
            
            // 关闭前一个管道的读端（如果有）
            if (i > 0) {
                close(prev_pipe);
            }
            
            // 保存当前管道读端（用于下一个命令）
            if (i < cmd_count - 1) {
                close(fd[1]); // 关闭写端
                prev_pipe = fd[0]; // 保存读端
            }
        }
    }
    
    if (background) {
        // 后台管道作业：添加到作业列表
        int job_id = add_job(pgid, JOB_RUNNING, command_str, 1);
        if (job_id != -1) {
            printf("[%d] %d\n", job_id, pgid);
        }
    } else {
        // 前台管道作业：等待所有进程完成
        if (shell_is_interactive) {
            tcsetpgrp(STDIN_FILENO, pgid);
        }
        
        int status;
        pid_t result;
        int stopped_count = 0;
        
        // 等待管道中的所有进程
        for (int i = 0; i < cmd_count; i++) {
            result = waitpid(pids[i], &status, WUNTRACED);
            if (result > 0 && WIFSTOPPED(status)) {
                stopped_count++;
            }
        }
        
        if (shell_is_interactive) {
            tcsetpgrp(STDIN_FILENO, shell_pgid);
        }
        
        // 如果有进程被暂停，添加到作业列表
        if (stopped_count > 0) {
            int job_id = add_job(pgid, JOB_STOPPED, command_str, 1);
            if (job_id != -1) {
                printf("\n[%d]+\tStopped\t\t%s\n", job_id, command_str);
            }
        }
    }
}

/**********************************************************************
 * 主函数
 **********************************************************************/

int main() {
    // 初始化作业列表和shell环境
    init_jobs();
    
    // 设置SIGCHLD信号处理函数
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    
    // 主循环
    while (1) {
        print_prompt();
        
        // 读取用户输入
        char buff[256] = {0};
        if (fgets(buff, sizeof(buff), stdin) == NULL) {
            // Ctrl+D 输入EOF，退出shell
            printf("exit\n");
            break;
        }
        
        buff[strcspn(buff, "\n")] = '\0'; // 安全去除换行符
        
        // 跳过空命令
        if (strlen(buff) == 0) continue;
        
        // 保存原始命令字符串
        char *command_str = strdup(buff);
        if (!command_str) {
            perror("strdup");
            continue;
        }
        
        // 处理内置命令
        if (handle_builtin_commands(buff, command_str)) {
            continue; // 已处理内置命令
        }
        
        // 命令解析
        char *commands[MAX_COMMANDS][MAX_ARGS_PER_COMMAND];
        int in_redirect, out_redirect, append, background;
        char *in_file = NULL, *out_file = NULL;
        
        int cmd_count = parse_command(buff, commands, &in_redirect, &out_redirect,
                                    &append, &in_file, &out_file, &background);
        
        if (cmd_count == 0) {
            free(command_str);
            continue;
        }
        
        // 执行逻辑
        if (cmd_count == 1) {
            execute_single_command(commands[0],
                                  in_redirect, in_file,
                                  out_redirect, out_file,
                                  append,
                                  background,
                                  command_str);
        } else {
            execute_pipeline(commands, cmd_count, background, command_str);
        }
        
        free(command_str);
    }
    
    return 0;
}
