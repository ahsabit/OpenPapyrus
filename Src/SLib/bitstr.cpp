// BITSTR.CPP
// Copyright (c) Sobolev A. 1995-2001, 2004, 2005, 2006, 2008, 2010, 2013, 2016, 2017, 2018, 2019, 2020, 2021, 2022, 2023
//
#include <slib-internal.h>
#pragma hdrstop

#define HIBIT       0x80000000UL
#define BLKIDX(p)   ((p)>>5)
#define BLKBIT(p)   ((p)&0x1f)
//
// See: http://wiki.cs.pdx.edu/forge/popcount.html
//
/* @v11.9.2 (replaced with SBits::Cpop) uint32 FASTCALL popcount32(uint32 x)
{
#define m1 0x55555555
#define m2 0x33333333
#define m4 0x0f0f0f0f
	x -= (x >> 1) & m1;
	x = (x & m2) + ((x >> 2) & m2);
	x = (x + (x >> 4)) & m4;
	x += x >>  8;
	return (x + (x >> 16)) & 0x3f;
#undef m1
#undef m2
#undef m4
}*/
//
// from the "Bit Twiddling Hacks" webpage
//
uint32 FASTCALL parity32(uint32 v)
{
	v ^= v >> 1;
	v ^= v >> 2;
	v = (v & 0x11111111U) * 0x11111111U;
	return (v >> 28) & 1;
}

void FASTCALL resetbitstring(void * pBuf, size_t len)
{
	memzero(pBuf, len);
}

#pragma warn -par

void STDCALL setbit32(void * pBuf, size_t len, size_t pos)
{
	size_t idx = BLKIDX(pos);
	if(idx < (len>>2))
		PTR32(pBuf)[idx] |= (1U << BLKBIT(pos));
}

void STDCALL resetbit32(void * pBuf, size_t len, size_t pos)
{
	size_t idx = BLKIDX(pos);
	if(idx < (len>>2))
		PTR32(pBuf)[idx] &= ~(1U << BLKBIT(pos));
}

static FORCEINLINE int implement_getbit32(const void * pBuf, size_t len, size_t pos)
{
	size_t idx = BLKIDX(pos);
	return (idx < (len>>2)) ? BIN(PTR32C(pBuf)[idx] & (1U << BLKBIT(pos))) : 0;
}

int STDCALL getbit32(const void * pBuf, size_t len, size_t pos)
{
	return implement_getbit32(pBuf, len, pos);
}

int STDCALL getbit8(const void * pBuf, size_t len, size_t pos)
{
	size_t idx = (pos >> 3);
	pos = pos & 0x7;
	return (idx < len) ? ((PTR8C(pBuf)[idx] >> pos) & 1) : 0;
}

uint32 getbits(const void * pBuf, size_t len, size_t pos, size_t count)
{
	uint32 r = 0;
	assert(count <= 32);
	size_t idx = BLKIDX(pos);
	if(idx < (len>>2)) {
		size_t max_count = (len>>2)-idx;
		max_count = MIN(32, max_count);
		SETMIN(count, max_count);
		switch(count) {
			case 1:
				r |= implement_getbit32(pBuf, len, pos);
				break;
			case 2:
				r |= (implement_getbit32(pBuf, len, pos) << 1);
				r |= (implement_getbit32(pBuf, len, pos+1));
				break;
			case 3:
				r |= (implement_getbit32(pBuf, len, pos) << 2);
				r |= (implement_getbit32(pBuf, len, pos+1) << 1);
				r |= (implement_getbit32(pBuf, len, pos+2));
				break;
			case 4:
				r |= (implement_getbit32(pBuf, len, pos) << 3);
				r |= (implement_getbit32(pBuf, len, pos+1) << 2);
				r |= (implement_getbit32(pBuf, len, pos+2) << 1);
				r |= (implement_getbit32(pBuf, len, pos+3));
				break;
			case 5:
				r |= (implement_getbit32(pBuf, len, pos) << 4);
				r |= (implement_getbit32(pBuf, len, pos+1) << 3);
				r |= (implement_getbit32(pBuf, len, pos+2) << 2);
				r |= (implement_getbit32(pBuf, len, pos+3) << 1);
				r |= (implement_getbit32(pBuf, len, pos+4));
				break;
			case 6:
				r |= (implement_getbit32(pBuf, len, pos) << 5);
				r |= (implement_getbit32(pBuf, len, pos+1) << 4);
				r |= (implement_getbit32(pBuf, len, pos+2) << 3);
				r |= (implement_getbit32(pBuf, len, pos+3) << 2);
				r |= (implement_getbit32(pBuf, len, pos+4) << 1);
				r |= (implement_getbit32(pBuf, len, pos+5));
				break;
			case 7:
				r |= (implement_getbit32(pBuf, len, pos) << 6);
				r |= (implement_getbit32(pBuf, len, pos+1) << 5);
				r |= (implement_getbit32(pBuf, len, pos+2) << 4);
				r |= (implement_getbit32(pBuf, len, pos+3) << 3);
				r |= (implement_getbit32(pBuf, len, pos+4) << 2);
				r |= (implement_getbit32(pBuf, len, pos+5) << 1);
				r |= (implement_getbit32(pBuf, len, pos+6));
				break;
			case 8:
				r |= (implement_getbit32(pBuf, len, pos) << 7);
				r |= (implement_getbit32(pBuf, len, pos+1) << 6);
				r |= (implement_getbit32(pBuf, len, pos+2) << 5);
				r |= (implement_getbit32(pBuf, len, pos+3) << 4);
				r |= (implement_getbit32(pBuf, len, pos+4) << 3);
				r |= (implement_getbit32(pBuf, len, pos+5) << 2);
				r |= (implement_getbit32(pBuf, len, pos+6) << 1);
				r |= (implement_getbit32(pBuf, len, pos+7));
				break;
			default:
				{
					for(uint i = 0; i < count; ++i)
						r |= (implement_getbit32(pBuf, len, pos+i) << (count - i - 1));
				}
				break;
		}
	}
	return r;
}

