/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2003 Peter Wemm
 * Copyright (c) 1999 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _X86_UCONTEXT_H_
#define	_X86_UCONTEXT_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __i386__
/* Keep _MC_* values similar to amd64 */
#define	_MC_HASSEGS	0x1
#define	_MC_HASBASES	0x2
#define	_MC_HASFPXSTATE	0x4
#define	_MC_FLAG_MASK	(_MC_HASSEGS | _MC_HASBASES | _MC_HASFPXSTATE)

typedef struct __mcontext {
	/*
	 * The definition of mcontext_t must match the layout of
	 * struct sigcontext after the sc_mask member.  This is so
	 * that we can support sigcontext and ucontext_t at the same
	 * time.
	 */
	int	mc_onstack;	/* XXX - sigcontext compat. */
	int	mc_gs;		/* machine state (struct trapframe) */
	int	mc_fs;
	int	mc_es;
	int	mc_ds;
	int	mc_edi;
	int	mc_esi;
	int	mc_ebp;
	int	mc_isp;
	int	mc_ebx;
	int	mc_edx;
	int	mc_ecx;
	int	mc_eax;
	int	mc_trapno;
	int	mc_err;
	int	mc_eip;
	int	mc_cs;
	int	mc_eflags;
	int	mc_esp;
	int	mc_ss;

	int	mc_len;			/* sizeof(mcontext_t) */
#define	_MC_FPFMT_NODEV		0x10000	/* device not present or configured */
#define	_MC_FPFMT_387		0x10001
#define	_MC_FPFMT_XMM		0x10002
	int	mc_fpformat;
#define	_MC_FPOWNED_NONE	0x20000	/* FP state not used */
#define	_MC_FPOWNED_FPU		0x20001	/* FP state came from FPU */
#define	_MC_FPOWNED_PCB		0x20002	/* FP state came from PCB */
	int	mc_ownedfp;
	int mc_flags;
	/*
	 * See <machine/npx.h> for the internals of mc_fpstate[].
	 */
	int	mc_fpstate[128] __aligned(16);

	int mc_fsbase;
	int mc_gsbase;

	int mc_xfpustate;
	int mc_xfpustate_len;

	int	mc_spare2[4];
} mcontext_t;
#endif /* __i386__ */

#ifdef __amd64__
/*
 * mc_flags bits. Shall be in sync with TF_XXX.
 */
#define	_MC_HASSEGS	0x1
#define	_MC_HASBASES	0x2
#define	_MC_HASFPXSTATE	0x4
#define	_MC_FLAG_MASK	(_MC_HASSEGS | _MC_HASBASES | _MC_HASFPXSTATE)

typedef struct __mcontext {
	/*
	 * The definition of mcontext_t must match the layout of
	 * struct sigcontext after the sc_mask member.  This is so
	 * that we can support sigcontext and ucontext_t at the same
	 * time.
	 */
	int	mc_onstack;	/* XXX - sigcontext compat. */
	int	mc_rdi;		/* machine state (struct trapframe) */
	int	mc_rsi;
	int	mc_rdx;
	int	mc_rcx;
	int	mc_r8;
	int	mc_r9;
	int	mc_rax;
	int	mc_rbx;
	int	mc_rbp;
	int	mc_r10;
	int	mc_r11;
	int	mc_r12;
	int	mc_r13;
	int	mc_r14;
	int	mc_r15;
	__uint32_t	mc_trapno;
	__uint16_t	mc_fs;
	__uint16_t	mc_gs;
	int	mc_addr;
	__uint32_t	mc_flags;
	__uint16_t	mc_es;
	__uint16_t	mc_ds;
	int	mc_err;
	int	mc_rip;
	int	mc_cs;
	int	mc_rflags;
	int	mc_rsp;
	int	mc_ss;

	long	mc_len;			/* sizeof(mcontext_t) */

#define	_MC_FPFMT_NODEV		0x10000	/* device not present or configured */
#define	_MC_FPFMT_XMM		0x10002
	long	mc_fpformat;
#define	_MC_FPOWNED_NONE	0x20000	/* FP state not used */
#define	_MC_FPOWNED_FPU		0x20001	/* FP state came from FPU */
#define	_MC_FPOWNED_PCB		0x20002	/* FP state came from PCB */
	long	mc_ownedfp;
	/*
	 * See <machine/fpu.h> for the internals of mc_fpstate[].
	 */
	long	mc_fpstate[64] __aligned(16);

	int	mc_fsbase;
	int	mc_gsbase;

	int	mc_xfpustate;
	int	mc_xfpustate_len;

	long	mc_spare[4];
} mcontext_t;
#endif /* __amd64__ */

#endif /* !_X86_UCONTEXT_H_ */