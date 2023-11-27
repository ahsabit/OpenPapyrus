// PATH.CPP
// Copyright (c) A.Sobolev 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2013, 2015, 2016, 2017, 2019, 2020, 2021, 2022, 2023
// @codepage UTF-8
// @Kernel
//
#include <pp.h>
#pragma hdrstop
//
// PPPath
//
struct PathItem { // Variable length struct
	PathItem(PPID pathID, short flags, const char * str);
	void * operator new(size_t sz, const char * str = 0);
	void   operator delete(void *, const char *);
	SString & FASTCALL GetPath(SString & rBuf) const { return (rBuf = Path()); }
	const  char * Path() const { return (Size > sizeof(PathItem)) ? reinterpret_cast<const char *>(this + 1) : 0; }
	PPID   ID;
	int16  Flags;
	uint16 Size;
	// ... zstring
};

struct PathData {          // @persistent @store(PropertyTbl)
	PPID   SecurObj;       // SecurType
	PPID   SecurID;        // SecurID
	PPID   Prop;
	char   Reserve1[64];
	int16  Reserve2;
	uint16 Flags;
	uint32 TailSize;
	// ... PathItem[]
};

PathItem::PathItem(PPID pathID, short flags, const char * str) : ID(pathID), Flags(flags)
{
	if(!isempty(str)) {
		const size_t len = sstrlen(str) + 1;
		Size  = static_cast<uint16>(sizeof(PathItem) + len);
		memcpy(this + 1, str, len);
	}
	else
		Size = sizeof(PathItem);
}

void * PathItem::operator new(size_t sz, const char * pStr)
{
	const size_t len = isempty(pStr) ? 0 : (sstrlen(pStr) + 1);
	return ::new char[sz + len];
}

void PathItem::operator delete(void * p, const char *)
{
	::delete(p);
}

/*int PathItem::GetPath(char * pBuf, size_t bufLen) const
{
	strnzcpy(pBuf, Path(), bufLen);
	return 1;
}*/
//
//
//
PPPaths::PPPaths() : P(0)
{
}

PPPaths::PPPaths(const PPPaths & rS)
{
	Resize(rS.Size());
	if(P)
		memmove(P, rS.P, rS.Size());
}

PPPaths::~PPPaths()
{
	ZFREE(P);
}

PPPaths & PPPaths::Z()
{
	ZFREE(P);
	return *this;
}

bool PPPaths::IsEmpty() const
{
	return (P == 0);
}

bool FASTCALL PPPaths::IsEq(const PPPaths & rS) const
{
	bool   eq = true;
	if(P) {
		if(rS.P) {
			SString temp_buf1;
			SString temp_buf2;
			{
				for(uint s = 0; eq && s < P->TailSize;) {
					const PathItem * p = reinterpret_cast<const PathItem *>(PTR8(P + 1) + s);
					bool found = false;
					for(uint s2 = 0; !found && s2 < rS.P->TailSize;) {
						const PathItem * p2 = reinterpret_cast<const PathItem *>(PTR8(rS.P + 1) + s2);
						if(p2->ID == p->ID) {
							found = true;
							SFsPath::NormalizePath(p->Path(), SFsPath::npfCompensateDotDot|SFsPath::npfOEM, temp_buf1);
							SFsPath::NormalizePath(p2->Path(), SFsPath::npfCompensateDotDot|SFsPath::npfOEM, temp_buf2);
							if(temp_buf1 != temp_buf2)
								eq = false;
						}
						s2 += p2->Size; // shifting of iterator
					}
					s += p->Size; // shifting of iterator
					if(!found)
						eq = false;
				}
			}
			if(eq) {
				// В предыдущем цикле мы не проверяли существование идентификаторов в rS, которых нет в this
				// здесь исправим это упущение.
				for(uint s2 = 0; eq && s2 < rS.P->TailSize;) {
					const PathItem * p2 = reinterpret_cast<const PathItem *>(PTR8(rS.P + 1) + s2);
					bool found = false;
					for(uint s = 0; s < P->TailSize;) {
						const PathItem * p = reinterpret_cast<const PathItem *>(PTR8(P + 1) + s);
						if(p2->ID == p->ID)
							found = true;
						s += p->Size; // shifting of iterator
					}
					s2 += p2->Size; // shifting of iterator
					if(!found)
						eq = false;
				}
			}
		}
		else
			eq = false;
	}
	else
		eq = !rS.P;
	return eq;
}

