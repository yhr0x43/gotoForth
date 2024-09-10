#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// assert(sizeof(cell) == sizeof(ptr) == sizeof(fptr))
typedef long long cell;
typedef char *ptr;
typedef void (*fptr)(void);

/* stack definition */
#define DSTACK_SIZE 4096
#define RSTACK_SIZE 4096
cell dstack[DSTACK_SIZE], rstack[RSTACK_SIZE];
cell *dsp = dstack + DSTACK_SIZE - 1, *rsp = rstack + RSTACK_SIZE - 1;

#define PUSH(s, x) (*(s)-- = (x))
#define POP(s) (*++(s))

void stack_print(cell *sp)
{
  while (sp != dstack + DSTACK_SIZE - 1 && sp != rstack + RSTACK_SIZE - 1) {
    printf("%llx, ", (long long)*++sp);
  }
  puts("");
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

#if __GNUC__
#define GOTO(addr) asm volatile("jmp *%0" : : "r" (addr));
#else
void go(void *addr) {
  (&addr)[2] = addr;
}
#define GOTO(addr) go((char *)addr);
#endif

#define NEXT asm volatile("leave"); w = (fptr **)*ip; ip++; GOTO(*w)
void fth_docon(void) { PUSH(dsp, *((*(cell **)w)+1)); NEXT }
void fth_docol(void) { PUSH(rsp, (cell)ip); ip = w+1; NEXT }
void fth_exit(void)  { ip = (fptr **)POP(rsp); NEXT }
void fth_push(void)  { w = (void *)*ip++; PUSH(dsp, (cell)w); NEXT }
void fth_fetch(void) { dsp[1] = *(cell *)dsp[1]; NEXT }
void fth_store(void) { *(cell *)dsp[1] = dsp[2]; dsp += 2; NEXT }
void fth_create(void) {
  while (isspace(tib[in])) { in++; }
  int end = in;
  while (isgraph(tib[end]) || (0x80 & tib[end])) { end++; }
  if (end - in > 31) { /* TODO Abort */ }
  dict_entry *d = (dict_entry *)cp;
  d->flen = end - in;
  memcpy(d->name, tib + in, end - in);
  d->link = link;
  link = d;
  cp += sizeof(dict_entry);
  NEXT;
}
/* This dodoes implementation */
void fth_dodoes(void) {
  PUSH(rsp, (cell)ip);
  PUSH(dsp, (cell)w+2);
  ip = (fptr **)*(w+1);
  NEXT
}
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
void fth_tor(void)   { PUSH(rsp, POP(dsp)); NEXT }
void fth_rfrom(void) { PUSH(dsp, POP(rsp)); NEXT }
void fth_dup(void)   { dsp[0] = dsp[1]; dsp--; NEXT }
void fth_swap(void)  {
  cell tmp = dsp[1]; dsp[1] = dsp[2]; dsp[2] = tmp;
NEXT; }
void fth_execute(void) { w = *(void **)&POP(dsp); asm volatile("leave"); GOTO(*w) }
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
  void (*push)(void);
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

/* avoid touching anything in the ctx function! the code rely on the size of code to work! */
void ctx(void)
{
  GOTO(*w);
}

int main()
{
  build_core_code();
  dict_dump();

  PUSH(dsp, 6);
  PUSH(dsp, 2);

  stack_print(dsp);
  stack_print(rsp);  

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
  fptr ctxptr = &ctx;
  done_ptr = (fptr)(*(long long *)&ctxptr + 21);
  
  ip = (fptr **)&bt;
  w = (fptr **)*ip++;
  
  ctx();

  stack_print(dsp);
  stack_print(rsp);

  return 0;
}
