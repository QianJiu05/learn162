/*
 * Implementation of the word_count interface using Pintos lists.
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
#error "PINTOS_LIST must be #define'd when compiling word_count_l.c"
#endif

#include "word_count.h"

//
// typedef struct word_count {
//   char* word;
//   int count;
//   struct list_elem elem;
// } word_count_t;

// typedef struct list word_count_list_t;
//

void init_words(word_count_list_t* wclist) { /* TODO */

  list_init(wclist);//wclist : struct list*

}

size_t len_words(word_count_list_t *wclist) {
  /* TODO */
  return list_size(wclist); // Returns the number of elements in the list
}

word_count_t* find_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  struct list_elem* temp = list_begin(wclist);
  while(temp != list_end(wclist)) {
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
  printf("Adding word(wordcount_c): %s\n", word);
  word_count_t* wc = NULL;
  if((wc = find_word(wclist, word)) != NULL) {
    wc->count++;
  }else{
    wc = malloc(sizeof(word_count_t));
    wc->word = malloc(strlen(word) + 1); // Allocate space for the word
    strcpy(wc->word, word); // Copy the word into the new_word structure
    wc->count = 1; // Initialize the count to 1
    list_push_back(wclist, &wc->elem);

  }

  return wc; // Return the updated word count structure
}

void fprint_words(word_count_list_t* wclist, FILE* outfile) {
  /* TODO */
  /* Please follow this format: fprintf(<file>, "%i\t%s\n", <count>, <word>); */

  struct list_elem* temp = list_begin(wclist);
  while(temp != list_end(wclist)) {
    word_count_t* wc = list_entry(temp, word_count_t,elem );
    fprintf(outfile,"%i\t%s\n", wc->count, wc->word);
    temp = list_next(temp); // Move to the next element
  }
}

/* Sort a word count list using the provided comparator function. */
static bool less_list(const struct list_elem* ewc1, const struct list_elem* ewc2, void* aux) {
  /* TODO */
  //传入list_elem可以找到word cout t，但这不是typedef struct list word_count_list_t的内容
  word_count_t* wc1 = list_entry(ewc1, word_count_t, elem);
  word_count_t* wc2 = list_entry(ewc2, word_count_t, elem);

  // aux 传进来的是比较函数指针
  bool (*less)(const word_count_t*, const word_count_t*) = aux;

  return less(wc1, wc2);
}

//这是提供的比较函数
void wordcount_sort(word_count_list_t* wclist,
                    bool less(const word_count_t*, const word_count_t*)) 
{
  list_sort(wclist, less_list, less);
}
