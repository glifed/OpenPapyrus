// SYNCCASH.CPP
// @codepage windows-1251
// ��������� (����������) ��� ���
//
#include <pp.h>
#pragma hdrstop
#include <comdisp.h>

#define DEF_BAUD_RATE		   10	// �������� ������ �� ��������� 128000 ���
#define MAX_BAUD_RATE		   10	// Max �������� ������ 128000 ���
#define DEF_STRLEN                 36	// ����� ������
#define DEF_DRAWER_NUMBER		0	// ����� ��������� �����
#define DEF_FONTSIZE			3	// ������� ������ ������

#define SERVICEDOC				0	// ��� ��������� - ���������
#define	SALECHECK				1	// ��� ��������� - ��� �� �������
#define RETURNCHECK				2	// ��� ��������� - ��� �� �������
#define DEPOSITCHECK			3	// ��� ��������� - ��� �������
#define INCASSCHECK				4	// ��� ��������� - ��� ������
#define CHECKRIBBON				0	// ������ �� �������� - ������� �����
#define JOURNALRIBBON			1	// ������ �� �������� - ����������� �����
#define FISCALMEM				2	// ������ �� �������� - ���������� ������
#define SLIPDOCUMENT			3	// ������ �� �������� - ���������� ��������

//
//   ������ ������ ����� � ��������� �����
//
#define PRNMODE_NO_PRINT			0x00	// ��� ������
#define NO_PAPER					0x01	// ��� ������
#define PRNMODE_AFTER_NO_PAPER		0x02	// �������� ������� ������ ����� ������ 2
#define PRNMODE_PRINT				0x04	// ����� ������
#define FRMODE_OPEN_CHECK			0x08	// ��� ������

//
//   ���� �������� ��� ��������� ������
//

#define RESCODE_NO_ERROR			  0
#define RESCODE_UNKNOWNCOMMAND		301		// ����������� �������
#define RESCODE_NO_CONNECTION		303		// ���������� �� �����������
#define RESCODE_SLIP_IS_EMPTY		400		// ����� ����������� ��������� ����
#define RESCODE_INVEKLZSTATE		401		// ������������ ��������� ����
#define RESCODE_MEMOVERFLOW					402		// ���� ��� �� �����������
#define RESCODE_GOTOCTO				403		// ������ ����. ������� ���������� � ���

//
//   Sync_BillTaxArray
//
struct Sync_BillTaxEntry {
	SLAPI  Sync_BillTaxEntry()
	{
		THISZERO();
	}
	long   VAT;      // prec 0.01
	long   SalesTax; // prec 0.01
	double Amount;
};

class Sync_BillTaxArray : public SArray {
public:
	SLAPI  Sync_BillTaxArray() : SArray(sizeof(Sync_BillTaxEntry))
	{
	}
	SLAPI ~Sync_BillTaxArray()
	{
		freeAll();
	}
	int    SLAPI Search(long VAT, long salesTax, uint * p = 0);
	int    SLAPI Insert(Sync_BillTaxEntry * e, uint * p = 0);
	int    SLAPI Add(Sync_BillTaxEntry * e);
	Sync_BillTaxEntry & SLAPI  at(uint p);
};

IMPL_CMPFUNC(Sync_BillTaxEnKey, i1, i2)
{
	int    si;
	CMPCASCADE2(si, (Sync_BillTaxEntry*)i1, (Sync_BillTaxEntry*)i2, VAT, SalesTax);
	return si;
}

int SLAPI Sync_BillTaxArray::Search(long VAT, long salesTax, uint * p)
{
	Sync_BillTaxEntry bte;
	bte.VAT      = VAT;
	bte.SalesTax = salesTax;
	return bsearch(&bte, p, PTR_CMPFUNC(Sync_BillTaxEnKey));
}

int SLAPI Sync_BillTaxArray::Insert(Sync_BillTaxEntry * e, uint * p)
{
	return ordInsert(e, p, PTR_CMPFUNC(Sync_BillTaxEnKey)) ? 1 : PPSetErrorSLib();
}

Sync_BillTaxEntry & SLAPI Sync_BillTaxArray::at(uint p)
{
	return *(Sync_BillTaxEntry*)SArray::at(p);
}

int SLAPI Sync_BillTaxArray::Add(Sync_BillTaxEntry * e)
{
	int    ok = 1;
	uint   p;
	if(Search(e->VAT, e->SalesTax, &p)) {
		Sync_BillTaxEntry & bte = at(p);
		bte.Amount += e->Amount;
	}
	else {
		THROW(Insert(e));
	}
	CATCHZOK
	return ok;
}

class SCS_SYNCCASH : public PPSyncCashSession {
public:
	SLAPI  SCS_SYNCCASH(PPID n, char * name, char * port);
	SLAPI ~SCS_SYNCCASH();
	virtual int SLAPI PrintCheck(CCheckPacket *, uint flags);
	virtual int SLAPI PrintCheckByBill(PPBillPacket * pPack, double multiplier);
	virtual int SLAPI PrintCheckCopy(CCheckPacket * pPack, const char * pFormatName, uint flags);
	virtual int SLAPI PrintSlipDoc(CCheckPacket * pPack, const char * pFormatName, uint flags);
	virtual int SLAPI GetSummator(double * val);
	virtual int SLAPI AddSummator(double add) { return 1; }
	virtual int SLAPI CloseSession(PPID sessID);
	virtual int SLAPI PrintXReport(const CSessInfo *);
	virtual int SLAPI PrintZReportCopy(const CSessInfo *);
	virtual int SLAPI PrintIncasso(double sum, int isIncome);
	virtual int SLAPI GetPrintErrCode();
	virtual int SLAPI OpenBox();
	virtual int SLAPI CheckForSessionOver();
	virtual int SLAPI PrintBnkTermReport(const char * pZCheck);

	// PPCashNode NodeRec;
	PPAbstractDevice * P_AbstrDvc;
	StrAssocArray Arr_In;
	StrAssocArray Arr_Out;
private:
	virtual int SLAPI InitChannel() { return 1; }
	int  SLAPI Connect();
	int  SLAPI ExchangeParams();
	int	 SLAPI GetDevParam(/*const PPCashNode * pIn,*/ StrAssocArray & rOut);
	int  SLAPI AnnulateCheck();
	int  SLAPI CheckForCash(double sum);
	//int  SLAPI CheckForEKLZOrFMOverflow() { return 1; }
	int  SLAPI PrintReport(int withCleaning);
	int	 SLAPI PrintDiscountInfo(CCheckPacket * pPack, uint flags);
	int  SLAPI GetCheckInfo(PPBillPacket * pPack, Sync_BillTaxArray * pAry, long * pFlags, SString & rName);
	int  SLAPI AllowPrintOper();
	void SLAPI SetErrorMessage();
	//void SLAPI WriteLogFile(PPID id);
	void SLAPI CutLongTail(char * pBuf);
	void SLAPI CutLongTail(SString & rBuf);
	int  SLAPI LineFeed(int lineCount, int useReceiptRibbon, int useJournalRibbon);
	int  SLAPI CheckForRibbonUsing(uint ribbonParam, StrAssocArray & rOut); // ribbonParam == SlipLineParam::RegTo
	int SLAPI ExecPrintOper(int cmd, StrAssocArray & rIn, StrAssocArray & rOut);
	int SLAPI ExecOper(int cmd, StrAssocArray & rIn, StrAssocArray & rOut);
	int GetStatus(int & rStatus);
	int SetLogotype();
	int GetPort(const char * pPortName, int * pPortNo);

	enum DevFlags {
		sfConnected     = 0x0001, // ����������� ����� � ���, COM-���� �����
		sfOpenCheck     = 0x0002, // ��� ������
		sfCancelled     = 0x0004, // �������� ������ ���� �������� �������������
		sfPrintSlip     = 0x0010, // ������ ����������� ���������
		sfNotUseCutter  = 0x0020, // �� ������������ �������� �����
		sfUseWghtSensor = 0x0040  // ������������ ������� ������
	};
	static int RefToIntrf;
	int	   Port;            // ����� �����
	long   CashierPassword; // ������ �������
	long   AdmPassword;     // ������ ����.��������������
	int    ResCode;         // ��� ���������� �������
	int    ErrCode;         // ��� ������, ������������ ��������
	int    DeviceType;
	long   CheckStrLen;     // ����� ������ ���� � ��������
	long   Flags;           // ����� ��������� ���
	uint   RibbonParam;     // �������� ��� ������
	uint   Inited;          // ���� ����� 0, �� ���������� �� ����������������, 1 - ����������������
	int	   PrintLogo;       // �������� �������
	int	   IsSetLogo;       // 0 - ������� �� ��������, 1 - ������� ��������
	SString AdmName;        // ��� ����.��������������
};

int  SCS_SYNCCASH::RefToIntrf = 0;

class CM_SYNCCASH : public PPCashMachine {
public:
	SLAPI CM_SYNCCASH(PPID cashID) : PPCashMachine(cashID)
	{
	}
	PPSyncCashSession * SLAPI SyncInterface();
};

