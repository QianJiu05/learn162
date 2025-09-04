#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

#define PIPE_READ_SIDE 0
#define PIPE_WRITE_SIDE 1

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

char* cat_fullpath(char* usrpath,int idx,char*** head_path,char* full_path);
static int path_init(char*** head_path);
bool is_full_path(char* path);
void complete_and_run(struct tokens* tokens);
bool check_stdio(int cmd_num,char* args[],int* infile,int* outfile);
void get_redirect_args(char* old_args[],char* new_args[],int cmd_num);
/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "print current path"},
    {cmd_cd, "cd", "cd a file"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

int cmd_pwd(unused struct tokens* tokens) {
  char path[4096];
  getcwd(path,sizeof(path));
  printf("path = %s\n",path);
  return 1;
}

int cmd_cd(struct tokens* tokens) {
  const char* path = tokens_get_token(tokens,1);
  int ret = chdir(path);
  if(ret == 0){return 1;}
  else{
    printf("enter failed!\n");
    return -1;
  }
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

char* cat_fullpath(char* usrpath,int idx,char*** head_path,char* full_path){
  memset(full_path,'\0',strlen(full_path));
  strcat(full_path,(*head_path)[idx]);
  strcat(full_path,"/");
  strcat(full_path,usrpath);
  return full_path;
}

void complete_and_run(struct tokens* tokens){
  int cmd_num = tokens_get_length(tokens);
  char* args[cmd_num + 1];
  for(int i = 0; i < cmd_num + 1; i++){
    args[i] = malloc(sizeof(char*));
  }

  for(int i = 0; i < cmd_num; i++){
    args[i] = tokens_get_token(tokens,i);
  }
  args[cmd_num] = NULL;

  /* 输入输出重定向*/
  // FILE *infile = NULL, *outfile = NULL;
  int infile = -1, outfile = -1;
  bool is_redirect = check_stdio(cmd_num,args,&infile,&outfile);
  char* new_args[cmd_num];

  if(is_redirect == true){
    if(infile != -1){
      dup2(infile,STDIN_FILENO);
    }
    if(outfile != -1){
      dup2(outfile,STDOUT_FILENO);
    }
    if((outfile == -1) && (infile == -1)){
      perror("no file!\n");
    }
    close(infile);
    close(outfile);


    get_redirect_args(args,new_args,cmd_num);
  }

  // /* 打印args*/
  // for(int i=0;i<sizeof(new_args);i++){
  //         printf("args=%s\n",new_args[i]);
  // }

  char* path = tokens_get_token(tokens,0);

  /*对于没有补全的path做拼接处理*/
  if(is_full_path(path) == false){//不是全路径
    char** head_paths = NULL;
    int num_of_paths = path_init(&head_paths);
    // for(int i=0;i<num_of_paths;i++){
    //   printf("%s\n",head_paths[i]);
    // }
    char* full_path = malloc(sizeof(char)*100);
    for(int i = 0; i < num_of_paths; i++){
      cat_fullpath(path,i,&head_paths,full_path);
      if(is_redirect){
        execv(full_path,new_args);
      }else{
        execv(full_path,args);
      }
    }
  }else{
    if(is_redirect){
      execv(path,new_args);
    }else{
      execv(path,args);
    }
  }  
  perror("execv failed\n");
}

void get_redirect_args(char* old_args[],char* new_args[],int cmd_num){
  int i = 0, j = 0;
  while(i < cmd_num){// i = cmd_num是null
    if((strcmp(old_args[i],"<") == 0) || (strcmp(old_args[i],">") == 0)){
      i+=2;
    }else{
      new_args[j] = old_args[i];
      i++;
      j++;
    }
  }
  new_args[j] = NULL;
}

bool check_stdio(int cmd_num,char* args[],int* infile,int* outfile){
  /* i应该小于cmdnum - 1，因为cmdnum位置是NULL
  *  cmdnum-1是最后一个参数，不能是 < or >   */
  bool flag_in = false, flag_out = false;
  for(int i = 1; i < cmd_num - 1; i++){
    if(strcmp(args[i],"<") == 0){
      *infile = open(args[i+1],O_RDONLY);
      i++;
      if(*infile == -1){
        perror("can't find infile\n");
      }
      flag_in = true;
    }else if(strcmp(args[i],">") == 0){
      *outfile = open(args[i+1],O_WRONLY | O_CREAT | O_TRUNC, 0644);
      i++;
      flag_out = true;
    }
    if(flag_in && flag_out)return true;
  }
  return flag_in || flag_out;
}

static int path_init(char*** head_path){
  char s[] = "/home/workspace/.vscode-server/bin/6f17636121051a53c88d3e605c491d22af2ba755/bin/remote-cli:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/home/workspace/.bin:/home/workspace/.cargo/bin:/home/workspace/.bin:/home/workspace/.cargo/bin";
  char *token, *save_ptr;
  static int num_of_paths;

  *head_path = malloc(sizeof(char**));

  for (token = strtok_r (s, ":", &save_ptr); token != NULL;
            token = strtok_r (NULL, ":", &save_ptr))
  { 
    (*head_path)[num_of_paths] = malloc(sizeof(char*)*(strlen(token)+1));
    strcpy((*head_path)[num_of_paths],token);
    num_of_paths++;
  }
  return num_of_paths;
}

bool is_full_path(char* path){
  int len = strlen(path);
  for(int i = 0; i < len; i++){
    if((path[i] == '\\') || (path[i] == '/')){
      return true;
    }
  }
  return false;
}

int main(unused int argc, unused char* argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);


  

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      /* shell内置命令 */
      cmd_table[fundex].fun(tokens);
    } else {
      /* run commands as programs. */
      int cpid = fork();
      if(cpid > 0){//parent process
        /*如果不wait，二者会同时读取命令*/
        waitpid(cpid,NULL,0);
      }else if(cpid == 0){//child
        complete_and_run(tokens);
      }else{
        fprintf(stdout,"fork failed\n");
      }
    }

    struct pipe_fds{
    int fds[2];
  };
  int pipe_num = 10;
  struct pipe_fds pipe_arr[pipe_num - 1];

  for(int i = 0; i < pipe_num - 1; i++){
    pipe(pipe_arr[i].fds);
  }

  for(int i = 0; i < pipe_num; i++){
    int cpid = fork();//pipe由于fork被share了
    if(cpid >0){
      /* shell进程不使用管道 */
      for (int j = 0; j < pipe_num - 1; j++) {
        close(pipe_arr[j].fds[PIPE_READ_SIDE]);
        close(pipe_arr[j].fds[PIPE_WRITE_SIDE]);
      }
    }else if(cpid == 0){//child
      if(i > 0){
        dup2(pipe_arr[i-1].fds[PIPE_READ_SIDE],STDIN_FILENO);
      }
      if(i < pipe_num - 1){
        dup2(pipe_arr[i].fds[PIPE_WRITE_SIDE],STDOUT_FILENO);
      }
      /* close all */
      for (int j = 0; j < pipe_num - 1; j++) {
        close(pipe_arr[j].fds[PIPE_READ_SIDE]);
        close(pipe_arr[j].fds[PIPE_WRITE_SIDE]);
      }
    }
  }
    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
