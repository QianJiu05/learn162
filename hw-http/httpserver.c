#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

#define READ_BUFFER 1024
#define CHILD_NUM 100
#define backlog 1024

struct double_end_fd{
  int fd;
  int target_fd;
};
/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue; // Only used by poolserver
int num_threads; // Only used by poolserver
int server_port; // Default value: 8000
char* server_files_directory;
char* server_proxy_hostname;
int server_proxy_port;

void* listen_func(struct double_end_fd *fds);
void* back_func(struct double_end_fd *fds);

void* head_func();
void* body_func();

/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's reponsibility to ensure that the file stored at `path` exists.
 * 
 * 将存储在 `path` 中的文件内容提供给客户端套接字 `fd`。调用者有责任确保存储在 `path` 中的文件存在。
 */
void serve_file(int fd, char* path) {

  /* TODO: PART 2 */
  /* PART 2 BEGIN */
  // FILE* file = fopen(path,"rb");
  int file_fd = open(path,O_RDONLY);//直接打开就行，在外面已经检测过存在了
  
  off_t _fileSize = lseek(file_fd,0,SEEK_END);
  lseek(file_fd,0,SEEK_SET);
  char fileSize[32];
  snprintf(fileSize,sizeof(fileSize),"%ld",_fileSize);

  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(path));
  http_send_header(fd, "Content-Length", fileSize);//%s,%s格式的
  http_end_headers(fd);

  char buffer[READ_BUFFER];
  ssize_t byte_read;
  while((byte_read = read(file_fd,buffer,READ_BUFFER)) > 0){//-1 Err, 0 EOF, >0 num read
    if(write(fd, buffer,byte_read) < 0){//发送失败了
      break;
    }
  }

  close(file_fd);
  /* PART 2 END */
}

void serve_directory(int fd, char* path) {
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  http_end_headers(fd);

  /* TODO: PART 3 */
  /* PART 3 BEGIN */

  // TODO: Open the directory (Hint: opendir() may be useful here)
  DIR* dir = opendir(path);
  if(dir == NULL){
    perror("failed open dir!");
  }

  /**
   * TODO: For each entry in the directory (Hint: look at the usage of readdir() ),
   * send a string containing a properly formatted HTML. (Hint: the http_format_href()
   * function in libhttp.c may be useful here)
   * 
   * 对于目录中的每个条目（提示：查看 readdir() 的用法），
   * 发送一个包含正确格式的 HTML 的字符串。
   * （提示：libhttp.c 中的 http_format_href()函数可能有用）
   */
  struct dirent *entry;
  char child[CHILD_NUM][256];//这里用一个数组存储文件，直接申请一个大的，省的去变长
  // struct stat file_stat;
  int child_idx = 0;
  while((entry = readdir(dir)) != NULL){//访问该目录下所有文件
    
    // 跳过 . 和 .. 目录
    if(strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0){
      continue;
    }

    if(strcmp("index.html",entry->d_name) == 0){
    /*If the directory contains an index.html file, respond with a 200 OK 
    and the full contents of the index.html file. You may not assume that 
    directory requests will have a trailing slash in the query string.
    The http_format_index function in libhttp.c may be useful.*/
      char index_path[100];
      http_format_index(index_path,path);
      serve_file(fd,index_path);
      closedir(dir);
      return;
    }

    if(child_idx < CHILD_NUM){
      fprintf(stdout,"see name = %s",entry->d_name);
      //把目录的子内容在format处理过后存入child
      http_format_href(child[child_idx],path,entry->d_name);
      child_idx++;
    }else{
      break;
    }

  }
  for(int i = 0; i < child_idx; i++){
    write(fd,child[i],strlen(child[i]));
  }
  
  

  closedir(dir);

  /* PART 3 END */
}

/*
 * Reads an HTTP request from client socket (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 *
 *   Closes the client socket (fd) when finished.
 * 
 *  从客户端套接字 (fd) 读取 HTTP 请求，并写入 HTTP 响应。
    1) 如果用户请求的文件已存在，则返回该文件。
    2) 如果用户请求的目录存在 index.html 文件，则发送该 index.html 文件。
    3) 如果用户请求的目录不存在 index.html 文件，则发送该目录中的文件列表及其链接。
    4) 发送 404 Not Found 响应。完成后关闭客户端套接字 (fd)。
 */