PPSyncCashSession * SLAPI CM_SYNCCASH::SyncInterface()
{
	PPSyncCashSession * cs = 0;
	if(IsValid()) {
		cs = (PPSyncCashSession *)new SCS_SYNCCASH(NodeID, NodeRec.Name, NodeRec.Port);
		CALLPTRMEMB(cs, Init(NodeRec.Name, NodeRec.Port));
	}
	return cs;
}

REGISTER_CMT(SYNCCASH,1,0);

static void SLAPI WriteLogFile_PageWidthOver(const char * pFormatName)
{
	SString msg_fmt, msg;
	msg.Printf(PPLoadTextS(PPTXT_SLIPFMT_WIDTHOVER, msg_fmt), pFormatName);
	PPLogMessage(PPFILNAM_SHTRIH_LOG, msg, LOGMSGF_TIME|LOGMSGF_USER);
}

int SCS_SYNCCASH::GetPort(const char * pPortName, int * pPortNo)
{
	int    ok = 0, port = 0;
	*pPortNo = 0;
	if(pPortName) {
		int  comdvcs = IsComDvcSymb(pPortName, &port);
		if(comdvcs == comdvcsCom && port > 0 && port < 32) {
			ASSIGN_PTR(pPortNo, port-1);
			ok = 1;
		}
	}
	if(!ok)
		PPSetError(PPERR_SYNCCASH_INVPORT);
	return ok;
}

SLAPI SCS_SYNCCASH::SCS_SYNCCASH(PPID n, char * name, char * port) : PPSyncCashSession(n, name, port)
{
	Port			= 0;
	CashierPassword = 0;
	AdmPassword     = 0;
	ResCode         = RESCODE_NO_ERROR;
	ErrCode         = SYNCPRN_NO_ERROR;
	CheckStrLen     = DEF_STRLEN;
	Flags           = 0;
	RibbonParam     = 0;
	Inited			= 0;
	IsSetLogo		= 0;
	PrintLogo		= 0;
	if(SCn.Flags & CASHF_NOTUSECHECKCUTTER)
		Flags |= sfNotUseCutter;
	RefToIntrf++;

	P_AbstrDvc = new PPAbstractDevice(0);
	P_AbstrDvc->PCpb.Cls = DVCCLS_SYNCPOS;
	P_AbstrDvc->GetDllName(DVCCLS_SYNCPOS, SCn.CashType, P_AbstrDvc->PCpb.DllName);
	THROW(P_AbstrDvc->IdentifyDevice(P_AbstrDvc->PCpb.DllName));
	THROW(GetPort(SCn.Port, &Port));
	CATCH
		State |= stError;
	ENDCATCH
}

SLAPI SCS_SYNCCASH::~SCS_SYNCCASH()
{
	if(Flags & sfConnected) {
		ExecOper(DVCCMD_DISCONNECT, Arr_In, Arr_Out);
		ExecOper(DVCCMD_RELEASE, Arr_In, Arr_Out);
		Inited = 0;
	}
	ZDELETE(P_AbstrDvc);
}

int ArrAdd(StrAssocArray & rArr, int pos, int val)
{
	SString str;
	return rArr.Add(pos, (str = 0).Cat(val), 1) ? 1 : PPSetErrorSLib();
}

int ArrAdd(StrAssocArray & rArr, int pos, double val)
{
	SString str;
	return rArr.Add(pos, (str = 0).Cat(val), 1) ? 1 : PPSetErrorSLib();
}

int ArrAdd(StrAssocArray & rArr, int pos, const char * str)
{
	return rArr.Add(pos, str, 1) ? 1 : PPSetErrorSLib();
}

int SLAPI SCS_SYNCCASH::Connect()
{
	int    ok = 1;
	int    model_type = 0, major_prot_ver = 0, minor_prot_ver = 0, not_use_wght_sensor = 0;
	int    def_baud_rate = DEF_BAUD_RATE;
	SString buf, buf1;
	PPIniFile ini_file;

	if(Flags & sfConnected) {
		Arr_In.Clear();
		THROW(ExecOper(DVCCMD_DISCONNECT, Arr_In, Arr_Out));
		Flags &= ~sfConnected;
	}
	ok = 0;
	if(!Inited) {
		Arr_In.Clear();
		THROW(ExecOper(DVCCMD_INIT, Arr_In, Arr_Out));
	}
	THROW_PP(ini_file.Get(PPINISECT_SYSTEM, PPINIPARAM_SHTRIHFRPASSWORD, buf) > 0, PPERR_SHTRIHFRADMPASSW);
	buf.Divide(',', buf1, AdmName);
	CashierPassword = AdmPassword = buf1.ToLong();
	AdmName.Strip().Transf(CTRANSF_INNER_TO_OUTER);
	if(ini_file.Get(PPINISECT_CONFIG, PPINIPARAM_SHTRIHFRCONNECTPARAM, buf) > 0) {
		SString  buf2;
		if(buf.Divide(',', buf1, buf2) > 0)
			def_baud_rate = buf1.ToLong();
		if(def_baud_rate > MAX_BAUD_RATE)
			def_baud_rate = DEF_BAUD_RATE;
	}
	do {
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_PORT, Port));
		THROW(ArrAdd(Arr_In, DVCPARAM_BAUDRATE, def_baud_rate));
		ok = ExecOper(DVCCMD_CONNECT, Arr_In, Arr_Out);
		def_baud_rate--;
		if((ok != 1) && (ResCode != RESCODE_NO_CONNECTION))
			THROW(ok);
	} while((ok != 1) && (def_baud_rate >= 0));
	THROW(ok);
	Flags |= sfConnected;
	THROW(ini_file.GetInt(PPINISECT_CONFIG, PPINIPARAM_SHTRIHFRNOTUSEWEIGHTSENSOR, &not_use_wght_sensor));
	SETFLAG(Flags, sfUseWghtSensor, !not_use_wght_sensor);
	THROW(ExchangeParams());
	CATCH
		if(Flags & sfConnected) {
			SetErrorMessage();
			Arr_In.Clear();
			THROW(ExecOper(DVCCMD_DISCONNECT, Arr_In, Arr_Out));
		}
		else {
			Flags |= sfConnected;
			SetErrorMessage();
		}
		Flags &= ~sfConnected;
		ok = 0;
	ENDCATCH
	return ok;
}

int  SLAPI SCS_SYNCCASH::AnnulateCheck()
{
	int    ok = -1, mode = 0, adv_mode = 0, cut = 0, status = 0;
	SString buf, param_name, param_val;
	GetStatus(status);
	// �������� �� ������������� ������
	if(status & PRNMODE_AFTER_NO_PAPER) {
		Flags |= sfOpenCheck;
		Arr_In.Clear();
		THROW(ExecPrintOper(DVCCMD_CONTINUEPRINT, Arr_In, Arr_Out));
		do {
			GetStatus(status);
		} while(status & PRNMODE_PRINT);
		Flags &= ~sfOpenCheck;
	}
	// �������� �� ������� ��������� ����, ������� ���� ������������
	GetStatus(status);
	if(status & FRMODE_OPEN_CHECK) {
		Flags |= sfOpenCheck | sfCancelled;
		PPMessage(mfInfo|mfOK, PPINF_SHTRIHFR_CHK_ANNUL, 0);
		Arr_In.Clear();
		THROW(ExecPrintOper(DVCCMD_ANNULATE, Arr_In, Arr_Out));
		Flags &= ~(sfOpenCheck | sfCancelled);
		ok = 1;
	}
	CATCHZOK
	return ok;
}

static int DestrStr(const SString & rStr, SString & rParamName, SString & rParamVal)
{
	if(rStr.NotEmpty())
		rStr.Divide('=', rParamName, rParamVal);
	return 1;
}

