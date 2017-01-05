// V_QUOT.CPP
// Copyright (c) A.Sobolev 2003, 2004, 2005, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016
// @codepage windows-1251
//
#include <pp.h>
#pragma hdrstop
//
// @ModuleDef(PPViewQuot)
//
#define QUOT_BUF_SIZE    sizeof(((TempQuotTbl::Rec*)0)->Quot1)

IMPLEMENT_PPFILT_FACTORY(Quot); SLAPI QuotFilt::QuotFilt() : PPBaseFilt(PPFILT_QUOT, 0, 3) // @v6.2.6 ver 0-->1; @v6.4.2 ver 1-->2; @v7.3.5 2-->3
{
	SetFlatChunk(offsetof(QuotFilt, InitOrder),
		offsetof(QuotFilt, Reserve)-offsetof(QuotFilt, InitOrder)+sizeof(Reserve));
	SetBranchObjIdListFilt(offsetof(QuotFilt, LocList));
	Init(1, 0);
}

int SLAPI QuotFilt::ReadPreviosVer(SBuffer & rBuf, int ver)
{
	int    ok = -1;
	if(ver == 2) {
		struct QuotFilt_v2 : public PPBaseFilt {
			SLAPI  QuotFilt_v2::QuotFilt_v2() : PPBaseFilt(PPFILT_QUOT, 0, 2)
			{
				SetFlatChunk(offsetof(QuotFilt_v2, ReserveStart),
					offsetof(QuotFilt_v2, Reserve)-offsetof(QuotFilt_v2, ReserveStart)+sizeof(Reserve));
				SetBranchObjIdListFilt(offsetof(QuotFilt_v2, LocList));
				Init(1, 0);
			}
			char   ReserveStart[4];  // @anchor
			int32  QkCls;            // @v6.5.12 ����� ���� ���������
			DateRange Period;        // @v6.2.7 (quot2) ������ ��������
			PPID   QTaID;            // @v6.2.7 (quot2) �� ���������� ��������� ���������
			PPID   SellerID;         // @v6.2.6 ->Article.ID ��������
			PPID   SellerLocWorldID; // @v6.2.6 ->World.ID �� �������� World, �������� ������ ������������ ������� LocID
			PPID   BrandID;          // @v6.2.6
			PPID   LocID;            //
			PPID   QuotKindID;       //
			PPID   CurID;            //
			PPID   ArID;             //
			PPID   GoodsGrpID;       // if !0, ����� ��� ���� ������������ ������� ������� ������ ��������� �������
			PPID   GoodsID;          // if !0, �� ������ ���������� ���� �� �������,
				// ���� �� ��������, ���� �� ����� ���������
			RealRange Val;           // �������� �������� ��������� (0..0 - ������������)
			long   Flags;
			long   Reserve;          // @anchor �������� ��� ������ "��������" ������� �������
			ObjIdListFilt LocList;   // @v6.4.2 ������ �������
		};
		QuotFilt_v2 fv2;
		THROW(fv2.Read(rBuf, 0));
#define CPYFLD(f) f = fv2.f
		CPYFLD(QkCls);
		CPYFLD(Period);
		CPYFLD(QTaID);
		CPYFLD(SellerID);
		CPYFLD(SellerLocWorldID);
		CPYFLD(BrandID);
		CPYFLD(LocID);
		CPYFLD(QuotKindID);
		CPYFLD(CurID);
		CPYFLD(ArID);
		CPYFLD(GoodsGrpID);
		CPYFLD(GoodsID);
		CPYFLD(Val);
		CPYFLD(Flags);
		CPYFLD(LocList);
#undef CPYFLD
		ok = 1;
	}
	CATCHZOK
	return ok;
}

SLAPI PPViewQuot::PPViewQuot() : PPView(0, &Filt, PPVIEW_QUOT)
{
	P_Qc = 0;
	P_Qc2 = 0;
	if(CConfig.Flags2 & CCFLG2_QUOT2)
		P_Qc2 = new Quotation2Core;
	else
		P_Qc = new QuotationCore;
	P_BObj = BillObj;
	P_TempTbl = 0;
	P_TempSerTbl = 0;
	P_TempOrd = 0; // @v8.1.1
	P_GoodsSelDlg = 0;
	FirstQuotBrwColumn = 0;
	HasPeriodVal = 0;
	PPObjQuotKind::GetSpecialKinds(&Spc, 1);
}

SLAPI PPViewQuot::~PPViewQuot()
{
	ZDELETE(P_GoodsSelDlg);
	ZDELETE(P_TempTbl);
	ZDELETE(P_TempSerTbl);
	ZDELETE(P_TempOrd); // @v8.1.1
	ZDELETE(P_Qc);
	ZDELETE(P_Qc2);
}

const StrAssocArray & SLAPI PPViewQuot::GetQuotKindList() const
{
	return QuotKindList;
}
//
//
//
#define GRP_GOODSFILT 1
#define GRP_LOC       2

class QuotFiltDialog : public TDialog {
public:
	QuotFiltDialog() : TDialog(DLG_QUOTFLT)
	{
		Cls = PPQuot::clsGeneral;
		LastAccSheetID = 0;
		PPObjQuotKind::GetSpecialKinds(&Spc, 1);
		addGroup(GRP_GOODSFILT, new GoodsFiltCtrlGroup(CTLSEL_QUOTFLT_GOODS, CTLSEL_QUOTFLT_GGRP, cmGoodsFilt));
		addGroup(GRP_LOC, new LocationCtrlGroup(CTLSEL_QUOTFLT_LOC, 0, 0, cmLocList, 0, LocationCtrlGroup::fEnableSelUpLevel, 0));
		SetupCalDate(CTLCAL_QUOTFLT_EFFDATE, CTL_QUOTFLT_EFFDATE);
		SetupCalPeriod(CTLCAL_QUOTFLT_SETPRD, CTL_QUOTFLT_SETPRD); // @v8.4.8
	}
	int    setDTS(const QuotFilt *);
	int    getDTS(QuotFilt *);
private:
	DECL_HANDLE_EVENT;
	int    SetupCtrls();

	PPObjQuotKind::Special Spc;
	int    Cls;
	PPID   LastAccSheetID;
	QuotFilt Data;
};

IMPL_HANDLE_EVENT(QuotFiltDialog)
{
	TDialog::handleEvent(event);
	if(event.isClusterClk(CTL_QUOTFLT_TYPE)) {
		GetClusterData(CTL_QUOTFLT_TYPE, &Data.QkCls);
		SetupCtrls();
	}
	else if(event.isClusterClk(CTL_QUOTFLT_ABSENCE)) {
		GetClusterData(CTL_QUOTFLT_ABSENCE, &Data.Flags);
		SetupCtrls();
	}
	else if(TVCOMMAND && (event.isCbSelected(CTLSEL_QUOTFLT_QK) || event.isCbSelected(CTLSEL_QUOTFLT_CLIENT))) {
		GetClusterData(CTL_QUOTFLT_TYPE, &Data.QkCls);
		SetupCtrls();
	}
	else
		return;
	clearEvent(event);
}

int QuotFiltDialog::SetupCtrls()
{
	int    disable_qk = 0;
	long   combo_ext = 0;
	int    prev_cls = Data.QkCls;
	if(!Spc.MtxID) {
		DisableClusterItem(CTL_QUOTFLT_TYPE, 2);
		if(Data.QkCls == PPQuot::clsMtx)
			Data.QkCls = PPQuot::clsGeneral;
	}
	if(!Spc.MtxRestrID) {
		DisableClusterItem(CTL_QUOTFLT_TYPE, 3);
		if(Data.QkCls == PPQuot::clsMtxRestr)
			Data.QkCls = PPQuot::clsGeneral;
	}
	if(!Spc.PredictCoeffID) {
		DisableClusterItem(CTL_QUOTFLT_TYPE, 4);
		if(Data.QkCls == PPQuot::clsPredictCoeff)
			Data.QkCls = PPQuot::clsGeneral;
	}
	if(!Spc.SupplDealID && !Spc.SupplDevUpID && !Spc.SupplDevDnID) {
		DisableClusterItem(CTL_QUOTFLT_TYPE, 1);
		if(Data.QkCls == PPQuot::clsSupplDeal)
			Data.QkCls = PPQuot::clsGeneral;
	}
	if(Data.QkCls != prev_cls)
		SetClusterData(CTL_QUOTFLT_TYPE, Data.QkCls);
	if(Data.QkCls == PPQuot::clsGeneral) {
		getCtrlData(CTLSEL_QUOTFLT_QK,     &Data.QuotKindID);
		getCtrlData(CTLSEL_QUOTFLT_CLIENT, &Data.ArID);
	}
	else {
		if(Data.QkCls == PPQuot::clsSupplDeal) {
			getCtrlData(CTLSEL_QUOTFLT_QK,     &Data.QuotKindID);
			getCtrlData(CTLSEL_QUOTFLT_CLIENT, &Data.ArID);
			/*
			if(!Data.QuotKindID || !oneof3(Data.QuotKindID, Spc.SupplDealID, Spc.SupplDevUpID, Spc.SupplDevDnID))
				Data.QuotKindID = Spc.SupplDealID;
			*/
			combo_ext = QuotKindFilt::fSupplDeal;
		}
		else if(Data.QkCls == PPQuot::clsMtx) {
			Data.QuotKindID = Spc.MtxID;
			combo_ext = QuotKindFilt::fGoodsMatrix;
			disable_qk = 1;
			Data.Flags |= QuotFilt::fCrosstab;
		}
		else if(Data.QkCls == PPQuot::clsMtxRestr) {
			Data.QuotKindID = Spc.MtxRestrID;
			combo_ext = QuotKindFilt::fGoodsMatrixRestrict;
			disable_qk = 1;
			Data.Flags |= QuotFilt::fCrosstab;
		}
		else if(Data.QkCls == PPQuot::clsPredictCoeff) {
			Data.QuotKindID = Spc.PredictCoeffID;
			combo_ext = QuotKindFilt::fPredictCoeff;
			disable_qk = 1;
			Data.Flags |= QuotFilt::fCrosstab;
		}
	}
	SetupPPObjCombo(this, CTLSEL_QUOTFLT_QK, PPOBJ_QUOTKIND, Data.QuotKindID, 0, (void *)combo_ext);
	disableCtrl(CTLSEL_QUOTFLT_QK, disable_qk);
	DisableClusterItem(CTL_QUOTFLT_FLAGS, 0, !getCtrlLong(CTLSEL_QUOTFLT_QK));
	if(Data.Flags & QuotFilt::fOnlyAbsence)
		Data.Flags &= ~QuotFilt::fAbsence;
	DisableClusterItem(CTL_QUOTFLT_ABSENCE, 1, Data.Flags & QuotFilt::fOnlyAbsence);
	{
		PPID   acs_id = 0;
		PPObjQuotKind qk_obj;
		PPQuotKind qk_rec;
		if(qk_obj.Fetch(Data.QuotKindID, &qk_rec) > 0)
			acs_id = qk_rec.AccSheetID;
		SETIFZ(acs_id, PPObjQuotKind::GetDefaultAccSheetID(Data.QkCls));
		if(acs_id != LastAccSheetID) {
			if(Data.ArID) {
				PPID   acs2_id = 0;
				GetArticleSheetID(Data.ArID, &acs2_id);
				if(acs2_id != acs_id)
					Data.ArID = 0;
			}
			SetupArCombo(this, CTLSEL_QUOTFLT_CLIENT, Data.ArID, LastAccSheetID ? 0 : OLW_LOADDEFONOPEN, acs_id, sacfDisableIfZeroSheet);
			LastAccSheetID = acs_id;
		}
	}
	return 1;
}