PPPaths & FASTCALL PPPaths::operator = (const PPPaths & rS)
{
	Resize(rS.Size());
	if(P)
		memmove(P, rS.P, rS.Size());
	return *this;
}

size_t PPPaths::Size() const
{
	return P ? (sizeof(PathData) + P->TailSize) : 0;
}

int PPPaths::Resize(size_t sz)
{
	int    ok = 1;
	if(sz == 0)
		ZFREE(P);
	else {
		const size_t prev_size = Size();
		sz = (sz < sizeof(PathData)) ? sizeof(PathData) : sz;
		P = static_cast<PathData *>(SAlloc::R(P, sz));
		if(P) {
			if(sz > prev_size)
				memzero(PTR8(P) + prev_size, sz - prev_size);
			P->TailSize = sz - sizeof(PathData);
		}
		else
			ok = PPSetErrorNoMem();
	}
	return ok;
}

int PPPaths::GetPath(PPID pathID, short * pFlags, SString & rBuf) const
{
	rBuf.Z();
	if(P) {
		for(uint s = 0; s < P->TailSize;) {
			const PathItem * p = reinterpret_cast<const PathItem *>(PTR8C(P + 1) + s);
			if(p->ID == pathID) {
				ASSIGN_PTR(pFlags, p->Flags);
				return p->GetPath(rBuf).NotEmpty() ? 1 : -1;
			}
			s += p->Size;
		}
	}
	ASSIGN_PTR(pFlags, PATHF_EMPTY);
	return -1;
}

int PPPaths::SetPath(PPID pathID, const char * pBuf, short flags, int replace)
{
	int    ok = 1;
	PathItem * pi = 0;
	THROW(P || Resize(sizeof(PathData)));
	{
		uint8 * cp = PTR8(P + 1);
		size_t hs = sizeof(PathData);
		size_t ts = (size_t)P->TailSize;
		size_t s  = 0;
		bool   found = false;
		THROW(pi = new(pBuf) PathItem(pathID, flags, pBuf));
		while(s < ts && !found) {
			PathItem * p = reinterpret_cast<PathItem *>(cp + s);
			const size_t os = p->Size;
			if(p->ID == 0 || os == 0) {
				//
				// Такая ситуация встречаться не должна, но на всякий случай обработать ее стоит.
				// В этом случае будем считать, что достигли конца структуры и не нашли заданный путь.
				//
				P->TailSize = s;
				ts = (size_t)P->TailSize;
				break;
			}
			else if(p->ID == pathID) {
				if(isempty(pBuf)) {
					//
					// Remove item and resize buffer
					//
					memmove(cp + s, cp + s + os, ts - s - os);
					THROW(Resize(hs + ts - os));
				}
				else if(replace) {
					if(pi->Size == os)
						//
						// Simply copy new data
						//
						memmove(p, pi, pi->Size);
					else {
						//
						// Resize buffer and copy new data
						//
						if(pi->Size > os) {
							THROW(Resize(hs + ts + pi->Size - os));
							cp = PTR8(P + 1);
						}
						memmove(cp + s + pi->Size, cp + s + os, ts - s - os);
						memmove(cp + s, pi, pi->Size);
						if(pi->Size < os) {
							THROW(Resize(hs + ts + pi->Size - os));
							cp = PTR8(P + 1);
						}
					}
				}
				found = true;
			}
			else
				s += os;
		}
		if(!found && !isempty(pBuf)) {
			THROW(Resize(hs + ts + pi->Size));
			cp = PTR8(P + 1);
			memmove(cp + s, pi, pi->Size);
		}
	}
	CATCHZOK
	PathItem::operator delete(pi, 0);
	return ok;
}

