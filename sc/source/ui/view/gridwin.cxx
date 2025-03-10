/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <scitems.hxx>

#include <memory>
#include <editeng/adjustitem.hxx>
#include <sot/storage.hxx>
#include <editeng/eeitem.hxx>
#include <editeng/editview.hxx>
#include <editeng/editstat.hxx>
#include <editeng/flditem.hxx>
#include <editeng/justifyitem.hxx>
#include <editeng/unolingu.hxx>
#include <editeng/langitem.hxx>
#include <editeng/misspellrange.hxx>
#include <editeng/editobj.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/ipclient.hxx>
#include <svl/stritem.hxx>
#include <svl/sharedstringpool.hxx>
#include <vcl/canvastools.hxx>
#include <vcl/commandevent.hxx>
#include <vcl/cursor.hxx>
#include <vcl/inputctx.hxx>
#include <vcl/settings.hxx>
#include <sot/formats.hxx>
#include <comphelper/classids.hxx>

#include <svx/svdview.hxx>
#include <editeng/outliner.hxx>
#include <svx/svdocapt.hxx>
#include <svx/svdpagv.hxx>

#include <com/sun/star/sheet/DataPilotFieldFilter.hpp>
#include <com/sun/star/sheet/DataPilotFieldOrientation.hpp>
#include <com/sun/star/sheet/DataPilotTableHeaderData.hpp>
#include <com/sun/star/sheet/MemberResultFlags.hpp>
#include <com/sun/star/sheet/TableValidationVisibility.hpp>
#include <com/sun/star/awt/KeyModifier.hpp>
#include <com/sun/star/awt/MouseButton.hpp>
#include <com/sun/star/script/vba/VBAEventId.hpp>
#include <com/sun/star/script/vba/XVBAEventProcessor.hpp>
#include <com/sun/star/text/textfield/Type.hpp>

#include <gridwin.hxx>
#include <tabvwsh.hxx>
#include <docsh.hxx>
#include <viewdata.hxx>
#include <tabview.hxx>
#include <select.hxx>
#include <scmod.hxx>
#include <document.hxx>
#include <attrib.hxx>
#include <dbdata.hxx>
#include <stlpool.hxx>
#include <printfun.hxx>
#include <cbutton.hxx>
#include <sc.hrc>
#include <helpids.h>
#include <globstr.hrc>
#include <strings.hrc>
#include <editutil.hxx>
#include <scresid.hxx>
#include <inputhdl.hxx>
#include <uiitems.hxx>
#include <formulacell.hxx>
#include <patattr.hxx>
#include <notemark.hxx>
#include <rfindlst.hxx>
#include <output.hxx>
#include <docfunc.hxx>
#include <dbdocfun.hxx>
#include <dpobject.hxx>
#include <transobj.hxx>
#include <drwtrans.hxx>
#include <seltrans.hxx>
#include <sizedev.hxx>
#include <AccessibilityHints.hxx>
#include <dpsave.hxx>
#include <viewuno.hxx>
#include <compiler.hxx>
#include <editable.hxx>
#include <fillinfo.hxx>
#include <filterentries.hxx>
#include <drwlayer.hxx>
#include <validat.hxx>
#include <tabprotection.hxx>
#include <postit.hxx>
#include <dpcontrol.hxx>
#include <checklistmenu.hxx>
#include <clipparam.hxx>
#include <overlayobject.hxx>
#include <cellsuno.hxx>
#include <drawview.hxx>
#include <dragdata.hxx>
#include <cliputil.hxx>
#include <queryentry.hxx>
#include <markdata.hxx>
#include <externalrefmgr.hxx>
#include <dociter.hxx>
#include <hints.hxx>
#include <spellcheckcontext.hxx>
#include <uiobject.hxx>
#include <undoblk.hxx>
#include <datamapper.hxx>

#include <svx/sdrpagewindow.hxx>
#include <svx/sdr/overlay/overlaymanager.hxx>
#include <vcl/svapp.hxx>
#include <vcl/uitest/logger.hxx>
#include <vcl/uitest/eventdescription.hxx>
#include <svx/sdr/overlay/overlayselection.hxx>
#include <comphelper/string.hxx>
#include <comphelper/lok.hxx>
#include <sfx2/lokhelper.hxx>

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <vector>

using namespace css;
using namespace css::uno;

namespace {

enum class ScFilterBoxMode
{
    DataSelect,
    Scenario
};

}

struct ScGridWindow::MouseEventState
{
    bool mbActivatePart;

    MouseEventState() :
        mbActivatePart(false)
    {}
};

#define SC_FILTERLISTBOX_LINES  12

ScGridWindow::VisibleRange::VisibleRange(const ScDocument* pDoc)
    : mnCol1(0)
    , mnCol2(pDoc->MaxCol())
    , mnRow1(0)
    , mnRow2(pDoc->MaxRow())
{
}

bool ScGridWindow::VisibleRange::isInside(SCCOL nCol, SCROW nRow) const
{
    return mnCol1 <= nCol && nCol <= mnCol2 && mnRow1 <= nRow && nRow <= mnRow2;
}

bool ScGridWindow::VisibleRange::set(SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2)
{
    bool bChanged = mnCol1 != nCol1 || mnRow1 != nRow1 || mnCol2 != nCol2 || mnRow2 != nRow2;

    mnCol1 = nCol1;
    mnRow1 = nRow1;
    mnCol2 = nCol2;
    mnRow2 = nRow2;

    return bChanged;
}

class ScFilterListBox : public ListBox
{
private:
    VclPtr<ScGridWindow>   pGridWin;
    SCCOL const     nCol;
    SCROW const     nRow;
    bool            bInit;
    bool            bCancelled;
    bool            bInSelect;
    sal_uLong       nSel;
    ScFilterBoxMode const eMode;

protected:
    void            SelectHdl();

public:
                ScFilterListBox( vcl::Window* pParent, ScGridWindow* pGrid,
                                 SCCOL nNewCol, SCROW nNewRow, ScFilterBoxMode eNewMode );
                virtual ~ScFilterListBox() override;
    virtual void dispose() override;

    virtual bool    PreNotify( NotifyEvent& rNEvt ) override;
    virtual void    Select() override;

    SCCOL           GetCol() const          { return nCol; }
    SCROW           GetRow() const          { return nRow; }
    ScFilterBoxMode GetMode() const         { return eMode; }
    void            EndInit();
    bool            IsInInit() const        { return bInit; }
    void            SetCancelled()          { bCancelled = true; }
    bool            IsInSelect() const      { return bInSelect; }
};

//  ListBox in a FloatingWindow (pParent)
ScFilterListBox::ScFilterListBox( vcl::Window* pParent, ScGridWindow* pGrid,
                                  SCCOL nNewCol, SCROW nNewRow, ScFilterBoxMode eNewMode ) :
    ListBox( pParent, WB_AUTOHSCROLL ),
    pGridWin( pGrid ),
    nCol( nNewCol ),
    nRow( nNewRow ),
    bInit( true ),
    bCancelled( false ),
    bInSelect( false ),
    nSel( 0 ),
    eMode( eNewMode )
{
}

ScFilterListBox::~ScFilterListBox()
{
    disposeOnce();
}

void ScFilterListBox::dispose()
{
    if (IsMouseCaptured())
        ReleaseMouse();
    pGridWin.clear();
    ListBox::dispose();
}

void ScFilterListBox::EndInit()
{
    sal_Int32 nPos = GetSelectedEntryPos();
    if ( LISTBOX_ENTRY_NOTFOUND == nPos )
        nSel = 0;
    else
        nSel = nPos;

    bInit = false;
}

bool ScFilterListBox::PreNotify( NotifyEvent& rNEvt )
{
    bool bDone = false;
    if ( rNEvt.GetType() == MouseNotifyEvent::KEYINPUT )
    {
        KeyEvent aKeyEvt = *rNEvt.GetKeyEvent();
        vcl::KeyCode aCode = aKeyEvt.GetKeyCode();
        if ( !aCode.GetModifier() ) // no modifiers
        {
            sal_uInt16 nKey = aCode.GetCode();
            if ( nKey == KEY_RETURN )
            {
                SelectHdl(); // select
                bDone = true;
            }
            else if ( nKey == KEY_ESCAPE )
            {
                pGridWin->ClickExtern();  // clears the listbox
                bDone = true;
            }
        }
    }

    return bDone || ListBox::PreNotify( rNEvt );
}

void ScFilterListBox::Select()
{
    ListBox::Select();
    SelectHdl();
}

void ScFilterListBox::SelectHdl()
{
    if ( !IsTravelSelect() && !bInit && !bCancelled )
    {
        sal_Int32 nPos = GetSelectedEntryPos();
        if ( LISTBOX_ENTRY_NOTFOUND != nPos )
        {
            nSel = nPos;
            // #i81298# set bInSelect flag, so the box isn't deleted from modifications within FilterSelect
            bInSelect = true;
            pGridWin->FilterSelect( nSel );
            bInSelect = false;
        }
    }
}

namespace {

// use a System floating window for the above filter listbox
class ScFilterFloatingWindow : public FloatingWindow
{
public:
    ScFilterFloatingWindow( vcl::Window* pParent, WinBits nStyle );
    virtual ~ScFilterFloatingWindow() override;
    virtual void dispose() override;
};

}

ScFilterFloatingWindow::ScFilterFloatingWindow( vcl::Window* pParent, WinBits nStyle ) :
    FloatingWindow( pParent, nStyle|WB_SYSTEMWINDOW ) // make it a system floater
    {}

ScFilterFloatingWindow::~ScFilterFloatingWindow()
{
    disposeOnce();
}

void ScFilterFloatingWindow::dispose()
{
    EndPopupMode();
    FloatingWindow::dispose();
}

static bool lcl_IsEditableMatrix( ScDocument* pDoc, const ScRange& rRange )
{
    // If it is an editable range and if there is a Matrix cell at the bottom right with an
    // origin top left then the range will be set to contain the exact matrix.
    //! Extract the MatrixEdges functions directly from the column ???
    if ( !pDoc->IsBlockEditable( rRange.aStart.Tab(), rRange.aStart.Col(),rRange.aStart.Row(),
                                    rRange.aEnd.Col(),rRange.aEnd.Row() ) )
        return false;

    ScRefCellValue aCell(*pDoc, rRange.aEnd);
    ScAddress aPos;
    return (aCell.meType == CELLTYPE_FORMULA && aCell.mpFormula->GetMatrixOrigin(aPos) && aPos == rRange.aStart);
}

static void lcl_UnLockComment( ScDrawView* pView, const Point& rPos, const ScViewData* pViewData )
{
    if (!pView || !pViewData)
        return;

    ScDocument& rDoc = *pViewData->GetDocument();
    ScAddress aCellPos( pViewData->GetCurX(), pViewData->GetCurY(), pViewData->GetTabNo() );
    ScPostIt* pNote = rDoc.GetNote( aCellPos );
    SdrObject* pObj = pNote ? pNote->GetCaption() : nullptr;
    if( pObj && pObj->GetLogicRect().IsInside( rPos ) && ScDrawLayer::IsNoteCaption( pObj ) )
    {
        const ScProtectionAttr* pProtAttr = rDoc.GetAttr( aCellPos, ATTR_PROTECTION );
        bool bProtectAttr = pProtAttr->GetProtection() || pProtAttr->GetHideCell() ;
        bool bProtectDoc =  rDoc.IsTabProtected( aCellPos.Tab() ) || pViewData->GetSfxDocShell()->IsReadOnly() ;
        // unlock internal layer (if not protected), will be relocked in ScDrawView::MarkListHasChanged()
        pView->LockInternalLayer( bProtectDoc && bProtectAttr );
    }
}

static bool lcl_GetHyperlinkCell(
    ScDocument* pDoc, SCCOL& rPosX, SCROW nPosY, SCTAB nTab, ScRefCellValue& rCell, OUString& rURL )
{
    bool bFound = false;
    do
    {
        ScAddress aPos(rPosX, nPosY, nTab);
        rCell.assign(*pDoc, aPos);
        if (rCell.isEmpty())
        {
            if ( rPosX <= 0 )
                return false;                           // everything empty to the links
            else
                --rPosX;                                // continue search
        }
        else
        {
            const ScPatternAttr* pPattern = pDoc->GetPattern(aPos);
            if ( !pPattern->GetItem(ATTR_HYPERLINK).GetValue().isEmpty() )
            {
                rURL = pPattern->GetItem(ATTR_HYPERLINK).GetValue();
                bFound = true;
            }
            else if (rCell.meType == CELLTYPE_EDIT)
                bFound = true;
            else if (rCell.meType == CELLTYPE_FORMULA && rCell.mpFormula->IsHyperLinkCell())
                bFound = true;
            else
                return false;                               // other cell
        }
    }
    while ( !bFound );

    return bFound;
}

//  WB_DIALOGCONTROL needed for UNO-Controls
ScGridWindow::ScGridWindow( vcl::Window* pParent, ScViewData* pData, ScSplitPos eWhichPos )
:           Window( pParent, WB_CLIPCHILDREN | WB_DIALOGCONTROL ),
            DropTargetHelper( this ),
            DragSourceHelper( this ),
            mpOOCursors(),
            mpOOSelection(),
            mpOOSelectionBorder(),
            mpOOAutoFill(),
            mpOODragRect(),
            mpOOHeader(),
            mpOOShrink(),
            maVisibleRange(pData->GetDocument()),
            pViewData( pData ),
            eWhich( eWhichPos ),
            mpNoteMarker(),
            mpFilterBox(),
            mpFilterFloat(),
            mpAutoFilterPopup(),
            mpDPFieldPopup(),
            mpFilterButton(),
            nCursorHideCount( 0 ),
            nButtonDown( 0 ),
            nMouseStatus( SC_GM_NONE ),
            nNestedButtonState( ScNestedButtonState::NONE ),
            nDPField( 0 ),
            pDragDPObj( nullptr ),
            nRFIndex( 0 ),
            nRFAddX( 0 ),
            nRFAddY( 0 ),
            nPagebreakMouse( SC_PD_NONE ),
            nPagebreakBreak( 0 ),
            nPagebreakPrev( 0 ),
            nPageScript( SvtScriptType::NONE ),
            nDragStartX( -1 ),
            nDragStartY( -1 ),
            nDragEndX( -1 ),
            nDragEndY( -1 ),
            meDragInsertMode( INS_NONE ),
            aComboButton( this ),
            aCurMousePos( 0,0 ),
            nPaintCount( 0 ),
            aRFSelectedCorned( NONE ),
            bEEMouse( false ),
            bDPMouse( false ),
            bRFMouse( false ),
            bRFSize( false ),
            bPagebreakDrawn( false ),
            bDragRect( false ),
            bIsInPaint( false ),
            bNeedsRepaint( false ),
            bAutoMarkVisible( false ),
            bListValButton( false )
{
    set_id("grid_window");
    switch(eWhich)
    {
        case SC_SPLIT_TOPLEFT:
            eHWhich = SC_SPLIT_LEFT;
            eVWhich = SC_SPLIT_TOP;
            break;
        case SC_SPLIT_TOPRIGHT:
            eHWhich = SC_SPLIT_RIGHT;
            eVWhich = SC_SPLIT_TOP;
            break;
        case SC_SPLIT_BOTTOMLEFT:
            eHWhich = SC_SPLIT_LEFT;
            eVWhich = SC_SPLIT_BOTTOM;
            break;
        case SC_SPLIT_BOTTOMRIGHT:
            eHWhich = SC_SPLIT_RIGHT;
            eVWhich = SC_SPLIT_BOTTOM;
            break;
        default:
            OSL_FAIL("GridWindow: wrong position");
    }

    SetBackground();

    SetMapMode(pViewData->GetLogicMode(eWhich));
    EnableChildTransparentMode();
    SetDialogControlFlags( DialogControlFlags::Return | DialogControlFlags::WantFocus );

    SetHelpId( HID_SC_WIN_GRIDWIN );

    SetDigitLanguage( SC_MOD()->GetOptDigitLanguage() );
    EnableRTL( false );
}

ScGridWindow::~ScGridWindow()
{
    disposeOnce();
}

void ScGridWindow::dispose()
{
    ImpDestroyOverlayObjects();

    mpFilterBox.disposeAndClear();
    mpFilterFloat.disposeAndClear();
    mpNoteMarker.reset();
    mpAutoFilterPopup.disposeAndClear();
    mpDPFieldPopup.disposeAndClear();
    aComboButton.SetOutputDevice(nullptr);

    vcl::Window::dispose();
}

void ScGridWindow::ClickExtern()
{
    do
    {
        // #i81298# don't delete the filter box when called from its select handler
        // (possible through row header size update)
        // #i84277# when initializing the filter box, a Basic error can deactivate the view
        if (mpFilterBox && (mpFilterBox->IsInSelect() || mpFilterBox->IsInInit()))
        {
            break;
        }
        mpFilterBox.disposeAndClear();
        mpFilterFloat.disposeAndClear();
    }
    while (false);

    if (mpDPFieldPopup)
    {
        mpDPFieldPopup->close(false);
        mpDPFieldPopup.disposeAndClear();
    }
}

IMPL_LINK_NOARG(ScGridWindow, PopupModeEndHdl, FloatingWindow*, void)
{
    if (mpFilterBox)
        mpFilterBox->SetCancelled();     // cancel select
    GrabFocus();
}

IMPL_LINK( ScGridWindow, PopupSpellingHdl, SpellCallbackInfo&, rInfo, void )
{
    if( rInfo.nCommand == SpellCallbackCommand::STARTSPELLDLG )
        pViewData->GetDispatcher().Execute( SID_SPELL_DIALOG, SfxCallMode::ASYNCHRON );
    else if (rInfo.nCommand == SpellCallbackCommand::AUTOCORRECT_OPTIONS)
        pViewData->GetDispatcher().Execute( SID_AUTO_CORRECT_DLG, SfxCallMode::ASYNCHRON );
}

namespace {

struct AutoFilterData : public ScCheckListMenuWindow::ExtendedData
{
    ScAddress maPos;
    ScDBData* mpData;
};

class AutoFilterAction : public ScMenuFloatingWindow::Action
{
    VclPtr<ScGridWindow> mpWindow;
    ScGridWindow::AutoFilterMode const meMode;
public:
    AutoFilterAction(ScGridWindow* p, ScGridWindow::AutoFilterMode eMode) :
        mpWindow(p), meMode(eMode) {}
    virtual void execute() override
    {
        mpWindow->UpdateAutoFilterFromMenu(meMode);
    }
};

class AutoFilterPopupEndAction : public ScMenuFloatingWindow::Action
{
    VclPtr<ScGridWindow> mpWindow;
    ScAddress const maPos;
public:
    AutoFilterPopupEndAction(ScGridWindow* p, const ScAddress& rPos) :
        mpWindow(p), maPos(rPos) {}
    virtual void execute() override
    {
        mpWindow->RefreshAutoFilterButton(maPos);
    }
};

class AddItemToEntry
{
    ScQueryEntry::QueryItemsType& mrItems;
    svl::SharedStringPool& mrPool;
public:
    AddItemToEntry(ScQueryEntry::QueryItemsType& rItems, svl::SharedStringPool& rPool) :
        mrItems(rItems), mrPool(rPool) {}
    void operator() (const ScCheckListMenuWindow::ResultEntry& rEntry)
    {
        if (rEntry.bValid)
        {
            ScQueryEntry::Item aNew;
            aNew.maString = mrPool.intern(rEntry.aName);
            aNew.meType = rEntry.bDate ? ScQueryEntry::ByDate : ScQueryEntry::ByString;
            aNew.mfVal = 0.0;
            mrItems.push_back(aNew);
        }
    }
};

class AddSelectedItemString
{
    std::unordered_set<OUString>& mrSet;
public:
    explicit AddSelectedItemString(std::unordered_set<OUString>& r) :
        mrSet(r) {}

    void operator() (const ScQueryEntry::Item& rItem)
    {
        mrSet.insert(rItem.maString.getString());
    }
};

void collectUIInformation(const OUString& aRow, const OUString& aCol)
{
    EventDescription aDescription;
    aDescription.aAction = "LAUNCH";
    aDescription.aID = "grid_window";
    aDescription.aParameters = {{"AUTOFILTER", ""},
        {"ROW", aRow}, {"COL", aCol}};
    aDescription.aParent = "MainWindow";
    aDescription.aKeyWord = "ScGridWinUIObject";

    UITestLogger::getInstance().logEvent(aDescription);
}

}

void ScGridWindow::LaunchAutoFilterMenu(SCCOL nCol, SCROW nRow)
{
    SCTAB nTab = pViewData->GetTabNo();
    ScDocument* pDoc = pViewData->GetDocument();

    mpAutoFilterPopup.disposeAndClear();
    int nColWidth = ScViewData::ToPixel(pDoc->GetColWidth(nCol, nTab), pViewData->GetPPTX());
    mpAutoFilterPopup.reset(VclPtr<ScCheckListMenuWindow>::Create(this, pDoc, nColWidth));

    // Avoid flicker when hovering over the menu items.
    if (!IsNativeControlSupported(ControlType::Pushbutton, ControlPart::Focus))
        // If NWF renders the focus rects itself, that breaks double-buffering.
        mpAutoFilterPopup->RequestDoubleBuffering(true);

    if (comphelper::LibreOfficeKit::isActive())
        mpAutoFilterPopup->SetLOKNotifier(SfxViewShell::Current());
    mpAutoFilterPopup->setOKAction(new AutoFilterAction(this, AutoFilterMode::Normal));
    mpAutoFilterPopup->setPopupEndAction(
        new AutoFilterPopupEndAction(this, ScAddress(nCol, nRow, nTab)));
    std::unique_ptr<AutoFilterData> pData(new AutoFilterData);
    pData->maPos = ScAddress(nCol, nRow, nTab);

    Point aPos = pViewData->GetScrPos(nCol, nRow, eWhich);
    long nSizeX  = 0;
    long nSizeY  = 0;
    pViewData->GetMergeSizePixel(nCol, nRow, nSizeX, nSizeY);
    tools::Rectangle aCellRect(OutputToScreenPixel(aPos), Size(nSizeX, nSizeY));

    ScDBData* pDBData = pDoc->GetDBAtCursor(nCol, nRow, nTab, ScDBDataPortion::AREA);
    if (!pDBData)
        return;

    pData->mpData = pDBData;
    mpAutoFilterPopup->setExtendedData(std::move(pData));

    ScQueryParam aParam;
    pDBData->GetQueryParam(aParam);
    std::vector<ScQueryEntry*> aEntries = aParam.FindAllEntriesByField(nCol);
    std::unordered_set<OUString> aSelected;
    for (ScQueryEntry* pEntry : aEntries)
    {
        if (pEntry && pEntry->bDoQuery && pEntry->eOp == SC_EQUAL)
        {
            ScQueryEntry::QueryItemsType& rItems = pEntry->GetQueryItems();
            std::for_each(rItems.begin(), rItems.end(), AddSelectedItemString(aSelected));
        }
    }

    // Populate the check box list.
    ScFilterEntries aFilterEntries;
    pDoc->GetFilterEntries(nCol, nRow, nTab, aFilterEntries);

    mpAutoFilterPopup->setHasDates(aFilterEntries.mbHasDates);
    mpAutoFilterPopup->setMemberSize(aFilterEntries.size());
    for (const auto& rEntry : aFilterEntries)
    {
        const OUString& aVal = rEntry.GetString();
        bool bSelected = true;
        if (!aSelected.empty())
            bSelected = aSelected.count(aVal) > 0;
        if ( rEntry.IsDate() )
            mpAutoFilterPopup->addDateMember( aVal, rEntry.GetValue(), bSelected );
        else
            mpAutoFilterPopup->addMember(aVal, bSelected);
    }
    mpAutoFilterPopup->initMembers();

    // Populate the menu.
    mpAutoFilterPopup->addMenuItem(
        ScResId(STR_MENU_SORT_ASC),
        new AutoFilterAction(this, AutoFilterMode::SortAscending));
    mpAutoFilterPopup->addMenuItem(
        ScResId(STR_MENU_SORT_DESC),
        new AutoFilterAction(this, AutoFilterMode::SortDescending));
    mpAutoFilterPopup->addSeparator();
    mpAutoFilterPopup->addMenuItem(
        ScResId(SCSTR_TOP10FILTER), new AutoFilterAction(this, AutoFilterMode::Top10));
    mpAutoFilterPopup->addMenuItem(
        ScResId(SCSTR_FILTER_EMPTY), new AutoFilterAction(this, AutoFilterMode::Empty));
    mpAutoFilterPopup->addMenuItem(
        ScResId(SCSTR_FILTER_NOTEMPTY), new AutoFilterAction(this, AutoFilterMode::NonEmpty));
    mpAutoFilterPopup->addSeparator();
    mpAutoFilterPopup->addMenuItem(
        ScResId(SCSTR_STDFILTER), new AutoFilterAction(this, AutoFilterMode::Custom));

    ScCheckListMenuWindow::Config aConfig;
    aConfig.mbAllowEmptySet = false;
    aConfig.mbRTL = pViewData->GetDocument()->IsLayoutRTL(pViewData->GetTabNo());
    mpAutoFilterPopup->setConfig(aConfig);
    mpAutoFilterPopup->launch(aCellRect);

    // remember filter rules before modification
    mpAutoFilterPopup->getResult(aSaveAutoFilterResult);

    collectUIInformation(OUString::number(nRow), OUString::number(nCol));
}

void ScGridWindow::RefreshAutoFilterButton(const ScAddress& rPos)
{
    if (mpFilterButton)
    {
        bool bFilterActive = IsAutoFilterActive(rPos.Col(), rPos.Row(), rPos.Tab());
        mpFilterButton->setHasHiddenMember(bFilterActive);
        mpFilterButton->setPopupPressed(false);
        mpFilterButton->draw();
    }
}

void ScGridWindow::UpdateAutoFilterFromMenu(AutoFilterMode eMode)
{
    const AutoFilterData* pData =
        static_cast<const AutoFilterData*>(mpAutoFilterPopup->getExtendedData());

    if (!pData)
        return;

    const ScAddress& rPos = pData->maPos;
    ScDBData* pDBData = pData->mpData;
    if (!pDBData)
        return;

    ScDocument* pDoc = pViewData->GetDocument();
    svl::SharedStringPool& rPool = pDoc->GetSharedStringPool();
    switch (eMode)
    {
        case AutoFilterMode::SortAscending:
        case AutoFilterMode::SortDescending:
        {
            SCCOL nCol = rPos.Col();
            ScSortParam aSortParam;
            pDBData->GetSortParam(aSortParam);
            if (nCol < aSortParam.nCol1 || nCol > aSortParam.nCol2)
                // out of bound
                return;

            bool bHasHeader = pDBData->HasHeader();

            aSortParam.bHasHeader = bHasHeader;
            aSortParam.bByRow = true;
            aSortParam.bCaseSens = false;
            aSortParam.bNaturalSort = false;
            aSortParam.bIncludeComments = false;
            aSortParam.bIncludeGraphicObjects = true;
            aSortParam.bIncludePattern = true;
            aSortParam.bInplace = true;
            aSortParam.maKeyState[0].bDoSort = true;
            aSortParam.maKeyState[0].nField = nCol;
            aSortParam.maKeyState[0].bAscending = (eMode == AutoFilterMode::SortAscending);

            for (size_t i = 1; i < aSortParam.GetSortKeyCount(); ++i)
                aSortParam.maKeyState[i].bDoSort = false;

            pViewData->GetViewShell()->UISort(aSortParam);
            return;
        }
        default:
            ;
    }

    if (eMode == AutoFilterMode::Custom)
    {
        ScRange aRange;
        pDBData->GetArea(aRange);
        pViewData->GetView()->MarkRange(aRange);
        pViewData->GetView()->SetCursor(rPos.Col(), rPos.Row());
        pViewData->GetDispatcher().Execute(SID_FILTER, SfxCallMode::SLOT|SfxCallMode::RECORD);
        return;
    }
    if (eMode != AutoFilterMode::Top10
            && eMode != AutoFilterMode::Empty
            && eMode != AutoFilterMode::NonEmpty)
    {
        // do not recreate auto-filter rules if there is no any changes from the user
        ScCheckListMenuWindow::ResultType aResult;
        mpAutoFilterPopup->getResult(aResult);

        if (aResult == aSaveAutoFilterResult)
        {
            SAL_INFO("sc.ui", "nothing to do when autofilter entries are the same");
            return;
        }
    }

    ScQueryParam aParam;
    pDBData->GetQueryParam(aParam);

    // Remove old entries.
    aParam.RemoveAllEntriesByField(rPos.Col());

    if( !(eMode == AutoFilterMode::Normal && mpAutoFilterPopup->isAllSelected() ) )
    {
        // Try to use the existing entry for the column (if one exists).
        ScQueryEntry* pEntry = aParam.FindEntryByField(rPos.Col(), true);

        if (!pEntry)
            // Something went terribly wrong!
            return;

        if (ScTabViewShell::isAnyEditViewInRange(/*bColumns*/ false, aParam.nRow1, aParam.nRow2))
            return;

        pEntry->bDoQuery = true;
        pEntry->nField = rPos.Col();
        pEntry->eConnect = SC_AND;

        switch (eMode)
        {
            case AutoFilterMode::Normal:
            {
                pEntry->eOp = SC_EQUAL;

                ScCheckListMenuWindow::ResultType aResult;
                mpAutoFilterPopup->getResult(aResult);

                ScQueryEntry::QueryItemsType& rItems = pEntry->GetQueryItems();
                rItems.clear();
                std::for_each(aResult.begin(), aResult.end(), AddItemToEntry(rItems, rPool));

                if (mpAutoFilterPopup->isAllSelected())
                {
                    // get all strings from the column
                    std::vector<ScTypedStrData> aAllStrings; // case sensitive
                    pDoc->GetDataEntries(rPos.Col(), rPos.Row(), rPos.Tab(), aAllStrings, true);

                    if (rItems.size() == aAllStrings.size() || aAllStrings.empty())
                    {
                        // all selected => Remove filter entries
                        aParam.RemoveAllEntriesByField(rPos.Col());
                    }
                }
            }
            break;
            case AutoFilterMode::Top10:
                pEntry->eOp = SC_TOPVAL;
                pEntry->GetQueryItem().meType = ScQueryEntry::ByString;
                pEntry->GetQueryItem().maString = rPool.intern("10");
            break;
            case AutoFilterMode::Empty:
                pEntry->SetQueryByEmpty();
            break;
            case AutoFilterMode::NonEmpty:
                pEntry->SetQueryByNonEmpty();
            break;
            default:
                // We don't know how to handle this!
                return;
        }
    }

    pViewData->GetView()->Query(aParam, nullptr, true);
    pDBData->SetQueryParam(aParam);
}

namespace {

void getCellGeometry(Point& rScrPos, Size& rScrSize, const ScViewData* pViewData, SCCOL nCol, SCROW nRow, ScSplitPos eWhich)
{
    // Get the screen position of the cell.
    rScrPos = pViewData->GetScrPos(nCol, nRow, eWhich);

    // Get the screen size of the cell.
    long nSizeX, nSizeY;
    pViewData->GetMergeSizePixel(nCol, nRow, nSizeX, nSizeY);
    rScrSize = Size(nSizeX-1, nSizeY-1);
}

}

void ScGridWindow::LaunchPageFieldMenu( SCCOL nCol, SCROW nRow )
{
    if (nCol == 0)
        // We assume that the page field button is located in cell to the immediate left.
        return;

    SCTAB nTab = pViewData->GetTabNo();
    ScDPObject* pDPObj = pViewData->GetDocument()->GetDPAtCursor(nCol, nRow, nTab);
    if (!pDPObj)
        return;

    Point aScrPos;
    Size aScrSize;
    getCellGeometry(aScrPos, aScrSize, pViewData, nCol, nRow, eWhich);
    DPLaunchFieldPopupMenu(OutputToScreenPixel(aScrPos), aScrSize, ScAddress(nCol-1, nRow, nTab), pDPObj);
}