int QuotFiltDialog::setDTS(const QuotFilt * pData)
{
	Data = *pData;
	AddClusterAssoc(CTL_QUOTFLT_TYPE, -1, PPQuot::clsGeneral);
	AddClusterAssoc(CTL_QUOTFLT_TYPE,  0, PPQuot::clsGeneral);
	AddClusterAssoc(CTL_QUOTFLT_TYPE,  1, PPQuot::clsSupplDeal);
	AddClusterAssoc(CTL_QUOTFLT_TYPE,  2, PPQuot::clsMtx);
	AddClusterAssoc(CTL_QUOTFLT_TYPE,  3, PPQuot::clsMtxRestr);
	AddClusterAssoc(CTL_QUOTFLT_TYPE,  4, PPQuot::clsPredictCoeff);
	SetClusterData(CTL_QUOTFLT_TYPE, Data.QkCls);
	SetupPPObjCombo(this, CTLSEL_QUOTFLT_QK,  PPOBJ_QUOTKIND, Data.QuotKindID, 0, (void *)1);
	SetupPPObjCombo(this, CTLSEL_QUOTFLT_CUR, PPOBJ_CURRENCY, Data.CurID, 0);
	AddClusterAssoc(CTL_QUOTFLT_ALLLOC, 0, QuotFilt::fAllLocations);
	SetClusterData(CTL_QUOTFLT_ALLLOC, Data.Flags);
	AddClusterAssoc(CTL_QUOTFLT_ABSENCE, 0, QuotFilt::fOnlyAbsence);
	AddClusterAssoc(CTL_QUOTFLT_ABSENCE, 1, QuotFilt::fAbsence);
	SetClusterData(CTL_QUOTFLT_ABSENCE, Data.Flags);
	AddClusterAssoc(CTL_QUOTFLT_FLAGS, 0, QuotFilt::fCrosstab);
	SetClusterData(CTL_QUOTFLT_FLAGS, Data.Flags);
	// @v8.6.6 {
	{
		long  ggd = CHKXORFLAGS(Data.Flags, QuotFilt::fByGoodsOnly, QuotFilt::fByGroupOnly);
		AddClusterAssoc(CTL_QUOTFLT_GGRPDIFF, 0, 0);
		AddClusterAssoc(CTL_QUOTFLT_GGRPDIFF, 1, QuotFilt::fByGoodsOnly);
		AddClusterAssoc(CTL_QUOTFLT_GGRPDIFF, 2, QuotFilt::fByGroupOnly);
		SetClusterData(CTL_QUOTFLT_GGRPDIFF, ggd);
	}
	// } @v8.6.6
	// @v8.1.1 {
	AddClusterAssoc(CTL_QUOTFLT_ORDER, 0, PPViewQuot::OrdByDefault);
	AddClusterAssoc(CTL_QUOTFLT_ORDER, -1, PPViewQuot::OrdByDefault);
	AddClusterAssoc(CTL_QUOTFLT_ORDER, 1, PPViewQuot::OrdByGoods);
	AddClusterAssoc(CTL_QUOTFLT_ORDER, 2, PPViewQuot::OrdByArGoods);
	SetClusterData(CTL_QUOTFLT_ORDER, Data.InitOrder);
	// } @v8.1.1
	SetRealRangeInput(this, CTL_QUOTFLT_VALRANGE, &Data.Val);
	GoodsFiltCtrlGroup::Rec rec(Data.GoodsGrpID, Data.GoodsID, 0, GoodsCtrlGroup::enableSelUpLevel);
	setGroupData(GRP_GOODSFILT, &rec);
	LocationCtrlGroup::Rec loc_rec(&Data.LocList);
	setGroupData(GRP_LOC, &loc_rec);
	setCtrlDate(CTL_QUOTFLT_EFFDATE, Data.EffDate); // @v7.3.5
	SetPeriodInput(this, CTL_QUOTFLT_SETPRD, &Data.Period); // @v8.4.8
	SetupCtrls();
	return 1;
}

int QuotFiltDialog::getDTS(QuotFilt * pData)
{
	int    ok = 1;
	GoodsFiltCtrlGroup::Rec rec;
	LocationCtrlGroup::Rec loc_rec;
	getCtrlData(CTLSEL_QUOTFLT_QK,  &Data.QuotKindID);
	getCtrlData(CTLSEL_QUOTFLT_CUR, &Data.CurID);
	getCtrlData(CTLSEL_QUOTFLT_CLIENT,    &Data.ArID);
	GetClusterData(CTL_QUOTFLT_TYPE,      &Data.QkCls);
	GetClusterData(CTL_QUOTFLT_ALLLOC,    &Data.Flags);
	GetClusterData(CTL_QUOTFLT_ABSENCE,   &Data.Flags);
	GetClusterData(CTL_QUOTFLT_FLAGS,     &Data.Flags);
	// @v8.6.6 {
	{
		long   ggd = GetClusterData(CTL_QUOTFLT_GGRPDIFF);
		Data.Flags &= ~(QuotFilt::fByGoodsOnly|QuotFilt::fByGroupOnly);
		Data.Flags |= ggd;
	}
	// } @v8.6.6
	GetClusterData(CTL_QUOTFLT_ORDER,     &Data.InitOrder); // @v8.1.1
	GetRealRangeInput(this, CTL_QUOTFLT_VALRANGE, &Data.Val);
	getGroupData(GRP_GOODSFILT, &rec);
	Data.GoodsGrpID = rec.GoodsGrpID;
	Data.GoodsID = rec.GoodsID;
	Data.EffDate = getCtrlDate(CTL_QUOTFLT_EFFDATE); // @v7.3.5
	GetPeriodInput(this, CTL_QUOTFLT_SETPRD, &Data.Period); // @v8.4.8
	getGroupData(GRP_LOC, &loc_rec);
	Data.LocList = loc_rec.LocList;
	if(!Data.QuotKindID)
		Data.Flags &= ~QuotFilt::fCrosstab;
	if(Data.Flags & QuotFilt::fOnlyAbsence)
		Data.Flags &= ~QuotFilt::fAbsence;
	ASSIGN_PTR(pData, Data);
	return ok;
}

int SLAPI PPViewQuot::EditBaseFilt(PPBaseFilt * pFilt)
{
	if(!Filt.IsA(pFilt))
		return 0;
	return PPDialogProcBody <QuotFiltDialog, QuotFilt> ((QuotFilt *)pFilt);
}

static char * FASTCALL GetQuotTblBuf(const TempQuotTbl::Rec * pRec, uint pos)
{
	if(pos < MAX_QUOTS_PER_TERM_REC) {
		size_t offs = offsetof(TempQuotTbl::Rec, Quot1);
		return ((char*)pRec) + offs + pos * QUOT_BUF_SIZE;
	}
	else
		return 0;
}

int SLAPI PPViewQuot::CreateCrosstab(int useTa)
{
	PPWait(1);
	class QuotCrosstab : public Crosstab {
	public:
		SLAPI  QuotCrosstab(PPViewQuot * pV) : Crosstab()
		{
			P_V = pV;
		}
		virtual BrowserWindow * SLAPI CreateBrowser(uint brwId, int dataOwner)
		{
			PPViewBrowser * p_brw = new PPViewBrowser(brwId, CreateBrowserQuery(), P_V, dataOwner);
			SetupBrowserCtColumns(p_brw);
			return p_brw;
		}
	private:
		virtual int SLAPI GetTabTitle(const void * pVal, TYPEID typ, SString & rBuf) const
		{
			return (pVal && /*typ == MKSTYPE(S_INT, 4) &&*/ P_V) ? P_V->GetTabTitle(*(const long *)pVal, rBuf) : 0;
		}
		PPViewQuot * P_V;
	};
	//
	// ���������� �������� ����� ����� ��������� ������ ����� ��������� �������� ������.
	// � ��������� ������ ��������� �������� �������. ��������, ����� ������� ���������
	// ��������� � ��������������� ����� ��, ��� � ������������. � ���������� ���������� ����������
	// ���������� ������ (��-�� ����, ��� ����� ������� ��������� ��� ���� ������������).
	//
	int    ok = -1;
	Crosstab * p_prev_ct = P_Ct;
	P_Ct = 0;
	if(Filt.Flags & QuotFilt::fCrosstab) {
		uint   fld_pos = 4; // Quot1
		DBField quot_fld;
		SString temp_buf;
		DBFieldList total_list;
		THROW_MEM(P_Ct = new QuotCrosstab(this));
		P_Ct->SetTable(P_TempTbl, P_TempTbl->LocID);
		P_Ct->AddIdxField(P_TempTbl->GoodsID);
		P_Ct->SetSortIdx("GoodsName", 0);
		P_Ct->AddInheritedFixField(P_TempTbl->GoodsName);
		for(uint i = 0; i < MAX_QUOTS_PER_TERM_REC && i < QuotKindList.getCount(); i++, fld_pos++) {
			if(Filt.QkCls == PPQuot::clsGeneral) {
				if(QuotKindList.at(i).Id == Filt.QuotKindID)
					break;
			}
			else if(Filt.QkCls == PPQuot::clsMtx) {
				if(QuotKindList.at(i).Id == Spc.MtxID)
					break;
			}
			else if(Filt.QkCls == PPQuot::clsMtxRestr) {
				if(QuotKindList.at(i).Id == Spc.MtxRestrID)
					break;
			}
			else if(Filt.QkCls == PPQuot::clsPredictCoeff) {
				if(QuotKindList.at(i).Id == Spc.PredictCoeffID)
					break;
			}
		}
		P_TempTbl->getField(fld_pos, &quot_fld);
		P_Ct->AddAggrField(quot_fld);
		THROW(P_Ct->Create(useTa));
	}
	ZDELETE(p_prev_ct);
	CATCH
		ok = 0;
		ZDELETE(P_Ct);
	ENDCATCH
	PPWait(0);
	return ok;
}

const PPQuotItemArray * SLAPI PPViewQuot::GetQList() const
{
	return &QList_;
}

PP_CREATE_TEMP_FILE_PROC(CreateTempFile, TempQuot);
PP_CREATE_TEMP_FILE_PROC(CreateTempSerFile, TempQuotSerial);

