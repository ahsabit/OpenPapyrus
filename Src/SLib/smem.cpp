// SMEM.CPP
// Copyright (c) Sobolev A. 1993-2001, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2016, 2017, 2019, 2020, 2021, 2022, 2023
// @codepage UTF-8
//
#include <slib-internal.h>
#pragma hdrstop
//
// @v11.2.3 Поступила информация, что JobServer в последних релизах "падает". Отключаем для сервера mimalloc и проверям.
// @v11.2.4 Похоже, возникают спонтанные аварии и на клиентских сессиях
//
#if _MSC_VER >= 1900 /*&& !defined(NDEBUG)*/ && !defined(_PPSERVER)
	// @v11.2.4 #define USE_MIMALLOC
#endif
#ifdef USE_MIMALLOC
	#include "mimalloc\mimalloc.h"
#endif
//
//
//
#define REP8(op)       op;op;op;op;op;op;op;op
#define REP_CASE3(op)  case 3:op;case 2:op;case 1:op;
#define REP_CASE7(op)  case 7:op;case 6:op;case 5:op;case 4:op;case 3:op;case 2:op;case 1:op;

//#define ALIGNSIZE(s,bits)     ((((s) + ((1 << (bits))-1)) >> (bits)) << (bits))
#define ALIGNDOWN(s,bits)     (((s) >> (bits)) << (bits))

bool FASTCALL ismemzero(const void * p, size_t s)
{
	switch(s) {
		case  1: return BIN(PTR8C(p)[0] == 0);
		case  2: return BIN(PTR16C(p)[0] == 0);
		case  4: return BIN(PTR32C(p)[0] == 0);
		case  8: return BIN(PTR32C(p)[0] == 0 && PTR32C(p)[1] == 0);
		case 12: return BIN(PTR32C(p)[0] == 0 && PTR32C(p)[1] == 0 && PTR32C(p)[2] == 0);
		case 16: return BIN(PTR32C(p)[0] == 0 && PTR32C(p)[1] == 0 && PTR32C(p)[2] == 0 && PTR32C(p)[3] == 0);
	}
	size_t v = s / 4;
	size_t m = s % 4;
	size_t i;
	for(i = 0; i < v; i++) {
		if(PTR32C(p)[i] != 0)
			return false;
	}
	for(i = 0; i < m; i++)
		if(PTR8C(p)[(v*4)+i] != 0)
			return false;
	return true;
}

void * FASTCALL memzero(void * p, size_t s)
{
	if(p) {
		switch(s) {
			case 1: PTR8(p)[0] = 0; break;
			case 2: PTR16(p)[0] = 0; break;
			case 4:
				PTR32(p)[0] = 0;
				break;
			case 8:
				PTR32(p)[0] = 0;
				PTR32(p)[1] = 0;
				break;
			case 12:
				PTR32(p)[0] = 0;
				PTR32(p)[1] = 0;
				PTR32(p)[2] = 0;
				break;
			case 16:
				PTR32(p)[0] = 0;
				PTR32(p)[1] = 0;
				PTR32(p)[2] = 0;
				PTR32(p)[3] = 0;
				break;
			case 20:
				PTR32(p)[0] = 0;
				PTR32(p)[1] = 0;
				PTR32(p)[2] = 0;
				PTR32(p)[3] = 0;
				PTR32(p)[4] = 0;
				break;
			default:
				memset(p, 0, s);
				break;
		}
	}
	return p;
}

int memdword(void * p, size_t size, uint32 key, size_t * pOffs)
{
	const size_t s4 = size / 4;
	uint32 * p32 = static_cast<uint32 *>(p);
	for(size_t i = 0; i < s4; i++)
		if(*p32 == key) {
			ASSIGN_PTR(pOffs, i * 4);
			return 1;
		}
		else
			p32++;
	return 0;
}

uint16 FASTCALL swapw(uint16 w) { return ((w>>8) | (w<<8)); }
uint32 FASTCALL swapdw(uint32 dw) { return (((dw & 0xffff0000U) >> 16) | ((dw & 0x0000ffffU) << 16)); }

uint32 FASTCALL msb32(uint32 x)
{
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return (x & ~(x >> 1));
}

