// DBBACKUP.CPP
// Copyright (c) A.Sobolev 1999, 2000, 2001, 2002, 2003, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2014, 2015, 2017, 2018, 2019, 2020, 2021, 2022, 2023
// @codepage UTF-8
//
#include <slib-internal.h>
#pragma hdrstop

int CallbackCompress(long, long, const char *, int);
/*
	Формат файла backup.dat
	Текстовый файл. Данные об одной копии записываются в одну строку.
	Разделители полей - запятая (',').

	copy_set,
	copy_id,
	dest_path,
	dest_subdir,
	date (dd/mm/yyyy),
	time (hh:mm:ss),
	copy_format (0 - raw copy),
	src_size (bytes),
	dest_size (bytes),
	check_sum
*/
//
//
//
BCopyData::BCopyData() : ID(0), BssFactor(0), Flags(0), Dtm(ZERODATETIME), SrcSize(0), DestSize(0), CheckSum(0)
{
}

BCopyData & BCopyData::Z()
{
	ID = 0;
	BssFactor = 0;
	Flags = 0;
	Dtm = ZERODATETIME;
	SrcSize = 0;
	DestSize = 0;
	CheckSum = 0;
	Set.Z();
	CopyPath.Z();
	TempPath.Z();
	SubDir.Z();
	return *this;
}

#define DEFAULT_SPACE_SAFETY_FACTOR 1200

BCopySet::BCopySet(const char * pName) : TSCollection <BCopyData> (), Name(pName)
{
}

static IMPL_CMPFUNC(BCopyData_Dt, i1, i2) { return cmp(static_cast<const BCopyData *>(i1)->Dtm, static_cast<const BCopyData *>(i2)->Dtm); }

static IMPL_CMPFUNC(BCopyData_DtDesc, i1, i2)
{
	int    r = CMPFUNC(BCopyData_Dt, i1, i2);
	return r ? -r : 0;
}

void BCopySet::Sort(BCopySet::Order ord)
{
	if(ord == ordByDate)
		sort(PTR_CMPFUNC(BCopyData_Dt));
	else if(ord == ordByDateDesc)
		sort(PTR_CMPFUNC(BCopyData_DtDesc));
}
//
//
//
const char * BackupInfoFile = "BACKUP.INF";

DBBackup::InfoFile::InfoFile(DbProvider * pDb) : Stream(0)
{
	if(pDb) {
		SString data_path;
		pDb->GetDataPath(data_path);
		MakeFileName(data_path, BackupInfoFile, FileName, sizeof(FileName));
	}
	else
		PTR32(FileName)[0] = 0;
}

DBBackup::InfoFile::~InfoFile()
{
	CloseStream();
}

int DBBackup::InfoFile::MakeFileName(const char * pPath, const char * pFileName, char * pBuf, size_t bufLen)
{
	SString temp;
	if(pPath)
		(temp = pPath).SetLastSlash();
	else {
		SPathStruc ps(FileName);
		ps.Merge(0, SPathStruc::fNam|SPathStruc::fExt, temp);
		temp.SetLastSlash();
	}
	temp.Cat(pFileName).CopyTo(pBuf, bufLen);
	return 1;
}

int DBBackup::InfoFile::OpenStream(int readOnly)
{
	char   mode[16];
	CloseStream();
	if(!fileExists(FileName)) {
		mode[0] = 'w';
		mode[1] = 0;
		Stream = fopen(FileName, mode);
		if(Stream == 0)
			return (DBErrCode = SDBERR_FCRFAULT, 0);
		CloseStream();
	}
	mode[0] = 'r';
	if(readOnly)
		mode[1] = 0;
	else {
		mode[1] = '+';
		mode[2] = 0;
	}
	Stream = fopen(FileName, mode);
	return Stream ? 1 : (DBErrCode = SDBERR_FOPENFAULT, 0);
}

void DBBackup::InfoFile::CloseStream()
{
	SFile::ZClose(&Stream);
}

int DBBackup::InfoFile::ReadSet(BCopySet * set)
{
	int    ok = 1;
	BCopyData bc_data;
	if(OpenStream(1)) {
		while(ok && ReadRecord(Stream, &bc_data) > 0)
			if(set->Name.IsEmpty() || bc_data.Set.CmpNC(set->Name) == 0)
				set->insert(new BCopyData(bc_data));
		CloseStream();
	}
	else
		ok = 0;
	return ok;
}

int DBBackup::InfoFile::ReadItem(long copyID, BCopyData * pData)
{
	int    ok = -1;
	BCopyData bc_data;
	if(OpenStream(1)) {
		while(ok < 0 && ReadRecord(Stream, &bc_data) > 0) {
			if(bc_data.ID == copyID) {
				ASSIGN_PTR(pData, bc_data);
				ok = 1;
				break;
			}
		}
		CloseStream();
	}
	else
		ok = 0;
	return ok;
}

