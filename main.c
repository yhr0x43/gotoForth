#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* assert(sizeof(cell) == sizeof(ptr) == sizeof(fptr)) */
typedef volatile uint64_t cell;
typedef volatile char * volatile ptr;
typedef void *(*(*fptr)(void))(void);

/* stack definition */
#define DSTACK_SIZE 4096
#define RSTACK_SIZE 4096
cell dstack[DSTACK_SIZE], rstack[RSTACK_SIZE];
cell * const DSTACK_TOP = dstack + DSTACK_SIZE - 1;
cell * const RSTACK_TOP = rstack + RSTACK_SIZE - 1;
int stack_empty(cell *sp) {
  return sp == DSTACK_TOP || sp == RSTACK_TOP;
}
cell * volatile dsp = DSTACK_TOP, * volatile rsp = RSTACK_TOP;

#define PUSH(s, x) (*(s)-- = (x))
#define POP(s) (stack_empty(s) ? (assert(0 && "stack underflow"), 0) : *++(s))

void stack_print(cell *sp)
{
  while (!stack_empty(sp)) {
    printf("%llx, ", (long long)*++sp);
  }
  puts("");
}

void stacks_print(void) {
  printf("DSP> ");
  stack_print(dsp);
  printf("RSP> ");
  stack_print(rsp);
}

/* dict definition */
typedef struct dict_entry {
  unsigned char flen;
  char name[31];
  struct dict_entry *link;
  fptr xt;
  ptr param[];
} dict_entry;

#define DICT_SIZE 4096
cell dict[DICT_SIZE];

ptr cp = (ptr)dict;
dict_entry *link = NULL;

struct tag {
  char len;
  char name[31];
};

int imemcmp(const void *s1, const void *s2, size_t n)
{
  const char *c1 = (const char *)s1, *c2 = (const char *)s2;
  while (n > 0) {
    if (tolower(*c1) != tolower(*c2)) {
      return tolower(*c1) - tolower(*c2);
    }
    c1++; c2++; n--;
  }
  return 0;
}

dict_entry *find(struct tag tag)
{
  dict_entry *curr = (dict_entry *)link;
  while (NULL != curr) {
    if (tag.len == (curr->flen & 0x1F) &&
	0 == imemcmp(&tag.name, &curr->name, tag.len)) {
      return curr;
    }
    curr = curr->link;
  }
  return NULL;
}

struct tag strtotag(char str[]) {
  struct tag tag;
  tag.len = strlen(str);
  strncpy(tag.name, str, 15);
  return tag;
}

/* ip sometimes points directly to a function pointer, sometimes it points to
   a pointer to a function pointer, because dereference a function pointer
   give the "funcion" (i.e. it does nothing), ip is better be a pointer to a 
   pointer to a function pointer */
/* in general, ip points to the next pointer of xt to be executed */
fptr **ip;
/* w is the pointer to the current xt that is actually being executed */
fptr **w;

cell in;
#define TIB_SIZE 4096
volatile char tib[TIB_SIZE];

#define NEXT w = (fptr **)*ip; ip++; return *w;
fptr *fth_docon(void) { PUSH(dsp, *((*(cell **)w)+1)); NEXT }
fptr *fth_docol(void) { PUSH(rsp, (cell)ip); ip = w+1; NEXT }
fptr *fth_exit(void)  { ip = (fptr **)POP(rsp); NEXT }
fptr *fth_push(void)  { PUSH(dsp, (cell)*ip++); NEXT }
fptr *fth_fetch(void) { dsp[1] = *(cell *)dsp[1]; NEXT }
fptr *fth_store(void) { *(cell *)dsp[1] = dsp[2]; dsp += 2; NEXT }
fptr *fth_tor(void)   { PUSH(rsp, POP(dsp)); NEXT }
fptr *fth_rfrom(void) { PUSH(dsp, POP(rsp)); NEXT }
fptr *fth_execute(void) { w = (fptr **)POP(dsp); return *w; }
fptr *fth_dodoes(void) {
  PUSH(rsp, (cell)ip);
  PUSH(dsp, (cell)w+2);
  ip = (fptr **)*(w+1);
  NEXT;
}
fptr *fth_does(void) {
  link->xt = (fptr)fth_dodoes;
  link->param[0] = (ptr)(w+1);
  cp += sizeof(link->param[0]);
  ip = (fptr **)POP(rsp);
  NEXT;
}
fptr *fth_gotoif(void) {
  if (POP(dsp)) {
    ip = (fptr **)*ip; NEXT;
  } else {
    ip++; NEXT;
  }
}
fptr *fth_refill(void) {
  if (NULL == fgets((char *)tib, TIB_SIZE, stdin)) {
    /* TODO abort */
  }
  in = 0;
  NEXT;
}
fptr *fth_emit(void) { putchar(POP(dsp)); NEXT; }
fptr *fth_find(void)  {
  dict_entry *result = find(*(struct tag *)POP(dsp));
  if (NULL == result) {
    PUSH(dsp, 0); NEXT;
  }
  PUSH(dsp, (cell)&result->xt);
  PUSH(dsp, result->flen & 0x80 ? 1 : -1);
  NEXT;
}
fptr *fth_word(void) {
  while (isspace(tib[in])) { in++; }
  int begin = in;
  while (isgraph(tib[in]) || (0x80 & tib[in])) { in++; }
  if (in - begin > 63) { /* TODO decide the max length and abort */ }
  *(char *)cp = in - begin;
  memcpy((char *)cp + 1, (const void *)(tib + begin), in - begin);
  PUSH(dsp, (cell)cp);
  NEXT;
}
fptr *fth_drop(void) { POP(dsp); NEXT }
fptr *fth_dup(void)   { dsp[0] = dsp[1]; dsp--; NEXT }
fptr *fth_swap(void)  {
  cell tmp = dsp[1]; dsp[1] = dsp[2]; dsp[2] = tmp;
NEXT; }