int SLAPI PPViewQuot::Init_(const PPBaseFilt * pFilt)
{
	int    ok = 1, use_ta = 1;
	SString temp_buf, msg_buf;
	THROW(Helper_InitBaseFilt(pFilt));
	HasPeriodVal = 0;
	QList_.freeAll();
	QList_.setDelta(1024);
	ZDELETE(P_TempTbl);
	ZDELETE(P_TempOrd);
	{
		QuotKindList.Clear();
		QuotKindFilt filt;
		if(Filt.QkCls == PPQuot::clsSupplDeal)
			filt.Flags |= QuotKindFilt::fSupplDeal;
		else if(Filt.QkCls == PPQuot::clsMtx)
			filt.Flags |= QuotKindFilt::fGoodsMatrix;
		else if(Filt.QkCls == PPQuot::clsMtxRestr)
			filt.Flags |= QuotKindFilt::fGoodsMatrixRestrict;
		else if(Filt.QkCls == PPQuot::clsPredictCoeff)
			filt.Flags |= QuotKindFilt::fPredictCoeff;
		else
			filt.AccSheetID = GetSellAccSheet();
		QkObj.MakeList(&filt, &QuotKindList);
		if(Filt.QkCls == PPQuot::clsMtxRestr) {
			PPGetWord(PPWORD_ACCESSIBLE, 0, temp_buf);
			QuotKindList.Add(0, 0, temp_buf);
		}
		else if(Filt.QkCls == PPQuot::clsGeneral && Filt.QuotKindID != 0) {
			for(long i = (long)QuotKindList.getCount(); i >= 0; i--)
				if(QuotKindList.at(i).Id != Filt.QuotKindID)
					QuotKindList.atFree(i);
		}
	}
	if(Filt.IsSeries() && P_Qc2) {
		PPIDArray rel_list;
		PPQuotArray qlist;
		P_Qc2->GetRelListByFilt(&Filt, &rel_list);
		rel_list.sort();
		{
			for(uint i = 0; i < rel_list.getCount(); i++) {
				const PPID rel_id = rel_list.get(i);
				qlist.clear();
				P_Qc2->Get(Filt.GoodsID, rel_id, 0, &qlist);
				for(uint j = 0; j < qlist.getCount(); j++) {
					PPQuotItem_ item(qlist.at(j));
					THROW_SL(QList_.insert(&item));
				}
			}
			if(!(Filt.Flags & QuotFilt::fListOnly)) {
				THROW(P_TempSerTbl = CreateTempSerFile());
				BExtInsert bei(P_TempSerTbl);
				{
					PPTransaction tra(ppDbDependTransaction, use_ta);
					THROW(tra);
					for(uint i = 0; i < QList_.getCount(); i++) {
						const PPQuotItem_ & r_item = QList_.at(i);
						TempQuotSerialTbl::Rec ts_rec;
						MEMSZERO(ts_rec);
						ts_rec.GoodsID = r_item.GoodsID;
						ts_rec.QuotKindID = r_item.KindID;
						ts_rec.LocID = r_item.LocID;
						ts_rec.ArticleID = r_item.ArID;
						ts_rec.Dt = r_item.Dtm.d;
						ts_rec.Tm = r_item.Dtm.t;
						ts_rec.Val = r_item.Val;
						ts_rec.ValF = r_item.Flags;
						PPQuot::PutValToStr(r_item.Val, r_item.Flags, temp_buf).CopyTo(ts_rec.ValS, sizeof(ts_rec.ValS));
						THROW_DB(bei.insert(&ts_rec));
					}
					THROW_DB(bei.flash());
					THROW(tra.Commit());
				}
			}
		}
	}
	else {
		PPWait(1);
		if(!(Filt.Flags & QuotFilt::fListOnly)) {
			THROW(P_TempTbl = CreateTempFile());
		}
		if(Filt.Flags & (QuotFilt::fAbsence|QuotFilt::fOnlyAbsence)) {
			Goods2Tbl::Rec goods_rec;
			QuotIdent qi(Filt.LocList.GetSingle(), Filt.QuotKindID, Filt.CurID, Filt.ArID);
			PPQuotItemArray temp_list;
			PPQuotArray quot_list;
			PPTransaction tra(ppDbDependTransaction, use_ta);
			THROW(tra);
			PPLoadText(PPTXT_WAIT_QUOTVIEWBUILDING, msg_buf);
			for(GoodsIterator iter(Filt.GoodsGrpID, 0); iter.Next(&goods_rec) > 0; PPWaitPercent(iter.GetIterCounter(), msg_buf)) {
				uint   pos = 0;
				quot_list.clear();
				GObj.GetQuotList(goods_rec.ID, 0, quot_list);
				int    exists = 0;
				if(!Filt.QuotKindID && Filt.LocList.IsEmpty() && !Filt.CurID && !Filt.ArID && !quot_list.getCount())
					exists = 1;
				else {
					PPQuot * p_quot;
					for(uint i = 0; !exists && quot_list.enumItems(&i, (void **)&p_quot);) {
						int    e_qk = 0, e_loc = 0, e_cur = 0, e_ar = 0;
						int    sum = 0;
						if(Filt.QuotKindID) {
							e_qk = BIN(p_quot->Kind == Filt.QuotKindID);
							sum++;
						}
						if(!Filt.IsEmpty() || (Filt.Flags & QuotFilt::fAllLocations)) {
							e_loc = Filt.IsEmpty() ? BIN(p_quot->LocID == 0L) : BIN(Filt.LocList.CheckID(p_quot->LocID));
							sum++;
						}
						if(Filt.CurID) {
							e_cur = BIN(p_quot->CurID == Filt.CurID);
							sum++;
						}
						if(Filt.ArID) {
							e_ar = BIN(p_quot->ArID == Filt.ArID);
							sum++;
						}
						// @v7.1.12 {
						else if(Filt.Flags & QuotFilt::fZeroArOnly) {
							e_ar = BIN(p_quot->ArID == 0L);
							sum++;
						}
						// } @v7.1.12
						if((e_qk + e_loc + e_cur + e_ar) == sum)
							exists = 1;
					}
				}
				if(!exists || !(Filt.Flags & QuotFilt::fOnlyAbsence)) {
					// @v7.4.0 {
					if(exists) {
						QuotFilt temp_filt = Filt;
						temp_filt.GoodsID = goods_rec.ID;
						temp_list.clear();
						THROW(Helper_CreateTmpTblEntries(&temp_filt, (Filt.Flags & QuotFilt::fListOnly) ? &QList_ : &temp_list, 0));
					}
					else {
						// } @v7.4.0
						PPQuotItem_ item_;
						item_.GoodsID = goods_rec.ID;
						THROW_SL(QList_.insert(&item_));
					} // @v7.4.0
				}
			}
			if(!(Filt.Flags & QuotFilt::fListOnly)) {
				BExtInsert bei(P_TempTbl);
				for(uint i = 0; i < QList_.getCount(); i++) {
					const PPQuotItem_ & r_item = QList_.at(i);
					TempQuotTbl::Rec rec;
					MEMSZERO(rec);
					rec.GoodsID  = r_item.GoodsID;
					if(rec.GoodsID) {
						STRNSCPY(rec.GoodsName, GetGoodsName(rec.GoodsID, temp_buf));
						THROW_DB(bei.insert(&rec));
					}
				}
				THROW_DB(bei.flash());
			}
			// @v8.3.6 @fix {
			if(!(Filt.Flags & QuotFilt::fCrosstab)) {
				THROW(CreateOrderTable((IterOrder)Filt.InitOrder, &P_TempOrd));
			}
			// } @v8.3.6 @fix
			THROW(tra.Commit());
		}
		else {
			THROW(Helper_CreateTmpTblEntries(&Filt, 0, 1));
			if(!(Filt.Flags & QuotFilt::fCrosstab)) {
				THROW(CreateOrderTable((IterOrder)Filt.InitOrder, &P_TempOrd));
			}
		}
		THROW(CreateCrosstab(1));
	}
	CATCH
		ZDELETE(P_TempTbl);
		ZDELETE(P_TempSerTbl);
		ZDELETE(P_TempOrd);
		ok = 0;
	ENDCATCH
	PPWait(0);
	return ok;
}

int SLAPI PPViewQuot::MakeOrderEntry(IterOrder ord, const TempQuotTbl::Rec & rSrcRec, TempOrderTbl::Rec & rDestRec)
{
	static const char * p_fmt = "%04.0lf%030.8lf";
	int    ok = 1;
	SString temp_buf, goods_name;
	MEMSZERO(rDestRec);
	rDestRec.ID = rSrcRec.ID__;
	if(oneof2(ord, OrdByDefault, OrdByGoods)) {
		GetGoodsName(rSrcRec.GoodsID, temp_buf);
	}
	else if(ord == OrdByArGoods) {
		if(rSrcRec.ArticleID)
			GetArticleName(rSrcRec.ArticleID, temp_buf);
		else
			(temp_buf = 0).CatChar('\xFF');
		GetGoodsName(rSrcRec.GoodsID, goods_name);
		while((temp_buf.Len() + goods_name.Len() + 1) > sizeof(rDestRec.Name)) {
			if(temp_buf.Len() > goods_name.Len())
				temp_buf.TrimRight();
			else
				goods_name.TrimRight();
		}
		temp_buf.Cat(goods_name);
	}
	temp_buf.CopyTo(rDestRec.Name, sizeof(rDestRec.Name));
	return ok;
}

int SLAPI PPViewQuot::CreateOrderTable(IterOrder ord, TempOrderTbl ** ppTbl)
{
	int    ok = 1;
	TempOrderTbl * p_o = 0;
	*ppTbl = 0;
	if(P_TempTbl && oneof3(ord, OrdByDefault, OrdByArGoods, OrdByGoods)) {
		TempQuotTbl * p_t = P_TempTbl;
		BExtQuery q(P_TempTbl, 0, 64);
		THROW(p_o = CreateTempOrderFile());
		{
			SString msg_buf;
			PPLoadText(PPTXT_WAIT_ORDERBUILDING, msg_buf);
			IterCounter _cntr;
			BExtInsert bei(p_o);
			q.select(p_t->ID__, p_t->GoodsID, p_t->ArticleID, p_t->LocID, 0L);
			TempQuotTbl::Key0 k, k_;
			MEMSZERO(k);
			k_ = k;
			_cntr.Init(q.countIterations(0, &k_, spFirst));
			for(q.initIteration(0, &k, spFirst); q.nextIteration() > 0;) {
				TempOrderTbl::Rec ord_rec;
				MakeOrderEntry(ord, p_t->data, ord_rec);
				THROW_DB(bei.insert(&ord_rec));
				PPWaitPercent(_cntr.Increment(), msg_buf);
			}
			THROW_DB(bei.flash());
		}
		*ppTbl = p_o;
		p_o = 0;
	}
	else
		ok = -1;
	CATCHZOK
	delete p_o;
	return ok;
}

int SLAPI PPViewQuot::InitIteration()
{
	int    ok = 1;
	int    idx = 1;
	TempQuotTbl * t = P_TempTbl;
	TempQuotTbl::Key1 k1; /*k1_*/;
	ZDELETE(P_IterQuery);
	Counter.Init();
	THROW_MEM(P_IterQuery = new BExtQuery(t, idx));
	P_IterQuery->select(t->GoodsID, t->LocID, t->ArticleID, t->PeriodIdx,
		t->Quot1, t->Quot2, t->Quot3, t->Quot4, t->Quot5, t->Quot6, t->Quot7,
		t->Quot8, t->Quot9, t->Quot10, t->Quot11, t->Quot12, t->Quot13,
		t->Quot14, t->Quot15, t->Quot16, 0L);
	MEMSZERO(k1);
	// @v7.0.14 k1_ = k1;
	// @v7.0.14 Counter.Init(P_IterQuery->countIterations(0, &k1_, spFirst));
	Counter.Init(t); // @v7.0.14
	P_IterQuery->initIteration(0, &k1, spFirst);
	CATCHZOK
	return ok;
}

int SLAPI PPViewQuot::NextIteration(QuotViewItem * pItem)
{
	int    ok = -1;
	if(pItem)
		memzero(pItem, sizeof(*pItem));
	if(P_IterQuery && P_IterQuery->nextIteration() > 0) {
		if(pItem) {
			pItem->GoodsID   = P_TempTbl->data.GoodsID;
			pItem->LocID     = P_TempTbl->data.LocID;
			pItem->ArticleID = P_TempTbl->data.ArticleID;
			for(uint i = 0; i < MAX_QUOTS_PER_TERM_REC; i++)
				STRNSCPY(pItem->Quots[i], GetQuotTblBuf(&P_TempTbl->data, i));
		}
		Counter.Increment();
		ok = 1;
	}
	return ok;
}

struct VQuotEntry {
	PPID   GoodsID;
	PPID   LocID;
	PPID   ArID;
	uint   Qp[MAX_QUOTS_PER_TERM_REC];
};

IMPL_CMPFUNC(VQuotEntry, i1, i2)
{
	int    si;
	CMPCASCADE3(si, (VQuotEntry *)i1, (VQuotEntry *)i2, GoodsID, LocID, ArID);
	return si;
}

class VQuotCache : public TSArray <VQuotEntry> {
public:
	SLAPI  VQuotCache() : TSArray <VQuotEntry>()
	{
		MaxQuotPos = 0;
		ValPool.add("$", 0); // zero index - is empty string
	}
	int    SLAPI Add(const TempQuotTbl::Rec *, uint quotPos);
	// @v8.1.1 @unused int    SLAPI PrepareForWriting();
	int    SLAPI GetRec(uint i, TempQuotTbl::Rec * pRec) const;
	int    SLAPI Clear();
private:
	uint   MaxQuotPos;
	PPObjGoods   GObj;
	StrAssocArray GoodsNameList;
	StringSet ValPool;
};

