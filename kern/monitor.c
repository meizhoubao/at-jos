// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display information about the $esp & $ebp", mon_backtrace},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

/******** mon_backtrace Implementation *******/
static __inline void read_six_words(volatile uint32_t *p) {
	volatile uint32_t memoryTemp[6] = {0};
	
	int i = 0;
	memoryTemp[0] = (volatile uint32_t)p;
	p++;
	for (i = 1; i < 6; i++, p++) {
		memoryTemp[i] = *p;
	}
	
	cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x\n", 
		memoryTemp[0], memoryTemp[1], memoryTemp[2], memoryTemp[3],
		memoryTemp[4], memoryTemp[5]
	);
}

static __inline void print_function_info(volatile uint32_t* addr) {
	struct Eipdebuginfo eipInfo, *info = &eipInfo;

	debuginfo_eip((uintptr_t)addr, info);
	cprintf("\t%s:%s: ", info->eip_file, info->eip_line);
	cprintf("%.*s+%d\n", info->eip_fn_namelen, info->eip_fn_name, info->eip_fn_narg);
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.

	volatile uint32_t *p = (uint32_t*)read_ebp();

	cprintf("Stack backtrace:\n");
	while (p != 0x0) {
		read_six_words(p);
		// cprintf("\t%s", eip_file,)
		print_function_info((volatile uint32_t*)*(p + 1));
		p = (uint32_t*)*p;
	}
	return 0;

	/*
	uint32_t ebp,eip;
	struct Eipdebuginfo info;
	int i=0;
	ebp=read_ebp();
	while(ebp!=0) {
		eip=*((uint32_t *)(ebp+4));
		
		// change ip to addr
		// debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info)
		debuginfo_eip((uintptr_t)eip,&info);
		cprintf("ebp %0x eip %0x ",ebp,eip);
		cprintf("args ");
		for(i=0;i<=4;i++)
			cprintf("%08x ",*(uint32_t *)(ebp+8+4*i));
		cprintf("\n");
		cprintf("\t%s\t%s ",info.eip_file,info.eip_fn_name);
		cprintf("\n");
		
		ebp=*((uint32_t *)ebp);
	}
	return 0;
	*/
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