int STDCALL findbit(const void * pBuf, size_t count, int val, size_t * pPos)
{
	/* @project
	static const uint32 map[32] = {
		0x00000001, 0x00000002, 0x00000004, 0x00000008,
		0x00000010, 0x00000020, 0x00000040, 0x00000080,
		0x00000100, 0x00000200, 0x00000400, 0x00000800,
		0x00001000, 0x00002000, 0x00004000, 0x00008000,
		0x00010000, 0x00020000, 0x00040000, 0x00080000,
		0x00100000, 0x00200000, 0x00400000, 0x00800000,
		0x01000000, 0x02000000, 0x04000000, 0x08000000,
		0x10000000, 0x20000000, 0x40000000, 0x80000000
	};
	*/
	size_t i = 0;
	size_t last_word = BLKIDX(count);
	if(val) {
		while(i <= last_word && PTR32C(pBuf)[i] == 0)
			++i;
		if(i <= last_word) {
			uint32 dw = PTR32C(pBuf)[i];
			for(uint j = 0; j < 32; j++) {
				// @project if(dw & map[j]) {
				if(dw & (1U << j)) {
					ASSIGN_PTR(pPos, (i << 5) + j);
					return 1;
				}
			}
		}
	}
	else {
		while(i <= last_word && PTR32C(pBuf)[i] == 0xffffffffUL)
			++i;
		if(i <= last_word) {
			uint32 dw = PTR32C(pBuf)[i];
			for(uint j = 0; j < 32; j++)
				// @project if((dw & map[j]) == 0) {
				if((dw & (1U << j)) == 0) {
					ASSIGN_PTR(pPos, (i << 5) + j);
					return 1;
				}
		}
	}
	return 0;
}

#pragma warn +par

void insbit(void * pBuf, size_t len, size_t pos)
{
	const size_t firstword = BLKIDX(pos);
	uint32 blk = PTR32(pBuf)[firstword];
	uint32 carry = 0;
	if(blk & HIBIT) {
		carry = 1;
		blk &= ~HIBIT;
	}
	{
		for(int i = 30; i >= (int)BLKBIT(pos); i--) {
			if(blk & (1U << i)) {
				blk |= (1U << (i + 1));
				blk &= ~(1U << i);
			}
		}
	}
	PTR32(pBuf)[firstword] = blk;
	{
		const size_t num_blk = (int)(len>>2);
		for(size_t i = firstword + 1; i < num_blk; i++) {
			blk = PTR32(pBuf)[i];
			uint32 carry1 = BIN(blk & HIBIT);
			blk <<= 1;
			blk |= carry;
			carry = carry1;
			PTR32(pBuf)[i] = blk;
		}
	}
	resetbit32(pBuf, len, pos);
}

void delbit(void * pBuf, size_t len, size_t pos)
{
	size_t i;
	size_t firstword = BLKIDX(pos);
	uint32 blk = PTR32(pBuf)[firstword];
	blk &= ~(1U << BLKBIT(pos));
	for(i = BLKBIT(pos) + 1; i < 32; i++) {
		if(blk & (1U << i)) {
			blk |= (1U << (i - 1));
			blk &= ~(1U << i);
		}
	}
	PTR32(pBuf)[firstword] = blk;
	size_t num_blk = (len>>2);
	for(i = firstword + 1; i < num_blk; i++) {
		if(PTR32(pBuf)[i] & 1U)
			PTR32(pBuf)[i-1] |= HIBIT;
		PTR32(pBuf)[i] >>= 1;
	}
}
//
//
//
uint8 FASTCALL bitscanforward(uint * pIdx, uint32 mask) { return bitscanforward(reinterpret_cast<ulong *>(pIdx), mask); }

uint8 FASTCALL bitscanforward(ulong * pIdx, uint32 mask)
{
#if _MSC_VER >= 1600
	if(mask)
		return _BitScanForward(pIdx, mask);
	else {
		ASSIGN_PTR(pIdx, 0);
		return 0;
	}
#else
	uint16 ret_ = 0;
	uint32 idx = 0;
	__asm {
		xor edx, edx
		bsf edx, mask
		jz  lab_zero
		mov ax, 1
		jmp lab_done
lab_zero:
		xor edx, edx
		xor ax, ax
lab_done:
		mov idx, edx
		mov ret_, ax
	}
	ASSIGN_PTR(pIdx, idx);
	return (uint8)ret_;
#endif
}

uint8 FASTCALL bitscanreverse(uint * pIdx, uint32 mask) { return bitscanreverse(reinterpret_cast<ulong *>(pIdx), mask); }

uint8 FASTCALL bitscanreverse(ulong * pIdx, uint32 mask)
{
#if _MSC_VER >= 1600
	if(mask)
		return _BitScanReverse(pIdx, mask);
	else {
		ASSIGN_PTR(pIdx, 0);
		return 0;
	}
#else
	uint16 ret_ = 0;
	uint32 idx = 0;
	__asm {
		xor edx, edx
		bsr edx, mask
		jz  lab_zero
		mov ax, 1
		jmp lab_done;
lab_zero:
		xor edx, edx
		xor ax, ax
lab_done:
		mov idx, edx
		mov ret_, ax
	}
	ASSIGN_PTR(pIdx, idx);
	return (uint8)ret_;
#endif
}