void ScGridWindow::LaunchDPFieldMenu( SCCOL nCol, SCROW nRow )
{
    SCTAB nTab = pViewData->GetTabNo();
    ScDPObject* pDPObj = pViewData->GetDocument()->GetDPAtCursor(nCol, nRow, nTab);
    if (!pDPObj)
        return;

    Point aScrPos;
    Size aScrSize;
    getCellGeometry(aScrPos, aScrSize, pViewData, nCol, nRow, eWhich);
    DPLaunchFieldPopupMenu(OutputToScreenPixel(aScrPos), aScrSize, ScAddress(nCol, nRow, nTab), pDPObj);
}

void ScGridWindow::DoScenarioMenu( const ScRange& rScenRange )
{
    ScDocument* pDoc = pViewData->GetDocument();
    mpFilterBox.disposeAndClear();
    mpFilterFloat.disposeAndClear();

    SCCOL nCol = rScenRange.aEnd.Col();     // Cell is below the Buttons
    SCROW nRow = rScenRange.aStart.Row();
    if (nRow == 0)
    {
        nRow = rScenRange.aEnd.Row() + 1;   // Range at very the top -> Button below
        if (nRow>pDoc->MaxRow()) nRow = pDoc->MaxRow();
        //! Add text height (if it is stored in the View...)
    }

    SCTAB nTab = pViewData->GetTabNo();
    bool bLayoutRTL = pDoc->IsLayoutRTL( nTab );

    long nSizeX  = 0;
    long nSizeY  = 0;
    long nHeight = 0;
    pViewData->GetMergeSizePixel( nCol, nRow, nSizeX, nSizeY );
    // The button height should not use the merged cell height, should still use single row height
    nSizeY = ScViewData::ToPixel(pDoc->GetRowHeight(nRow, nTab), pViewData->GetPPTY());
    Point aPos = pViewData->GetScrPos( nCol, nRow, eWhich );
    if ( bLayoutRTL )
        aPos.AdjustX( -nSizeX );
    tools::Rectangle aCellRect( OutputToScreenPixel(aPos), Size(nSizeX,nSizeY) );
    aCellRect.AdjustTop( -nSizeY );
    aCellRect.AdjustBottom( -(nSizeY - 1) );
    //  Place the ListBox directly below the black line of the cell grid
    //  (It looks odd if the line gets hidden...)

    mpFilterFloat.reset(VclPtr<ScFilterFloatingWindow>::Create(this, WinBits(WB_BORDER)));
    mpFilterFloat->SetPopupModeEndHdl( LINK( this, ScGridWindow, PopupModeEndHdl ) );
    mpFilterBox.reset(VclPtr<ScFilterListBox>::Create(mpFilterFloat.get(), this, nCol, nRow, ScFilterBoxMode::Scenario));
    if (bLayoutRTL)
        mpFilterBox->EnableMirroring();

    nSizeX += 1;

    {
        vcl::Font aOldFont = GetFont();
        SetFont(mpFilterBox->GetFont());
        MapMode aOldMode = GetMapMode();
        SetMapMode(MapMode(MapUnit::MapPixel));

        nHeight  = GetTextHeight();
        nHeight *= SC_FILTERLISTBOX_LINES;

        SetMapMode( aOldMode );
        SetFont( aOldFont );
    }

    //  SetSize later

    //  ParentSize missing check
    Size aSize( nSizeX, nHeight );
    mpFilterBox->SetSizePixel( aSize );
    mpFilterBox->Show();                 // Show has to be before SetUpdateMode !!!
    mpFilterBox->SetUpdateMode(false);

    //  SetOutputSizePixel/StartPopupMode first below, when the size is set

    //  Listbox fill

    long nMaxText = 0;
    OUString aCurrent;
    OUString aTabName;
    SCTAB nTabCount = pDoc->GetTableCount();
    SCTAB nEntryCount = 0;
    for (SCTAB i=nTab+1; i<nTabCount && pDoc->IsScenario(i); i++)
    {
        if (pDoc->HasScenarioRange( i, rScenRange ))
            if (pDoc->GetName( i, aTabName ))
            {
                mpFilterBox->InsertEntry(aTabName);
                if (pDoc->IsActiveScenario(i))
                    aCurrent = aTabName;
                long nTextWidth = mpFilterBox->GetTextWidth(aTabName);
                if ( nTextWidth > nMaxText )
                    nMaxText = nTextWidth;
                ++nEntryCount;
            }
    }
    if (nEntryCount > SC_FILTERLISTBOX_LINES)
        nMaxText += GetSettings().GetStyleSettings().GetScrollBarSize();
    nMaxText += 4;          // for Rand
    if ( nMaxText > 300 )
        nMaxText = 300;     // do not over do it (Pixel)

    if (nMaxText > nSizeX)  // Adjust size to what is needed
    {
        long nDiff = nMaxText - nSizeX;
        aSize = Size( nMaxText, nHeight );
        mpFilterBox->SetSizePixel(aSize);
        mpFilterFloat->SetOutputSizePixel(aSize);

        if ( !bLayoutRTL )
        {
            //  also move popup position
            long nNewX = aCellRect.Left() - nDiff;
            if ( nNewX < 0 )
                nNewX = 0;
            aCellRect.SetLeft( nNewX );
        }
    }

    mpFilterFloat->SetOutputSizePixel( aSize );
    mpFilterFloat->StartPopupMode( aCellRect, FloatWinPopupFlags::Down|FloatWinPopupFlags::GrabFocus );

    mpFilterBox->SetUpdateMode(true);
    mpFilterBox->GrabFocus();

    sal_Int32 nPos = LISTBOX_ENTRY_NOTFOUND;
    if (!aCurrent.isEmpty())
    {
        nPos = mpFilterBox->GetEntryPos(aCurrent);
    }
    if (LISTBOX_ENTRY_NOTFOUND == nPos && mpFilterBox->GetEntryCount() > 0 )
    {
        nPos = 0;
    }
    if (LISTBOX_ENTRY_NOTFOUND != nPos )
    {
        mpFilterBox->SelectEntryPos(nPos);
    }
    mpFilterBox->EndInit();

    // Scenario selection comes from MouseButtonDown:
    // The next MouseMove on the FilterBox is like a ButtonDown
    nMouseStatus = SC_GM_FILTER;
    CaptureMouse();
}

void ScGridWindow::LaunchDataSelectMenu( SCCOL nCol, SCROW nRow )
{
    mpFilterBox.disposeAndClear();
    mpFilterFloat.disposeAndClear();

    ScDocument* pDoc = pViewData->GetDocument();
    SCTAB nTab = pViewData->GetTabNo();
    bool bLayoutRTL = pDoc->IsLayoutRTL( nTab );

    long nSizeX  = 0;
    long nSizeY  = 0;
    long nHeight = 0;
    pViewData->GetMergeSizePixel( nCol, nRow, nSizeX, nSizeY );
    Point aPos = pViewData->GetScrPos( nCol, nRow, eWhich );
    if ( bLayoutRTL )
        aPos.AdjustX( -nSizeX );

    tools::Rectangle aCellRect( OutputToScreenPixel(aPos), Size(nSizeX,nSizeY) );

    aPos.AdjustX( -1 );
    aPos.AdjustY( nSizeY - 1 );

    mpFilterFloat.reset(VclPtr<ScFilterFloatingWindow>::Create(this, WinBits(WB_BORDER)));
    if (comphelper::LibreOfficeKit::isActive())
    {
        mpFilterFloat->SetLOKNotifier(SfxViewShell::Current());
    }
    mpFilterFloat->SetPopupModeEndHdl(LINK( this, ScGridWindow, PopupModeEndHdl));
    mpFilterBox.reset(VclPtr<ScFilterListBox>::Create(mpFilterFloat.get(), this, nCol, nRow, ScFilterBoxMode::DataSelect));
    // Fix for bug fdo#44925
    if (AllSettings::GetLayoutRTL() != bLayoutRTL)
        mpFilterBox->EnableMirroring();

    nSizeX += 1;

    {
        vcl::Font aOldFont = GetFont();
        SetFont(mpFilterBox->GetFont());
        MapMode aOldMode = GetMapMode();
        SetMapMode(MapMode(MapUnit::MapPixel));

        nHeight  = GetTextHeight();
        nHeight *= SC_FILTERLISTBOX_LINES;

        SetMapMode( aOldMode );
        SetFont( aOldFont );
    }

    // SetSize later

    bool bEmpty = false;
    std::vector<ScTypedStrData> aStrings; // case sensitive
    // Fill List
    pDoc->GetDataEntries(nCol, nRow, nTab, aStrings);
    if (aStrings.empty())
        bEmpty = true;

    if (!bEmpty)
    {
        //  Adjust position and size to Window
        //! Check first if the entries fit (width)

        // minimum width in pixel
        const long nMinLOKWinWidth = static_cast<long>(1.3 * STD_COL_WIDTH * pViewData->GetPPTX());
        if (comphelper::LibreOfficeKit::isActive() && nSizeX < nMinLOKWinWidth)
            nSizeX = nMinLOKWinWidth;

        Size aParentSize = GetParent()->GetOutputSizePixel();
        Size aSize( nSizeX, nHeight );

        if ( aSize.Height() > aParentSize.Height() )
            aSize.setHeight( aParentSize.Height() );
        if ( aPos.Y() + aSize.Height() > aParentSize.Height() )
            aPos.setY( aParentSize.Height() - aSize.Height() );

        mpFilterBox->SetSizePixel(aSize);
        mpFilterBox->Show();                 // Show has to be before SetUpdateMode !!!
        mpFilterBox->SetUpdateMode(false);

        mpFilterFloat->SetOutputSizePixel(aSize);
        mpFilterFloat->StartPopupMode(aCellRect, FloatWinPopupFlags::Down | FloatWinPopupFlags::GrabFocus);

        // Fill Listbox
        bool bWait = aStrings.size() > 100;

        if (bWait)
            EnterWait();

        for (const auto& rString : aStrings)
            mpFilterBox->InsertEntry(rString.GetString());

        if (bWait)
            LeaveWait();

        mpFilterBox->SetUpdateMode(true);
    }

    sal_Int32 nSelPos = LISTBOX_ENTRY_NOTFOUND;

    sal_uLong nIndex = pDoc->GetAttr( nCol, nRow, nTab, ATTR_VALIDDATA )->GetValue();
    if ( nIndex )
    {
        const ScValidationData* pData = pDoc->GetValidationEntry( nIndex );
        if (pData)
        {
            std::unique_ptr<ScTypedStrData> pNew;
            OUString aDocStr = pDoc->GetString(nCol, nRow, nTab);
            if ( pDoc->HasValueData( nCol, nRow, nTab ) )
            {
                double fVal = pDoc->GetValue(ScAddress(nCol, nRow, nTab));
                pNew.reset(new ScTypedStrData(aDocStr, fVal, ScTypedStrData::Value));
            }
            else
                pNew.reset(new ScTypedStrData(aDocStr, 0.0, ScTypedStrData::Standard));

            if (pData->GetListType() == css::sheet::TableValidationVisibility::SORTEDASCENDING)
            {
                auto it = std::lower_bound(aStrings.begin(), aStrings.end(), *pNew, ScTypedStrData::LessCaseSensitive());
                if (it != aStrings.end() && ScTypedStrData::EqualCaseSensitive()(*it, *pNew))
                    nSelPos = static_cast<sal_Int32>(std::distance(aStrings.begin(), it));
            }
            else
            {
                auto it = std::find_if(aStrings.begin(), aStrings.end(), FindTypedStrData(*pNew, true));
                if (it != aStrings.end())
                    nSelPos = static_cast<sal_Int32>(std::distance(aStrings.begin(), it));
            }
        }
    }

    // Do not show an empty selection List:

    if ( bEmpty )
    {
        mpFilterBox.disposeAndClear();
        mpFilterFloat.disposeAndClear();
    }
    else
    {
        mpFilterBox->GrabFocus();

        // Select only after GrabFocus, so that the focus rectangle gets correct
        if ( LISTBOX_ENTRY_NOTFOUND != nSelPos )
            mpFilterBox->SelectEntryPos(nSelPos);
        else
        {
            mpFilterBox->SetNoSelection();
        }

        mpFilterBox->EndInit();
    }
}

void ScGridWindow::FilterSelect( sal_uLong nSel )
{
    OUString aString = mpFilterBox->GetEntry(static_cast<sal_Int32>(nSel));

    SCCOL nCol = mpFilterBox->GetCol();
    SCROW nRow = mpFilterBox->GetRow();
    switch (mpFilterBox->GetMode())
    {
        case ScFilterBoxMode::DataSelect:
            ExecDataSelect(nCol, nRow, aString);
            break;
        case ScFilterBoxMode::Scenario:
            pViewData->GetView()->UseScenario(aString);
            break;
    }

    if (mpFilterFloat)
        mpFilterFloat->EndPopupMode();

    GrabFocus();        // Otherwise the focus would be wrong on OS/2
}

void ScGridWindow::ExecDataSelect( SCCOL nCol, SCROW nRow, const OUString& rStr )
{
    ScModule* pScMod = SC_MOD();
    ScInputHandler* pViewHdl = pScMod->GetInputHdl(pViewData->GetViewShell());
    if (pViewHdl && pViewData->HasEditView(pViewData->GetActivePart()))
        pViewHdl->CancelHandler();

    SCTAB nTab = pViewData->GetTabNo();
    ScViewFunc* pView = pViewData->GetView();
    pView->EnterData( nCol, nRow, nTab, rStr );

    // #i52307# CellContentChanged is not in EnterData so it isn't called twice
    // if the cursor is moved afterwards.
    pView->CellContentChanged();
}

void ScGridWindow::MoveMouseStatus( ScGridWindow& rDestWin )
{
    if (nButtonDown)
    {
        rDestWin.nButtonDown = nButtonDown;
        rDestWin.nMouseStatus = nMouseStatus;
    }

    if (bRFMouse)
    {
        rDestWin.bRFMouse = bRFMouse;
        rDestWin.bRFSize  = bRFSize;
        rDestWin.nRFIndex = nRFIndex;
        rDestWin.nRFAddX  = nRFAddX;
        rDestWin.nRFAddY  = nRFAddY;
        bRFMouse = false;
    }

    if (nPagebreakMouse)
    {
        rDestWin.nPagebreakMouse  = nPagebreakMouse;
        rDestWin.nPagebreakBreak  = nPagebreakBreak;
        rDestWin.nPagebreakPrev   = nPagebreakPrev;
        rDestWin.aPagebreakSource = aPagebreakSource;
        rDestWin.aPagebreakDrag   = aPagebreakDrag;
        nPagebreakMouse = SC_PD_NONE;
    }
}

bool ScGridWindow::TestMouse( const MouseEvent& rMEvt, bool bAction )
{
    //  MouseEvent buttons must only be checked if bAction==TRUE
    //  to allow changing the mouse pointer in MouseMove,
    //  but not start AutoFill with right button (#74229#).
    //  with bAction==sal_True, SetFillMode / SetDragMode is called

    if ( bAction && !rMEvt.IsLeft() )
        return false;

    bool bNewPointer = false;

    SfxInPlaceClient* pClient = pViewData->GetViewShell()->GetIPClient();
    bool bOleActive = ( pClient && pClient->IsObjectInPlaceActive() );

    if ( pViewData->IsActive() && !bOleActive )
    {
        ScDocument* pDoc = pViewData->GetDocument();
        SCTAB nTab = pViewData->GetTabNo();
        bool bLayoutRTL = pDoc->IsLayoutRTL( nTab );

        //  Auto-Fill

        ScRange aMarkRange;
        if (pViewData->GetSimpleArea( aMarkRange ) == SC_MARK_SIMPLE)
        {
            if (aMarkRange.aStart.Tab() == pViewData->GetTabNo() && mpAutoFillRect)
            {
                Point aMousePos = rMEvt.GetPosPixel();
                if (mpAutoFillRect->IsInside(aMousePos))
                {
                    SetPointer( PointerStyle::Cross );     //! bold cross ?
                    if (bAction)
                    {
                        SCCOL nX = aMarkRange.aEnd.Col();
                        SCROW nY = aMarkRange.aEnd.Row();

                        if ( lcl_IsEditableMatrix( pViewData->GetDocument(), aMarkRange ) )
                            pViewData->SetDragMode(
                                aMarkRange.aStart.Col(), aMarkRange.aStart.Row(), nX, nY, ScFillMode::MATRIX );
                        else
                            pViewData->SetFillMode(
                                aMarkRange.aStart.Col(), aMarkRange.aStart.Row(), nX, nY );

                        //  The simple selection must also be recognized when dragging,
                        //  where the Marking flag is set and MarkToSimple won't work anymore.
                        pViewData->GetMarkData().MarkToSimple();
                    }
                    bNewPointer = true;
                }
            }
        }

        //  Embedded rectangle

        if (pDoc->IsEmbedded())
        {
            ScRange aRange;
            pDoc->GetEmbedded( aRange );
            if ( pViewData->GetTabNo() == aRange.aStart.Tab() )
            {
                Point aStartPos = pViewData->GetScrPos( aRange.aStart.Col(), aRange.aStart.Row(), eWhich );
                Point aEndPos   = pViewData->GetScrPos( aRange.aEnd.Col()+1, aRange.aEnd.Row()+1, eWhich );
                Point aMousePos = rMEvt.GetPosPixel();
                if ( bLayoutRTL )
                {
                    aStartPos.AdjustX(2 );
                    aEndPos.AdjustX(2 );
                }
                bool bTop = ( aMousePos.X() >= aStartPos.X()-3 && aMousePos.X() <= aStartPos.X()+1 &&
                              aMousePos.Y() >= aStartPos.Y()-3 && aMousePos.Y() <= aStartPos.Y()+1 );
                bool bBottom = ( aMousePos.X() >= aEndPos.X()-3 && aMousePos.X() <= aEndPos.X()+1 &&
                                 aMousePos.Y() >= aEndPos.Y()-3 && aMousePos.Y() <= aEndPos.Y()+1 );
                if ( bTop || bBottom )
                {
                    SetPointer( PointerStyle::Cross );
                    if (bAction)
                    {
                        ScFillMode nMode = bTop ? ScFillMode::EMBED_LT : ScFillMode::EMBED_RB;
                        pViewData->SetDragMode(
                                    aRange.aStart.Col(), aRange.aStart.Row(),
                                    aRange.aEnd.Col(), aRange.aEnd.Row(), nMode );
                    }
                    bNewPointer = true;
                }
            }
        }
    }

    if (!bNewPointer && bAction)
    {
        pViewData->ResetFillMode();
    }

    return bNewPointer;
}

void ScGridWindow::LogicMouseButtonDown(const MouseEvent& rMouseEvent)
{
    MouseButtonDown(rMouseEvent);
}

void ScGridWindow::LogicMouseButtonUp(const MouseEvent& rMouseEvent)
{
    MouseButtonUp(rMouseEvent);
}

void ScGridWindow::LogicMouseMove(const MouseEvent& rMouseEvent)
{
    MouseMove(rMouseEvent);
}

void ScGridWindow::MouseButtonDown( const MouseEvent& rMEvt )
{
    nNestedButtonState = ScNestedButtonState::Down;

    MouseEventState aState;
    HandleMouseButtonDown(rMEvt, aState);
    if (aState.mbActivatePart)
        pViewData->GetView()->ActivatePart(eWhich);

    if ( nNestedButtonState == ScNestedButtonState::Up )
    {
        // #i41690# If an object is deactivated from MouseButtonDown, it might reschedule,
        // so MouseButtonUp comes before the MouseButtonDown call is finished. In this case,
        // simulate another MouseButtonUp call, so the selection state is consistent.

        nButtonDown = rMEvt.GetButtons();
        FakeButtonUp();

        if ( IsTracking() )
            EndTracking();      // normally done in VCL as part of MouseButtonUp handling
    }
    nNestedButtonState = ScNestedButtonState::NONE;
}

bool ScGridWindow::IsCellCoveredByText(SCCOL nPosX, SCROW nPosY, SCTAB nTab, SCCOL &rTextStartPosX)
{
    ScDocument* pDoc = pViewData->GetDocument();

    // find the first non-empty cell (this, or to the left)
    SCCOL nNonEmptyX = nPosX;
    for (; nNonEmptyX >= 0; --nNonEmptyX)
    {
        ScRefCellValue aCell(*pDoc, ScAddress(nNonEmptyX, nPosY, nTab));
        if (!aCell.isEmpty())
            break;
    }

    // the initial cell already contains text
    if (nNonEmptyX == nPosX)
    {
        rTextStartPosX = nNonEmptyX;
        return true;
    }

    // to the left, there is no cell that would contain (potentially
    // overrunning) text
    if (nNonEmptyX < 0 || pDoc->HasAttrib(nNonEmptyX, nPosY, nTab, nPosX, nPosY, nTab, HasAttrFlags::Merged | HasAttrFlags::Overlapped))
        return false;

    double nPPTX = pViewData->GetPPTX();
    double nPPTY = pViewData->GetPPTY();

    ScTableInfo aTabInfo;
    pDoc->FillInfo(aTabInfo, 0, nPosY, nPosX, nPosY, nTab, nPPTX, nPPTY, false, false);

    Fraction aZoomX = pViewData->GetZoomX();
    Fraction aZoomY = pViewData->GetZoomY();
    ScOutputData aOutputData(this, OUTTYPE_WINDOW, aTabInfo, pDoc, nTab,
            0, 0, 0, nPosY, nPosX, nPosY, nPPTX, nPPTY,
            &aZoomX, &aZoomY);

    MapMode aCurrentMapMode(GetMapMode());
    SetMapMode(MapMode(MapUnit::MapPixel));

    // obtain the bounding box of the text in first non-empty cell
    // to the left
    tools::Rectangle aRect(aOutputData.LayoutStrings(false, false, ScAddress(nNonEmptyX, nPosY, nTab)));

    SetMapMode(aCurrentMapMode);

    // the text does not overrun from the cell
    if (aRect.IsEmpty())
        return false;

    SCCOL nTextEndX;
    SCROW nTextEndY;

    // test the rightmost position of the text bounding box
    long nMiddle = (aRect.Top() + aRect.Bottom()) / 2;
    pViewData->GetPosFromPixel(aRect.Right(), nMiddle, eWhich, nTextEndX, nTextEndY);
    if (nTextEndX >= nPosX)
    {
        rTextStartPosX = nNonEmptyX;
        return true;
    }

    return false;
}

void ScGridWindow::HandleMouseButtonDown( const MouseEvent& rMEvt, MouseEventState& rState )
{
    // We have to check if a context menu is shown and we have an UI
    // active inplace client. In that case we have to ignore the event.
    // Otherwise we would crash (context menu has been
    // opened by inplace client and we would deactivate the inplace client,
    // the contex menu is closed by VCL asynchronously which in the end
    // would work on deleted objects or the context menu has no parent anymore)
    SfxViewShell* pViewSh = pViewData->GetViewShell();
    SfxInPlaceClient* pClient = pViewSh->GetIPClient();
    if ( pClient &&
         pClient->IsObjectInPlaceActive() &&
         PopupMenu::IsInExecute() )
        return;

    aCurMousePos = rMEvt.GetPosPixel();

    // Filter popup is ended with its own mouse click, not when clicking into the Grid Window,
    // so the following query is no longer necessary:
    ClickExtern();  // deletes FilterBox when available

    HideNoteMarker();

    bEEMouse = false;

    ScModule* pScMod = SC_MOD();
    if (pScMod->IsModalMode(pViewData->GetSfxDocShell()))
        return;

    pScActiveViewShell = pViewData->GetViewShell();         // if left is clicked
    nScClickMouseModifier = rMEvt.GetModifier();            // to always catch a control click

    bool bDetective = pViewData->GetViewShell()->IsAuditShell();
    bool bRefMode = pViewData->IsRefMode();                 // Start reference
    bool bFormulaMode = pScMod->IsFormulaMode();            // next click -> reference
    bool bEditMode = pViewData->HasEditView(eWhich);        // also in Mode==SC_INPUT_TYPE
    bool bDouble = (rMEvt.GetClicks() == 2);
    ScDocument* pDoc = pViewData->GetDocument();
    bool bIsTiledRendering = comphelper::LibreOfficeKit::isActive();

    // DeactivateIP does only happen when MarkListHasChanged

    // An error message can show up during GrabFocus call
    // (for instance when renaming tables per sheet title)

    if ( !nButtonDown || !bDouble )             // single (first) click is always valid
        nButtonDown = rMEvt.GetButtons();       // set nButtonDown first, so StopMarking works

    // special handling of empty cells with tiled rendering
    if (bIsTiledRendering)
    {
        Point aPos(rMEvt.GetPosPixel());
        SCCOL nPosX, nNonEmptyX(0);
        SCROW nPosY;
        SCTAB nTab = pViewData->GetTabNo();
        pViewData->GetPosFromPixel(aPos.X(), aPos.Y(), eWhich, nPosX, nPosY);

        ScRefCellValue aCell(*pDoc, ScAddress(nPosX, nPosY, nTab));
        bool bIsEmpty = aCell.isEmpty();
        bool bIsCoveredByText = bIsEmpty && IsCellCoveredByText(nPosX, nPosY, nTab, nNonEmptyX);

        if (bIsCoveredByText)
        {
            // if there's any text flowing to this cell, activate the
            // editengine, so that the text actually gets the events
            if (bDouble)
            {
                ScViewFunc* pView = pViewData->GetView();

                pView->SetCursor(nNonEmptyX, nPosY);
                SC_MOD()->SetInputMode(SC_INPUT_TABLE);

                bEditMode = pViewData->HasEditView(eWhich);
                assert(bEditMode);

                // synthesize the 1st click
                EditView* pEditView = pViewData->GetEditView(eWhich);
                MouseEvent aEditEvt(rMEvt.GetPosPixel(), 1, MouseEventModifiers::SYNTHETIC, MOUSE_LEFT, 0);
                pEditView->MouseButtonDown(aEditEvt);
                pEditView->MouseButtonUp(aEditEvt);
            }
        }
        else if (bIsEmpty && bEditMode && bDouble)
        {
            // double-click in an empty cell: the entire cell is selected
            SetCellSelectionPixel(LOK_SETTEXTSELECTION_START, aPos.X(), aPos.Y());
            SetCellSelectionPixel(LOK_SETTEXTSELECTION_END, aPos.X(), aPos.Y());
            return;
        }
    }

    if ( ( bEditMode && pViewData->GetActivePart() == eWhich ) || !bFormulaMode )
        GrabFocus();

    // #i31846# need to cancel a double click if the first click has set the "ignore" state,
    // but a single (first) click is always valid
    if ( nMouseStatus == SC_GM_IGNORE && bDouble )
    {
        nButtonDown = 0;
        nMouseStatus = SC_GM_NONE;
        return;
    }

    if ( bDetective )               // Detectiv fill mode
    {
        if ( rMEvt.IsLeft() && !rMEvt.GetModifier() )
        {
            Point   aPos = rMEvt.GetPosPixel();
            SCCOL  nPosX;
            SCROW  nPosY;
            pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nPosX, nPosY );

            SfxInt16Item aPosXItem( SID_RANGE_COL, nPosX );
            SfxInt32Item aPosYItem( SID_RANGE_ROW, nPosY );
            pViewData->GetDispatcher().ExecuteList(SID_FILL_SELECT,
                    SfxCallMode::SLOT | SfxCallMode::RECORD,
                    { &aPosXItem, &aPosYItem });

        }
        nButtonDown = 0;
        nMouseStatus = SC_GM_NONE;
        return;
    }

    if (!bDouble)
        nMouseStatus = SC_GM_NONE;

    rState.mbActivatePart = !bFormulaMode; // Don't activate when in formula mode.

    if (bFormulaMode)
    {
        ScViewSelectionEngine* pSelEng = pViewData->GetView()->GetSelEngine();
        pSelEng->SetWindow(this);
        pSelEng->SetWhich(eWhich);
        pSelEng->SetVisibleArea( tools::Rectangle(Point(), GetOutputSizePixel()) );
    }

    if (bEditMode && (pViewData->GetRefTabNo() == pViewData->GetTabNo()))
    {
        Point   aPos = rMEvt.GetPosPixel();
        SCCOL  nPosX;
        SCROW  nPosY;
        pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nPosX, nPosY );

        EditView*   pEditView;
        SCCOL       nEditCol;
        SCROW       nEditRow;
        pViewData->GetEditView( eWhich, pEditView, nEditCol, nEditRow );
        SCCOL nEndCol = pViewData->GetEditEndCol();
        SCROW nEndRow = pViewData->GetEditEndRow();

        if ( nPosX >= nEditCol && nPosX <= nEndCol &&
             nPosY >= nEditRow && nPosY <= nEndRow )
        {
            // when clicking in the table EditView, always reset the focus
            if (bFormulaMode)   // otherwise this has already happen above
                GrabFocus();

            pScMod->SetInputMode( SC_INPUT_TABLE );
            bEEMouse = true;
            pEditView->MouseButtonDown( rMEvt );
            return;
        }
    }

    if (pScMod->GetIsWaterCan())
    {
        //!     what's up with the Mac ???
        if ( rMEvt.GetModifier() + rMEvt.GetButtons() == MOUSE_RIGHT )
        {
            nMouseStatus = SC_GM_WATERUNDO;
            return;
        }
    }

    // Order that matches the displayed Cursor:
    // RangeFinder, AutoFill, PageBreak, Drawing

    RfCorner rCorner = NONE;
    bool bFound = HitRangeFinder(rMEvt.GetPosPixel(), rCorner, &nRFIndex, &nRFAddX, &nRFAddY);
    bRFSize = (rCorner != NONE);
    aRFSelectedCorned = rCorner;

    if (bFound)
    {
        bRFMouse = true;        // the other variables are initialized above

        rState.mbActivatePart = true; // always activate ?
        StartTracking();
        return;
    }

    bool bCrossPointer = TestMouse( rMEvt, true );
    if ( bCrossPointer )
    {
        if ( bDouble )
            pViewData->GetView()->FillCrossDblClick();
        else
            pScMod->InputEnterHandler();                                // Autofill etc.
    }

    if ( !bCrossPointer )
    {
        nPagebreakMouse = HitPageBreak( rMEvt.GetPosPixel(), &aPagebreakSource,
                                            &nPagebreakBreak, &nPagebreakPrev );
        if (nPagebreakMouse)
        {
            bPagebreakDrawn = false;
            StartTracking();
            PagebreakMove( rMEvt, false );
            return;
        }
    }

    // in the tiled rendering case, single clicks into drawing objects take
    // precedence over bEditMode
    if (((!bFormulaMode && !bEditMode) || bIsTiledRendering) && rMEvt.IsLeft())
    {
        if ( !bCrossPointer && DrawMouseButtonDown(rMEvt) )
        {
            return;
        }

        pViewData->GetViewShell()->SetDrawShell( false );               // no Draw-object selected

        // TestMouse has already happened above
    }

    Point aPos = rMEvt.GetPosPixel();
    SCCOL nPosX;
    SCROW nPosY;
    pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nPosX, nPosY );
    SCTAB nTab = pViewData->GetTabNo();

    // FIXME: this is to limit the number of rows handled in the Online
    // to 1000; this will be removed again when the performance
    // bottlenecks are sorted out
    if ( comphelper::LibreOfficeKit::isActive() && nPosY > MAXTILEDROW - 1 )
    {
        nButtonDown = 0;
        nMouseStatus = SC_GM_NONE;
        return;
    }

    // Auto filter / pivot table / data select popup.  This shouldn't activate the part.

    if ( !bDouble && !bFormulaMode && rMEvt.IsLeft() )
    {
        SCCOL nRealPosX;
        SCROW nRealPosY;
        pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nRealPosX, nRealPosY, false );//the real row/col
        const ScMergeFlagAttr* pRealPosAttr = pDoc->GetAttr( nRealPosX, nRealPosY, nTab, ATTR_MERGE_FLAG );
        const ScMergeFlagAttr* pAttr = pDoc->GetAttr( nPosX, nPosY, nTab, ATTR_MERGE_FLAG );
        if( pRealPosAttr->HasAutoFilter() )
        {
            SC_MOD()->InputEnterHandler();
            if (DoAutoFilterButton( nRealPosX, nRealPosY, rMEvt))
                return;
        }
        if (pAttr->HasAutoFilter())
        {
            if (DoAutoFilterButton(nPosX, nPosY, rMEvt))
            {
                rState.mbActivatePart = false;
                return;
            }
        }

        if (pAttr->HasPivotButton() || pAttr->HasPivotPopupButton())
        {
            DoPushPivotButton(nPosX, nPosY, rMEvt, pAttr->HasPivotButton(), pAttr->HasPivotPopupButton());
            rState.mbActivatePart = false;
            return;
        }

        //  List Validity drop-down button

        if ( bListValButton )
        {
            tools::Rectangle aButtonRect = GetListValButtonRect( aListValPos );
            if ( aButtonRect.IsInside( aPos ) )
            {
                // tdf#125917 typically we have the mouse captured already, except if are editing the cell.
                // Ensure its captured before the menu is launched even in the cell editing case
                CaptureMouse();

                LaunchDataSelectMenu( aListValPos.Col(), aListValPos.Row() );

                nMouseStatus = SC_GM_FILTER;    // not set in DoAutoFilterMenue for bDataSelect
                CaptureMouse();
                rState.mbActivatePart = false;
                return;
            }
        }
    }

            //      scenario selection

    ScRange aScenRange;
    if ( rMEvt.IsLeft() && HasScenarioButton( aPos, aScenRange ) )
    {
        DoScenarioMenu( aScenRange );
        return;
    }

            //      double click started ?

    // StopMarking can be called from DrawMouseButtonDown

    if ( nMouseStatus != SC_GM_IGNORE && !bRefMode )
    {
        if ( bDouble && !bCrossPointer )
        {
            if (nMouseStatus == SC_GM_TABDOWN)
                nMouseStatus = SC_GM_DBLDOWN;
        }
        else
            nMouseStatus = SC_GM_TABDOWN;
    }

            //      links in the edit cell

    bool bAlt = rMEvt.IsMod2();
    if ( !bAlt && rMEvt.IsLeft() && ScGlobal::ShouldOpenURL() &&
            GetEditUrl(rMEvt.GetPosPixel()) )           // click on link: do not move cursor
    {
        SetPointer( PointerStyle::RefHand );
        nMouseStatus = SC_GM_URLDOWN;                   // also only execute when ButtonUp
        return;
    }

            //      Gridwin - Selection Engine

    if ( rMEvt.IsLeft() )
    {
        ScViewSelectionEngine* pSelEng = pViewData->GetView()->GetSelEngine();
        pSelEng->SetWindow(this);
        pSelEng->SetWhich(eWhich);
        pSelEng->SetVisibleArea( tools::Rectangle(Point(), GetOutputSizePixel()) );

        //  SelMouseButtonDown on the View is still setting the bMoveIsShift flag
        if ( pViewData->GetView()->SelMouseButtonDown( rMEvt ) )
        {
            if (IsMouseCaptured())
            {
                //  Tracking instead of CaptureMouse, so it can be canceled cleanly
                //! Someday SelectionEngine should call StartTracking on its own!?!
                ReleaseMouse();
                StartTracking();
            }
            pViewData->GetMarkData().SetMarking(true);
            return;
        }
    }
}

