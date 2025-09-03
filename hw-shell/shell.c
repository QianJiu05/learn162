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
bool check_stdio(int cmd_num,char* args[],FILE** infile,FILE** outfile);

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
  // printf("full_path cated: %s\n",full_path);
  return full_path;
}
// void build_args(struct tokens* tokens){

// }
void complete_and_run(struct tokens* tokens){
  int cmd_num = tokens_get_length(tokens);
  char* args[cmd_num + 1];
  // args = malloc(sizeof(char**));
  for(int i = 0; i < cmd_num + 1; i++){
    args[i] = malloc(sizeof(char*));
  }

  for(int i = 0; i < cmd_num; i++){
    args[i] = tokens_get_token(tokens,i);
  }
  args[cmd_num] = NULL;

  /* 输入输出重定向*/
  FILE *infile = NULL, *outfile = NULL;
  if(check_stdio(cmd_num,args,&infile,&outfile) == true){
    if(infile != NULL){
      dup2(fileno(infile),STDIN_FILENO);
    }
    if(outfile != NULL){
      dup2(fileno(outfile),STDOUT_FILENO);
    }
    if((outfile == NULL) && (infile == NULL)){
      perror("no file!\n");
    }
    fclose(infile);
    fclose(outfile);
  }

  // /* 打印args*/
  for(int i=0;i<sizeof(args);i++){
          printf("args=%s\n",args[i]);
  }

  char* path = tokens_get_token(tokens,0);

  /*对于没有补全的path做拼接处理*/
  if(is_full_path(path) == false){//不是全路径
    // printf("uncomplete: %s\n",path);
    char** head_paths = NULL;
    int num_of_paths = path_init(&head_paths);
    for(int i=0;i<num_of_paths;i++){
      printf("%s\n",head_paths[i]);
    }
    char* full_path = malloc(sizeof(char)*100);
    for(int i = 0; i < num_of_paths; i++){
      cat_fullpath(path,i,&head_paths,full_path);
      execv(full_path,args);
    }
  }else{
    execv(path,args);
  }  
  perror("execv failed\n");
}

bool check_stdio(int cmd_num,char* args[],FILE** infile,FILE** outfile){
  /* i应该小于cmdnum - 1，因为cmdnum位置是NULL
  *  cmdnum-1是最后一个参数，不能是 < or >   */
  bool flag_in = false, flag_out = false;
  for(int i = 1; i < cmd_num - 1; i++){
    if(strcmp(args[i],"<") == 0){
      *infile = fopen(args[i+1],"r");
      i++;
      flag_in = true;
    }else if(strcmp(args[i],">") == 0){
      *outfile = fopen(args[i+1],"w");
      i++;
      flag_out = true;
    }
    if(flag_in && flag_out)return true;
  }
  if(flag_in || flag_out)return true;
  return false;
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
    // printf ("'%s'\n", (*head_path)[num_of_paths]);
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
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      int cpid = fork();
      if(cpid > 0){//parent process
        /*如果不wait，二者会同时读取命令*/
        waitpid(cpid,NULL,0);
      }else if(cpid == 0){//child
        complete_and_run(tokens);
      }else{
        printf("fork failed\n");
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
