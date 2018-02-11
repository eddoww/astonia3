	.file	"rdtsc.c"
	.version	"01.01"
gcc2_compiled.:
.text
	.align 4
.globl rdtsc
	.type	 rdtsc,@function
rdtsc:
	rdtsc
	ret
.Lfe1:
	.size	 rdtsc,.Lfe1-rdtsc
	.ident	"GCC: (GNU) egcs-2.91.66 19990314/Linux (egcs-1.1.2 release)"