void ScGridWindow::MouseButtonUp( const MouseEvent& rMEvt )
{
    aCurMousePos = rMEvt.GetPosPixel();
    ScDocument* pDoc = pViewData->GetDocument();
    ScMarkData& rMark = pViewData->GetMarkData();
    // #i41690# detect a MouseButtonUp call from within MouseButtonDown
    // (possible through Reschedule from storing an OLE object that is deselected)

    if ( nNestedButtonState == ScNestedButtonState::Down )
        nNestedButtonState = ScNestedButtonState::Up;

    if (nButtonDown != rMEvt.GetButtons())
        nMouseStatus = SC_GM_IGNORE;            // reset and return

    nButtonDown = 0;

    if (nMouseStatus == SC_GM_IGNORE)
    {
        nMouseStatus = SC_GM_NONE;
                                        // Selection engine: cancel selection
        pViewData->GetView()->GetSelEngine()->Reset();
        rMark.SetMarking(false);
        if (pViewData->IsAnyFillMode())
        {
            pViewData->GetView()->StopRefMode();
            pViewData->ResetFillMode();
        }
        StopMarking();
        DrawEndAction();                // cancel selection/moving in drawing layer
        ReleaseMouse();
        return;
    }

    if (nMouseStatus == SC_GM_FILTER)
    {
        nMouseStatus = SC_GM_NONE;
        ReleaseMouse();
        return;                         // nothing more should happen here
    }

    ScModule* pScMod = SC_MOD();
    if (pScMod->IsModalMode(pViewData->GetSfxDocShell()))
        return;

    SfxBindings& rBindings = pViewData->GetBindings();
    if (bEEMouse && pViewData->HasEditView( eWhich ))
    {
        EditView*   pEditView;
        SCCOL       nEditCol;
        SCROW       nEditRow;
        pViewData->GetEditView( eWhich, pEditView, nEditCol, nEditRow );
        pEditView->MouseButtonUp( rMEvt );

        if ( rMEvt.IsMiddle() &&
                 GetSettings().GetMouseSettings().GetMiddleButtonAction() == MouseMiddleButtonAction::PasteSelection )
        {
            //  EditView may have pasted from selection
            pScMod->InputChanged( pEditView );
        }
        else
            pScMod->InputSelection( pEditView );            // parentheses etc.

        pViewData->GetView()->InvalidateAttribs();
        rBindings.Invalidate( SID_HYPERLINK_GETLINK );
        bEEMouse = false;
        return;
    }

    if (bDPMouse)
    {
        DPMouseButtonUp( rMEvt );       // resets bDPMouse
        return;
    }

    if (bRFMouse)
    {
        RFMouseMove( rMEvt, true );     // Again the proper range
        bRFMouse = false;
        SetPointer( PointerStyle::Arrow );
        ReleaseMouse();
        return;
    }

    if (nPagebreakMouse)
    {
        PagebreakMove( rMEvt, true );
        nPagebreakMouse = SC_PD_NONE;
        SetPointer( PointerStyle::Arrow );
        ReleaseMouse();
        return;
    }

    if (nMouseStatus == SC_GM_WATERUNDO)    // Undo in format paintbrush mode
    {
        SfxUndoManager* pMgr = pViewData->GetDocShell()->GetUndoManager();
        if ( pMgr->GetUndoActionCount() && dynamic_cast<ScUndoSelectionStyle*>(pMgr->GetUndoAction()) )
            pMgr->Undo();
        return;
    }

    if (DrawMouseButtonUp(rMEvt))       // includes format paint brush handling for drawing objects
    {
        ScTabViewShell* pViewShell = pViewData->GetViewShell();
        SfxBindings& rFrmBindings=pViewShell->GetViewFrame()->GetBindings();
        rFrmBindings.Invalidate(SID_ATTR_TRANSFORM_WIDTH);
        rFrmBindings.Invalidate(SID_ATTR_TRANSFORM_HEIGHT);
        rFrmBindings.Invalidate(SID_ATTR_TRANSFORM_POS_X);
        rFrmBindings.Invalidate(SID_ATTR_TRANSFORM_POS_Y);
        rFrmBindings.Invalidate(SID_ATTR_TRANSFORM_ANGLE);
        rFrmBindings.Invalidate(SID_ATTR_TRANSFORM_ROT_X);
        rFrmBindings.Invalidate(SID_ATTR_TRANSFORM_ROT_Y);
        rFrmBindings.Invalidate(SID_ATTR_TRANSFORM_AUTOWIDTH);
        rFrmBindings.Invalidate(SID_ATTR_TRANSFORM_AUTOHEIGHT);
        return;
    }

    rMark.SetMarking(false);

    SetPointer( PointerStyle::Arrow );

    if (pViewData->IsFillMode() ||
        ( pViewData->GetFillMode() == ScFillMode::MATRIX && rMEvt.IsMod1() ))
    {
        nScFillModeMouseModifier = rMEvt.GetModifier();
        SCCOL nStartCol;
        SCROW nStartRow;
        SCCOL nEndCol;
        SCROW nEndRow;
        pViewData->GetFillData( nStartCol, nStartRow, nEndCol, nEndRow );
        ScRange aDelRange;
        bool bIsDel = pViewData->GetDelMark( aDelRange );

        ScViewFunc* pView = pViewData->GetView();
        pView->StopRefMode();
        pViewData->ResetFillMode();
        pView->GetFunctionSet().SetAnchorFlag( false );    // #i5819# don't use AutoFill anchor flag for selection

        if ( bIsDel )
        {
            pView->MarkRange( aDelRange, false );
            pView->DeleteContents( InsertDeleteFlags::CONTENTS );
            SCTAB nTab = pViewData->GetTabNo();
            ScRange aBlockRange( nStartCol, nStartRow, nTab, nEndCol, nEndRow, nTab );
            if ( aBlockRange != aDelRange )
            {
                if ( aDelRange.aStart.Row() == nStartRow )
                    aBlockRange.aEnd.SetCol( aDelRange.aStart.Col() - 1 );
                else
                    aBlockRange.aEnd.SetRow( aDelRange.aStart.Row() - 1 );
                pView->MarkRange( aBlockRange, false );
            }
        }
        else
            pViewData->GetDispatcher().Execute( FID_FILL_AUTO, SfxCallMode::SLOT | SfxCallMode::RECORD );
    }
    else if (pViewData->GetFillMode() == ScFillMode::MATRIX)
    {
        SCTAB nTab = pViewData->GetTabNo();
        SCCOL nStartCol;
        SCROW nStartRow;
        SCCOL nEndCol;
        SCROW nEndRow;
        pViewData->GetFillData( nStartCol, nStartRow, nEndCol, nEndRow );
        ScRange aBlockRange( nStartCol, nStartRow, nTab, nEndCol, nEndRow, nTab );
        SCCOL nFillCol = pViewData->GetRefEndX();
        SCROW nFillRow = pViewData->GetRefEndY();
        ScAddress aEndPos( nFillCol, nFillRow, nTab );

        ScTabView* pView = pViewData->GetView();
        pView->StopRefMode();
        pViewData->ResetFillMode();
        pView->GetFunctionSet().SetAnchorFlag( false );

        if ( aEndPos != aBlockRange.aEnd )
        {
            pViewData->GetDocShell()->GetDocFunc().ResizeMatrix( aBlockRange, aEndPos );
            pViewData->GetView()->MarkRange( ScRange( aBlockRange.aStart, aEndPos ) );
        }
    }
    else if (pViewData->IsAnyFillMode())
    {
                                                // Embedded area has been changed
        ScTabView* pView = pViewData->GetView();
        pView->StopRefMode();
        pViewData->ResetFillMode();
        pView->GetFunctionSet().SetAnchorFlag( false );
        pViewData->GetDocShell()->UpdateOle(pViewData);
    }

    bool bRefMode = pViewData->IsRefMode();
    if (bRefMode)
        pScMod->EndReference();

        // Format paintbrush mode (Switch)

    if (pScMod->GetIsWaterCan())
    {
        // Check on undo already done above

        ScStyleSheetPool* pStylePool = pViewData->GetDocument()->
                                            GetStyleSheetPool();
        if ( pStylePool )
        {
            SfxStyleSheet* pStyleSheet = static_cast<SfxStyleSheet*>(
                                         pStylePool->GetActualStyleSheet());

            if ( pStyleSheet )
            {
                SfxStyleFamily eFamily = pStyleSheet->GetFamily();

                switch ( eFamily )
                {
                    case SfxStyleFamily::Para:
                        pViewData->GetView()->SetStyleSheetToMarked( pStyleSheet );
                        pViewData->GetView()->DoneBlockMode();
                        break;

                    case SfxStyleFamily::Page:
                        pViewData->GetDocument()->SetPageStyle( pViewData->GetTabNo(),
                                                                pStyleSheet->GetName() );

                        ScPrintFunc( pViewData->GetDocShell(),
                                     pViewData->GetViewShell()->GetPrinter(true),
                                     pViewData->GetTabNo() ).UpdatePages();

                        rBindings.Invalidate( SID_STATUS_PAGESTYLE );
                        break;

                    default:
                        break;
                }
            }
        }
    }

    ScDBFunc* pView = pViewData->GetView();
    ScDocument* pBrushDoc = pView->GetBrushDocument();
    if ( pBrushDoc )
    {
        pView->PasteFromClip( InsertDeleteFlags::ATTRIB, pBrushDoc );
        if ( !pView->IsPaintBrushLocked() )
            pView->ResetBrushDocument();            // invalidates pBrushDoc pointer
    }

    // double click (only left button)
    // in the tiled rendering case, single click works this way too

    bool bIsTiledRendering = comphelper::LibreOfficeKit::isActive();
    bool bDouble = ( rMEvt.GetClicks() == 2 && rMEvt.IsLeft() );
    if ((bDouble || bIsTiledRendering)
            && !bRefMode
            && (nMouseStatus == SC_GM_DBLDOWN || (bIsTiledRendering && nMouseStatus != SC_GM_URLDOWN))
            && !pScMod->IsRefDialogOpen())
    {
        //  data pilot table
        Point aPos = rMEvt.GetPosPixel();
        SCCOL nPosX;
        SCROW nPosY;
        SCTAB nTab = pViewData->GetTabNo();
        pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nPosX, nPosY );
        ScDPObject* pDPObj  = pDoc->GetDPAtCursor( nPosX, nPosY, nTab );
        if ( pDPObj && pDPObj->GetSaveData()->GetDrillDown() )
        {
            ScAddress aCellPos( nPosX, nPosY, pViewData->GetTabNo() );

            // Check for header drill-down first.
            sheet::DataPilotTableHeaderData aData;
            pDPObj->GetHeaderPositionData(aCellPos, aData);

            if ( ( aData.Flags & sheet::MemberResultFlags::HASMEMBER ) &&
                 ! ( aData.Flags & sheet::MemberResultFlags::SUBTOTAL ) )
            {
                css::sheet::DataPilotFieldOrientation nDummy;
                if ( pView->HasSelectionForDrillDown( nDummy ) )
                {
                    // execute slot to show dialog
                    pViewData->GetDispatcher().Execute( SID_OUTLINE_SHOW, SfxCallMode::SLOT | SfxCallMode::RECORD );
                }
                else
                {
                    // toggle single entry
                    ScDPObject aNewObj( *pDPObj );
                    pDPObj->ToggleDetails( aData, &aNewObj );
                    ScDBDocFunc aFunc( *pViewData->GetDocShell() );
                    aFunc.DataPilotUpdate( pDPObj, &aNewObj, true, false );
                    pViewData->GetView()->CursorPosChanged();       // shells may be switched
                }
            }
            else
            {
                // Check if the data area is double-clicked.

                Sequence<sheet::DataPilotFieldFilter> aFilters;
                if ( pDPObj->GetDataFieldPositionData(aCellPos, aFilters) )
                    pViewData->GetView()->ShowDataPilotSourceData( *pDPObj, aFilters );
            }

            return;
        }

        // Check for cell protection attribute.
        ScTableProtection* pProtect = pDoc->GetTabProtection( nTab );
        bool bEditAllowed = true;
        if ( pProtect && pProtect->isProtected() )
        {
            bool bCellProtected = pDoc->HasAttrib(nPosX, nPosY, nTab, nPosX, nPosY, nTab, HasAttrFlags::Protected);
            bool bSkipProtected = !pProtect->isOptionEnabled(ScTableProtection::SELECT_LOCKED_CELLS);
            bool bSkipUnprotected = !pProtect->isOptionEnabled(ScTableProtection::SELECT_UNLOCKED_CELLS);

            if ( bSkipProtected && bSkipUnprotected )
                bEditAllowed = false;
            else if ( (bCellProtected && bSkipProtected) || (!bCellProtected && bSkipUnprotected) )
                bEditAllowed = false;
        }

        // We don't want to activate the edit view for a single click in tiled rendering
        // (but we should probably keep the same behaviour for double clicks).
        if ( bEditAllowed && (!bIsTiledRendering || bDouble) )
        {
            // don't forward the event to an empty cell, causes deselection in
            // case we used the double-click to select the empty cell
            if (bIsTiledRendering && bDouble)
            {
                ScRefCellValue aCell(*pViewData->GetDocument(), ScAddress(nPosX, nPosY, nTab));
                if (aCell.isEmpty())
                    return;
            }

            //  edit cell contents
            pViewData->GetViewShell()->UpdateInputHandler();
            pScMod->SetInputMode( SC_INPUT_TABLE );
            if (pViewData->HasEditView(eWhich))
            {
                // Set text cursor where clicked
                EditView* pEditView = pViewData->GetEditView( eWhich );
                MouseEvent aEditEvt( rMEvt.GetPosPixel(), 1, MouseEventModifiers::SYNTHETIC, MOUSE_LEFT, 0 );
                pEditView->MouseButtonDown( aEditEvt );
                pEditView->MouseButtonUp( aEditEvt );
            }
        }

        if ( bIsTiledRendering && rMEvt.IsLeft() && pViewData->GetView()->GetSelEngine()->SelMouseButtonUp( rMEvt ) )
        {
            pViewData->GetView()->SelectionChanged();
        }

        if ( bDouble )
            return;
    }

            //      Links in edit cells

    bool bAlt = rMEvt.IsMod2();
    if ( !bAlt && !bRefMode && !bDouble && nMouseStatus == SC_GM_URLDOWN )
    {
        //  Only execute on ButtonUp, if ButtonDown also was done on a URL

        OUString aName, aUrl, aTarget;
        if ( GetEditUrl( rMEvt.GetPosPixel(), &aName, &aUrl, &aTarget ) )
        {
            nMouseStatus = SC_GM_NONE;              // Ignore double-click

            // ScGlobal::OpenURL() only understands Calc A1 style syntax.
            // Convert it to Calc A1 before calling OpenURL().
            if (pDoc->GetAddressConvention() == formula::FormulaGrammar::CONV_OOO)
                ScGlobal::OpenURL(aUrl, aTarget);
            else
            {
                ScAddress aTempAddr;
                ScAddress::ExternalInfo aExtInfo;
                ScRefFlags nRes = aTempAddr.Parse(aUrl, pDoc, pDoc->GetAddressConvention(), &aExtInfo);
                if (!(nRes & ScRefFlags::VALID))
                {
                    // Not a reference string. Pass it through unmodified.
                    ScGlobal::OpenURL(aUrl, aTarget);
                    return;
                }

                OUStringBuffer aBuf;
                if (aExtInfo.mbExternal)
                {
                    // External reference.
                    ScExternalRefManager* pRefMgr = pDoc->GetExternalRefManager();
                    const OUString* pStr = pRefMgr->getExternalFileName(aExtInfo.mnFileId);
                    if (pStr)
                        aBuf.append(*pStr);

                    aBuf.append('#');
                    aBuf.append(aExtInfo.maTabName);
                    aBuf.append('.');
                    OUString aRefCalcA1(aTempAddr.Format(ScRefFlags::ADDR_ABS, nullptr, formula::FormulaGrammar::CONV_OOO));
                    aBuf.append(aRefCalcA1);
                    ScGlobal::OpenURL(aBuf.makeStringAndClear(), aTarget);
                }
                else
                {
                    // Internal reference.
                    aBuf.append('#');
                    OUString aUrlCalcA1(aTempAddr.Format(ScRefFlags::ADDR_ABS_3D, pDoc, formula::FormulaGrammar::CONV_OOO));
                    aBuf.append(aUrlCalcA1);
                    ScGlobal::OpenURL(aBuf.makeStringAndClear(), aTarget);
                }
            }

            // fire worksheet_followhyperlink event
            uno::Reference< script::vba::XVBAEventProcessor > xVbaEvents = pDoc->GetVbaEventProcessor();
            if( xVbaEvents.is() ) try
            {
                Point aPos = rMEvt.GetPosPixel();
                SCCOL nPosX;
                SCROW nPosY;
                SCTAB nTab = pViewData->GetTabNo();
                pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nPosX, nPosY );
                OUString sURL;
                ScRefCellValue aCell;
                if (lcl_GetHyperlinkCell(pDoc, nPosX, nPosY, nTab, aCell, sURL))
                {
                    ScAddress aCellPos( nPosX, nPosY, nTab );
                    uno::Reference< table::XCell > xCell( new ScCellObj( pViewData->GetDocShell(), aCellPos ) );
                    uno::Sequence< uno::Any > aArgs(1);
                    aArgs[0] <<= xCell;
                    xVbaEvents->processVbaEvent( script::vba::VBAEventId::WORKSHEET_FOLLOWHYPERLINK, aArgs );
                }
            }
            catch( uno::Exception& )
            {
            }

            return;
        }
    }

            //      Gridwin - SelectionEngine

    //  SelMouseButtonDown is called only for left button, but SelMouseButtonUp would return
    //  sal_True for any call, so IsLeft must be checked here, too.

    if ( rMEvt.IsLeft() && pViewData->GetView()->GetSelEngine()->SelMouseButtonUp( rMEvt ) )
    {
        pViewData->GetView()->SelectionChanged();

        SfxDispatcher* pDisp = pViewData->GetViewShell()->GetDispatcher();
        bool bFormulaMode = pScMod->IsFormulaMode();
        OSL_ENSURE( pDisp || bFormulaMode, "Cursor moved on inactive View ?" );

        //  #i14927# execute SID_CURRENTCELL (for macro recording) only if there is no
        //  multiple selection, so the argument string completely describes the selection,
        //  and executing the slot won't change the existing selection (executing the slot
        //  here and from a recorded macro is treated equally)
        if ( pDisp && !bFormulaMode && !rMark.IsMultiMarked() )
        {
            OUString aAddr;                               // CurrentCell
            if( rMark.IsMarked() )
            {
                ScRange aScRange;
                rMark.GetMarkArea( aScRange );
                aAddr = aScRange.Format(*pDoc, ScRefFlags::RANGE_ABS);
                if ( aScRange.aStart == aScRange.aEnd )
                {
                    //  make sure there is a range selection string even for a single cell
                    aAddr += ":" + aAddr;
                }

                //! SID_MARKAREA does not exist anymore ???
                //! What happens when selecting with the cursor ???
            }
            else                                        // only move cursor
            {
                ScAddress aScAddress( pViewData->GetCurX(), pViewData->GetCurY(), 0 );
                aAddr = aScAddress.Format(ScRefFlags::ADDR_ABS);
            }

            SfxStringItem aPosItem( SID_CURRENTCELL, aAddr );
            // We don't want to align to the cursor position because if the
            // cell cursor isn't visible after making selection, it would jump
            // back to the origin of the selection where the cell cursor is.
            SfxBoolItem aAlignCursorItem( FN_PARAM_2, false );
            pDisp->ExecuteList(SID_CURRENTCELL,
                    SfxCallMode::SLOT | SfxCallMode::RECORD,
                    { &aPosItem, &aAlignCursorItem });

            pViewData->GetView()->InvalidateAttribs();

        }
        pViewData->GetViewShell()->SelectionChanged();

        return;
    }
}

void ScGridWindow::FakeButtonUp()
{
    if ( nButtonDown )
    {
        MouseEvent aEvent( aCurMousePos );      // nButtons = 0 -> ignore
        MouseButtonUp( aEvent );
    }
}

void ScGridWindow::MouseMove( const MouseEvent& rMEvt )
{
    aCurMousePos = rMEvt.GetPosPixel();

    if (rMEvt.IsLeaveWindow() && mpNoteMarker && !mpNoteMarker->IsByKeyboard())
        HideNoteMarker();

    ScModule* pScMod = SC_MOD();
    if (pScMod->IsModalMode(pViewData->GetSfxDocShell()))
        return;

    // If the Drag&Drop is started in the edit mode then sadly nothing else is kept
    if (bEEMouse && nButtonDown && !rMEvt.GetButtons())
    {
        bEEMouse = false;
        nButtonDown = 0;
        nMouseStatus = SC_GM_NONE;
        return;
    }

    if (nMouseStatus == SC_GM_IGNORE)
        return;

    if (nMouseStatus == SC_GM_WATERUNDO)    // Undo in format paintbrush mode -> only what for Up
        return;

    if ( pViewData->GetViewShell()->IsAuditShell() )        // Detective Fill Mode
    {
        SetPointer( PointerStyle::Fill );
        return;
    }

    if (nMouseStatus == SC_GM_FILTER && mpFilterBox)
    {
        Point aRelPos = mpFilterBox->ScreenToOutputPixel( OutputToScreenPixel( rMEvt.GetPosPixel() ) );
        if ( tools::Rectangle(Point(), mpFilterBox->GetOutputSizePixel()).IsInside(aRelPos) )
        {
            nButtonDown = 0;
            nMouseStatus = SC_GM_NONE;
            ReleaseMouse();
            mpFilterBox->MouseButtonDown( MouseEvent( aRelPos, 1, MouseEventModifiers::SIMPLECLICK, MOUSE_LEFT ) );
            return;
        }
    }

    bool bFormulaMode = pScMod->IsFormulaMode();            // next click -> reference

    if (bEEMouse && pViewData->HasEditView( eWhich ))
    {
        EditView*   pEditView;
        SCCOL       nEditCol;
        SCROW       nEditRow;
        pViewData->GetEditView( eWhich, pEditView, nEditCol, nEditRow );
        pEditView->MouseMove( rMEvt );
        return;
    }

    if (bDPMouse)
    {
        DPMouseMove( rMEvt );
        return;
    }

    if (bRFMouse)
    {
        RFMouseMove( rMEvt, false );
        return;
    }

    if (nPagebreakMouse)
    {
        PagebreakMove( rMEvt, false );
        return;
    }

    // Show other mouse pointer?

    bool bEditMode = pViewData->HasEditView(eWhich);

    //! Test if refMode dragging !!!
    if ( bEditMode && (pViewData->GetRefTabNo() == pViewData->GetTabNo()) )
    {
        Point   aPos = rMEvt.GetPosPixel();
        SCCOL  nPosX;
        SCROW  nPosY;
        pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nPosX, nPosY );

        EditView*   pEditView;
        SCCOL       nEditCol;
        SCROW       nEditRow;
        pViewData->GetEditView( eWhich, pEditView, nEditCol, nEditRow );
        SCCOL nEndCol = pViewData->GetEditEndCol();
        SCROW nEndRow = pViewData->GetEditEndRow();

        if ( nPosX >= nEditCol && nPosX <= nEndCol &&
             nPosY >= nEditRow && nPosY <= nEndRow )
        {
            if ( !pEditView )
            {
                SetPointer( PointerStyle::Text );
                return;
            }

            const SvxFieldItem* pFld;
            if ( comphelper::LibreOfficeKit::isActive() )
            {
                Point aLogicClick = pEditView->GetWindow()->PixelToLogic( aPos );
                pFld = pEditView->GetField( aLogicClick );
            }
            else
            {
                pFld = pEditView->GetFieldUnderMousePointer();
            }
            //  Field can only be URL field
            bool bAlt = rMEvt.IsMod2();
            if ( !bAlt && !nButtonDown && ScGlobal::ShouldOpenURL() && pFld )
                SetPointer( PointerStyle::RefHand );
            else if ( pEditView->GetEditEngine()->IsVertical() )
                SetPointer( PointerStyle::TextVertical );
            else
                SetPointer( PointerStyle::Text );
            return;
        }
    }

    bool bWater = SC_MOD()->GetIsWaterCan() || pViewData->GetView()->HasPaintBrush();
    if (bWater)
        SetPointer( PointerStyle::Fill );

    if (!bWater)
    {
        bool bCross = false;

        // range finder

        RfCorner rCorner = NONE;
        if ( HitRangeFinder( rMEvt.GetPosPixel(), rCorner, nullptr, nullptr, nullptr ) )
        {
            if (rCorner != NONE)
                SetPointer( PointerStyle::Cross );
            else
                SetPointer( PointerStyle::Hand );
            bCross = true;
        }

        //  Page-Break-Mode

        sal_uInt16 nBreakType;
        if ( !nButtonDown && pViewData->IsPagebreakMode() &&
                ( nBreakType = HitPageBreak( rMEvt.GetPosPixel(), nullptr, nullptr, nullptr ) ) != 0 )
        {
            PointerStyle eNew = PointerStyle::Arrow;
            switch ( nBreakType )
            {
                case SC_PD_RANGE_L:
                case SC_PD_RANGE_R:
                case SC_PD_BREAK_H:
                    eNew = PointerStyle::ESize;
                    break;
                case SC_PD_RANGE_T:
                case SC_PD_RANGE_B:
                case SC_PD_BREAK_V:
                    eNew = PointerStyle::SSize;
                    break;
                case SC_PD_RANGE_TL:
                case SC_PD_RANGE_BR:
                    eNew = PointerStyle::SESize;
                    break;
                case SC_PD_RANGE_TR:
                case SC_PD_RANGE_BL:
                    eNew = PointerStyle::NESize;
                    break;
            }
            SetPointer( eNew );
            bCross = true;
        }

        // Show fill cursor?

        if ( !bFormulaMode && !nButtonDown )
            if (TestMouse( rMEvt, false ))
                bCross = true;

        if ( nButtonDown && pViewData->IsAnyFillMode() )
        {
            SetPointer( PointerStyle::Cross );
            bCross = true;
            nScFillModeMouseModifier = rMEvt.GetModifier(); // evaluated for AutoFill and Matrix
        }

        if (!bCross)
        {
            bool bAlt = rMEvt.IsMod2();

            if (bEditMode)                                  // First has to be in edit mode!
                SetPointer( PointerStyle::Arrow );
            else if ( !bAlt && !nButtonDown && ScGlobal::ShouldOpenURL() &&
                        GetEditUrl(rMEvt.GetPosPixel()) )
                SetPointer( PointerStyle::RefHand );
            else if ( DrawMouseMove(rMEvt) )                // Reset pointer
                return;
        }
    }

    if ( pViewData->GetView()->GetSelEngine()->SelMouseMove( rMEvt ) )
        return;
}

static void lcl_InitMouseEvent(css::awt::MouseEvent& rEvent, const MouseEvent& rEvt)
{
    rEvent.Modifiers = 0;
    if ( rEvt.IsShift() )
        rEvent.Modifiers |= css::awt::KeyModifier::SHIFT;
    if ( rEvt.IsMod1() )
        rEvent.Modifiers |= css::awt::KeyModifier::MOD1;
    if ( rEvt.IsMod2() )
        rEvent.Modifiers |= css::awt::KeyModifier::MOD2;
    if ( rEvt.IsMod3() )
        rEvent.Modifiers |= css::awt::KeyModifier::MOD3;

    rEvent.Buttons = 0;
    if ( rEvt.IsLeft() )
        rEvent.Buttons |= css::awt::MouseButton::LEFT;
    if ( rEvt.IsRight() )
        rEvent.Buttons |= css::awt::MouseButton::RIGHT;
    if ( rEvt.IsMiddle() )
        rEvent.Buttons |= css::awt::MouseButton::MIDDLE;

    rEvent.X = rEvt.GetPosPixel().X();
    rEvent.Y = rEvt.GetPosPixel().Y();
    rEvent.ClickCount = rEvt.GetClicks();
    rEvent.PopupTrigger = false;
}