int PPPaths::Get(PPID obj, PPID id, PPID pathID, SString & rBuf)
{
	int    ok = 1;
	SFsPath ps;
	if(obj && id)
		THROW(Get(obj, id));
   	GetPath(pathID, 0, rBuf);
	rBuf.Strip();
	if(pathID == PPPATH_DRIVE) {
		if(rBuf.IsEmpty()) {
			// @v5.5.9 rBuf.CatChar('A'+getdisk()).Colon();
			; // @v5.5.9 Видимо, не следует подставлять текущий диск, ибо возможно указание относительных каталогов
		}
		else if(rBuf.Len() == 1)
			rBuf.Colon();
	}
	else {
		ps.Split(rBuf.SetLastSlash());
		if(!(ps.Flags & SFsPath::fDrv)) {
			//
			// Обработка неполного каталога
			//
			if(pathID == PPPATH_ROOT) {
				//
				// Если извлекаем корневой каталог, то остается добавить только драйвер
				//
				THROW(Get(0, 0, PPPATH_DRIVE, ps.Drv)); // @recursion
				if(ps.Drv.NotEmpty()) {
					ps.Flags |= SFsPath::fDrv;
					if(!oneof2(ps.Dir.C(0), '/', '\\'))
						ps.Dir.PadLeft(1, '\\');
				}
				ps.Merge(rBuf);
			}
			else {
				//
				// Если извлекаем что-то кроме корня или драйвера, то
				// два случая:
				//
				if(ps.Flags & SFsPath::fDir && oneof2(ps.Dir.C(0), '/', '\\')) {
					//
					// 1. Если путь rBuf содержит каталог и начинается с '\', то
					//    добавляем к имени спереди только драйвер
					//
					THROW(Get(0, 0, PPPATH_DRIVE, ps.Dir)); // @recursion
					ps.Dir.RmvLastSlash();
				}
				else {
					//
					// 2. В противном случае добавляем к имени спереди весь корневой каталог
					//
					THROW(Get(0, 0, PPPATH_ROOT, ps.Dir)); // @recursion
				}
				rBuf = ps.Dir.Cat(rBuf);
			}
		}
		rBuf.SetLastSlash();
	}
	CATCH
		ok = 0;
		rBuf.Z();
	ENDCATCH
	return ok;
}