int DBBackup::InfoFile::AddRecord(BCopyData * data)
{
	int    ok = 1;
	long   max_id = 1;
	BCopyData temp_rec;
	if(OpenStream(0)) {
		while(ReadRecord(Stream, &temp_rec) > 0)
			if(temp_rec.ID >= max_id)
				max_id = temp_rec.ID + 1;
		data->ID = max_id;
		if(WriteRecord(Stream, data))
			CloseStream();
		else
			ok = 0;
	}
	else
		ok = 0;
	return ok;
}

int DBBackup::InfoFile::RemoveRecord(const char * pSet, long id)
{
	EXCEPTVAR(DBErrCode);
	int    ok = 1;
	long   i;
	char   temp_name[MAX_PATH];
	char   nam[32];
	FILE * temp_stream = 0;
	BCopyData data;
	//
	// @todo use MakeTempFileName() {
	//
	for(i = 1; i < 100000L; i++) {
		sprintf(nam, "BI%06ld.TMP", i);
		MakeFileName(0, nam, temp_name, sizeof(temp_name));
		if(!fileExists(temp_name))
			break;
	}
	// }
	THROW(OpenStream(1));
	THROW_V(temp_stream = fopen(temp_name, "w"), SDBERR_FCRFAULT);
	while(ReadRecord(Stream, &data) > 0)
		if(data.Set.CmpNC(pSet) != 0 || data.ID != id) {
			THROW(WriteRecord(temp_stream, &data));
		}
	SFile::ZClose(&temp_stream);
	CloseStream();
	THROW_V(SFile::Remove(FileName), SDBERR_SLIB);
	THROW_V(SFile::Rename(temp_name, FileName), SDBERR_SLIB);
	CATCH
		ok = 0;
		SFile::ZClose(&temp_stream);
		CloseStream();
	ENDCATCH
	return ok;
}

int DBBackup::InfoFile::WriteRecord(FILE * stream, const BCopyData * pData)
{
	int    ok = 1;
	int    copy_format = BIN(pData->Flags & BCOPYDF_USECOMPRESS);
	if(stream) {
		/*
		fprintf(stream, "%s,%ld,%s,%s,%s,%s,%d,%lu,%lu,%lu\n",
			data->Set, data->ID, data->CopyPath, data->SubDir, sdt, stm,
			copy_format, data->SrcSize, data->DestSize, data->CheckSum);
		*/
		SString line_buf;
		line_buf.
			Cat(pData->Set).Comma().
			Cat(pData->ID).Comma().
			Cat(pData->CopyPath).Comma().
			Cat(pData->SubDir).Comma().
			Cat(pData->Dtm.d, DATF_DMY|DATF_CENTURY).Comma().
			Cat(pData->Dtm.t, TIMF_HMS).Comma().
			Cat(copy_format).Comma().
			Cat(pData->SrcSize).Comma().
			Cat(pData->DestSize).Comma().
			Cat(pData->CheckSum).CR();
		fputs(line_buf, stream);
	}
	else
		ok = 0;
	return ok;
}

int DBBackup::InfoFile::ReadRecord(FILE * stream, BCopyData * pData)
{
	int    ok = -1;
	char   buf[1024];
	CALLPTRMEMB(pData, Z());
	while(fgets(buf, sizeof(buf), stream)) {
		if(*strip(chomp(buf))) {
			if(pData) {
				uint   pos = 0, i = 0;
				SString str;
				StringSet ss(',', buf);
				while(ss.get(&pos, str)) {
					str.Strip();
					switch(i++) {
						case 0: pData->Set = str; break;
						case 1: pData->ID = str.ToLong(); break;
						case 2: pData->CopyPath = str; break;
						case 3: pData->SubDir = str; break;
						case 4: strtodate(str, DATF_DMY, &pData->Dtm.d); break;
						case 5: strtotime(str, TIMF_HMS, &pData->Dtm.t); break;
						case 6: SETFLAG(pData->Flags, BCOPYDF_USECOMPRESS, str.ToLong()); break; // copy format
						case 7: pData->SrcSize  = _atoi64(str); break;
						case 8: pData->DestSize = _atoi64(str); break;
						case 9: pData->CheckSum = strtoul(str, 0, 10); break;
					}
				}
			}
			ok = 1;
			break;
		}
	}
	return ok;
}
//
//
//
DBBackup::DBBackup() : P_Db(0), InfoF(0), AbortProcessFlag(0), SpaceSafetyFactor(DEFAULT_SPACE_SAFETY_FACTOR),
	TotalCopySize(0), TotalCopyReady(0)
{
}

DBBackup::~DBBackup()
{
}