bool ScGridWindow::PreNotify( NotifyEvent& rNEvt )
{
    bool bDone = false;
    MouseNotifyEvent nType = rNEvt.GetType();
    if ( nType == MouseNotifyEvent::MOUSEBUTTONUP || nType == MouseNotifyEvent::MOUSEBUTTONDOWN )
    {
        vcl::Window* pWindow = rNEvt.GetWindow();
        if (pWindow == this && pViewData)
        {
            SfxViewFrame* pViewFrame = pViewData->GetViewShell()->GetViewFrame();
            if (pViewFrame)
            {
                css::uno::Reference<css::frame::XController> xController = pViewFrame->GetFrame().GetController();
                if (xController.is())
                {
                    ScTabViewObj* pImp = comphelper::getUnoTunnelImplementation<ScTabViewObj>( xController );
                    if (pImp && pImp->IsMouseListening())
                    {
                        css::awt::MouseEvent aEvent;
                        lcl_InitMouseEvent( aEvent, *rNEvt.GetMouseEvent() );
                        if ( rNEvt.GetWindow() )
                            aEvent.Source = rNEvt.GetWindow()->GetComponentInterface();
                        if ( nType == MouseNotifyEvent::MOUSEBUTTONDOWN)
                            bDone = pImp->MousePressed( aEvent );
                        else
                            bDone = pImp->MouseReleased( aEvent );
                    }
                }
            }
        }
    }
    if (bDone)      // event consumed by a listener
    {
        if ( nType == MouseNotifyEvent::MOUSEBUTTONDOWN )
        {
            const MouseEvent* pMouseEvent = rNEvt.GetMouseEvent();
            if ( pMouseEvent->IsRight() && pMouseEvent->GetClicks() == 1 )
            {
                // If a listener returned true for a right-click call, also prevent opening the context menu
                // (this works only if the context menu is opened on mouse-down)
                nMouseStatus = SC_GM_IGNORE;
            }
        }

        return true;
    }
    else
        return Window::PreNotify( rNEvt );
}

void ScGridWindow::Tracking( const TrackingEvent& rTEvt )
{
    // Since the SelectionEngine does not track, the events have to be
    // handed to the different MouseHandler...

    const MouseEvent& rMEvt = rTEvt.GetMouseEvent();

    if ( rTEvt.IsTrackingCanceled() )           // Cancel everything...
    {
        if (!pViewData->GetView()->IsInActivatePart() && !SC_MOD()->IsRefDialogOpen())
        {
            if (bDPMouse)
                bDPMouse = false;               // Paint for each bDragRect
            if (bDragRect)
            {
                bDragRect = false;
                UpdateDragRectOverlay();
            }
            if (bRFMouse)
            {
                RFMouseMove( rMEvt, true );     // Not possible to cancel properly...
                bRFMouse = false;
            }
            if (nPagebreakMouse)
            {
                bPagebreakDrawn = false;
                UpdateDragRectOverlay();
                nPagebreakMouse = SC_PD_NONE;
            }

            SetPointer( PointerStyle::Arrow );
            StopMarking();
            MouseButtonUp( rMEvt );     // With status SC_GM_IGNORE from StopMarking

            bool bRefMode = pViewData->IsRefMode();
            if (bRefMode)
                SC_MOD()->EndReference();       // Do not let the Dialog remain minimized
        }
    }
    else if ( rTEvt.IsTrackingEnded() )
    {
        if ( !comphelper::LibreOfficeKit::isActive() )
        {
            // MouseButtonUp always with matching buttons (eg for test tool, # 63148 #)
            // The tracking event will indicate if it was completed and not canceled.
            MouseEvent aUpEvt( rMEvt.GetPosPixel(), rMEvt.GetClicks(),
                                rMEvt.GetMode(), nButtonDown, rMEvt.GetModifier() );
            MouseButtonUp( aUpEvt );
        }
    }
    else
        MouseMove( rMEvt );
}

void ScGridWindow::StartDrag( sal_Int8 /* nAction */, const Point& rPosPixel )
{
    if (mpFilterBox || nPagebreakMouse)
        return;

    HideNoteMarker();

    CommandEvent aDragEvent( rPosPixel, CommandEventId::StartDrag, true );

    if (bEEMouse && pViewData->HasEditView( eWhich ))
    {
        EditView*   pEditView;
        SCCOL       nEditCol;
        SCROW       nEditRow;
        pViewData->GetEditView( eWhich, pEditView, nEditCol, nEditRow );

        // don't remove the edit view while switching views
        ScModule* pScMod = SC_MOD();
        pScMod->SetInEditCommand( true );

        pEditView->Command( aDragEvent );

        ScInputHandler* pHdl = pScMod->GetInputHdl();
        if (pHdl)
            pHdl->DataChanged();

        pScMod->SetInEditCommand( false );
        if (!pViewData->IsActive())             // dropped to different view?
        {
            ScInputHandler* pViewHdl = pScMod->GetInputHdl( pViewData->GetViewShell() );
            if ( pViewHdl && pViewData->HasEditView( eWhich ) )
            {
                pViewHdl->CancelHandler();
                ShowCursor();   // missing from KillEditView
            }
        }
    }
    else
        if ( !DrawCommand(aDragEvent) )
            pViewData->GetView()->GetSelEngine()->Command( aDragEvent );
}

static void lcl_SetTextCursorPos( ScViewData* pViewData, ScSplitPos eWhich, vcl::Window* pWin )
{
    SCCOL nCol = pViewData->GetCurX();
    SCROW nRow = pViewData->GetCurY();
    tools::Rectangle aEditArea = pViewData->GetEditArea( eWhich, nCol, nRow, pWin, nullptr, true );
    aEditArea.SetRight( aEditArea.Left() );
    aEditArea = pWin->PixelToLogic( aEditArea );
    pWin->SetCursorRect( &aEditArea );
}

void ScGridWindow::Command( const CommandEvent& rCEvt )
{
    // The command event is send to the window after a possible context
    // menu from an inplace client is closed. Now we have the chance to
    // deactivate the inplace client without any problem regarding parent
    // windows and code on the stack.
    CommandEventId nCmd = rCEvt.GetCommand();
    ScTabViewShell* pTabViewSh = pViewData->GetViewShell();
    SfxInPlaceClient* pClient = pTabViewSh->GetIPClient();
    if ( pClient &&
         pClient->IsObjectInPlaceActive() &&
         nCmd == CommandEventId::ContextMenu )
    {
        pTabViewSh->DeactivateOle();
        return;
    }

    ScModule* pScMod = SC_MOD();
    OSL_ENSURE( nCmd != CommandEventId::StartDrag, "ScGridWindow::Command called with CommandEventId::StartDrag" );

    if (nCmd == CommandEventId::ModKeyChange)
    {
        Window::Command(rCEvt);
        return;
    }

    if ( nCmd == CommandEventId::StartExtTextInput ||
         nCmd == CommandEventId::EndExtTextInput ||
         nCmd == CommandEventId::ExtTextInput ||
         nCmd == CommandEventId::CursorPos ||
         nCmd == CommandEventId::QueryCharPosition )
    {
        bool bEditView = pViewData->HasEditView( eWhich );
        if (!bEditView)
        {
            //  only if no cell editview is active, look at drawview
            SdrView* pSdrView = pViewData->GetView()->GetSdrView();
            if ( pSdrView )
            {
                OutlinerView* pOlView = pSdrView->GetTextEditOutlinerView();
                if ( pOlView && pOlView->GetWindow() == this )
                {
                    pOlView->Command( rCEvt );
                    return;                             // done
                }
            }
        }

        if ( nCmd == CommandEventId::CursorPos && !bEditView )
        {
            //  CURSORPOS may be called without following text input,
            //  to set the input method window position
            //  -> input mode must not be started,
            //  manually calculate text insert position if not in input mode

            lcl_SetTextCursorPos( pViewData, eWhich, this );
            return;
        }

        ScInputHandler* pHdl = pScMod->GetInputHdl( pViewData->GetViewShell() );
        if ( pHdl )
        {
            pHdl->InputCommand( rCEvt );
            return;                                     // done
        }

        Window::Command( rCEvt );
        return;
    }

    if ( nCmd == CommandEventId::PasteSelection )
    {
        if ( bEEMouse )
        {
            //  EditEngine handles selection in MouseButtonUp - no action
            //  needed in command handler
        }
        else
        {
            PasteSelection( rCEvt.GetMousePosPixel() );
        }
        return;
    }

    if ( nCmd == CommandEventId::InputLanguageChange )
    {
        // #i55929# Font and font size state depends on input language if nothing is selected,
        // so the slots have to be invalidated when the input language is changed.

        SfxBindings& rBindings = pViewData->GetBindings();
        rBindings.Invalidate( SID_ATTR_CHAR_FONT );
        rBindings.Invalidate( SID_ATTR_CHAR_FONTHEIGHT );
        return;
    }

    if ( nCmd == CommandEventId::Wheel || nCmd == CommandEventId::StartAutoScroll || nCmd == CommandEventId::AutoScroll )
    {
        bool bDone = pViewData->GetView()->ScrollCommand( rCEvt, eWhich );
        if (!bDone)
            Window::Command(rCEvt);
        return;
    }
    // #i7560# FormulaMode check is below scrolling - scrolling is allowed during formula input
    bool bDisable = pScMod->IsFormulaMode() ||
                    pScMod->IsModalMode(pViewData->GetSfxDocShell());
    if (bDisable)
        return;

    if ( nCmd == CommandEventId::ContextMenu && !SC_MOD()->GetIsWaterCan() )
    {
        bool bMouse = rCEvt.IsMouseEvent();
        if ( bMouse && nMouseStatus == SC_GM_IGNORE )
            return;

        if (pViewData->IsAnyFillMode())
        {
            pViewData->GetView()->StopRefMode();
            pViewData->ResetFillMode();
        }
        ReleaseMouse();
        StopMarking();

        Point aPosPixel = rCEvt.GetMousePosPixel();
        Point aMenuPos = aPosPixel;

        SCCOL nCellX = -1;
        SCROW nCellY = -1;
        pViewData->GetPosFromPixel(aPosPixel.X(), aPosPixel.Y(), eWhich, nCellX, nCellY);

        bool bSpellError = false;
        SCCOL nColSpellError = nCellX;

        if ( bMouse )
        {
            ScDocument* pDoc = pViewData->GetDocument();
            SCTAB nTab = pViewData->GetTabNo();
            const ScTableProtection* pProtect = pDoc->GetTabProtection(nTab);
            bool bSelectAllowed = true;
            if ( pProtect && pProtect->isProtected() )
            {
                // This sheet is protected.  Check if a context menu is allowed on this cell.
                bool bCellProtected = pDoc->HasAttrib(nCellX, nCellY, nTab, nCellX, nCellY, nTab, HasAttrFlags::Protected);
                bool bSelProtected   = pProtect->isOptionEnabled(ScTableProtection::SELECT_LOCKED_CELLS);
                bool bSelUnprotected = pProtect->isOptionEnabled(ScTableProtection::SELECT_UNLOCKED_CELLS);

                if (bCellProtected)
                    bSelectAllowed = bSelProtected;
                else
                    bSelectAllowed = bSelUnprotected;
            }
            if (!bSelectAllowed)
                // Selecting this cell is not allowed, neither is context menu.
                return;

            if (mpSpellCheckCxt)
            {
                // Find the first string to the left for spell checking in case the current cell is empty.
                ScAddress aPos(nCellX, nCellY, nTab);
                ScRefCellValue aSpellCheckCell(*pDoc, aPos);
                while (aSpellCheckCell.meType == CELLTYPE_NONE)
                {
                    // Loop until we get the first non-empty cell in the row.
                    aPos.IncCol(-1);
                    if (aPos.Col() < 0)
                        break;

                    aSpellCheckCell.assign(*pDoc, aPos);
                }

                if (aPos.Col() >= 0 && (aSpellCheckCell.meType == CELLTYPE_STRING || aSpellCheckCell.meType == CELLTYPE_EDIT))
                    nColSpellError = aPos.Col();

                bSpellError = (mpSpellCheckCxt->isMisspelled(nColSpellError, nCellY));
                if (bSpellError)
                {
                    // Check and see if a misspelled word is under the mouse pointer.
                    bSpellError = IsSpellErrorAtPos(aPosPixel, nColSpellError, nCellY);
                }
            }

            //  #i18735# First select the item under the mouse pointer.
            //  This can change the selection, and the view state (edit mode, etc).
            SelectForContextMenu(aPosPixel, bSpellError ? nColSpellError : nCellX, nCellY);
        }

        bool bDone = false;
        bool bEdit = pViewData->HasEditView(eWhich);

        if ( !bEdit )
        {
                // Edit cell with spelling errors ?
            if (bMouse && (GetEditUrl(aPosPixel) || bSpellError))
            {
                //  GetEditUrlOrError has already moved the Cursor

                pScMod->SetInputMode( SC_INPUT_TABLE );
                bEdit = pViewData->HasEditView(eWhich);     // Did it work?

                OSL_ENSURE( bEdit, "Can not be switched in edit mode" );
            }
        }
        if ( bEdit )
        {
            EditView* pEditView = pViewData->GetEditView( eWhich );     // is then not 0

            if ( !bMouse )
            {
                vcl::Cursor* pCur = pEditView->GetCursor();
                if ( pCur )
                {
                    Point aLogicPos = pCur->GetPos();
                    //  use the position right of the cursor (spell popup is opened if
                    //  the cursor is before the word, but not if behind it)
                    aLogicPos.AdjustX(pCur->GetWidth() );
                    aLogicPos.AdjustY(pCur->GetHeight() / 2 );     // center vertically
                    aMenuPos = LogicToPixel( aLogicPos );
                }
            }

            //  if edit mode was just started above, online spelling may be incomplete
            pEditView->GetEditEngine()->CompleteOnlineSpelling();

            //  IsCursorAtWrongSpelledWord could be used for !bMouse
            //  if there was a corresponding ExecuteSpellPopup call

            if (bSpellError)
            {
                // On OS/2 when clicking next to the Popup menu, the MouseButtonDown
                // comes before the end of menu execute, thus the SetModified has to
                // be done prior to this (Bug #40968#)
                ScInputHandler* pHdl = pScMod->GetInputHdl();
                if (pHdl)
                    pHdl->SetModified();

                Link<SpellCallbackInfo&,void> aLink = LINK( this, ScGridWindow, PopupSpellingHdl );
                pEditView->ExecuteSpellPopup( aMenuPos, &aLink );

                bDone = true;
            }
        }
        else if ( !bMouse )
        {
            //  non-edit menu by keyboard -> use lower right of cell cursor position
            ScDocument* aDoc = pViewData->GetDocument();
            SCTAB nTabNo = pViewData->GetTabNo();
            bool bLayoutIsRTL = aDoc->IsLayoutRTL(nTabNo);

            SCCOL nCurX = pViewData->GetCurX();
            SCROW nCurY = pViewData->GetCurY();
            aMenuPos = pViewData->GetScrPos( nCurX, nCurY, eWhich, true );
            long nSizeXPix;
            long nSizeYPix;
            pViewData->GetMergeSizePixel( nCurX, nCurY, nSizeXPix, nSizeYPix );
            // fdo#55432 take the correct position for RTL sheet
            aMenuPos.AdjustX(bLayoutIsRTL ? -nSizeXPix : nSizeXPix );
            aMenuPos.AdjustY(nSizeYPix );

            ScTabViewShell* pViewSh = pViewData->GetViewShell();
            if (pViewSh)
            {
                //  Is a draw object selected?

                SdrView* pDrawView = pViewSh->GetSdrView();
                if (pDrawView && pDrawView->AreObjectsMarked())
                {
                    // #100442#; the context menu should open in the middle of the selected objects
                    tools::Rectangle aSelectRect(LogicToPixel(pDrawView->GetAllMarkedBoundRect()));
                    aMenuPos = aSelectRect.Center();
                }
            }
        }

        if (!bDone)
        {
            SfxDispatcher::ExecutePopup( this, &aMenuPos );
        }
    }
}

void ScGridWindow::SelectForContextMenu( const Point& rPosPixel, SCCOL nCellX, SCROW nCellY )
{
    //  #i18735# if the click was outside of the current selection,
    //  the cursor is moved or an object at the click position selected.
    //  (see SwEditWin::SelectMenuPosition in Writer)

    ScTabView* pView = pViewData->GetView();
    ScDrawView* pDrawView = pView->GetScDrawView();

    //  check cell edit mode

    if ( pViewData->HasEditView(eWhich) )
    {
        ScModule* pScMod = SC_MOD();
        SCCOL nEditStartCol = pViewData->GetEditViewCol(); //! change to GetEditStartCol after calcrtl is integrated
        SCROW nEditStartRow = pViewData->GetEditViewRow();
        SCCOL nEditEndCol = pViewData->GetEditEndCol();
        SCROW nEditEndRow = pViewData->GetEditEndRow();

        if ( nCellX >= nEditStartCol && nCellX <= nEditEndCol &&
             nCellY >= nEditStartRow && nCellY <= nEditEndRow )
        {
            //  handle selection within the EditView

            EditView* pEditView = pViewData->GetEditView( eWhich );     // not NULL (HasEditView)
            EditEngine* pEditEngine = pEditView->GetEditEngine();
            tools::Rectangle aOutputArea = pEditView->GetOutputArea();
            tools::Rectangle aVisArea = pEditView->GetVisArea();

            Point aTextPos = PixelToLogic( rPosPixel );
            if ( pEditEngine->IsVertical() )            // have to manually transform position
            {
                aTextPos -= aOutputArea.TopRight();
                long nTemp = -aTextPos.X();
                aTextPos.setX( aTextPos.Y() );
                aTextPos.setY( nTemp );
            }
            else
                aTextPos -= aOutputArea.TopLeft();
            aTextPos += aVisArea.TopLeft();             // position in the edit document

            EPosition aDocPosition = pEditEngine->FindDocPosition(aTextPos);
            ESelection aCompare(aDocPosition.nPara, aDocPosition.nIndex);
            ESelection aSelection = pEditView->GetSelection();
            aSelection.Adjust();    // needed for IsLess/IsGreater
            if ( aCompare < aSelection || aCompare > aSelection )
            {
                // clicked outside the selected text - deselect and move text cursor
                MouseEvent aEvent( rPosPixel );
                pEditView->MouseButtonDown( aEvent );
                pEditView->MouseButtonUp( aEvent );
                pScMod->InputSelection( pEditView );
            }

            return;     // clicked within the edit view - keep edit mode
        }
        else
        {
            // outside of the edit view - end edit mode, regardless of cell selection, then continue
            pScMod->InputEnterHandler();
        }
    }

    //  check draw text edit mode

    Point aLogicPos = PixelToLogic( rPosPixel );        // after cell edit mode is ended
    if ( pDrawView && pDrawView->GetTextEditObject() && pDrawView->GetTextEditOutlinerView() )
    {
        OutlinerView* pOlView = pDrawView->GetTextEditOutlinerView();
        tools::Rectangle aOutputArea = pOlView->GetOutputArea();
        if ( aOutputArea.IsInside( aLogicPos ) )
        {
            //  handle selection within the OutlinerView

            Outliner* pOutliner = pOlView->GetOutliner();
            const EditEngine& rEditEngine = pOutliner->GetEditEngine();
            tools::Rectangle aVisArea = pOlView->GetVisArea();

            Point aTextPos = aLogicPos;
            if ( pOutliner->IsVertical() )              // have to manually transform position
            {
                aTextPos -= aOutputArea.TopRight();
                long nTemp = -aTextPos.X();
                aTextPos.setX( aTextPos.Y() );
                aTextPos.setY( nTemp );
            }
            else
                aTextPos -= aOutputArea.TopLeft();
            aTextPos += aVisArea.TopLeft();             // position in the edit document

            EPosition aDocPosition = rEditEngine.FindDocPosition(aTextPos);
            ESelection aCompare(aDocPosition.nPara, aDocPosition.nIndex);
            ESelection aSelection = pOlView->GetSelection();
            aSelection.Adjust();    // needed for IsLess/IsGreater
            if ( aCompare < aSelection || aCompare > aSelection )
            {
                // clicked outside the selected text - deselect and move text cursor
                // use DrawView to allow extra handling there (none currently)
                MouseEvent aEvent( rPosPixel );
                pDrawView->MouseButtonDown( aEvent, this );
                pDrawView->MouseButtonUp( aEvent, this );
            }

            return;     // clicked within the edit area - keep edit mode
        }
        else
        {
            // Outside of the edit area - end text edit mode, then continue.
            // DrawDeselectAll also ends text edit mode and updates the shells.
            // If the click was on the edited object, it will be selected again below.
            pView->DrawDeselectAll();
        }
    }

    //  look for existing selection

    bool bHitSelected = false;
    if ( pDrawView && pDrawView->IsMarkedObjHit( aLogicPos ) )
    {
        //  clicked on selected object -> don't change anything
        bHitSelected = true;
    }
    else if ( pViewData->GetMarkData().IsCellMarked(nCellX, nCellY) )
    {
        //  clicked on selected cell -> don't change anything
        bHitSelected = true;
    }

    //  select drawing object or move cell cursor

    if ( !bHitSelected )
    {
        bool bWasDraw = ( pDrawView && pDrawView->AreObjectsMarked() );
        bool bHitDraw = false;
        if ( pDrawView )
        {
            pDrawView->UnmarkAllObj();
            // Unlock the Internal Layer in order to activate the context menu.
            // re-lock in ScDrawView::MarkListHasChanged()
            lcl_UnLockComment( pDrawView, aLogicPos ,pViewData);
            bHitDraw = pDrawView->MarkObj( aLogicPos );
            // draw shell is activated in MarkListHasChanged
        }
        if ( !bHitDraw )
        {
            pView->Unmark();
            pView->SetCursor(nCellX, nCellY);
            if ( bWasDraw )
                pViewData->GetViewShell()->SetDrawShell( false );   // switch shells
        }
    }
}

void ScGridWindow::KeyInput(const KeyEvent& rKEvt)
{
    // Cursor control for ref input dialog
    const vcl::KeyCode& rKeyCode = rKEvt.GetKeyCode();

#ifdef DBG_UTIL

    if (rKeyCode.IsMod1() && rKeyCode.IsShift())
    {
        if (rKeyCode.GetCode() == KEY_F12)
        {
            dumpColumnInformationPixel();
        }
        else if (rKeyCode.GetCode() == KEY_F11)
        {
            dumpGraphicInformation();
        }
        else if (rKeyCode.GetCode() == KEY_F10)
        {
            dumpColumnInformationHmm();
        }
        else if (rKeyCode.GetCode() == KEY_F6)
        {
            dumpCellProperties();
        }
        else if (rKeyCode.GetCode() == KEY_F8)
        {
            dumpColumnCellStorage();
        }
        else if (rKeyCode.GetCode() == KEY_F7)
        {
            ScDocument* pDoc   = pViewData->GetDocument();
            auto& rMapper = pDoc->GetExternalDataMapper();
            for (auto& itr : rMapper.getDataSources())
            {
                itr.refresh(pDoc);
            }
            return;
        }
    }

#endif

    if( SC_MOD()->IsRefDialogOpen() )
    {
        if( !rKeyCode.GetModifier() && (rKeyCode.GetCode() == KEY_F2) )
        {
            SC_MOD()->EndReference();
        }
        else if( pViewData->GetViewShell()->MoveCursorKeyInput( rKEvt ) )
        {
            ScRange aRef(
                pViewData->GetRefStartX(), pViewData->GetRefStartY(), pViewData->GetRefStartZ(),
                pViewData->GetRefEndX(), pViewData->GetRefEndY(), pViewData->GetRefEndZ() );
            SC_MOD()->SetReference( aRef, *pViewData->GetDocument() );
        }
        pViewData->GetViewShell()->SelectionChanged();
        return ;
    }
    else if( rKeyCode.GetCode() == KEY_RETURN && pViewData->IsPasteMode() )
    {
        ScTabViewShell* pTabViewShell = pViewData->GetViewShell();
        ScClipUtil::PasteFromClipboard( pViewData, pTabViewShell, true );

        // Clear clipboard content.
        uno::Reference<datatransfer::clipboard::XClipboard> xSystemClipboard =
            GetClipboard();
        if (xSystemClipboard.is())
        {
            xSystemClipboard->setContents(
                    uno::Reference<datatransfer::XTransferable>(),
                    uno::Reference<datatransfer::clipboard::XClipboardOwner>());
        }

        // hide the border around the copy source
        pViewData->SetPasteMode( ScPasteFlags::NONE );
        // Clear CopySourceOverlay in each window of a split/frozen tabview
        pViewData->GetView()->UpdateCopySourceOverlay();
        return;
    }
    // if semi-modeless SfxChildWindow dialog above, then no KeyInputs:
    else if( !pViewData->IsAnyFillMode() )
    {
        if (rKeyCode.GetCode() == KEY_ESCAPE)
        {
            pViewData->SetPasteMode( ScPasteFlags::NONE );
            // Clear CopySourceOverlay in each window of a split/frozen tabview
            pViewData->GetView()->UpdateCopySourceOverlay();
        }
        //  query for existing note marker before calling ViewShell's keyboard handling
        //  which may remove the marker
        bool bHadKeyMarker = mpNoteMarker && mpNoteMarker->IsByKeyboard();
        ScTabViewShell* pViewSh = pViewData->GetViewShell();

        if (pViewData->GetDocShell()->GetProgress())
            return;

        if (DrawKeyInput(rKEvt))
        {
            const vcl::KeyCode& rLclKeyCode = rKEvt.GetKeyCode();
            if (rLclKeyCode.GetCode() == KEY_DOWN
                || rLclKeyCode.GetCode() == KEY_UP
                || rLclKeyCode.GetCode() == KEY_LEFT
                || rLclKeyCode.GetCode() == KEY_RIGHT)
            {
                ScTabViewShell* pViewShell = pViewData->GetViewShell();
                SfxBindings& rBindings = pViewShell->GetViewFrame()->GetBindings();
                rBindings.Invalidate(SID_ATTR_TRANSFORM_POS_X);
                rBindings.Invalidate(SID_ATTR_TRANSFORM_POS_Y);
             }
            return;
        }

        if (!pViewData->GetView()->IsDrawSelMode() && !DrawHasMarkedObj())  // No entries in draw mode
        {                                                           //! check DrawShell !!!
            if (pViewSh->TabKeyInput(rKEvt))
                return;
        }
        else
            if (pViewSh->SfxViewShell::KeyInput(rKEvt))             // from SfxViewShell
                return;

        vcl::KeyCode aCode = rKEvt.GetKeyCode();
        if ( aCode.GetCode() == KEY_ESCAPE && aCode.GetModifier() == 0 )
        {
            if ( bHadKeyMarker )
                HideNoteMarker();
            else
                pViewSh->Escape();
            return;
        }
        if ( aCode.GetCode() == KEY_F1 && aCode.GetModifier() == KEY_MOD1 )
        {
            //  ctrl-F1 shows or hides the note or redlining info for the cursor position
            //  (hard-coded because F1 can't be configured)

            if ( bHadKeyMarker )
                HideNoteMarker();       // hide when previously visible
            else
                ShowNoteMarker( pViewData->GetCurX(), pViewData->GetCurY(), true );
            return;
        }
        if (aCode.GetCode() == KEY_BRACKETLEFT && aCode.GetModifier() == KEY_MOD1)
        {
            pViewSh->DetectiveMarkPred();
            return;
        }
        if (aCode.GetCode() == KEY_BRACKETRIGHT && aCode.GetModifier() == KEY_MOD1)
        {
            pViewSh->DetectiveMarkSucc();
            return;
        }

    }

    Window::KeyInput(rKEvt);
}

void ScGridWindow::StopMarking()
{
    DrawEndAction();                // Cancel Select/move on Drawing-Layer

    if (nButtonDown)
    {
        pViewData->GetMarkData().SetMarking(false);
        nMouseStatus = SC_GM_IGNORE;
    }
}

void ScGridWindow::UpdateInputContext()
{
    bool bReadOnly = pViewData->GetDocShell()->IsReadOnly();
    InputContextFlags nOptions = bReadOnly ? InputContextFlags::NONE : ( InputContextFlags::Text | InputContextFlags::ExtText );

    //  when font from InputContext is used,
    //  it must be taken from the cursor position's cell attributes

    InputContext aContext;
    aContext.SetOptions( nOptions );
    SetInputContext( aContext );
}

                                // sensitive range (Pixel)
#define SCROLL_SENSITIVE 20

void ScGridWindow::DropScroll( const Point& rMousePos )
{
    ScDocument* pDoc = pViewData->GetDocument();
    SCCOL nDx = 0;
    SCROW nDy = 0;
    Size aSize = GetOutputSizePixel();

    if (aSize.Width() > SCROLL_SENSITIVE * 3)
    {
        if ( rMousePos.X() < SCROLL_SENSITIVE && pViewData->GetPosX(WhichH(eWhich)) > 0 )
            nDx = -1;
        if ( rMousePos.X() >= aSize.Width() - SCROLL_SENSITIVE
                && pViewData->GetPosX(WhichH(eWhich)) < pDoc->MaxCol() )
            nDx = 1;
    }
    if (aSize.Height() > SCROLL_SENSITIVE * 3)
    {
        if ( rMousePos.Y() < SCROLL_SENSITIVE && pViewData->GetPosY(WhichV(eWhich)) > 0 )
            nDy = -1;
        if ( rMousePos.Y() >= aSize.Height() - SCROLL_SENSITIVE
                && pViewData->GetPosY(WhichV(eWhich)) < pDoc->MaxRow() )
            nDy = 1;
    }

    if ( nDx != 0 || nDy != 0 )
    {
        if ( nDx != 0 )
            pViewData->GetView()->ScrollX( nDx, WhichH(eWhich) );
        if ( nDy != 0 )
            pViewData->GetView()->ScrollY( nDy, WhichV(eWhich) );
    }
}

static bool lcl_TestScenarioRedliningDrop( const ScDocument* pDoc, const ScRange& aDragRange)
{
    //  Test, if a scenario is affected by a drop when turing on RedLining,
    bool bReturn = false;
    SCTAB nTab = aDragRange.aStart.Tab();
    SCTAB nTabCount = pDoc->GetTableCount();

    if(pDoc->GetChangeTrack()!=nullptr)
    {
        if( pDoc->IsScenario(nTab) && pDoc->HasScenarioRange(nTab, aDragRange))
        {
            bReturn = true;
        }
        else
        {
            for(SCTAB i=nTab+1; i<nTabCount && pDoc->IsScenario(i); i++)
            {
                if(pDoc->HasScenarioRange(i, aDragRange))
                {
                    bReturn = true;
                    break;
                }
            }
        }
    }
    return bReturn;
}

static ScRange lcl_MakeDropRange( const ScDocument* pDoc, SCCOL nPosX, SCROW nPosY, SCTAB nTab, const ScRange& rSource )
{
    SCCOL nCol1 = nPosX;
    SCCOL nCol2 = nCol1 + ( rSource.aEnd.Col() - rSource.aStart.Col() );
    if ( nCol2 > pDoc->MaxCol() )
    {
        nCol1 -= nCol2 - pDoc->MaxCol();
        nCol2 = pDoc->MaxCol();
    }
    SCROW nRow1 = nPosY;
    SCROW nRow2 = nRow1 + ( rSource.aEnd.Row() - rSource.aStart.Row() );
    if ( nRow2 > pDoc->MaxRow() )
    {
        nRow1 -= nRow2 - pDoc->MaxRow();
        nRow2 = pDoc->MaxRow();
    }

    return ScRange( nCol1, nRow1, nTab, nCol2, nRow2, nTab );
}