int SLAPI SCS_SYNCCASH::PrintCheck(CCheckPacket * pPack, uint flags)
{
	int    ok = 1, chk_no = 0, is_format = 0;
	SString buf, input, param_name, param_val;
	SString temp_buf;
	ResCode = RESCODE_NO_ERROR;
	ErrCode = SYNCPRN_ERROR;
	THROW_PP(pPack, PPERR_INVPARAM);
	if(pPack->GetCount() == 0)
		ok = -1;
	else {
		SlipDocCommonParam sdc_param;
		double amt = fabs(R2(MONEYTOLDBL(pPack->Rec.Amount)));
		double sum = fabs(pPack->_Cash) + 0.001;
		double running_total = 0.0;
		double fiscal = 0.0, nonfiscal = 0.0;
		pPack->HasNonFiscalAmount(&fiscal, &nonfiscal);
		THROW(Connect());
		if(flags & PRNCHK_LASTCHKANNUL)
			THROW(AnnulateCheck());
		if(flags & PRNCHK_RETURN && !(flags & PRNCHK_BANKING)) {
			int    is_cash;
			THROW(is_cash = CheckForCash(amt));
			THROW_PP(is_cash > 0, PPERR_SYNCCASH_NO_CASH);
		}
		if(P_SlipFmt) {
			int      prn_total_sale = 1, r = 0;
			SString  line_buf, format_name = "CCheck";
			SlipLineParam sl_param;
			THROW(r = P_SlipFmt->Init(format_name, &sdc_param));
			if(r > 0) {
				P_SlipFmt->InitIteration(pPack);
				P_SlipFmt->NextIteration(line_buf, &sl_param);
				is_format = 1;
				if(sdc_param.PageWidth > (uint)CheckStrLen)
					WriteLogFile_PageWidthOver(format_name);
				RibbonParam = 0;
				Arr_In.Clear();
				CheckForRibbonUsing(sdc_param.RegTo, Arr_In);
				if(fiscal != 0.0) {
					PROFILE_START_S("DVCCMD_OPENCHECK")
					THROW(ArrAdd(Arr_In, DVCPARAM_CHECKTYPE, (flags & PRNCHK_RETURN) ? RETURNCHECK : SALECHECK));
					THROW(ArrAdd(Arr_In, DVCPARAM_CHECKNUM, pPack->Rec.Code));
					THROW(ExecPrintOper(DVCCMD_OPENCHECK, Arr_In, Arr_Out));
					PROFILE_END
				}
				else {
					PROFILE_START_S("DVCCMD_OPENCHECK")
					//THROW(SetFR(DocumentName, "" /*sdc_param.Title*/)); // @v6.8.2
					//THROW(ExecFRPrintOper(PrintDocumentTitle)); // @v6.8.2
					THROW(ArrAdd(Arr_In, DVCPARAM_CHECKTYPE, SERVICEDOC));
					THROW(ArrAdd(Arr_In, DVCPARAM_CHECKNUM, pPack->Rec.Code));
					THROW(ExecPrintOper(DVCCMD_OPENCHECK, Arr_In, Arr_Out));
					PROFILE_END
					PROFILE_START_S("DVCCMD_PRINTTEXT")
					Arr_In.Clear();
					THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, sdc_param.Title));
					THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
					PROFILE_END
				}
				Flags |= sfOpenCheck;
				for(P_SlipFmt->InitIteration(pPack); P_SlipFmt->NextIteration(line_buf, &sl_param) > 0;) {
					Arr_In.Clear();
					if(sl_param.Flags & SlipLineParam::fRegFiscal) {
						CheckForRibbonUsing(SlipLineParam::fRegRegular | SlipLineParam::fRegJournal, Arr_In);
						double _q = sl_param.Qtty;
						double _p = sl_param.Price;
						running_total += (_q * _p);
						PROFILE_START_S("DVCCMD_PRINTFISCAL")
						THROW(ArrAdd(Arr_In, DVCPARAM_QUANTITY, _q));
						THROW(ArrAdd(Arr_In, DVCPARAM_PRICE, fabs(_p)));
						THROW(ArrAdd(Arr_In, DVCPARAM_DEPARTMENT, (sl_param.DivID > 16 || sl_param.DivID < 0) ? 0 :  sl_param.DivID));
						THROW(ExecPrintOper(DVCCMD_PRINTFISCAL, Arr_In, Arr_Out));
						PROFILE_END
						Flags |= sfOpenCheck;
						prn_total_sale = 0;
					}
					else if(sl_param.Kind == sl_param.lkBarcode) {
						;
					}
					else if(sl_param.Kind == sl_param.lkSignBarcode) {
						if(line_buf.NotEmptyS()) {
							PROFILE(CheckForRibbonUsing(SlipLineParam::fRegRegular, Arr_In));
							// type: EAN8 EAN13 UPCA UPCE CODE39 IL2OF5 CODABAR PDF417 QRCODE
							// width (points)
							// height (points)
							// label : none below above
							// text: code
							PPBarcode::GetStdName(sl_param.BarcodeStd, temp_buf);
							if(temp_buf.Empty()) {
								temp_buf = "qr"; // "pdf417";
							}
							PROFILE_START_S("DVCCMD_PRINTBARCODE")
							ArrAdd(Arr_In, DVCPARAM_TYPE, temp_buf);
							ArrAdd(Arr_In, DVCPARAM_WIDTH,  sl_param.BarcodeWd);
							ArrAdd(Arr_In, DVCPARAM_HEIGHT, sl_param.BarcodeHt);
							if(sl_param.Flags & sl_param.fBcTextBelow) {
								ArrAdd(Arr_In, DVCPARAM_LABEL, "below");
							}
							else if(sl_param.Flags & sl_param.fBcTextAbove) {
								ArrAdd(Arr_In, DVCPARAM_LABEL, "above");
							}
							THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, line_buf));
							THROW(ExecPrintOper(DVCCMD_PRINTBARCODE, Arr_In, Arr_Out));
							PROFILE_END
						}
					}
					else {
						PROFILE(CheckForRibbonUsing(sl_param.Flags, Arr_In));
						PROFILE_START_S("DVCCMD_PRINTTEXT")
						THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, line_buf.Trim((sl_param.Font > 1) ? CheckStrLen / 2 : CheckStrLen)));
						THROW(ArrAdd(Arr_In, DVCPARAM_FONTSIZE, (sl_param.Font == 1) ? DEF_FONTSIZE : sl_param.Font));
						THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
						PROFILE_END
					}
				}
				Arr_In.Clear();
				running_total = fabs(running_total); // @v7.7.2
				CheckForRibbonUsing(SlipLineParam::fRegRegular|SlipLineParam::fRegJournal, Arr_In);
				if(prn_total_sale) {
					if(fiscal != 0.0) {
						PROFILE_START_S("DVCCMD_PRINTFISCAL")
						if(!pPack->GetCount()) {
							THROW(ArrAdd(Arr_In, DVCPARAM_QUANTITY, 1L));
							THROW(ArrAdd(Arr_In, DVCPARAM_PRICE, amt));
							THROW(ExecPrintOper(DVCCMD_PRINTFISCAL, Arr_In, Arr_Out));
							Flags |= sfOpenCheck;
							running_total += amt;
						}
						else /*if(fiscal != 0.0)*/ {
							THROW(ArrAdd(Arr_In, DVCPARAM_QUANTITY, 1L));
							THROW(ArrAdd(Arr_In, DVCPARAM_PRICE, fiscal));
							THROW(ExecPrintOper(DVCCMD_PRINTFISCAL, Arr_In, Arr_Out));
							Flags |= sfOpenCheck;
							running_total += fiscal;
						}
						PROFILE_END
					}
				}
				else if(running_total != amt) {
					SString fmt_buf, msg_buf, added_buf;
					PPLoadText(PPTXT_SHTRIH_RUNNGTOTALGTAMT, fmt_buf);
					const char * p_sign = (running_total > amt) ? " > " : ((running_total < amt) ? " < " : " ?==? ");
					(added_buf = 0).Cat(running_total, MKSFMTD(0, 20, NMBF_NOTRAILZ)).Cat(p_sign).Cat(amt, MKSFMTD(0, 20, NMBF_NOTRAILZ));
					msg_buf.Printf(fmt_buf, added_buf.cptr());
					PPLogMessage(PPFILNAM_SHTRIH_LOG, msg_buf, LOGMSGF_TIME|LOGMSGF_USER);
				}
			}
		}
		if(!is_format) {
			CCheckLineTbl::Rec ccl;
			for(uint pos = 0; pPack->EnumLines(&pos, &ccl) > 0;) {
				int  division = (ccl.DivID >= CHECK_LINE_IS_PRINTED_BIAS) ? ccl.DivID - CHECK_LINE_IS_PRINTED_BIAS : ccl.DivID;
				// ������������ ������
				GetGoodsName(ccl.GoodsID, buf);
				buf.Strip().Transf(CTRANSF_INNER_TO_OUTER).Trim(CheckStrLen);
				Arr_In.Clear();
				THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, buf));
				THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
				// ����
				Arr_In.Clear();
				THROW(ArrAdd(Arr_In, DVCPARAM_PRICE, R2(intmnytodbl(ccl.Price) - ccl.Dscnt)));
				// ����������
				THROW(ArrAdd(Arr_In, DVCPARAM_QUANTITY, R3(fabs(ccl.Quantity))));
				// �����
				THROW(ArrAdd(Arr_In, DVCPARAM_DEPARTMENT, (division > 16 || division < 0) ? 0 : division));
				THROW(ExecPrintOper(DVCCMD_PRINTFISCAL, Arr_In, Arr_Out));
				Flags |= sfOpenCheck;
			}
			// ���������� � ������
			THROW(PrintDiscountInfo(pPack, flags));
			(buf = 0).CatCharN('=', CheckStrLen);
			Arr_In.Clear();
			THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, buf));
			THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
		}
		Arr_In.Clear();
		if(nonfiscal > 0.0) {
			if(fiscal > 0.0) {
				if(flags & PRNCHK_BANKING) {
					THROW(ArrAdd(Arr_In, DVCPARAM_PAYMCARD, fiscal))
				}
				else {
					THROW(ArrAdd(Arr_In, DVCPARAM_PAYMCASH, fiscal));
				}
			}
		}
		else {
			if(running_total > sum || ((flags & PRNCHK_BANKING) && running_total != sum)) // @v7.5.1
				sum = running_total;
			if(flags & PRNCHK_BANKING) {
				double  add_paym = 0.0; // @v9.0.4 intmnytodbl(pPack->Ext.AddPaym)-->0.0
				const double add_paym_epsilon = 0.01;
				const double add_paym_delta = (add_paym - sum);
				if(add_paym_delta > 0.0 || fabs(add_paym_delta) < add_paym_epsilon) {
					add_paym = 0.0;
				}
				if(add_paym) {
					THROW(ArrAdd(Arr_In, DVCPARAM_PAYMCASH, sum - amt + add_paym));
					THROW(ArrAdd(Arr_In, DVCPARAM_PAYMCARD, amt - add_paym));
				}
				else {
					THROW(ArrAdd(Arr_In, DVCPARAM_PAYMCARD, sum));
				}
			}
			else {
				THROW(ArrAdd(Arr_In, DVCPARAM_PAYMCASH, sum));
			}
		}
		// ������ ��������� ���
		//if(fiscal != 0.0) {
			THROW(ExecPrintOper(DVCCMD_CLOSECHECK, Arr_In, Arr_Out));
		//}
		Flags &= ~sfOpenCheck;
		ErrCode = SYNCPRN_ERROR_AFTER_PRINT;
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_CHECKNUM, 0));
		THROW(ExecPrintOper(DVCCMD_GETCHECKPARAM, Arr_In, Arr_Out));
		if(Arr_Out.getCount()) {
			for(uint i = 0; Arr_Out.Get(i, buf) > 0; i++) {
				DestrStr(buf, param_name, param_val);
				if(param_name.CmpNC("CHECKNUM") == 0)
					pPack->Rec.Code = (int32)param_val.ToInt64();
			}
		}
		ErrCode = SYNCPRN_NO_ERROR;
	}
	CATCH
		if(Flags & sfCancelled) {
			Flags &= ~sfCancelled;
			if(ErrCode != SYNCPRN_ERROR_AFTER_PRINT) {
				SString no_print_txt;
				PPLoadText(PPTXT_CHECK_NOT_PRINTED, no_print_txt);
				ErrCode = (Flags & sfOpenCheck) ? SYNCPRN_CANCEL_WHILE_PRINT : SYNCPRN_CANCEL;
				PPLogMessage(PPFILNAM_SHTRIH_LOG, CCheckCore::MakeCodeString(&pPack->Rec, no_print_txt),
					LOGMSGF_TIME|LOGMSGF_USER);
				ok = 0;
			}
		}
		else {
			SetErrorMessage();
			if(Flags & sfOpenCheck)
				ErrCode = SYNCPRN_ERROR_WHILE_PRINT;
			ok = 0;
		}
	ENDCATCH
	return ok;
}