void FASTCALL memswap(void * p1, void * p2, size_t size)
{
	switch(size) {
		case 1:
			{
				const uint8 t8 = PTR8(p1)[0];
				PTR8(p1)[0] = PTR8(p2)[0];
				PTR8(p2)[0] = t8;
			}
			break;
		case 2:
			{
				const uint16 t16 = PTR16(p1)[0];
				PTR16(p1)[0] = PTR16(p2)[0];
				PTR16(p2)[0] = t16;
			}
			break;
		case 4:
			{
				const uint32 t32 = PTR32(p1)[0];
				PTR32(p1)[0] = PTR32(p2)[0];
				PTR32(p2)[0] = t32;
			}
			break;
		case 8:
			{
				uint32 t32 = PTR32(p1)[0];
				PTR32(p1)[0] = PTR32(p2)[0];
				PTR32(p2)[0] = t32;
				t32 = PTR32(p1)[1];
				PTR32(p1)[1] = PTR32(p2)[1];
				PTR32(p2)[1] = t32;
			}
			break;
		default:
			{
				const size_t s4 = size / 4; // size >> 2
				const size_t m4 = size % 4; // size &= 3
				if(s4) {
					uint32 * p321 = static_cast<uint32 *>(p1);
					uint32 * p322 = static_cast<uint32 *>(p2);
					for(size_t i = 0; i < s4; i++) {
						const uint32 t32 = *p321;
						*p321++ = *p322;
						*p322++ = t32;
					}
				}
				if(m4) {
					uint8 * p81 = reinterpret_cast<uint8 *>(PTR32(p1)+s4);
					uint8 * p82 = reinterpret_cast<uint8 *>(PTR32(p2)+s4);
					for(size_t i = 0; i < m4; i++) {
						const uint8 t8 = *p81;
						*p81++ = *p82;
						*p82++ = t8;
					}
				}
			}
			break;
	}
}

void * catmem(void * pDest, size_t destSize, const void * pSrc, size_t srcSize)
{
	size_t new_size = destSize + srcSize;
	void * p_tmp = pDest;
	if(new_size >= destSize && (p_tmp = SAlloc::R(pDest, new_size)) != 0)
		memmove((char *)p_tmp + destSize, pSrc, srcSize);
	return p_tmp;
}

//void FASTCALL Exchange(int * pA, int * pB)
//{
//	int    temp = *pA;
//	*pA = *pB;
//	*pB = temp;
//}
//
//void FASTCALL Exchange(uint * pA, uint * pB)
//{
//	uint   temp = *pA;
//	*pA = *pB;
//	*pB = temp;
//}
//
//void FASTCALL ExchangeForOrder(int * pA, int * pB)
//{
//	if(*pA > *pB) {
//		int    temp = *pA;
//		*pA = *pB;
//		*pB = temp;
//	}
//}
//
//void FASTCALL Exchange(long * pA, long * pB)
//{
//	long   temp = *pA;
//	*pA = *pB;
//	*pB = temp;
//}
//
//void FASTCALL ExchangeForOrder(long * pA, long * pB)
//{
//	if(*pA > *pB) {
//		long   temp = *pA;
//		*pA = *pB;
//		*pB = temp;
//	}
//}
//
//void FASTCALL Exchange(ulong * pA, ulong * pB)
//{
//	ulong  temp = *pA;
//	*pA = *pB;
//	*pB = temp;
//}
//
//void FASTCALL Exchange(int16 * pA, int16 * pB)
//{
//	int16  temp = *pA;
//	*pA = *pB;
//	*pB = temp;
//}
//
//void FASTCALL ExchangeForOrder(int16 * pA, int16 * pB)
//{
//	if(*pA > *pB) {
//		int16  temp = *pA;
//		*pA = *pB;
//		*pB = temp;
//	}
//}
//
//void FASTCALL Exchange(int64 * pA, int64 * pB)
//{
//	int64  temp = *pA;
//	*pA = *pB;
//	*pB = temp;
//}
//
//void FASTCALL Exchange(float * pA, float * pB)
//{
//	memswap(pA, pB, sizeof(*pA));
//}
//
//void FASTCALL Exchange(double * pA, double * pB)
//{
//	memswap(pA, pB, sizeof(*pA));
//}
//
//void FASTCALL ExchangeForOrder(double * pA, double * pB)
//{
//	if(*pA > *pB)
//		memswap(pA, pB, sizeof(*pA));
//}
//
//
// @costruction {
SAlloc::Stat::Stat()
{
}