sal_Int8 ScGridWindow::AcceptPrivateDrop( const AcceptDropEvent& rEvt )
{
    if ( rEvt.mbLeaving )
    {
        bDragRect = false;
        UpdateDragRectOverlay();
        return rEvt.mnAction;
    }

    const ScDragData& rData = SC_MOD()->GetDragData();
    if ( rData.pCellTransfer )
    {
        // Don't move source that would include filtered rows.
        if ((rEvt.mnAction & DND_ACTION_MOVE) && rData.pCellTransfer->HasFilteredRows())
        {
            if (bDragRect)
            {
                bDragRect = false;
                UpdateDragRectOverlay();
            }
            return DND_ACTION_NONE;
        }

        Point aPos = rEvt.maPosPixel;

        ScDocument* pSourceDoc = rData.pCellTransfer->GetSourceDocument();
        ScDocument* pThisDoc   = pViewData->GetDocument();
        if (pSourceDoc == pThisDoc)
        {
            OUString aName;
            if ( pThisDoc->HasChartAtPoint(pViewData->GetTabNo(), PixelToLogic(aPos), aName ))
            {
                if (bDragRect)          // Remove rectangle
                {
                    bDragRect = false;
                    UpdateDragRectOverlay();
                }

                //! highlight chart? (selection border?)

                sal_Int8 nRet = rEvt.mnAction;
                return nRet;
            }
        }

        if (rData.pCellTransfer->GetDragSourceFlags() & ScDragSrc::Table) // whole sheet?
        {
            bool bOk = pThisDoc->IsDocEditable();
            return bOk ? rEvt.mnAction : 0;                     // don't draw selection frame
        }

        SCCOL  nPosX;
        SCROW  nPosY;
        pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nPosX, nPosY );

        ScRange aSourceRange = rData.pCellTransfer->GetRange();
        SCCOL nSourceStartX = aSourceRange.aStart.Col();
        SCROW nSourceStartY = aSourceRange.aStart.Row();
        SCCOL nSourceEndX = aSourceRange.aEnd.Col();
        SCROW nSourceEndY = aSourceRange.aEnd.Row();
        SCCOL nSizeX = nSourceEndX - nSourceStartX + 1;
        SCROW nSizeY = nSourceEndY - nSourceStartY + 1;

        if ( rEvt.mnAction != DND_ACTION_MOVE )
            nSizeY = rData.pCellTransfer->GetNonFilteredRows();     // copy/link: no filtered rows

        SCCOL nNewDragX = nPosX - rData.pCellTransfer->GetDragHandleX();
        if (nNewDragX<0) nNewDragX=0;
        if (nNewDragX+(nSizeX-1) > pThisDoc->MaxCol())
            nNewDragX = pThisDoc->MaxCol()-(nSizeX-1);
        SCROW nNewDragY = nPosY - rData.pCellTransfer->GetDragHandleY();
        if (nNewDragY<0) nNewDragY=0;
        if (nNewDragY+(nSizeY-1) > pThisDoc->MaxRow())
            nNewDragY = pThisDoc->MaxRow()-(nSizeY-1);

        //  don't break scenario ranges, don't drop on filtered
        SCTAB nTab = pViewData->GetTabNo();
        ScRange aDropRange = lcl_MakeDropRange( pThisDoc, nNewDragX, nNewDragY, nTab, aSourceRange );
        if ( lcl_TestScenarioRedliningDrop( pThisDoc, aDropRange ) ||
             lcl_TestScenarioRedliningDrop( pSourceDoc, aSourceRange ) ||
             ScViewUtil::HasFiltered( aDropRange, pThisDoc) )
        {
            if (bDragRect)
            {
                bDragRect = false;
                UpdateDragRectOverlay();
            }
            return DND_ACTION_NONE;
        }

        InsCellCmd eDragInsertMode = INS_NONE;
        Window::PointerState aState = GetPointerState();

        // check for datapilot item sorting
        ScDPObject* pDPObj = nullptr;
        if ( pThisDoc == pSourceDoc && ( pDPObj = pThisDoc->GetDPAtCursor( nNewDragX, nNewDragY, nTab ) ) != nullptr )
        {
            // drop on DataPilot table: sort or nothing

            bool bDPSort = false;
            if ( pThisDoc->GetDPAtCursor( nSourceStartX, nSourceStartY, aSourceRange.aStart.Tab() ) == pDPObj )
            {
                sheet::DataPilotTableHeaderData aDestData;
                pDPObj->GetHeaderPositionData( ScAddress(nNewDragX, nNewDragY, nTab), aDestData );
                bool bValid = ( aDestData.Dimension >= 0 );        // dropping onto a field

                // look through the source range
                for (SCROW nRow = aSourceRange.aStart.Row(); bValid && nRow <= aSourceRange.aEnd.Row(); ++nRow )
                    for (SCCOL nCol = aSourceRange.aStart.Col(); bValid && nCol <= aSourceRange.aEnd.Col(); ++nCol )
                    {
                        sheet::DataPilotTableHeaderData aSourceData;
                        pDPObj->GetHeaderPositionData( ScAddress( nCol, nRow, aSourceRange.aStart.Tab() ), aSourceData );
                        if ( aSourceData.Dimension != aDestData.Dimension || aSourceData.MemberName.isEmpty() )
                            bValid = false;     // empty (subtotal) or different field
                    }

                if ( bValid )
                {
                    bool bIsDataLayout;
                    OUString aDimName = pDPObj->GetDimName( aDestData.Dimension, bIsDataLayout );
                    const ScDPSaveDimension* pDim = pDPObj->GetSaveData()->GetExistingDimensionByName( aDimName );
                    if ( pDim )
                    {
                        ScRange aOutRange = pDPObj->GetOutRange();

                        sheet::DataPilotFieldOrientation nOrient = pDim->GetOrientation();
                        if ( nOrient == sheet::DataPilotFieldOrientation_COLUMN )
                        {
                            eDragInsertMode = INS_CELLSRIGHT;
                            nSizeY = aOutRange.aEnd.Row() - nNewDragY + 1;
                            bDPSort = true;
                        }
                        else if ( nOrient == sheet::DataPilotFieldOrientation_ROW )
                        {
                            eDragInsertMode = INS_CELLSDOWN;
                            nSizeX = aOutRange.aEnd.Col() - nNewDragX + 1;
                            bDPSort = true;
                        }
                    }
                }
            }

            if ( !bDPSort )
            {
                // no valid sorting in a DataPilot table -> disallow
                if ( bDragRect )
                {
                    bDragRect = false;
                    UpdateDragRectOverlay();
                }
                return DND_ACTION_NONE;
            }
        }
        else if ( aState.mnState & KEY_MOD2 )
        {
            if ( pThisDoc == pSourceDoc && nTab == aSourceRange.aStart.Tab() )
            {
                long nDeltaX = labs( static_cast< long >( nNewDragX - nSourceStartX ) );
                long nDeltaY = labs( static_cast< long >( nNewDragY - nSourceStartY ) );
                if ( nDeltaX <= nDeltaY )
                {
                    eDragInsertMode = INS_CELLSDOWN;
                }
                else
                {
                    eDragInsertMode = INS_CELLSRIGHT;
                }

                if ( ( eDragInsertMode == INS_CELLSDOWN && nNewDragY <= nSourceEndY &&
                       ( nNewDragX + nSizeX - 1 ) >= nSourceStartX && nNewDragX <= nSourceEndX &&
                       ( nNewDragX != nSourceStartX || nNewDragY >= nSourceStartY ) ) ||
                     ( eDragInsertMode == INS_CELLSRIGHT && nNewDragX <= nSourceEndX &&
                       ( nNewDragY + nSizeY - 1 ) >= nSourceStartY && nNewDragY <= nSourceEndY &&
                       ( nNewDragY != nSourceStartY || nNewDragX >= nSourceStartX ) ) )
                {
                    if ( bDragRect )
                    {
                        bDragRect = false;
                        UpdateDragRectOverlay();
                    }
                    return DND_ACTION_NONE;
                }
            }
            else
            {
                if ( static_cast< long >( nSizeX ) >= static_cast< long >( nSizeY ) )
                {
                    eDragInsertMode = INS_CELLSDOWN;

                }
                else
                {
                    eDragInsertMode = INS_CELLSRIGHT;
                }
            }
        }

        if ( nNewDragX != nDragStartX || nNewDragY != nDragStartY ||
             nDragStartX+nSizeX-1 != nDragEndX || nDragStartY+nSizeY-1 != nDragEndY ||
             !bDragRect || eDragInsertMode != meDragInsertMode )
        {
            nDragStartX = nNewDragX;
            nDragStartY = nNewDragY;
            nDragEndX = nDragStartX+nSizeX-1;
            nDragEndY = nDragStartY+nSizeY-1;
            bDragRect = true;
            meDragInsertMode = eDragInsertMode;

            UpdateDragRectOverlay();
        }
    }

    return rEvt.mnAction;
}

sal_Int8 ScGridWindow::AcceptDrop( const AcceptDropEvent& rEvt )
{
    const ScDragData& rData = SC_MOD()->GetDragData();
    if ( rEvt.mbLeaving )
    {
        DrawMarkDropObj( nullptr );
        if ( rData.pCellTransfer )
            return AcceptPrivateDrop( rEvt );   // hide drop marker for internal D&D
        else
            return rEvt.mnAction;
    }

    if ( pViewData->GetDocShell()->IsReadOnly() )
        return DND_ACTION_NONE;

    ScDocument* pThisDoc = pViewData->GetDocument();
    sal_Int8 nRet = DND_ACTION_NONE;

    if (rData.pCellTransfer)
    {
        ScRange aSource = rData.pCellTransfer->GetRange();
        if ( aSource.aStart.Col() != 0 || aSource.aEnd.Col() != pThisDoc->MaxCol() ||
             aSource.aStart.Row() != 0 || aSource.aEnd.Row() != pThisDoc->MaxRow() )
            DropScroll( rEvt.maPosPixel );

        nRet = AcceptPrivateDrop( rEvt );
    }
    else
    {
        if ( !rData.aLinkDoc.isEmpty() )
        {
            OUString aThisName;
            ScDocShell* pDocSh = pViewData->GetDocShell();
            if (pDocSh && pDocSh->HasName())
                aThisName = pDocSh->GetMedium()->GetName();

            if ( rData.aLinkDoc != aThisName )
                nRet = rEvt.mnAction;
        }
        else if (!rData.aJumpTarget.isEmpty())
        {
            //  internal bookmarks (from Navigator)
            //  local jumps from an unnamed document are possible only within a document

            if ( !rData.pJumpLocalDoc || rData.pJumpLocalDoc == pViewData->GetDocument() )
                nRet = rEvt.mnAction;
        }
        else
        {
            sal_Int8 nMyAction = rEvt.mnAction;

            // clear DND_ACTION_LINK when other actions are set. The usage below cannot handle
            // multiple set values
            if((nMyAction & DND_ACTION_LINK) && (nMyAction & DND_ACTION_COPYMOVE))
            {
                nMyAction &= ~DND_ACTION_LINK;
            }

            if ( !rData.pDrawTransfer ||
                    !IsMyModel(rData.pDrawTransfer->GetDragSourceView()) )      // drawing within the document
                if ( rEvt.mbDefault && nMyAction == DND_ACTION_MOVE )
                    nMyAction = DND_ACTION_COPY;

            SdrObject* pHitObj = pThisDoc->GetObjectAtPoint(
                        pViewData->GetTabNo(), PixelToLogic(rEvt.maPosPixel) );
            if ( pHitObj && nMyAction == DND_ACTION_LINK )
            {
                if ( IsDropFormatSupported(SotClipboardFormatId::SVXB)
                    || IsDropFormatSupported(SotClipboardFormatId::GDIMETAFILE)
                    || IsDropFormatSupported(SotClipboardFormatId::PNG)
                    || IsDropFormatSupported(SotClipboardFormatId::BITMAP) )
                {
                    //  graphic dragged onto drawing object
                    DrawMarkDropObj( pHitObj );
                    nRet = nMyAction;
                }
            }
            if (!nRet)
            {
                DrawMarkDropObj(nullptr);

                switch ( nMyAction )
                {
                    case DND_ACTION_COPY:
                    case DND_ACTION_MOVE:
                    case DND_ACTION_COPYMOVE:
                        {
                            bool bMove = ( nMyAction == DND_ACTION_MOVE );
                            if ( IsDropFormatSupported( SotClipboardFormatId::EMBED_SOURCE ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::LINK_SOURCE ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::EMBED_SOURCE_OLE ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::LINK_SOURCE_OLE ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::EMBEDDED_OBJ_OLE ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::STRING ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::STRING_TSVC ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::SYLK ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::LINK ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::HTML ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::HTML_SIMPLE ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::DIF ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::DRAWING ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::SVXB ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::RTF ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::RICHTEXT ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::GDIMETAFILE ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::PNG ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::BITMAP ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::SBA_DATAEXCHANGE ) ||
                                 IsDropFormatSupported( SotClipboardFormatId::SBA_FIELDDATAEXCHANGE ) ||
                                 ( !bMove && (
                                    IsDropFormatSupported( SotClipboardFormatId::FILE_LIST ) ||
                                     IsDropFormatSupported( SotClipboardFormatId::SIMPLE_FILE ) ||
                                     IsDropFormatSupported( SotClipboardFormatId::SOLK ) ||
                                     IsDropFormatSupported( SotClipboardFormatId::UNIFORMRESOURCELOCATOR ) ||
                                     IsDropFormatSupported( SotClipboardFormatId::NETSCAPE_BOOKMARK ) ||
                                     IsDropFormatSupported( SotClipboardFormatId::FILEGRPDESCRIPTOR ) ) ) )
                            {
                                nRet = nMyAction;
                            }
                        }
                        break;
                    case DND_ACTION_LINK:
                        if ( IsDropFormatSupported( SotClipboardFormatId::LINK_SOURCE ) ||
                             IsDropFormatSupported( SotClipboardFormatId::LINK_SOURCE_OLE ) ||
                             IsDropFormatSupported( SotClipboardFormatId::LINK ) ||
                             IsDropFormatSupported( SotClipboardFormatId::FILE_LIST ) ||
                             IsDropFormatSupported( SotClipboardFormatId::SIMPLE_FILE ) ||
                             IsDropFormatSupported( SotClipboardFormatId::SOLK ) ||
                             IsDropFormatSupported( SotClipboardFormatId::UNIFORMRESOURCELOCATOR ) ||
                             IsDropFormatSupported( SotClipboardFormatId::NETSCAPE_BOOKMARK ) ||
                             IsDropFormatSupported( SotClipboardFormatId::FILEGRPDESCRIPTOR ) )
                        {
                            nRet = nMyAction;
                        }
                        break;
                }

                if ( nRet )
                {
                    // Simple check for protection: It's not known here if the drop will result
                    // in cells or drawing objects (some formats can be both) and how many cells
                    // the result will be. But if IsFormatEditable for the drop cell position
                    // is sal_False (ignores matrix formulas), nothing can be pasted, so the drop
                    // can already be rejected here.

                    Point aPos = rEvt.maPosPixel;
                    SCCOL nPosX;
                    SCROW nPosY;
                    pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nPosX, nPosY );
                    SCTAB nTab = pViewData->GetTabNo();
                    ScDocument* pDoc = pViewData->GetDocument();

                    ScEditableTester aTester( pDoc, nTab, nPosX,nPosY, nPosX,nPosY );
                    if ( !aTester.IsFormatEditable() )
                        nRet = DND_ACTION_NONE;             // forbidden
                }
            }
        }

        //  scroll only for accepted formats
        if (nRet)
            DropScroll( rEvt.maPosPixel );
    }

    return nRet;
}

static SotClipboardFormatId lcl_GetDropFormatId( const uno::Reference<datatransfer::XTransferable>& xTransfer, bool bPreferText )
{
    TransferableDataHelper aDataHelper( xTransfer );

    if ( !aDataHelper.HasFormat( SotClipboardFormatId::SBA_DATAEXCHANGE ) )
    {
        //  use bookmark formats if no sba is present

        if ( aDataHelper.HasFormat( SotClipboardFormatId::SOLK ) )
            return SotClipboardFormatId::SOLK;
        else if ( aDataHelper.HasFormat( SotClipboardFormatId::UNIFORMRESOURCELOCATOR ) )
            return SotClipboardFormatId::UNIFORMRESOURCELOCATOR;
        else if ( aDataHelper.HasFormat( SotClipboardFormatId::NETSCAPE_BOOKMARK ) )
            return SotClipboardFormatId::NETSCAPE_BOOKMARK;
        else if ( aDataHelper.HasFormat( SotClipboardFormatId::FILEGRPDESCRIPTOR ) )
            return SotClipboardFormatId::FILEGRPDESCRIPTOR;
    }

    SotClipboardFormatId nFormatId = SotClipboardFormatId::NONE;
    if ( aDataHelper.HasFormat( SotClipboardFormatId::DRAWING ) )
        nFormatId = SotClipboardFormatId::DRAWING;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::SVXB ) )
        nFormatId = SotClipboardFormatId::SVXB;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::EMBED_SOURCE ) )
    {
        //  If it's a Writer object, insert RTF instead of OLE

        bool bDoRtf = false;
        tools::SvRef<SotStorageStream> xStm;
        TransferableObjectDescriptor aObjDesc;
        if( aDataHelper.GetTransferableObjectDescriptor( SotClipboardFormatId::OBJECTDESCRIPTOR, aObjDesc ) &&
            aDataHelper.GetSotStorageStream( SotClipboardFormatId::EMBED_SOURCE, xStm ) )
        {
            tools::SvRef<SotStorage> xStore( new SotStorage( *xStm ) );
            bDoRtf = ( ( aObjDesc.maClassName == SvGlobalName( SO3_SW_CLASSID ) ||
                         aObjDesc.maClassName == SvGlobalName( SO3_SWWEB_CLASSID ) )
                       && ( aDataHelper.HasFormat( SotClipboardFormatId::RTF ) || aDataHelper.HasFormat( SotClipboardFormatId::RICHTEXT ) ) );
        }
        if ( bDoRtf )
            nFormatId = aDataHelper.HasFormat( SotClipboardFormatId::RTF ) ? SotClipboardFormatId::RTF : SotClipboardFormatId::RICHTEXT;
        else
            nFormatId = SotClipboardFormatId::EMBED_SOURCE;
    }
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::LINK_SOURCE ) )
        nFormatId = SotClipboardFormatId::LINK_SOURCE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::SBA_DATAEXCHANGE ) )
        nFormatId = SotClipboardFormatId::SBA_DATAEXCHANGE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::SBA_FIELDDATAEXCHANGE ) )
        nFormatId = SotClipboardFormatId::SBA_FIELDDATAEXCHANGE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::BIFF_8 ) )
        nFormatId = SotClipboardFormatId::BIFF_8;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::BIFF_5 ) )
        nFormatId = SotClipboardFormatId::BIFF_5;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::EMBED_SOURCE_OLE ) )
        nFormatId = SotClipboardFormatId::EMBED_SOURCE_OLE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::EMBEDDED_OBJ_OLE ) )
        nFormatId = SotClipboardFormatId::EMBEDDED_OBJ_OLE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::LINK_SOURCE_OLE ) )
        nFormatId = SotClipboardFormatId::LINK_SOURCE_OLE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::RTF ) )
        nFormatId = SotClipboardFormatId::RTF;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::RICHTEXT ) )
        nFormatId = SotClipboardFormatId::RICHTEXT;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::HTML ) )
        nFormatId = SotClipboardFormatId::HTML;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::HTML_SIMPLE ) )
        nFormatId = SotClipboardFormatId::HTML_SIMPLE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::SYLK ) )
        nFormatId = SotClipboardFormatId::SYLK;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::LINK ) )
        nFormatId = SotClipboardFormatId::LINK;
    else if ( bPreferText && aDataHelper.HasFormat( SotClipboardFormatId::STRING ) ) // #i86734# the behaviour introduced in #i62773# is wrong when pasting
        nFormatId = SotClipboardFormatId::STRING;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::FILE_LIST ) )
        nFormatId = SotClipboardFormatId::FILE_LIST;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::SIMPLE_FILE ) )    // #i62773# FILE_LIST/FILE before STRING (Unix file managers)
        nFormatId = SotClipboardFormatId::SIMPLE_FILE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::STRING_TSVC ) )
        nFormatId = SotClipboardFormatId::STRING_TSVC;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::STRING ) )
        nFormatId = SotClipboardFormatId::STRING;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::GDIMETAFILE ) )
        nFormatId = SotClipboardFormatId::GDIMETAFILE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::PNG ) )
        nFormatId = SotClipboardFormatId::PNG;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::BITMAP ) )
        nFormatId = SotClipboardFormatId::BITMAP;

    return nFormatId;
}

static SotClipboardFormatId lcl_GetDropLinkId( const uno::Reference<datatransfer::XTransferable>& xTransfer )
{
    TransferableDataHelper aDataHelper( xTransfer );

    SotClipboardFormatId nFormatId = SotClipboardFormatId::NONE;
    if ( aDataHelper.HasFormat( SotClipboardFormatId::LINK_SOURCE ) )
        nFormatId = SotClipboardFormatId::LINK_SOURCE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::LINK_SOURCE_OLE ) )
        nFormatId = SotClipboardFormatId::LINK_SOURCE_OLE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::LINK ) )
        nFormatId = SotClipboardFormatId::LINK;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::FILE_LIST ) )
        nFormatId = SotClipboardFormatId::FILE_LIST;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::SIMPLE_FILE ) )
        nFormatId = SotClipboardFormatId::SIMPLE_FILE;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::SOLK ) )
        nFormatId = SotClipboardFormatId::SOLK;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::UNIFORMRESOURCELOCATOR ) )
        nFormatId = SotClipboardFormatId::UNIFORMRESOURCELOCATOR;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::NETSCAPE_BOOKMARK ) )
        nFormatId = SotClipboardFormatId::NETSCAPE_BOOKMARK;
    else if ( aDataHelper.HasFormat( SotClipboardFormatId::FILEGRPDESCRIPTOR ) )
        nFormatId = SotClipboardFormatId::FILEGRPDESCRIPTOR;

    return nFormatId;
}

sal_Int8 ScGridWindow::ExecutePrivateDrop( const ExecuteDropEvent& rEvt )
{
    // hide drop marker
    bDragRect = false;
    UpdateDragRectOverlay();

    ScModule* pScMod = SC_MOD();
    const ScDragData& rData = pScMod->GetDragData();

    return DropTransferObj( rData.pCellTransfer, nDragStartX, nDragStartY,
                                PixelToLogic(rEvt.maPosPixel), rEvt.mnAction );
}

sal_Int8 ScGridWindow::DropTransferObj( ScTransferObj* pTransObj, SCCOL nDestPosX, SCROW nDestPosY,
                                        const Point& rLogicPos, sal_Int8 nDndAction )
{
    if ( !pTransObj )
        return 0;

    ScDocument* pSourceDoc = pTransObj->GetSourceDocument();
    ScDocShell* pDocSh     = pViewData->GetDocShell();
    ScDocument* pThisDoc   = pViewData->GetDocument();
    ScViewFunc* pView      = pViewData->GetView();
    SCTAB       nThisTab   = pViewData->GetTabNo();
    ScDragSrc   nFlags     = pTransObj->GetDragSourceFlags();

    bool bIsNavi = (nFlags & ScDragSrc::Navigator) == ScDragSrc::Navigator;
    bool bIsMove = ( nDndAction == DND_ACTION_MOVE && !bIsNavi );

    // workaround for wrong nDndAction on Windows when pressing solely
    // the Alt key during drag and drop;
    // can be removed after #i79215# has been fixed
    if ( meDragInsertMode != INS_NONE )
    {
        bIsMove = ( nDndAction & DND_ACTION_MOVE && !bIsNavi );
    }

    bool bIsLink = ( nDndAction == DND_ACTION_LINK );

    ScRange aSource = pTransObj->GetRange();

    //  only use visible tab from source range - when dragging within one table,
    //  all selected tables at the time of dropping are used (handled in MoveBlockTo)
    SCTAB nSourceTab = pTransObj->GetVisibleTab();
    aSource.aStart.SetTab( nSourceTab );
    aSource.aEnd.SetTab( nSourceTab );

    SCCOL nSizeX = aSource.aEnd.Col() - aSource.aStart.Col() + 1;
    SCROW nSizeY = (bIsMove ? (aSource.aEnd.Row() - aSource.aStart.Row() + 1) :
            pTransObj->GetNonFilteredRows());   // copy/link: no filtered rows
    ScRange aDest( nDestPosX, nDestPosY, nThisTab,
                   nDestPosX + nSizeX - 1, nDestPosY + nSizeY - 1, nThisTab );

    /* NOTE: AcceptPrivateDrop() already checked for filtered conditions during
     * dragging and adapted drawing of the selection frame. We check here
     * (again) because this may actually also be called from PasteSelection(),
     * we would have to duplicate determination of flags and destination range
     * and would lose the context of the "filtered destination is OK" cases
     * below, which is already awkward enough as is. */

    // Don't move filtered source.
    bool bFiltered = (bIsMove && pTransObj->HasFilteredRows());
    if (!bFiltered)
    {
        if (pSourceDoc != pThisDoc && ((nFlags & ScDragSrc::Table) ||
                    (!bIsLink && meDragInsertMode == INS_NONE)))
        {
            // Nothing. Either entire sheet to be dropped, or the one case
            // where PasteFromClip() is to be called that handles a filtered
            // destination itself. Drag-copy from another document without
            // inserting cells.
        }
        else
            // Don't copy or move to filtered destination.
            bFiltered = ScViewUtil::HasFiltered( aDest, pThisDoc);
    }

    bool bDone = false;

    if (!bFiltered && pSourceDoc == pThisDoc)
    {
        if (nFlags & ScDragSrc::Table) // whole sheet?
        {
            if ( pThisDoc->IsDocEditable() )
            {
                SCTAB nSrcTab = aSource.aStart.Tab();
                pViewData->GetDocShell()->MoveTable( nSrcTab, nThisTab, !bIsMove, true );   // with Undo
                pView->SetTabNo( nThisTab, true );
                bDone = true;
            }
        }
        else                                        // move/copy block
        {
            OUString aChartName;
            if (pThisDoc->HasChartAtPoint( nThisTab, rLogicPos, aChartName ))
            {
                OUString aRangeName(aSource.Format(*pThisDoc, ScRefFlags::RANGE_ABS_3D));
                SfxStringItem aNameItem( SID_CHART_NAME, aChartName );
                SfxStringItem aRangeItem( SID_CHART_SOURCE, aRangeName );
                sal_uInt16 nId = bIsMove ? SID_CHART_SOURCE : SID_CHART_ADDSOURCE;
                pViewData->GetDispatcher().ExecuteList(nId,
                        SfxCallMode::ASYNCHRON | SfxCallMode::RECORD,
                        { &aRangeItem, &aNameItem });
                bDone = true;
            }
            else if ( pThisDoc->GetDPAtCursor( nDestPosX, nDestPosY, nThisTab ) )
            {
                // drop on DataPilot table: try to sort, fail if that isn't possible

                ScAddress aDestPos( nDestPosX, nDestPosY, nThisTab );
                if ( aDestPos != aSource.aStart )
                    bDone = pViewData->GetView()->DataPilotMove( aSource, aDestPos );
                else
                    bDone = true;   // same position: nothing
            }
            else if ( nDestPosX != aSource.aStart.Col() || nDestPosY != aSource.aStart.Row() ||
                        nSourceTab != nThisTab )
            {
                OUString aUndo = ScResId( bIsMove ? STR_UNDO_MOVE : STR_UNDO_COPY );
                pDocSh->GetUndoManager()->EnterListAction( aUndo, aUndo, 0, pViewData->GetViewShell()->GetViewShellId() );

                SCCOL nCorrectCursorPosCol = 0;
                SCROW nCorrectCursorPosRow = 0;

                bDone = true;
                if ( meDragInsertMode != INS_NONE )
                {
                    // call with bApi = sal_True to avoid error messages in drop handler
                    bDone = pDocSh->GetDocFunc().InsertCells( aDest, nullptr, meDragInsertMode, true /*bRecord*/, true /*bApi*/, true /*bPartOfPaste*/ );
                    if ( bDone )
                    {
                        if ( nThisTab == nSourceTab )
                        {
                            if ( meDragInsertMode == INS_CELLSDOWN &&
                                 nDestPosX == aSource.aStart.Col() && nDestPosY < aSource.aStart.Row() )
                            {
                                ScRange aErrorRange( ScAddress::UNINITIALIZED );
                                bDone = aSource.Move( 0, nSizeY, 0, aErrorRange, pSourceDoc );
                                nCorrectCursorPosRow = nSizeY;
                            }
                            else if ( meDragInsertMode == INS_CELLSRIGHT &&
                                      nDestPosY == aSource.aStart.Row() && nDestPosX < aSource.aStart.Col() )
                            {
                                ScRange aErrorRange( ScAddress::UNINITIALIZED );
                                bDone = aSource.Move( nSizeX, 0, 0, aErrorRange, pSourceDoc );
                                nCorrectCursorPosCol = nSizeX;
                            }
                        }
                        pDocSh->UpdateOle( pViewData );
                        pView->CellContentChanged();
                    }
                }

                if ( bDone )
                {
                    if ( bIsLink )
                    {
                        bDone = pView->LinkBlock( aSource, aDest.aStart );
                    }
                    else
                    {
                        bDone = pView->MoveBlockTo( aSource, aDest.aStart, bIsMove );
                    }
                }

                if ( bDone && meDragInsertMode != INS_NONE && bIsMove && nThisTab == nSourceTab )
                {
                    DelCellCmd eCmd = DelCellCmd::NONE;
                    if ( meDragInsertMode == INS_CELLSDOWN )
                    {
                        eCmd = DelCellCmd::CellsUp;
                    }
                    else if ( meDragInsertMode == INS_CELLSRIGHT )
                    {
                        eCmd = DelCellCmd::CellsLeft;
                    }

                    if ( ( eCmd == DelCellCmd::CellsUp  && nDestPosX == aSource.aStart.Col() ) ||
                         ( eCmd == DelCellCmd::CellsLeft && nDestPosY == aSource.aStart.Row() ) )
                    {
                        // call with bApi = sal_True to avoid error messages in drop handler
                        bDone = pDocSh->GetDocFunc().DeleteCells( aSource, nullptr, eCmd, true /*bApi*/ );
                        if ( bDone )
                        {
                            if ( eCmd == DelCellCmd::CellsUp && nDestPosY > aSource.aEnd.Row() )
                            {
                                ScRange aErrorRange( ScAddress::UNINITIALIZED );
                                bDone = aDest.Move( 0, -nSizeY, 0, aErrorRange, pThisDoc );
                            }
                            else if ( eCmd == DelCellCmd::CellsLeft && nDestPosX > aSource.aEnd.Col() )
                            {
                                ScRange aErrorRange( ScAddress::UNINITIALIZED );
                                bDone = aDest.Move( -nSizeX, 0, 0, aErrorRange, pThisDoc );
                            }
                            pDocSh->UpdateOle( pViewData );
                            pView->CellContentChanged();
                        }
                    }
                }

                if ( bDone )
                {
                    pView->MarkRange( aDest, false );

                    SCCOL nDCol;
                    SCROW nDRow;
                    if (pTransObj->WasSourceCursorInSelection())
                    {
                        nDCol = pTransObj->GetSourceCursorX() - aSource.aStart.Col() + nCorrectCursorPosCol;
                        nDRow = pTransObj->GetSourceCursorY() - aSource.aStart.Row() + nCorrectCursorPosRow;
                    }
                    else
                    {
                        nDCol = 0;
                        nDRow = 0;
                    }
                    pView->SetCursor( aDest.aStart.Col() + nDCol, aDest.aStart.Row() + nDRow );
                }

                pDocSh->GetUndoManager()->LeaveListAction();

            }
            else
                bDone = true;       // nothing to do
        }

        if (bDone)
            pTransObj->SetDragWasInternal();    // don't delete source in DragFinished
    }
    else if ( !bFiltered && pSourceDoc )                        // between documents
    {
        if (nFlags & ScDragSrc::Table)          // copy/link sheets between documents
        {
            if ( pThisDoc->IsDocEditable() )
            {
                ScDocShell* pSrcShell = pTransObj->GetSourceDocShell();

                std::vector<SCTAB> nTabs;

                ScMarkData  aMark       = pTransObj->GetSourceMarkData();
                SCTAB       nTabCount   = pSourceDoc->GetTableCount();

                for(SCTAB i=0; i<nTabCount; i++)
                {
                    if(aMark.GetTableSelect(i))
                    {
                        nTabs.push_back(i);
                        for(SCTAB j=i+1;j<nTabCount;j++)
                        {
                            if((!pSourceDoc->IsVisible(j))&&(pSourceDoc->IsScenario(j)))
                            {
                                nTabs.push_back( j );
                                i=j;
                            }
                            else break;
                        }
                    }
                }

                pView->ImportTables( pSrcShell,static_cast<SCTAB>(nTabs.size()), nTabs.data(), bIsLink, nThisTab );
                bDone = true;
            }
        }
        else if ( bIsLink )
        {
            //  as in PasteDDE
            //  (external references might be used instead?)

            SfxObjectShell* pSourceSh = pSourceDoc->GetDocumentShell();
            OSL_ENSURE(pSourceSh, "drag document has no shell");
            if (pSourceSh)
            {
                OUString aUndo = ScResId( STR_UNDO_COPY );
                pDocSh->GetUndoManager()->EnterListAction( aUndo, aUndo, 0, pViewData->GetViewShell()->GetViewShellId() );

                bDone = true;
                if ( meDragInsertMode != INS_NONE )
                {
                    // call with bApi = sal_True to avoid error messages in drop handler
                    bDone = pDocSh->GetDocFunc().InsertCells( aDest, nullptr, meDragInsertMode, true /*bRecord*/, true /*bApi*/, true /*bPartOfPaste*/ );
                    if ( bDone )
                    {
                        pDocSh->UpdateOle( pViewData );
                        pView->CellContentChanged();
                    }
                }

                if ( bDone )
                {
                    OUString aApp = Application::GetAppName();
                    OUString aTopic = pSourceSh->GetTitle( SFX_TITLE_FULLNAME );
                    OUString aItem(aSource.Format(*pSourceDoc, ScRefFlags::VALID | ScRefFlags::TAB_3D));

                    // TODO: we could define ocQuote for "
                    const OUString aQuote('"');
                    const OUString& sSep = ScCompiler::GetNativeSymbol( ocSep);
                    OUStringBuffer aFormula;
                    aFormula.append('=');
                    aFormula.append(ScCompiler::GetNativeSymbol(ocDde));
                    aFormula.append(ScCompiler::GetNativeSymbol(ocOpen));
                    aFormula.append(aQuote);
                    aFormula.append(aApp);
                    aFormula.append(aQuote);
                    aFormula.append(sSep);
                    aFormula.append(aQuote);
                    aFormula.append(aTopic);
                    aFormula.append(aQuote);
                    aFormula.append(sSep);
                    aFormula.append(aQuote);
                    aFormula.append(aItem);
                    aFormula.append(aQuote);
                    aFormula.append(ScCompiler::GetNativeSymbol(ocClose));

                    pView->DoneBlockMode();
                    pView->InitBlockMode( nDestPosX, nDestPosY, nThisTab );
                    pView->MarkCursor( nDestPosX + nSizeX - 1,
                                       nDestPosY + nSizeY - 1, nThisTab );

                    pView->EnterMatrix( aFormula.makeStringAndClear(), ::formula::FormulaGrammar::GRAM_NATIVE );

                    pView->MarkRange( aDest, false );
                    pView->SetCursor( aDest.aStart.Col(), aDest.aStart.Row() );
                }

                pDocSh->GetUndoManager()->LeaveListAction();
            }
        }
        else
        {
            //! HasSelectedBlockMatrixFragment without selected sheet?
            //! or don't start dragging on a part of a matrix

            OUString aUndo = ScResId( bIsMove ? STR_UNDO_MOVE : STR_UNDO_COPY );
            pDocSh->GetUndoManager()->EnterListAction( aUndo, aUndo, 0, pViewData->GetViewShell()->GetViewShellId() );

            bDone = true;
            if ( meDragInsertMode != INS_NONE )
            {
                // call with bApi = sal_True to avoid error messages in drop handler
                bDone = pDocSh->GetDocFunc().InsertCells( aDest, nullptr, meDragInsertMode, true /*bRecord*/, true /*bApi*/, true /*bPartOfPaste*/ );
                if ( bDone )
                {
                    pDocSh->UpdateOle( pViewData );
                    pView->CellContentChanged();
                }
            }

            if ( bDone )
            {
                pView->Unmark();  // before SetCursor, so CheckSelectionTransfer isn't called with a selection
                pView->SetCursor( nDestPosX, nDestPosY );
                bDone = pView->PasteFromClip( InsertDeleteFlags::ALL, pTransObj->GetDocument() );  // clip-doc
                if ( bDone )
                {
                    pView->MarkRange( aDest, false );
                    pView->SetCursor( aDest.aStart.Col(), aDest.aStart.Row() );
                }
            }

            pDocSh->GetUndoManager()->LeaveListAction();

            //  no longer call ResetMark here - the inserted block has been selected
            //  and may have been copied to primary selection
        }
    }

    sal_Int8 nRet = bDone ? nDndAction : DND_ACTION_NONE;
    return nRet;
}