int SLAPI SCS_SYNCCASH::CheckForCash(double sum)
{
	double cash_sum = 0.0;
	if(GetSummator(&cash_sum))
		return (cash_sum < sum) ? -1 : 1;
	return 0;
}

int SLAPI SCS_SYNCCASH::GetSummator(double * val)
{
	int    ok = 1;
	double cash_amt = 0.0;
	ResCode = RESCODE_NO_ERROR;
	SString input, buf, param_name, param_val;
	THROW(Connect());
	Arr_In.Clear();
	THROW(ArrAdd(Arr_In, DVCPARAM_CASHAMOUNT, 0));
	THROW(ExecPrintOper(DVCCMD_GETCHECKPARAM, Arr_In, Arr_Out));
	if(Arr_Out.getCount()) {
		for(uint i = 0; Arr_Out.Get(i, buf) > 0; i++) {
			DestrStr(buf, param_name, param_val);
			if(param_name.CmpNC("CASHAMOUNT") == 0)
				cash_amt = param_val.ToReal();
		}
	}
	CATCH
		ok = (SetErrorMessage(), 0);
	ENDCATCH
	ASSIGN_PTR(val, cash_amt);
	return ok;
}

int	SLAPI SCS_SYNCCASH::PrintDiscountInfo(CCheckPacket * pPack, uint flags)
{
	int    ok = 1;
	SString input;
	double amt = R2(fabs(MONEYTOLDBL(pPack->Rec.Amount)));
	double dscnt = R2(MONEYTOLDBL(pPack->Rec.Discount));
	if(flags & PRNCHK_RETURN)
		dscnt = -dscnt;
	if(dscnt > 0.0) {
		double  pcnt = round(dscnt * 100.0 / (amt + dscnt), 1);
		SString prn_str, temp_str;
		SCardCore scc;
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, (prn_str = 0).CatCharN('-', CheckStrLen)));
		THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
		(temp_str = 0).Cat(amt + dscnt, SFMT_MONEY);
		prn_str = "����� ��� ������"; // @cstr #0
		prn_str.CatCharN(' ', CheckStrLen - prn_str.Len() - temp_str.Len()).Cat(temp_str);
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, prn_str));
		THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
		if(scc.Search(pPack->Rec.SCardID, 0) > 0) {
			Arr_In.Clear();
			THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, (prn_str = "�����").Space().Cat(scc.data.Code)));
			THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
			if(scc.data.PersonID && GetPersonName(scc.data.PersonID, temp_str) > 0) { // @v6.0.9 GetObjectName-->GetPersonName
				(prn_str = "��������").Space().Cat(temp_str.Transf(CTRANSF_INNER_TO_OUTER)); // @cstr #2
				CutLongTail(prn_str);
				Arr_In.Clear();
				THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, prn_str));
				THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
			}
		}
		(temp_str = 0).Cat(dscnt, SFMT_MONEY);
		(prn_str = "������").Space().Cat(pcnt, MKSFMTD(0, (flags & PRNCHK_ROUNDINT) ? 0 : 1, NMBF_NOTRAILZ)).CatChar('%'); // @cstr #3
		prn_str.CatCharN(' ', CheckStrLen - prn_str.Len() - temp_str.Len()).Cat(temp_str);
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, prn_str));
		THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
	}
	CATCHZOK
	return ok;
}

void SLAPI SCS_SYNCCASH::CutLongTail(char * pBuf)
{
	char * p = 0;
	if(pBuf && strlen(pBuf) > (uint)CheckStrLen) {
		pBuf[CheckStrLen + 1] = 0;
		if((p = strrchr(pBuf, ' ')) != 0)
			*p = 0;
		else
			pBuf[CheckStrLen] = 0;
	}
}

void SLAPI SCS_SYNCCASH::CutLongTail(SString & rBuf)
{
	char  buf[256];
	rBuf.CopyTo(buf, sizeof(buf));
	CutLongTail(buf);
	rBuf = buf;
}

int SLAPI SCS_SYNCCASH::CloseSession(PPID sessID)
{
	return PrintReport(1);
}

int SLAPI SCS_SYNCCASH::PrintXReport(const CSessInfo *)
{
	return PrintReport(0);
}

int SLAPI SCS_SYNCCASH::PrintReport(int withCleaning)
{
	int    ok = 1, mode = 0;
	long   cshr_pssw = 0;
	ResCode = RESCODE_NO_ERROR;
	THROW(Connect());
	Flags |= sfOpenCheck;
	Arr_In.Clear();
	if(withCleaning) {
		THROW(ExecPrintOper(DVCCMD_ZREPORT, Arr_In, Arr_Out));
	}
	else {
		THROW(ExecPrintOper(DVCCMD_XREPORT, Arr_In, Arr_Out));
	}
	CATCH
		if(Flags & sfCancelled) {
			Flags &= ~sfCancelled;
			ok = -1;
		}
		else {
			SetErrorMessage();
			ok = 0;
		}
	ENDCATCH
	if(Flags & sfOpenCheck)
		Flags &= ~sfOpenCheck;
	return ok;
}

void SLAPI SCS_SYNCCASH::SetErrorMessage()
{
	Arr_In.Clear();
	if((Flags & sfConnected) && ResCode != RESCODE_NO_ERROR && ExecOper(DVCCMD_GETLASTERRORTEXT, Arr_In, Arr_Out) > 0) {
		SString err_msg, err_buf;
		for(uint i = 0; Arr_Out.Get(i, err_buf) > 0; i++)
			err_msg.Cat(err_buf);
		err_msg.ToOem();
		PPSetError(PPERR_SYNCCASH, err_msg);
		ResCode = RESCODE_NO_ERROR;
	}
}