int SLAPI VQuotCache::Add(const TempQuotTbl::Rec * pRec, uint quotPos)
{
	int    ok = 1;
	uint   p = 0;
	VQuotEntry entry;
	SETMAX(MaxQuotPos, quotPos);
	MEMSZERO(entry);
	entry.GoodsID = pRec->GoodsID;
	entry.LocID = pRec->LocID;
	entry.ArID = pRec->ArticleID;
	const  size_t offs = offsetof(VQuotEntry, Qp);
	if(lsearch(&entry, &p, PTR_CMPFUNC(VQuotEntry))) {
		VQuotEntry & r_entry = at(p);
		ValPool.add(GetQuotTblBuf(pRec, quotPos), ((uint *)(PTR8(&r_entry)+offs))+quotPos);
		ok = 2;
	}
	else {
		ValPool.add(GetQuotTblBuf(pRec, quotPos), ((uint *)(PTR8(&entry)+offs))+quotPos);
		ok = insert(&entry) ? 1 : PPSetErrorSLib();
	}
	return ok;
}

/* @v8.1.1 @unused
int SLAPI VQuotCache::PrepareForWriting()
{
	int    ok = 1;
	PPIDArray goods_id_list, ar_id_list;
	VQuotEntry * p_entry;
	for(uint i = 0; enumItems(&i, (void **)&p_entry);) {
		const PPID ar_id = p_entry->ArID;
		if(p_entry->GoodsID)
			THROW(goods_id_list.addUnique(labs(p_entry->GoodsID)));
	}
	//
	// ���������� ������� ���� �������
	//
	THROW(GObj.P_Tbl->LoadNameList(&goods_id_list, 0, &GoodsNameList));
	GoodsNameList.SortByID();
	CATCHZOK
	return ok;
}
*/

int SLAPI VQuotCache::Clear()
{
	GoodsNameList.Clear();
	ValPool.clear(1);
	clear();
	return 1;
}

int SLAPI VQuotCache::GetRec(uint i, TempQuotTbl::Rec * pRec) const
{
	int    ok = 1;
	MEMSZERO(*pRec);
	if(i < getCount()) {
		const VQuotEntry & r_entry = at(i);
		SString temp_buf;
		pRec->GoodsID = r_entry.GoodsID;
		pRec->LocID = r_entry.LocID;
		pRec->ArticleID = r_entry.ArID;
		for(uint j = 0; j < MAX_QUOTS_PER_TERM_REC; j++) {
			ValPool.getnz(r_entry.Qp[j], temp_buf);
			temp_buf.CopyTo(GetQuotTblBuf(pRec, j), QUOT_BUF_SIZE);
		}
		if(!GoodsNameList.Get(labs(pRec->GoodsID), temp_buf))
			if(pRec->GoodsID)
				ideqvalstr(labs(pRec->GoodsID), temp_buf);
		temp_buf.CopyTo(pRec->GoodsName, sizeof(pRec->GoodsName));
	}
	else
		ok = 0;
	return ok;
}
//
//
//
IMPL_CMPFUNC(PPQuotItem_GLA, i1, i2)
{
	int    si = 0;
	CMPCASCADE3(si, (PPQuotItem_ *)i1, (PPQuotItem_ *)i2, GoodsID, LocID, ArID);
	if(si == 0) {
		int32  _pidx1 = 0, _pidx2 = 0;
		Quotation2Core::PeriodToPeriodIdx(&((PPQuotItem_ *)i1)->Period, &_pidx1);
		Quotation2Core::PeriodToPeriodIdx(&((PPQuotItem_ *)i2)->Period, &_pidx2);
		si = CMPSIGN(_pidx1, _pidx2);
	}
	return si;
}

int SLAPI PPViewQuot::Helper_RemoveTempRecsByGoodsID(PPID goodsID, int use_ta)
{
	int    ok = 1;
	if(P_TempTbl) {
		TempQuotTbl::Key1 tk1;
		MEMSZERO(tk1);
		PPTransaction tra(use_ta);
		THROW(tra);
        tk1.GoodsID = goodsID;
        if(P_TempTbl->search(1, &tk1, spGe) && P_TempTbl->data.GoodsID == goodsID) do {
        	const PPID id__ = P_TempTbl->data.ID__;
            THROW_DB(P_TempTbl->deleteRec());
			THROW(RemoveByID(P_TempOrd, id__, 0));
        } while(P_TempTbl->search(1, &tk1, spNext) && P_TempTbl->data.GoodsID == goodsID);
		//THROW_DB(deleteFrom(P_TempTbl, 0, P_TempTbl->GoodsID == goodsID));
		THROW(tra.Commit());
	}
	else
		ok = -1;
	CATCHZOK
	return ok;
}

int FASTCALL PPViewQuot::CheckGoodsKindDiffRestriction(PPID goodsID)
{
	int    ok = 1;
	const  long  _ggd = CHKXORFLAGS(Filt.Flags, QuotFilt::fByGoodsOnly, QuotFilt::fByGroupOnly);
	if(_ggd) {
		Goods2Tbl::Rec goods_rec;
		if(GObj.Fetch(goodsID, &goods_rec) > 0) {
			if(_ggd == QuotFilt::fByGroupOnly && goods_rec.Kind != PPGDSK_GROUP)
				ok = 0;
			else if(_ggd == QuotFilt::fByGoodsOnly && goods_rec.Kind != PPGDSK_GOODS)
				ok = 0;
		}
		else
			ok = 0;
	}
	return ok;
}

