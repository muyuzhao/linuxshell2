#include <stdio.h>
#include <pwd.h>
#include <wait.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#define ARG_MAX 10
#define PATH_BIN "/home/stu/quzijie/bash/mybin/"
char *get_cmd(char *buff,char* myargv[]){
	if(buff==NULL||myargv==NULL){
		return NULL;
	}
	char *s=strtok(buff," ");
	int i=0;
	while(s!=NULL){
		myargv[i++]=s;
		s=strtok(NULL," ");
	}
	return myargv[0];
}

void run_cmd(char* path,char* myargv[]){
	if(path==NULL||path==NULL){
		return ;
	}
	//fork+exec
	pid_t pid=fork();
	if(pid==-1){
		return ;
	}
	if(pid==0)//子进程
	{
		//execvp(path,myargv);
		char pathname[128]={0};
		if(strncmp(path,"/",1)==0||strncmp(path,"./",2)==0){
			strcpy(pathname,path);
		}else{
			strcpy(pathname,PATH_BIN);
			strcat(pathname,path);
		}
		execv(pathname,myargv);
		perror("execv error!\n");
		exit(0);
	}else{
		wait(NULL);//处理僵死进程	
	}
}
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
int main(){
	while(1){
		//printf("stu@localhost   ~$");
		printf_info();
		//to do

		//
		fflush(stdout);

		char buff[128]={0};
		fgets(buff,128,stdin);
		buff[strlen(buff)-1]='\0';
		

		char*myargv[ARG_MAX]={0};
		char *cmd=get_cmd(buff,myargv);

		if(cmd==NULL){
			continue;
		}else if(strcmp(cmd,"cd")==0){
			if(myargv[1]!=NULL){
				if(chdir(myargv[1])==-1){
					printf("cd err!");
				}
			}
		}else if(strcmp(cmd,"exit")==0){
			//exit(0); //ok,不建议
			break;
		}else{
			//
			run_cmd(cmd,myargv);
		}
	}
	exit(0);
}