int  SLAPI SCS_SYNCCASH::CheckForRibbonUsing(uint ribbonParam, StrAssocArray & rOut)
{
	int    ok = 1;
	if(ribbonParam) {
		if((RibbonParam & SlipLineParam::fRegRegular) != (ribbonParam & SlipLineParam::fRegRegular)) {
			if(ribbonParam & SlipLineParam::fRegRegular) {
                THROW(ArrAdd(rOut, DVCPARAM_RIBBONPARAM, CHECKRIBBON));
				SETFLAG(RibbonParam, SlipLineParam::fRegRegular, ribbonParam & SlipLineParam::fRegRegular);
			}
		}
		if((RibbonParam & SlipLineParam::fRegJournal) != (ribbonParam & SlipLineParam::fRegJournal)) {
			if(ribbonParam & SlipLineParam::fRegJournal) {
				THROW(ArrAdd(rOut, DVCPARAM_RIBBONPARAM, JOURNALRIBBON));
				SETFLAG(RibbonParam, SlipLineParam::fRegJournal, ribbonParam & SlipLineParam::fRegJournal);
			}
		}
	}
	CATCHZOK;
	return ok;
}

int SLAPI SCS_SYNCCASH::ExchangeParams()
{
	int     ok = 1;
	long    cshr_pssw = 0L;
	SString cshr_name, cshr_str, input, buf, param_name, param_val;
	// �������� ������ �������
	Arr_In.Clear();
	THROW(ExecPrintOper(DVCCMD_GETCONFIG, Arr_In, Arr_Out));
	if(Arr_Out.getCount())
		for(uint i = 0; Arr_Out.Get(i, buf) > 0; i++) {
			DestrStr(buf, param_name, param_val);
			if(strcmpi(param_name, "CASHPASS") == 0)
				CashierPassword = param_val.ToLong();
			else if(strcmpi(param_name, "CHECKSTRLEN") == 0)
				CheckStrLen = param_val.ToLong();
		}
	// ��������� �� ������� ����������� ����
	THROW(AnnulateCheck());

	// �������� ��������� ���������
	Arr_In.Clear();
	GetDevParam(/*&NodeRec,*/ Arr_In);
	THROW(ExecOper(DVCCMD_SETCFG, Arr_In, Arr_Out));

	// ��������� �������
	Arr_In.Clear();
	if(!IsSetLogo)
		THROW(SetLogotype());

	CATCHZOK
	CashierPassword = cshr_pssw;
	return ok;
}

int	 SLAPI SCS_SYNCCASH::GetDevParam(/*const PPCashNode * pIn,*/ StrAssocArray & rOut)
{
	int    ok = 1;
	int    val = 0;
	// @v7.4.4 PPObjCashNode cn_obj;
	// @v7.4.4 PPSyncCashNode  scn;
	PPIniFile ini_file;
	SString str, str1, str2;
	PPIDArray list;
	PPSecur sec_rec;

	// ���������� �������������� ��������� ����������
	THROW(ArrAdd(rOut, DVCPARAM_AUTOCASHNULL, 1));
	// @v7.4.4 cn_obj.GetSync(/*pIn->*/SCn.ID, &scn);
	// ��������� ID ���
	THROW(ArrAdd(rOut, DVCPARAM_ID, /*pIn->*/SCn.ID));
	// ���������� ��� ����� � ��������� ��� � �����
	GetPort(/*pIn->*/SCn.Port, &val);
	THROW(ArrAdd(rOut, DVCPARAM_PORT, val));
	// ���������� ����� ���
	THROW(ArrAdd(rOut, DVCPARAM_LOGNUM, /*pIn->LogNum*/1)); // @v7.4.4 (pIn->LogNum)-->(1)
    // �����
	THROW(ArrAdd(rOut, DVCPARAM_FLAGS, /*pIn->*/SCn.Flags));
	// ������� (���� ��� ���� ������ ��������� �����)
	THROW(ArrAdd(rOut, DVCPARAM_PRINTLOGO, PrintLogo))
	// ������ ��������� ��� �����-��-�
	ini_file.Get(PPINISECT_SYSTEM, PPINIPARAM_SHTRIHFRPASSWORD, str);
	str.Divide(',', str1, str2);
	THROW(ArrAdd(rOut, DVCPARAM_ADMINPASSWORD, str1));
	// ���������� ��� �������
	if(GetCurUserPerson(0, &str) == -1) {
		PPObjSecur sec_obj(PPOBJ_USR, 0);
		if(sec_obj.Fetch(LConfig.User, &sec_rec) > 0)
			str = sec_rec.Name;
	}
	THROW(ArrAdd(rOut, DVCPARAM_CSHRNAME, str));
	// ��� ��������������
	THROW(ArrAdd(rOut, DVCPARAM_ADMINNAME, AdmName.NotEmpty() ? AdmName : str));
	// ������� �������� �����
	THROW(ArrAdd(rOut, DVCPARAM_SESSIONID, /*pIn->*/SCn.CurSessID));
	CATCHZOK;
	return ok;

}

int SLAPI SCS_SYNCCASH::CheckForSessionOver()
{
	int    ok = -1;
	SString buf;
	THROW(Connect());
	Arr_In.Clear();
	THROW(ExecPrintOper(DVCCMD_CHECKSESSOVER, Arr_In, Arr_Out));
	if(Arr_Out.getCount())
		Arr_Out.Get(0, buf);
	if(buf.CmpNC("1") == 0) // ������ ������ 24 �����
		ok = 1;
	CATCHZOK;
	return ok;
}

int SLAPI SCS_SYNCCASH::PrintCheckCopy(CCheckPacket * pPack, const char * pFormatName, uint flags)
{
	int     ok = 1, is_format = 0;
	SString  input;
	SlipDocCommonParam  sdc_param;
	ResCode = RESCODE_NO_ERROR;
	ErrCode = SYNCPRN_ERROR_AFTER_PRINT;
	THROW_PP(pPack, PPERR_INVPARAM);
	THROW(Connect());
	Arr_In.Clear();
	THROW(ArrAdd(Arr_In, DVCPARAM_CHECKTYPE, SERVICEDOC));
	THROW(ArrAdd(Arr_In, DVCPARAM_CHECKNUM, pPack->Rec.Code));
	THROW(ExecPrintOper(DVCCMD_OPENCHECK, Arr_In, Arr_Out));
	if(P_SlipFmt) {
		int   r = 0;
		SString  line_buf, format_name = (pFormatName && pFormatName[0]) ? pFormatName : ((flags & PRNCHK_RETURN) ? "CCheckRetCopy" : "CCheckCopy");
		SlipLineParam  sl_param;
		THROW(r = P_SlipFmt->Init(format_name, &sdc_param));
		if(r > 0) {
			is_format = 1;
			if(sdc_param.PageWidth > (uint)CheckStrLen)
				WriteLogFile_PageWidthOver(format_name);
			RibbonParam = 0;
			Arr_In.Clear();
			CheckForRibbonUsing(sdc_param.RegTo, Arr_In);
			THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, sdc_param.Title));
			THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
			for(P_SlipFmt->InitIteration(pPack); P_SlipFmt->NextIteration(line_buf, &sl_param) > 0;) {
				Arr_In.Clear();
				CheckForRibbonUsing(sl_param.Flags, Arr_In);
				THROW(ArrAdd(Arr_In, DVCPARAM_FONTSIZE, (sl_param.Font == 1) ? DEF_FONTSIZE : sl_param.Font));
				THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, line_buf.Trim((sl_param.Font > 1) ? CheckStrLen / 2 : CheckStrLen)));
				THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
			}
		}
	}
	if(!is_format) {
		uint    pos;
		SString prn_str, temp_str;
		CCheckLineTbl::Rec ccl;
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, "����� ����"));
		THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, (flags & PRNCHK_RETURN) ? "������� �������" : "�������"));
		THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
		for(pos = 0; pPack->EnumLines(&pos, &ccl) > 0;) {
			double  price = intmnytodbl(ccl.Price) - ccl.Dscnt;
			double  qtty  = R3(fabs(ccl.Quantity));
			GetGoodsName(ccl.GoodsID, prn_str);
			CutLongTail(prn_str.Transf(CTRANSF_INNER_TO_OUTER));
			Arr_In.Clear();
			THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, prn_str));
			THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
			if(qtty != 1.0) {
				(temp_str = 0).Cat(qtty, MKSFMTD(0, 3, NMBF_NOTRAILZ)).CatDiv('X', 1).Cat(price, SFMT_MONEY);
				Arr_In.Clear();
				THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, (prn_str = 0).CatCharN(' ', CheckStrLen - temp_str.Len()).Cat(temp_str)));
				THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
			}
			(temp_str = 0).CatEq(0, qtty * price, SFMT_MONEY);
			Arr_In.Clear();
			THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, (prn_str = 0).CatCharN(' ', CheckStrLen - temp_str.Len()).Cat(temp_str)));
			THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
		}

		THROW(PrintDiscountInfo(pPack, flags));
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, (prn_str = 0).CatCharN('=', CheckStrLen)));
		THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
		(temp_str = 0).CatEq(0, fabs(MONEYTOLDBL(pPack->Rec.Amount)), SFMT_MONEY);
		prn_str = "����"; // @cstr #12
		prn_str.CatCharN(' ', CheckStrLen / 2 - prn_str.Len() - temp_str.Len()).Cat(temp_str);
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, prn_str));
		THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
	}
	THROW(LineFeed(6, TRUE, FALSE));
	Arr_In.Clear();
	THROW(ExecPrintOper(DVCCMD_CLOSECHECK, Arr_In, Arr_Out));
	ErrCode = SYNCPRN_NO_ERROR;
	CATCH
		if(Flags & sfCancelled) {
			Flags &= ~sfCancelled;
			ok = -1;
		}
		else {
			SetErrorMessage();
		}
	ENDCATCH
	return ok;
}

