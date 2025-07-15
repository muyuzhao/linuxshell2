# MyBash - 简单的 C 语言实现的 Shell

这个项目包含了三个不同阶段实现的简化版 Unix Shell。从基础的命令执行到支持管道、重定向和作业控制，每个版本都在前一个版本的基础上增加了新的功能。

## 项目结构

*   `mybash.c`: 第一个版本，实现基础的 Shell 功能。
*   `mybash01.c`: 第二个版本，增加了 I/O 重定向和管道功能。
*   `mybash02.c`: 第三个版本，增加了作业控制（前台/后台运行、`jobs`、`fg`、`bg` 命令）和信号处理。

在编译和运行非内置命令时，请注意 `PATH_BIN` 的定义，它默认为 `/home/stu/quzijie/bash/mybin/`。你需要将自定义的可执行文件放在该目录下，或者修改 `PATH_BIN` 到你的实际路径。

## 版本详情

### `mybash.c` - 版本 1：基础 Shell

**功能特性：**
*   **交互式提示符:** 显示当前用户名、主机名、当前工作目录以及用户类型（`$` 为普通用户，`#` 为 Root 用户）。
*   **命令执行:** 能够执行位于 `PATH_BIN` 目录下的外部命令以及系统自带的命令（通过 `execv`）。
*   **内置命令:** 支持 `cd` (改变目录) 和 `exit` (退出 Shell)。
*   **进程管理:** 使用 `fork` 和 `wait` 来创建子进程并等待其完成，避免僵尸进程。

**缺失功能 (将在后续版本中添加):**
*   管道 (`|`)
*   I/O 重定向 (`<`, `>`, `>>`)
*   后台运行 (`&`)
*   作业控制 (`jobs`, `fg`, `bg`)

### `mybash01.c` - 版本 2：重定向与管道

此版本在 `mybash.c` 的基础上进行了扩展，增加了对 I/O 重定向和管道的支持。

**新增功能特性：**
*   **输入重定向 (`<`):** 允许将文件内容作为命令的输入。
*   **输出重定向 (`>`):** 允许将命令的输出写入到文件，如果文件存在则覆盖。
*   **输出追加重定向 (`>>`):** 允许将命令的输出追加到文件末尾。
*   **管道 (`|`):** 支持将一个命令的输出作为另一个命令的输入，实现多命令串联执行。

**缺失功能 (将在后续版本中添加):**
*   后台运行 (`&`)
*   作业控制 (`jobs`, `fg`, `bg`)

### `mybash02.c` - 版本 3：作业控制

此版本是功能最完善的版本，在 `mybash01.c` 的基础上增加了作业控制、后台执行和更健壮的信号处理机制。

**新增功能特性：**
*   **后台执行 (`&`):** 允许命令在后台运行，不阻塞 Shell 提示符。
*   **作业控制 (`jobs`, `fg`, `bg`):**
    *   `jobs`: 列出当前 Shell 管理的所有后台或停止的作业。
    *   `fg [job_id | %job_id]`: 将指定的后台或停止的作业切换到前台运行。
    *   `bg [job_id | %job_id]`: 将指定的停止的作业切换到后台运行。
*   **进程组管理:** 为前台和后台进程创建和管理独立的进程组，确保作业控制的正确性。
*   **信号处理:** 实现了 `SIGCHLD` 信号处理器，用于异步监控子进程状态变化（完成、停止、继续），并更新作业列表。忽略了 `SIGINT`, `SIGQUIT`, `SIGTSTP`, `SIGTTIN`, `SIGTTOU` 等信号，以确保 Shell 不受子进程信号影响。
*   **交互模式:** 支持交互式模式下的终端控制权转移，确保只有前台进程组才能访问终端。

**注意:**
*   内置命令（如 `cd`, `jobs`, `fg`, `bg`, `exit`）由 Shell 自身处理，不创建子进程。
*   外部命令（包括管道命令）会在新的进程中执行，并根据是否指定 `&` 符号决定在前台或后台运行。

## 如何编译和运行

### 编译所有版本

```bash
gcc -o mybash mybash.c
gcc -o mybash01 mybash01.c
gcc -o mybash02 mybash02.c
```

### 运行

```bash
./mybash
./mybash01
./mybash02
```

## 示例用法 (以 `mybash02` 为例)

### 基础命令

```bash
stu@localhost:/home/stu/quzijie/bash$ ls -l
stu@localhost:/home/stu/quzijie/bash$ cd ..
stu@localhost:/home/stu/quzijie$ pwd
```

### I/O 重定向

```bash
stu@localhost:/home/stu/quzijie/bash$ echo "Hello World" > output.txt
stu@localhost:/home/stu/quzijie/bash$ cat < output.txt
stu@localhost:/home/stu/quzijie/bash$ echo "Appended Line" >> output.txt
stu@localhost:/home/stu/quzijie/bash$ cat output.txt
```

### 管道

```bash
stu@localhost:/home/stu/quzijie/bash$ ls -l | grep mybash
stu@localhost:/home/stu/quzijie/bash$ ps aux | grep bash | wc -l
```

### 后台运行与作业控制

```bash
stu@localhost:/home/stu/quzijie/bash$ sleep 10 &
 12345
stu@localhost:/home/stu/quzijie/bash$ jobs
+ Running    sleep 10
stu@localhost:/home/stu/quzijie/bash$ sleep 50 &
 12346
stu@localhost:/home/stu/quzijie/bash$ jobs
- Running    sleep 10
+ Running    sleep 50
stu@localhost:/home/stu/quzijie/bash$ fg %1
# (sleep 10 会回到前台，你可以按 Ctrl+Z 暂停它)
# Ctrl+Z
+ Stopped    sleep 10
stu@localhost:/home/stu/quzijie/bash$ jobs
+ Stopped    sleep 10
- Running    sleep 50
stu@localhost:/home/stu/quzijie/bash$ bg %1
+ Continued  sleep 10
stu@localhost:/home/stu/quzijie/bash$ jobs
+ Running    sleep 10
- Running    sleep 50
```