void FASTCALL SAlloc::Stat::RegisterAlloc(uint size)
{
	int do_shrink = 0;
	Lck_A.Lock();
	AllocEntry entry;
	entry.Size = size;
	entry.Count = 1;
	AL.insert(&entry);
	if((AL.getCount() & 0xfff) == 0)
		do_shrink = 1;
	Lck_A.Unlock();
	if(do_shrink)
		Shrink();
}

void SAlloc::Stat::RegisterRealloc(uint fromSize, uint toSize)
{
	int do_shrink = 0;
	Lck_R.Lock();
	ReallocEntry entry;
	entry.FromSize = fromSize;
	entry.ToSize = toSize;
	entry.Count = 1;
	RL.insert(&entry);
	if((RL.getCount() & 0xfff) == 0)
		do_shrink = 1;
	Lck_R.Unlock();
	if(do_shrink)
		Shrink();
}

void SAlloc::Stat::Shrink()
{
	{
		Lck_A.Lock();
		uint i = AL.getCount();
		if(i) do {
			const AllocEntry & r_entry = AL.at(--i);
			for(uint j = 0; j < i; j++) {
				if(AL.at(j).Size == r_entry.Size) {
					AL.at(j).Count += r_entry.Count;
					AL.atFree(i);
					break;
				}
			}
		} while(i);
		Lck_A.Unlock();
	}
	{
		Lck_R.Lock();
		uint i = RL.getCount();
		if(i) do {
			const ReallocEntry & r_entry = RL.at(--i);
			for(uint j = 0; j < i; j++) {
				const ReallocEntry & r_entry2 = RL.at(j);
				if(r_entry2.FromSize == r_entry.FromSize && r_entry2.ToSize == r_entry.ToSize) {
					RL.at(j).Count += r_entry.Count;
					RL.atFree(i);
					break;
				}
			}
		} while(i);
		Lck_R.Unlock();
	}
}

void FASTCALL SAlloc::Stat::Merge(const Stat & rS)
{
	{
		Lck_A.Lock();
		for(uint i = 0; i < rS.AL.getCount(); i++) {
			const AllocEntry & r_entry = rS.AL.at(i);
			uint pos = 0;
			if(AL.lsearch(&r_entry.Size, &pos, CMPF_LONG)) {
				AL.at(pos).Count += r_entry.Count;
			}
		}
		Lck_A.Unlock();
	}
	{
		Lck_R.Lock();
		for(uint i = 0; i < rS.RL.getCount(); i++) {
			const ReallocEntry & r_entry = rS.RL.at(i);
			uint pos = 0;
			if(RL.lsearch(&r_entry.FromSize, &pos, PTR_CMPFUNC(_2long))) {
				RL.at(pos).Count += r_entry.Count;
			}
		}
		Lck_R.Unlock();
	}
}

int SAlloc::Stat::Output(SString & rBuf)
{
	int    ok = -1;
	rBuf.Z();
	if(AL.getCount() || RL.getCount()) {
		Shrink();
		{
			Lck_A.Lock();
			if(AL.getCount()) {
				AL.sort(CMPF_LONG);
				rBuf.Cat("alloc-stat").CR();
				for(uint i = 0; i < AL.getCount(); i++) {
					const AllocEntry & r_entry = AL.at(i);
					rBuf.Cat(r_entry.Size).Tab().Cat(r_entry.Count).CR();
				}
			}
			Lck_A.Unlock();
		}
		{
			Lck_R.Lock();
			if(RL.getCount()) {
				RL.sort(PTR_CMPFUNC(_2long));
				rBuf.Cat("realloc-stat").CR();
				for(uint i = 0; i < RL.getCount(); i++) {
					const ReallocEntry & r_entry = RL.at(i);
					rBuf.Cat(r_entry.FromSize).Tab().Cat(r_entry.ToSize).Tab().Cat(r_entry.Count).CR();
				}
			}
			Lck_R.Unlock();
		}
	}
	return ok;
}
// } @costruction 