int SLAPI SCS_SYNCCASH::LineFeed(int lineCount, int useReceiptRibbon, int useJournalRibbon)
{
	int    ok = 1, cur_receipt, cur_journal;
	SString input, buf, param_name, param_val;
	Arr_In.Clear();
	THROW(ArrAdd(Arr_In, DVCPARAM_RIBBONPARAM, 0));
	THROW(ExecPrintOper(DVCCMD_GETCHECKPARAM, Arr_In, Arr_Out));
	if(Arr_Out.getCount()) {
		for(uint i = 0; Arr_Out.Get(i, buf) > 0; i++) {
			DestrStr(buf, param_name, param_val);
			if((param_name.CmpNC("RIBBONPARAM") == 0) && param_val.ToLong() == 0)
				cur_receipt = 1;
			else if((param_name.CmpNC("RIBBONPARAM") == 0) && param_val.ToLong() == 1)
				cur_journal = 1;
		}
	}
	for(uint i = 0; i < (uint)lineCount; i++) {
		Arr_In.Clear();
		if(cur_receipt != useReceiptRibbon)
			THROW(ArrAdd(Arr_In, DVCPARAM_RIBBONPARAM, 0));
		if(cur_journal != useJournalRibbon)
			THROW(ArrAdd(Arr_In, DVCPARAM_RIBBONPARAM, 1));
		THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, (buf = 0).CatCharN(' ', CheckStrLen)));
		THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
	}
	CATCHZOK
	return ok;
}

int SLAPI SCS_SYNCCASH::GetCheckInfo(PPBillPacket * pPack, Sync_BillTaxArray * pAry, long * pFlags, SString & rName)
{
	int    ok = 1, wovatax = 0;
	long   flags = 0;
	Sync_BillTaxEntry  bt_entry;
	PPID   main_org_id;
	if(GetMainOrgID(&main_org_id)) {
		PersonTbl::Rec prec;
		if(SearchObject(PPOBJ_PERSON, main_org_id, &prec) > 0 && prec.Flags & PSNF_NOVATAX)
			wovatax = 1;
	}
	THROW_PP(pPack && pAry, PPERR_INVPARAM);
	if(pFlags)
		flags = *pFlags;
	if(pPack->OprType == PPOPT_ACCTURN) {
		long   s_tax = 0;
		double amt1 = 0.0, amt2 = 0.0;
		PPObjAmountType amtt_obj;
		TaxAmountIDs    tais;
		double sum = BR2(pPack->Rec.Amount);
		if(sum < 0.0)
			flags |= PRNCHK_RETURN;
		sum = fabs(sum);
		amtt_obj.GetTaxAmountIDs(&tais, 1);
		if(tais.STaxAmtID)
			s_tax = tais.STaxRate;
		if(tais.VatAmtID[0])
			amt1 = fabs(pPack->Amounts.Get(tais.VatAmtID[0], 0L));
		if(tais.VatAmtID[1])
			amt2 = fabs(pPack->Amounts.Get(tais.VatAmtID[1], 0L));
		bt_entry.VAT = (amt1 || amt2) ? ((amt1 > amt2) ? tais.VatRate[0] : tais.VatRate[1]) : 0;
		bt_entry.SalesTax = s_tax;
		bt_entry.Amount   = sum;
		THROW(pAry->Add(&bt_entry));
	}
	else {
		PPTransferItem * ti;
		PPObjGoods  goods_obj;
		if(pPack->OprType == PPOPT_GOODSRETURN) {
			PPOprKind op_rec;
			if(GetOpData(pPack->Rec.OpID, &op_rec) > 0 && IsExpendOp(op_rec.LinkOpID) > 0)
				flags |= PRNCHK_RETURN;
		}
		else if(pPack->OprType == PPOPT_GOODSRECEIPT)
			flags |= PRNCHK_RETURN;
		for(uint i = 0; pPack->EnumTItems(&i, &ti);) {
			int re;
			PPGoodsTaxEntry  gt_entry;
			THROW(goods_obj.FetchTax(ti->GoodsID, pPack->Rec.Dt, pPack->Rec.OpID, &gt_entry));
			bt_entry.VAT = wovatax ? 0 : gt_entry.VAT;
			re = (ti->Flags & PPTFR_RMVEXCISE) ? 1 : 0;
			bt_entry.SalesTax = ((CConfig.Flags & CCFLG_PRICEWOEXCISE) ? !re : re) ? 0 : gt_entry.SalesTax;
			bt_entry.Amount   = ti->CalcAmount();
			THROW(pAry->Add(&bt_entry));
		}
	}
	if(pPack->Rec.Object)
		GetArticleName(pPack->Rec.Object, rName);
	CATCHZOK
	ASSIGN_PTR(pFlags, flags);
	return ok;
}

int SLAPI SCS_SYNCCASH::PrintCheckByBill(PPBillPacket * pPack, double multiplier)
{
	int     ok = 1, print_tax = 0;
	uint    pos;
	long    flags = 0;
	double  price, sum = 0.0;
	SString prn_str, name, input;
	Sync_BillTaxArray  bt_ary;
	PPIDArray     vat_ary;
	ResCode = RESCODE_NO_ERROR;
	ErrCode = SYNCPRN_ERROR;
	THROW_PP(pPack, PPERR_INVPARAM);
	THROW(GetCheckInfo(pPack, &bt_ary, &flags, name));
	if(bt_ary.getCount() == 0)
		return -1;
	THROW(Connect());
	THROW(AnnulateCheck());
	if(multiplier < 0)
		flags |= PRNCHK_RETURN;
	if(flags & PRNCHK_RETURN) {
		int    is_cash;
		THROW(is_cash = CheckForCash(fabs(BR2(pPack->Rec.Amount) * multiplier)));
		THROW_PP(is_cash > 0, PPERR_SYNCCASH_NO_CASH);
	}
	Arr_In.Clear();
	THROW(ArrAdd(Arr_In, DVCPARAM_CHECKTYPE, (flags & PRNCHK_RETURN) ? RETURNCHECK : SALECHECK));
	THROW(ExecPrintOper(DVCCMD_OPENCHECK, Arr_In, Arr_Out));
	for(pos = 0; pos < bt_ary.getCount(); pos++) {
		Sync_BillTaxEntry & bte = bt_ary.at(pos);
		// ����
		price = R2(fabs(bte.Amount * multiplier));
		sum += price;
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_PRICE, price));
		// ����������
		THROW(ArrAdd(Arr_In, DVCPARAM_QUANTITY, 1L));
		THROW(ExecPrintOper(DVCCMD_PRINTFISCAL, Arr_In, Arr_Out));
		(prn_str = "����� �� ������ ���").Space().Cat(fdiv100i(bte.VAT), MKSFMTD(0, 2, NMBF_NOTRAILZ)).CatChar('%'); // @cstr #6
		if(bte.SalesTax)
			prn_str.Space().Cat("���").Space().Cat(fdiv100i(bte.SalesTax), MKSFMTD(0, 2, NMBF_NOTRAILZ)).CatChar('%'); // @cstr #7
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, prn_str));
		THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
		Flags |= sfOpenCheck;
	}
	if(name.NotEmptyS()) {
		(prn_str = "����������").Space().Cat(name.Transf(CTRANSF_INNER_TO_OUTER)); // @cstr #8
		CutLongTail(prn_str);
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, prn_str));
		THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
	}
	Arr_In.Clear();
	THROW(ArrAdd(Arr_In, DVCPARAM_PAYMCASH, sum));
	THROW(ExecPrintOper(DVCCMD_CLOSECHECK, Arr_In, Arr_Out));
	Flags &= ~sfOpenCheck;
	ErrCode = SYNCPRN_NO_ERROR;
	CATCH
		if(Flags & sfCancelled) {
			Flags &= ~sfCancelled;
			if(ErrCode != SYNCPRN_ERROR_AFTER_PRINT) {
				ErrCode = (Flags & sfOpenCheck) ? SYNCPRN_CANCEL_WHILE_PRINT : SYNCPRN_CANCEL;
				ok = 0;
			}
		}
		else {
			SetErrorMessage();
			if(Flags & sfOpenCheck)
				ErrCode = SYNCPRN_ERROR_WHILE_PRINT;
			ok = 0;
		}
	ENDCATCH
	return ok;
}

