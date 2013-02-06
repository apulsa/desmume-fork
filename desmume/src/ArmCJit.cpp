/*
	Copyright (C) 2008-2010 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "ArmCJit.h"
#include "ArmAnalyze.h"
#include "instructions.h"
#include "Disassembler.h"

#include "armcpu.h"
#include "MMU.h"
#include "MMU_timing.h"
#include "JitBase.h"
#include "utils/tinycc/libtcc.h"

#ifdef HAVE_JIT

#define GETCPUPTR (&ARMPROC)
#define GETCPU (ARMPROC)

#define TEMPLATE template<int PROCNUM> 
#define OPCDECODER_DECL(name) void FASTCALL name##_CDecoder(const Decoded &d, char *&szCodeBuffer)
#define WRITE_CODE(...) szCodeBuffer += sprintf(szCodeBuffer, __VA_ARGS__)

typedef void (FASTCALL* IROpCDecoder)(const Decoded &d, char *&szCodeBuffer);

typedef u32 (FASTCALL* MemOp1)(u32, u32*);
typedef u32 (FASTCALL* MemOp2)(u32, u32);
typedef u32 (FASTCALL* MemOp3)(u32, u32*, u32);

// (*(u32*)0x11)
// ((u32 (FASTCALL *)(u32,u32))0x11)(1,1);

namespace ArmCJit
{
	enum {
		MEMTYPE_GENERIC = 0, // no assumptions
		MEMTYPE_MAIN = 1,
		MEMTYPE_DTCM = 2,
		MEMTYPE_ERAM = 3,
		MEMTYPE_SWIRAM = 4,

		MEMTYPE_COUNT,
	};

	static u32 classify_adr(u32 PROCNUM, u32 adr, bool store)
	{
		if(PROCNUM==ARMCPU_ARM9 && (adr & ~0x3FFF) == MMU.DTCMRegion)
			return MEMTYPE_DTCM;
		else if((adr & 0x0F000000) == 0x02000000)
			return MEMTYPE_MAIN;
		else if(PROCNUM==ARMCPU_ARM7 && !store && (adr & 0xFF800000) == 0x03800000)
			return MEMTYPE_ERAM;
		else if(PROCNUM==ARMCPU_ARM7 && !store && (adr & 0xFF800000) == 0x03000000)
			return MEMTYPE_SWIRAM;
		else
			return MEMTYPE_GENERIC;
	}

	void FASTCALL IRShiftOpGenerate(const Decoded &d, char *&szCodeBuffer, bool clacCarry)
	{
		u32 PROCNUM = d.ProcessID;

		switch (d.Typ)
		{
		case IRSHIFT_LSL:
			if (!d.R)
			{
				if (clacCarry)
				{
					if (d.Immediate == 0)
						WRITE_CODE("u32 c = ((Status_Reg*)%d)->bits.C;\n", &(GETCPU.CPSR));
					else
						WRITE_CODE("u32 c = BIT_N((*(u32*)%d), %d);\n", &(GETCPU.R[d.Rm]), 32-d.Immediate);
				}

				if (d.Immediate == 0)
					WRITE_CODE("u32 shift_op = (*(u32*)%d);\n", &(GETCPU.R[d.Rm]));
				else
					WRITE_CODE("u32 shift_op = (*(u32*)%d)<<%d;\n", &(GETCPU.R[d.Rm]), d.Immediate);
			}
			else
			{
				if (clacCarry)
				{
					WRITE_CODE("u32 c;\n");
					WRITE_CODE("u32 shift_op = (*(u32*)%d)&0xFF;\n", &(GETCPU.R[d.Rs]));
					WRITE_CODE("if (shift_op == 0){\n");
					WRITE_CODE("c=((Status_Reg*)%d)->bits.C;\n", &(GETCPU.CPSR));
					WRITE_CODE("shift_op=(*(u32*)%d);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("}else if (shift_op < 32){\n");
					WRITE_CODE("c = BIT_N((*(u32*)%d), 32-shift_op);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("shift_op = (*(u32*)%d)<<shift_op;\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("}else if (shift_op == 32){\n");
					WRITE_CODE("c = BIT0((*(u32*)%d));\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("shift_op=0;\n");
					WRITE_CODE("}else{\n");
					WRITE_CODE("shift_op=c=0;}\n");
				}
				else
				{
					WRITE_CODE("u32 shift_op = (*(u32*)%d)&0xFF;\n", &(GETCPU.R[d.Rs]));
					WRITE_CODE("if (shift_op >= 32)\n");
					WRITE_CODE("shift_op=0;\n");
					WRITE_CODE("else\n");
					WRITE_CODE("shift_op=(*(u32*)%d)<<shift_op;\n", &(GETCPU.R[d.Rm]));
				}
			}
			break;
		case IRSHIFT_LSR:
			if (!d.R)
			{
				if (clacCarry)
				{
					if (d.Immediate == 0)
						WRITE_CODE("u32 c = BIT31((*(u32*)%d));\n", &(GETCPU.R[d.Rm]));
					else
						WRITE_CODE("u32 c = BIT_N((*(u32*)%d), %d);\n", &(GETCPU.R[d.Rm]), d.Immediate-1);
				}

				if (d.Immediate == 0)
					WRITE_CODE("u32 shift_op = 0;\n");
				else
					WRITE_CODE("u32 shift_op = (*(u32*)%d)>>%d;\n", &(GETCPU.R[d.Rm]), d.Immediate);
			}
			else
			{
				if (clacCarry)
				{
					WRITE_CODE("u32 c;\n");
					WRITE_CODE("u32 shift_op = (*(u32*)%d)&0xFF;\n", &(GETCPU.R[d.Rs]));
					WRITE_CODE("if (shift_op == 0){\n");
					WRITE_CODE("c=((Status_Reg*)%d)->bits.C;\n", &(GETCPU.CPSR));
					WRITE_CODE("shift_op=(*(u32*)%d);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("}else if (shift_op < 32){\n");
					WRITE_CODE("c = BIT_N((*(u32*)%d), shift_op-1);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("shift_op = (*(u32*)%d)>>shift_op;\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("}else if (shift_op == 32){\n");
					WRITE_CODE("c = BIT31((*(u32*)%d));\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("shift_op=0;\n");
					WRITE_CODE("}else{\n");
					WRITE_CODE("shift_op=c=0;}\n");
				}
				else
				{
					WRITE_CODE("u32 shift_op = (*(u32*)%d)&0xFF;\n", &(GETCPU.R[d.Rs]));
					WRITE_CODE("if (shift_op >= 32)\n");
					WRITE_CODE("shift_op=0;\n");
					WRITE_CODE("else\n");
					WRITE_CODE("shift_op=(*(u32*)%d)>>shift_op;\n", &(GETCPU.R[d.Rm]));
				}
			}
			break;
		case IRSHIFT_ASR:
			if (!d.R)
			{
				if (clacCarry)
				{
					if (d.Immediate == 0)
						WRITE_CODE("u32 c = BIT31((*(u32*)%d));\n", &(GETCPU.R[d.Rm]));
					else
						WRITE_CODE("u32 c = BIT_N((*(u32*)%d), %d);\n", &(GETCPU.R[d.Rm]), d.Immediate-1);
				}

				if (d.Immediate == 0)
					WRITE_CODE("u32 shift_op = BIT31((*(u32*)%d))*0xFFFFFFFF;\n", &(GETCPU.R[d.Rm]));
				else
					WRITE_CODE("u32 shift_op = (u32)((*(s32*)%d)>>%u);\n", &(GETCPU.R[d.Rm]), d.Immediate);
			}
			else
			{
				if (clacCarry)
				{
					WRITE_CODE("u32 c;\n");
					WRITE_CODE("u32 shift_op = (*(u32*)%d)&0xFF;\n", &(GETCPU.R[d.Rs]));
					WRITE_CODE("if (shift_op == 0){\n");
					WRITE_CODE("c=((Status_Reg*)%d)->bits.C;\n", &(GETCPU.CPSR));
					WRITE_CODE("shift_op = (*(u32*)%d);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("}else if (shift_op < 32){\n");
					WRITE_CODE("c = BIT_N((*(u32*)%d), shift_op-1);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("shift_op = (u32)((*(s32*)%d)>>shift_op);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("}else{\n");
					WRITE_CODE("c = BIT31((*(u32*)%d));\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("shift_op = BIT31((*(u32*)%d))*0xFFFFFFFF;}\n", &(GETCPU.R[d.Rm]));
				}
				else
				{
					WRITE_CODE("u32 shift_op = (*(u32*)%d)&0xFF;\n", &(GETCPU.R[d.Rs]));
					WRITE_CODE("if (shift_op == 0)\n");
					WRITE_CODE("shift_op = (*(u32*)%d);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("else if (shift_op < 32)\n");
					WRITE_CODE("shift_op = (u32)((*(s32*)%d)>>shift_op);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("else\n");
					WRITE_CODE("shift_op = BIT31((*(u32*)%d))*0xFFFFFFFF;\n", &(GETCPU.R[d.Rm]));
				}
			}
			break;
		case IRSHIFT_ROR:
			if (!d.R)
			{
				if (clacCarry)
				{
					if (d.Immediate == 0)
						WRITE_CODE("u32 c = BIT0((*(u32*)%d));\n", &(GETCPU.R[d.Rm]));
					else
						WRITE_CODE("u32 c = BIT_N((*(u32*)%d), %d);\n", &(GETCPU.R[d.Rm]), d.Immediate-1);
				}

				if (d.Immediate == 0)
					WRITE_CODE("u32 shift_op = (((u32)((Status_Reg*)%d)->bits.C)<<31)|((*(u32*)%d)>>1);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rm]));
				else
					WRITE_CODE("u32 shift_op = ROR((*(u32*)%d), %d);\n", &(GETCPU.R[d.Rm]), d.Immediate);
			}
			else
			{
				if (clacCarry)
				{
					WRITE_CODE("u32 c;\n");
					WRITE_CODE("u32 shift_op = (*(u32*)%d)&0xFF;\n", &(GETCPU.R[d.Rs]));
					WRITE_CODE("if (shift_op == 0){\n");
					WRITE_CODE("c=((Status_Reg*)%d)->bits.C;\n", &(GETCPU.CPSR));
					WRITE_CODE("shift_op = (*(u32*)%d);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("}else{\n");
					WRITE_CODE("shift_op &= 0x1F;\n");
					WRITE_CODE("if (shift_op != 0){\n");
					WRITE_CODE("c = BIT_N((*(u32*)%d), shift_op-1);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("shift_op = ROR((*(u32*)%d), shift_op);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("}else{\n");
					WRITE_CODE("c = BIT31((*(u32*)%d));\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("shift_op = (*(u32*)%d);}}\n", &(GETCPU.R[d.Rm]));
				
				}
				else
				{
					WRITE_CODE("u32 shift_op = (*(u32*)%d)&0x1F;\n", &(GETCPU.R[d.Rs]));
					WRITE_CODE("if (shift_op == 0)\n");
					WRITE_CODE("shift_op = (*(u32*)%d);\n", &(GETCPU.R[d.Rm]));
					WRITE_CODE("else\n");
					WRITE_CODE("shift_op = ROR((*(u32*)%d), shift_op);\n", &(GETCPU.R[d.Rm]));
				}
			}
			break;
		default:
			INFO("Unknow Shift Op : %d.\n", d.Typ);
			if (clacCarry)
				WRITE_CODE("u32 c = 0;\n");
			WRITE_CODE("u32 shift_op = 0;\n");
			break;
		}
	}

	void FASTCALL DataProcessLoadCPSRGenerate(const Decoded &d, char *&szCodeBuffer)
	{
	}

	void FASTCALL R15ModifiedGenerate(const Decoded &d, char *&szCodeBuffer)
	{
	}

	OPCDECODER_DECL(IR_UND)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("TRAPUNDEF((armcpu_t*)%d);\n", GETCPUPTR);
	}

	OPCDECODER_DECL(IR_NOP)
	{
	}

	OPCDECODER_DECL(IR_MOV)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			WRITE_CODE("(*(u32*)%d)=%d;\n", &(GETCPU.R[d.Rd]), d.Immediate);
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=%d;\n", &(GETCPU.CPSR), BIT31(d.Immediate));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=%d;\n", &(GETCPU.CPSR), BIT31(d.Immediate));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=%d;\n", &(GETCPU.CPSR), d.Immediate==0 ? 1 : 0);
			}
		}
		else
		{
			const bool clacCarry = d.S && !d.R15Modified && (d.FlagsSet & FLAG_C);
			IRShiftOpGenerate(d, szCodeBuffer, clacCarry);

			WRITE_CODE("(*(u32*)%d)=shift_op;\n", &(GETCPU.R[d.Rd]));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=c;\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_MVN)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			WRITE_CODE("(*(u32*)%d)=%d;\n", &(GETCPU.R[d.Rd]), ~d.Immediate);
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=%d;\n", &(GETCPU.CPSR), BIT31(d.Immediate));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=%d;\n", &(GETCPU.CPSR), BIT31(~d.Immediate));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=%d;\n", &(GETCPU.CPSR), (~d.Immediate)==0 ? 1 : 0);
			}
		}
		else
		{
			const bool clacCarry = d.S && !d.R15Modified && (d.FlagsSet & FLAG_C);
			IRShiftOpGenerate(d, szCodeBuffer, clacCarry);

			WRITE_CODE("shift_op=(*(u32*)%d)=~shift_op;\n", &(GETCPU.R[d.Rd]));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=c;\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_AND)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			WRITE_CODE("u32 shift_op=(*(u32*)%d)=(*(u32*)%d)&%d;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]), d.Immediate);
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=%d;\n", &(GETCPU.CPSR), BIT31(d.Immediate));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}
		else
		{
			const bool clacCarry = d.S && !d.R15Modified && (d.FlagsSet & FLAG_C);
			IRShiftOpGenerate(d, szCodeBuffer, clacCarry);

			WRITE_CODE("shift_op=(*(u32*)%d)=(*(u32*)%d)&shift_op;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=c;\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_TST)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			WRITE_CODE("u32 shift_op=(*(u32*)%d)&%d;\n", &(GETCPU.R[d.Rn]), d.Immediate);

			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=%d;\n", &(GETCPU.CPSR), BIT31(d.Immediate));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}
		else
		{
			const bool clacCarry = d.S && !d.R15Modified && (d.FlagsSet & FLAG_C);
			IRShiftOpGenerate(d, szCodeBuffer, clacCarry);

			WRITE_CODE("shift_op=(*(u32*)%d)&shift_op;\n", &(GETCPU.R[d.Rn]));
			
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=c;\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}
	}

	OPCDECODER_DECL(IR_EOR)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			WRITE_CODE("u32 shift_op=(*(u32*)%d)=(*(u32*)%d)^%d;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]), d.Immediate);
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=%d;\n", &(GETCPU.CPSR), BIT31(d.Immediate));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}
		else
		{
			const bool clacCarry = d.S && !d.R15Modified && (d.FlagsSet & FLAG_C);
			IRShiftOpGenerate(d, szCodeBuffer, clacCarry);

			WRITE_CODE("shift_op=(*(u32*)%d)=(*(u32*)%d)^shift_op;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=c;\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_TEQ)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			WRITE_CODE("u32 shift_op=(*(u32*)%d)^%d;\n", &(GETCPU.R[d.Rn]), d.Immediate);

			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=%d;\n", &(GETCPU.CPSR), BIT31(d.Immediate));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}
		else
		{
			const bool clacCarry = d.S && !d.R15Modified && (d.FlagsSet & FLAG_C);
			IRShiftOpGenerate(d, szCodeBuffer, clacCarry);

			WRITE_CODE("shift_op=(*(u32*)%d)^shift_op;\n", &(GETCPU.R[d.Rn]));
			
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=c;\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}
	}

	OPCDECODER_DECL(IR_ORR)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			WRITE_CODE("u32 shift_op=(*(u32*)%d)=(*(u32*)%d)|%d;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]), d.Immediate);
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=%d;\n", &(GETCPU.CPSR), BIT31(d.Immediate));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}
		else
		{
			const bool clacCarry = d.S && !d.R15Modified && (d.FlagsSet & FLAG_C);
			IRShiftOpGenerate(d, szCodeBuffer, clacCarry);

			WRITE_CODE("shift_op=(*(u32*)%d)=(*(u32*)%d)|shift_op;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=c;\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_BIC)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			WRITE_CODE("u32 shift_op=(*(u32*)%d)=(*(u32*)%d)&%d;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]), ~d.Immediate);
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=%d;\n", &(GETCPU.CPSR), BIT31(d.Immediate));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}
		else
		{
			const bool clacCarry = d.S && !d.R15Modified && (d.FlagsSet & FLAG_C);
			IRShiftOpGenerate(d, szCodeBuffer, clacCarry);

			WRITE_CODE("shift_op=(*(u32*)%d)=(*(u32*)%d)&(~shift_op);\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=c;\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(shift_op==0);\n", &(GETCPU.CPSR));
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_ADD)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("(*(u32*)%d)=(*(u32*)%d)+%d;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]), d.Immediate);
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=CarryFrom(v, %d);\n", &(GETCPU.CPSR), d.Immediate);
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=OverflowFromADD((*(u32*)%d), v, %d);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]), d.Immediate);
			}
		}
		else
		{
			IRShiftOpGenerate(d, szCodeBuffer, false);

			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("shift_op=(*(u32*)%d)=(*(u32*)%d)+shift_op;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=CarryFrom(v, shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=OverflowFromADD((*(u32*)%d), v, shift_op);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_ADC)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("(*(u32*)%d)=(*(u32*)%d)+%d+((Status_Reg*)%d)->bits.C;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]), d.Immediate, &(GETCPU.CPSR));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=BIT31((v^%d^-1) & (v^(*(u32*)%d)));\n", &(GETCPU.CPSR), d.Immediate, &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
				{
					WRITE_CODE("if(((Status_Reg*)%d)->bits.C)\n", &(GETCPU.CPSR));
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)<=v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
					WRITE_CODE("else\n");
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)<v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				}
			}
		}
		else
		{
			IRShiftOpGenerate(d, szCodeBuffer, false);

			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("shift_op=(*(u32*)%d)=(*(u32*)%d)+shift_op+((Status_Reg*)%d)->bits.C;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]), &(GETCPU.CPSR));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=BIT31((v^shift_op^-1) & (v^(*(u32*)%d)));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
				{
					WRITE_CODE("if(((Status_Reg*)%d)->bits.C)\n", &(GETCPU.CPSR));
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)<=v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
					WRITE_CODE("else\n");
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)<v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				}
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_SUB)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("(*(u32*)%d)=(*(u32*)%d)-%d;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]), d.Immediate);
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=!BorrowFrom(v, %d);\n", &(GETCPU.CPSR), d.Immediate);
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=OverflowFromSUB((*(u32*)%d), v, %d);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]), d.Immediate);
			}
		}
		else
		{
			IRShiftOpGenerate(d, szCodeBuffer, false);

			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("shift_op=(*(u32*)%d)=(*(u32*)%d)-shift_op;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=!BorrowFrom(v, shift_op);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=OverflowFromSUB((*(u32*)%d), v, shift_op);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_SBC)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("(*(u32*)%d)=(*(u32*)%d)-%d-!((Status_Reg*)%d)->bits.C;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]), d.Immediate, &(GETCPU.CPSR));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=BIT31((v^%d) & (v^(*(u32*)%d)));\n", &(GETCPU.CPSR), d.Immediate, &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
				{
					WRITE_CODE("if(((Status_Reg*)%d)->bits.C)\n", &(GETCPU.CPSR));
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)>=v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
					WRITE_CODE("else\n");
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)>v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				}
			}
		}
		else
		{
			IRShiftOpGenerate(d, szCodeBuffer, false);

			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("shift_op=(*(u32*)%d)=(*(u32*)%d)-shift_op-!((Status_Reg*)%d)->bits.C;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]), &(GETCPU.CPSR));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=BIT31((v^shift_op) & (v^(*(u32*)%d)));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
				{
					WRITE_CODE("if(((Status_Reg*)%d)->bits.C)\n", &(GETCPU.CPSR));
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)>=v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
					WRITE_CODE("else\n");
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)>v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				}
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_RSB)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("(*(u32*)%d)=%d-(*(u32*)%d);\n", &(GETCPU.R[d.Rd]), d.Immediate, &(GETCPU.R[d.Rn]));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=!BorrowFrom(%d, v);\n", &(GETCPU.CPSR), d.Immediate);
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=OverflowFromSUB((*(u32*)%d), %d, v);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]), d.Immediate);
			}
		}
		else
		{
			IRShiftOpGenerate(d, szCodeBuffer, false);

			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("shift_op=(*(u32*)%d)=shift_op-(*(u32*)%d);\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=!BorrowFrom(shift_op, v);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=OverflowFromSUB((*(u32*)%d), shift_op, v);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_RSC)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("(*(u32*)%d)=%d-(*(u32*)%d)-!((Status_Reg*)%d)->bits.C;\n", &(GETCPU.R[d.Rd]), d.Immediate, &(GETCPU.R[d.Rn]), &(GETCPU.CPSR));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=BIT31((%d^v) & ((*(u32*)%d)^v));\n", &(GETCPU.CPSR), d.Immediate, &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
				{
					WRITE_CODE("if(((Status_Reg*)%d)->bits.C)\n", &(GETCPU.CPSR));
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)>=v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
					WRITE_CODE("else\n");
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)>v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				}
			}
		}
		else
		{
			IRShiftOpGenerate(d, szCodeBuffer, false);

			if (d.S && !d.R15Modified && ((d.FlagsSet & FLAG_C) || (d.FlagsSet & FLAG_V)))
				WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("shift_op=(*(u32*)%d)=shift_op-(*(u32*)%d)-!((Status_Reg*)%d)->bits.C;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]), &(GETCPU.CPSR));
			if (d.S && !d.R15Modified)
			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=BIT31((v^shift_op) & (v^(*(u32*)%d)));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				if (d.FlagsSet & FLAG_C)
				{
					WRITE_CODE("if(((Status_Reg*)%d)->bits.C)\n", &(GETCPU.CPSR));
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)>=v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
					WRITE_CODE("else\n");
					WRITE_CODE("((Status_Reg*)%d)->bits.C=(*(u32*)%d)>v;\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
				}
			}
		}

		if (d.R15Modified)
		{
			if (d.S)
			{
				DataProcessLoadCPSRGenerate(d, szCodeBuffer);
			}

			R15ModifiedGenerate(d, szCodeBuffer);
		}
	}

	OPCDECODER_DECL(IR_CMP)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			WRITE_CODE("u32 tmp=(*(u32*)%d)-%d;\n", &(GETCPU.R[d.Rn]), d.Immediate);

			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(tmp);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(tmp==0);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=!BorrowFrom((*(u32*)%d), %d);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rn]), d.Immediate);
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=OverflowFromSUB(tmp, (*(u32*)%d), %d);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rn]), d.Immediate);
			}
		}
		else
		{
			IRShiftOpGenerate(d, szCodeBuffer, false);

			WRITE_CODE("u32 tmp=(*(u32*)%d)-shift_op;\n", &(GETCPU.R[d.Rn]));

			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(tmp);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(tmp==0);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=!BorrowFrom((*(u32*)%d), shift_op);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rn]));
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=OverflowFromSUB(tmp, (*(u32*)%d), shift_op);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rn]));
			}
		}
	}

	OPCDECODER_DECL(IR_CMN)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.I)
		{
			WRITE_CODE("u32 tmp=(*(u32*)%d)+%d;\n", &(GETCPU.R[d.Rn]), d.Immediate);

			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(tmp);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(tmp==0);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=CarryFrom((*(u32*)%d), %d);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rn]), d.Immediate);
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=OverflowFromADD(tmp, (*(u32*)%d), %d);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rn]), d.Immediate);
			}
		}
		else
		{
			IRShiftOpGenerate(d, szCodeBuffer, false);

			WRITE_CODE("u32 tmp=(*(u32*)%d)+shift_op;\n", &(GETCPU.R[d.Rn]));

			{
				if (d.FlagsSet & FLAG_N)
					WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31(tmp);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_Z)
					WRITE_CODE("((Status_Reg*)%d)->bits.Z=(tmp==0);\n", &(GETCPU.CPSR));
				if (d.FlagsSet & FLAG_C)
					WRITE_CODE("((Status_Reg*)%d)->bits.C=CarryFrom((*(u32*)%d), shift_op);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rn]));
				if (d.FlagsSet & FLAG_V)
					WRITE_CODE("((Status_Reg*)%d)->bits.V=OverflowFromADD(tmp, (*(u32*)%d), shift_op);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rn]));
			}
		}
	}

	OPCDECODER_DECL(IR_MUL)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rs]));
		WRITE_CODE("(*(u32*)%d)=(*(u32*)%d)*v;\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rm]));
		if (d.S)
		{
			if (d.FlagsSet & FLAG_N)
				WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
			if (d.FlagsSet & FLAG_Z)
				WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
		}

		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if((v==0)||(v==0xFFFFFF)){\n");
		WRITE_CODE("ExecuteCycles+=1+1;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if((v==0)||(v==0xFFFF)){\n");
		WRITE_CODE("ExecuteCycles+=1+2;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if((v==0)||(v==0xFF)){\n");
		WRITE_CODE("ExecuteCycles+=1+3;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("ExecuteCycles+=1+4;\n");
		WRITE_CODE("}}}\n");
	}

	OPCDECODER_DECL(IR_MLA)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rs]));
		WRITE_CODE("(*(u32*)%d)=(*(u32*)%d)*v+(*(u32*)%d);\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rm]), &(GETCPU.R[d.Rn]));
		if (d.S)
		{
			if (d.FlagsSet & FLAG_N)
				WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
			if (d.FlagsSet & FLAG_Z)
				WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
		}

		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if((v==0)||(v==0xFFFFFF)){\n");
		WRITE_CODE("ExecuteCycles+=2+1;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if((v==0)||(v==0xFFFF)){\n");
		WRITE_CODE("ExecuteCycles+=2+2;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if((v==0)||(v==0xFF)){\n");
		WRITE_CODE("ExecuteCycles+=2+3;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("ExecuteCycles+=2+4;\n");
		WRITE_CODE("}}}\n");
	}

	OPCDECODER_DECL(IR_UMULL)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rs]));
		WRITE_CODE("u64 res=(*(u32*)%d)*v;\n", &(GETCPU.R[d.Rm]));
		WRITE_CODE("(*(u32*)%d)=(u32)res;\n", &(GETCPU.R[d.Rn]));
		WRITE_CODE("(*(u32*)%d)=(u32)(res>>32);\n", &(GETCPU.R[d.Rd]));
		if (d.S)
		{
			if (d.FlagsSet & FLAG_N)
				WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
			if (d.FlagsSet & FLAG_Z)
				WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0)&&((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
		}

		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if(v==0){\n");
		WRITE_CODE("ExecuteCycles+=2+1;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if(v==0){\n");
		WRITE_CODE("ExecuteCycles+=2+2;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if(v==0){\n");
		WRITE_CODE("ExecuteCycles+=2+3;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("ExecuteCycles+=2+4;\n");
		WRITE_CODE("}}}\n");
	}

	OPCDECODER_DECL(IR_UMLAL)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("u32 v=(*(u32*)%d);\n", &(GETCPU.R[d.Rs]));
		WRITE_CODE("u64 res=(*(u32*)%d)*v;\n", &(GETCPU.R[d.Rm]));
		WRITE_CODE("u32 tmp=(u32)res;\n");
		WRITE_CODE("(*(u32*)%d)=(u32)(res>>32)+(*(u32*)%d)+CarryFrom(tmp,(*(u32*)%d));\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
		WRITE_CODE("(*(u32*)%d)+=tmp;\n", &(GETCPU.R[d.Rn]));
		if (d.S)
		{
			if (d.FlagsSet & FLAG_N)
				WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
			if (d.FlagsSet & FLAG_Z)
				WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0)&&((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
		}

		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if(v==0){\n");
		WRITE_CODE("ExecuteCycles+=3+1;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if(v==0){\n");
		WRITE_CODE("ExecuteCycles+=3+2;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v >>= 8;\n");
		WRITE_CODE("if(v==0){\n");
		WRITE_CODE("ExecuteCycles+=3+3;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("ExecuteCycles+=3+4;\n");
		WRITE_CODE("}}}\n");
	}

	OPCDECODER_DECL(IR_SMULL)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("s64 v=(*(s32*)%d);\n", &(GETCPU.R[d.Rs]));
		WRITE_CODE("s64 res=(s64)(*(s32*)%d)*v;\n", &(GETCPU.R[d.Rm]));
		WRITE_CODE("(*(u32*)%d)=(u32)res;\n", &(GETCPU.R[d.Rn]));
		WRITE_CODE("(*(u32*)%d)=(u32)(res>>32);\n", &(GETCPU.R[d.Rd]));
		if (d.S)
		{
			if (d.FlagsSet & FLAG_N)
				WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
			if (d.FlagsSet & FLAG_Z)
				WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0)&&((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
		}

		WRITE_CODE("u32 v2 = v&0xFFFFFFFF;\n");
		WRITE_CODE("v2 >>= 8;\n");
		WRITE_CODE("if((v2==0)||(v2==0xFFFFFF)){\n");
		WRITE_CODE("ExecuteCycles+=2+1;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v2 >>= 8;\n");
		WRITE_CODE("if((v2==0)||(v2==0xFFFF)){\n");
		WRITE_CODE("ExecuteCycles+=2+2;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v2 >>= 8;\n");
		WRITE_CODE("if((v2==0)||(v2==0xFF)){\n");
		WRITE_CODE("ExecuteCycles+=2+3;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("ExecuteCycles+=2+4;\n");
		WRITE_CODE("}}}\n");
	}

	OPCDECODER_DECL(IR_SMLAL)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("s64 v=(*(s32*)%d);\n", &(GETCPU.R[d.Rs]));
		WRITE_CODE("s64 res=(s64)(*(s32*)%d)*v;\n", &(GETCPU.R[d.Rm]));
		WRITE_CODE("u32 tmp=(u32)res;\n");
		WRITE_CODE("(*(u32*)%d)=(u32)(res>>32)+(*(u32*)%d)+CarryFrom(tmp,(*(u32*)%d));\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
		WRITE_CODE("(*(u32*)%d)+=tmp;\n", &(GETCPU.R[d.Rn]));
		if (d.S)
		{
			if (d.FlagsSet & FLAG_N)
				WRITE_CODE("((Status_Reg*)%d)->bits.N=BIT31((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]));
			if (d.FlagsSet & FLAG_Z)
				WRITE_CODE("((Status_Reg*)%d)->bits.Z=((*(u32*)%d)==0)&&((*(u32*)%d)==0);\n", &(GETCPU.CPSR), &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
		}

		WRITE_CODE("u32 v2 = v&0xFFFFFFFF;\n");
		WRITE_CODE("v2 >>= 8;\n");
		WRITE_CODE("if((v2==0)||(v2==0xFFFFFF)){\n");
		WRITE_CODE("ExecuteCycles+=3+1;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v2 >>= 8;\n");
		WRITE_CODE("if((v2==0)||(v2==0xFFFF)){\n");
		WRITE_CODE("ExecuteCycles+=3+2;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("v2 >>= 8;\n");
		WRITE_CODE("if((v2==0)||(v2==0xFF)){\n");
		WRITE_CODE("ExecuteCycles+=3+3;\n");
		WRITE_CODE("}else{\n");
		WRITE_CODE("ExecuteCycles+=3+4;\n");
		WRITE_CODE("}}}\n");
	}

	OPCDECODER_DECL(IR_SMULxy)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("(*(u32*)%d)=(u32)(", &(GETCPU.R[d.Rd]));
		if (d.X)
			WRITE_CODE("HWORD(");
		else
			WRITE_CODE("LWORD(");
		WRITE_CODE("(*(u32*)%d))*", &(GETCPU.R[d.Rm]));
		if (d.Y)
			WRITE_CODE("HWORD(");
		else
			WRITE_CODE("LWORD(");
		WRITE_CODE("(*(u32*)%d)));\n", &(GETCPU.R[d.Rs]));
	}

	OPCDECODER_DECL(IR_SMLAxy)
	{
		u32 PROCNUM = d.ProcessID;

		if (!d.X && !d.Y)
		{
			WRITE_CODE("u32 tmp=(u32)((s16)(*(u32*)%d) * (s16)(*(u32*)%d));\n", &(GETCPU.R[d.Rm]), &(GETCPU.R[d.Rs]));
			WRITE_CODE("(*(u32*)%d) = tmp + (*(u32*)%d);\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
			WRITE_CODE("if (OverflowFromADD((*(u32*)%d), tmp, (*(u32*)%d)))\n", &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rn]));
			WRITE_CODE("((Status_Reg*)%d)->bits.Q=1;\n", &(GETCPU.CPSR));
		}
		else
		{
			WRITE_CODE("u32 tmp=(u32)(");
			if (d.X)
				WRITE_CODE("HWORD(");
			else
				WRITE_CODE("LWORD(");
			WRITE_CODE("(*(u32*)%d))*", &(GETCPU.R[d.Rm]));
			if (d.Y)
				WRITE_CODE("HWORD(");
			else
				WRITE_CODE("LWORD(");
			WRITE_CODE("(*(u32*)%d)));\n", &(GETCPU.R[d.Rs]));
			WRITE_CODE("u32 a = (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			WRITE_CODE("(*(u32*)%d) = tmp + a;\n", &(GETCPU.R[d.Rd]));
			WRITE_CODE("if (SIGNED_OVERFLOW(tmp, a, (*(u32*)%d)))\n", &(GETCPU.R[d.Rd]));
			WRITE_CODE("((Status_Reg*)%d)->bits.Q=1;\n", &(GETCPU.CPSR));
		}
	}

	OPCDECODER_DECL(IR_SMULWy)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("s64 tmp = (s64)");
		if (d.Y)
			WRITE_CODE("HWORD(");
		else
			WRITE_CODE("LWORD(");
		WRITE_CODE("(*(u32*)%d)) * (s64)((s32)(*(u32*)%d));\n", &(GETCPU.R[d.Rs]), &(GETCPU.R[d.Rm]));
		WRITE_CODE("(*(u32*)%d) = ((tmp>>16)&0xFFFFFFFF);\n", &(GETCPU.R[d.Rd]));
	}

	OPCDECODER_DECL(IR_SMLAWy)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("s64 tmp = (s64)");
		if (d.Y)
			WRITE_CODE("HWORD(");
		else
			WRITE_CODE("LWORD(");
		WRITE_CODE("(*(u32*)%d)) * (s64)((s32)(*(u32*)%d));\n", &(GETCPU.R[d.Rs]), &(GETCPU.R[d.Rm]));
		WRITE_CODE("u32 a = (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
		WRITE_CODE("tmp = ((tmp>>16)&0xFFFFFFFF);\n");
		WRITE_CODE("(*(u32*)%d) = tmp + a;\n", &(GETCPU.R[d.Rd]));
		WRITE_CODE("if (SIGNED_OVERFLOW((u32)tmp, a, (*(u32*)%d)))\n", &(GETCPU.R[d.Rd]));
		WRITE_CODE("((Status_Reg*)%d)->bits.Q=1;\n", &(GETCPU.CPSR));
	}

	OPCDECODER_DECL(IR_SMLALxy)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("s64 tmp=(s64)(");
		if (d.X)
			WRITE_CODE("HWORD(");
		else
			WRITE_CODE("LWORD(");
		WRITE_CODE("(*(u32*)%d))*", &(GETCPU.R[d.Rm]));
		if (d.Y)
			WRITE_CODE("HWORD(");
		else
			WRITE_CODE("LWORD(");
		WRITE_CODE("(*(u32*)%d)));\n", &(GETCPU.R[d.Rs]));
		WRITE_CODE("u64 res = (u64)tmp + (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
		WRITE_CODE("(*(u32*)%d) = (u32)res;\n", &(GETCPU.R[d.Rn]));
		WRITE_CODE("(*(u32*)%d) += (res + ((tmp<0)*0xFFFFFFFF));\n", &(GETCPU.R[d.Rd]));
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_LDR(u32 adr, u32 *dstreg)
	{
		u32 data = READ32(GETCPU.mem_if->data, adr);
		if(adr&3)
			data = ROR(data, 8*(adr&3));
		*dstreg = data;
		return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_READ>(cycle,adr);
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_LDRB(u32 adr, u32 *dstreg)
	{
		*dstreg = READ8(GETCPU.mem_if->data, adr);
		return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_READ>(cycle,adr);
	}

	static const MemOp1 LDR_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_LDR<0,MEMTYPE_GENERIC,3>,
			MEMOP_LDR<0,MEMTYPE_MAIN,3>,
			MEMOP_LDR<0,MEMTYPE_DTCM,3>,
			MEMOP_LDR<0,MEMTYPE_ERAM,3>,
			MEMOP_LDR<0,MEMTYPE_SWIRAM,3>,
		},
		{
			MEMOP_LDR<1,MEMTYPE_GENERIC,3>,
			MEMOP_LDR<1,MEMTYPE_MAIN,3>,
			MEMOP_LDR<1,MEMTYPE_DTCM,3>,
			MEMOP_LDR<1,MEMTYPE_ERAM,3>,
			MEMOP_LDR<1,MEMTYPE_SWIRAM,3>,
		}
	};

	static const MemOp1 LDR_R15_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_LDR<0,MEMTYPE_GENERIC,5>,
			MEMOP_LDR<0,MEMTYPE_MAIN,5>,
			MEMOP_LDR<0,MEMTYPE_DTCM,5>,
			MEMOP_LDR<0,MEMTYPE_ERAM,5>,
			MEMOP_LDR<0,MEMTYPE_SWIRAM,5>,
		},
		{
			MEMOP_LDR<1,MEMTYPE_GENERIC,5>,
			MEMOP_LDR<1,MEMTYPE_MAIN,5>,
			MEMOP_LDR<1,MEMTYPE_DTCM,5>,
			MEMOP_LDR<1,MEMTYPE_ERAM,5>,
			MEMOP_LDR<1,MEMTYPE_SWIRAM,5>,
		}
	};

	static const MemOp1 LDRB_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_LDRB<0,MEMTYPE_GENERIC,3>,
			MEMOP_LDRB<0,MEMTYPE_MAIN,3>,
			MEMOP_LDRB<0,MEMTYPE_DTCM,3>,
			MEMOP_LDRB<0,MEMTYPE_ERAM,3>,
			MEMOP_LDRB<0,MEMTYPE_SWIRAM,3>,
		},
		{
			MEMOP_LDRB<1,MEMTYPE_GENERIC,3>,
			MEMOP_LDRB<1,MEMTYPE_MAIN,3>,
			MEMOP_LDRB<1,MEMTYPE_DTCM,3>,
			MEMOP_LDRB<1,MEMTYPE_ERAM,3>,
			MEMOP_LDRB<1,MEMTYPE_SWIRAM,3>,
		}
	};

	OPCDECODER_DECL(IR_LDR)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.P)
		{
			if (d.I)
				WRITE_CODE("u32 adr = (*(u32*)%d) %c %d;\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', d.Immediate);
			else
			{
				IRShiftOpGenerate(d, szCodeBuffer, false);

				WRITE_CODE("u32 adr = (*(u32*)%d) %c shift_op;\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-');
			}

			if (d.W)
				WRITE_CODE("(*(u32*)%d) = adr;\n", &(GETCPU.R[d.Rn]));
		}
		else
		{
			WRITE_CODE("u32 adr = (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));
			if (d.I)
				WRITE_CODE("(*(u32*)%d) = adr %c %d;\n", &(GETCPU.R[d.Rn]), d.Immediate, d.U ? '+' : '-');
			else
			{
				IRShiftOpGenerate(d, szCodeBuffer, false);

				WRITE_CODE("(*(u32*)%d) = adr %c shift_op;\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-');
			}
		}

		if (d.B)
			WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32*))%d)(adr,(u32*)%d);\n", LDRB_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));
		else
		{
			if (d.R15Modified)
			{
				WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32*))%d)(adr,(u32*)%d);\n", LDR_R15_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));

				if (PROCNUM == 0)
				{
					WRITE_CODE("((Status_Reg*)%d)->bits.T=BIT0((*(u32*)%d));\n", &(GETCPU.CPSR), &(GETCPU.R[15]));
					WRITE_CODE("(*(u32*)%d) &= 0xFFFFFFFE", &(GETCPU.R[15]));
				}
				else
					WRITE_CODE("(*(u32*)%d) &= 0xFFFFFFFC", &(GETCPU.R[15]));

				R15ModifiedGenerate(d, szCodeBuffer);
			}
			else
				WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32*))%d)(adr,(u32*)%d);\n", LDR_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));
		}
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_STR(u32 adr, u32 data)
	{
		WRITE32(GETCPU.mem_if->data, adr, data);
		return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_WRITE>(cycle,adr);
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_STRB(u32 adr, u32 data)
	{
		WRITE8(GETCPU.mem_if->data, adr, data);
		return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_WRITE>(cycle,adr);
	}

	static const MemOp2 STR_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_STR<0,MEMTYPE_GENERIC,2>,
			MEMOP_STR<0,MEMTYPE_MAIN,2>,
			MEMOP_STR<0,MEMTYPE_DTCM,2>,
			MEMOP_STR<0,MEMTYPE_ERAM,2>,
			MEMOP_STR<0,MEMTYPE_SWIRAM,2>,
		},
		{
			MEMOP_STR<1,MEMTYPE_GENERIC,2>,
			MEMOP_STR<1,MEMTYPE_MAIN,2>,
			MEMOP_STR<1,MEMTYPE_DTCM,2>,
			MEMOP_STR<1,MEMTYPE_ERAM,2>,
			MEMOP_STR<1,MEMTYPE_SWIRAM,2>,
		}
	};

	static const MemOp2 STRB_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_STRB<0,MEMTYPE_GENERIC,2>,
			MEMOP_STRB<0,MEMTYPE_MAIN,2>,
			MEMOP_STRB<0,MEMTYPE_DTCM,2>,
			MEMOP_STRB<0,MEMTYPE_ERAM,2>,
			MEMOP_STRB<0,MEMTYPE_SWIRAM,2>,
		},
		{
			MEMOP_STRB<1,MEMTYPE_GENERIC,2>,
			MEMOP_STRB<1,MEMTYPE_MAIN,2>,
			MEMOP_STRB<1,MEMTYPE_DTCM,2>,
			MEMOP_STRB<1,MEMTYPE_ERAM,2>,
			MEMOP_STRB<1,MEMTYPE_SWIRAM,2>,
		}
	};

	OPCDECODER_DECL(IR_STR)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.P)
		{
			if (d.I)
				WRITE_CODE("u32 adr = (*(u32*)%d) %c %d;\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', d.Immediate);
			else
			{
				IRShiftOpGenerate(d, szCodeBuffer, false);

				WRITE_CODE("u32 adr = (*(u32*)%d) %c shift_op;\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-');
			}

			if (d.W)
				WRITE_CODE("(*(u32*)%d) = adr;\n", &(GETCPU.R[d.Rn]));
		}
		else
			WRITE_CODE("u32 adr = (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));

		if (d.B)
			WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32))%d)(adr,(*(u32*)%d));\n", STRB_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));
		else
			WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32))%d)(adr,(*(u32*)%d));\n", STR_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));

		if (!d.P)
		{
			if (d.I)
				WRITE_CODE("(*(u32*)%d) = adr %c %d;\n", &(GETCPU.R[d.Rn]), d.Immediate, d.U ? '+' : '-');
			else
			{
				IRShiftOpGenerate(d, szCodeBuffer, false);

				WRITE_CODE("(*(u32*)%d) = adr %c shift_op;\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-');
			}
		}
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_LDRH(u32 adr, u32 *dstreg)
	{
		*dstreg = READ16(GETCPU.mem_if->data, adr);
		return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_READ>(cycle,adr);
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_LDRSH(u32 adr, u32 *dstreg)
	{
		*dstreg = (s16)READ16(GETCPU.mem_if->data, adr);
		return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_READ>(cycle,adr);
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_LDRSB(u32 adr, u32 *dstreg)
	{
		*dstreg = (s8)READ8(GETCPU.mem_if->data, adr);
		return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_READ>(cycle,adr);
	}

	static const MemOp1 LDRH_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_LDRH<0,MEMTYPE_GENERIC,3>,
			MEMOP_LDRH<0,MEMTYPE_MAIN,3>,
			MEMOP_LDRH<0,MEMTYPE_DTCM,3>,
			MEMOP_LDRH<0,MEMTYPE_ERAM,3>,
			MEMOP_LDRH<0,MEMTYPE_SWIRAM,3>,
		},
		{
			MEMOP_LDRH<1,MEMTYPE_GENERIC,3>,
			MEMOP_LDRH<1,MEMTYPE_MAIN,3>,
			MEMOP_LDRH<1,MEMTYPE_DTCM,3>,
			MEMOP_LDRH<1,MEMTYPE_ERAM,3>,
			MEMOP_LDRH<1,MEMTYPE_SWIRAM,3>,
		}
	};

	static const MemOp1 LDRSH_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_LDRSH<0,MEMTYPE_GENERIC,3>,
			MEMOP_LDRSH<0,MEMTYPE_MAIN,3>,
			MEMOP_LDRSH<0,MEMTYPE_DTCM,3>,
			MEMOP_LDRSH<0,MEMTYPE_ERAM,3>,
			MEMOP_LDRSH<0,MEMTYPE_SWIRAM,3>,
		},
		{
			MEMOP_LDRSH<1,MEMTYPE_GENERIC,3>,
			MEMOP_LDRSH<1,MEMTYPE_MAIN,3>,
			MEMOP_LDRSH<1,MEMTYPE_DTCM,3>,
			MEMOP_LDRSH<1,MEMTYPE_ERAM,3>,
			MEMOP_LDRSH<1,MEMTYPE_SWIRAM,3>,
		}
	};

	static const MemOp1 LDRSB_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_LDRSB<0,MEMTYPE_GENERIC,3>,
			MEMOP_LDRSB<0,MEMTYPE_MAIN,3>,
			MEMOP_LDRSB<0,MEMTYPE_DTCM,3>,
			MEMOP_LDRSB<0,MEMTYPE_ERAM,3>,
			MEMOP_LDRSB<0,MEMTYPE_SWIRAM,3>,
		},
		{
			MEMOP_LDRSB<1,MEMTYPE_GENERIC,3>,
			MEMOP_LDRSB<1,MEMTYPE_MAIN,3>,
			MEMOP_LDRSB<1,MEMTYPE_DTCM,3>,
			MEMOP_LDRSB<1,MEMTYPE_ERAM,3>,
			MEMOP_LDRSB<1,MEMTYPE_SWIRAM,3>,
		}
	};

	OPCDECODER_DECL(IR_LDRx)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.P)
		{
			if (d.I)
				WRITE_CODE("u32 adr = (*(u32*)%d) %c %d;\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', d.Immediate);
			else
				WRITE_CODE("u32 adr = (*(u32*)%d) %c (*(u32*)%d);\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', &(GETCPU.R[d.Rm]));

			if (d.W)
				WRITE_CODE("(*(u32*)%d) = adr;\n", &(GETCPU.R[d.Rn]));
		}
		else
		{
			WRITE_CODE("u32 adr = (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));

			if (d.I)
				WRITE_CODE("(*(u32*)%d) = adr %c %d;\n", &(GETCPU.R[d.Rn]), d.Immediate, d.U ? '+' : '-');
			else
				WRITE_CODE("(*(u32*)%d) = adr %c (*(u32*)%d);\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', &(GETCPU.R[d.Rm]));
		}

		if (d.H)
		{
			if (d.S)
				WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32*))%d)(adr,(u32*)%d);\n", LDRSH_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));
			else
				WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32*))%d)(adr,(u32*)%d);\n", LDRH_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));
		}
		else
			WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32*))%d)(adr,(u32*)%d);\n", LDRSB_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_STRH(u32 adr, u32 data)
	{
		WRITE16(GETCPU.mem_if->data, adr, data);
		return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_WRITE>(cycle,adr);
	}

	static const MemOp2 STRH_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_STRH<0,MEMTYPE_GENERIC,2>,
			MEMOP_STRH<0,MEMTYPE_MAIN,2>,
			MEMOP_STRH<0,MEMTYPE_DTCM,2>,
			MEMOP_STRH<0,MEMTYPE_ERAM,2>,
			MEMOP_STRH<0,MEMTYPE_SWIRAM,2>,
		},
		{
			MEMOP_STRH<1,MEMTYPE_GENERIC,2>,
			MEMOP_STRH<1,MEMTYPE_MAIN,2>,
			MEMOP_STRH<1,MEMTYPE_DTCM,2>,
			MEMOP_STRH<1,MEMTYPE_ERAM,2>,
			MEMOP_STRH<1,MEMTYPE_SWIRAM,2>,
		}
	};

	OPCDECODER_DECL(IR_STRx)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.P)
		{
			if (d.I)
				WRITE_CODE("u32 adr = (*(u32*)%d) %c %d;\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', d.Immediate);
			else
				WRITE_CODE("u32 adr = (*(u32*)%d) %c (*(u32*)%d);\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', &(GETCPU.R[d.Rm]));

			if (d.W)
				WRITE_CODE("(*(u32*)%d) = adr;\n", &(GETCPU.R[d.Rn]));
		}
		else
			WRITE_CODE("u32 adr = (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));

		WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32))%d)(adr,(*(u32*)%d));\n", STRH_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));

		if (!d.P)
		{
			if (d.I)
				WRITE_CODE("(*(u32*)%d) = adr %c %d;\n", &(GETCPU.R[d.Rn]), d.Immediate, d.U ? '+' : '-');
			else
				WRITE_CODE("(*(u32*)%d) = adr %c (*(u32*)%d);\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', &(GETCPU.R[d.Rm]));
		}
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_LDRD(u32 adr, u32 *dstreg)
	{
		*dstreg = READ32(GETCPU.mem_if->data, adr);
		*(dstreg+1) = READ32(GETCPU.mem_if->data, adr+4);
		return MMU_aluMemCycles<PROCNUM>(cycle, MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr+4));
	}

	static const MemOp1 LDRD_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_LDRD<0,MEMTYPE_GENERIC,3>,
			MEMOP_LDRD<0,MEMTYPE_MAIN,3>,
			MEMOP_LDRD<0,MEMTYPE_DTCM,3>,
			MEMOP_LDRD<0,MEMTYPE_ERAM,3>,
			MEMOP_LDRD<0,MEMTYPE_SWIRAM,3>,
		},
		{
			MEMOP_LDRD<1,MEMTYPE_GENERIC,3>,
			MEMOP_LDRD<1,MEMTYPE_MAIN,3>,
			MEMOP_LDRD<1,MEMTYPE_DTCM,3>,
			MEMOP_LDRD<1,MEMTYPE_ERAM,3>,
			MEMOP_LDRD<1,MEMTYPE_SWIRAM,3>,
		}
	};

	OPCDECODER_DECL(IR_LDRD)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.P)
		{
			if (d.I)
				WRITE_CODE("u32 adr = (*(u32*)%d) %c %d;\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', d.Immediate);
			else
				WRITE_CODE("u32 adr = (*(u32*)%d) %c (*(u32*)%d);\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', &(GETCPU.R[d.Rm]));

			if (d.W)
				WRITE_CODE("(*(u32*)%d) = adr;\n", &(GETCPU.R[d.Rn]));
		}
		else
		{
			WRITE_CODE("u32 adr = (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));

			if (d.I)
				WRITE_CODE("(*(u32*)%d) = adr %c %d;\n", &(GETCPU.R[d.Rn]), d.Immediate, d.U ? '+' : '-');
			else
				WRITE_CODE("(*(u32*)%d) = adr %c (*(u32*)%d);\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', &(GETCPU.R[d.Rm]));
		}

		WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32*))%d)(adr,(u32*)%d);\n", LDRD_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_STRD(u32 adr, u32 *srcreg)
	{
		WRITE32(GETCPU.mem_if->data, adr, *srcreg);
		WRITE32(GETCPU.mem_if->data, adr+4, *(srcreg+1));
		return MMU_aluMemCycles<PROCNUM>(cycle, MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr+4));
	}

	static const MemOp1 STRD_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_STRD<0,MEMTYPE_GENERIC,3>,
			MEMOP_STRD<0,MEMTYPE_MAIN,3>,
			MEMOP_STRD<0,MEMTYPE_DTCM,3>,
			MEMOP_STRD<0,MEMTYPE_ERAM,3>,
			MEMOP_STRD<0,MEMTYPE_SWIRAM,3>,
		},
		{
			MEMOP_STRD<1,MEMTYPE_GENERIC,3>,
			MEMOP_STRD<1,MEMTYPE_MAIN,3>,
			MEMOP_STRD<1,MEMTYPE_DTCM,3>,
			MEMOP_STRD<1,MEMTYPE_ERAM,3>,
			MEMOP_STRD<1,MEMTYPE_SWIRAM,3>,
		}
	};

	OPCDECODER_DECL(IR_STRD)
	{
		u32 PROCNUM = d.ProcessID;

		if (d.P)
		{
			if (d.I)
				WRITE_CODE("u32 adr = (*(u32*)%d) %c %d;\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', d.Immediate);
			else
				WRITE_CODE("u32 adr = (*(u32*)%d) %c (*(u32*)%d);\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', &(GETCPU.R[d.Rm]));

			if (d.W)
				WRITE_CODE("(*(u32*)%d) = adr;\n", &(GETCPU.R[d.Rn]));
		}
		else
		{
			WRITE_CODE("u32 adr = (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));

			if (d.I)
				WRITE_CODE("(*(u32*)%d) = adr %c %d;\n", &(GETCPU.R[d.Rn]), d.Immediate, d.U ? '+' : '-');
			else
				WRITE_CODE("(*(u32*)%d) = adr %c (*(u32*)%d);\n", &(GETCPU.R[d.Rn]), d.U ? '+' : '-', &(GETCPU.R[d.Rm]));
		}

		WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32*))%d)(adr,(u32*)%d);\n", STRD_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));
	}

	OPCDECODER_DECL(IR_LDREX)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("u32 adr = (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));

		WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32*))%d)(adr,(u32*)%d);\n", LDR_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));
	}

	OPCDECODER_DECL(IR_STREX)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("u32 adr = (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));

		WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32))%d)(adr,(*(u32*)%d));\n", STR_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]));

		WRITE_CODE("(*(u32*)%d) = 0;\n", &(GETCPU.R[d.Rm]));
	}

	OPCDECODER_DECL(IR_LDM)
	{
	}

	OPCDECODER_DECL(IR_STM)
	{
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_SWP(u32 adr, u32 *Rd, u32 Rm)
	{
		u32 tmp = ROR(READ32(GETCPU.mem_if->data, adr), (adr & 3)<<3);
		WRITE32(GETCPU.mem_if->data, adr, Rm);
		*Rd = tmp;

		return MMU_aluMemCycles<PROCNUM>(cycle, MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr));
	}

	template<u32 PROCNUM, u32 memtype, u32 cycle>
	static u32 FASTCALL MEMOP_SWPB(u32 adr, u32 *Rd, u32 Rm)
	{
		u32 tmp = READ8(GETCPU.mem_if->data, adr);
		WRITE8(GETCPU.mem_if->data, adr, Rm);
		*Rd = tmp;

		return MMU_aluMemCycles<PROCNUM>(cycle, MMU_memAccessCycles<PROCNUM,8,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,8,MMU_AD_WRITE>(adr));
	}

	static const MemOp3 SWP_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_SWP<0,MEMTYPE_GENERIC,4>,
			MEMOP_SWP<0,MEMTYPE_MAIN,4>,
			MEMOP_SWP<0,MEMTYPE_DTCM,4>,
			MEMOP_SWP<0,MEMTYPE_ERAM,4>,
			MEMOP_SWP<0,MEMTYPE_SWIRAM,4>,
		},
		{
			MEMOP_SWP<1,MEMTYPE_GENERIC,4>,
			MEMOP_SWP<1,MEMTYPE_MAIN,4>,
			MEMOP_SWP<1,MEMTYPE_DTCM,4>,
			MEMOP_SWP<1,MEMTYPE_ERAM,4>,
			MEMOP_SWP<1,MEMTYPE_SWIRAM,4>,
		}
	};

	static const MemOp3 SWPB_Tab[2][MEMTYPE_COUNT] = 
	{
		{
			MEMOP_SWPB<0,MEMTYPE_GENERIC,4>,
			MEMOP_SWPB<0,MEMTYPE_MAIN,4>,
			MEMOP_SWPB<0,MEMTYPE_DTCM,4>,
			MEMOP_SWPB<0,MEMTYPE_ERAM,4>,
			MEMOP_SWPB<0,MEMTYPE_SWIRAM,4>,
		},
		{
			MEMOP_SWPB<1,MEMTYPE_GENERIC,4>,
			MEMOP_SWPB<1,MEMTYPE_MAIN,4>,
			MEMOP_SWPB<1,MEMTYPE_DTCM,4>,
			MEMOP_SWPB<1,MEMTYPE_ERAM,4>,
			MEMOP_SWPB<1,MEMTYPE_SWIRAM,4>,
		}
	};

	OPCDECODER_DECL(IR_SWP)
	{
		u32 PROCNUM = d.ProcessID;

		WRITE_CODE("u32 adr = (*(u32*)%d);\n", &(GETCPU.R[d.Rn]));

		if (d.B)
			WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32*, u32))%d)(adr,(u32*)%d,(*(u32*)%d));\n", SWPB_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rm]));
		else
			WRITE_CODE("ExecuteCycles+=((u32 (FASTCALL *)(u32, u32*, u32))%d)(adr,(u32*)%d,(*(u32*)%d));\n", SWP_Tab[PROCNUM][0], &(GETCPU.R[d.Rd]), &(GETCPU.R[d.Rm]));
	}

	OPCDECODER_DECL(IR_B)
	{
	}

	OPCDECODER_DECL(IR_BL)
	{
	}

	OPCDECODER_DECL(IR_BX)
	{
	}

	OPCDECODER_DECL(IR_BLX)
	{
	}

	OPCDECODER_DECL(IR_SWI)
	{
	}

	OPCDECODER_DECL(IR_MSR)
	{
	}

	OPCDECODER_DECL(IR_MRS)
	{
	}

	OPCDECODER_DECL(IR_MCR)
	{
	}

	OPCDECODER_DECL(IR_MRC)
	{
	}

	OPCDECODER_DECL(IR_CLZ)
	{
	}

	OPCDECODER_DECL(IR_QADD)
	{
	}

	OPCDECODER_DECL(IR_QSUB)
	{
	}

	OPCDECODER_DECL(IR_QDADD)
	{
	}

	OPCDECODER_DECL(IR_QDSUB)
	{
	}

	OPCDECODER_DECL(IR_BLX_IMM)
	{
	}

	OPCDECODER_DECL(IR_BKPT)
	{
	}
};

static const IROpCDecoder iropcdecoder_set[IR_MAXNUM] = {
#define TABDECL(x) ArmCJit::x##_CDecoder
#include "ArmAnalyze_tabdef.inc"
#undef TABDECL
};

#endif