int SLAPI PPViewQuot::Helper_CreateTmpTblEntries(const QuotFilt * pFilt, PPQuotItemArray * pQList, int use_ta)
{
	int    ok = 1, is_added = 0;
	int    done = 0;
	SString temp_buf, msg_buf;
	PPIDArray loc_list, goods_list, temp_goods_list;
	int    use_goods_list = 0;
	DBQ  * dbq = 0;
	PPID   prev_goods_id = 0;
	PPObjGoodsGroup gg_obj;
	PPIDArray qk_id_list;
	TempOrderTbl::Rec ord_rec;
	SETIFZ(pQList, &QList_);
	if(pFilt->QuotKindID)
		qk_id_list.add(pFilt->QuotKindID);
	else
		QuotKindList.GetListByParent(0, 0, qk_id_list);
	PPID   filt_group_as_goods = 0;
	if(pFilt->GoodsID) {
		Goods2Tbl::Rec goods_rec;
		if(gg_obj.Fetch(pFilt->GoodsID, &goods_rec) > 0 && goods_rec.Kind == PPGDSK_GROUP) {
			filt_group_as_goods = pFilt->GoodsID;
		}
	}
	const PPID filt_group_id = NZOR(filt_group_as_goods, pFilt->GoodsGrpID);
	if((pFilt->GoodsGrpID && !pFilt->GoodsID) || filt_group_as_goods) {
		GoodsIterator::GetListByGroup(filt_group_id, &goods_list);
		{
			//
			// ��������� � ������ ������� ������, ������������� ������ pFilt->GoodsGrpID
			// (� ��� ���� ����� ���� ��������� ���������).
			//
			StrAssocArray * p_list = gg_obj.MakeStrAssocList((void *)filt_group_id);
			if(p_list) {
				PPIDArray grp_list;
				p_list->GetListByParent(filt_group_id, 1, grp_list);
				for(uint i = 0; i < grp_list.getCount(); i++)
					goods_list.add(grp_list.get(i)); // @v8.1.1 addUnique-->add
			}
			ZDELETE(p_list);
			goods_list.add(filt_group_id); // @v8.1.1 addUnique-->add
		}
		goods_list.sortAndUndup(); // @v8.1.1 sort-->sortAndUndup
		use_goods_list = 1;
		if(goods_list.getCount() == 0)
			done = 1;
	}
	// @Muxa v7.5.10 {
	if(pFilt->BrandID && !pFilt->GoodsID) {
		if(use_goods_list) {
			PPIDArray temp_list;
			Goods2Tbl::Rec goods_rec;
			for(uint i = 0; i < goods_list.getCount(); i++) {
				int id = goods_list.at(i);
				if(GObj.Fetch(id, &goods_rec) > 0) { // @v8.1.1 Search-->Fetch
					if(goods_rec.BrandID == pFilt->BrandID)
						temp_list.add(id);
				}
			}
			temp_list.sort();
			goods_list.intersect(&temp_list, 1);
		}
		else {
			GoodsFilt filt;
			Goods2Tbl::Rec goods_rec;
			filt.BrandList.Add(pFilt->BrandID);
			for(GoodsIterator iter(&filt, 0); iter.Next(&goods_rec) > 0;)
				goods_list.add(goods_rec.ID); // @v8.1.1 addUnique-->add
			goods_list.sortAndUndup();
			use_goods_list = 1;
		}
		if(goods_list.getCount() == 0)
			done = 1;
	}
	// } @Muxa
	if(!done) {
		PPID   first_goods_id = 0, last_goods_id = 0; // ������ � ��������� ������ � ������� (������������ ��� ��������� ���� ��������)
		PPTransaction tra(ppDbDependTransaction, use_ta);
		THROW(tra);
		if(P_Qc2) {
			PPIDArray rel_list;
			Quotation2Tbl::Key3 k3, k3_;
			if(pFilt->GoodsID) {
				k3.GoodsID = pFilt->GoodsID;
				THROW(Helper_RemoveTempRecsByGoodsID(pFilt->GoodsID, 0));
			}
			else if(use_goods_list) {
				first_goods_id = goods_list.at(0);
				last_goods_id = goods_list.getLast();
			}
			else {
				MEMSZERO(k3_);
				if(P_Qc2->search(3, &k3_, spFirst))
					first_goods_id = P_Qc2->data.GoodsID;
				MEMSZERO(k3_);
				if(P_Qc2->search(3, &k3_, spLast))
					last_goods_id = P_Qc2->data.GoodsID;
			}
			P_Qc2->GetRelListByFilt(&Filt, &rel_list);
			if(rel_list.getCount()) {
				rel_list.sort();
				PPQuot quot;
				if(pFilt->GoodsID) {
					if(pFilt->Period.IsZero()) {
						BExtQuery q(P_Qc2, 3);
						q.selectAll().where(P_Qc2->Actual == 1L && P_Qc2->GoodsID == pFilt->GoodsID);
						MEMSZERO(k3);
						k3.Actual = 1;
						k3.GoodsID = pFilt->GoodsID;
						for(q.initIteration(0, &k3, spGe); q.nextIteration() > 0;) {
							if(rel_list.bsearch(P_Qc2->data.RelID)) {
								P_Qc2->RecToQuot(&P_Qc2->data, quot);
								uint   kpos = 0;
								const  PPID qk_id = quot.Kind;
								if(qk_id && QuotKindList.Search(qk_id, &kpos) && kpos < MAX_QUOTS_PER_TERM_REC && quot.CheckForVal(&pFilt->Val)) {
									// @v8.4.8 PPQuotItem_ item_(quot);
									// @v8.4.8 THROW_SL(pQList->insert(&item_));
									THROW(pQList->Add(quot)); // @v8.4.8
									is_added = BIN(pFilt->GoodsID);
								}
							}
						}
					}
					else {
						//GoodsID, RelID, Dt, Tm (unique mod);          // #0
						Quotation2Tbl::Key0 k0;
						BExtQuery q(P_Qc2, 0);
						q.selectAll().where(P_Qc2->GoodsID == pFilt->GoodsID);
						MEMSZERO(k0);
						k0.GoodsID = pFilt->GoodsID;
						for(q.initIteration(0, &k0, spGe); q.nextIteration() > 0;) {
							if(rel_list.bsearch(P_Qc2->data.RelID)) {
								P_Qc2->RecToQuot(&P_Qc2->data, quot);
								uint   kpos = 0;
								const  PPID qk_id = quot.Kind;
								if(qk_id && pFilt->Period.CheckDate(P_Qc2->data.Dt) &&
									QuotKindList.Search(qk_id, &kpos) && kpos < MAX_QUOTS_PER_TERM_REC && quot.CheckForVal(&pFilt->Val)) {
									THROW(pQList->Set(quot, PPQuotItemArray::rfGt));
									is_added = BIN(pFilt->GoodsID);
								}
							}
						}
					}
				}
				else {
					PPLoadText(PPTXT_WAIT_QUOTVIEWBUILDING, msg_buf);
					const uint _c = last_goods_id - first_goods_id + 1;
					const uint rc = rel_list.getCount();
					for(uint i = 0; i < rc; i++) {
						const PPID rel_id = rel_list.get(i);
						quot.Clear();
						P_Qc2->FetchRel(rel_id, &quot);
						const  PPID qk_id = quot.Kind;
						uint   kpos = 0;
						if(qk_id && QuotKindList.Search(qk_id, &kpos) && kpos < MAX_QUOTS_PER_TERM_REC) {
							if(pFilt->Period.IsZero()) {
								Quotation2Tbl::Key2 k2;
								BExtQuery q(P_Qc2, 2);
								q.selectAll().where(P_Qc2->Actual == 1L && P_Qc2->RelID == rel_id);
								MEMSZERO(k2);
								k2.Actual = 1;
								k2.RelID = rel_id;
								prev_goods_id = 0;
								for(q.initIteration(0, &k2, spGe); q.nextIteration() > 0;) {
									const PPID goods_id = P_Qc2->data.GoodsID;
									if(pFilt->GoodsID || !use_goods_list || goods_list.bsearch(goods_id)) {
										if(CheckGoodsKindDiffRestriction(goods_id)) {
											PROFILE(P_Qc2->RecToQuotRel(&P_Qc2->data, quot));
											if(quot.CheckForVal(&pFilt->Val)) {
												// @v8.4.8 PPQuotItem_ item_(quot);
												// @v8.4.8 THROW_SL(pQList->insert(&item_));
												THROW(pQList->Add(quot)); // @v8.4.8
												is_added = BIN(pFilt->GoodsID);
											}
										}
									}
									prev_goods_id = goods_id;
									if(last_goods_id >= first_goods_id && ((goods_id - first_goods_id) & 0xff) == 0) {
										PROFILE(PPWaitPercent((goods_id - first_goods_id) + _c * i, _c * rc, msg_buf));
									}
								}
							}
							else {
								// RelID, GoodsID, Dt, Tm (unique mod);          // #1
								Quotation2Tbl::Key1 k1;
								BExtQuery q(P_Qc2, 1);
								q.selectAll().where(P_Qc2->RelID == rel_id);
								MEMSZERO(k1);
								k1.RelID = rel_id;
								prev_goods_id = 0;
								for(q.initIteration(0, &k1, spGe); q.nextIteration() > 0;) {
									const PPID goods_id = P_Qc2->data.GoodsID;
									if(pFilt->Period.CheckDate(P_Qc2->data.Dt)) {
										if(pFilt->GoodsID || !use_goods_list || goods_list.bsearch(goods_id)) {
											if(CheckGoodsKindDiffRestriction(goods_id)) {
												PROFILE(P_Qc2->RecToQuotRel(&P_Qc2->data, quot));
												if(quot.CheckForVal(&pFilt->Val)) {
													THROW(pQList->Set(quot, PPQuotItemArray::rfGt));
													is_added = BIN(pFilt->GoodsID);
												}
											}
										}
									}
									prev_goods_id = goods_id;
									if(last_goods_id >= first_goods_id && ((goods_id - first_goods_id) & 0xff) == 0) {
										PROFILE(PPWaitPercent((goods_id - first_goods_id) + _c * i, _c * rc, msg_buf));
									}
								}
							}
						}
					}
				}
			}
		}
		else if(P_Qc) {
			PPLoadText(PPTXT_WAIT_QUOTVIEWBUILDING, msg_buf);
			const PPID single_goods_id = NZOR(pFilt->GoodsID, (use_goods_list && goods_list.getCount() == 1) ? goods_list.get(0) : 0);
			QuotationTbl & _t = *P_Qc;
			QuotationTbl::Key2 k2, k2_;
			BExtQuery q(&_t, 2, 64);
			PPTransaction tra(ppDbDependTransaction, use_ta);
			THROW(tra);
			pFilt->LocList.CopyTo(&loc_list);
			q.select(_t.ID, _t.GoodsID, _t.Kind, _t.Location, _t.Flags, _t.ArID, _t.CurID, _t.Actual, _t.Quot, 0L);
			dbq = &(*dbq && _t.Actual > 0L && _t.CurID == pFilt->CurID);
			dbq = ppcheckfiltid(dbq, _t.GoodsID, single_goods_id);
			if(pFilt->Flags & QuotFilt::fZeroArOnly)
				dbq = &(*dbq && _t.ArID == 0L);
			else
				dbq = ppcheckfiltid(dbq, _t.ArID, pFilt->ArID);
			dbq = ppcheckfiltid(dbq, _t.CurID, pFilt->CurID);
			dbq = ppcheckfiltidlist(dbq, _t.Location, &loc_list);
			if(pFilt->LocList.IsEmpty() && pFilt->Flags & QuotFilt::fAllLocations)
				dbq = &(*dbq && _t.Location == 0L);
			dbq = ppcheckfiltidlist(dbq, _t.Kind, &qk_id_list);
			q.where(*dbq);
			MEMSZERO(k2);
			if(single_goods_id) {
				k2.GoodsID = single_goods_id;
				THROW(Helper_RemoveTempRecsByGoodsID(single_goods_id, 0));
			}
			else if(use_goods_list) {
				first_goods_id = goods_list.at(0);
				last_goods_id = goods_list.getLast();
			}
			else {
				MEMSZERO(k2_);
				if(_t.search(2, &k2_, spFirst))
					first_goods_id = _t.data.GoodsID;
				MEMSZERO(k2_);
				if(_t.search(2, &k2_, spLast))
					last_goods_id = _t.data.GoodsID;
			}
			for(q.initIteration(0, &k2, spGe); q.nextIteration() > 0;) {
				const PPID goods_id = _t.data.GoodsID;
				if(single_goods_id || !use_goods_list || goods_list.bsearch(goods_id)) {
					if(CheckGoodsKindDiffRestriction(goods_id)) {
						PPQuot quot(_t.data);
						uint   kpos = 0;
						const  PPID qk_id = quot.Kind;
						if(qk_id && QuotKindList.Search(qk_id, &kpos) && kpos < MAX_QUOTS_PER_TERM_REC && quot.CheckForVal(&pFilt->Val)) {
							PPQuotItem_ item_(quot);
							THROW_SL(pQList->insert(&item_));
							is_added = BIN(pFilt->GoodsID);
						}
					}
				}
				prev_goods_id = goods_id;
				if(last_goods_id >= first_goods_id)
					PPWaitPercent(goods_id - first_goods_id, last_goods_id - first_goods_id + 1, msg_buf);
			}
			if(is_added == 0 && single_goods_id && (pFilt->Flags & (QuotFilt::fOnlyAbsence|QuotFilt::fAbsence))) {
				if(!filt_group_id || (_t.data.GoodsID == filt_group_id) || GObj.BelongToGroup(pFilt->GoodsID, filt_group_id, 0) > 0) {
					PPQuotItem_ item_;
					item_.GoodsID = pFilt->GoodsID;
					THROW_SL(pQList->insert(&item_));
					is_added = BIN(pFilt->GoodsID);
				}
			}
		}
		if(!(pFilt->Flags & QuotFilt::fListOnly)) {
			uint   i;
			SString last_goods_name;
			PPIDArray temp_goods_list;
			TempQuotTbl::Rec rec;
			MEMSZERO(rec);
			pQList->sort(PTR_CMPFUNC(PPQuotItem_GLA));
			const uint qlc = pQList->getCount();
			PPLoadText(PPTXT_WAIT_QUOTTMPTBUILDING, msg_buf);
			if(pQList == &QList_ && pFilt->QkCls == PPQuot::clsGeneral && QuotKindList.getCount() > 1) {
				//
				// ��������� ���������� ����� ��������� ��� �����������, ������ �� ����, ��� ������� ���
				// �� ������ �������� (������ ���� ������ ������� �� ������ ������ ���������).
				//
				PPIDArray qk_list;
				for(i = 0; i < qlc; i++)
					qk_list.addUnique(pQList->at(i).KindID);
				i = QuotKindList.getCount();
				if(i) do {
					StrAssocArray::Item qki = QuotKindList.at(--i);
					if(qki.Id != pFilt->QuotKindID && !qk_list.lsearch(qki.Id))
						QuotKindList.atFree(i);
				} while(i);
			}
#ifndef NDEBUG
			const int debug_dup = 1;
#else
			const int debug_dup = 0;
#endif
			BExtInsert bei(P_TempTbl);
			for(i = 0; i < qlc; i++) {
				const PPQuotItem_ & r_item = pQList->at(i);
				int32  _pidx = 0;
				Quotation2Core::PeriodToPeriodIdx(&r_item.Period, &_pidx);
				uint   pos = 0;
				const  PPID qk_id = r_item.KindID;
				if(qk_id && QuotKindList.Search(qk_id, &pos) && pos < MAX_QUOTS_PER_TERM_REC) {
					if(r_item.GoodsID != rec.GoodsID || r_item.LocID != rec.LocID || r_item.ArID != rec.ArticleID || _pidx != rec.PeriodIdx) {
						if(rec.GoodsID) {
							STRNSCPY(rec.GoodsName, temp_buf = GetGoodsName(rec.GoodsID, last_goods_name));
							if(rec.PeriodIdx != 0 && !HasPeriodVal)
								HasPeriodVal = 1;
							if(debug_dup || P_TempOrd) {
								PPID   id__ = 0;
								const int ir = P_TempTbl->insertRecBuf(&rec, 0, &id__);
								THROW_DB(ir);
								if(P_TempOrd) {
									rec.ID__ = id__;
									MakeOrderEntry((IterOrder)Filt.InitOrder, rec, ord_rec);
									THROW_DB(P_TempOrd->insertRecBuf(&ord_rec));
								}
							}
							else {
								THROW_DB(bei.insert(&rec));
							}
						}
						MEMSZERO(rec);
						rec.GoodsID   = r_item.GoodsID;
						rec.LocID     = r_item.LocID;
						rec.ArticleID = r_item.ArID;
						rec.PeriodIdx = _pidx;
					}
					PPQuot::PutValToStr(r_item.Val, r_item.Flags, temp_buf).CopyTo(GetQuotTblBuf(&rec, pos), QUOT_BUF_SIZE);
					if(pFilt->QkCls == PPQuot::clsMtxRestr) {
						uint   rest_pos = 0;
						double count = 0.0;
						GoodsFilt gf(rec.GoodsID);
						gf.LocList.Add(rec.LocID);
						gf.Flags |= GoodsFilt::fRestrictByMatrix;
						temp_goods_list.clear();
						GoodsIterator::GetListByFilt(&gf, &temp_goods_list);
						count = r_item.Val - temp_goods_list.getCount();
						QuotKindList.Search(0, &rest_pos);
						(temp_buf = 0).Cat(count, SFMT_MONEY).CopyTo(GetQuotTblBuf(&rec, rest_pos), QUOT_BUF_SIZE);
					}
				}
				if((i & 0xff) == 0)
					PPWaitPercent(i+1, qlc, msg_buf);
			}
			if(rec.GoodsID) {
				STRNSCPY(rec.GoodsName, GetGoodsName(rec.GoodsID, temp_buf));
				if(rec.PeriodIdx != 0 && !HasPeriodVal)
					HasPeriodVal = 1;
				if(debug_dup || P_TempOrd) {
					PPID   id__ = 0;
					THROW_DB(P_TempTbl->insertRecBuf(&rec, 0, &id__));
					if(P_TempOrd) {
						rec.ID__ = id__;
						MakeOrderEntry((IterOrder)Filt.InitOrder, rec, ord_rec);
						THROW_DB(P_TempOrd->insertRecBuf(&ord_rec));
					}
				}
				else {
					THROW_DB(bei.insert(&rec));
				}
			}
			if(!debug_dup) {
				THROW_DB(bei.flash());
			}
		}
		THROW(tra.Commit());
	}
	CATCHZOK
	return ok;
}