int DBBackup::CBP_CopyProcess(const char *, const char *, int64, int64, int64, int64)
{
	return SPRGRS_CONTINUE;
}

uint DBBackup::GetSpaceSafetyFactor()
{
	return SpaceSafetyFactor;
}

void DBBackup::SetSpaceSafetyFactor(uint v)
{
	SpaceSafetyFactor = v;
}

int DBBackup::SetDictionary(DbProvider * pDb)
{
	P_Db = pDb;
	ZDELETE(InfoF);
	if(P_Db) {
		InfoF = new DBBackup::InfoFile(P_Db);
		if(InfoF == 0) {
			P_Db = 0;
			return (DBErrCode = SDBERR_NOMEM, 0);
		}
	}
	return 1;
}

int DBBackup::GetCopySet(BCopySet * pSet)
{
	return P_Db ? InfoF->ReadSet(pSet) : (DBErrCode = SDBERR_BU_DICTNOPEN, 0);
}

int DBBackup::GetCopyData(long copyID, BCopyData * pData)
{
	return P_Db ? InfoF->ReadItem(copyID, pData) : (DBErrCode = SDBERR_BU_DICTNOPEN, 0);
}

int DBBackup::CheckAvailableDiskSpace(const char * pPath, int64 size)
{
	int64 total, free_space;
	SFile::GetDiskSpace(pPath, &total, &free_space);
	if(free_space > ((size * SpaceSafetyFactor) / 1000L)) {
		return 1;
	}
	else {
		SString msg_buf;
		msg_buf.Cat(pPath).Space().CatEq("free-space", free_space).Space().CatEq("size-needed", size).Space().CatEq("safety-factor", SpaceSafetyFactor);
		DBS.SetAddedMsgString(msg_buf);
		return (DBErrCode = SDBERR_BU_NOFREESPACE, 0);
	}
}

static void LogMessage(BackupLogFunc fnLog, int recId, const char * pInfo, void * extraPtr)
{
	if(fnLog)
		fnLog(recId, pInfo, extraPtr);
}

int DBBackup::CheckCopy(const BCopyData * pData, const CopyParams & rCP, BackupLogFunc fnLog, void * extraPtr)
{
	EXCEPTVAR(DBErrCode);
	int    ok = 1;
	const  int use_compression = BIN(pData && (pData->Flags & BCOPYDF_USECOMPRESS));
	BTBLID i = 0;
	SString path, spart;
	DbTableStat ts;
	StrAssocArray tbl_list;
	CopyParams cp;
	cp.TotalSize = 0;
	THROW_V(P_Db, SDBERR_BU_DICTNOPEN);
	P_Db->GetListOfTables(0, &tbl_list);
	for(uint j = 0; j < tbl_list.getCount(); j++) {
		const StrAssocArray::Item item = tbl_list.Get(j);
		if(P_Db->GetTableInfo(item.Id, &ts) > 0 && !(ts.Flags & XTF_DICT) && ts.Location.NotEmpty()) {
			TablePartsEnum tpe(0);
			path = ts.Location;
			ulong  sz = 0;
			for(tpe.Init(P_Db->MakeFileName_(ts.TblName, path)); tpe.Next(spart) > 0;) {
				if(fileExists(spart)) {
					SFile::Stat stat;
					if(SFile::GetStat(spart, 0, &stat, 0)) {
						cp.TotalSize += stat.Size;
						THROW_V(cp.SsFiles.add(spart), SDBERR_SLIB);
					}
					else {
						LogMessage(fnLog, BACKUPLOG_ERR_GETFILEPARAM, spart, extraPtr);
						CALLEXCEPT();
					}
				}
			}
		}
	}
	// check total size criteria
	if(!use_compression) {
		double diff = static_cast<double>((rCP.TotalSize - cp.TotalSize) * 100);
		diff = (diff < 0) ? -diff : diff;
		THROW_V(cp.TotalSize && (diff / (double)cp.TotalSize) <= 30, SDBERR_BU_COPYINVALID);
	}
	// check files count criteria
	{
		const uint pcp_ssf_count = rCP.SsFiles.getCount();
		const uint cp_ssf_count = cp.SsFiles.getCount();
		double diff = (double)labs((long)pcp_ssf_count - (long)cp_ssf_count) * 100;
		THROW_V(cp_ssf_count && (diff / (double)cp_ssf_count) <= 30, SDBERR_BU_COPYINVALID);
	}
	CATCHZOK
	return ok;
}

