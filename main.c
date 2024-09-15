#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// assert(sizeof(cell) == sizeof(ptr) == sizeof(fptr))
typedef volatile long long cell;
typedef volatile char * volatile ptr;
typedef void (*fptr)(void);

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
  char flen;
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

dict_entry *find(struct tag tag)
{
  dict_entry *curr = (dict_entry *)link;
  while (NULL != curr) {
    if (tag.len == (curr->flen & 0x1F) &&
	0 == memcmp(&tag.name, &curr->name, tag.len)) {
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
fptr **ip;
/* w is the pointer to the current xt that is actually being executed */
fptr **w;

cell in;
#define TIB_SIZE 4096
cell tib[TIB_SIZE];

void go(void *addr) {
    long long ptr = (long long)&addr;
    ((void * volatile *)ptr)[1] = addr;
}
#define GOTO(addr) go(addr);

#define NEXT w = (fptr **)*ip; ip++; GOTO(*w)
#define MSG(msg) puts((msg));
[[gnu::naked]]
void fth_docon(void) { PUSH(dsp, *((*(cell **)w)+1)); NEXT }
[[gnu::naked]]
void fth_docol(void) { PUSH(rsp, (cell)ip); ip = w+1; NEXT }
[[gnu::naked]]
void fth_exit(void)  { ip = (fptr **)POP(rsp); NEXT }
[[gnu::naked]]
void fth_push(void)  { w = (void *)*ip++; PUSH(dsp, (cell)w); NEXT }
[[gnu::naked]]
void fth_fetch(void) { dsp[1] = *(cell *)dsp[1]; NEXT }
[[gnu::naked]]
void fth_store(void) { *(cell *)dsp[1] = dsp[2]; dsp += 2; NEXT }
[[gnu::naked]]
void fth_create(void) {
  while (isspace(tib[in])) { in++; }
  int end = in;
  while (isgraph(tib[end]) || (0x80 & tib[end])) { end++; }
  if (end - in > 31) { /* TODO Abort */ }
  dict_entry *d = (dict_entry *)cp;
  d->flen = end - in;
  memcpy(d->name, (const void *)(tib + in), end - in);
  d->link = link;
  link = d;
  cp += sizeof(dict_entry);
  NEXT;
}
/* This dodoes implementation */
[[gnu::naked]]
void fth_dodoes(void) {
  PUSH(rsp, (cell)ip);
  PUSH(dsp, (cell)w+2);
  ip = (fptr **)*(w+1);
  NEXT
}
[[gnu::naked]]
void fth_does(void) {
  link->xt = fth_dodoes;
  link->param[0] = (ptr)(w+1);
  cp += sizeof(link->param[0]);
  ip = (fptr **)POP(rsp);
  NEXT
}
void fth_rightbracket(void) {
  while (1) {
  }
}
[[gnu::naked]]
void fth_tor(void)   { PUSH(rsp, POP(dsp)); NEXT }
[[gnu::naked]]
void fth_rfrom(void) { PUSH(dsp, POP(rsp)); NEXT }
[[gnu::naked]]
void fth_dup(void)   { dsp[0] = dsp[1]; dsp--; NEXT }
[[gnu::naked]]
void fth_swap(void)  {
  cell tmp = dsp[1]; dsp[1] = dsp[2]; dsp[2] = tmp;
NEXT; }
[[gnu::naked]]
void fth_execute(void) { w = (void (***)(void))POP(dsp); GOTO(*w) }
[[gnu::naked]]
void fth_find(void)  {
  dict_entry *result = find(*(struct tag *)(dsp + 1));
  if (NULL == result) {
    PUSH(dsp, 0); NEXT;
  }
  dsp[1] = (cell)&result->xt;
  PUSH(dsp, result->flen & 0x80 ? 1 : -1);
  NEXT;
}

struct {
  fptr push;
} kernel_words =
  {
    fth_push,
  };

#define K(name) ((fptr *)&kernel_words.name),
#define C(name) (&find(strtotag(#name))->xt),
#define LIST_OF_WORD							\
  X(exit, 0, fth_exit, )						\
    X(@, 0, fth_fetch, )						\
    X(!, 0, fth_store, )						\
    X(>in, 0, fth_docon, (void *)&in)					\
    X(does>, 0, fth_does, )						\
    X(>r, 0, fth_tor, )							\
    X(r>, 0, fth_rfrom, )						\
    X(dup, 0, fth_dup, )						\
    X(swap, 0, fth_swap, )						\
    X(execute, 0, fth_execute, )					\
    X(over, 0, fth_docol, C(>r) C(dup) C(r>) C(swap) C(exit) )		\
    X(dup2, 0, fth_docol, C(over) C(over) C(exit))

#define X(NAME, FLAG, CP, PARAM)					\
  do {									\
    entry = (dict_entry){						\
      (FLAG) | (sizeof(#NAME) - 1),					\
      #NAME,								\
      (dict_entry *)link,						\
      CP								\
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
#undef LIST_OF_CODE
  
}

void dict_dump(void)
{
  char *begin = (char *)dict;
  FILE *fp = fopen("dump", "w+");
  while (begin < (char *)cp) {
    putc(*begin++, fp);
  }
  fclose(fp);
}

fptr **bootstrap = NULL;

/* avoid touching anything in the ctx function! */
void ctx(void)
{
  volatile int x = 0;
  (void)x;
  GOTO(*w);
  x = 0;
}

int main()
{
  build_core_code();
  dict_dump();

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
    &done_ptr
  };
  void (*ctxptr)(void) = &ctx;
  void (**p)(void) = &ctxptr;
  done_ptr = (fptr)((*(long long*)p) + 34);
  
  ip = (fptr **)&bt;
  w = (fptr **)*ip++;
  
  ctx();

  stacks_print();

  return 0;
}
