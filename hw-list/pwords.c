/*
 * Word count application with one thread per input file.
 *
 * You may modify this file in any way you like, and are expected to modify it.
 * Your solution must read each input file from a separate thread. We encourage
 * you to make as few changes as necessary.
 */

/*
 * Copyright © 2021 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>

#include "word_count.h"
#include "word_helpers.h"

struct thread_arg{
  FILE* file;
  word_count_list_t* word_counts;
};
typedef struct thread_arg thread_arg;

void* threadfun(void* arg) {
  printf("Thread starting\n");
  thread_arg* t_arg = (thread_arg*) arg;

  count_words(t_arg->word_counts, t_arg->file);
  fclose(t_arg->file);
  pthread_exit(NULL);
}

/*
 * main - handle command line, spawning one thread per file.
 */
int main(int argc, char* argv[]) {

  static int begin,finish;
  begin = time(NULL);
  /* Create the empty data structure. */
  word_count_list_t word_counts;
  init_words(&word_counts);

  if (argc <= 1) {//程序从stdin读入word
    /* Process stdin in a single thread. */
    count_words(&word_counts, stdin);
  } else {//argc > 1
    /* TODO, 从文件中读取word */
    

    thread_arg thread_arg_temp[argc - 1];
    //有多少文件就要创建多少个线程进行处理
    pthread_t threads[argc - 1];

    for(int i = 1; i <= argc -1 ; i++){
      thread_arg_temp[i-1].file = fopen(argv[i],"r");
      thread_arg_temp[i-1].word_counts = &word_counts;
      // pthread_mutex_init(&(thread_arg_temp->word_counts->lock), NULL);
      int rc = pthread_create(&threads[i-1], NULL, threadfun, (void*)&thread_arg_temp[i-1]);
      if(rc){
        printf("ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
      }
    }
    for (int i = 1; i <= argc - 1; i++) {
      pthread_join(threads[i-1], NULL);
    }




  /* Output final result of all threads' work. */
  // wordcount_sort(&word_counts, less_count);

  FILE* outfile = fopen("word_count_res.txt","w");
  fprint_words(&word_counts, outfile);
  fclose(outfile);

  finish = time(NULL);
  printf("The total time is: %d seconds\n", finish - begin);
  // printf("finish\n");

  return 0;
}
}