sal_Int8 ScGridWindow::ExecuteDrop( const ExecuteDropEvent& rEvt )
{
    DrawMarkDropObj( nullptr );    // drawing layer

    ScModule* pScMod = SC_MOD();
    const ScDragData& rData = pScMod->GetDragData();
    if (rData.pCellTransfer)
        return ExecutePrivateDrop( rEvt );

    Point aPos = rEvt.maPosPixel;

    if ( !rData.aLinkDoc.isEmpty() )
    {
        //  try to insert a link

        bool bOk = true;
        OUString aThisName;
        ScDocShell* pDocSh = pViewData->GetDocShell();
        if (pDocSh && pDocSh->HasName())
            aThisName = pDocSh->GetMedium()->GetName();

        if ( rData.aLinkDoc == aThisName )              // error - no link within a document
            bOk = false;
        else
        {
            ScViewFunc* pView = pViewData->GetView();
            if ( !rData.aLinkTable.isEmpty() )
                pView->InsertTableLink( rData.aLinkDoc, EMPTY_OUSTRING, EMPTY_OUSTRING,
                                        rData.aLinkTable );
            else if ( !rData.aLinkArea.isEmpty() )
            {
                SCCOL  nPosX;
                SCROW  nPosY;
                pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nPosX, nPosY );
                pView->MoveCursorAbs( nPosX, nPosY, SC_FOLLOW_NONE, false, false );

                pView->InsertAreaLink( rData.aLinkDoc, EMPTY_OUSTRING, EMPTY_OUSTRING,
                                        rData.aLinkArea );
            }
            else
            {
                OSL_FAIL("drop with link: no sheet nor area");
                bOk = false;
            }
        }

        return bOk ? rEvt.mnAction : DND_ACTION_NONE;           // don't try anything else
    }

    Point aLogicPos = PixelToLogic(aPos);
    bool bIsLink = ( rEvt.mnAction == DND_ACTION_LINK );

    if (!bIsLink && rData.pDrawTransfer)
    {
        ScDragSrc nFlags = rData.pDrawTransfer->GetDragSourceFlags();

        bool bIsNavi = (nFlags & ScDragSrc::Navigator) == ScDragSrc::Navigator;
        bool bIsMove = ( rEvt.mnAction == DND_ACTION_MOVE && !bIsNavi );

        bPasteIsMove = bIsMove;

        pViewData->GetView()->PasteDraw(
            aLogicPos, rData.pDrawTransfer->GetModel(), false, "A", "B");

        if (bPasteIsMove)
            rData.pDrawTransfer->SetDragWasInternal();
        bPasteIsMove = false;

        return rEvt.mnAction;
    }

    SCCOL  nPosX;
    SCROW  nPosY;
    pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nPosX, nPosY );

    if (!rData.aJumpTarget.isEmpty())
    {
        //  internal bookmark (from Navigator)
        //  bookmark clipboard formats are in PasteScDataObject

        if ( !rData.pJumpLocalDoc || rData.pJumpLocalDoc == pViewData->GetDocument() )
        {
            pViewData->GetViewShell()->InsertBookmark( rData.aJumpText, rData.aJumpTarget,
                                                        nPosX, nPosY );
            return rEvt.mnAction;
        }
    }

    ScDocument* pThisDoc = pViewData->GetDocument();
    SdrObject* pHitObj = pThisDoc->GetObjectAtPoint( pViewData->GetTabNo(), PixelToLogic(aPos) );
    if ( pHitObj && bIsLink )
    {
        //  dropped on drawing object
        //  PasteOnDrawObjectLinked checks for valid formats
        if ( pViewData->GetView()->PasteOnDrawObjectLinked( rEvt.maDropEvent.Transferable, *pHitObj ) )
            return rEvt.mnAction;
    }

    bool bDone = false;

    SotClipboardFormatId nFormatId = bIsLink ?
                        lcl_GetDropLinkId( rEvt.maDropEvent.Transferable ) :
                        lcl_GetDropFormatId( rEvt.maDropEvent.Transferable, false );
    if ( nFormatId != SotClipboardFormatId::NONE )
    {
        pScMod->SetInExecuteDrop( true );   // #i28468# prevent error messages from PasteDataFormat
        bPasteIsDrop = true;
        bDone = pViewData->GetView()->PasteDataFormat(
                    nFormatId, rEvt.maDropEvent.Transferable, nPosX, nPosY, &aLogicPos, bIsLink );
        bPasteIsDrop = false;
        pScMod->SetInExecuteDrop( false );
    }

    sal_Int8 nRet = bDone ? rEvt.mnAction : DND_ACTION_NONE;
    return nRet;
}

void ScGridWindow::PasteSelection( const Point& rPosPixel )
{
    Point aLogicPos = PixelToLogic( rPosPixel );

    SCCOL  nPosX;
    SCROW  nPosY;
    pViewData->GetPosFromPixel( rPosPixel.X(), rPosPixel.Y(), eWhich, nPosX, nPosY );

    // If the mouse down was inside a visible note window, ignore it and
    // leave it up to the ScPostIt to handle it
    SdrView* pDrawView = pViewData->GetViewShell()->GetSdrView();
    if (pDrawView)
    {
        const size_t nCount = pDrawView->GetMarkedObjectCount();
        for (size_t i = 0; i < nCount; ++i)
        {
            SdrObject* pObj = pDrawView->GetMarkedObjectByIndex(i);
            if (pObj && pObj->GetLogicRect().IsInside(aLogicPos))
            {
                // Inside an active drawing object.  Bail out.
                return;
            }
        }
    }

    ScSelectionTransferObj* pOwnSelection = SC_MOD()->GetSelectionTransfer();
    if ( pOwnSelection )
    {
        //  within Calc

        // keep a reference to the data in case the selection is changed during paste
        rtl::Reference<ScTransferObj> pCellTransfer = pOwnSelection->GetCellData();
        if ( pCellTransfer )
        {
            DropTransferObj( pCellTransfer.get(), nPosX, nPosY, aLogicPos, DND_ACTION_COPY );
        }
        else
        {
            // keep a reference to the data in case the selection is changed during paste
            rtl::Reference<ScDrawTransferObj> pDrawTransfer = pOwnSelection->GetDrawData();
            if ( pDrawTransfer )
            {
                //  bSameDocClipboard argument for PasteDraw is needed
                //  because only DragData is checked directly inside PasteDraw
                pViewData->GetView()->PasteDraw(
                    aLogicPos, pDrawTransfer->GetModel(), false,
                    pDrawTransfer->GetShellID(), SfxObjectShell::CreateShellID(pViewData->GetDocShell()));
            }
        }
    }
    else
    {
        //  get selection from system

        TransferableDataHelper aDataHelper( TransferableDataHelper::CreateFromSelection( this ) );
        const uno::Reference<datatransfer::XTransferable>& xTransferable = aDataHelper.GetTransferable();
        if ( xTransferable.is() )
        {
            SotClipboardFormatId nFormatId = lcl_GetDropFormatId( xTransferable, true );
            if ( nFormatId != SotClipboardFormatId::NONE )
            {
                bPasteIsDrop = true;
                pViewData->GetView()->PasteDataFormat( nFormatId, xTransferable, nPosX, nPosY, &aLogicPos );
                bPasteIsDrop = false;
            }
        }
    }
}

void ScGridWindow::UpdateEditViewPos()
{
    if (pViewData->HasEditView(eWhich))
    {
        EditView* pView;
        SCCOL nCol;
        SCROW nRow;
        pViewData->GetEditView( eWhich, pView, nCol, nRow );
        SCCOL nEndCol = pViewData->GetEditEndCol();
        SCROW nEndRow = pViewData->GetEditEndRow();

        //  hide EditView?

        bool bHide = ( nEndCol<pViewData->GetPosX(eHWhich) || nEndRow<pViewData->GetPosY(eVWhich) );
        if ( SC_MOD()->IsFormulaMode() )
            if ( pViewData->GetTabNo() != pViewData->GetRefTabNo() )
                bHide = true;

        if (bHide)
        {
            tools::Rectangle aRect = pView->GetOutputArea();
            long nHeight = aRect.Bottom() - aRect.Top();
            aRect.SetTop( PixelToLogic(GetOutputSizePixel(), pViewData->GetLogicMode()).
                            Height() * 2 );
            aRect.SetBottom( aRect.Top() + nHeight );
            pView->SetOutputArea( aRect );
            pView->HideCursor();
        }
        else
        {
            // bForceToTop = sal_True for editing
            tools::Rectangle aPixRect = pViewData->GetEditArea( eWhich, nCol, nRow, this, nullptr, true );
            Point aScrPos = PixelToLogic( aPixRect.TopLeft(), pViewData->GetLogicMode() );

            tools::Rectangle aRect = pView->GetOutputArea();
            aRect.SetPos( aScrPos );
            pView->SetOutputArea( aRect );
            pView->ShowCursor();
        }
    }
}

void ScGridWindow::ScrollPixel( long nDifX, long nDifY )
{
    ClickExtern();
    HideNoteMarker();

    SetMapMode(MapMode(MapUnit::MapPixel));
    Scroll( nDifX, nDifY, ScrollFlags::Children );
    SetMapMode( GetDrawMapMode() );             // generated shifted MapMode

    UpdateEditViewPos();

    DrawAfterScroll();
}

// Update Formulas ------------------------------------------------------

void ScGridWindow::UpdateFormulas(SCCOL nX1, SCROW nY1, SCCOL nX2, SCROW nY2)
{
    if (pViewData->GetView()->IsMinimized())
        return;

    if ( nPaintCount )
    {
        // Do not start, switched to paint
        //  (then at least the MapMode would no longer be right)

        bNeedsRepaint = true;           // -> at end of paint run Invalidate on all
        aRepaintPixel = tools::Rectangle();    // All
        return;
    }

    if ( comphelper::LibreOfficeKit::isActive() )
    {
        ScTabViewShell* pViewShell = pViewData->GetViewShell();
        if (nX1 < 0)
            nX1 = pViewShell->GetLOKStartHeaderCol() + 1;
        if (nY1 < 0)
            nY1 = pViewShell->GetLOKStartHeaderRow() + 1;
        if (nX2 < 0)
            nX2 = pViewShell->GetLOKEndHeaderCol();
        if (nY2 < 0)
            nY2 = pViewShell->GetLOKEndHeaderRow();

        if (nX1 < 0 || nY1 < 0) return;
    }
    else
    {
        nX1 = pViewData->GetPosX( eHWhich );
        nY1 = pViewData->GetPosY( eVWhich );
        nX2 = nX1 + pViewData->VisibleCellsX( eHWhich );
        nY2 = nY1 + pViewData->VisibleCellsY( eVWhich );
    }

    if (nX2 < nX1) nX2 = nX1;
    if (nY2 < nY1) nY2 = nY1;

    ScDocument& rDoc = *pViewData->GetDocument();

    if (nX2 > rDoc.MaxCol()) nX2 = rDoc.MaxCol();
    if (nY2 > rDoc.MaxRow()) nY2 = rDoc.MaxRow();

    // Draw( nX1, nY1, nX2, nY2, SC_UPDATE_CHANGED );

    // don't draw directly - instead use OutputData to find changed area and invalidate

    SCROW nPosY = nY1;

    SCTAB nTab = pViewData->GetTabNo();

    if ( !comphelper::LibreOfficeKit::isActive() )
    {
        rDoc.ExtendHidden( nX1, nY1, nX2, nY2, nTab );
    }

    Point aScrPos = pViewData->GetScrPos( nX1, nY1, eWhich );
    long nMirrorWidth = GetSizePixel().Width();
    bool bLayoutRTL = rDoc.IsLayoutRTL( nTab );
    if ( bLayoutRTL )
    {
        long nEndPixel = pViewData->GetScrPos( nX2+1, nPosY, eWhich ).X();
        nMirrorWidth = aScrPos.X() - nEndPixel;
        aScrPos.setX( nEndPixel + 1 );
    }

    long nScrX = aScrPos.X();
    long nScrY = aScrPos.Y();

    double nPPTX = pViewData->GetPPTX();
    double nPPTY = pViewData->GetPPTY();

    ScTableInfo aTabInfo;
    rDoc.FillInfo( aTabInfo, nX1, nY1, nX2, nY2, nTab, nPPTX, nPPTY, false, false );

    Fraction aZoomX = pViewData->GetZoomX();
    Fraction aZoomY = pViewData->GetZoomY();
    ScOutputData aOutputData( this, OUTTYPE_WINDOW, aTabInfo, &rDoc, nTab,
                                nScrX, nScrY, nX1, nY1, nX2, nY2, nPPTX, nPPTY,
                                &aZoomX, &aZoomY );
    aOutputData.SetMirrorWidth( nMirrorWidth );

    aOutputData.FindChanged();

    // #i122149# do not use old GetChangedArea() which used polygon-based Regions, but use
    // the region-band based new version; anyways, only rectangles are added
    vcl::Region aChangedRegion( aOutputData.GetChangedAreaRegion() );   // logic (PixelToLogic)
    if(!aChangedRegion.IsEmpty())
    {
        Invalidate(aChangedRegion);
    }

    CheckNeedsRepaint();    // #i90362# used to be called via Draw() - still needed here
}

void ScGridWindow::UpdateAutoFillMark(bool bMarked, const ScRange& rMarkRange)
{
    if ( bMarked != bAutoMarkVisible || ( bMarked && rMarkRange.aEnd != aAutoMarkPos ) )
    {
        bAutoMarkVisible = bMarked;
        if ( bMarked )
            aAutoMarkPos = rMarkRange.aEnd;

        UpdateAutoFillOverlay();
    }
}

void ScGridWindow::updateLOKValListButton( bool bVisible, const ScAddress& rPos ) const
{
    SCCOL nX = rPos.Col();
    SCROW nY = rPos.Row();
    std::stringstream ss;
    ss << nX << ", " << nY << ", " << static_cast<unsigned int>(bVisible);
    ScTabViewShell* pViewShell = pViewData->GetViewShell();
    pViewShell->libreOfficeKitViewCallback(LOK_CALLBACK_VALIDITY_LIST_BUTTON, ss.str().c_str());
}

void ScGridWindow::UpdateListValPos( bool bVisible, const ScAddress& rPos )
{
    bool bOldButton = bListValButton;
    ScAddress aOldPos = aListValPos;

    bListValButton = bVisible;
    aListValPos = rPos;

    if ( bListValButton )
    {
        if ( !bOldButton || aListValPos != aOldPos )
        {
            // paint area of new button
            if ( comphelper::LibreOfficeKit::isActive() )
            {
                updateLOKValListButton( true, aListValPos );
            }
            else
            {
                Invalidate( PixelToLogic( GetListValButtonRect( aListValPos ) ) );
            }
        }
    }
    if ( bOldButton )
    {
        if ( !bListValButton || aListValPos != aOldPos )
        {
            // paint area of old button
            if ( comphelper::LibreOfficeKit::isActive() )
            {
                updateLOKValListButton( false, aOldPos );
            }
            else
            {
                Invalidate( PixelToLogic( GetListValButtonRect( aOldPos ) ) );
            }
        }
    }
}

void ScGridWindow::HideCursor()
{
    ++nCursorHideCount;
}

void ScGridWindow::ShowCursor()
{
    --nCursorHideCount;
}

void ScGridWindow::GetFocus()
{
    ScTabViewShell* pViewShell = pViewData->GetViewShell();
    pViewShell->SetFormShellAtTop( false );     // focus in GridWindow -> FormShell no longer on top

    if (pViewShell->HasAccessibilityObjects())
        pViewShell->BroadcastAccessibility(ScAccGridWinFocusGotHint(eWhich));

    if ( !SC_MOD()->IsFormulaMode() )
    {
        pViewShell->UpdateInputHandler();
//      StopMarking();      // If Dialog (error), because then no ButtonUp
                            // MO: only when not in RefInput mode
                            //     -> GetFocus/MouseButtonDown order on Mac
    }

    pViewData->GetDocShell()->CheckConfigOptions();
    Window::GetFocus();
}

void ScGridWindow::LoseFocus()
{
    ScTabViewShell* pViewShell = pViewData->GetViewShell();

    if (pViewShell && pViewShell->HasAccessibilityObjects())
        pViewShell->BroadcastAccessibility(ScAccGridWinFocusLostHint(eWhich));

    Window::LoseFocus();
}

bool ScGridWindow::HitRangeFinder( const Point& rMouse, RfCorner& rCorner,
                                sal_uInt16* pIndex, SCCOL* pAddX, SCROW* pAddY)
{
    bool bFound = false;
    ScInputHandler* pHdl = SC_MOD()->GetInputHdl( pViewData->GetViewShell() );
    if (pHdl)
    {
        ScRangeFindList* pRangeFinder = pHdl->GetRangeFindList();
        if ( pRangeFinder && !pRangeFinder->IsHidden() &&
                pRangeFinder->GetDocName() == pViewData->GetDocShell()->GetTitle() )
        {
            ScDocument* pDoc = pViewData->GetDocument();
            SCTAB nTab = pViewData->GetTabNo();
            bool bLayoutRTL = pDoc->IsLayoutRTL( nTab );
            long nLayoutSign = bLayoutRTL ? -1 : 1;

            SCCOL nPosX;
            SCROW nPosY;
            pViewData->GetPosFromPixel( rMouse.X(), rMouse.Y(), eWhich, nPosX, nPosY );
            //  merged (single/Range) ???
            ScAddress aAddr( nPosX, nPosY, nTab );

            Point aCellStart = pViewData->GetScrPos( nPosX, nPosY, eWhich, true );
            Point aCellEnd = aCellStart;
            long nSizeXPix;
            long nSizeYPix;
            pViewData->GetMergeSizePixel( nPosX, nPosY, nSizeXPix, nSizeYPix );

            aCellEnd.AdjustX(nSizeXPix * nLayoutSign );
            aCellEnd.AdjustY(nSizeYPix );

            bool bCornerHorizontalRight;
            bool bCornerHorizontalLeft;
            if ( bLayoutRTL )
            {
                bCornerHorizontalRight = ( rMouse.X() >= aCellEnd.X()       && rMouse.X() <= aCellEnd.X() + 8 );
                bCornerHorizontalLeft  = ( rMouse.X() >= aCellStart.X() - 8 && rMouse.X() <= aCellStart.X() );
            }
            else
            {
                bCornerHorizontalRight = ( rMouse.X() >= aCellEnd.X() - 8 && rMouse.X() <= aCellEnd.X() );
                bCornerHorizontalLeft  = ( rMouse.X() >= aCellStart.X()   && rMouse.X() <= aCellStart.X() + 8 );
            }

            bool bCornerVerticalDown = rMouse.Y() >= aCellEnd.Y() - 8  && rMouse.Y() <= aCellEnd.Y();
            bool bCornerVerticalUp   = rMouse.Y() >= aCellStart.Y()    && rMouse.Y() <= aCellStart.Y() + 8;

            //  corner is hit only if the mouse is within the cell
            sal_uInt16 nCount = static_cast<sal_uInt16>(pRangeFinder->Count());
            for (sal_uInt16 i=nCount; i;)
            {
                //  search backwards so that the last repainted frame is found
                --i;
                ScRangeFindData& rData = pRangeFinder->GetObject(i);
                if ( rData.aRef.In(aAddr) )
                {
                    if (pIndex)
                        *pIndex = i;
                    if (pAddX)
                        *pAddX = nPosX - rData.aRef.aStart.Col();
                    if (pAddY)
                        *pAddY = nPosY - rData.aRef.aStart.Row();

                    bFound = true;

                    rCorner = NONE;

                    ScAddress aEnd = rData.aRef.aEnd;
                    ScAddress aStart = rData.aRef.aStart;

                    if ( bCornerHorizontalLeft && bCornerVerticalUp &&
                         aAddr == aStart)
                    {
                        rCorner = LEFT_UP;
                    }
                    else if (bCornerHorizontalRight && bCornerVerticalDown &&
                             aAddr == aEnd)
                    {
                        rCorner = RIGHT_DOWN;
                    }
                    else if (bCornerHorizontalRight && bCornerVerticalUp &&
                             aAddr == ScAddress(aEnd.Col(), aStart.Row(), aStart.Tab()))
                    {
                        rCorner = RIGHT_UP;
                    }
                    else if (bCornerHorizontalLeft && bCornerVerticalDown &&
                             aAddr == ScAddress(aStart.Col(), aEnd.Row(), aStart.Tab()))
                    {
                        rCorner = LEFT_DOWN;
                    }
                    break;
                }
            }
        }
    }
    return bFound;
}

#define SCE_TOP     1
#define SCE_BOTTOM  2
#define SCE_LEFT    4
#define SCE_RIGHT   8
#define SCE_ALL     15

static void lcl_PaintOneRange( ScDocShell* pDocSh, const ScRange& rRange, sal_uInt16 nEdges )
{
    // the range is always properly orientated

    SCCOL nCol1 = rRange.aStart.Col();
    SCROW nRow1 = rRange.aStart.Row();
    SCTAB nTab1 = rRange.aStart.Tab();
    SCCOL nCol2 = rRange.aEnd.Col();
    SCROW nRow2 = rRange.aEnd.Row();
    SCTAB nTab2 = rRange.aEnd.Tab();
    bool bHiddenEdge = false;
    SCROW nTmp;

    ScDocument& rDoc = pDocSh->GetDocument();
    while ( nCol1 > 0 && rDoc.ColHidden(nCol1, nTab1) )
    {
        --nCol1;
        bHiddenEdge = true;
    }
    while ( nCol2 < rDoc.MaxCol() && rDoc.ColHidden(nCol2, nTab1) )
    {
        ++nCol2;
        bHiddenEdge = true;
    }
    nTmp = rDoc.FirstVisibleRow(0, nRow1, nTab1);
    if (!ValidRow(nTmp))
        nTmp = 0;
    if (nTmp < nRow1)
    {
        nRow1 = nTmp;
        bHiddenEdge = true;
    }
    nTmp = rDoc.FirstVisibleRow(nRow2, rDoc.MaxRow(), nTab1);
    if (!ValidRow(nTmp))
        nTmp = rDoc.MaxRow();
    if (nTmp > nRow2)
    {
        nRow2 = nTmp;
        bHiddenEdge = true;
    }

    if ( nCol2 > nCol1 + 1 && nRow2 > nRow1 + 1 && !bHiddenEdge )
    {
        // Only along the edges (The corners are hit twice)
        if ( nEdges & SCE_TOP )
            pDocSh->PostPaint( nCol1, nRow1, nTab1, nCol2, nRow1, nTab2, PaintPartFlags::Marks );
        if ( nEdges & SCE_LEFT )
            pDocSh->PostPaint( nCol1, nRow1, nTab1, nCol1, nRow2, nTab2, PaintPartFlags::Marks );
        if ( nEdges & SCE_RIGHT )
            pDocSh->PostPaint( nCol2, nRow1, nTab1, nCol2, nRow2, nTab2, PaintPartFlags::Marks );
        if ( nEdges & SCE_BOTTOM )
            pDocSh->PostPaint( nCol1, nRow2, nTab1, nCol2, nRow2, nTab2, PaintPartFlags::Marks );
    }
    else    // everything in one call
        pDocSh->PostPaint( nCol1, nRow1, nTab1, nCol2, nRow2, nTab2, PaintPartFlags::Marks );
}

static void lcl_PaintRefChanged( ScDocShell* pDocSh, const ScRange& rOldUn, const ScRange& rNewUn )
{
    //  Repaint for the parts of the frame in old, which in are no more in New

    ScRange aOld = rOldUn;
    ScRange aNew = rNewUn;
    aOld.PutInOrder();
    aNew.PutInOrder();

    if ( aOld.aStart == aOld.aEnd )                 //! Ignore sheet ?
        pDocSh->GetDocument().ExtendMerge(aOld);
    if ( aNew.aStart == aNew.aEnd )                 //! Ignore sheet ?
        pDocSh->GetDocument().ExtendMerge(aNew);

    SCCOL nOldCol1 = aOld.aStart.Col();
    SCROW nOldRow1 = aOld.aStart.Row();
    SCCOL nOldCol2 = aOld.aEnd.Col();
    SCROW nOldRow2 = aOld.aEnd.Row();
    SCCOL nNewCol1 = aNew.aStart.Col();
    SCROW nNewRow1 = aNew.aStart.Row();
    SCCOL nNewCol2 = aNew.aEnd.Col();
    SCROW nNewRow2 = aNew.aEnd.Row();
    SCTAB nTab1 = aOld.aStart.Tab();        // sheet is not changed
    SCTAB nTab2 = aOld.aEnd.Tab();

    if ( nNewRow2 < nOldRow1 || nNewRow1 > nOldRow2 ||
         nNewCol2 < nOldCol1 || nNewCol1 > nOldCol2 ||
         ( nNewCol1 != nOldCol1 && nNewRow1 != nOldRow1 &&
           nNewCol2 != nOldCol2 && nNewRow2 != nOldRow2 ) )
    {
        // Completely removed or changed all sides
        // (check <= instead of <  goes wrong for single rows/columns)

        lcl_PaintOneRange( pDocSh, aOld, SCE_ALL );
    }
    else        // Test all four corners separately
    {
        // upper part
        if ( nNewRow1 < nOldRow1 )                  // only delete upper line
            lcl_PaintOneRange( pDocSh, ScRange(
                    nOldCol1, nOldRow1, nTab1, nOldCol2, nOldRow1, nTab2 ), SCE_ALL );
        else if ( nNewRow1 > nOldRow1 )             // the upper part which is will be removed
            lcl_PaintOneRange( pDocSh, ScRange(
                    nOldCol1, nOldRow1, nTab1, nOldCol2, nNewRow1-1, nTab2 ),
                    SCE_ALL &~ SCE_BOTTOM );

        //  bottom part
        if ( nNewRow2 > nOldRow2 )                  // only delete bottom line
            lcl_PaintOneRange( pDocSh, ScRange(
                    nOldCol1, nOldRow2, nTab1, nOldCol2, nOldRow2, nTab2 ), SCE_ALL );
        else if ( nNewRow2 < nOldRow2 )             // the bottom part which is will be removed
            lcl_PaintOneRange( pDocSh, ScRange(
                    nOldCol1, nNewRow2+1, nTab1, nOldCol2, nOldRow2, nTab2 ),
                    SCE_ALL &~ SCE_TOP );

        //  left part
        if ( nNewCol1 < nOldCol1 )                  // only delete left line
            lcl_PaintOneRange( pDocSh, ScRange(
                    nOldCol1, nOldRow1, nTab1, nOldCol1, nOldRow2, nTab2 ), SCE_ALL );
        else if ( nNewCol1 > nOldCol1 )             // the left part which is will be removed
            lcl_PaintOneRange( pDocSh, ScRange(
                    nOldCol1, nOldRow1, nTab1, nNewCol1-1, nOldRow2, nTab2 ),
                    SCE_ALL &~ SCE_RIGHT );

        // right part
        if ( nNewCol2 > nOldCol2 )                  // only delete right line
            lcl_PaintOneRange( pDocSh, ScRange(
                    nOldCol2, nOldRow1, nTab1, nOldCol2, nOldRow2, nTab2 ), SCE_ALL );
        else if ( nNewCol2 < nOldCol2 )             // the right part which is will be removed
            lcl_PaintOneRange( pDocSh, ScRange(
                    nNewCol2+1, nOldRow1, nTab1, nOldCol2, nOldRow2, nTab2 ),
                    SCE_ALL &~ SCE_LEFT );
    }
}