/*static*/void * FASTCALL SAlloc::M(size_t sz)
{
#if SLGATHERALLOCSTATISTICS
	SLS.GetAllocStat().RegisterAlloc(sz);
#endif
#ifdef USE_MIMALLOC
	void * p_result = mi_malloc(sz);
#else
	void * p_result = malloc(sz);
#endif
	if(!p_result)
		SLS.SetError(SLERR_NOMEM);
	return p_result;
}

/*static*/void * FASTCALL SAlloc::M_zon0(size_t sz) { return sz ? M(sz) : 0; }

/*static*/void * FASTCALL SAlloc::C(size_t n, size_t sz)
{
#if SLGATHERALLOCSTATISTICS
	SLS.GetAllocStat().RegisterAlloc(n * sz);
#endif
#ifdef USE_MIMALLOC
	void * p_result = mi_calloc(n, sz);
#else
	void * p_result = calloc(n, sz);
#endif
	if(!p_result)
		SLS.SetError(SLERR_NOMEM);
	return p_result;
}

/*static*/void * FASTCALL SAlloc::C_zon0(size_t n, size_t sz) { return (n && sz) ? C(n, sz) : 0; }

/*static*/void * FASTCALL SAlloc::R(void * ptr, size_t sz)
{
#if SLGATHERALLOCSTATISTICS
	// На предварительном этапе будем realloc регистрировать как Alloc поскольку я пока не разобрался как получить размер
	// перераспределяемого блока
	SLS.GetAllocStat().RegisterAlloc(sz);
#endif
#ifdef USE_MIMALLOC
	void * p_result = mi_realloc(ptr, sz);
#else
	void * p_result = realloc(ptr, sz);
#endif
	if(!p_result)
		SLS.SetError(SLERR_NOMEM);
	return p_result;
}

/*static*/void FASTCALL SAlloc::F(void * ptr)
{
#ifdef USE_MIMALLOC
	mi_free(ptr);
#else
	free(ptr);
#endif
}

void * operator new(size_t sz)
{
#if SLGATHERALLOCSTATISTICS
	SLS.GetAllocStat().RegisterAlloc(sz);
#endif
#ifdef USE_MIMALLOC
	void * p_result = mi_new(sz);
#else
	void * p_result = malloc(sz);
#endif
	if(!p_result)
		SLS.SetError(SLERR_NOMEM);
	return p_result;
}

void operator delete(void * ptr)
{
#ifdef USE_MIMALLOC
	mi_free(ptr);
#else
	free(ptr);
#endif
}

void operator delete [] (void * ptr)
{
#ifdef USE_MIMALLOC
	mi_free(ptr);
#else
	free(ptr);
#endif
}

void FASTCALL SObfuscateBuffer(void * pBuf, size_t bufSize)
{
	SLS.GetTLA().Rg.ObfuscateBuffer(pBuf, bufSize);
}
//
// @TEST {
//
#if SLTEST_RUNNING // {

#define MEMPAIR_COUNT 1000
#define MEMBLK_SIZE   (36*1024)
#define MEMBLK_EXT_SIZE 32
#define BYTES_SIZE    (5 * 1024) // Величина должна превышать размер страницы памяти

#include <asmlib.h> // @v7.0.12

SLTEST_R(Endian)
{
	int    ok = 1;
	const uint64 k_initial_number{0x0123456789abcdef};
	const uint64 k64value{k_initial_number};
	const uint   k32value_u{0x01234567U};
	const ulong  k32value_l{0x01234567UL};
	const uint16 k16value{0x0123};
#if defined(SL_BIGENDIAN)
	const uint64_t k_initial_in_network_order{k_initial_number};
	const uint64_t k64value_le{0xefcdab8967452301};
	const uint32_t k32value_le{0x67452301};
	const uint16_t k16value_le{0x2301};

	const uint64_t k64value_be{kInitialNumber};
	const uint32_t k32value_be{k32value_u};
	const uint16_t k16value_be{k16value};
#elif defined(SL_LITTLEENDIAN)
	const uint64_t k_initial_in_network_order{0xefcdab8967452301};
	const uint64_t k64value_le{k_initial_number};
	const uint32_t k32value_le{k32value_u};
	const uint16_t k16value_le{k16value};

	const uint64_t k64value_be{0xefcdab8967452301};
	const uint32_t k32value_be{0x67452301};
	const uint16_t k16value_be{0x2301};

	assert(k64value_be == sbswap64(k64value));
	assert(k32value_be == sbswap32(k32value_u));
	assert(k32value_be == sbswap32(k32value_l));
	assert(k16value_be == sbswap16(k16value));
	assert(k64value_be == sbswap64_fallback(k64value));
	assert(k32value_be == sbswap32_fallback(k32value_u));
	assert(k32value_be == sbswap32_fallback(k32value_l));
	assert(k16value_be == sbswap16_fallback(k16value));
#endif
	return ok;
}