int PPPaths::Get(PPID securType, PPID securID)
{
	int    ok = 1, r;
	Reference * p_ref = PPRef;
	size_t sz = 2048;
	uint   s;
	//
	// Каталоги TEMP, LOG, PACK могут быть устанавлены до считывания из базы данных.
	// В этом случае не допускаем переписывания уже установленных значений теми, что указаны
	// в конфигурации.
	//
	SString temp_path;
	SString log_path;
	SString pack_path;
	SString spii_path;
	SString sartredb_path;
	SString reportdata_path;
	SString workspace_path;
	GetPath(PPPATH_TEMP, 0, temp_path);
	GetPath(PPPATH_LOG, 0, log_path);
	GetPath(PPPATH_PACK, 0, pack_path);
	GetPath(PPPATH_SPII, 0, spii_path);
	GetPath(PPPATH_SARTREDB, 0, sartredb_path);
	GetPath(PPPATH_REPORTDATA, 0, reportdata_path);
	GetPath(PPPATH_WORKSPACE, 0, workspace_path); // @v10.6.1
	PathItem * p = 0;
	THROW(Resize(sz));
	THROW(r = p_ref->GetConfig(securType, securID, PPPRP_PATHS, P, sz));
	if(r > 0) {
		if(Size() > sz) {
			THROW(Resize(sz = Size()));
			THROW(p_ref->GetConfig(securType, securID, PPPRP_PATHS, P, sz) > 0);
		}
		if(P->SecurObj < securType) {
			P->Flags |= PATHF_INHERITED;
			for(s = 0; s < P->TailSize; s += p->Size) {
				p = reinterpret_cast<PathItem *>(PTR8(P + 1) + s);
				p->Flags |= PATHF_INHERITED;
			}
		}
		if(oneof2(P->SecurObj, PPOBJ_USRGRP, PPOBJ_USR)) {
			//
			// Необходимо унаследовать от более высоких уровней иерархии
			// пути, которые не определены на заданном уровне (Рекурсия)
			//
			PPPaths temp;
			PPID   prev_type = (securType == PPOBJ_USRGRP) ? PPOBJ_CONFIG : PPOBJ_USRGRP;
			THROW(p_ref->GetItem(securType, securID) > 0);
			THROW(temp.Get(prev_type, reinterpret_cast<const PPSecur *>(&p_ref->data)->ParentID)); // @recursion
			if(temp.P) {
				uint prev_s = UINT_MAX;
				for(s = 0; s < temp.P->TailSize; s += p->Size) {
					//
					// Защита от бесконечного цикла {
					//
					if(s == prev_s)
						break;
					else
						prev_s = s;
					// }
					p = reinterpret_cast<PathItem *>(PTR8(temp.P + 1) + s);
					if(p->Path())
						THROW(SetPath(p->ID, p->Path(), p->Flags | PATHF_INHERITED, 0));
				}
			}
		}
		THROW(Resize(Size()));
	}
	else
		Z();
	//
	// Восстановление значений сохраненных ранее TEMP и LOG каталогов
	//
	if(temp_path.NotEmptyS())
		SetPath(PPPATH_TEMP, temp_path, 0, 1);
	if(log_path.NotEmptyS())
		SetPath(PPPATH_LOG, log_path, 0, 1);
	if(pack_path.NotEmptyS())
		SetPath(PPPATH_PACK, pack_path, 0, 1);
	if(spii_path.NotEmptyS())
		SetPath(PPPATH_SPII, spii_path, 0, 1);
	if(sartredb_path.NotEmptyS())
		SetPath(PPPATH_SARTREDB, sartredb_path, 0, 1);
	if(reportdata_path.NotEmptyS())
		SetPath(PPPATH_REPORTDATA, reportdata_path, 0, 1);
	// @v10.6.1 {
	if(workspace_path.NotEmptyS())
		SetPath(PPPATH_WORKSPACE, workspace_path, 0, 1);
	// } @v10.6.1
	CATCHZOK
	return ok;
}

int PPPaths::Put(PPID securType, PPID securID)
{
	int    ok = 1;
	if(!IsEmpty()) {
		P->Flags   &= ~PATHF_INHERITED;
		P->SecurObj = securType;
		P->SecurID  = securID;
		P->Prop     = PPPRP_PATHS;
		memzero(P->Reserve1, sizeof(P->Reserve2));
		P->Reserve2 = 0;
		for(uint s = 0; s < P->TailSize;) {
			PathItem * p = reinterpret_cast<PathItem *>(PTR8(P + 1) + s);
			if(!p->Path() || p->Flags & PATHF_INHERITED) {
				THROW(SetPath(p->ID, 0, 0, 1));
			}
			else {
				p->Flags &= ~PATHF_INHERITED;
				s += p->Size;
			}
		}
		THROW(PPRef->SetConfig(securType, securID, PPPRP_PATHS, P, Size()));
	}
	CATCHZOK
	return ok;
}

int PPPaths::Remove(PPID securType, PPID securID) { return PPRef->RemoveProperty(securType, securID, PPPRP_PATHS, 0); }
int FASTCALL PPGetPath(PPID pathID, SString & rBuf) { return DS.GetPath(pathID, rBuf); }

