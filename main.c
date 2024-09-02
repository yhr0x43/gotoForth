#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// assert(8 == sizeof(void *))
typedef volatile long long cell;
typedef volatile void *ptr;

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
  char name[15];
  struct dict_entry *link;
  void (*xt)(void);
} dict_entry;

#define DICT_SIZE 4096
cell dict[DICT_SIZE];

ptr cp = (ptr)dict;
volatile dict_entry *link = NULL;

struct tag {
  char len;
  char name[15];
};

dict_entry *find(struct tag tag)
{
  dict_entry *curr = (dict_entry *)link;
  char *p = (char *)&tag.name;
  for ( ; *p; ++p) *p = tolower(*p);
  while (NULL != curr) {
    if (tag.len == (curr->flen & 0xF) &&
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
void (***volatile ip)(void);
/* w is the pointer to the current xt that is actually being executed */
void (***volatile w)(void);

#if 0
/* #define GOTO(addr) goto *(addr) */
#define GOTO(addr) asm volatile("jmp *%0" : : "r" (addr));
#else // __GNUC__
void go(void *addr) {
  (&addr)[2] = addr;
}
#define GOTO(addr) go(addr)
#endif // __GNUC__

#define MSG puts(__FUNCTION__);

#define NEXT w = (void *)*ip++; asm volatile("leave"); GOTO(*w);
void fth_docol(void) { MSG; PUSH(rsp, (cell)ip); ip = w+1; NEXT }
void fth_exit(void)  { MSG; ip = (void *)POP(rsp); NEXT }
void fth_tor(void)   { MSG; PUSH(rsp, POP(dsp)); NEXT }
void fth_rto(void)   { MSG; PUSH(dsp, POP(rsp)); NEXT }
void fth_dup(void)   { MSG; dsp[0] = dsp[1]; dsp--; NEXT }
void fth_swap(void)  { MSG;
  cell tmp = dsp[1]; dsp[1] = dsp[2]; dsp[2] = tmp;
NEXT; }
void fth_execute(void) { MSG; w = *(void **)&POP(dsp); asm volatile("leave"); GOTO(*w); }
void fth_find(void)  { MSG;
  dict_entry *result = find(*(struct tag *)(dsp + 1));
  if (NULL == result) {
    PUSH(dsp, 0); NEXT;
  }
  dsp[1] = (cell)&result->xt;
  PUSH(dsp, result->flen & 0x80 ? 1 : -1);
  NEXT;
}

#define C(name) (assert(find(strtotag(#name))), &find(strtotag(#name))->xt),
#define LIST_OF_WORD							\
    X(exit, 0, fth_exit, { })						\
    X(>r, 0, fth_tor, { })						\
    X(r>, 0, fth_rto, { })						\
    X(dup, 0, fth_dup, { })						\
    X(swap, 0, fth_swap, { })						\
    X(execute, 0, fth_execute, { })					\
    X(over, 0, fth_docol, { C(>r) C(dup) C(r>) C(swap) C(exit) })	\
    X(dup2, 0, fth_docol, { C(over) C(over) C(exit) })
    
#define X(NAME, FLAG, CP, PARAM)			\
  do {							\
    entry = (dict_entry){				\
      (FLAG) | (sizeof(#NAME) - 1),			\
      #NAME,						\
      (dict_entry *)link,				\
      CP						\
    };							\
    memcpy((void *)cp, &entry, sizeof(dict_entry));	\
    link = cp;						\
    cp += sizeof(dict_entry);				\
    void (**params[])(void) = PARAM;			\
    memcpy((void *)cp, params, sizeof(params));		\
    cp += sizeof(params);				\
  } while (0);
  
void build_core_code(void) {
  dict_entry entry;
  LIST_OF_WORD;
}
#undef X
#undef LIST_OF_CODE
#undef C

void dict_dump(void)
{
  char *begin = (char *)dict;
  FILE *fp = fopen("dump", "w+");
  while (begin < (char *)cp) {
    putc(*begin++, fp);
  }
  fclose(fp);
}

void (*done_ptr)(void) = NULL;
void (**(*bootstrap)[])(void) = NULL;

void ctx(void)
{
  done_ptr = &&done;
  ip = (void (***)(void))*bootstrap;
  w = (void *)*ip++;
  GOTO(*w);
 done:
  /* NOTE if there's no code here, the label will be after cleanup and ret.
     stuff like return; or (void *)0; doesn't help here
  */
  asm volatile("nop");
}

int main()
{
  build_core_code();
  dict_dump();

  PUSH(dsp, 6);
  PUSH(dsp, 2);

  stack_print(dsp);
  stack_print(rsp);

  dict_entry *target;
  if (NULL == (target = find(strtotag("dup2")))) {
    puts("test target not found\n");
    return 1;
  }

  PUSH(dsp, (long long)&target->xt);

  if (NULL == (target = find(strtotag("execute")))) {
    puts("execute target not found\n");
    return 1;
  }

  void (**bt[])(void) = {
    &target->xt,
    &done_ptr
  };
  bootstrap = &bt;
  
  ctx();

  stack_print(dsp);
  stack_print(rsp);

  return 0;
}