struct SlTestFixtureMEMMOVO {
public:
	SlTestFixtureMEMMOVO()
	{
		assert(BYTES_SIZE % 4 == 0);
		P_Bytes = static_cast<char *>(SAlloc::M(BYTES_SIZE));
		assert(P_Bytes);
		P_Pattern = static_cast<char *>(SAlloc::M(BYTES_SIZE));
		assert(P_Pattern);
		{
			SRng * p_rng = SRng::CreateInstance(SRng::algMT, 0);
			p_rng->Set(7619);
			for(size_t i = 0; i < BYTES_SIZE/sizeof(uint32); i++) {
				((uint32 *)P_Bytes)[i] = ((uint32 *)P_Pattern)[i] = p_rng->Get();
			}
			delete p_rng;
		}
	}
	~SlTestFixtureMEMMOVO()
	{
		SAlloc::F(P_Bytes);
		SAlloc::F(P_Pattern);
	}
	char * P_Bytes;
	char * P_Pattern;
};

SLTEST_FIXTURE(MEMMOVO, SlTestFixtureMEMMOVO)
{
	int    ok = 1;
	char * b = (char *)F.P_Bytes;
	const  size_t bs = BYTES_SIZE;
	int    bm = -1;
	if(pBenchmark == 0)
		bm = 0;
	else if(sstreqi_ascii(pBenchmark, "memmovo"))
		bm = 1;
	else if(sstreqi_ascii(pBenchmark, "memmove"))
		bm = 2;
	else if(sstreqi_ascii(pBenchmark, "A_memmove"))
		bm = 3;
	else if(sstreqi_ascii(pBenchmark, "char[0]=0")) // @v11.4.11
		bm = 4;
	else if(sstreqi_ascii(pBenchmark, "PTR32(char)[0]=0")) // @v11.4.11
		bm = 5;
	else if(sstreqi_ascii(pBenchmark, "PTR64(char)[0]=0")) // @v11.4.11
		bm = 6;
	else
		SetInfo("invalid benchmark");
	if(bm >= 0) {
		if(oneof3(bm, 4, 5, 6)) {
			const uint iter_count = 4000000;
			STempBuffer zbuf(iter_count*8);
			LongArray idx_list;
			{
				SRandGenerator & r_rg = SLS.GetTLA().Rg;
				for(uint i = 0; i < iter_count; i++) {
					long idx = static_cast<long>(r_rg.GetUniformInt(iter_count-8));
					assert(idx >= 0 && idx < (iter_count-8));
					idx_list.add(idx);
				}
			}
			if(bm == 4) {
				assert(idx_list.getCount() == iter_count);
				for(uint i = 0; i < iter_count; i++) {
					const long idx = idx_list.get(i);
					static_cast<char *>(zbuf.vptr())[idx] = 0;
				}
			}
			else if(bm == 5) {
				assert(idx_list.getCount() == iter_count);
				for(uint i = 0; i < iter_count; i++) {
					const long idx = idx_list.get(i);
					PTR32(zbuf.vptr())[idx] = 0;
				}
			}
			else if(bm == 6) {
				assert(idx_list.getCount() == iter_count);
				for(uint i = 0; i < iter_count; i++) {
					const long idx = idx_list.get(i);
					PTR64(zbuf.vptr())[idx] = 0;
				}
			}
		}
		else {
			size_t s;
			for(s = 1; s <= bs/4; s++) {
				const size_t start = bs/4;
				const size_t zone  = bs/2;
				for(size_t offs = start; offs < (start+zone-s); offs++) {
					const size_t src  = offs;
					const size_t dest = bs-offs-s;
					assert(src >= start && src < start+zone);
					assert(dest >= start && dest < start+zone);
					if(bm == 0) {
						//
						// Тестируем A_memmove
						// Она должна скопировать блок длиной s из одной части F.P_Bytes в другую не задев
						// сопредельные участки памяти. Результат копирования сравниваем с F.P_Pattern
						//
						//memmovo(F.P_Bytes+dest, F.P_Bytes+src, s);
						A_memmove(F.P_Bytes+dest, F.P_Bytes+src, s);
						THROW(SLCHECK_Z(memcmp(F.P_Bytes+dest, F.P_Pattern+src, s)));
						THROW(SLCHECK_Z(memcmp(F.P_Bytes, F.P_Pattern, dest)));
						THROW(SLCHECK_Z(memcmp(F.P_Bytes+dest+s, F.P_Pattern+dest+s, BYTES_SIZE-dest-s)));
						//
						// Стандартная процедура копирования для восстановления эквивалентности P_Bytes и P_Pattern
						// Закладываемся на то, что memmove работает правильно.
						// В случае бенчмарка этот вызов "разбавляет" кэш за счет обращения к отличному от F.P_Bytes
						// блоку памяти.
						//
#undef memmove
						memmove(F.P_Pattern+dest, F.P_Pattern+src, s);
					}
					else if(bm == 1) {
						// @v11.6.5 memmovo(F.P_Bytes+dest, F.P_Bytes+src, s);
						// @v11.6.5 memmovo(F.P_Pattern+dest, F.P_Pattern+src, s);
					}
					else if(bm == 2) {
						memmove(F.P_Bytes+dest, F.P_Bytes+src, s);
						memmove(F.P_Pattern+dest, F.P_Pattern+src, s);
					}
					else if(bm == 3) {
						A_memmove(F.P_Bytes+dest, F.P_Bytes+src, s);
						A_memmove(F.P_Pattern+dest, F.P_Pattern+src, s);
					}
				}
			}
			//
			// Тестирование A_memset и ismemzero
			//
			if(bm == 0) {
				for(s = 1; s <= bs/4; s++) {
					const size_t start = bs/4;
					const size_t zone  = bs/2;
					for(size_t offs = start; offs < (start+zone-s); offs++) {
						const size_t src  = offs;
						const size_t dest = bs-offs-s;
						assert(src >= start && src < start+zone);
						assert(dest >= start && dest < start+zone);
						//
						// Тестируем A_memset
						// Она должна обнулить заданный участок памяти не задев сопредельные участки.
						//
						A_memset(F.P_Bytes+dest, 0, s);
						THROW(SLCHECK_NZ(ismemzero(F.P_Bytes+dest, s)));
						THROW(SLCHECK_Z(memcmp(F.P_Bytes, F.P_Pattern, dest)));
						THROW(SLCHECK_Z(memcmp(F.P_Bytes+dest+s, F.P_Pattern+dest+s, BYTES_SIZE-dest-s)));
						//
						// Возвращаем назад содержимое F.P_Bytes[dest..s-1]
						//
						memmove(F.P_Bytes+dest, F.P_Pattern+dest, s);
					}
				}
			}
		}
	}
	CATCHZOK
	return ok;
}
//
//
//
void * AVX_memset(void * dest, int val, size_t numbytes);

