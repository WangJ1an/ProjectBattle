#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#if 0

一、内核怎样保证各个进程寻址到同一个共享内存区域的内存页面

1、page cache及swap cache中页面的区分：一个被访问文件的物理页面都驻留在page cache或swap cache中，一个页面的所有信息由struct page来描述。
struct page中有一个域为指针mapping ，它指向一个struct address_space类型结构。page cache或swap cache中
的所有页面就是根据address_space结构以及一个偏移量来区分的。

2、文件与address_space结构的对应：一个具体的文件在打开后，内核会在内存中为之建立一个struct inode结构，其中的i_mapping域指向一个address_space结构。这样，一个文件就对应一个address_space结构，一个address_space与一个偏移量能够确定一个page cache 或swap cache中的一个页面
。因此，当要寻址某个数据时，很容易根据给定的文件及数据在文件内的偏移量而找到相应的页面。

3、进程调用mmap()时，只是在进程空间内新增了一块相应大小的缓冲区，并设置了相应的访问标识，但并没有建立进程空间到物理页面的映射。因此，第一次访问该空间时，会引发一个缺页异常
。

4、对于共享内存映射情况，缺页异常处理程序首先在swap cache中寻找目标页（符合address_space以及偏移量的物理页），如果找到，则直接返回地址；如果没有找到，则判断该页是否在交换区(swap area)，如果在，则执行一个换入操作；如果上述两种情况都不满足，处理程序将分配新的物理页面，并把它插入到page cache中。进程最终将更新进程页表。 
注：对于映射普通文件情况（非共享映射），缺页异常处理程序首先会在page cache中根据address_space以及数据偏移量寻找相应的页面。如果没有找到，则说明文件数据还没有读入内存，处理程序会从磁盘读入相应的页面，并返回相应地址，同时，进程页表也会更新。

5、所有进程在映射同一个共享内存区域时，情况都一样，在建立线性地址与物理地址之间的映射之后，不论进程各自的返回地址如何，实际访问的必然是同一个共享内存区域对应的物理页面。 
注：一个共享内存区域可以看作是特殊文件系统shm中的一个文件，shm的安装点在交换区上。

系统调用mmap()用于共享内存的两种方式：

（1）使用普通文件提供的内存映射：适用于任何进程之间； 此时，需要打开或创建一个文件，然后再调用mmap()；典型调用代码如下：

    fd=open(name, flag, mode);
if(fd<0)
    ...
ptr=mmap(NULL, len , PROT_READ|PROT_WRITE, MAP_SHARED , fd , 0); 通过mmap()实现共享内存的通信方式有许多特点和要注意的地方，我们将在范例中进行具体说明。

（2）使用特殊文件提供匿名内存映射：适用于具有亲缘关系的进程之间； 由于父子进程特殊的亲缘关系，在父进程中先调用mmap()，然后调用fork()。
那么在调用fork()之后，子进程继承父进程匿名映射后的地址空间，同样也继承mmap()返回的地址，这样，父子进程就可以通过映射区域进行通信了。注意，这里不是一般的继承关系。一般来说，子进程单独维护从父进程继承下来的一些变量。而mmap()返回的地址，却由父子进程共同维护。 
对于具有亲缘关系的进程实现共享内存最好的方式应该是采用匿名内存映射的方式。此时，不必指定具体的文件，只要设置相应的标志即可


系统调用munmap()

int munmap( void * addr, size_t len ) 
该调用在进程地址空间中解除一个映射关系，addr是调用mmap()时返回的地址，len是映射区的大小。当映射关系解除后，对原来映射地址的访问将导致段错误发生。

系统调用msync()

int msync ( void * addr , size_t len, int flags) 
一般说来，进程在映射空间的对共享内容的改变并不直接写回到磁盘文件中，往往在调用munmap（）后才执行该操作。可以通过调用msync()实现
磁盘上文件内容与共享内存区的内容一致。



linux采用的是页式管理机制。对于用mmap()映射普通文件来说，进程会在自己的地址空间新增一块空间，空间大小由mmap()的len参数指定，注意，进程并不一定能够对全部新增空间都能进行有效访问。进程能够访问的有效地址大小取决于文件被映射部分的大小。简单的说，能够容纳文件被映射部分大小的最少页面个数决定了进程从mmap()返回的地址开始，能够有效访问的地址空间大小。超过这个空间大小，内核会根据超过的严重程度返回发送不同的信号给进程.


#endif 
typedef struct{
  char name[4];
  int  age;
}people;

int main(int argc, char** argv) // map a normal file as shared mem:
{
  int fd,i;
  people *p_map;
  char temp;
  
  fd=open(argv[1],O_CREAT|O_RDWR|O_TRUNC,00777);
  lseek(fd,sizeof(people)*5-1,SEEK_SET);
  write(fd,"",1);
  
  p_map = (people*) mmap( NULL,sizeof(people)*10,PROT_READ|PROT_WRITE,
        MAP_SHARED,fd,0 );
  close( fd );
  temp = 'a';
  for(i=0; i<10; i++)
  {
    temp += 1;
    memcpy( ( *(p_map+i) ).name, &temp,2 );
    ( *(p_map+i) ).age = 20+i;
  }
  printf(" initialize over \n ");
  sleep(10);
  munmap( p_map, sizeof(people)*10 );
  printf( "umap ok \n" );
  return 0;
}