int DBBackup::ReleaseContinuousMode(BackupLogFunc fnLog, void * extraPtr)
{
	EXCEPTVAR(DBErrCode);
	int    ok = 1;
	DbTableStat ts;
	StrAssocArray tbl_list;
	SString path;
	SString spart;
	// THROW_V(Btrieve::RemoveContinuous(copycont_filelist.getBuf()), SDBERR_BTRIEVE);
	SString msg_buf;
	THROW_V(P_Db, SDBERR_BU_DICTNOPEN);
	P_Db->GetListOfTables(0, &tbl_list);
	for(uint j = 0; j < tbl_list.getCount(); j++) {
		const StrAssocArray::Item item = tbl_list.Get(j);
		if(P_Db->GetTableInfo(item.Id, &ts) > 0 && !(ts.Flags & XTF_DICT) && ts.Location.NotEmpty()) {
			TablePartsEnum tpe(0);
			path = ts.Location;
			if(tpe.Init(P_Db->MakeFileName_(ts.TblName, path))) {
				for(int is_first = 1; tpe.Next(spart) > 0; is_first = 0) {
					if(fileExists(spart)) {
						SFile::Stat stat;
						if(SFile::GetStat(spart, 0, &stat, 0)) {
							//cp.TotalSize += stat.Size;
							//THROW_V(cp.SsFiles.add(spart), SDBERR_SLIB);
							if(is_first) {
								DBTable _tbl(item.Txt);
								if(_tbl.IsOpened()) {
									int r2 = Btrieve::RemoveContinuous(_tbl.GetFileName());
									if(r2) {
										(msg_buf = "Remove continuous").CatDiv(':', 2).Cat("OK for file").Space().Cat(_tbl.GetFileName());
										SLS.LogMessage(0, msg_buf, 0);
									}
									else {
										const long db_err = BtrError;
										(msg_buf = "Remove continuous").CatDiv(':', 2).Cat("error removing continuous mode for file").
										Space().CatChar('(').CatEq("dberr", db_err).CatChar(')').Space().Cat(_tbl.GetFileName());
										SLS.LogMessage(0, msg_buf, 0);
									}
								}
								else {
									(msg_buf = "Remove continuous").CatDiv(':', 2).Cat("error opening file").Space().Cat(_tbl.GetFileName());
									SLS.LogMessage(0, msg_buf, 0);
								}
							}
						}
						else {
							LogMessage(fnLog, BACKUPLOG_ERR_GETFILEPARAM, spart, extraPtr);
							CALLEXCEPT();
						}
					}
				}
			}
		}
	}
	CATCHZOK
	return ok;
}