static bool Test_memset_fill_or_check(bool check, void * ptr, uint8 byte, size_t size, size_t leftSentinelSize, size_t rightSentinelSize, uint8 sentinelByte)
{
	size_t offs = 0;
	bool   check_result = true;
	assert(ptr != 0);
	assert(size > 0);
	assert(byte != sentinelByte);
	assert(leftSentinelSize <= 16 && rightSentinelSize <= 16);
	if(ptr && size && byte != sentinelByte) {
		if(leftSentinelSize) {
			if(check) {
				for(size_t i = 0; i < leftSentinelSize; i++) {
					if(PTR8(ptr)[i+offs] != sentinelByte)
						check_result = false;
					assert(check_result);
				}
			}
			else {
				for(size_t i = 0; i < leftSentinelSize; i++)
					PTR8(ptr)[i+offs] = sentinelByte;
			}
			offs += leftSentinelSize;
		}
		if(check_result) {
			for(size_t i = 0; i < size; i++) {
				if(check) {
					if(PTR8(ptr)[i+offs] != byte)
						check_result = false;
				}
				else {
					PTR8(ptr)[i+offs] = byte;
				}
			}
			assert(check_result);
			offs += size;
			if(check_result && rightSentinelSize) {
				if(check) {
					for(size_t i = 0; i < rightSentinelSize; i++) {
						if(PTR8(ptr)[i+offs] != sentinelByte)
							check_result = false;
						assert(check_result);
					}
				}
				else {
					for(size_t i = 0; i < rightSentinelSize; i++)
						PTR8(ptr)[i+offs] = sentinelByte;
				}
				offs += rightSentinelSize;
			}
		}
	}
	else
		check_result = false;
	assert(offs == (leftSentinelSize+size+rightSentinelSize));
	return check_result;
}