void handle_files_request(int fd) {

  struct http_request* request = http_request_parse(fd);

  if (request == NULL || request->path[0] != '/') {
    http_start_response(fd, 400);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }

  if (strstr(request->path, "..") != NULL) {
    http_start_response(fd, 403);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }

  /* Add `./` to the beginning of the requested path */
  char* path = malloc(2 + strlen(request->path) + 1);
  path[0] = '.';
  path[1] = '/';
  memcpy(path + 2, request->path, strlen(request->path) + 1);

  /*
   * TODO: PART 2 is to serve files. If the file given by `path` exists,
   * call serve_file() on it. Else, serve a 404 Not Found error below.
   * The `stat()` syscall will be useful here.
   * 
   * 第二部分是提供文件服务。如果 `path` 指定的文件存在，则调用 serve_file() 函数。
   * 否则，返回下面的 404 Not Found 错误。
   * `stat()` 系统调用在这里会很有用。
   *
   * TODO: PART 3 is to serve both files and directories. You will need to
   * determine when to call serve_file() or serve_directory() depending
   * on `path`. Make your edits below here in this function.
   * 
   * 第 3 部分是同时提供文件和目录服务。
   * 您需要根据 `path` 确定何时调用 serve_file() 或 serve_directory()。
   * 请在此函数中进行以下编辑。
   */

  /* PART 2 & 3 BEGIN */
  struct stat file_assert;
  if(stat(path,&file_assert) == 0){//文件存在

    if(S_ISREG(file_assert.st_mode)){//普通文件
      serve_file(fd,path);
    }else if(S_ISDIR(file_assert.st_mode)){//目录
      serve_directory(fd,path);
    }

  }else{//不存在

    http_start_response(fd, 404);
    http_send_header(fd, "Content-Type","text/plain");

    // http_send_header(fd, "Content-Type","text/html");
    http_end_headers(fd);

  }
  
  /* PART 2 & 3 END */

  close(fd);
  return;
}

/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target_fd. HTTP requests from the client (fd) should be sent to the
 * proxy target (target_fd), and HTTP responses from the proxy target (target_fd)
 * should be sent to the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 *
 *   Closes client socket (fd) and proxy target fd (target_fd) when finished.
 * 
 *  打开与代理目标 (hostname=server_proxy_hostname 和
 *  port=server_proxy_port) 的连接，并将流量中继到/来自流 fd 和
 *  代理 target_fd。来自客户端 (fd) 的 HTTP 请求应发送到
 *  代理目标 (target_fd)，来自代理目标 (target_fd) 的 HTTP 响应应发送到
 *  客户端 (fd)。
 *  完成后关闭客户端套接字（fd）和代理目标 fd（target_fd）。
 * 
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and
  * opens a connection to it. Please do not modify.
  */
  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  // Use DNS to resolve the proxy target's IP address
  struct hostent* target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  // Create an IPv4 TCP socket to communicate with the proxy target.
  int target_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (target_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    close(fd);
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    close(target_fd);
    close(fd);
    exit(ENXIO);
  }

  char* dns_address = target_dns_entry->h_addr_list[0];

  // Connect to the proxy target.
  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status =
      connect(target_fd, (struct sockaddr*)&target_address, sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(target_fd);
    close(fd);
    return;
  }

  /* TODO: PART 4 */
  /* PART 4 BEGIN */
  // dup2() 只是复制文件描述符，让两个描述符指向同一个文件/socket，但不会在它们之间传输数据。
  //select或者创建线程监听socket,题目要求pthread
  pthread_t listen, back; 
  struct double_end_fd fds = {fd,target_fd};
  pthread_create(&listen,NULL,listen_func,&fds);//创建监听线程，转发给target
  pthread_create(&back,NULL,back_func,&fds);//把target的内容转发回client

  pthread_join(listen, NULL);
  pthread_join(back, NULL);

  close(fd);
  close(target_fd);

  /* PART 4 END */
}
//fd -> target_fd
void* listen_func(struct double_end_fd *fds){
  int fd = fds->fd;
  int target_fd = fds->target_fd;
  char buffer[READ_BUFFER];
  ssize_t read_byte;

  while( (read_byte = read(fd,buffer,sizeof(buffer))) > 0){
    if(write(target_fd,buffer,read_byte) <= 0){
      //write = 0： 连接可能关闭； = 1： 写入错误
      break;
    }
  }

  return NULL;
}
//target_fd -> fd
void* back_func(struct double_end_fd *fds){
  int fd = fds->fd;
  int target_fd = fds->target_fd;
  char buffer[READ_BUFFER];
  ssize_t read_byte;

  while( (read_byte = read(target_fd,buffer,sizeof(buffer))) > 0){
    if(write(fd,buffer,read_byte) <= 0){
      break;
    }
  }

  return NULL;
}
#ifdef POOLSERVER
/*
 * All worker threads will run this function until the server shutsdown.
 * Each thread should block until a new request has been received.
 * When the server accepts a new connection, a thread should be dispatched
 * to send a response to the client.
 * 
 * 所有工作线程都将运行此函数，直到服务器关闭。每个线程都应阻塞，直到收到新的请求。
 * 当服务器接受新的连接时，应调度一个线程向客户端发送响应。
 */
void* handle_clients(void* void_request_handler) {
  void (*request_handler)(int) = (void (*)(int))void_request_handler;
  /* (Valgrind) Detach so thread frees its memory on completion, since we won't
   * be joining on it. */
  pthread_detach(pthread_self());

  /* TODO: PART 7 */
  /* PART 7 BEGIN */
  while(1){
    int fd= wq_pop(&work_queue);
    request_handler(fd);
    close(fd);//工作线程处理完关闭客户端
  }
  
  

  /* PART 7 END */
}

/*
 * Creates `num_threads` amount of threads. Initializes the work queue.
 */