int DBBackup::Backup(BCopyData * pData, BackupLogFunc fnLog, void * extraPtr)
{
	// DbProvider Implement_Open
	EXCEPTVAR(DBErrCode);
	int    ok = 1;
	const  int use_compression    = 0; //BIN(pData->Flags & BCOPYDF_USECOMPRESS); // @v10.8.3 unconditionally 0
	const  int use_copycontinouos = BIN(pData->Flags & BCOPYDF_USECOPYCONT);
	// @v10.9.5 int    do_release_cont = BIN(pData->Flags & BCOPYDF_RELEASECONT);
	const  LDATETIME cur_dtm = getcurdatetime_();
	DbTableStat ts;
	StrAssocArray tbl_list;
	SString path;
	SString spart;
	SStrCollection file_list2;
	StringSet copycont_filelist(",");
	CopyParams cp;
	cp.TotalSize = 0;
	THROW_V(P_Db, SDBERR_BU_DICTNOPEN);
	P_Db->GetListOfTables(0, &tbl_list);
	{
		for(uint j = 0; j < tbl_list.getCount(); j++) {
			const StrAssocArray::Item item = tbl_list.Get(j);
			if(P_Db->GetTableInfo(item.Id, &ts) > 0 && !(ts.Flags & XTF_DICT) && ts.Location.NotEmpty()) {
				TablePartsEnum tpe(0);
				path = ts.Location;
				if(tpe.Init(P_Db->MakeFileName_(ts.TblName, path))) {
					for(int is_first = 1; tpe.Next(spart) > 0; is_first = 0) {
						if(fileExists(spart)) {
							SFile::Stat stat;
							if(SFile::GetStat(spart, 0, &stat, 0)) {
								cp.TotalSize += stat.Size;
								THROW_V(cp.SsFiles.add(spart), SDBERR_SLIB);
								if(is_first) {
									copycont_filelist.add(spart);
									file_list2.insert(newStr(spart));
								}
							}
							else {
								LogMessage(fnLog, BACKUPLOG_ERR_GETFILEPARAM, spart, extraPtr);
								CALLEXCEPT();
							}
						}
					}
				}
			}
		}
	}
#if 0 // @v10.9.5 (block is moved to DBBackup::ReleaseContinuousMode) {
	if(do_release_cont) {
		// THROW_V(Btrieve::RemoveContinuous(copycont_filelist.getBuf()), SDBERR_BTRIEVE);
		SString msg_buf;
		for(uint j = 0; j < tbl_list.getCount(); j++) {
			const StrAssocArray::Item item = tbl_list.Get(j);
			if(P_Db->GetTableInfo(item.Id, &ts) > 0 && !(ts.Flags & XTF_DICT) && ts.Location.NotEmpty()) {
				TablePartsEnum tpe(0);
				path = ts.Location;
				if(tpe.Init(P_Db->MakeFileName_(ts.TblName, path))) {
					for(int is_first = 1; tpe.Next(spart) > 0; is_first = 0) {
						if(fileExists(spart)) {
							SFile::Stat stat;
							if(SFile::GetStat(spart, 0, &stat, 0)) {
								cp.TotalSize += stat.Size;
								THROW_V(cp.SsFiles.add(spart), SDBERR_SLIB);
								if(is_first) {
									DBTable _tbl(item.Txt);
									if(_tbl.IsOpened()) {
										int r2 = Btrieve::RemoveContinuous(_tbl.GetFileName());
										if(r2) {
											(msg_buf = "Remove continuous").CatDiv(':', 2).Cat("OK for file").Space().Cat(_tbl.GetFileName());
											SLS.LogMessage(0, msg_buf, 0);
										}
										else {
											const long db_err = BtrError;
											(msg_buf = "Remove continuous").CatDiv(':', 2).Cat("error removing continuous mode for file").
											Space().CatChar('(').CatEq("dberr", db_err).CatChar(')').Space().Cat(_tbl.GetFileName());
											SLS.LogMessage(0, msg_buf, 0);
										}
									}
									else {
										(msg_buf = "Remove continuous").CatDiv(':', 2).Cat("error opening file").Space().Cat(_tbl.GetFileName());
										SLS.LogMessage(0, msg_buf, 0);
									}
								}
							}
							else {
								LogMessage(fnLog, BACKUPLOG_ERR_GETFILEPARAM, spart, extraPtr);
								CALLEXCEPT();
							}
						}
					}
				}
			}
		}
	}
	else 
#endif // } @v10.9.5 (block is moved to DBBackup::ReleaseContinuousMode)
	{
		THROW(MakeCopyPath(pData, cp.Path));
		cp.TempPath = pData->TempPath;
		if(pData->BssFactor > 0)
			SetSpaceSafetyFactor(static_cast<uint>(pData->BssFactor));
		if(use_compression || CheckAvailableDiskSpace(cp.Path, cp.TotalSize)) {
			SString data_path;
			pData->SrcSize = cp.TotalSize;
			if(use_copycontinouos)
				THROW_V(Btrieve::AddContinuous(copycont_filelist.getBuf()), SDBERR_BTRIEVE); // @!
			ok = DoCopy(&cp, fnLog, extraPtr);
			if(use_copycontinouos)
				THROW_V(Btrieve::RemoveContinuous(copycont_filelist.getBuf()), SDBERR_BTRIEVE);
			THROW(ok);
			P_Db->GetDataPath(data_path);
			THROW(CopyLinkFiles(data_path, cp.Path, fnLog, extraPtr));
			pData->DestSize = cp.TotalSize;
			pData->Dtm = cur_dtm;
			THROW(InfoF->AddRecord(pData));
		}
		else {
			SFile::Remove(cp.Path.RmvLastSlash());
			CALLEXCEPT();
		}
	}
	CATCHZOK
	return ok;
}

int DBBackup::GetCopyParams(const BCopyData * data, DBBackup::CopyParams * params)
{
	EXCEPTVAR(DBErrCode);
	int    ok = 1;
	SString copy_path, wildcard, file_name;
	SDirEntry dir_entry;
	SDirec * direc = 0;
	params->TotalSize = 0;
	params->CheckSum  = 0;
	THROW_V(P_Db, SDBERR_BU_DICTNOPEN);
	(copy_path = data->CopyPath).Strip().SetLastSlash().Cat(data->SubDir).Strip();
	if(::access(copy_path.RmvLastSlash(), 0) != 0) {
		DBS.SetAddedMsgString(copy_path);
		CALLEXCEPTV(SDBERR_BU_NOCOPYPATH);
	}
	(wildcard = copy_path).SetLastSlash().Cat("*.*");
	direc = new SDirec(wildcard);
	for(; direc->Next(&dir_entry) > 0;)
		if(!(dir_entry.Attr & 0x10)) {
			dir_entry.GetNameA(copy_path, file_name);
			SFile::Stat stat;
			if(SFile::GetStat(file_name, 0, &stat, 0)) {
				params->TotalSize += stat.Size;
				THROW_V(params->SsFiles.add(file_name), SDBERR_SLIB);
			}
		}
	params->Path = copy_path;
	if(data->BssFactor > 0)
		SetSpaceSafetyFactor((uint)data->BssFactor);
	CATCHZOK
	ZDELETE(direc);
	return ok;
}