static bool Test_memset_helper(void * (*func)(void *, int, size_t), uint8 * pBuffer, size_t bufferSize)
{
	bool ok = true;
	const uint8 sentinel_byte = 0x0B;
	const uint8 pre_fill_byte = 0xff;
	const uint8 memset_byte = 0x01;
	const size_t left_sentinel_size = 5;
	const size_t right_sentinel_size = 5;
	assert(pre_fill_byte != memset_byte);
	for(size_t offs = 0; offs <= 128 && (offs + left_sentinel_size + right_sentinel_size) < bufferSize; offs++) {
		for(size_t test_size = 1; ((offs + left_sentinel_size + test_size + right_sentinel_size) <= bufferSize); test_size++) {
			int r1 = Test_memset_fill_or_check(false, pBuffer+offs, pre_fill_byte, test_size, left_sentinel_size, right_sentinel_size, sentinel_byte);
			assert(r1);
			THROW(r1);
			void * p_memset_result = func(pBuffer+offs+left_sentinel_size, memset_byte, test_size);
			assert(p_memset_result == pBuffer+offs+left_sentinel_size);
			THROW(p_memset_result == pBuffer+offs+left_sentinel_size);
			int r2 = Test_memset_fill_or_check(true, pBuffer+offs, memset_byte, test_size, left_sentinel_size, right_sentinel_size, sentinel_byte);
			assert(r2);
			THROW(r2);
		}
	}
	CATCHZOK
	return ok;
}

static void Profile_memset_helper(void * (*func)(void *, int, size_t), uint8 * pBuffer, size_t bufferSize)
{
	const uint8 memset_byte = 0xff;
	volatile void * p_memset_result = 0;
	for(size_t offs = 0; offs <= 64 && offs < bufferSize; offs++) {
		for(size_t test_size = 1; ((offs + test_size) <= bufferSize); test_size++) {
			p_memset_result = func(pBuffer+offs, memset_byte, test_size);
		}
	}
}

#undef memset

SLTEST_R(memset)
{
	//A_memset;AVR_memset;memset
	int    bm = -1;
	if(pBenchmark == 0) 
		bm = 0;
	else if(sstreqi_ascii(pBenchmark, "memset"))
		bm = 1;
	else if(sstreqi_ascii(pBenchmark, "A_memset"))
		bm = 2;
	else if(sstreqi_ascii(pBenchmark, "AVR_memset"))      
		bm = 3;
	if(bm == 0) {
		const size_t buffer_size = SKILOBYTE(32);
		STempBuffer buffer(buffer_size);
		SLCHECK_NZ(Test_memset_helper(memset, static_cast<uint8 *>(buffer.vptr()), buffer_size));
		SLCHECK_NZ(Test_memset_helper(A_memset, static_cast<uint8 *>(buffer.vptr()), buffer_size));
		SLCHECK_NZ(Test_memset_helper(AVX_memset, static_cast<uint8 *>(buffer.vptr()), buffer_size));
	}
	else {
		const size_t buffer_size = SKILOBYTE(64);
		STempBuffer buffer(buffer_size);
		if(bm == 1) {
			Profile_memset_helper(memset, static_cast<uint8 *>(buffer.vptr()), buffer_size);
		}
		if(bm == 2) {
			Profile_memset_helper(A_memset, static_cast<uint8 *>(buffer.vptr()), buffer_size);
		}
		if(bm == 3) {
			Profile_memset_helper(AVX_memset, static_cast<uint8 *>(buffer.vptr()), buffer_size);
		}
	}
	return CurrentStatus;
}
#endif // } SLTEST_RUNNING
//
// } @TEST
//