int SLAPI SCS_SYNCCASH::PrintSlipDoc(CCheckPacket * pPack, const char * pFormatName, uint flags)
{
	int    ok = -1;
	SString input;
	SString temp_buf;
	ResCode = RESCODE_NO_ERROR;
	ErrCode = SYNCPRN_ERROR_AFTER_PRINT;
	THROW_PP(pPack, PPERR_INVPARAM);
	THROW(Connect());
	if(P_SlipFmt) {
		int   r = 1;
		SString   line_buf, format_name = (pFormatName && pFormatName[0]) ? pFormatName : "SlipDocument";
		StringSet head_lines((const char *)&r);
		SlipDocCommonParam  sdc_param;
		THROW(r = P_SlipFmt->Init(format_name, &sdc_param));
		if(r > 0) {
			int   str_num, print_head_lines = 0, fill_head_lines = 1;
			SlipLineParam  sl_param;
			Flags |= sfPrintSlip;
			Arr_In.Clear();
			THROW(ArrAdd(Arr_In, DVCPARAM_CHECKTYPE, SERVICEDOC));
			THROW(ExecPrintOper(DVCCMD_OPENCHECK, Arr_In, Arr_Out));
			Arr_In.Clear();
			THROW(ExecPrintOper(DVCCMD_CLEARSLIPBUF, Arr_In, Arr_Out));
			for(str_num = 0, P_SlipFmt->InitIteration(pPack); P_SlipFmt->NextIteration(line_buf, &sl_param) > 0;) {
				if(print_head_lines) {
					for(uint i = 0; head_lines.get(&i, temp_buf);) {
						Arr_In.Clear();
						THROW(ArrAdd(Arr_In, DVCPARAM_STRNUM, ++str_num));
						THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, temp_buf));
						THROW(ExecPrintOper(DVCCMD_FILLSLIPBUF, Arr_In, Arr_Out));
					}
					print_head_lines = 0;
				}
				else if(fill_head_lines)
					if(str_num < (int)sdc_param.HeadLines)
						head_lines.add(line_buf);
					else
						fill_head_lines = 0;
				Arr_In.Clear();
				THROW(ArrAdd(Arr_In, DVCPARAM_STRNUM, ++str_num));
				THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, line_buf));
				THROW(ExecPrintOper(DVCCMD_FILLSLIPBUF, Arr_In, Arr_Out));
				str_num++;
				if(str_num == sdc_param.PageLength) {
					Arr_In.Clear();
					THROW(ExecPrintOper(DVCCMD_PRINTSLIPDOC, Arr_In, Arr_Out));
					print_head_lines = 1;
					str_num = 0;
				}
			}
			if(str_num) {
				Arr_In.Clear();
				THROW(ExecPrintOper(DVCCMD_PRINTSLIPDOC, Arr_In, Arr_Out));
			}
			THROW(ExecPrintOper(DVCCMD_CLOSECHECK, Arr_In, Arr_Out));
			ok = 1;
		}
	}
	ErrCode = SYNCPRN_NO_ERROR;
	CATCH
		if(Flags & sfCancelled) {
			Flags &= ~sfCancelled;
			ok = -1;
		}
		else {
			SetErrorMessage();
		}
	ENDCATCH
	Flags &= ~sfPrintSlip;
	return ok;
}

int SLAPI SCS_SYNCCASH::PrintZReportCopy(const CSessInfo * pInfo)
{
	int  ok = -1;
	SString input;
	ResCode = RESCODE_NO_ERROR;
	ErrCode = SYNCPRN_ERROR_AFTER_PRINT;
	THROW_PP(pInfo, PPERR_INVPARAM);
	THROW(Connect());
	Arr_In.Clear();
	THROW(ArrAdd(Arr_In, DVCPARAM_CHECKTYPE, SERVICEDOC));
	THROW(ArrAdd(Arr_In, DVCPARAM_CHECKNUM, pInfo->Rec.SessNumber));
	THROW(ExecPrintOper(DVCCMD_OPENCHECK, Arr_In, Arr_Out));
	if(P_SlipFmt) {
		int   r = 0;
		SString  line_buf, format_name = "ZReportCopy";
		SlipDocCommonParam  sdc_param;
		THROW(r = P_SlipFmt->Init(format_name, &sdc_param));
		if(r > 0) {
			SlipLineParam  sl_param;
			if(sdc_param.PageWidth > (uint)CheckStrLen)
				WriteLogFile_PageWidthOver(format_name);
			RibbonParam = 0;
			Arr_In.Clear();
			CheckForRibbonUsing(sdc_param.RegTo, Arr_In);
			THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, sdc_param.Title));
			THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
			for(P_SlipFmt->InitIteration(pInfo); P_SlipFmt->NextIteration(line_buf, &sl_param) > 0;) {
				Arr_In.Clear();
				CheckForRibbonUsing(sl_param.Flags, Arr_In);
				THROW(ArrAdd(Arr_In, DVCPARAM_FONTSIZE, (sl_param.Font == 1) ? DEF_FONTSIZE : sl_param.Font));
				THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, line_buf.Trim((sl_param.Font > 1) ? CheckStrLen / 2 : CheckStrLen)));
				THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
			}
			THROW(LineFeed(6, TRUE, FALSE));
		}
	}
	Arr_In.Clear();
	THROW(ExecPrintOper(DVCCMD_CLOSECHECK, Arr_In, Arr_Out));
	ErrCode = SYNCPRN_NO_ERROR;
	CATCH
		if(Flags & sfCancelled) {
			Flags &= ~sfCancelled;
			ok = -1;
		}
		else {
			SetErrorMessage();
		}
	ENDCATCH
	return ok;
}

int SLAPI SCS_SYNCCASH::PrintIncasso(double sum, int isIncome)
{
	int    ok = 1;
	SString input;
	StrAssocArray in;

	ResCode = RESCODE_NO_ERROR;
	THROW(Connect());
	Flags |= sfOpenCheck;
	if(isIncome) {
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_CHECKTYPE, DEPOSITCHECK));
		THROW(ExecPrintOper(DVCCMD_OPENCHECK, Arr_In, Arr_Out));
		//THROW(LineFeed(6, TRUE, FALSE)); // @vmiller ��� ��� �� ��������� ��������, ������� ������ ��� �������� �����
	}
	else {
		int    is_cash;
		THROW(is_cash = CheckForCash(sum));
		THROW_PP(is_cash > 0, PPERR_SYNCCASH_NO_CASH);
		Arr_In.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_CHECKTYPE, INCASSCHECK));
		THROW(ExecPrintOper(DVCCMD_OPENCHECK, Arr_In, Arr_Out));
		//THROW(LineFeed(6, TRUE, FALSE)); // @vmiller ��� ��� �� ��������� ��������, ������� ������ ��� �������� �����
	}
	Arr_In.Clear();
	THROW(ArrAdd(Arr_In, DVCPARAM_AMOUNT, sum));
	THROW(ExecPrintOper(DVCCMD_INCASHMENT, Arr_In, Arr_Out));
	Arr_In.Clear();
	THROW(ExecPrintOper(DVCCMD_CLOSECHECK, Arr_In, Arr_Out));
	CATCH
		if(Flags & sfCancelled) {
			Flags &= ~sfCancelled;
			ok = -1;
		}
		else {
			SetErrorMessage();
			ok = 0;
		}
	ENDCATCH
	Flags &= ~sfOpenCheck;
	return ok;
}

int SLAPI SCS_SYNCCASH::OpenBox()
{
	int     ok = -1, is_drawer_open = 0;
	SString input;
	ResCode = RESCODE_NO_ERROR;
	ErrCode = SYNCPRN_ERROR;
	THROW(Connect());
	Arr_In.Clear();
	THROW(ArrAdd(Arr_In, DVCPARAM_DRAWERNUM, DEF_DRAWER_NUMBER));
	THROW(ExecPrintOper(DVCCMD_OPENBOX, Arr_In, Arr_Out));
	ok = 1;

	CATCH
		if(Flags & sfCancelled) {
			Flags &= ~sfCancelled;
			if(ErrCode != SYNCPRN_ERROR_AFTER_PRINT) {
				ErrCode = (Flags & sfOpenCheck) ? SYNCPRN_CANCEL_WHILE_PRINT : SYNCPRN_CANCEL;
				ok = 0;
			}
		}
		else {
			SetErrorMessage();
			if(Flags & sfOpenCheck)
				ErrCode = SYNCPRN_ERROR_WHILE_PRINT;
			PPError();
			ok = 0;
		}
	ENDCATCH
	return ok;
}

int SLAPI SCS_SYNCCASH::GetPrintErrCode()
{
	return ErrCode;
}