int DBBackup::RemoveDatabase(int safe)
{
	EXCEPTVAR(DBErrCode);
	int    ok = 1;
	DbTableStat ts;
	StrAssocArray tbl_list;
	SString path, spart;
	THROW_V(P_Db, SDBERR_BU_DICTNOPEN);
	P_Db->GetListOfTables(0, &tbl_list);
	for(uint j = 0; j < tbl_list.getCount(); j++) {
		const StrAssocArray::Item item = tbl_list.Get(j);
		if(P_Db->GetTableInfo(item.Id, &ts) > 0 && !(ts.Flags & XTF_DICT) && ts.Location.NotEmpty()) {
			int    first = 0;
			TablePartsEnum tpe(0);
			path = ts.Location;
			for(tpe.Init(P_Db->MakeFileName_(ts.TblName, path)); tpe.Next(spart, &first) > 0;) {
				if(fileExists(spart)) {
					if(safe) {
						SString safe_file;
						tpe.ReplaceExt(first, spart, safe_file);
						if(fileExists(safe_file))
							SFile::Remove(safe_file);
						THROW_V(SFile::Rename(spart, safe_file), SDBERR_SLIB);
					}
					else {
						THROW_V(SFile::Remove(spart), SDBERR_SLIB);
					}
				}
			}
		}
	}
	CATCHZOK
	return ok;
}

int DBBackup::RestoreRemovedDB(int restoreFiles)
{
	EXCEPTVAR(DBErrCode);
	int    ok = 1;
	SString path, spart, spart_saved;
	DbTableStat ts;
	StrAssocArray tbl_list;
	THROW_V(P_Db, SDBERR_BU_DICTNOPEN);
	P_Db->GetListOfTables(0, &tbl_list);
	for(uint j = 0; j < tbl_list.getCount(); j++) {
		const StrAssocArray::Item item = tbl_list.Get(j);
		if(P_Db->GetTableInfo(item.Id, &ts) > 0 && !(ts.Flags & XTF_DICT) && ts.Location.NotEmpty()) {
			path = ts.Location;
			int    first = 0;
			TablePartsEnum tpe(0);
			SString saved_file = P_Db->MakeFileName_(ts.TblName, path);
			SPathStruc::ReplaceExt(saved_file, "___", 1);
			for(tpe.Init(saved_file); tpe.Next(spart_saved, &first) > 0;) {
				tpe.ReplaceExt(first, spart_saved, spart);
				if(restoreFiles) {
					if(fileExists(spart_saved)) {
						if(fileExists(spart))
							SFile::Remove(spart);
						THROW_V(SFile::Rename(spart_saved, spart), SDBERR_SLIB);
					}
				}
				else if(fileExists(spart_saved))
					SFile::Remove(spart_saved);
			}
		}
	}
	CATCHZOK
	return ok;
}

#define SUBDIR_LINKFILES "LinkFiles" // PPTXT_LNKFILESDIR

int DBBackup::CopyLinkFiles(const char * pSrcPath, const char * pDestPath, BackupLogFunc fnLog, void * extraPtr)
{
	int    ok = -1;
	SString buf, src_dir, dest_dir;
	src_dir.CopyFrom(pSrcPath).SetLastSlash().Cat(SUBDIR_LINKFILES).RmvLastSlash();
	dest_dir.CopyFrom(pDestPath).SetLastSlash().Cat(SUBDIR_LINKFILES).RmvLastSlash();
	if(IsDirectory(src_dir)) {
		SString src_path, dest_path;
		SDirec direc;
		SDirEntry fb;
		RemoveDir(dest_dir);
		createDir(dest_dir);
		src_dir.SetLastSlash();
		dest_dir.SetLastSlash();
		(buf = src_dir).Cat("*.*");
		for(direc.Init(buf); direc.Next(&fb) > 0;) {
			if(!(fb.Attr & 0x10)) {
				fb.GetNameA(src_dir, src_path);
				fb.GetNameA(dest_dir, dest_path);
				if(SCopyFile(src_path, dest_path, DBBackup::CopyProgressProc, FILE_SHARE_READ, this) <= 0)
					LogMessage(fnLog, BACKUPLOG_ERR_COPY, src_path, extraPtr);
			}
		}
		ok = 1;
	}
	return ok;
}