int SLAPI PPViewQuot::UpdateTempTable(PPID goodsID, int use_ta)
{
	if(goodsID) {
		QuotFilt temp_filt = Filt;
		temp_filt.GoodsID = goodsID;
		PPQuotItemArray temp_qlist;
		return BIN(Helper_CreateTmpTblEntries(&temp_filt, &temp_qlist, use_ta));
	}
	else
		return -1;
}

int SLAPI PPViewQuot::GetTabTitle(long tabID, SString & rBuf)
{
	if(tabID) {
		LocationTbl::Rec loc_rec;
		if(LocObj.Search(tabID, &loc_rec) > 0)
			rBuf.CopyFrom(loc_rec.Name);
		else
			(rBuf = 0).Cat(tabID);
	}
	else {
		// @v9.3.10 PPGetWord(PPWORD_ALLLOCS, 0, rBuf);
		PPLoadString("allwarehouses", rBuf); // @v9.3.10
	}
	return 1;
}

static void SLAPI dbqf_quotperiod_i(int option, DBConst * result, DBConst * params)
{
	char   buf[32];
	if(option == CALC_SIZE) {
		result->init((long)sizeof(buf));
	}
	else {
		SString temp_buf;
		DateRange period;
		Quotation2Core::PeriodIdxToPeriod(params[0].lval, &period);
		temp_buf.Cat(period, 1);
		temp_buf.CopyTo(buf, sizeof(buf));
		result->init(buf);
	}
}

// static
int PPViewQuot::DynFuncPeriod = 0;

DBQuery * SLAPI PPViewQuot::CreateBrowserQuery(uint * pBrwId, SString * pSubTitle)
{
	DbqFuncTab::RegisterDyn(&DynFuncPeriod, 0, BTS_STRING, dbqf_quotperiod_i, 1, BTS_INT);

	uint   brw_id = 0;
	DBQuery     * q = 0;
	TempQuotTbl * tbl = 0;
	TempQuotSerialTbl * p_ts = 0;
	TempOrderTbl * p_ot = 0;
	if(P_TempSerTbl) {
		brw_id = BROWSER_QUOT_SER;
		p_ts = new TempQuotSerialTbl(P_TempSerTbl->fileName);
		q = & select(
			p_ts->Dt,
			p_ts->Tm,
			p_ts->Val,
			p_ts->ValF,
			p_ts->ValS,
			0).from(p_ts, 0);
		THROW(CheckQueryPtr(q));
	}
	else if(P_TempTbl) {
		tbl = new TempQuotTbl(P_TempTbl->fileName);
		if(P_Ct == 0) {
			if(P_TempOrd) {
				p_ot = new TempOrderTbl(P_TempOrd->fileName);
			}
			if(Filt.ArID) {
				brw_id = BROWSER_QUOT_AR;
				if(pSubTitle)
					GetArticleName(Filt.ArID, *pSubTitle);
			}
			else if(Filt.LocList.GetSingle()) {
				brw_id = BROWSER_QUOT_LOC;
				if(pSubTitle)
					GetExtLocationName(Filt.LocList, 6, *pSubTitle);
			}
			else
				brw_id = BROWSER_QUOT;
			DBE    dbe_loc;
			DBE    dbe_ar;
			DBE    dbe_period;
			PPDbqFuncPool::InitObjNameFunc(dbe_loc, PPDbqFuncPool::IdObjNameLoc, tbl->LocID);
			PPDbqFuncPool::InitObjNameFunc(dbe_ar,  PPDbqFuncPool::IdObjNameAr,  tbl->ArticleID);
			PPDbqFuncPool::InitLongFunc(dbe_period, PPViewQuot::DynFuncPeriod,   tbl->PeriodIdx);
			q = & select(
				tbl->GoodsID,       // #0
				tbl->LocID,         // #1
				tbl->ArticleID,     // #2
				tbl->PeriodIdx,     // #3 // @v7.3.8
				tbl->GoodsName,     // #4 // @v7.3.8 3-->4
				dbe_loc,            // #5 // @v7.3.8 4-->5
				dbe_ar,             // #6 // @v7.3.8 5-->6
				dbe_period,         // #7 // @v7.3.8
				tbl->Quot1,
				tbl->Quot2,
				tbl->Quot3,
				tbl->Quot4,
				tbl->Quot5,
				tbl->Quot6,
				tbl->Quot7,
				tbl->Quot8,
				tbl->Quot9,
				tbl->Quot10,
				tbl->Quot11,
				tbl->Quot12,
				tbl->Quot13,
				tbl->Quot14,
				tbl->Quot15,
				tbl->Quot16,

				tbl->Quot17,
				tbl->Quot18,
				tbl->Quot19,
				tbl->Quot20,
				tbl->Quot21,
				tbl->Quot22,
				tbl->Quot23,
				tbl->Quot24,
				tbl->Quot25,
				tbl->Quot26,
				tbl->Quot27,
				tbl->Quot28,
				tbl->Quot29,
				tbl->Quot30,
				tbl->Quot31,
				tbl->Quot32,

				0);
			if(p_ot)
				q->from(p_ot, tbl, 0L).where(p_ot->ID == tbl->ID__).orderBy(p_ot->Name, 0L);
			else
				q->from(tbl, 0L);
			/* @v8.3.5
			else
				q->from(tbl, 0L).orderBy(tbl->GoodsName, 0L);
			*/
			THROW(CheckQueryPtr(q));
		}
		else {
			if(pSubTitle) {
				if(QuotKindList.getCount() && !oneof2(Filt.QkCls, PPQuot::clsGeneral, PPQuot::clsSupplDeal))
					pSubTitle->CopyFrom(QuotKindList.at(0).Txt);
				else if(Filt.QuotKindID)
					QuotKindList.Get(Filt.QuotKindID, *pSubTitle);
			}
			brw_id = BROWSER_QUOT_CROSSTAB;
			q = PPView::CrosstabDbQueryStub;
		}
	}
	CATCH
		if(q)
			ZDELETE(q);
		else {
			delete tbl;
			delete p_ts;
		}
	ENDCATCH
	ASSIGN_PTR(pBrwId, brw_id);
	return q;
}

int SLAPI PPViewQuot::OnExecBrowser(PPViewBrowser * pBrw)
{
	int    disable_group_selection = 0;
	PPAccessRestriction accsr;
	if(ObjRts.GetAccessRestriction(accsr).OnlyGoodsGrpID && accsr.CFlags & PPAccessRestriction::cfStrictOnlyGoodsGrp) {
		disable_group_selection = 1;
	}
	if(!disable_group_selection)
		pBrw->SetupToolbarCombo(PPOBJ_GOODSGROUP, Filt.GoodsGrpID, OLW_CANSELUPLEVEL, 0);
	return -1;
}

//static
int PPViewQuot::CellStyleFunc(const void * pData, long col, int paintAction, BrowserWindow::CellStyle * pStyle, void * extraPtr)
{
	int    ok = -1;
	PPViewQuot * p_view = (PPViewQuot*)extraPtr;
	const QuotFilt * p_filt = (const QuotFilt*)p_view->GetBaseFilt();
	PPViewQuot::BrwHdr hdr;
	if(p_view && p_filt && pStyle && paintAction == BrowserWindow::paintNormal) {
		if(p_filt->IsSeries() && p_view->P_Qc2) {
			BrwHdrSer * p_hs = (BrwHdrSer *)pData;
			if(col == 2 && p_hs->ValF & PPQuot::fActual) {
				pStyle->Flags = BrowserWindow::CellStyle::fCorner;
				pStyle->Color = GetColorRef(SClrCyan);
				ok = 1;
			}
		}
		else if(p_view->GetEditIds(pData, &hdr, col)) {
			double val = 0.0;
			p_view->GetCtQuotVal(pData, col, 0, &val);
			if(val > 0) {
				pStyle->Flags = BrowserWindow::CellStyle::fCorner;
				pStyle->Color = GetColorRef(SClrGreen);
				ok = 1;
			}
			else if(val < 0) {
				pStyle->Flags = BrowserWindow::CellStyle::fCorner;
				pStyle->Color = GetColorRef(SClrRed);
				ok = 1;
			}
		}
	}
	return ok;
}

int SLAPI PPViewQuot::PreprocessBrowser(PPViewBrowser * pBrw)
{
	int    ok = -1;
	if(pBrw) {
		if(P_TempSerTbl) {
			pBrw->SetCellStyleFunc(PPViewQuot::CellStyleFunc, this);
		}
		else if(P_Ct == 0) {
			FirstQuotBrwColumn = pBrw->view->getDef()->getCount();
			// @v7.3.8 {
			if(HasPeriodVal) {
				SString temp_buf;
				PPLoadString("daterange", temp_buf);
				pBrw->insertColumn(-1, temp_buf, 7, 0L, 0, 0);
				HasPeriodVal = 2; // ������� ����, ��� ������� �������, ��������������� �������
			}
			// } @v7.3.8
			for(uint i = 0; i < MAX_QUOTS_PER_TERM_REC && i < QuotKindList.getCount(); i++) {
				const StrAssocArray::Item entry = QuotKindList.at(i);
				pBrw->insertColumn(-1, entry.Txt, 7+1+i, 0L, MKSFMTD(8, 2, ALIGN_RIGHT|NMBF_NOZERO), 0);
				ok = 1;
			}
		}
		else if(Filt.QkCls == PPQuot::clsMtx)
			pBrw->SetCellStyleFunc(PPViewQuot::CellStyleFunc, this);
	}
	return ok;
}

int SLAPI PPViewQuot::GetCtQuotVal(const void * pRow, long col, long aggrNum, double * pVal)
{
	int    ok = -1;
	double val = 0.0;
	if(P_Ct)
		P_Ct->GetAggrFieldVal(col - 1, aggrNum, pRow, &val, sizeof(val));
	ASSIGN_PTR(pVal, val);
	return ok;
}

int SLAPI PPViewQuot::GetEditIds(const void * pRow, PPViewQuot::BrwHdr * pHdr, long col)
{
	BrwHdr hdr;
	MEMSZERO(hdr);
	if(pRow) {
		if(P_Ct) {
			uint   tab_idx = col;
			int    r = (tab_idx > 0) ? P_Ct->GetTab(tab_idx - 1, &hdr.LocID) : 1;
			if(r > 0)
				P_Ct->GetIdxFieldVal(0, pRow, &hdr.GoodsID, sizeof(hdr.GoodsID));
			hdr.ArticleID  = Filt.ArID;
			hdr.QuotKindID = Filt.QuotKindID;
		}
		else {
			long   quot_col = (Filt.ArID || Filt.LocList.GetSingle()) ? col - 3 : col - 4;
			if(HasPeriodVal == 2)
				quot_col--;
			struct E_ {
				long   GoodsID;
				long   LocID;
				long   ArticleID;
				int32  PeriodIdx;
			} e_;
			e_ = *(E_*)pRow;
			hdr.GoodsID   = e_.GoodsID;
			hdr.LocID     = e_.LocID;
			hdr.ArticleID = e_.ArticleID;
			hdr.PeriodIdx = e_.PeriodIdx;
			if(quot_col >= 0 && quot_col < (long)QuotKindList.getCount())
				hdr.QuotKindID = QuotKindList.at(quot_col).Id;
			SETIFZ(hdr.QuotKindID, Filt.QuotKindID);
		}
	}
	ASSIGN_PTR(pHdr, hdr);
	return 1;
}