void ScGridWindow::RFMouseMove( const MouseEvent& rMEvt, bool bUp )
{
    ScInputHandler* pHdl = SC_MOD()->GetInputHdl( pViewData->GetViewShell() );
    if (!pHdl)
        return;
    ScRangeFindList* pRangeFinder = pHdl->GetRangeFindList();
    if (!pRangeFinder || nRFIndex >= pRangeFinder->Count())
        return;
    ScRangeFindData& rData = pRangeFinder->GetObject( nRFIndex );

    // Mouse pointer

    if (bRFSize)
        SetPointer( PointerStyle::Cross );
    else
        SetPointer( PointerStyle::Hand );

    //  Scrolling

    bool bTimer = false;
    Point aPos = rMEvt.GetPosPixel();
    SCCOL nDx = 0;
    SCROW nDy = 0;
    if ( aPos.X() < 0 ) nDx = -1;
    if ( aPos.Y() < 0 ) nDy = -1;
    Size aSize = GetOutputSizePixel();
    if ( aPos.X() >= aSize.Width() )
        nDx = 1;
    if ( aPos.Y() >= aSize.Height() )
        nDy = 1;
    if ( nDx != 0 || nDy != 0 )
    {
        if ( nDx != 0) pViewData->GetView()->ScrollX( nDx, WhichH(eWhich) );
        if ( nDy != 0 ) pViewData->GetView()->ScrollY( nDy, WhichV(eWhich) );
        bTimer = true;
    }

    // Switching when fixating (so Scrolling works)

    if ( eWhich == pViewData->GetActivePart() )     //??
    {
        if ( pViewData->GetHSplitMode() == SC_SPLIT_FIX )
            if ( nDx > 0 )
            {
                if ( eWhich == SC_SPLIT_TOPLEFT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_TOPRIGHT );
                else if ( eWhich == SC_SPLIT_BOTTOMLEFT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_BOTTOMRIGHT );
            }

        if ( pViewData->GetVSplitMode() == SC_SPLIT_FIX )
            if ( nDy > 0 )
            {
                if ( eWhich == SC_SPLIT_TOPLEFT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_BOTTOMLEFT );
                else if ( eWhich == SC_SPLIT_TOPRIGHT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_BOTTOMRIGHT );
            }
    }

    // Move

    SCCOL  nPosX;
    SCROW  nPosY;
    pViewData->GetPosFromPixel( aPos.X(), aPos.Y(), eWhich, nPosX, nPosY );

    ScRange aOld = rData.aRef;
    ScRange aNew = aOld;
    if ( bRFSize )
    {
        switch (aRFSelectedCorned)
        {
            case LEFT_UP:
                aNew.aStart.SetCol(nPosX);
                aNew.aStart.SetRow(nPosY);
                break;
            case LEFT_DOWN:
                aNew.aStart.SetCol(nPosX);
                aNew.aEnd.SetRow(nPosY);
                break;
            case RIGHT_UP:
                aNew.aEnd.SetCol(nPosX);
                aNew.aStart.SetRow(nPosY);
                break;
            case RIGHT_DOWN:
                aNew.aEnd.SetCol(nPosX);
                aNew.aEnd.SetRow(nPosY);
                break;
            default:
                break;
        }
    }
    else
    {
        ScDocument* pDoc = pViewData->GetDocument();
        long nStartX = nPosX - nRFAddX;
        if ( nStartX < 0 ) nStartX = 0;
        long nStartY = nPosY - nRFAddY;
        if ( nStartY < 0 ) nStartY = 0;
        long nEndX = nStartX + aOld.aEnd.Col() - aOld.aStart.Col();
        if ( nEndX > pDoc->MaxCol() )
        {
            nStartX -= ( nEndX - pDoc->MaxRow() );
            nEndX = pDoc->MaxCol();
        }
        long nEndY = nStartY + aOld.aEnd.Row() - aOld.aStart.Row();
        if ( nEndY > pDoc->MaxRow() )
        {
            nStartY -= ( nEndY - pDoc->MaxRow() );
            nEndY = pDoc->MaxRow();
        }

        aNew.aStart.SetCol(static_cast<SCCOL>(nStartX));
        aNew.aStart.SetRow(static_cast<SCROW>(nStartY));
        aNew.aEnd.SetCol(static_cast<SCCOL>(nEndX));
        aNew.aEnd.SetRow(static_cast<SCROW>(nEndY));
    }

    if ( bUp )
        aNew.PutInOrder();             // For ButtonUp again in the proper order

    if ( aNew != aOld )
    {
        pHdl->UpdateRange( nRFIndex, aNew );

        ScDocShell* pDocSh = pViewData->GetDocShell();

        pHdl->UpdateLokReferenceMarks();

        // only redrawing what has been changed...
        lcl_PaintRefChanged( pDocSh, aOld, aNew );

        // only redraw new frame (synchronously)
        pDocSh->Broadcast( ScIndexHint( SfxHintId::ScShowRangeFinder, nRFIndex ) );

        Update();   // what you move, will be seen immediately
    }

    //  Timer for Scrolling

    if (bTimer)
        pViewData->GetView()->SetTimer( this, rMEvt );          // repeat event
    else
        pViewData->GetView()->ResetTimer();
}

namespace {

SvxAdjust toSvxAdjust( const ScPatternAttr& rPat )
{
    SvxCellHorJustify eHorJust =
            rPat.GetItem(ATTR_HOR_JUSTIFY).GetValue();

    SvxAdjust eSvxAdjust = SvxAdjust::Left;
    switch (eHorJust)
    {
        case SvxCellHorJustify::Left:
        case SvxCellHorJustify::Repeat:            // not implemented
        case SvxCellHorJustify::Standard:          // always Text if an EditCell type
                eSvxAdjust = SvxAdjust::Left;
                break;
        case SvxCellHorJustify::Right:
                eSvxAdjust = SvxAdjust::Right;
                break;
        case SvxCellHorJustify::Center:
                eSvxAdjust = SvxAdjust::Center;
                break;
        case SvxCellHorJustify::Block:
                eSvxAdjust = SvxAdjust::Block;
                break;
    }

    return eSvxAdjust;
}

std::shared_ptr<ScFieldEditEngine> createEditEngine( ScDocShell* pDocSh, const ScPatternAttr& rPat )
{
    ScDocument& rDoc = pDocSh->GetDocument();

    std::shared_ptr<ScFieldEditEngine> pEngine(new ScFieldEditEngine(&rDoc, rDoc.GetEditPool()));
    ScSizeDeviceProvider aProv(pDocSh);
    pEngine->SetRefDevice(aProv.GetDevice());
    pEngine->SetRefMapMode(MapMode(MapUnit::Map100thMM));
    SfxItemSet aDefault = pEngine->GetEmptyItemSet();
    rPat.FillEditItemSet(&aDefault);
    aDefault.Put( SvxAdjustItem(toSvxAdjust(rPat), EE_PARA_JUST) );
    pEngine->SetDefaults(aDefault);

    return pEngine;
}

bool extractURLInfo( const SvxFieldItem* pFieldItem, OUString* pName, OUString* pUrl, OUString* pTarget )
{
    if (!pFieldItem)
        return false;

    const SvxFieldData* pField = pFieldItem->GetField();
    if (pField->GetClassId() != text::textfield::Type::URL)
        return false;

    const SvxURLField* pURLField = static_cast<const SvxURLField*>(pField);

    if (pName)
        *pName = pURLField->GetRepresentation();
    if (pUrl)
        *pUrl = pURLField->GetURL();
    if (pTarget)
        *pTarget = pURLField->GetTargetFrame();

    return true;
}

}

bool ScGridWindow::GetEditUrl( const Point& rPos,
                               OUString* pName, OUString* pUrl, OUString* pTarget )
{
    ScTabViewShell* pViewSh = pViewData->GetViewShell();
    ScInputHandler* pInputHdl = nullptr;
    if (pViewSh)
        pInputHdl = pViewSh->GetInputHandler();
    EditView* pView = (pInputHdl && pInputHdl->IsInputMode()) ? pInputHdl->GetTableView() : nullptr;
    if (pView)
        return extractURLInfo(pView->GetFieldUnderMousePointer(), pName, pUrl, pTarget);

    //! Pass on nPosX/Y?
    SCCOL nPosX;
    SCROW nPosY;
    pViewData->GetPosFromPixel( rPos.X(), rPos.Y(), eWhich, nPosX, nPosY );

    SCTAB nTab = pViewData->GetTabNo();
    ScDocShell* pDocSh = pViewData->GetDocShell();
    ScDocument& rDoc = pDocSh->GetDocument();
    OUString sURL;
    ScRefCellValue aCell;
    bool bFound = lcl_GetHyperlinkCell(&rDoc, nPosX, nPosY, nTab, aCell, sURL);
    if( !bFound )
        return false;

    const ScPatternAttr* pPattern = rDoc.GetPattern( nPosX, nPosY, nTab );
    // bForceToTop = sal_False, use the cell's real position
    tools::Rectangle aEditRect = pViewData->GetEditArea( eWhich, nPosX, nPosY, this, pPattern, false );
    if (rPos.Y() < aEditRect.Top())
        return false;

        // vertical can not (yet) be clicked:

    if (pPattern->GetCellOrientation() != SvxCellOrientation::Standard)
        return false;

    bool bBreak = pPattern->GetItem(ATTR_LINEBREAK).GetValue() ||
                    (pPattern->GetItem( ATTR_HOR_JUSTIFY ).GetValue() == SvxCellHorJustify::Block);
    SvxCellHorJustify eHorJust = pPattern->GetItem(ATTR_HOR_JUSTIFY).GetValue();

        //  EditEngine

    std::shared_ptr<ScFieldEditEngine> pEngine = createEditEngine(pDocSh, *pPattern);

    MapMode aEditMode = pViewData->GetLogicMode(eWhich);            // without draw scaling
    tools::Rectangle aLogicEdit = PixelToLogic( aEditRect, aEditMode );
    long nThisColLogic = aLogicEdit.Right() - aLogicEdit.Left() + 1;
    Size aPaperSize( 1000000, 1000000 );
    if (aCell.meType == CELLTYPE_FORMULA)
    {
        long nSizeX  = 0;
        long nSizeY  = 0;
        pViewData->GetMergeSizePixel( nPosX, nPosY, nSizeX, nSizeY );
        aPaperSize = Size(nSizeX, nSizeY );
        aPaperSize = PixelToLogic(aPaperSize);
    }

    if (bBreak)
        aPaperSize.setWidth( nThisColLogic );
    pEngine->SetPaperSize( aPaperSize );

    std::unique_ptr<EditTextObject> pTextObj;
    if (aCell.meType == CELLTYPE_EDIT)
    {
        if (aCell.mpEditText)
            pEngine->SetText(*aCell.mpEditText);
    }
    else  // Not an Edit cell and is a formula cell with 'Hyperlink'
          // function if we have no URL, otherwise it could be a formula
          // cell ( or other type ? ) with a hyperlink associated with it.
    {
        if (sURL.isEmpty())
            pTextObj = aCell.mpFormula->CreateURLObject();
        else
        {
            OUString aRepres = sURL;

            // TODO: text content of formatted numbers can be different
            if (aCell.hasNumeric())
                aRepres = OUString::number(aCell.getValue());
            else if (aCell.meType == CELLTYPE_FORMULA)
                aRepres = aCell.mpFormula->GetString().getString();

            pTextObj = ScEditUtil::CreateURLObjectFromURL(rDoc, sURL, aRepres);
        }

        if (pTextObj)
            pEngine->SetText(*pTextObj);
    }

    long nStartX = aLogicEdit.Left();

    long nTextWidth = pEngine->CalcTextWidth();
    long nTextHeight = pEngine->GetTextHeight();
    if ( nTextWidth < nThisColLogic )
    {
        if (eHorJust == SvxCellHorJustify::Right)
            nStartX += nThisColLogic - nTextWidth;
        else if (eHorJust == SvxCellHorJustify::Center)
            nStartX += (nThisColLogic - nTextWidth) / 2;
    }

    aLogicEdit.SetLeft( nStartX );
    if (!bBreak)
        aLogicEdit.SetRight( nStartX + nTextWidth );

    // There is one glitch when dealing with a hyperlink cell and
    // the cell content is NUMERIC. This defaults to right aligned and
    // we need to adjust accordingly.
    if (aCell.hasNumeric() && eHorJust == SvxCellHorJustify::Standard)
    {
        aLogicEdit.SetRight( aLogicEdit.Left() + nThisColLogic - 1 );
        aLogicEdit.SetLeft(  aLogicEdit.Right() - nTextWidth );
    }
    aLogicEdit.SetBottom( aLogicEdit.Top() + nTextHeight );

    Point aLogicClick = PixelToLogic(rPos,aEditMode);
    if ( aLogicEdit.IsInside(aLogicClick) )
    {
        EditView aTempView(pEngine.get(), this);
        aTempView.SetOutputArea( aLogicEdit );

        bool bRet;
        if (comphelper::LibreOfficeKit::isActive())
        {
            bRet = extractURLInfo(aTempView.GetField(aLogicClick), pName, pUrl, pTarget);
        }
        else
        {
            MapMode aOld = GetMapMode();
            SetMapMode(aEditMode);                  // no return anymore
            bRet = extractURLInfo(aTempView.GetFieldUnderMousePointer(), pName, pUrl, pTarget);
            SetMapMode(aOld);
        }
        return bRet;
    }
    return false;
}

bool ScGridWindow::IsSpellErrorAtPos( const Point& rPos, SCCOL nCol1, SCROW nRow )
{
    if (!mpSpellCheckCxt)
        return false;

    SCTAB nTab = pViewData->GetTabNo();
    ScDocShell* pDocSh = pViewData->GetDocShell();
    ScDocument& rDoc = pDocSh->GetDocument();

    ScAddress aCellPos(nCol1, nRow, nTab);
    ScRefCellValue aCell(rDoc, aCellPos);
    if (aCell.meType != CELLTYPE_STRING && aCell.meType != CELLTYPE_EDIT)
        return false;

    const std::vector<editeng::MisspellRanges>* pRanges = mpSpellCheckCxt->getMisspellRanges(nCol1, nRow);
    if (!pRanges)
        return false;

    const ScPatternAttr* pPattern = rDoc.GetPattern(nCol1, nRow, nTab);

    tools::Rectangle aEditRect = pViewData->GetEditArea(eWhich, nCol1, nRow, this, pPattern, false);
    if (rPos.Y() < aEditRect.Top())
        return false;

    std::shared_ptr<ScFieldEditEngine> pEngine = createEditEngine(pDocSh, *pPattern);

    Size aPaperSize(1000000, 1000000);
    pEngine->SetPaperSize(aPaperSize);

    if (aCell.meType == CELLTYPE_EDIT)
        pEngine->SetText(*aCell.mpEditText);
    else
        pEngine->SetText(aCell.mpString->getString());

    long nTextWidth = static_cast<long>(pEngine->CalcTextWidth());

    MapMode aEditMode = pViewData->GetLogicMode(eWhich);
    tools::Rectangle aLogicEdit = PixelToLogic(aEditRect, aEditMode);
    Point aLogicClick = PixelToLogic(rPos, aEditMode);

    aLogicEdit.setWidth(nTextWidth + 1);

    if (!aLogicEdit.IsInside(aLogicClick))
        return false;

    pEngine->SetControlWord(pEngine->GetControlWord() | EEControlBits::ONLINESPELLING);
    pEngine->SetAllMisspellRanges(*pRanges);

    EditView aTempView(pEngine.get(), this);
    aTempView.SetOutputArea(aLogicEdit);

    return aTempView.IsWrongSpelledWordAtPos(rPos);
}

bool ScGridWindow::HasScenarioButton( const Point& rPosPixel, ScRange& rScenRange )
{
    ScDocument* pDoc = pViewData->GetDocument();
    SCTAB nTab = pViewData->GetTabNo();
    SCTAB nTabCount = pDoc->GetTableCount();
    if ( nTab+1<nTabCount && pDoc->IsScenario(nTab+1) && !pDoc->IsScenario(nTab) )
    {
        bool bLayoutRTL = pDoc->IsLayoutRTL( nTab );

        Size aButSize = pViewData->GetScenButSize();
        long nBWidth  = aButSize.Width();
        if (!nBWidth)
            return false;                   // No Button drawn yet -> there is none
        long nBHeight = aButSize.Height();
        long nHSpace  = static_cast<long>( SC_SCENARIO_HSPACE * pViewData->GetPPTX() );

        //! cache the Ranges in Table!!!!

        ScMarkData aMarks(pDoc->MaxRow(), pDoc->MaxCol());
        for (SCTAB i=nTab+1; i<nTabCount && pDoc->IsScenario(i); i++)
            pDoc->MarkScenario( i, nTab, aMarks, false, ScScenarioFlags::ShowFrame );
        ScRangeList aRanges;
        aMarks.FillRangeListWithMarks( &aRanges, false );

        size_t nRangeCount = aRanges.size();
        for (size_t j=0;  j< nRangeCount; ++j)
        {
            ScRange aRange = aRanges[j];
            // Always extend scenario frame to merged cells where no new non-covered cells
            // are framed
            pDoc->ExtendTotalMerge( aRange );

            bool bTextBelow = ( aRange.aStart.Row() == 0 );

            Point aButtonPos;
            if ( bTextBelow )
            {
                aButtonPos = pViewData->GetScrPos( aRange.aEnd.Col()+1, aRange.aEnd.Row()+1,
                                                    eWhich, true );
            }
            else
            {
                aButtonPos = pViewData->GetScrPos( aRange.aEnd.Col()+1, aRange.aStart.Row(),
                                                    eWhich, true );
                aButtonPos.AdjustY( -nBHeight );
            }
            if ( bLayoutRTL )
                aButtonPos.AdjustX( -(nHSpace - 1) );
            else
                aButtonPos.AdjustX( -(nBWidth - nHSpace) );    // same for top or bottom

            tools::Rectangle aButRect( aButtonPos, Size(nBWidth,nBHeight) );
            if ( aButRect.IsInside( rPosPixel ) )
            {
                rScenRange = aRange;
                return true;
            }
        }
    }

    return false;
}

void ScGridWindow::DrawLayerCreated()
{
    SetMapMode( GetDrawMapMode() );

    // initially create overlay objects
    ImpCreateOverlayObjects();
}

namespace {

struct SpellCheckStatus
{
    bool mbModified;

    SpellCheckStatus() : mbModified(false) {};

    DECL_LINK( EventHdl, EditStatus&, void );
};

IMPL_LINK(SpellCheckStatus, EventHdl, EditStatus&, rStatus, void)
{
    EditStatusFlags nStatus = rStatus.GetStatusWord();
    if (nStatus & EditStatusFlags::WRONGWORDCHANGED)
        mbModified = true;
}

}

bool ScGridWindow::ContinueOnlineSpelling()
{
    if (!mpSpellCheckCxt)
        return false;

    if (!mpSpellCheckCxt->maPos.isValid())
        return false;

    ScDocument* pDoc = pViewData->GetDocument();
    ScDPCollection* pDPs = nullptr;
    if (pDoc->HasPivotTable())
        pDPs = pDoc->GetDPCollection();

    SCTAB nTab = pViewData->GetTabNo();
    SpellCheckStatus aStatus;

    ScHorizontalCellIterator aIter(
        pDoc, nTab, maVisibleRange.mnCol1, mpSpellCheckCxt->maPos.mnRow, maVisibleRange.mnCol2, maVisibleRange.mnRow2);

    ScRangeList aPivotRanges = pDPs ? pDPs->GetAllTableRanges(nTab) : ScRangeList();

    SCCOL nCol;
    SCROW nRow;
    ScRefCellValue* pCell = aIter.GetNext(nCol, nRow);
    SCROW nEndRow = 0;
    bool bHidden = pCell && pDoc->RowHidden(nRow, nTab, nullptr, &nEndRow);
    bool bSkip = pCell && (nRow < mpSpellCheckCxt->maPos.mnRow || bHidden);
    while (bSkip)
    {
        pCell = aIter.GetNext(nCol, nRow);
        if (pCell && nRow > nEndRow)
        {
            bHidden = pDoc->RowHidden(nRow, nTab, nullptr, &nEndRow);
        }
        bSkip = pCell && (nRow < mpSpellCheckCxt->maPos.mnRow || bHidden);
    }

    SCCOL nEndCol = 0;
    bHidden = pCell && pDoc->ColHidden(nCol, nTab, nullptr, &nEndCol);
    bSkip = pCell && (nCol < mpSpellCheckCxt->maPos.mnCol || bHidden);
    while (bSkip)
    {
        pCell = aIter.GetNext(nCol, nRow);
        if (pCell && nCol > nEndCol)
        {
            bHidden = pDoc->ColHidden(nCol, nTab, nullptr, &nEndCol);
        }
        bSkip = pCell && (nCol < mpSpellCheckCxt->maPos.mnCol || bHidden);
    }

    std::unique_ptr<ScTabEditEngine> pEngine;

    // Check only up to 256 cells at a time.
    size_t nTotalCellCount = 0;
    size_t nTextCellCount = 0;
    bool bSpellCheckPerformed = false;

    while (pCell)
    {
        ++nTotalCellCount;

        if (aPivotRanges.In(ScAddress(nCol, nRow, nTab)))
        {
            // Don't spell check within pivot tables.
            if (nTotalCellCount >= 255)
                break;

            pCell = aIter.GetNext(nCol, nRow);
            continue;
        }

        CellType eType = pCell->meType;
        if (eType == CELLTYPE_STRING || eType == CELLTYPE_EDIT)
        {
            ++nTextCellCount;

            // NB: For spell-checking, we currently only use the primary
            // language; not CJK nor CTL.
            const ScPatternAttr* pPattern = pDoc->GetPattern(nCol, nRow, nTab);
            LanguageType nCellLang = pPattern->GetItem(ATTR_FONT_LANGUAGE).GetValue();

            if (nCellLang == LANGUAGE_SYSTEM)
                nCellLang = Application::GetSettings().GetLanguageTag().getLanguageType();   // never use SYSTEM for spelling

            if (nCellLang == LANGUAGE_NONE)
            {
                // No need to spell check this cell.
                pCell = aIter.GetNext(nCol, nRow);
                continue;
            }

            if (!pEngine)
            {
                //  ScTabEditEngine is needed
                //  because MapMode must be set for some old documents
                pEngine.reset(new ScTabEditEngine(pDoc));
                pEngine->SetControlWord(
                    pEngine->GetControlWord() | (EEControlBits::ONLINESPELLING | EEControlBits::ALLOWBIGOBJS));
                pEngine->SetStatusEventHdl(LINK(&aStatus, SpellCheckStatus, EventHdl));
                //  Delimiters here like in inputhdl.cxx !!!
                pEngine->SetWordDelimiters(
                            ScEditUtil::ModifyDelimiters(pEngine->GetWordDelimiters()));

                uno::Reference<linguistic2::XSpellChecker1> xXSpellChecker1(LinguMgr::GetSpellChecker());
                pEngine->SetSpeller(xXSpellChecker1);
                pEngine->SetDefaultLanguage(ScGlobal::GetEditDefaultLanguage());
            }

            pEngine->SetDefaultItem(SvxLanguageItem(nCellLang, EE_CHAR_LANGUAGE));

            if (eType == CELLTYPE_STRING)
                pEngine->SetText(pCell->mpString->getString());
            else
                pEngine->SetText(*pCell->mpEditText);

            aStatus.mbModified = false;
            pEngine->CompleteOnlineSpelling();
            if (aStatus.mbModified)
            {
                std::vector<editeng::MisspellRanges> aRanges;
                pEngine->GetAllMisspellRanges(aRanges);
                if (!aRanges.empty())
                {
                    sc::SpellCheckContext::CellPos aPos(nCol, nRow);
                    mpSpellCheckCxt->maMisspellCells.emplace(aPos, aRanges);
                }

                // Broadcast for re-paint.
                ScPaintHint aHint(ScRange(nCol, nRow, nTab), PaintPartFlags::Grid);
                aHint.SetPrintFlag(false);
                pDoc->GetDocumentShell()->Broadcast(aHint);
            }

            bSpellCheckPerformed = true;
        }

        if (nTotalCellCount >= 255 || nTextCellCount >= 1)
            break;

        pCell = aIter.GetNext(nCol, nRow);
    }

    if (pCell)
        // Move to the next cell position for the next iteration.
        pCell = aIter.GetNext(nCol, nRow);

    if (pCell)
    {
        // This will become the first cell position for the next time.
        mpSpellCheckCxt->maPos.mnCol = nCol;
        mpSpellCheckCxt->maPos.mnRow = nRow;
    }
    else
    {
        // No more cells to spell check.
        mpSpellCheckCxt->maPos.setInvalid();
    }

    return bSpellCheckPerformed;
}

void ScGridWindow::EnableAutoSpell( bool bEnable )
{
    if (bEnable)
        mpSpellCheckCxt.reset(new sc::SpellCheckContext);
    else
        mpSpellCheckCxt.reset();
}

void ScGridWindow::ResetAutoSpell()
{
    if (mpSpellCheckCxt)
    {
        mpSpellCheckCxt->reset();
        mpSpellCheckCxt->maPos.mnCol = maVisibleRange.mnCol1;
        mpSpellCheckCxt->maPos.mnRow = maVisibleRange.mnRow1;
    }
}

void ScGridWindow::SetAutoSpellData( SCCOL nPosX, SCROW nPosY, const std::vector<editeng::MisspellRanges>* pRanges )
{
    if (!mpSpellCheckCxt)
        return;

    if (!maVisibleRange.isInside(nPosX, nPosY))
        return;

    mpSpellCheckCxt->setMisspellRanges(nPosX, nPosY, pRanges);
}

const std::vector<editeng::MisspellRanges>* ScGridWindow::GetAutoSpellData( SCCOL nPosX, SCROW nPosY )
{
    if (!mpSpellCheckCxt)
        return nullptr;

    if (!maVisibleRange.isInside(nPosX, nPosY))
        return nullptr;

    return mpSpellCheckCxt->getMisspellRanges(nPosX, nPosY);
}

bool ScGridWindow::InsideVisibleRange( SCCOL nPosX, SCROW nPosY )
{
    return maVisibleRange.isInside(nPosX, nPosY);
}

// Use the same zoom calculations as in paintTile - this
// means the client can ensure they can get the correct
// cursor corresponding to their current tile sizings.
OString ScGridWindow::getCellCursor( int nOutputWidth, int nOutputHeight,
                                     long nTileWidth, long nTileHeight )
{
    Fraction zoomX(long(nOutputWidth * TWIPS_PER_PIXEL), nTileWidth);
    Fraction zoomY(long(nOutputHeight * TWIPS_PER_PIXEL), nTileHeight);
    return getCellCursor(zoomX, zoomY);
}

OString ScGridWindow::getCellCursor(const Fraction& rZoomX, const Fraction& rZoomY) const
{
    // GridWindow stores a shown cell cursor in mpOOCursors, hence
    // we can use that to determine whether we would want to be showing
    // one (client-side) for tiled rendering too.
    if (!mpOOCursors)
    {
        return "EMPTY";
    }

    SCCOL nX = pViewData->GetCurX();
    SCROW nY = pViewData->GetCurY();

    Fraction defaultZoomX = pViewData->GetZoomX();
    Fraction defaultZoomY = pViewData->GetZoomY();

    pViewData->SetZoom(rZoomX, rZoomY, true);

    Point aScrPos = pViewData->GetScrPos( nX, nY, eWhich, true );
    long nSizeXPix;
    long nSizeYPix;
    pViewData->GetMergeSizePixel( nX, nY, nSizeXPix, nSizeYPix );

    double fPPTX = pViewData->GetPPTX();
    double fPPTY = pViewData->GetPPTY();

    // make it a slim cell cursor, but not empty
    if (nSizeXPix == 0)
        nSizeXPix = 1;

    if (nSizeYPix == 0)
        nSizeYPix = 1;

    long nPosXTw = rtl::math::round(aScrPos.getX() / fPPTX);
    long nPosYTw = rtl::math::round(aScrPos.getY() / fPPTY);
    // look at Rectangle( const Point& rLT, const Size& rSize ) for the '- 1'
    long nSizeXTw = rtl::math::round(nSizeXPix / fPPTX) - 1;
    long nSizeYTw = rtl::math::round(nSizeYPix / fPPTY) - 1;

    std::stringstream ss;
    ss << nPosXTw << ", " << nPosYTw << ", " << nSizeXTw << ", " << nSizeYTw << ", "
       << nX << ", " << nY;

    pViewData->SetZoom(defaultZoomX, defaultZoomY, true);

    pViewData->GetLOKWidthHelper().removeByIndex(pViewData->GetLOKOldCurX() - 1);
    pViewData->GetLOKWidthHelper().insert(nX - 1, aScrPos.getX());
    pViewData->SetLOKOldCurX(nX);
    pViewData->GetLOKHeightHelper().removeByIndex(pViewData->GetLOKOldCurY() - 1);
    pViewData->GetLOKHeightHelper().insert(nY - 1, aScrPos.getY());
    pViewData->SetLOKOldCurY(nY);

    return ss.str().c_str();
}

void ScGridWindow::updateLibreOfficeKitCellCursor(const SfxViewShell* pOtherShell) const
{
    OString aCursor = getCellCursor(pViewData->GetZoomX(), pViewData->GetZoomY());
    ScTabViewShell* pViewShell = pViewData->GetViewShell();
    if (pOtherShell)
    {
        if (pOtherShell == pViewShell)
        {
            pViewShell->libreOfficeKitViewCallback(LOK_CALLBACK_CELL_CURSOR, aCursor.getStr());

            if (bListValButton && aListValPos == pViewData->GetCurPos())
                updateLOKValListButton(true, aListValPos);
        }
        else
        {
            SfxLokHelper::notifyOtherView(pViewShell, pOtherShell, LOK_CALLBACK_CELL_VIEW_CURSOR, "rectangle", aCursor);
        }
    }
    else
    {
        pViewShell->libreOfficeKitViewCallback(LOK_CALLBACK_CELL_CURSOR, aCursor.getStr());
        SfxLokHelper::notifyOtherViews(pViewShell, LOK_CALLBACK_CELL_VIEW_CURSOR, "rectangle", aCursor);
    }
}

void ScGridWindow::CursorChanged()
{
    // here the created OverlayObjects may be transformed in later versions. For
    // now, just re-create them

    UpdateCursorOverlay();
}

void ScGridWindow::ImpCreateOverlayObjects()
{
    UpdateCursorOverlay();
    UpdateCopySourceOverlay();
    UpdateSelectionOverlay();
    UpdateAutoFillOverlay();
    UpdateDragRectOverlay();
    UpdateHeaderOverlay();
    UpdateShrinkOverlay();
}