struct {
  fptr push;
  fptr gotoif;
} kernel_words =
  {
    (fptr)fth_push,
    (fptr)fth_gotoif,
  };

#define K(name) ((fptr *)&kernel_words.name),
#define C(name) (&find(strtotag(#name))->xt),
#define N ,
#define LIST_OF_WORD							\
  X(exit, 0, fth_exit, )						\
  X(@, 0, fth_fetch, )							\
  X(!, 0, fth_store, )							\
  X(>in, 0, fth_docon, (void *)&in)					\
  X(does>, 0, fth_does, )						\
  X(>r, 0, fth_tor, )							\
  X(r>, 0, fth_rfrom, )							\
  X(dup, 0, fth_dup, )							\
  X(drop, 0, fth_drop, )						\
  X(swap, 0, fth_swap, )						\
  X(refill, 0, fth_refill, )						\
  X(execute, 0, fth_execute, )						\
  X(word, 0, fth_word, )						\
  X(find, 0, fth_find, )						\
  X([, 0x80, fth_docol,							\
     C(refill) C(word) C(find)						\
     K(gotoif) (void **)cp+5 N						\
     C(execute)								\
     K(push) (void *)1 N K(gotoif) (void *)cp)				\
  X(over, 0, fth_docol, C(>r) C(dup) C(r>) C(swap) C(exit) )		\
  X(dup2, 0, fth_docol, C(over) C(over) C(exit))

#define X(NAME, FLAG, CP, PARAM)					\
  do {									\
    entry = (dict_entry){						\
      (FLAG) | (sizeof(#NAME) - 1),					\
      #NAME,								\
      (dict_entry *)link,						\
      (fptr)CP								\
    };									\
    memcpy((void *)cp, &entry, sizeof(dict_entry));			\
    link = (dict_entry *)cp;						\
    cp += sizeof(dict_entry);						\
    void *params[] = { 0, PARAM };					\
    params[0] = (fptr *)cp + 1;						\
    memcpy((void *)cp, params + 1, sizeof(params) - sizeof(*params));	\
    cp += sizeof(params) - sizeof(*params);				\
  } while (0);
  
void build_core_code(void) {
  dict_entry entry;
  LIST_OF_WORD;
#undef X
#undef LIST_OF_WORD
}

void dict_dump(void)
{
  FILE *fp = fopen("dump", "w+");
  size_t dict_size = (char *)cp - (char *)dict;
  if (dict_size != fwrite((void *)dict, 1, dict_size, fp)) {
    perror("dump");
    fclose(fp);
    return;
  }
  fclose(fp);
}

fptr **bootstrap = NULL;

void ctx(void)
{
  fptr curr = *(fptr *)w;
  while (NULL != curr) {
    curr = (fptr)curr();
  }
}

int main()
{
  build_core_code();
  // dict_dump();

  PUSH(dsp, 6);
  PUSH(dsp, 2);

  stacks_print();

  dict_entry *dup2;
  if (NULL == (dup2 = find(strtotag("dup2")))) {
    puts("dup2 not found\n");
    return 1;
  }

  PUSH(dsp, (long long)&dup2->xt);

  dict_entry *execute;
  if (NULL == (execute = find(strtotag("execute")))) {
    puts("execute not found\n");
    return 1;
  }

  fptr done_ptr = NULL;
  fptr *bt[] = {
    C(execute)
    K(push)
    C(dup2)
    C(execute)
    C([)
    &done_ptr
  };
  
  ip = (fptr **)&bt;
  w = (fptr **)*ip++;
  
  ctx();

  stacks_print();

  return 0;
}