void init_thread_pool(int num_threads, void (*request_handler)(int)) {

  /* TODO: PART 7 */
  /* PART 7 BEGIN */
  wq_init(&work_queue);//wq是全局变量

  for(int i = 0; i < num_threads; i++){
    pthread_t tid;
    if (pthread_create(&tid,NULL,handle_clients,request_handler) != 0){
      //绑定request handler到handle clients
      perror("thread create failed");
      exit(-1);
    }
  }
  /* PART 7 END */
}
#endif

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 * 
 * 在所有端口号为 PORTNO 的接口上打开一个 TCP 流套接字。将服务器套接字的 fd 号保存在 *socket_number 中。
 * 对于每个已接受的连接，使用已接受的 fd 号调用 request_handler。
 */
void serve_forever(int* socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  // Creates a socket for IPv4 and TCP.
  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option, sizeof(socket_option)) ==
      -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  // Setup arguments for bind()
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  /*
   * TODO: PART 1
   *
   * Given the socket created above, call bind() to give it
   * an address and a port. Then, call listen() with the socket.
   * An appropriate size of the backlog is 1024, though you may
   * play around with this value during performance testing.
   * 
   * 给定上面创建的套接字，调用 bind() 为其指定地址和端口。然后，使用该套接字调用 listen()。
   * 合适的 backlog 大小为 1024，不过您可以在性能测试期间调整此值。
   */

  /* PART 1 BEGIN */
  // int backlog = 1024;
  // bind(*socket_number,&server_address,sizeof(server_address));
  if (bind(*socket_number,&server_address, sizeof(server_address)) < 0) {
    perror("Failed to bind socket");
    exit(errno);
  }

  if (listen(*socket_number,backlog) == -1){//只需要一次listen即可永远accept？
    perror("failed to listen");
    exit(errno);
  }



  /* PART 1 END */
  printf("Listening on port %d...\n", server_port);
#ifdef POOLSERVER
  /*
   * The thread pool is initialized *before* the server
   * begins accepting client connections.
   */

  init_thread_pool(num_threads, request_handler);

#endif

  while (1) {
    client_socket_number = accept(*socket_number, (struct sockaddr*)&client_address,
                                  (socklen_t*)&client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n", inet_ntoa(client_address.sin_addr),
           client_address.sin_port);

#ifdef BASICSERVER
    /*
     * This is a single-process, single-threaded HTTP server.
     * When a client connection has been accepted, the main
     * process sends a response to the client. During this
     * time, the server does not listen and accept connections.
     * Only after a response has been sent to the client can
     * the server accept a new connection.
     */
    request_handler(client_socket_number);

#elif FORKSERVER
    /*
     * TODO: PART 5
     *
     * When a client connection has been accepted, a new
     * process is spawned. This child process will send
     * a response to the client. Afterwards, the child
     * process should exit. During this time, the parent
     * process should continue listening and accepting
     * connections.
     * 
     * 当客户端连接被接受后，会生成一个新的进程。
     * 该子进程将向客户端发送响应。
     * 之后，子进程应该退出。在此期间，父进程应该继续监听并接受连接。
     */

    /* PART 5 BEGIN */
    pid_t pid = fork();
    if(pid == 0){//子进程
      // handle_files_request(fd);
      request_handler(client_socket_number);
      exit(0);
    }else if(pid > 0){//父进程
      close(client_socket_number);//关闭客户端
      
    }else{
      perror("fork failed");
    }
    /* PART 5 END */

#elif THREADSERVER
    /*
     * TODO: PART 6
     *
     * When a client connection has been accepted, a new
     * thread is created. This thread will send a response
     * to the client. The main thread should continue
     * listening and accepting connections. The main
     * thread will NOT be joining with the new thread.
     * 
     * 当客户端连接被接受后，会创建一个新的线程。
     * 该线程将向客户端发送响应。
     * 主线程应继续监听并接受连接。主程不会与新线程合并。
     */

    /* PART 6 BEGIN */

    pthread_t tid;
    if(pthread_create(&tid,NULL,request_handler,(void*)&client_socket_number) != 0){
      perror("create thread failed");
      close(client_socket_number);
      continue;
    };
    pthread_detach(tid);//线程运行结束后自动回收资源

    /* PART 6 END */
#elif POOLSERVER
    /*
     * TODO: PART 7
     *
     * When a client connection has been accepted, add the
     * client's socket number to the work queue. A thread
     * in the thread pool will send a response to the client.
     */

    /* PART 7 BEGIN */
    wq_push(&work_queue,client_socket_number);
    

    /* PART 7 END */
#endif
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}


int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0)
    perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char* USAGE =
    "Usage: ./httpserver --files some_directory/ [--port 8000 --num-threads 5]\n"
    "       ./httpserver --proxy inst.eecs.berkeley.edu:80 [--port 8000 --num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
  signal(SIGINT, signal_callback_handler);
  signal(SIGPIPE, SIG_IGN);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char* proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char* colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char* server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char* num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

#ifdef POOLSERVER
  if (num_threads < 1) {
    fprintf(stderr, "Please specify \"--num-threads [N]\"\n");
    exit_with_usage();
  }
#endif

  chdir(server_files_directory);
  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
