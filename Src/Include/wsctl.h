// WSCTL.H
// Copyright (c) A.Sobolev 2023
//
//
// Descr: ���� ����-�������������. 
//
class WsCtl_SelfIdentityBlock {
public:
	WsCtl_SelfIdentityBlock();
	int    GetOwnUuid();

	S_GUID Uuid;
	PPID   PrcID; // ������������� ���������� �� �������. ������������ ������� �� �������.
	SString PrcName; // ������������ ���������� �� �������. ������������ ������� �� �������.
};
//
// 
// 
class WsCtl_Config {
public:
	WsCtl_Config();
	WsCtl_Config & Z();
	//
	// Descr: ��������� ������������ �� win-�������
	//
	int    Read();
	//
	// Descr: ���������� ������������ � win-������
	//
	int    Write();
	SJson * ToJsonObj() const;
	int    FromJsonObj(const SJson * pJsObj);

	SString Server;
	int    Port;
	int    Timeout;
	//
	SString DbSymb;
	SString User;
	SString Password;
};
/*
{
	"account": {
		"login": "abc",
		"password": "",
	},
	"app": {
		"enable": [
			"abc",
			"123"
		],
		"disable" : [
			"def",
			"456"
		]
	}
}
*/
class WsCtl_ClientPolicy {
public:
	WsCtl_ClientPolicy();
	bool FASTCALL operator == (const WsCtl_ClientPolicy & rS) { return IsEq(rS); }
	bool FASTCALL operator != (const WsCtl_ClientPolicy & rS) { return !IsEq(rS); }
	WsCtl_ClientPolicy & Z();
	bool   FASTCALL IsEq(const WsCtl_ClientPolicy & rS) const;
	SJson * ToJsonObj() const;
	int    FromJsonObj(const SJson * pJsObj);
	int    Apply();

	SString SysUser;
	SString SysPassword;
	StringSet SsAppEnabled;
	StringSet SsAppDisabled;
};
//
// Descr: ���������� ����������� ���������, ������������ � ���������� ����.
//
class WsCtl_ProgramEntry {
public:
	WsCtl_ProgramEntry();
	WsCtl_ProgramEntry & Z();
	SJson * ToJsonObj() const;
	int    FromJsonObj(const SJson * pJsObj);
	SString Category;         // utf8 ��������� ���������
	SString Title;            // utf8 ������������ �� ������ ��������� ���������
	SString ExeFileName;      // utf8 ��� ������������ ����� (� �����������) 
	SString FullResolvedPath; // utf8 ������ ���� � ������������ �����.
	SString PicSymb;          // utf8 ������ ����������� ������ //
};

class WsCtl_ProgramCollection : public TSCollection <WsCtl_ProgramEntry> {
public:
	WsCtl_ProgramCollection();
	SJson * ToJsonObj() const;
	int    FromJsonObj(const SJson * pJsObj);
};
//
// Descr: ����, ���������� �� �������������� ��������� ������ � ������� WsCtl
//
class WsCtlSrvBlock {
public:
	struct AuthBlock {
		AuthBlock();
		AuthBlock & Z();
		//
		// Descr: ���������� true ���� �������� ��������� ���������������� � ����������� ������� ��� ������� ���������� �������
		//
		bool   IsEmpty() const;
		bool   FromJsonObj(const SJson * pJs);

		S_GUID  WsCtlUuid;
		SString LoginText;
		SString Pw;
		//
		// Results:
		//
		PPID   SCardID;
		PPID   PsnID;
	};
	struct StartSessBlock {
		StartSessBlock();
		//
		// Descr: ���������� true ���� �������� ��������� ���������������� � ����������� ������� ��� ������� ���������� �������
		//
		bool   IsEmpty() const { return (!SCardID || !GoodsID); }
		StartSessBlock & Z();
		bool   FromJsonObj(const SJson * pJs);

		S_GUID WsCtlUuid;
		PPID   SCardID;
		PPID   GoodsID;
		PPID   TechID;
		double Amount;
		//
		// Results:
		//
		PPID   TSessID;
		PPID   RetTechID;
		PPID   RetSCardID;
		PPID   RetGoodsID;
		LDATETIME ScOpDtm; // ����� �������� �������� ����� //
		double WrOffAmount; // ��������� ����� //
		STimeChunk SessTimeRange;
	};

	WsCtlSrvBlock();
	int    SearchPrcByWsCtlUuid(const S_GUID & rWsCtlUuid, PPID * pPrcID, SString * pPrcNameUtf8);
	int    Init(const S_GUID & rUuid);
	//
	// Descr: ���������� ������������� ������ ����� ���������, ������� ����� ���� ��������� ��� ��������� ��� ��� ��������������� ������.
	//
	int    GetRawQuotKindList(PPIDArray & rList);
	int    GetQuotList(PPID goodsID, PPID locID, const PPIDArray & rRawQkList, PPQuotArray & rQList);
	int    StartSess(StartSessBlock & rBlk);
	int    Auth(AuthBlock & rBlk);
	int    SendClientPolicy(SString & rResult);

	PPObjTSession TSesObj;
	PPObjQuotKind QkObj;
	PPObjGoods GObj;
	PPObjPerson PsnObj;
	PPObjSCard ScObj;
	PPObjArticle ArObj;
	S_GUID WsUUID; // UUID ����������� ������� �������
	PPID   PrcID;  // �� ����������, ���������������� ������� �������
	SString PrcNameUtf8;
private:
	PPSCardConfig ScCfg;
	PPID   ScSerID; // ����� ���� (�� ������������)
	PPID   PsnKindID; // ��� ����������, ��������� � ������ ���� ScSerID
};