int DBBackup::CopyByRedirect(const char * pDBPath, BackupLogFunc fnLog, void * extraPtr)
{
	SString redirect_file;
	redirect_file.CopyFrom(pDBPath).SetLastSlash().Cat(FILE_REDIRECT);
	if(fileExists(redirect_file)) {
		SFile f(redirect_file, SFile::mRead);
		if(f.IsValid()) {
 			SString buf;
			while(f.ReadLine(buf) > 0) {
				uint j = 0;
				SString spart;
				SPathStruc ps;
				SString tbl_name, dest_path, src_path;
				TablePartsEnum tpe(0);
				buf.Divide('=', tbl_name, dest_path);
				dest_path.TrimRightChr('\x0A');
				dest_path.TrimRightChr('\x0D');
				SPathStruc::ReplaceExt(tbl_name, ".btr", 1);
				dest_path.SetLastSlash().Cat(tbl_name);
				src_path.CopyFrom(pDBPath).SetLastSlash().Cat(tbl_name);
				for(tpe.Init(src_path); tpe.Next(spart) > 0;) {
					if(fileExists(spart)) {
						const SPathStruc sp(spart);
						SPathStruc::ReplaceExt(dest_path, sp.Ext, 1);
						if(SCopyFile(spart, dest_path, DBBackup::CopyProgressProc, FILE_SHARE_READ, this) <= 0)
							LogMessage(fnLog, BACKUPLOG_ERR_COPY, src_path, extraPtr);
						else
							SFile::Remove(spart);
					}
				}
			}
		}
	}
	return 1;
}

int DBBackup::Restore(const BCopyData * pData, BackupLogFunc fnLog, void * extraPtr)
{
	EXCEPTVAR(DBErrCode);
	int    ok = 1;
	CopyParams cp;
	THROW_V(P_Db, SDBERR_BU_DICTNOPEN);
	THROW(GetCopyParams(pData, &cp));
	P_Db->GetDataPath(cp.Path);
	if(CheckAvailableDiskSpace(cp.Path, cp.TotalSize) && CheckCopy(pData, cp, fnLog, extraPtr) > 0) {
		SString copy_path;
		THROW(RemoveDatabase(1));
		//THROW(DoCopy(&cp, /* @v10.8.3 -BIN(pData->SrcSize != pData->DestSize),*/ fnLog, extraPtr));
		THROW(DoRestore(&cp, fnLog, extraPtr));
		THROW(CopyByRedirect(cp.Path, fnLog, extraPtr));
		(copy_path = pData->CopyPath).Strip().SetLastSlash().Cat(pData->SubDir).Strip();
		THROW(CopyLinkFiles(copy_path, cp.Path, fnLog, extraPtr));
	}
	else
		CALLEXCEPT();
	CATCHZOK
	RestoreRemovedDB(ok != 1);
	return ok;
}

int DBBackup::RemoveCopy(const BCopyData * pData, BackupLogFunc fnLog, void * extraPtr)
{
	EXCEPTVAR(DBErrCode);
	int    ok = 1;
	THROW_V(P_Db, SDBERR_BU_DICTNOPEN);
	if(pData->CopyPath.NotEmpty()) { // @v11.2.8
		CopyParams cp;
		SString backup_path;
		(backup_path = pData->CopyPath).Strip().SetLastSlash().Cat(pData->SubDir).Strip();
		if(GetCopyParams(pData, &cp)) {
			SString src_file_name;
			for(uint ssp = 0; cp.SsFiles.get(&ssp, src_file_name);) {
				SFile::Remove(src_file_name);
				if(pData->DestSize != pData->SrcSize) {
					SPathStruc::ReplaceExt(src_file_name, "BT_", 1);
					SFile::Remove(src_file_name);
				}
			}
			RemoveDir(cp.Path);
			THROW(InfoF->RemoveRecord(pData->Set, pData->ID));
			LogMessage(fnLog, BACKUPLOG_SUC_REMOVE, /*pData->CopyPath*/backup_path, extraPtr);
		}
		else {
			InfoF->RemoveRecord(pData->Set, pData->ID);
			LogMessage(fnLog, BACKUPLOG_ERR_REMOVE, backup_path, extraPtr);
			ok = 0;
		}
	}
	else
		ok = -1;
	CATCHZOK
	return ok;
}

int DBBackup::MakeCopyPath(BCopyData * data, SString & rDestPath)
{
	int    ok = 1;
	SString path, subdir;
	rDestPath.Z();
	(path = data->CopyPath).RmvLastSlash();
	if(::access(path, 0) != 0) {
		if(!createDir(path))
			ok = (DBErrCode = SDBERR_SLIB, 0);
	}
	if(ok) {
		ulong  set_sum = 0;
		long   count = 0;
		for(int n = 0; n < sizeof(data->Set) && data->Set[n] != 0; n++)
			set_sum += static_cast<ulong>(data->Set[n]);
		set_sum %= 10000L;
		do {
			subdir.Z().Cat(getcurdate_(), DATF_YMD|DATF_CENTURY|DATF_NODIV).CatLongZ(++count, 4);
			(path = data->CopyPath).SetLastSlash().Cat(subdir);
		} while(::access(path, 0) == 0);
		path.SetLastSlash();
		if(!createDir(path))
			ok = (DBErrCode = SDBERR_SLIB, 0);
		else {
			rDestPath = path;
			data->SubDir = subdir;
		}
	}
	return ok;
}