int SCS_SYNCCASH::GetStatus(int & rStatus)
{
	int    ok = 1;
	SString param_name, param_val, buf;
	StrAssocArray arr_in, arr_out;
	arr_in.Clear();
	THROW(ExecOper(DVCCMD_GETECRSTATUS, arr_in, arr_out));
	//THROW(ResCode == RESCODE_NO_ERROR);
	if(arr_out.getCount()) {
		for(uint i = 0; arr_out.Get(i, buf) > 0; i++) {
			DestrStr(buf, param_name, param_val);
			if(param_name.CmpNC("STATUS") == 0)
				rStatus = param_val.ToLong();
		}
	}
	CATCHZOK;
	return ok;
}

int SLAPI SCS_SYNCCASH::AllowPrintOper()
{
	int    ok = 1;
	int    wait_prn_err = 0;
	int    status = 0;
	StrAssocArray arr_in, arr_out;
	// SetErrorMessage();
	// �������� ��������� �������� ������
	do {
		GetStatus(status);
		wait_prn_err = 1;
	} while(status & PRNMODE_PRINT);
	//
	// ���� ��� ������� �����
	//
	GetStatus(status);
	if(status & NO_PAPER) {
		if(status & FRMODE_OPEN_CHECK)
			Flags |= sfOpenCheck;
		while(status & NO_PAPER) {
			int  send_msg = 0, r;
			PPSetError(PPERR_SYNCCASH_NO_CHK_RBN);
			send_msg = 1;
			r = PPError();
			if((!send_msg && r != cmOK) || (send_msg &&	PPMessage(mfConf|mfYesNo, PPCFM_SETPAPERTOPRINT, 0) != cmYes)) {
				Flags |= sfCancelled;
				ok = 0;
			}
			GetStatus(status);
		}
		wait_prn_err = 1;
		ResCode = RESCODE_NO_ERROR;
	}
	// ���������, ���� �� ��������� ������ ����� �������� �����
	if(status & PRNMODE_AFTER_NO_PAPER) {
		arr_in.Clear();
		THROW(ExecPrintOper(DVCCMD_CONTINUEPRINT, arr_in, arr_out));
		GetStatus(status);
		wait_prn_err = 1;
	}
	// �������������� ������, ��� ��� �� ���� ��� ��������� ������� �� ����������� ������, � ����� ���������� ������ ����
	do {
		GetStatus(status);
	} while(status & PRNMODE_PRINT);
	//
	// ������ ����
	//
	GetStatus(status);
	if(ResCode == RESCODE_GOTOCTO) {
		SetErrorMessage();
		SString  err_msg(DS.GetConstTLA().AddedMsgString), added_msg;
		if(PPLoadText(PPTXT_APPEAL_CTO, added_msg))
			err_msg.CR().Cat("\003").Cat(added_msg);
		PPSetAddedMsgString(err_msg);
		PPError();
		ok = -1;
	}
	THROW_PP(ResCode != RESCODE_MEMOVERFLOW, PPERR_SYNCCASH_OVERFLOW);
	//
	// ���� �������� �� ������� ��������������� � ��������� ������, ������ ��������� �� ������
	// @v5.9.2 ��� �������� ���� - ����� ������ ������ ����� ���� - �� ������� � ��������� ������, �� wait_prn_err == 1
	//
	if(!wait_prn_err || (ResCode == RESCODE_INVEKLZSTATE)) {
		SetErrorMessage();
		PPError();
		Flags |= sfCancelled;
		ok = 0;
	}
	CATCHZOK
	return ok;
}

int SLAPI SCS_SYNCCASH::ExecPrintOper(int cmd, StrAssocArray & rIn, StrAssocArray & rOut)
{
	int    ok = 1;
	int    r = 0;
	SString temp_buf;
	do {
		THROW(ok = P_AbstrDvc->RunCmd__(cmd, rIn, rOut));
		if(ok == -1) {
			THROW(rOut.Get(0, temp_buf));
			ResCode = temp_buf.ToLong();
			ok = 0;
		}
		if(ResCode == RESCODE_UNKNOWNCOMMAND || (Flags & sfPrintSlip && ResCode == RESCODE_SLIP_IS_EMPTY)) {
			ok = 0;
			break;
		}
		r = AllowPrintOper();
		//
		// ���� ������ ������, �� ��������� � ����������, �� ������� ��� ��������� ������ ������
		//
		/*
		if((ResCode != RESCODE_NO_ERROR) && (ResCode != RESCODE_UNKNOWNCOMMAND) && (ResCode != RESCODE_NO_CONNECTION) && (ResCode != RESCODE_SLIP_IS_EMPTY) &&
			(ResCode != RESCODE_INVEKLZSTATE) && (ResCode != RESCODE_MEMOVERFLOW) && (ResCode != RESCODE_GOTOCTO)) {
		*/
		if(!oneof7(ResCode, RESCODE_NO_ERROR, RESCODE_UNKNOWNCOMMAND, RESCODE_NO_CONNECTION, RESCODE_SLIP_IS_EMPTY, RESCODE_INVEKLZSTATE, RESCODE_MEMOVERFLOW, RESCODE_GOTOCTO)) {
			ok = 0;
			break;
		}
	} while((ok != 1) && (r > 0));
	CATCHZOK
	return ok;
}

int SLAPI SCS_SYNCCASH::ExecOper(int cmd, StrAssocArray & rIn, StrAssocArray & rOut)
{
	int    ok = 1, r = 0;
	THROW(ok = P_AbstrDvc->RunCmd__(cmd, rIn, rOut));
	if(ok == -1) {
		SString buf;
		THROW(rOut.Get(0, buf));
		ResCode = buf.ToLong();
		ok = 0;
	}
	else if((ok == 1) && (ResCode == RESCODE_NO_CONNECTION)) // ��� ������� �������� ������ ����� � ������� ��� ��� ��������� ���� ��� ������ �
													// �� ����������, ���� ����� ���������� ������� ���������������
													// ������ �� �������� ��������� � ���������� �����������, � ������ � ��� ������ ������
		ResCode = RESCODE_NO_ERROR;
	CATCHZOK
	return ok;
}

int SCS_SYNCCASH::SetLogotype()
{
	int ok = 1;
	SString str;
	SFile file;
	int64 fsize = 0;
	SImageBuffer img_buf;
	SlipDocCommonParam sdc_param;
	SlipLineParam sl_param;
	CCheckPacket p_pack;

	THROW(P_SlipFmt->Init("CCheck", &sdc_param));
	P_SlipFmt->InitIteration(&p_pack);
	P_SlipFmt->NextIteration(str, &sl_param);
	if(sl_param.PictPath.Empty()) {
		PrintLogo = 0;
	}
	else {
		PrintLogo = 1;
		THROW(img_buf.Load(sl_param.PictPath));
		uint height = img_buf.GetHeight();
		uint width = img_buf.GetWidth();
		THROW(file.Open(sl_param.PictPath, mRead|mBinary));
		if(file.IsValid()) {
			file.CalcSize(&fsize);
			file.Close();
		}
		THROW(ArrAdd(Arr_In, DVCPARAM_LOGOSIZE, (int)fsize));
		THROW(ArrAdd(Arr_In, DVCPARAM_LOGOTYPE, sl_param.PictPath));
		THROW(ArrAdd(Arr_In, DVCPARAM_LOGOWIDTH, (int)width));
		THROW(ArrAdd(Arr_In, DVCPARAM_LOGOHEIGHT, (int)height));
		THROW(ExecOper(DVCCMD_SETLOGOTYPE, Arr_In, Arr_Out));
		IsSetLogo = 1;
	}
	CATCHZOK;
    return ok;
}

int SLAPI SCS_SYNCCASH::PrintBnkTermReport(const char * pZCheck)
{
	int ok = 1;
	SlipDocCommonParam sdc_param;
	SString text, str;
	StringSet str_set('\n', pZCheck);
	Arr_In.Clear();
	Arr_Out.Clear();
	THROW(Connect());
	THROW(P_SlipFmt->Init("CCheck", &sdc_param));
	THROW(ArrAdd(Arr_In, DVCPARAM_CHECKTYPE, SERVICEDOC));
	THROW(ArrAdd(Arr_In, DVCPARAM_CHECKNUM, 0));
	THROW(ExecPrintOper(DVCCMD_OPENCHECK, Arr_In, Arr_Out));
	for(uint pos = 0; str_set.get(&pos, str) > 0;) {
		Arr_In.Clear();
		Arr_Out.Clear();
		THROW(ArrAdd(Arr_In, DVCPARAM_RIBBONPARAM, CHECKRIBBON));
		THROW(ArrAdd(Arr_In, DVCPARAM_FONTSIZE, DEF_FONTSIZE));
		THROW(ArrAdd(Arr_In, DVCPARAM_TEXT, str));
		THROW(ExecPrintOper(DVCCMD_PRINTTEXT, Arr_In, Arr_Out));
	}
	Arr_In.Clear();
	Arr_Out.Clear();
	THROW(ExecPrintOper(DVCCMD_CLOSECHECK, Arr_In, Arr_Out));
	CATCHZOK;
	return ok;
}