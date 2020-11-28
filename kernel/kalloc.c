// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

//original one
// struct run {
//   struct run *next;
// };

// struct {
//   struct spinlock lock;
//   struct run *freelist;
// } kmem;


//my implementation
struct run {
  struct run *next;
};

struct kmem{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];


//original one
// void
// kinit()
// {
//   initlock(&kmem.lock, "kmem");
//   freerange(end, (void*)PHYSTOP);
// }

//my implementation
void
kinit()
{
  //loop to init every list
  int i;
  for(i=0; i<NCPU; ++i){
    initlock(&kmems[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}


//unmodified
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
//original one
// void
// kfree(void *pa)
// {
//   struct run *r;

//   if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
//     panic("kfree");

//   // Fill with junk to catch dangling refs.
//   memset(pa, 1, PGSIZE);

//   r = (struct run*)pa;

//   acquire(&kmem.lock);
//   r->next = kmem.freelist;
//   kmem.freelist = r;
//   release(&kmem.lock);
// }

//my implementation
void kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  //get cpuid
  push_off();
  int cpu = cpuid()%NCPU;
  pop_off();
  if(cpu < 0 || cpu >= NCPU)
    panic("cpuid");
  
  acquire(&kmems[cpu].lock);
  r->next = kmems[cpu].freelist;
  kmems[cpu].freelist = r;
  release(&kmems[cpu].lock);

}





// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
//original one
// void *
// kalloc(void)
// {
//   struct run *r;

//   acquire(&kmem.lock);
//   r = kmem.freelist;
//   if(r)
//     kmem.freelist = r->next;
//   release(&kmem.lock);

//   if(r)
//     memset((char*)r, 5, PGSIZE); // fill with junk
//   return (void*)r;
// }

//my implementation
// struct run* stealmem(int cpu)
// {  
//   int i;
//   struct run* p;
//   for(i=0; i<NCPU; ++i ){
//     if(i != cpu){
//       acquire(&kmems[i].lock);
//       p = kmems[i].freelist;
//       if(p){
//         kmems[i].freelist = p->next;
//         release(&kmems[i].lock);
//         return p;
//       }
//       else
//         release(&kmems[i].lock);
//     }
//   }
//   //not found free memory from other cpus
//   return 0;
// }

void* kalloc(void)
{
  struct run *r;
  //get cpuid
  push_off();
  int cpu = cpuid();
  pop_off();

  if(cpu < 0 || cpu >= NCPU)
    panic("cpuid");
  
  acquire(&kmems[cpu].lock);
  r = kmems[cpu].freelist;
  if(r){
    kmems[cpu].freelist = r->next;
  }
  release(&kmems[cpu].lock);  
  if(!r)
  {
    int i;
    int flag = 0;
    for(i=0; i<NCPU; i++){
      if(flag)
        break;
      if(i != cpu){
        acquire(&kmems[i].lock);
        r = kmems[i].freelist;
        if(r){
          kmems[i].freelist = r->next;
          flag = 1;
        }
        release(&kmems[i].lock);
      }
    }

  }
  

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
  }

  return (void*)r;
}