int SLAPI PPViewQuot::CheckDelFromMtx(PPID goodsID)
{
	int    ok = 1;
	const  PPID draft_op_id = DS.GetTLA().Cc.DraftRcptOp;
	SString bill_code, bill_dt;
	if(draft_op_id && Filt.QkCls == PPQuot::clsMtx && (GObj.GetConfig().Flags & GCF_DONTDELFROMMTXGOODSINOPENORD) && P_BObj) {
		BillCore * tbl = P_BObj->P_Tbl;
		CpTransfCore * p_cp_trfr = P_BObj->P_CpTrfr;
		BillTbl::Key2 k2;
		DBQ  * dbq = 0;
		BExtQuery q_b(tbl, 2);

		dbq = ppcheckfiltid(dbq, tbl->OpID, draft_op_id);
		q_b.select(tbl->ID, tbl->Dt, tbl->Code, tbl->Flags, 0).where(*dbq);
		MEMSZERO(k2);
		k2.OpID = draft_op_id;
		k2.Dt   = ZERODATE;
		for(q_b.initIteration(0, &k2, spGt); ok > 0 && q_b.nextIteration() > 0;) {
			if(!(tbl->data.Flags & BILLF_CLOSEDORDER)) {
				PPID bill_id = tbl->data.ID;
				TransferTbl::Key0 k0;
				BExtQuery q(p_cp_trfr, 0);
				q.select(p_cp_trfr->GoodsID, 0).where(p_cp_trfr->BillID == bill_id);
				MEMSZERO(k0);
				k0.BillID = bill_id;
				for(q.initIteration(0, &k0, spGt); ok > 0 && q.nextIteration() > 0;) {
					if(goodsID == p_cp_trfr->data.GoodsID) {
						bill_code.CopyFrom(tbl->data.Code);
						bill_dt.Cat(tbl->data.Dt);
						ok = -1;
					}
				}
			}
		}
	}
	if(ok < 0) {
		SString mtx_err_msg;
		SString temp_buf;
		mtx_err_msg.Printf(PPLoadTextS(PPTXT_ERRDELGOODSFROMMTX, temp_buf), bill_dt.cptr(), bill_code.cptr());
		PPOutputMessage(mtx_err_msg, 0);
	}
    return ok;
}

int SLAPI PPViewQuot::ProcessCommand(uint ppvCmd, const void * pHdr, PPViewBrowser * pBrw)
{
	int    ok = PPView::ProcessCommand(ppvCmd, pHdr, pBrw);
	BrwHdr hdr;
	MEMSZERO(hdr);
	if(ok == -2) {
		GetEditIds(pHdr, &hdr, (pBrw) ? pBrw->GetCurColumn() : 0);
		PPID   id = hdr.GoodsID;
		int    is_suppl_deal = BIN(Filt.QkCls == PPQuot::clsSupplDeal);
		switch(ppvCmd) {
			case PPVCMD_INPUTCHAR:
				ok = -1;
				if(Filt.QkCls != PPQuot::clsMtxRestr && pHdr && isdigit(*(const char *)pHdr)) {
					int    init_char = *(const char *)pHdr;
					int    r = 0;
					Goods2Tbl::Rec goods_rec;
					double qtty = 0.0;
					if(pBrw && pBrw->view) {
						if((r = GObj.SelectGoodsByBarcode(init_char, Filt.ArID, &goods_rec, &qtty, 0)) > 0) {
							if(pBrw->view->search2(&goods_rec.ID, CMPF_LONG, srchFirst, 0) <= 0)
								if(AddItem(&goods_rec.ID) > 0)
									ok = 1;
						}
						else if(r != -1)
							pBrw->view->bottom();
					}
				}
				break;
			case PPVCMD_EDITITEM:
				ok = EditItem(&hdr, 0);
				break;
			case PPVCMD_EDITITEMSIMPLE:
				ok = EditItem(&hdr, 1);
				break;
			case PPVCMD_ADDITEM:
				hdr.GoodsID = 0;
				ok = AddItem(&hdr.GoodsID);
				break;
			case PPVCMD_DELETEITEM:
				ok = -1;
				if(P_Qc || P_Qc2) {
					if(hdr.GoodsID && CONFIRM(PPCFM_DELQUOTSBYGOODS) && CheckDelFromMtx(hdr.GoodsID) > 0) {
						PPQuotArray quot_list(hdr.GoodsID);
						if(P_Qc)
							P_Qc->GetCurrList(hdr.GoodsID, 0, 0, quot_list);
						else if(P_Qc2)
							P_Qc2->GetCurrList(hdr.GoodsID, 0, 0, quot_list);
						PPIDArray kind_list;
						QuotKindList.GetListByParent(0, 1, kind_list);
						uint i = quot_list.getCount();
						if(i)
							do {
								PPQuot & r_quot = quot_list.at(--i);
								if(r_quot.CheckForFilt(&Filt, &kind_list))
									quot_list.atFree(i);
							} while(i);
						if(P_Qc)
							ok = P_Qc->SetCurrList(quot_list, 0, 0, 1) ? 1 : PPErrorZ();
						else if(P_Qc2)
							ok = P_Qc2->Set(quot_list, 0, 0, 0, 1) ? 1 : PPErrorZ();
					}
				}
				break;
			case PPVCMD_SYSINFO:
				if(hdr.QuotKindID && hdr.GoodsID) {
					PPQuot quot;
					if(P_TempSerTbl) {
						if(pHdr) {
							struct __HdrSeries {
								LDATETIME Dtm;
								double Value;
								long   Flags;
							};
							const __HdrSeries * p_hs = (const __HdrSeries *)pHdr;
							quot.Kind = Filt.QuotKindID;
							quot.GoodsID = Filt.GoodsID;
							quot.LocID = Filt.LocID;
							quot.Quot = p_hs->Value;
							quot.Flags = p_hs->Flags;
							quot.Dtm = p_hs->Dtm;
							ViewQuotValueInfo(quot);
						}
					}
					else {
						PPQuotArray qary(hdr.GoodsID);
						QuotIdent qi(hdr.LocID, hdr.QuotKindID, Filt.CurID, hdr.ArticleID);
						GObj.GetQuotList(hdr.GoodsID, 0, qary);
						qary.GetQuot(qi, &quot);
						ViewQuotValueInfo(quot);
					}
				}
				break;
			case PPVCMD_EDITGOODS:
				ok = -1;
				if(Filt.QkCls != PPQuot::clsMtxRestr && hdr.GoodsID && GObj.Edit(&hdr.GoodsID, 0) == cmOK)
					ok = 1;
				break;
			case PPVCMD_VIEWLOTS:
				ok = -1;
				if(Filt.QkCls != PPQuot::clsMtxRestr && hdr.GoodsID)
					::ViewLots(hdr.GoodsID, hdr.LocID, 0L, 0, 0);
				break;
			case PPVCMD_EDITQUOTKIND:
				ok = -1;
				if(pBrw && pBrw->view) {
					PPID   qk_id = 0;
					if(!P_Ct) {
						int    pos = pBrw->view->GetCurColumn();
						if(pos >= FirstQuotBrwColumn) {
							pos -= FirstQuotBrwColumn;
							qk_id = QuotKindList.at(pos).Id;
						}
					}
					else if(Filt.QkCls == PPQuot::clsMtx && Spc.MtxID)
						qk_id = Spc.MtxID;
					else if(Filt.QkCls == PPQuot::clsMtxRestr && Spc.MtxRestrID)
						qk_id = Spc.MtxRestrID;
					else if(Filt.QkCls == PPQuot::clsPredictCoeff && Spc.PredictCoeffID)
						qk_id = Spc.PredictCoeffID;
					else
						qk_id = Filt.QuotKindID;
					if(qk_id > 0)
						QkObj.Edit(&qk_id, 0);
				}
				break;
			case PPVCMD_VIEWMTXBYGGRP:
				{
					if(oneof2(Filt.QkCls, PPQuot::clsMtx, PPQuot::clsMtxRestr)) {
						QuotFilt filt = Filt;
						filt.QkCls = PPQuot::clsMtx;
						filt.GoodsGrpID = id;
						filt.LocList.InitEmpty();
						filt.LocList.Add(hdr.LocID);
						ViewQuot(&filt);
						ok = 1;
					}
				}
				break;
			case PPVCMD_VIEWSERIES:
				if(!P_TempSerTbl) {
					if(hdr.GoodsID) {
						QuotFilt filt;
						filt.GoodsID = hdr.GoodsID;
						filt.LocID = hdr.LocID;
						if(hdr.LocID == 0)
							filt.Flags |= QuotFilt::fAllLocations;
						filt.ArID = hdr.ArticleID;
						if(filt.ArID == 0)
							filt.Flags |= QuotFilt::fZeroArOnly;
						filt.QuotKindID = hdr.QuotKindID;
						filt.Flags |= QuotFilt::fSeries;
						ViewQuot(&filt);
					}
				}
				break;
			case PPVCMD_TRANSMIT:
				ok = Transmit(&hdr);
				break;
			case PPVCMD_DORECOVER:
				ok = Recover();
				break;
			case PPVCMD_TB_CBX_SELECTED:
				{
					ok = -1;
					PPID ggrp = 0;
					if(pBrw && pBrw->GetToolbarComboData(&ggrp) && Filt.GoodsGrpID != ggrp) {
						Filt.GoodsGrpID = ggrp;
						ok = ChangeFilt(1, pBrw);
					}
				}
				break;
		}
	}
	if(ok > 0 && oneof5(ppvCmd, PPVCMD_EDITITEM, PPVCMD_EDITITEMSIMPLE, PPVCMD_DELETEITEM, PPVCMD_ADDITEM, PPVCMD_EDITGOODS))
		if(hdr.GoodsID) {
			if(P_Ct)
				ok = ChangeFilt(1, pBrw);
			else {
				ok = UpdateTempTable(hdr.GoodsID, 1);
				//ok = ChangeFilt(1, pBrw);
			}
		}
	return ok;
}

int SLAPI PPViewQuot::AddItem(PPID * pGoodsID)
{
	PPID   __goods_id = pGoodsID ? *pGoodsID : 0;
	if(!__goods_id) {
		if(Filt.QkCls == PPQuot::clsMtxRestr) {
			PPID   goods_id = 0;
			if(ListBoxSelDialog(PPOBJ_GOODSGROUP, &goods_id, (void *)GGRTYP_SEL_NORMAL) > 0)
				__goods_id = goods_id;
		}
		else {
			if(!P_GoodsSelDlg) {
				P_GoodsSelDlg = new ExtGoodsSelDialog(0, Filt.GoodsGrpID, ExtGoodsSelDialog::fByName);
				if(!CheckDialogPtr(&P_GoodsSelDlg, 1))
					return PPErrorZ();
			}
			if(P_GoodsSelDlg && ExecView(P_GoodsSelDlg) == cmOK) {
				TIDlgInitData tidi;
				if(P_GoodsSelDlg->getDTS(&tidi) > 0)
					__goods_id = tidi.GoodsID;
			}
		}
	}
	BrwHdr hdr;
	hdr.GoodsID = __goods_id;
	hdr.LocID = Filt.LocList.GetSingle();
	hdr.ArticleID = Filt.ArID;
	int    ok = EditItem(&hdr, 0);
	ASSIGN_PTR(pGoodsID, __goods_id);
	return ok;
}

int SLAPI PPViewQuot::EditItem(const BrwHdr * pHdr, int simple)
{
	int    ok = -1;
	PPID   acc_sheet_id = 0;
	if(Filt.QuotKindID) {
		PPQuotKind qk_rec;
		if(QkObj.Fetch(Filt.QuotKindID, &qk_rec) > 0) // @v7.1.2 Search-->Fetch
			acc_sheet_id = qk_rec.AccSheetID;
	}
	if(pHdr->GoodsID) {
		if(simple) {
			if(pHdr->QuotKindID) {
				PPQuot quot;
				PPQuotArray qary(pHdr->GoodsID);
				QuotIdent qi(pHdr->LocID, pHdr->QuotKindID, Filt.CurID, pHdr->ArticleID);
				THROW(GObj.GetQuotList(pHdr->GoodsID, 0, qary));
				qary.GetQuot(qi, &quot);
				if(EditQuotVal(&quot, Filt.QkCls) > 0) {
					qary.SetQuot(qi, quot.Quot, quot.Flags, quot.MinQtty, &quot.Period);
					THROW(ok = GObj.P_Tbl->SetQuotList(qary, 1));
				}
			}
		}
		else
			ok = GObj.EditQuotations(pHdr->GoodsID, pHdr->LocID, Filt.CurID, pHdr->ArticleID, Filt.QkCls, 0, acc_sheet_id);
	}
	CATCHZOK
	return ok;
}