void ScGridWindow::ImpDestroyOverlayObjects()
{
    DeleteCursorOverlay();
    DeleteCopySourceOverlay();
    DeleteSelectionOverlay();
    DeleteAutoFillOverlay();
    DeleteDragRectOverlay();
    DeleteHeaderOverlay();
    DeleteShrinkOverlay();
}

void ScGridWindow::UpdateAllOverlays()
{
    // delete and re-allocate all overlay objects

    ImpDestroyOverlayObjects();
    ImpCreateOverlayObjects();
}

void ScGridWindow::DeleteCursorOverlay()
{
    ScTabViewShell* pViewShell = pViewData->GetViewShell();
    pViewShell->libreOfficeKitViewCallback(LOK_CALLBACK_CELL_CURSOR, "EMPTY");
    SfxLokHelper::notifyOtherViews(pViewShell, LOK_CALLBACK_CELL_VIEW_CURSOR, "rectangle", "EMPTY");
    mpOOCursors.reset();
}

void ScGridWindow::DeleteCopySourceOverlay()
{
    mpOOSelectionBorder.reset();
}

void ScGridWindow::UpdateCopySourceOverlay()
{
    MapMode aDrawMode = GetDrawMapMode();
    MapMode aOldMode = GetMapMode();
    if ( aOldMode != aDrawMode )
        SetMapMode( aDrawMode );

    DeleteCopySourceOverlay();

    if (comphelper::LibreOfficeKit::isActive())
        return;
    if (!pViewData->ShowPasteSource())
        return;
    rtl::Reference<sdr::overlay::OverlayManager> xOverlayManager = getOverlayManager();
    if (!xOverlayManager.is())
        return;
    const ScTransferObj* pTransObj = ScTransferObj::GetOwnClipboard(ScTabViewShell::GetClipData(pViewData->GetActiveWin()));
    if (!pTransObj)
        return;
    ScDocument* pClipDoc = pTransObj->GetDocument();
    if (!pClipDoc)
        return;

    SCTAB nCurTab = pViewData->GetCurPos().Tab();

    ScClipParam& rClipParam = pClipDoc->GetClipParam();
    mpOOSelectionBorder.reset(new sdr::overlay::OverlayObjectList);
    for ( size_t i = 0; i < rClipParam.maRanges.size(); ++i )
    {
        ScRange const & r = rClipParam.maRanges[i];
        if (r.aStart.Tab() != nCurTab)
            continue;

        SCCOL nClipStartX = r.aStart.Col();
        SCROW nClipStartY = r.aStart.Row();
        SCCOL nClipEndX   = r.aEnd.Col();
        SCROW nClipEndY   = r.aEnd.Row();

        Point aClipStartScrPos = pViewData->GetScrPos( nClipStartX, nClipStartY, eWhich );
        Point aClipEndScrPos   = pViewData->GetScrPos( nClipEndX + 1, nClipEndY + 1, eWhich );
        aClipStartScrPos -= Point(1, 1);
        long nSizeXPix = aClipEndScrPos.X() - aClipStartScrPos.X();
        long nSizeYPix = aClipEndScrPos.Y() - aClipStartScrPos.Y();

        tools::Rectangle aRect( aClipStartScrPos, Size(nSizeXPix, nSizeYPix) );

        Color aHighlight = GetSettings().GetStyleSettings().GetHighlightColor();

        tools::Rectangle aLogic = PixelToLogic(aRect, aDrawMode);
        ::basegfx::B2DRange aRange = vcl::unotools::b2DRectangleFromRectangle(aLogic);
        std::unique_ptr<ScOverlayDashedBorder> pDashedBorder(new ScOverlayDashedBorder(aRange, aHighlight));
        xOverlayManager->add(*pDashedBorder);
        mpOOSelectionBorder->append(std::move(pDashedBorder));
    }

    if ( aOldMode != aDrawMode )
        SetMapMode( aOldMode );
}

/**
 * Turn the selection ranges rRectangles into the LibreOfficeKit selection, and call the callback.
 *
 * @param pLogicRects - if not 0, then don't invoke the callback, just collect the rectangles in the pointed vector.
 */
static void updateLibreOfficeKitSelection(const ScViewData* pViewData, const std::vector<tools::Rectangle>& rRectangles, std::vector<tools::Rectangle>* pLogicRects = nullptr)
{
    if (!comphelper::LibreOfficeKit::isActive())
        return;

    double nPPTX = pViewData->GetPPTX();
    double nPPTY = pViewData->GetPPTY();

    tools::Rectangle aBoundingBox;
    std::vector<OString> aRectangles;

    for (const auto& rRectangle : rRectangles)
    {
        // We explicitly create a copy, since we need to expand
        // the rectangle before coordinate conversion
        tools::Rectangle aRectangle(rRectangle);
        aRectangle.AdjustRight(1 );
        aRectangle.AdjustBottom(1 );

        aBoundingBox.Union(aRectangle);

        tools::Rectangle aRect(aRectangle.Left() / nPPTX, aRectangle.Top() / nPPTY,
                aRectangle.Right() / nPPTX, aRectangle.Bottom() / nPPTY);
        if (pLogicRects)
            pLogicRects->push_back(aRect);
        else
            aRectangles.push_back(aRect.toString());
    }

    if (pLogicRects)
        return;

    // selection start handle
    tools::Rectangle aRectangle(
            aBoundingBox.Left()  / nPPTX, aBoundingBox.Top() / nPPTY,
            aBoundingBox.Right() / nPPTX, aBoundingBox.Bottom() / nPPTY);

    // the selection itself
    OString aSelection = comphelper::string::join("; ", aRectangles).getStr();

    ScTabViewShell* pViewShell = pViewData->GetViewShell();
    pViewShell->libreOfficeKitViewCallback(LOK_CALLBACK_CELL_SELECTION_AREA, aRectangle.toString().getStr());
    pViewShell->libreOfficeKitViewCallback(LOK_CALLBACK_TEXT_SELECTION, aSelection.getStr());
    SfxLokHelper::notifyOtherViews(pViewShell, LOK_CALLBACK_TEXT_VIEW_SELECTION, "selection", aSelection.getStr());
}

namespace
{

void updateLibreOfficeKitAutoFill(const ScViewData* pViewData, tools::Rectangle const & rRectangle)
{
    if (!comphelper::LibreOfficeKit::isActive())
        return;

    double nPPTX = pViewData->GetPPTX();
    double nPPTY = pViewData->GetPPTY();

    // selection start handle
    tools::Rectangle aLogicRectangle(
            rRectangle.Left()  / nPPTX, rRectangle.Top() / nPPTY,
            rRectangle.Right() / nPPTX, rRectangle.Bottom() / nPPTY);

    ScTabViewShell* pViewShell = pViewData->GetViewShell();
    pViewShell->libreOfficeKitViewCallback(LOK_CALLBACK_CELL_AUTO_FILL_AREA, aLogicRectangle.toString().getStr());
}

} //end anonymous namespace

void ScGridWindow::UpdateCursorOverlay()
{
    ScDocument* pDoc = pViewData->GetDocument();

    MapMode aDrawMode = GetDrawMapMode();
    MapMode aOldMode = GetMapMode();
    if ( aOldMode != aDrawMode )
        SetMapMode( aDrawMode );

    // Existing OverlayObjects may be transformed in later versions.
    // For now, just re-create them.

    DeleteCursorOverlay();

    std::vector<tools::Rectangle> aPixelRects;

    //  determine the cursor rectangles in pixels (moved from ScGridWindow::DrawCursor)

    SCTAB nTab = pViewData->GetTabNo();
    SCCOL nX = pViewData->GetCurX();
    SCROW nY = pViewData->GetCurY();

    const ScPatternAttr* pPattern = pDoc->GetPattern(nX,nY,nTab);

    if (!comphelper::LibreOfficeKit::isActive() && !maVisibleRange.isInside(nX, nY))
    {
        if (maVisibleRange.mnCol2 < nX || maVisibleRange.mnRow2 < nY)
            return;     // no further check needed, nothing visible

        // fdo#87382 Also display the cell cursor for the visible part of
        // merged cells if the view position is part of merged cells.
        const ScMergeAttr& rMerge = pPattern->GetItem(ATTR_MERGE);
        if (rMerge.GetColMerge() <= 1 && rMerge.GetRowMerge() <= 1)
            return;     // not merged and invisible

        SCCOL nX2 = nX + rMerge.GetColMerge() - 1;
        SCROW nY2 = nY + rMerge.GetRowMerge() - 1;
        // Check if the middle or tail of the merged range is visible.
        if (!(maVisibleRange.mnCol1 <= nX2 && maVisibleRange.mnRow1 <= nY2))
            return;     // no visible part
    }

    //  don't show the cursor in overlapped cells
    const ScMergeFlagAttr& rMergeFlag = pPattern->GetItem(ATTR_MERGE_FLAG);
    bool bOverlapped = rMergeFlag.IsOverlapped();

    //  left or above of the screen?
    bool bVis = comphelper::LibreOfficeKit::isActive() || ( nX>=pViewData->GetPosX(eHWhich) && nY>=pViewData->GetPosY(eVWhich) );
    if (!bVis)
    {
        SCCOL nEndX = nX;
        SCROW nEndY = nY;
        const ScMergeAttr& rMerge = pPattern->GetItem(ATTR_MERGE);
        if (rMerge.GetColMerge() > 1)
            nEndX += rMerge.GetColMerge()-1;
        if (rMerge.GetRowMerge() > 1)
            nEndY += rMerge.GetRowMerge()-1;
        bVis = ( nEndX>=pViewData->GetPosX(eHWhich) && nEndY>=pViewData->GetPosY(eVWhich) );
    }

    if ( bVis && !bOverlapped && !pViewData->HasEditView(eWhich) && pViewData->IsActive() )
    {
        Point aScrPos = pViewData->GetScrPos( nX, nY, eWhich, true );
        bool bLayoutRTL = pDoc->IsLayoutRTL( nTab );

        //  completely right of/below the screen?
        //  (test with logical start position in aScrPos)
        bool bMaybeVisible;
        if ( bLayoutRTL )
            bMaybeVisible = ( aScrPos.X() >= -2 && aScrPos.Y() >= -2 );
        else
        {
            Size aOutSize = GetOutputSizePixel();
            bMaybeVisible = ( aScrPos.X() <= aOutSize.Width() + 2 && aScrPos.Y() <= aOutSize.Height() + 2 );
        }

        // in the tiled rendering case, don't limit to the screen size
        if (bMaybeVisible || comphelper::LibreOfficeKit::isActive())
        {
            long nSizeXPix;
            long nSizeYPix;
            pViewData->GetMergeSizePixel( nX, nY, nSizeXPix, nSizeYPix );

            if (bLayoutRTL)
                aScrPos.AdjustX( -(nSizeXPix - 2) );       // move instead of mirroring

            // show the cursor as 4 (thin) rectangles
            tools::Rectangle aRect(aScrPos, Size(nSizeXPix - 1, nSizeYPix - 1));

            float fScaleFactor = GetDPIScaleFactor();

            long aCursorWidth = 1 * fScaleFactor;

            tools::Rectangle aLeft = aRect;
            aLeft.AdjustTop( -aCursorWidth );
            aLeft.AdjustBottom(aCursorWidth );
            aLeft.SetRight( aLeft.Left() );
            aLeft.AdjustLeft( -aCursorWidth );

            tools::Rectangle aRight = aRect;
            aRight.AdjustTop( -aCursorWidth );
            aRight.AdjustBottom(aCursorWidth );
            aRight.SetLeft( aRight.Right() );
            aRight.AdjustRight(aCursorWidth );

            tools::Rectangle aTop = aRect;
            aTop.SetBottom( aTop.Top() );
            aTop.AdjustTop( -aCursorWidth );

            tools::Rectangle aBottom = aRect;
            aBottom.SetTop( aBottom.Bottom() );
            aBottom.AdjustBottom(aCursorWidth );

            aPixelRects.push_back(aLeft);
            aPixelRects.push_back(aRight);
            aPixelRects.push_back(aTop);
            aPixelRects.push_back(aBottom);
        }
    }

    if ( !aPixelRects.empty() )
    {
        if (comphelper::LibreOfficeKit::isActive())
        {
            mpOOCursors.reset(new sdr::overlay::OverlayObjectList);
            updateLibreOfficeKitCellCursor(nullptr);
        }
        else
        {
            // #i70788# get the OverlayManager safely
            rtl::Reference<sdr::overlay::OverlayManager> xOverlayManager = getOverlayManager();

            if (xOverlayManager.is())
            {
                Color aCursorColor( SC_MOD()->GetColorConfig().GetColorValue(svtools::FONTCOLOR).nColor );
                if (pViewData->GetActivePart() != eWhich)
                    // non-active pane uses a different color.
                    aCursorColor = SC_MOD()->GetColorConfig().GetColorValue(svtools::CALCPAGEBREAKAUTOMATIC).nColor;
                std::vector< basegfx::B2DRange > aRanges;
                const basegfx::B2DHomMatrix aTransform(GetInverseViewTransformation());

                for(const tools::Rectangle & rRA : aPixelRects)
                {
                    basegfx::B2DRange aRB(rRA.Left(), rRA.Top(), rRA.Right() + 1, rRA.Bottom() + 1);
                    aRB.transform(aTransform);
                    aRanges.push_back(aRB);
                }

                std::unique_ptr<sdr::overlay::OverlayObject> pOverlay(new sdr::overlay::OverlaySelection(
                    sdr::overlay::OverlayType::Solid,
                    aCursorColor,
                    aRanges,
                    false));

                xOverlayManager->add(*pOverlay);
                mpOOCursors.reset(new sdr::overlay::OverlayObjectList);
                mpOOCursors->append(std::move(pOverlay));
            }
        }
    }

    if ( aOldMode != aDrawMode )
        SetMapMode( aOldMode );
}

void ScGridWindow::GetCellSelection(std::vector<tools::Rectangle>& rLogicRects)
{
    std::vector<tools::Rectangle> aPixelRects;
    GetSelectionRects(aPixelRects);
    updateLibreOfficeKitSelection(pViewData, aPixelRects, &rLogicRects);
}

void ScGridWindow::DeleteSelectionOverlay()
{
    mpOOSelection.reset();
}

void ScGridWindow::UpdateSelectionOverlay()
{
    MapMode aDrawMode = GetDrawMapMode();
    MapMode aOldMode = GetMapMode();
    if ( aOldMode != aDrawMode )
        SetMapMode( aDrawMode );

    DeleteSelectionOverlay();
    std::vector<tools::Rectangle> aPixelRects;
    GetSelectionRects( aPixelRects );

    if (!aPixelRects.empty() && pViewData->IsActive())
    {
        // #i70788# get the OverlayManager safely
        rtl::Reference<sdr::overlay::OverlayManager> xOverlayManager = getOverlayManager();
        if (comphelper::LibreOfficeKit::isActive())
        {
            // notify the LibreOfficeKit too
            updateLibreOfficeKitSelection(pViewData, aPixelRects);
        }
        else if (xOverlayManager.is())
        {
            std::vector< basegfx::B2DRange > aRanges;
            const basegfx::B2DHomMatrix aTransform(GetInverseViewTransformation());
            ScDocument* pDoc = pViewData->GetDocument();
            SCTAB nTab = pViewData->GetTabNo();
            bool bLayoutRTL = pDoc->IsLayoutRTL( nTab );

            for(const tools::Rectangle & rRA : aPixelRects)
            {
                if (bLayoutRTL)
                {
                    basegfx::B2DRange aRB(rRA.Left(), rRA.Top() - 1, rRA.Right() + 1, rRA.Bottom());
                    aRB.transform(aTransform);
                    aRanges.push_back(aRB);
                }
                else
                {
                    basegfx::B2DRange aRB(rRA.Left() - 1, rRA.Top() - 1, rRA.Right(), rRA.Bottom());
                    aRB.transform(aTransform);
                    aRanges.push_back(aRB);
                }
            }

            // get the system's highlight color
            const SvtOptionsDrawinglayer aSvtOptionsDrawinglayer;
            const Color aHighlight(aSvtOptionsDrawinglayer.getHilightColor());

            std::unique_ptr<sdr::overlay::OverlayObject> pOverlay(new sdr::overlay::OverlaySelection(
                sdr::overlay::OverlayType::Transparent,
                aHighlight,
                aRanges,
                true));

            xOverlayManager->add(*pOverlay);
            mpOOSelection.reset(new sdr::overlay::OverlayObjectList);
            mpOOSelection->append(std::move(pOverlay));
        }
    }
    else
    {
        ScTabViewShell* pViewShell = pViewData->GetViewShell();
        pViewShell->libreOfficeKitViewCallback(LOK_CALLBACK_TEXT_SELECTION, "EMPTY");
        pViewShell->libreOfficeKitViewCallback(LOK_CALLBACK_CELL_SELECTION_AREA, "EMPTY");
        SfxLokHelper::notifyOtherViews(pViewShell, LOK_CALLBACK_TEXT_VIEW_SELECTION, "selection", "EMPTY");
    }

    if ( aOldMode != aDrawMode )
        SetMapMode( aOldMode );
}

void ScGridWindow::DeleteAutoFillOverlay()
{
    mpOOAutoFill.reset();
    mpAutoFillRect.reset();
}

void ScGridWindow::UpdateAutoFillOverlay()
{
    MapMode aDrawMode = GetDrawMapMode();
    MapMode aOldMode = GetMapMode();
    if ( aOldMode != aDrawMode )
        SetMapMode( aDrawMode );

    DeleteAutoFillOverlay();

    //  get the AutoFill handle rectangle in pixels

    if ( bAutoMarkVisible && aAutoMarkPos.Tab() == pViewData->GetTabNo() &&
         !pViewData->HasEditView(eWhich) && pViewData->IsActive() )
    {
        SCCOL nX = aAutoMarkPos.Col();
        SCROW nY = aAutoMarkPos.Row();

        if (!maVisibleRange.isInside(nX, nY) && !comphelper::LibreOfficeKit::isActive())
        {
            // Autofill mark is not visible.  Bail out.
            return;
        }

        SCTAB nTab = pViewData->GetTabNo();
        ScDocument* pDoc = pViewData->GetDocument();
        bool bLayoutRTL = pDoc->IsLayoutRTL( nTab );

        float fScaleFactor = GetDPIScaleFactor();
        // Size should be even
        Size aFillHandleSize(6 * fScaleFactor, 6 * fScaleFactor);

        Point aFillPos = pViewData->GetScrPos( nX, nY, eWhich, true );
        long nSizeXPix;
        long nSizeYPix;
        pViewData->GetMergeSizePixel( nX, nY, nSizeXPix, nSizeYPix );

        if (bLayoutRTL)
            aFillPos.AdjustX( -(nSizeXPix - 2 + (aFillHandleSize.Width() / 2)) );
        else
            aFillPos.AdjustX(nSizeXPix - (aFillHandleSize.Width() / 2) );

        aFillPos.AdjustY(nSizeYPix );
        aFillPos.AdjustY( -(aFillHandleSize.Height() / 2) );

        tools::Rectangle aFillRect(aFillPos, aFillHandleSize);

        // expand rect to increase hit area
        mpAutoFillRect = aFillRect;
        mpAutoFillRect->expand(fScaleFactor);

        // #i70788# get the OverlayManager safely
        rtl::Reference<sdr::overlay::OverlayManager> xOverlayManager = getOverlayManager();
        if (comphelper::LibreOfficeKit::isActive()) // notify the LibreOfficeKit
        {
            updateLibreOfficeKitAutoFill(pViewData, aFillRect);
        }
        else if (xOverlayManager.is())
        {
            Color aHandleColor( SC_MOD()->GetColorConfig().GetColorValue(svtools::FONTCOLOR).nColor );
            if (pViewData->GetActivePart() != eWhich)
                // non-active pane uses a different color.
                aHandleColor = SC_MOD()->GetColorConfig().GetColorValue(svtools::CALCPAGEBREAKAUTOMATIC).nColor;
            std::vector< basegfx::B2DRange > aRanges;
            const basegfx::B2DHomMatrix aTransform(GetInverseViewTransformation());
            basegfx::B2DRange aRB = vcl::unotools::b2DRectangleFromRectangle(aFillRect);

            aRB.transform(aTransform);
            aRanges.push_back(aRB);

            std::unique_ptr<sdr::overlay::OverlayObject> pOverlay(new sdr::overlay::OverlaySelection(
                sdr::overlay::OverlayType::Solid,
                aHandleColor,
                aRanges,
                false));

            xOverlayManager->add(*pOverlay);
            mpOOAutoFill.reset(new sdr::overlay::OverlayObjectList);
            mpOOAutoFill->append(std::move(pOverlay));
        }

        if ( aOldMode != aDrawMode )
            SetMapMode( aOldMode );
    }
}

void ScGridWindow::DeleteDragRectOverlay()
{
    mpOODragRect.reset();
}

void ScGridWindow::UpdateDragRectOverlay()
{
    MapMode aDrawMode = GetDrawMapMode();
    MapMode aOldMode = GetMapMode();
    if ( aOldMode != aDrawMode )
        SetMapMode( aDrawMode );

    DeleteDragRectOverlay();

    //  get the rectangles in pixels (moved from DrawDragRect)

    if ( bDragRect || bPagebreakDrawn )
    {
        std::vector<tools::Rectangle> aPixelRects;

        SCCOL nX1 = bDragRect ? nDragStartX : aPagebreakDrag.aStart.Col();
        SCROW nY1 = bDragRect ? nDragStartY : aPagebreakDrag.aStart.Row();
        SCCOL nX2 = bDragRect ? nDragEndX : aPagebreakDrag.aEnd.Col();
        SCROW nY2 = bDragRect ? nDragEndY : aPagebreakDrag.aEnd.Row();

        SCTAB nTab = pViewData->GetTabNo();

        SCCOL nPosX = pViewData->GetPosX(WhichH(eWhich));
        SCROW nPosY = pViewData->GetPosY(WhichV(eWhich));
        if (nX1 < nPosX) nX1 = nPosX;
        if (nX2 < nPosX) nX2 = nPosX;
        if (nY1 < nPosY) nY1 = nPosY;
        if (nY2 < nPosY) nY2 = nPosY;

        Point aScrPos( pViewData->GetScrPos( nX1, nY1, eWhich ) );

        long nSizeXPix=0;
        long nSizeYPix=0;
        ScDocument* pDoc = pViewData->GetDocument();
        double nPPTX = pViewData->GetPPTX();
        double nPPTY = pViewData->GetPPTY();
        SCCOLROW i;

        bool bLayoutRTL = pDoc->IsLayoutRTL( nTab );
        long nLayoutSign = bLayoutRTL ? -1 : 1;

        if (ValidCol(nX2) && nX2>=nX1)
            for (i=nX1; i<=nX2; i++)
                nSizeXPix += ScViewData::ToPixel( pDoc->GetColWidth( static_cast<SCCOL>(i), nTab ), nPPTX );
        else
        {
            aScrPos.AdjustX( -nLayoutSign );
            nSizeXPix   += 2;
        }

        if (ValidRow(nY2) && nY2>=nY1)
            for (i=nY1; i<=nY2; i++)
                nSizeYPix += ScViewData::ToPixel( pDoc->GetRowHeight( i, nTab ), nPPTY );
        else
        {
            aScrPos.AdjustY( -1 );
            nSizeYPix   += 2;
        }

        aScrPos.AdjustX( -(2 * nLayoutSign) );
        aScrPos.AdjustY( -2 );
        tools::Rectangle aRect( aScrPos.X(), aScrPos.Y(),
                         aScrPos.X() + ( nSizeXPix + 2 ) * nLayoutSign, aScrPos.Y() + nSizeYPix + 2 );
        if ( bLayoutRTL )
        {
            aRect.SetLeft( aRect.Right() );   // end position is left
            aRect.SetRight( aScrPos.X() );
        }

        if ( meDragInsertMode == INS_CELLSDOWN )
        {
            aPixelRects.emplace_back( aRect.Left()+1, aRect.Top()+3, aRect.Left()+1, aRect.Bottom()-2 );
            aPixelRects.emplace_back( aRect.Right()-1, aRect.Top()+3, aRect.Right()-1, aRect.Bottom()-2 );
            aPixelRects.emplace_back( aRect.Left()+1, aRect.Top(), aRect.Right()-1, aRect.Top()+2 );
            aPixelRects.emplace_back( aRect.Left()+1, aRect.Bottom()-1, aRect.Right()-1, aRect.Bottom()-1 );
        }
        else if ( meDragInsertMode == INS_CELLSRIGHT )
        {
            aPixelRects.emplace_back( aRect.Left(), aRect.Top()+1, aRect.Left()+2, aRect.Bottom()-1 );
            aPixelRects.emplace_back( aRect.Right()-1, aRect.Top()+1, aRect.Right()-1, aRect.Bottom()-1 );
            aPixelRects.emplace_back( aRect.Left()+3, aRect.Top()+1, aRect.Right()-2, aRect.Top()+1 );
            aPixelRects.emplace_back( aRect.Left()+3, aRect.Bottom()-1, aRect.Right()-2, aRect.Bottom()-1 );
        }
        else
        {
            aPixelRects.emplace_back( aRect.Left(), aRect.Top(), aRect.Left()+2, aRect.Bottom() );
            aPixelRects.emplace_back( aRect.Right()-2, aRect.Top(), aRect.Right(), aRect.Bottom() );
            aPixelRects.emplace_back( aRect.Left()+3, aRect.Top(), aRect.Right()-3, aRect.Top()+2 );
            aPixelRects.emplace_back( aRect.Left()+3, aRect.Bottom()-2, aRect.Right()-3, aRect.Bottom() );
        }

        // #i70788# get the OverlayManager safely
        rtl::Reference<sdr::overlay::OverlayManager> xOverlayManager = getOverlayManager();

        if (xOverlayManager.is() && !comphelper::LibreOfficeKit::isActive())
        {
            std::vector< basegfx::B2DRange > aRanges;
            const basegfx::B2DHomMatrix aTransform(GetInverseViewTransformation());

            for(const tools::Rectangle & rRA : aPixelRects)
            {
                basegfx::B2DRange aRB(rRA.Left(), rRA.Top(), rRA.Right() + 1, rRA.Bottom() + 1);
                aRB.transform(aTransform);
                aRanges.push_back(aRB);
            }

            std::unique_ptr<sdr::overlay::OverlayObject> pOverlay(new sdr::overlay::OverlaySelection(
                sdr::overlay::OverlayType::Invert,
                COL_BLACK,
                aRanges,
                false));

            xOverlayManager->add(*pOverlay);
            mpOODragRect.reset(new sdr::overlay::OverlayObjectList);
            mpOODragRect->append(std::move(pOverlay));
        }
    }

    if ( aOldMode != aDrawMode )
        SetMapMode( aOldMode );
}

void ScGridWindow::DeleteHeaderOverlay()
{
    mpOOHeader.reset();
}

void ScGridWindow::UpdateHeaderOverlay()
{
    MapMode aDrawMode = GetDrawMapMode();
    MapMode aOldMode = GetMapMode();
    if ( aOldMode != aDrawMode )
        SetMapMode( aDrawMode );

    DeleteHeaderOverlay();

    //  Pixel rectangle is in aInvertRect
    if ( !aInvertRect.IsEmpty() )
    {
        // #i70788# get the OverlayManager safely
        rtl::Reference<sdr::overlay::OverlayManager> xOverlayManager = getOverlayManager();

        if (xOverlayManager.is() && !comphelper::LibreOfficeKit::isActive())
        {
            // Color aHighlight = GetSettings().GetStyleSettings().GetHighlightColor();
            std::vector< basegfx::B2DRange > aRanges;
            const basegfx::B2DHomMatrix aTransform(GetInverseViewTransformation());
            basegfx::B2DRange aRB(aInvertRect.Left(), aInvertRect.Top(), aInvertRect.Right() + 1, aInvertRect.Bottom() + 1);

            aRB.transform(aTransform);
            aRanges.push_back(aRB);

            std::unique_ptr<sdr::overlay::OverlayObject> pOverlay(new sdr::overlay::OverlaySelection(
                sdr::overlay::OverlayType::Invert,
                COL_BLACK,
                aRanges,
                false));

            xOverlayManager->add(*pOverlay);
            mpOOHeader.reset(new sdr::overlay::OverlayObjectList);
            mpOOHeader->append(std::move(pOverlay));
        }
    }

    if ( aOldMode != aDrawMode )
        SetMapMode( aOldMode );
}

void ScGridWindow::DeleteShrinkOverlay()
{
    mpOOShrink.reset();
}

void ScGridWindow::UpdateShrinkOverlay()
{
    MapMode aDrawMode = GetDrawMapMode();
    MapMode aOldMode = GetMapMode();
    if ( aOldMode != aDrawMode )
        SetMapMode( aDrawMode );

    DeleteShrinkOverlay();

    //  get the rectangle in pixels

    tools::Rectangle aPixRect;
    ScRange aRange;
    SCTAB nTab = pViewData->GetTabNo();
    if ( pViewData->IsRefMode() && nTab >= pViewData->GetRefStartZ() && nTab <= pViewData->GetRefEndZ() &&
         pViewData->GetDelMark( aRange ) )
    {
        //! limit to visible area
        if ( aRange.aStart.Col() <= aRange.aEnd.Col() &&
             aRange.aStart.Row() <= aRange.aEnd.Row() )
        {
            Point aStart = pViewData->GetScrPos( aRange.aStart.Col(),
                                                 aRange.aStart.Row(), eWhich );
            Point aEnd = pViewData->GetScrPos( aRange.aEnd.Col()+1,
                                               aRange.aEnd.Row()+1, eWhich );
            aEnd.AdjustX( -1 );
            aEnd.AdjustY( -1 );

            aPixRect = tools::Rectangle( aStart,aEnd );
        }
    }

    if ( !aPixRect.IsEmpty() )
    {
        // #i70788# get the OverlayManager safely
        rtl::Reference<sdr::overlay::OverlayManager> xOverlayManager = getOverlayManager();

        if (xOverlayManager.is() && !comphelper::LibreOfficeKit::isActive())
        {
            std::vector< basegfx::B2DRange > aRanges;
            const basegfx::B2DHomMatrix aTransform(GetInverseViewTransformation());
            basegfx::B2DRange aRB(aPixRect.Left(), aPixRect.Top(), aPixRect.Right() + 1, aPixRect.Bottom() + 1);

            aRB.transform(aTransform);
            aRanges.push_back(aRB);

            std::unique_ptr<sdr::overlay::OverlayObject> pOverlay(new sdr::overlay::OverlaySelection(
                sdr::overlay::OverlayType::Invert,
                COL_BLACK,
                aRanges,
                false));

            xOverlayManager->add(*pOverlay);
            mpOOShrink.reset(new sdr::overlay::OverlayObjectList);
            mpOOShrink->append(std::move(pOverlay));
        }
    }

    if ( aOldMode != aDrawMode )
        SetMapMode( aOldMode );
}

// #i70788# central method to get the OverlayManager safely
rtl::Reference<sdr::overlay::OverlayManager> ScGridWindow::getOverlayManager() const
{
    SdrPageView* pPV = pViewData->GetView()->GetScDrawView()->GetSdrPageView();

    if(pPV)
    {
        SdrPageWindow* pPageWin = pPV->FindPageWindow( *this );

        if ( pPageWin )
        {
            return pPageWin->GetOverlayManager();
        }
    }

    return rtl::Reference<sdr::overlay::OverlayManager>();
}

void ScGridWindow::flushOverlayManager()
{
    // #i70788# get the OverlayManager safely
    rtl::Reference<sdr::overlay::OverlayManager> xOverlayManager = getOverlayManager();

    if (xOverlayManager.is())
        xOverlayManager->flush();
}

ScViewData* ScGridWindow::getViewData()
{
    return pViewData;
}

FactoryFunction ScGridWindow::GetUITestFactory() const
{
    return ScGridWinUIObject::create;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