void PPPaths::DumpToStr(SString & rBuf) const
{
	rBuf.Z();
	if(P) {
		for(uint s = 0; s < P->TailSize;) {
			const PathItem * p = reinterpret_cast<const PathItem *>(PTR8(P + 1) + s);
			if(rBuf.NotEmpty())
				rBuf.Semicol();
			switch(p->ID) {
				case PPPATH_DRIVE: rBuf.Cat("DRIVE"); break;					
				case PPPATH_ROOT: rBuf.Cat("ROOT"); break;
				case PPPATH_BIN: rBuf.Cat("BIN"); break;
				case PPPATH_DAT: rBuf.Cat("DAT"); break;
				case PPPATH_SYS: rBuf.Cat("SYS"); break;
				case PPPATH_IN: rBuf.Cat("IN"); break;
				case PPPATH_OUT: rBuf.Cat("OUT"); break;
				case PPPATH_LOG: rBuf.Cat("LOG"); break;
				case PPPATH_WORKSPACE: rBuf.Cat("WORKSPACE"); break;
				case PPPATH_TEMP: rBuf.Cat("TEMP"); break;
				case PPPATH_REPORTDATA: rBuf.Cat("REPORTDATA"); break;
				case PPPATH_TESTROOT: rBuf.Cat("TESTROOT"); break;
				case PPPATH_SARTREDB: rBuf.Cat("SARTREDB"); break;
				case PPPATH_SPII: rBuf.Cat("SPII"); break;
				case PPPATH_DD: rBuf.Cat("DD"); break;
				case PPPATH_LOCAL: rBuf.Cat("LOCAL"); break;
				case PPPATH_PACK: rBuf.Cat("PACK"); break;
				default: rBuf.CatChar('#').Cat(p->ID); break;
			}
			rBuf.Eq().Cat(p->Path());
			s += p->Size;
		}
	}
}

int STDCALL PPGetFilePath(PPID pathID, const char * pFileName, SString & rBuf)
{
	rBuf.Z();
	if(PPGetPath(pathID, rBuf)) {
		rBuf.SetLastSlash().Cat(pFileName);
		return 1;
	}
	else
		return 0;
}

SString & STDCALL PPGetFilePathS(PPID pathID, const char * pFileName, SString & rBuf)
{
	PPGetFilePath(pathID, pFileName, rBuf);
	return rBuf;
}

int STDCALL PPGetFilePath(PPID pathID, uint fileNameID, SString & rBuf)
{
	SString & r_temp_buf = SLS.AcquireRvlStr();
	return PPGetFilePath(pathID, PPGetFileName(fileNameID, r_temp_buf), rBuf);
}

SString & STDCALL PPGetFilePathS(PPID pathID, uint fileNameID, SString & rBuf)
{
	SString & r_temp_buf = SLS.AcquireRvlStr();
	PPGetFilePath(pathID, PPGetFileName(fileNameID, r_temp_buf), rBuf);
	return rBuf;
}

SString & FASTCALL PPGetFileName(uint fnameID, SString & rBuf)
{
	PPLoadText(fnameID, rBuf);
	return rBuf;
}

SString & STDCALL PPMakeTempFileName(const char * pPrefix, const char * pExt, long * pStart, SString & rBuf)
{
	SString path;
	PPGetPath(PPPATH_TEMP, path);
	return MakeTempFileName(path.SetLastSlash(), pPrefix, pExt, pStart, rBuf);
}

int PPRemoveFiles(const SFileEntryPool * pFileList, uint * pSuccCount, uint * pErrCount)
{
	int    ok = -1;
	uint   succ_count = 0;
	uint   err_count = 0;
	if(pFileList) {
		SString file_path;
		for(uint i = 0; i < pFileList->GetCount(); i++) {
			if(pFileList->Get(i, 0, &file_path)) {
				if(SFile::Remove(file_path))
					succ_count++;
				else
					err_count++;
			}
		}
		ok = 1;
	}
	ASSIGN_PTR(pSuccCount, succ_count);
	ASSIGN_PTR(pErrCount, err_count);
	return ok;
}

int PPRemoveFilesByExt(const char * pSrc, const char * pExt, uint * pSuccCount, uint * pErrCount)
{
	SFileEntryPool fep;
	SString wildcard;
	wildcard.CatChar('*');
	if(pExt && pExt[0] != '.')
		wildcard.Dot();
	wildcard.Cat(pExt);
	fep.Scan(pSrc, wildcard, 0);
	return PPRemoveFiles(&fep, pSuccCount, pErrCount);
}