int SLAPI PPViewQuot::Transmit(const BrwHdr * /*pHdr*/)
{
	int    ok = -1, sync_cmp = 1;
	ObjTransmitParam param;
	PPObjectTransmit * p_ot = 0;
	if(ObjTransmDialog(DLG_OBJTRANSM, &param) > 0) {
		QuotViewItem item;
		const  PPIDArray & rary = param.DestDBDivList.Get();
		sync_cmp = BIN(param.Flags & ObjTransmitParam::fSyncCmp);
		PPObjIDArray objid_ary;
		PPWait(1);
		if(!sync_cmp)
			DS.SetStateFlag(CFGST_TRANSQUOT, 1);
		for(InitIteration(); NextIteration(&item) > 0; PPWaitPercent(GetCounter())) {
			PPID   obj_type = 0;
			Goods2Tbl::Rec goods_rec;
			if(GObj.Fetch(item.GoodsID, &goods_rec) > 0)
				if(goods_rec.Kind == PPGDSK_GOODS)
					obj_type = PPOBJ_GOODS;
				else if(goods_rec.Kind == PPGDSK_GROUP)
					obj_type = PPOBJ_GOODSGROUP;
			if(obj_type)
				objid_ary.Add(obj_type, item.GoodsID);
		}
		THROW(PPObjectTransmit::Transmit(&rary, &objid_ary, &param));
		ok = 1;
	}
	CATCHZOKPPERR
	if(!sync_cmp)
		DS.SetStateFlag(CFGST_TRANSQUOT, 0);
	PPWait(0);
	return ok;
}

#if 0 // @v7.8.8 {
static void __CheckQuotList(PPQuotArray & rList, PPLogger & rLogger)
{
	SString msg_buf, fmt_buf;
	const uint c = rList.getCount();
	for(uint i = 0; i < c; i++) {
		const PPQuot & r_i = rList.at(i);
		for(uint j = 0; j < i; j++) {
			const PPQuot & r_j = rList.at(j);
			if(r_j.IsEqual(r_i, PPQuot::cmpNoID|PPQuot::cmpNoVal)) {
				// ����������� �������� ��������� [@int '@quotkind' '@goods' @hex @real] - [@int @hex @real]
				PPLoadText(PPTXT_INVQUOTDUP, fmt_buf);
				PPFormat(fmt_buf, &msg_buf, r_i.ID, r_i.Kind, r_i.GoodsID, r_i.Flags, r_i.Quot, r_j.ID, r_j.Flags, r_j.Quot);
				rLogger.Log(msg_buf);
			}
		}
	}
}
#endif // } 0 @v7.8.8

int SLAPI PPViewQuot::Recover()
{
	int    ok = -1, ta = 0;
	IterCounter cntr;
	SString msg_buf, fmt_buf;
	PPLogger logger;
	if(P_Qc2) {
		P_Qc2->Verify();
	}
	else if(P_Qc) {
		PPObjQuotKind::Special spc_qk;
		QuotationTbl::Key2 k2;
		PPWait(1);
		cntr.Init(P_Qc);
		PPObjQuotKind::GetSpecialKinds(&spc_qk, 0);
		THROW(PPStartTransaction(&ta, 1));
		MEMSZERO(k2);
		if(P_Qc->search(2, &k2, spFirst)) {
			PPQuotArray quot_list(0);
			do {
				QuotationTbl::Rec rec;
				P_Qc->copyBufTo(&rec);

				DBRowId dbpos;
				THROW_DB(P_Qc->getPosition(&dbpos)); // @v8.0.0
				PPWaitPercent(cntr.Increment());
				if(rec.Actual) {
					PPQuot quot(rec);
					if(rec.GoodsID != quot_list.GoodsID) {
						if(quot_list.GoodsID) {
							if(quot_list.Correct721(&logger) > 0) {
								THROW(P_Qc->SetCurrList(quot_list, 0, 0, 0));
							}
						}
						quot_list.clear();
						quot_list.GoodsID = rec.GoodsID;
					}
					quot_list.insert(&quot);
					//
					double q = fabs(rec.Quot);
					if(quot.IsEmpty()) {
						// ������� �������� ��������� [@int '@quotkind' '@goods' @hex @real]
						PPFormatT(PPTXT_INVQUOTZEROVAL, &msg_buf, rec.ID, rec.Kind, rec.GoodsID, rec.Flags, rec.Quot);
						logger.Log(msg_buf);
					}
					else if(q > 1000000.0 || (q < 0.0000001 && q > 0.0)) {
						// ������������ �������� ��������� [@int '@quotkind' '@goods' @hex @real] - ��������� ����� �������
						PPFormatT(PPTXT_INVQUOTVAL, &msg_buf, rec.ID, rec.Kind, rec.GoodsID, rec.Flags, rec.Quot);
						logger.Log(msg_buf);
						THROW_DB(deleteFrom(P_Qc, 0, (P_Qc->ID == rec.ID))); // @v7.1.2
						// @v7.1.2 THROW_DB(QC.deleteRec());
					}
					else if(rec.Kind && rec.Kind == spc_qk.MtxID) {
						if(rec.Quot != -1.0 && rec.Quot != 1.0 && rec.Quot != 0.0) {
							// ������������ �������� ��������� ��������� [@int '@quotkind' '@goods' @hex @real]
							PPFormatT(PPTXT_INVQUOTMTXVAL, &msg_buf, rec.ID, rec.Kind, rec.GoodsID, rec.Flags, rec.Quot);
							logger.Log(msg_buf);
						}
					}
				}
				THROW_DB(P_Qc->getDirect(2, &k2, dbpos)); // @v8.0.0
			} while(P_Qc->search(2, &k2, spNext));
			if(quot_list.GoodsID) {
				if(quot_list.Correct721(&logger) > 0) {
					THROW(P_Qc->SetCurrList(quot_list, 0, 0, 0));
				}
			}
		}
		THROW(PPCommitWork(&ta));
		PPWait(0);
	}
	CATCH
		PPRollbackWork(&ta);
		logger.LogLastError();
	ENDCATCH
	logger.Save(PPFILNAM_ERR_LOG, 0);
	return -1;
}

int SLAPI PPViewQuot::ViewTotal()
{
	int    ok = -1;
	TDialog * dlg = new TDialog(DLG_QUOTTOTAL);
	if(CheckDialogPtr(&dlg, 1)) {
		long   count = 0;
		QuotViewItem item;
		PPWait(1);
		for(InitIteration(); NextIteration(&item) > 0; PPWaitPercent(GetCounter()))
			count++;
		PPWait(0);
		dlg->setCtrlLong(CTL_QUOTTOTAL_COUNT, count);
		ExecViewAndDestroy(dlg);
	}
	else
		ok = 0;
	return ok;
}

int SLAPI PPViewQuot::Print(const void * pHdr)
{
	uint   rpt_id = REPORT_QUOTLIST;
	if(!Filt.LocList.IsEmpty())
		rpt_id = REPORT_QUOTLIST_LOC;
	else if(Filt.ArID)
		rpt_id = REPORT_QUOTLIST_OBJ;
	else
		rpt_id = REPORT_QUOTLIST;
	return Helper_Print(rpt_id);
}
//
//
//
int SLAPI ViewQuot(const QuotFilt * pFilt)
{
	return PPView::Execute(PPVIEW_QUOT, pFilt, 1, 0);
}
//
// Implementation of PPALDD_UhttQuot
//
PPALDD_CONSTRUCTOR(UhttQuot)
{
	InitFixData(rscDefHdr, &H, sizeof(H));
}

PPALDD_DESTRUCTOR(UhttQuot)
{
	Destroy();
}

int PPALDD_UhttQuot::InitData(PPFilt & rFilt, long rsrv)
{
	int    ok = -1;
	if(rFilt.Ptr) {
		PPQuot * p_quot = (PPQuot *)rFilt.Ptr;
		H.QuotKindID = p_quot->Kind;
		H.SellerLocID = p_quot->LocID;
		if(p_quot->LocID) {
			PPObjLocation loc_obj;
			LocationTbl::Rec loc_rec;
			if(loc_obj.Fetch(p_quot->LocID, &loc_rec) > 0) {
				PPID   person_id = 0;
				PPObjArticle ar_obj;
				if(loc_rec.Type == LOCTYP_WAREHOUSE) {
					GetMainOrgID(&person_id);
					ar_obj.P_Tbl->PersonToArticle(person_id, GetSupplAccSheet(), &H.SellerArID);
				}
				else if(loc_rec.OwnerID) {
					ar_obj.P_Tbl->PersonToArticle(loc_rec.OwnerID, GetSupplAccSheet(), &H.SellerArID);
				}
			}
		}
		H.BuyerArID = p_quot->ArID;
		H.GoodsID = p_quot->GoodsID;
		H.CurID   = p_quot->CurID;
		H.Flags = p_quot->Flags;
		H.Dt = p_quot->Dtm.d;
		H.Tm = p_quot->Dtm.t;
		H.MinQtty = p_quot->MinQtty;
		H.Quot = p_quot->Quot;
		ok = DlRtm::InitData(rFilt, rsrv);
	}
	return ok;
}
//
// Implementation of PPALDD_QuotView
//
PPALDD_CONSTRUCTOR(QuotView)
{
	if(Valid) {
		AssignHeadData(&H, sizeof(H));
		AssignIterData(1, &I, sizeof(I));
	}
}

PPALDD_DESTRUCTOR(QuotView)
{
	Destroy();
}

int PPALDD_QuotView::InitData(PPFilt & rFilt, long rsrv)
{
	INIT_PPVIEW_ALDD_DATA_U(Quot, rsrv);
	H.FltQuotKindID = p_filt->QuotKindID;
	H.FltLocID      = p_filt->LocList.GetSingle();
	H.FltClientID   = p_filt->ArID;
	H.FltCurID      = p_filt->CurID;
	H.FltGoodsGrpID = p_filt->GoodsGrpID;
	H.FltGoodsID    = p_filt->GoodsID;
	H.Flags         = p_filt->Flags;
	H.NumQuotKinds  = p_v->GetQuotKindList().getCount();
	const size_t sz = sizeof(H.QuotName1);
	const size_t offs = offsetof(PPALDD_QuotView::Head, QuotName1);
	for(uint i = 0; i < p_v->GetQuotKindList().getCount(); i++) {
		const StrAssocArray::Item item = p_v->GetQuotKindList().at(i);
		strnzcpy(((char *)&H)+offs+sz*i, item.Txt, sz);
	}
	return DlRtm::InitData(rFilt, rsrv);
}

int PPALDD_QuotView::InitIteration(PPIterID iterId, int sortId, long rsrv)
{
	INIT_PPVIEW_ALDD_ITER(Quot);
}

int PPALDD_QuotView::NextIteration(PPIterID iterId, long rsrv)
{
	START_PPVIEW_ALDD_ITER(Quot);
	I.GoodsID  = item.GoodsID;
	I.LocID    = item.LocID;
	I.ClientID = item.ArticleID;
	const size_t sz = sizeof(I.Quot1);
	for(uint i = 0; i < MAX_QUOTS_PER_TERM_REC; i++) {
		size_t offs = offsetof(PPALDD_QuotView::Iter, Quot1);
		strnzcpy(((char *)&I)+offs+sz*i, item.Quots[i], sz);
	}
	PPWaitPercent(p_v->GetCounter());
	FINISH_PPVIEW_ALDD_ITER();
}

int PPALDD_QuotView::Destroy()
{
	DESTROY_PPVIEW_ALDD(Quot);
}