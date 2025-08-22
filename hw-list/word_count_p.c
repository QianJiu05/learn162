/*
 * Implementation of the word_count interface using Pintos lists and pthreads.
 *
 * You may modify this file, and are expected to modify it.
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

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_lp.c"
#endif

#ifndef PTHREADS
#error "PTHREADS must be #define'd when compiling word_count_lp.c"
#endif

#include "word_count.h"

/* 定义
typedef struct word_count {
  char* word;
  int count;
  struct list_elem elem;
} word_count_t;


typedef struct word_count_list {
  struct list lst;
  pthread_mutex_t lock;
} word_count_list_t;
*/

void init_words(word_count_list_t* wclist) { /* TODO */
  list_init(&(wclist->lst));//wclist : struct list*
  // pthread_mutex_init(&wclist->lock, NULL);
}

size_t len_words(word_count_list_t* wclist) {
  /* TODO */

  return list_size(&(wclist->lst)); // Returns the number of elements in the list
}

word_count_t* find_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  struct list_elem* temp = list_begin(&(wclist->lst));
  while(temp != list_end(&(wclist->lst))) {
    word_count_t* wc = list_entry(temp, word_count_t,elem );
    if (strcmp(wc->word, word) == 0) {
      // printf("Found word: %s with count: %d\n", wc->word, wc->count);
      return wc; // Return the word count if found
    }
    temp = list_next(temp); // Move to the next element
  }
  return NULL;
}

word_count_t* add_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  printf("Adding word(wordcout p): %s\n", word);
  word_count_t* wc = NULL;
  if((wc = find_word(wclist, word)) != NULL) {
    wc->count++;
  }else{
    wc = malloc(sizeof(word_count_t));
    wc->word = malloc(strlen(word) + 1); // Allocate space for the word
    strcpy(wc->word, word); // Copy the word into the new_word structure
    wc->count = 1; // Initialize the count to 1
    list_push_back(&(wclist->lst), &wc->elem);

  }
  return wc; // Return the updated word count structure
}

void fprint_words(word_count_list_t* wclist, FILE* outfile) {
  /* TODO */
  /* Please follow this format: fprintf(<file>, "%i\t%s\n", <count>, <word>); */

  struct list_elem* temp = list_begin(&(wclist->lst));
  while(temp != list_end(&(wclist->lst))) {
    word_count_t* wc = list_entry(temp, word_count_t,elem );
    fprintf(outfile,"%i\t%s\n", wc->count, wc->word);
    temp = list_next(temp); // Move to the next element
  }

}

void wordcount_sort(word_count_list_t* wclist,
                    bool less(const word_count_t*, const word_count_t*)) {
  /* TODO */
}