//static
int DBBackup::CopyProgressProc(const SDataMoveProgressInfo * scfd)
{
	DBBackup * dbb = static_cast<DBBackup *>(scfd->ExtraPtr);
	return dbb->CBP_CopyProcess(scfd->P_Src, scfd->P_Dest, dbb->TotalCopySize, scfd->SizeTotal, dbb->TotalCopyReady + scfd->SizeDone, scfd->SizeDone);
}

int DBBackup::DoCopy(DBBackup::CopyParams * pParam, BackupLogFunc fnLog, void * extraPtr)
{
	EXCEPTVAR(DBErrCode);
#ifndef __CONFIG__
	int (* p_callback_proc)(long, long, const char *, int) = CallbackCompress;
#else
	int (* p_callback_proc)(long, long, const char *, int) = 0;
#endif //__CONFIG__
	int    ok = 1;
	SString src_file_name;
	SString dest_file;
	SPathStruc ps, ps_inner;
	StringSet temp_ss_files;
	LogMessage(fnLog, BACKUPLOG_BEGIN, pParam->Path, extraPtr);
	SString dest = pParam->Path;
	ps.Split(dest.SetLastSlash());
	TotalCopySize  = pParam->TotalSize;
	TotalCopyReady = 0;
	for(uint ssp = 0; pParam->SsFiles.get(&ssp, src_file_name);) {
		SFile::Stat stat;
		ps_inner.Split(src_file_name);
		ps_inner.Merge(&ps, SPathStruc::fDrv|SPathStruc::fDir, dest_file);
		if(!SFile::GetStat(src_file_name, 0, &stat, 0))
			LogMessage(fnLog, BACKUPLOG_ERR_GETFILEPARAM, src_file_name, extraPtr);
		{
			const int64 sz = stat.Size;
			if(SCopyFile(src_file_name, dest_file, DBBackup::CopyProgressProc, FILE_SHARE_READ, this) <= 0)
				LogMessage(fnLog, BACKUPLOG_ERR_COPY, src_file_name, extraPtr);
			LogMessage(fnLog, BACKUPLOG_SUC_COPY, src_file_name, extraPtr);
			TotalCopyReady += sz;
		}
	}
	pParam->TotalSize = TotalCopyReady;
	LogMessage(fnLog, BACKUPLOG_END, pParam->Path, extraPtr);
	return ok;
}

int DBBackup::DoRestore(DBBackup::CopyParams * pParam, BackupLogFunc fnLog, void * extraPtr)
{
	//EXCEPTVAR(DBErrCode);
#ifndef __CONFIG__
	int (* p_callback_proc)(long, long, const char *, int) = CallbackCompress;
#else
	int (* p_callback_proc)(long, long, const char *, int) = 0;
#endif //__CONFIG__
	int    ok = 1;
	uint   i = 0;
	SString src_file_name;
	SString dest_file;
	SPathStruc ps, ps_inner;
	StringSet temp_ss_files;
	SString dest = pParam->Path;
	LogMessage(fnLog, BACKUPLOG_BEGIN, pParam->Path, extraPtr);
	ps.Split(dest.SetLastSlash());
	TotalCopySize  = pParam->TotalSize;
	TotalCopyReady = 0;
	for(uint ssp = 0; pParam->SsFiles.get(&ssp, src_file_name);) {
		int64 sz = 0;
		SFile::Stat fs;
		ps_inner.Split(src_file_name);
		ps_inner.Merge(&ps, SPathStruc::fDrv|SPathStruc::fDir, dest_file);
		if(!SFile::GetStat(src_file_name, 0, &fs, 0))
			LogMessage(fnLog, BACKUPLOG_ERR_GETFILEPARAM, src_file_name, extraPtr);
		{
			SPathStruc::ReplaceExt(dest_file, "btr", 1);
			if(SCopyFile(src_file_name, dest_file, DBBackup::CopyProgressProc, FILE_SHARE_READ, this) <= 0)
				LogMessage(fnLog, BACKUPLOG_ERR_COPY, src_file_name, extraPtr);
		}
		LogMessage(fnLog, BACKUPLOG_SUC_RESTORE, dest_file, extraPtr);
		TotalCopyReady += sz;
	}
	pParam->TotalSize = TotalCopyReady;
	LogMessage(fnLog, BACKUPLOG_END, pParam->Path, extraPtr);
	/*CATCH
		ok = 0;
		LogMessage(fnLog, BACKUPLOG_ERR_RESTORE, src_file_name, extraPtr);
	ENDCATCH*/
	return ok;
}
