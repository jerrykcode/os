#include "exec.h"
#include "fs.h"
#include "string.h"
#include "debug.h"
#include "memory.h"
#include "stdbool.h"
#include "stddef.h"
#include "thread.h"
#include "asm.h"

extern void intr_exit(void);

typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* 32位ELF头 */
struct Elf32_Ehdr {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
};

/* ELF文件中存储的程序头, 即段描述头 */
struct Elf32_Phdr {
    Elf32_Word p_type;  // 见下面的enum segment_type
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

enum segment_type {
    PT_NULL,
    PT_LOAD,
    PT_DYNAMIC,
    PT_INTERP,
    PT_NOTE,
    PT_SHLIB,
    PT_PHDR
};

static bool segment_load(int32_t fd, Elf32_Off prog_off, Elf32_Word prog_sz, Elf32_Addr prog_vaddr) {
    int pages_num = 1; // pages_num记录需要几页虚拟内存才能存下这个程序，至少需要1页，初始化为1
    uint32_t first_page_capacity = PAGE_SIZE - (prog_vaddr & 0x00000fff); // prog_vaddr不一定正好在一页的开始
    if (prog_sz > first_page_capacity) {
        uint32_t prog_sz_left = prog_sz - first_page_capacity;
        pages_num += prog_sz_left / PAGE_SIZE + (prog_sz_left % PAGE_SIZE ? 1 : 0);
    }

    // 枚举用来存储程序的每一页虚拟地址
    void *page = (void *)(prog_vaddr & 0xfffff000);
    for (uint32_t i = 0; i < pages_num; i++) {
        // 判断页表中是否存在虚拟地址page
        uint32_t *pde = pde_ptr(page);
        uint32_t *pte = pte_ptr(page);
        if (!(*pde & 0x00000001) || !(*pte & 0x00000001)) { // 页表中不存在虚拟地址page
            // 分配一页物理地址并在页表中添加虚拟地址page到物理地址的映射, 若失败则直接return false
            if (malloc_page_with_vaddr(PF_USER, page) == NULL) {
                return false;
            }
        }
        page += PAGE_SIZE;
    }

    // 到这里，用来存储程序的每一页虚拟内存均指向了一页物理内存，
    // 现在把程序逐页存入这些虚拟内存

    uint32_t bytes_read;
    // 因为第一页可能不是正好完整的一页，单独处理
    sys_lseek(fd, prog_off, SEEK_SET);
    bytes_read = prog_sz < first_page_capacity ? prog_sz : first_page_capacity;
    if (sys_read(fd, prog_vaddr, bytes_read) != bytes_read) {
        return false;
    }

    // 如果只有一页，那么现在就可以返回了
    if (pages_num == 1) {
        return true;
    }
   
    prog_sz -= first_page_capacity;
    prog_off += first_page_capacity;
    prog_vaddr += first_page_capacity;
    ASSERT((prog_vaddr & 0x00000fff) == 0);

    // 逐页处理
    for (uint32_t i = 1; i < pages_num; i++) {
        sys_lseek(fd, prog_off, SEEK_SET);
        bytes_read = prog_sz < PAGE_SIZE ? prog_sz : PAGE_SIZE;
        if (sys_read(fd, prog_vaddr, bytes_read) != bytes_read) {
            return false;
        }
        prog_sz -= bytes_read;
        prog_off += bytes_read;
        prog_vaddr += bytes_read;
    }

    return true;
}

static int32_t load(const char *path) {
    /* 打开目标程序文件 */
    int32_t fd = sys_open(path, O_RONLY);
    if (fd == -1) {
        return -1;
    }

    /* 读取文件ELF头 */
    struct Elf32_Ehdr elf_header;
    memset(&elf_header, 0, sizeof(struct Elf32_Ehdr));

    if (sys_read(fd, &elf_header, sizeof(struct Elf32_Ehdr)) != sizeof(struct Elf32_Ehdr)) {
        sys_close(fd);
        return -1;
    }

    /* 校验ELF头 */
    if (elf_header.e_ident[0] != 0x7f
        || elf_header.e_ident[1] != 'E' 
        || elf_header.e_ident[2] != 'L'
        || elf_header.e_ident[3] != 'F'
        || elf_header.e_ident[4] != 0x1
        || elf_header.e_ident[5] != 0x1
        || elf_header.e_ident[6] != 0x1
       ) {
        sys_close(fd);
        return -1;
    }

    /* 遍历程序头 */
    struct Elf32_Phdr prog_header;
    Elf32_Off prog_header_off = elf_header.e_phoff;
    Elf32_Half prog_header_sz = elf_header.e_phentsize;
    ASSERT(prog_header_sz == sizeof(struct Elf32_Phdr));

    for (int i = 0; i < elf_header.e_phnum; i++) {
        sys_lseek(fd, prog_header_off, SEEK_SET);
        memset(&prog_header, 0, prog_header_sz);

        /* 读取程序头 */
        if (sys_read(fd, &prog_header, prog_header_sz) != prog_header_sz) {
            sys_close(fd);
            return -1;
        }

        /* 如果是可加载段就调用segment_load加载到内存 */
        if (prog_header.p_type == PT_LOAD) {
            if (!segment_load(fd, prog_header.p_offset, prog_header.p_filesz, prog_header.p_vaddr)) {
                sys_close(fd);
                return -1;
            }
        }

        /* 更新prog_header_off为下一个程序头的位置 */
        prog_header_off += prog_header_sz;
    }

    sys_close(fd);
    return elf_header.e_entry;

}

/* 用path指向的程序替换当前进程 */
int32_t sys_execv(const char *path, const char *argv[]) {
    uint32_t argc = 0;
    while (argv[argc])
        argc++;

    int32_t entry_point = load(path);
    if (entry_point == -1) {
        return -1;
    }

    struct task_st *cur = current_thread();
    // 修改进程名
    memcpy(cur->name, path, TASK_NAME_LEN);
    cur->name[TASK_NAME_LEN - 1] = 0;

    // 修改栈顶
    struct intr_stack *stack = (struct intr_stack *)((uint32_t)cur + PAGE_SIZE - sizeof(struct intr_stack));
    stack->ebx = (uint32_t)argv;
    stack->ecx = argc;
    stack->eip = (void *)entry_point;
    stack->esp = (void *)0xc0000000;

    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g"(stack) : "memory");

    return 0; // make gcc happy
}
