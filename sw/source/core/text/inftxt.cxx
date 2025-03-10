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

#include <com/sun/star/linguistic2/XHyphenator.hpp>

#include <unotools/linguprops.hxx>
#include <unotools/lingucfg.hxx>
#include <hintids.hxx>
#include <svl/ctloptions.hxx>
#include <sfx2/printer.hxx>
#include <sal/log.hxx>
#include <editeng/hyphenzoneitem.hxx>
#include <editeng/hngpnctitem.hxx>
#include <editeng/scriptspaceitem.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/splwrap.hxx>
#include <editeng/pgrditem.hxx>
#include <editeng/tstpitem.hxx>
#include <editeng/shaditem.hxx>

#include <SwSmartTagMgr.hxx>
#include <breakit.hxx>
#include <editeng/forbiddenruleitem.hxx>
#include <paintfrm.hxx>
#include <swmodule.hxx>
#include <vcl/svapp.hxx>
#include <viewsh.hxx>
#include <viewopt.hxx>
#include <frmtool.hxx>
#include <IDocumentSettingAccess.hxx>
#include <IDocumentDeviceAccess.hxx>
#include <IDocumentMarkAccess.hxx>
#include <paratr.hxx>
#include <rootfrm.hxx>
#include "inftxt.hxx"
#include <blink.hxx>
#include <noteurl.hxx>
#include "porftn.hxx"
#include "porrst.hxx"
#include "itratr.hxx"
#include "portab.hxx"
#include <accessibilityoptions.hxx>
#include <wrong.hxx>
#include <doc.hxx>
#include <pam.hxx>
#include <numrule.hxx>
#include <EnhancedPDFExportHelper.hxx>

using namespace ::com::sun::star;
using namespace ::com::sun::star::linguistic2;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::beans;

#define CHAR_UNDERSCORE u'_'
#define CHAR_LEFT_ARROW u'\x25C0'
#define CHAR_RIGHT_ARROW u'\x25B6'
#define CHAR_TAB u'\x2192'
#define CHAR_TAB_RTL u'\x2190'
#define CHAR_LINEBREAK u'\x21B5'
#define CHAR_LINEBREAK_RTL u'\x21B3'

#define DRAW_SPECIAL_OPTIONS_CENTER 1
#define DRAW_SPECIAL_OPTIONS_ROTATE 2

SwLineInfo::SwLineInfo()
    : pSpace( nullptr ),
      nVertAlign( SvxParaVertAlignItem::Align::Automatic ),
      nDefTabStop( 0 ),
      bListTabStopIncluded( false ),
      nListTabStopPosition( 0 )
{
}

SwLineInfo::~SwLineInfo()
{
}

void SwLineInfo::CtorInitLineInfo( const SwAttrSet& rAttrSet,
                                   const SwTextNode& rTextNode )
{
    pRuler.reset( new SvxTabStopItem( rAttrSet.GetTabStops() ) );
    if ( rTextNode.GetListTabStopPosition( nListTabStopPosition ) )
    {
        bListTabStopIncluded = true;

        // insert the list tab stop into SvxTabItem instance <pRuler>
        const SvxTabStop aListTabStop( nListTabStopPosition,
                                       SvxTabAdjust::Left );
        pRuler->Insert( aListTabStop );

        // remove default tab stops, which are before the inserted list tab stop
        for ( sal_uInt16 i = 0; i < pRuler->Count(); i++ )
        {
            if ( (*pRuler)[i].GetTabPos() < nListTabStopPosition &&
                 (*pRuler)[i].GetAdjustment() == SvxTabAdjust::Default )
            {
                pRuler->Remove(i);
                continue;
            }
        }
    }

    if ( !rTextNode.getIDocumentSettingAccess()->get(DocumentSettingId::TABS_RELATIVE_TO_INDENT) )
    {
        // remove default tab stop at position 0
        for ( sal_uInt16 i = 0; i < pRuler->Count(); i++ )
        {
            if ( (*pRuler)[i].GetTabPos() == 0 &&
                 (*pRuler)[i].GetAdjustment() == SvxTabAdjust::Default )
            {
                pRuler->Remove(i);
                break;
            }
        }
    }

    pSpace = &rAttrSet.GetLineSpacing();
    nVertAlign = rAttrSet.GetParaVertAlign().GetValue();
    nDefTabStop = USHRT_MAX;
}

void SwTextInfo::CtorInitTextInfo( SwTextFrame *pFrame )
{
    m_pPara = pFrame->GetPara();
    m_nTextStart = pFrame->GetOfst();
    if (!m_pPara)
    {
        SAL_WARN("sw.core", "+SwTextInfo::CTOR: missing paragraph information");
        pFrame->Format(pFrame->getRootFrame()->GetCurrShell()->GetOut());
        m_pPara = pFrame->GetPara();
    }
}

SwTextInfo::SwTextInfo( const SwTextInfo &rInf )
    : m_pPara( const_cast<SwTextInfo&>(rInf).GetParaPortion() )
    , m_nTextStart( rInf.GetTextStart() )
{ }

#if OSL_DEBUG_LEVEL > 0

static void ChkOutDev( const SwTextSizeInfo &rInf )
{
    if ( !rInf.GetVsh() )
        return;

    const OutputDevice* pOut = rInf.GetOut();
    const OutputDevice* pRef = rInf.GetRefDev();
    OSL_ENSURE( pOut && pRef, "ChkOutDev: invalid output devices" );
}
#endif

static TextFrameIndex GetMinLen( const SwTextSizeInfo &rInf )
{
    const TextFrameIndex nTextLen(rInf.GetText().getLength());
    if (rInf.GetLen() == TextFrameIndex(COMPLETE_STRING))
        return nTextLen;
    const TextFrameIndex nInfLen = rInf.GetIdx() + rInf.GetLen();
    return std::min(nTextLen, nInfLen);
}

SwTextSizeInfo::SwTextSizeInfo()
: m_pKanaComp(nullptr)
, m_pVsh(nullptr)
, m_pOut(nullptr)
, m_pRef(nullptr)
, m_pFnt(nullptr)
, m_pUnderFnt(nullptr)
, m_pFrame(nullptr)
, m_pOpt(nullptr)
, m_pText(nullptr)
, m_nIdx(0)
, m_nLen(0)
, m_nKanaIdx(0)
, m_bOnWin    (false)
, m_bNotEOL   (false)
, m_bURLNotify(false)
, m_bStopUnderflow(false)
, m_bFootnoteInside(false)
, m_bOtherThanFootnoteInside(false)
, m_bMulti(false)
, m_bFirstMulti(false)
, m_bRuby(false)
, m_bHanging(false)
, m_bScriptSpace(false)
, m_bForbiddenChars(false)
, m_bSnapToGrid(false)
, m_nDirection(0)
{}

SwTextSizeInfo::SwTextSizeInfo( const SwTextSizeInfo &rNew )
    : SwTextInfo( rNew ),
      m_pKanaComp(rNew.GetpKanaComp()),
      m_pVsh(const_cast<SwTextSizeInfo&>(rNew).GetVsh()),
      m_pOut(const_cast<SwTextSizeInfo&>(rNew).GetOut()),
      m_pRef(const_cast<SwTextSizeInfo&>(rNew).GetRefDev()),
      m_pFnt(const_cast<SwTextSizeInfo&>(rNew).GetFont()),
      m_pUnderFnt(rNew.GetUnderFnt()),
      m_pFrame(rNew.m_pFrame),
      m_pOpt(&rNew.GetOpt()),
      m_pText(&rNew.GetText()),
      m_nIdx(rNew.GetIdx()),
      m_nLen(rNew.GetLen()),
      m_nKanaIdx( rNew.GetKanaIdx() ),
      m_bOnWin( rNew.OnWin() ),
      m_bNotEOL( rNew.NotEOL() ),
      m_bURLNotify( rNew.URLNotify() ),
      m_bStopUnderflow( rNew.StopUnderflow() ),
      m_bFootnoteInside( rNew.IsFootnoteInside() ),
      m_bOtherThanFootnoteInside( rNew.IsOtherThanFootnoteInside() ),
      m_bMulti( rNew.IsMulti() ),
      m_bFirstMulti( rNew.IsFirstMulti() ),
      m_bRuby( rNew.IsRuby() ),
      m_bHanging( rNew.IsHanging() ),
      m_bScriptSpace( rNew.HasScriptSpace() ),
      m_bForbiddenChars( rNew.HasForbiddenChars() ),
      m_bSnapToGrid( rNew.SnapToGrid() ),
      m_nDirection( rNew.GetDirection() )
{
#if OSL_DEBUG_LEVEL > 0
    ChkOutDev( *this );
#endif
}

void SwTextSizeInfo::CtorInitTextSizeInfo( OutputDevice* pRenderContext, SwTextFrame *pFrame,
           TextFrameIndex const nNewIdx)
{
    m_pKanaComp = nullptr;
    m_nKanaIdx = 0;
    m_pFrame = pFrame;
    CtorInitTextInfo( m_pFrame );
    SwDoc const& rDoc(m_pFrame->GetDoc());
    m_pVsh = m_pFrame->getRootFrame()->GetCurrShell();

    // Get the output and reference device
    if ( m_pVsh )
    {
        m_pOut = pRenderContext;
        m_pRef = &m_pVsh->GetRefDev();
        m_bOnWin = m_pVsh->GetWin() || OUTDEV_WINDOW == m_pOut->GetOutDevType() || m_pVsh->isOutputToWindow();
    }
    else
    {
        // Access via StarONE. We do not need a Shell or an active one.
        if (rDoc.getIDocumentSettingAccess().get(DocumentSettingId::HTML_MODE))
        {
            // We can only pick the AppWin here? (there's nothing better to pick?)
            m_pOut = Application::GetDefaultDevice();
        }
        else
            m_pOut = rDoc.getIDocumentDeviceAccess().getPrinter(false);

        m_pRef = m_pOut;
    }

#if OSL_DEBUG_LEVEL > 0
    ChkOutDev( *this );
#endif

    // Set default layout mode ( LTR or RTL ).
    if ( m_pFrame->IsRightToLeft() )
    {
        m_pOut->SetLayoutMode( ComplexTextLayoutFlags::BiDiStrong | ComplexTextLayoutFlags::BiDiRtl );
        m_pRef->SetLayoutMode( ComplexTextLayoutFlags::BiDiStrong | ComplexTextLayoutFlags::BiDiRtl );
        m_nDirection = DIR_RIGHT2LEFT;
    }
    else
    {
        m_pOut->SetLayoutMode( ComplexTextLayoutFlags::BiDiStrong );
        m_pRef->SetLayoutMode( ComplexTextLayoutFlags::BiDiStrong );
        m_nDirection = DIR_LEFT2RIGHT;
    }

    // The Options

    m_pOpt = m_pVsh ?
           m_pVsh->GetViewOptions() :
           SW_MOD()->GetViewOption(rDoc.getIDocumentSettingAccess().get(DocumentSettingId::HTML_MODE)); // Options from Module, due to StarONE

    // bURLNotify is set if MakeGraphic prepares it
    // TODO: Unwind
    m_bURLNotify = pNoteURL && !m_bOnWin;

    SetSnapToGrid( m_pFrame->GetTextNodeForParaProps()->GetSwAttrSet().GetParaGrid().GetValue() &&
                   m_pFrame->IsInDocBody() );

    m_pFnt = nullptr;
    m_pUnderFnt = nullptr;
    m_pText = &m_pFrame->GetText();

    m_nIdx = nNewIdx;
    m_nLen = TextFrameIndex(COMPLETE_STRING);
    m_bNotEOL = false;
    m_bStopUnderflow = m_bFootnoteInside = m_bOtherThanFootnoteInside = false;
    m_bMulti = m_bFirstMulti = m_bRuby = m_bHanging = m_bScriptSpace =
        m_bForbiddenChars = false;

    SetLen( GetMinLen( *this ) );
}

SwTextSizeInfo::SwTextSizeInfo( const SwTextSizeInfo &rNew, const OUString* pText,
              TextFrameIndex const nIndex)
    : SwTextInfo( rNew ),
      m_pKanaComp(rNew.GetpKanaComp()),
      m_pVsh(const_cast<SwTextSizeInfo&>(rNew).GetVsh()),
      m_pOut(const_cast<SwTextSizeInfo&>(rNew).GetOut()),
      m_pRef(const_cast<SwTextSizeInfo&>(rNew).GetRefDev()),
      m_pFnt(const_cast<SwTextSizeInfo&>(rNew).GetFont()),
      m_pUnderFnt(rNew.GetUnderFnt()),
      m_pFrame( rNew.m_pFrame ),
      m_pOpt(&rNew.GetOpt()),
      m_pText(pText),
      m_nIdx(nIndex),
      m_nLen(COMPLETE_STRING),
      m_nKanaIdx( rNew.GetKanaIdx() ),
      m_bOnWin( rNew.OnWin() ),
      m_bNotEOL( rNew.NotEOL() ),
      m_bURLNotify( rNew.URLNotify() ),
      m_bStopUnderflow( rNew.StopUnderflow() ),
      m_bFootnoteInside( rNew.IsFootnoteInside() ),
      m_bOtherThanFootnoteInside( rNew.IsOtherThanFootnoteInside() ),
      m_bMulti( rNew.IsMulti() ),
      m_bFirstMulti( rNew.IsFirstMulti() ),
      m_bRuby( rNew.IsRuby() ),
      m_bHanging( rNew.IsHanging() ),
      m_bScriptSpace( rNew.HasScriptSpace() ),
      m_bForbiddenChars( rNew.HasForbiddenChars() ),
      m_bSnapToGrid( rNew.SnapToGrid() ),
      m_nDirection( rNew.GetDirection() )
{
#if OSL_DEBUG_LEVEL > 0
    ChkOutDev( *this );
#endif
    SetLen( GetMinLen( *this ) );
}

SwTextSizeInfo::SwTextSizeInfo(SwTextFrame *const pTextFrame,
            TextFrameIndex const nIndex)
    : m_bOnWin(false)
{
    CtorInitTextSizeInfo( pTextFrame->getRootFrame()->GetCurrShell()->GetOut(), pTextFrame, nIndex );
}

void SwTextSizeInfo::SelectFont()
{
     // The path needs to go via ChgPhysFnt or the FontMetricCache gets confused.
     // In this case pLastMet has it's old value.
     // Wrong: GetOut()->SetFont( GetFont()->GetFnt() );
    GetFont()->Invalidate();
    GetFont()->ChgPhysFnt( m_pVsh, *GetOut() );
}

void SwTextSizeInfo::NoteAnimation() const
{
    if( OnWin() )
        SwRootFrame::FlushVout();

    OSL_ENSURE( m_pOut == m_pVsh->GetOut(),
            "SwTextSizeInfo::NoteAnimation() changed m_pOut" );
}

SwPosSize SwTextSizeInfo::GetTextSize( OutputDevice* pOutDev,
                                     const SwScriptInfo* pSI,
                                     const OUString& rText,
                                     const TextFrameIndex nIndex,
                                     const TextFrameIndex nLength) const
{
    SwDrawTextInfo aDrawInf( m_pVsh, *pOutDev, pSI, rText, nIndex, nLength );
    aDrawInf.SetFrame( m_pFrame );
    aDrawInf.SetFont( m_pFnt );
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    aDrawInf.SetKanaComp( 0 );
    return SwPosSize(m_pFnt->GetTextSize_( aDrawInf ));
}

SwPosSize SwTextSizeInfo::GetTextSize() const
{
    const SwScriptInfo& rSI =
                     const_cast<SwParaPortion*>(GetParaPortion())->GetScriptInfo();

    // in some cases, compression is not allowed or suppressed for
    // performance reasons
    sal_uInt16 nComp =( SwFontScript::CJK == GetFont()->GetActual() &&
                    rSI.CountCompChg() &&
                    ! IsMulti() ) ?
                    GetKanaComp() :
                                0 ;

    SwDrawTextInfo aDrawInf( m_pVsh, *m_pOut, &rSI, *m_pText, m_nIdx, m_nLen );
    aDrawInf.SetFrame( m_pFrame );
    aDrawInf.SetFont( m_pFnt );
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    aDrawInf.SetKanaComp( nComp );
    return SwPosSize(m_pFnt->GetTextSize_( aDrawInf ));
}

void SwTextSizeInfo::GetTextSize( const SwScriptInfo* pSI, const TextFrameIndex nIndex,
                                const TextFrameIndex nLength, const sal_uInt16 nComp,
                                sal_uInt16& nMinSize, sal_uInt16& nMaxSizeDiff,
                                vcl::TextLayoutCache const*const pCache) const
{
    SwDrawTextInfo aDrawInf( m_pVsh, *m_pOut, pSI, *m_pText, nIndex, nLength,
            0, false, pCache);
    aDrawInf.SetFrame( m_pFrame );
    aDrawInf.SetFont( m_pFnt );
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    aDrawInf.SetKanaComp( nComp );
    SwPosSize aSize( m_pFnt->GetTextSize_( aDrawInf ) );
    nMaxSizeDiff = static_cast<sal_uInt16>(aDrawInf.GetKanaDiff());
    nMinSize = aSize.Width();
}

TextFrameIndex SwTextSizeInfo::GetTextBreak( const long nLineWidth,
                                       const TextFrameIndex nMaxLen,
                                       const sal_uInt16 nComp,
                                       vcl::TextLayoutCache const*const pCache) const
{
    const SwScriptInfo& rScriptInfo =
                     const_cast<SwParaPortion*>(GetParaPortion())->GetScriptInfo();

    OSL_ENSURE( m_pRef == m_pOut, "GetTextBreak is supposed to use the RefDev" );
    SwDrawTextInfo aDrawInf( m_pVsh, *m_pOut, &rScriptInfo,
                             *m_pText, GetIdx(), nMaxLen,  0, false, pCache );
    aDrawInf.SetFrame( m_pFrame );
    aDrawInf.SetFont( m_pFnt );
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    aDrawInf.SetKanaComp( nComp );
    aDrawInf.SetHyphPos( nullptr );

    return m_pFnt->GetTextBreak( aDrawInf, nLineWidth );
}

TextFrameIndex SwTextSizeInfo::GetTextBreak( const long nLineWidth,
                                       const TextFrameIndex nMaxLen,
                                       const sal_uInt16 nComp,
                                       TextFrameIndex& rExtraCharPos,
                                       vcl::TextLayoutCache const*const pCache) const
{
    const SwScriptInfo& rScriptInfo =
                     const_cast<SwParaPortion*>(GetParaPortion())->GetScriptInfo();

    OSL_ENSURE( m_pRef == m_pOut, "GetTextBreak is supposed to use the RefDev" );
    SwDrawTextInfo aDrawInf( m_pVsh, *m_pOut, &rScriptInfo,
                             *m_pText, GetIdx(), nMaxLen, 0, false, pCache );
    aDrawInf.SetFrame( m_pFrame );
    aDrawInf.SetFont( m_pFnt );
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    aDrawInf.SetKanaComp( nComp );
    aDrawInf.SetHyphPos( &rExtraCharPos );

    return m_pFnt->GetTextBreak( aDrawInf, nLineWidth );
}

bool SwTextSizeInfo::HasHint(TextFrameIndex const nPos) const
{
    std::pair<SwTextNode const*, sal_Int32> const pos(m_pFrame->MapViewToModel(nPos));
    return pos.first->GetTextAttrForCharAt(pos.second);
}

void SwTextPaintInfo::CtorInitTextPaintInfo( OutputDevice* pRenderContext, SwTextFrame *pFrame, const SwRect &rPaint )
{
    CtorInitTextSizeInfo( pRenderContext, pFrame, TextFrameIndex(0) );
    aTextFly.CtorInitTextFly( pFrame );
    aPaintRect = rPaint;
    nSpaceIdx = 0;
    pSpaceAdd = nullptr;
    m_pWrongList = nullptr;
    m_pGrammarCheckList = nullptr;
    m_pSmartTags = nullptr;
    pBrushItem = nullptr;
}

SwTextPaintInfo::SwTextPaintInfo( const SwTextPaintInfo &rInf, const OUString* pText )
    : SwTextSizeInfo( rInf, pText )
    , m_pWrongList( rInf.GetpWrongList() )
    , m_pGrammarCheckList( rInf.GetGrammarCheckList() )
    , m_pSmartTags( rInf.GetSmartTags() )
    , pSpaceAdd( rInf.GetpSpaceAdd() ),
      pBrushItem( rInf.GetBrushItem() ),
      aTextFly( rInf.GetTextFly() ),
      aPos( rInf.GetPos() ),
      aPaintRect( rInf.GetPaintRect() ),
      nSpaceIdx( rInf.GetSpaceIdx() )
{ }

SwTextPaintInfo::SwTextPaintInfo( const SwTextPaintInfo &rInf )
    : SwTextSizeInfo( rInf )
    , m_pWrongList( rInf.GetpWrongList() )
    , m_pGrammarCheckList( rInf.GetGrammarCheckList() )
    , m_pSmartTags( rInf.GetSmartTags() )
    , pSpaceAdd( rInf.GetpSpaceAdd() ),
      pBrushItem( rInf.GetBrushItem() ),
      aTextFly( rInf.GetTextFly() ),
      aPos( rInf.GetPos() ),
      aPaintRect( rInf.GetPaintRect() ),
      nSpaceIdx( rInf.GetSpaceIdx() )
{ }

SwTextPaintInfo::SwTextPaintInfo( SwTextFrame *pFrame, const SwRect &rPaint )
{
    CtorInitTextPaintInfo( pFrame->getRootFrame()->GetCurrShell()->GetOut(), pFrame, rPaint );
}

/// Returns if the current background color is dark.
static bool lcl_IsDarkBackground( const SwTextPaintInfo& rInf )
{
    const Color* pCol = rInf.GetFont()->GetBackColor();
    if( ! pCol || COL_TRANSPARENT == *pCol )
    {
        const SvxBrushItem* pItem;
        SwRect aOrigBackRect;
        drawinglayer::attribute::SdrAllFillAttributesHelperPtr aFillAttributes;

        // Consider, that [GetBackgroundBrush(...)] can set <pCol>
        // See implementation in /core/layout/paintfrm.cxx
        // There is a background color, if there is a background brush and
        // its color is *not* "no fill"/"auto fill".
        if( rInf.GetTextFrame()->GetBackgroundBrush( aFillAttributes, pItem, pCol, aOrigBackRect, false, /*bConsiderTextBox=*/false ) )
        {
            if ( !pCol )
                pCol = &pItem->GetColor();

            // Determined color <pCol> can be <COL_TRANSPARENT>. Thus, check it.
            if ( *pCol == COL_TRANSPARENT)
                pCol = nullptr;
        }
        else
            pCol = nullptr;
    }

    if( !pCol )
        pCol = &aGlobalRetoucheColor;

    return pCol->IsDark();
}

void SwTextPaintInfo::DrawText_( const OUString &rText, const SwLinePortion &rPor,
                                TextFrameIndex const nStart, TextFrameIndex const nLength,
                                const bool bKern, const bool bWrong,
                                const bool bSmartTag,
                                const bool bGrammarCheck )
{
    if( !nLength )
        return;

    if( GetFont()->IsBlink() && OnWin() && rPor.Width() )
    {
        // check if accessibility options allow blinking portions:
        const SwViewShell* pSh = GetTextFrame()->getRootFrame()->GetCurrShell();
        if ( pSh && ! pSh->GetAccessibilityOptions()->IsStopAnimatedText() &&
             ! pSh->IsPreview() )
        {
            if( !pBlink )
                pBlink = new SwBlink();

            Point aPoint( aPos );

            if ( GetTextFrame()->IsRightToLeft() )
                GetTextFrame()->SwitchLTRtoRTL( aPoint );

            if ( ComplexTextLayoutFlags::BiDiStrong != GetOut()->GetLayoutMode() )
                aPoint.AdjustX( -(rPor.Width()) );

            if ( GetTextFrame()->IsVertical() )
                GetTextFrame()->SwitchHorizontalToVertical( aPoint );

            pBlink->Insert( aPoint, &rPor, GetTextFrame(), m_pFnt->GetOrientation() );

            if( !pBlink->IsVisible() )
                return;
        }
        else
        {
            delete pBlink;
            pBlink = nullptr;
        }
    }

    // The SwScriptInfo is useless if we are inside a field portion
    SwScriptInfo* pSI = nullptr;
    if ( ! rPor.InFieldGrp() )
        pSI = &GetParaPortion()->GetScriptInfo();

    // in some cases, kana compression is not allowed or suppressed for
    // performance reasons
    sal_uInt16 nComp = 0;
    if ( ! IsMulti() )
        nComp = GetKanaComp();

    bool bCfgIsAutoGrammar = false;
    SvtLinguConfig().GetProperty( UPN_IS_GRAMMAR_AUTO ) >>= bCfgIsAutoGrammar;
    const bool bBullet = OnWin() && GetOpt().IsBlank() && IsNoSymbol();
    const bool bTmpWrong = bWrong && OnWin() && GetOpt().IsOnlineSpell();
    const bool bTmpGrammarCheck = bGrammarCheck && OnWin() && bCfgIsAutoGrammar && GetOpt().IsOnlineSpell();
    const bool bTmpSmart = bSmartTag && OnWin() && !GetOpt().IsPagePreview() && SwSmartTagMgr::Get().IsSmartTagsEnabled();

    OSL_ENSURE( GetParaPortion(), "No paragraph!");
    SwDrawTextInfo aDrawInf( m_pFrame->getRootFrame()->GetCurrShell(), *m_pOut, pSI, rText, nStart, nLength,
                             rPor.Width(), bBullet );

    aDrawInf.SetUnderFnt( m_pUnderFnt );

    const long nSpaceAdd = ( rPor.IsBlankPortion() || rPor.IsDropPortion() ||
                             rPor.InNumberGrp() ) ? 0 : GetSpaceAdd();
    if ( nSpaceAdd )
    {
        TextFrameIndex nCharCnt(0);
        // #i41860# Thai justified alignment needs some
        // additional information:
        aDrawInf.SetNumberOfBlanks( rPor.InTextGrp() ?
                                    static_cast<const SwTextPortion&>(rPor).GetSpaceCnt( *this, nCharCnt ) :
                                    TextFrameIndex(0) );
    }

    aDrawInf.SetSpace( nSpaceAdd );
    aDrawInf.SetKanaComp( nComp );

    // the font is used to identify the current script via nActual
    aDrawInf.SetFont( m_pFnt );
    // the frame is used to identify the orientation
    aDrawInf.SetFrame( GetTextFrame() );
    // we have to know if the paragraph should snap to grid
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    // for underlining we must know when not to add extra space behind
    // a character in justified mode
    aDrawInf.SetSpaceStop( ! rPor.GetNextPortion() ||
                             rPor.GetNextPortion()->InFixMargGrp() ||
                             rPor.GetNextPortion()->IsHolePortion() );

    // Draw text next to the left border
    Point aFontPos(aPos);
    if( m_pFnt->GetLeftBorder() && !static_cast<const SwTextPortion&>(rPor).GetJoinBorderWithPrev() )
    {
        const sal_uInt16 nLeftBorderSpace = m_pFnt->GetLeftBorderSpace();
        if ( GetTextFrame()->IsRightToLeft() )
        {
            aFontPos.AdjustX( -nLeftBorderSpace );
        }
        else
        {
            switch( m_pFnt->GetOrientation(GetTextFrame()->IsVertical()) )
            {
                case 0 :
                    aFontPos.AdjustX(nLeftBorderSpace );
                    break;
                case 900 :
                    aFontPos.AdjustY( -nLeftBorderSpace );
                    break;
                case 1800 :
                    aFontPos.AdjustX( -nLeftBorderSpace );
                    break;
                case 2700 :
                    aFontPos.AdjustY(nLeftBorderSpace );
                    break;
            }
        }
        if( aFontPos.X() < 0 )
            aFontPos.setX( 0 );
        if( aFontPos.Y() < 0 )
            aFontPos.setY( 0 );
    }

    if( GetTextFly().IsOn() )
    {
        // aPos needs to be the TopLeft, because we cannot calculate the
        // ClipRects otherwise
        const Point aPoint( aFontPos.X(), aFontPos.Y() - rPor.GetAscent() );
        const Size aSize( rPor.Width(), rPor.Height() );
        aDrawInf.SetPos( aPoint );
        aDrawInf.SetSize( aSize );
        aDrawInf.SetAscent( rPor.GetAscent() );
        aDrawInf.SetKern( bKern ? rPor.Width() : 0 );
        aDrawInf.SetWrong( bTmpWrong ? m_pWrongList : nullptr );
        aDrawInf.SetGrammarCheck( bTmpGrammarCheck ? m_pGrammarCheckList : nullptr );
        aDrawInf.SetSmartTags( bTmpSmart ? m_pSmartTags : nullptr );
        GetTextFly().DrawTextOpaque( aDrawInf );
    }
    else
    {
        aDrawInf.SetPos( aFontPos );
        if( bKern )
            m_pFnt->DrawStretchText_( aDrawInf );
        else
        {
            aDrawInf.SetWrong( bTmpWrong ? m_pWrongList : nullptr );
            aDrawInf.SetGrammarCheck( bTmpGrammarCheck ? m_pGrammarCheckList : nullptr );
            aDrawInf.SetSmartTags( bTmpSmart ? m_pSmartTags : nullptr );
            m_pFnt->DrawText_( aDrawInf );
        }
    }
}

void SwTextPaintInfo::CalcRect( const SwLinePortion& rPor,
                               SwRect* pRect, SwRect* pIntersect,
                               const bool bInsideBox ) const
{
    Size aSize( rPor.Width(), rPor.Height() );
    if( rPor.IsHangingPortion() )
        aSize.setWidth( static_cast<const SwHangingPortion&>(rPor).GetInnerWidth() );
    if( rPor.InSpaceGrp() && GetSpaceAdd() )
    {
        SwTwips nAdd = rPor.CalcSpacing( GetSpaceAdd(), *this );
        if( rPor.InFieldGrp() && GetSpaceAdd() < 0 && nAdd )
            nAdd += GetSpaceAdd() / SPACING_PRECISION_FACTOR;
        aSize.AdjustWidth(nAdd );
    }

    Point aPoint;

    if( IsRotated() )
    {
        long nTmp = aSize.Width();
        aSize.setWidth( aSize.Height() );
        aSize.setHeight( nTmp );
        if ( 1 == GetDirection() )
        {
            aPoint.setX( X() - rPor.GetAscent() );
            aPoint.setY( Y() - aSize.Height() );
        }
        else
        {
            aPoint.setX( X() - rPor.Height() + rPor.GetAscent() );
            aPoint.setY( Y() );
        }
    }
    else
    {
        aPoint.setX( X() );
        if (GetTextFrame()->IsVertLR() && !GetTextFrame()->IsVertLRBT())
            aPoint.setY( Y() - rPor.Height() + rPor.GetAscent() );
        else
            aPoint.setY( Y() - rPor.GetAscent() );
    }

    // Adjust x coordinate if we are inside a bidi portion
    const bool bFrameDir = GetTextFrame()->IsRightToLeft();
    const bool bCounterDir = ( !bFrameDir && DIR_RIGHT2LEFT == GetDirection() ) ||
                             (  bFrameDir && DIR_LEFT2RIGHT == GetDirection() );

    if ( bCounterDir )
        aPoint.AdjustX( -(aSize.Width()) );

    SwRect aRect( aPoint, aSize );

    if ( GetTextFrame()->IsRightToLeft() )
        GetTextFrame()->SwitchLTRtoRTL( aRect );

    if ( GetTextFrame()->IsVertical() )
        GetTextFrame()->SwitchHorizontalToVertical( aRect );

    if( bInsideBox && rPor.InTextGrp() )
    {
        const bool bJoinWithPrev =
            static_cast<const SwTextPortion&>(rPor).GetJoinBorderWithPrev();
        const bool bJoinWithNext =
            static_cast<const SwTextPortion&>(rPor).GetJoinBorderWithNext();
        const bool bIsVert = GetTextFrame()->IsVertical();
        const bool bIsVertLRBT = GetTextFrame()->IsVertLRBT();
        aRect.Top(aRect.Top()
                  + GetFont()->CalcShadowSpace(SvxShadowItemSide::TOP, bIsVert, bIsVertLRBT,
                                               bJoinWithPrev, bJoinWithNext));
        aRect.Bottom(aRect.Bottom()
                     - GetFont()->CalcShadowSpace(SvxShadowItemSide::BOTTOM, bIsVert, bIsVertLRBT,
                                                  bJoinWithPrev, bJoinWithNext));
        aRect.Left(aRect.Left()
                   + GetFont()->CalcShadowSpace(SvxShadowItemSide::LEFT, bIsVert, bIsVertLRBT,
                                                bJoinWithPrev, bJoinWithNext));
        aRect.Right(aRect.Right()
                    - GetFont()->CalcShadowSpace(SvxShadowItemSide::RIGHT, bIsVert, bIsVertLRBT,
                                                 bJoinWithPrev, bJoinWithNext));
    }

    if ( pRect )
        *pRect = aRect;

    if( aRect.HasArea() && pIntersect )
    {
        ::SwAlignRect( aRect, GetVsh(), GetOut() );

        if ( GetOut()->IsClipRegion() )
        {
            SwRect aClip( GetOut()->GetClipRegion().GetBoundRect() );
            aRect.Intersection( aClip );
        }

        *pIntersect = aRect;
    }
}

/**
 * Draws a special portion
 * E.g.: line break portion, tab portion
 *
 * @param rPor The portion
 * @param rRect The rectangle surrounding the character
 * @param rCol Specify a color for the character
 * @param bCenter Draw the character centered, otherwise left aligned
 * @param bRotate Rotate the character if character rotation is set
 */
static void lcl_DrawSpecial( const SwTextPaintInfo& rInf, const SwLinePortion& rPor,
                      SwRect& rRect, const Color& rCol, sal_Unicode cChar,
                      sal_uInt8 nOptions )
{
    bool bCenter = 0 != ( nOptions & DRAW_SPECIAL_OPTIONS_CENTER );
    bool bRotate = 0 != ( nOptions & DRAW_SPECIAL_OPTIONS_ROTATE );

    // rRect is given in absolute coordinates
    if ( rInf.GetTextFrame()->IsRightToLeft() )
        rInf.GetTextFrame()->SwitchRTLtoLTR( rRect );
    if ( rInf.GetTextFrame()->IsVertical() )
        rInf.GetTextFrame()->SwitchVerticalToHorizontal( rRect );

    const SwFont* pOldFnt = rInf.GetFont();

    // Font is generated only once:
    static SwFont s_aFnt = [&]()
    {
        SwFont tmp( *pOldFnt );
        tmp.SetFamily( FAMILY_DONTKNOW, tmp.GetActual() );
        tmp.SetName( numfunc::GetDefBulletFontname(), tmp.GetActual() );
        tmp.SetStyleName(OUString(), tmp.GetActual());
        tmp.SetCharSet( RTL_TEXTENCODING_SYMBOL, tmp.GetActual() );
        return tmp;
    }();

    // Some of the current values are set at the font:
    if ( ! bRotate )
        s_aFnt.SetVertical( 0, rInf.GetTextFrame()->IsVertical() );
    else
        s_aFnt.SetVertical( pOldFnt->GetOrientation() );

    s_aFnt.SetColor(rCol);

    Size aFontSize( 0, SPECIAL_FONT_HEIGHT );
    s_aFnt.SetSize( aFontSize, s_aFnt.GetActual() );

    const_cast<SwTextPaintInfo&>(rInf).SetFont( &s_aFnt );

    // The maximum width depends on the current orientation
    const sal_uInt16 nDir = s_aFnt.GetOrientation( rInf.GetTextFrame()->IsVertical() );
    SwTwips nMaxWidth;
    if (nDir == 900 || nDir == 2700)
        nMaxWidth = rRect.Height();
    else
    {
        assert(nDir == 0); //Unknown direction set at font
        nMaxWidth = rRect.Width();
    }

    // check if char fits into rectangle
    const OUString aTmp( cChar );
    aFontSize = rInf.GetTextSize( aTmp ).SvLSize();
    while ( aFontSize.Width() > nMaxWidth )
    {
        SwTwips nFactor = ( 100 * aFontSize.Width() ) / nMaxWidth;
        const SwTwips nOldWidth = aFontSize.Width();

        // new height for font
        const SwFontScript nAct = s_aFnt.GetActual();
        aFontSize.setHeight( ( 100 * s_aFnt.GetSize( nAct ).Height() ) / nFactor );
        aFontSize.setWidth( ( 100 * s_aFnt.GetSize( nAct).Width() ) / nFactor );

        if ( !aFontSize.Width() && !aFontSize.Height() )
            break;

        s_aFnt.SetSize( aFontSize, nAct );

        aFontSize = rInf.GetTextSize( aTmp ).SvLSize();

        if ( aFontSize.Width() >= nOldWidth )
            break;
    }

    const Point aOldPos( rInf.GetPos() );

    // adjust values so that tab is vertically and horizontally centered
    SwTwips nX = rRect.Left();
    SwTwips nY = rRect.Top();
    switch ( nDir )
    {
    case 0 :
        if ( bCenter )
            nX += ( rRect.Width() - aFontSize.Width() ) / 2;
        nY += ( rRect.Height() - aFontSize.Height() ) / 2 + rInf.GetAscent();
        break;
    case 900 :
        if ( bCenter )
            nX += ( rRect.Width() - aFontSize.Height() ) / 2 + rInf.GetAscent();
        nY += ( rRect.Height() + aFontSize.Width() ) / 2;
        break;
    case 2700 :
        if ( bCenter )
            nX += ( rRect.Width() + aFontSize.Height() ) / 2 - rInf.GetAscent();
        nY += ( rRect.Height() - aFontSize.Width() ) / 2;
        break;
    }

    Point aTmpPos( nX, nY );
    const_cast<SwTextPaintInfo&>(rInf).SetPos( aTmpPos );
    sal_uInt16 nOldWidth = rPor.Width();
    const_cast<SwLinePortion&>(rPor).Width( static_cast<sal_uInt16>(aFontSize.Width()) );
    rInf.DrawText( aTmp, rPor );
    const_cast<SwLinePortion&>(rPor).Width( nOldWidth );
    const_cast<SwTextPaintInfo&>(rInf).SetFont( const_cast<SwFont*>(pOldFnt) );
    const_cast<SwTextPaintInfo&>(rInf).SetPos( aOldPos );
}

void SwTextPaintInfo::DrawRect( const SwRect &rRect, bool bRetouche ) const
{
    if ( OnWin() || !bRetouche )
    {
        if( aTextFly.IsOn() )
            const_cast<SwTextPaintInfo*>(this)->GetTextFly().
                DrawFlyRect( m_pOut, rRect );
        else
            m_pOut->DrawRect( rRect.SVRect() );
    }
}

void SwTextPaintInfo::DrawTab( const SwLinePortion &rPor ) const
{
    if( OnWin() )
    {
        SwRect aRect;
        CalcRect( rPor, &aRect );

        if ( ! aRect.HasArea() )
            return;

        const sal_Unicode cChar = GetTextFrame()->IsRightToLeft() ? CHAR_TAB_RTL : CHAR_TAB;
        const sal_uInt8 nOptions = DRAW_SPECIAL_OPTIONS_CENTER | DRAW_SPECIAL_OPTIONS_ROTATE;

        lcl_DrawSpecial( *this, rPor, aRect, NON_PRINTING_CHARACTER_COLOR, cChar, nOptions );
    }
}

void SwTextPaintInfo::DrawLineBreak( const SwLinePortion &rPor ) const
{
    if( OnWin() )
    {
        sal_uInt16 nOldWidth = rPor.Width();
        const_cast<SwLinePortion&>(rPor).Width( LINE_BREAK_WIDTH );

        SwRect aRect;
        CalcRect( rPor, &aRect );

        if( aRect.HasArea() )
        {
            const sal_Unicode cChar = GetTextFrame()->IsRightToLeft() ?
                                      CHAR_LINEBREAK_RTL : CHAR_LINEBREAK;
            const sal_uInt8 nOptions = 0;

            lcl_DrawSpecial( *this, rPor, aRect, NON_PRINTING_CHARACTER_COLOR, cChar, nOptions );
        }

        const_cast<SwLinePortion&>(rPor).Width( nOldWidth );
    }
}

void SwTextPaintInfo::DrawRedArrow( const SwLinePortion &rPor ) const
{
    Size aSize( SPECIAL_FONT_HEIGHT, SPECIAL_FONT_HEIGHT );
    SwRect aRect( static_cast<const SwArrowPortion&>(rPor).GetPos(), aSize );
    sal_Unicode cChar;
    if( static_cast<const SwArrowPortion&>(rPor).IsLeft() )
    {
        aRect.Pos().AdjustY(20 - GetAscent() );
        aRect.Pos().AdjustX(20 );
        if( aSize.Height() > rPor.Height() )
            aRect.Height( rPor.Height() );
        cChar = CHAR_LEFT_ARROW;
    }
    else
    {
        if( aSize.Height() > rPor.Height() )
            aRect.Height( rPor.Height() );
        aRect.Pos().AdjustY( -(aRect.Height() + 20) );
        aRect.Pos().AdjustX( -(aRect.Width() + 20) );
        cChar = CHAR_RIGHT_ARROW;
    }

    if ( GetTextFrame()->IsVertical() )
        GetTextFrame()->SwitchHorizontalToVertical( aRect );

    if( aRect.HasArea() )
    {
        const sal_uInt8 nOptions = 0;
        lcl_DrawSpecial( *this, rPor, aRect, COL_LIGHTRED, cChar, nOptions );
    }
}

void SwTextPaintInfo::DrawPostIts( bool bScript ) const
{
    if( !OnWin() || !m_pOpt->IsPostIts() )
        return;

    Size aSize;
    Point aTmp;

    const sal_uInt16 nPostItsWidth = SwViewOption::GetPostItsWidth( GetOut() );
    const sal_uInt16 nFontHeight = m_pFnt->GetHeight( m_pVsh, *GetOut() );
    const sal_uInt16 nFontAscent = m_pFnt->GetAscent( m_pVsh, *GetOut() );

    switch ( m_pFnt->GetOrientation( GetTextFrame()->IsVertical() ) )
    {
    case 0 :
        aSize.setWidth( nPostItsWidth );
        aSize.setHeight( nFontHeight );
        aTmp.setX( aPos.X() );
        aTmp.setY( aPos.Y() - nFontAscent );
        break;
    case 900 :
        aSize.setHeight( nPostItsWidth );
        aSize.setWidth( nFontHeight );
        aTmp.setX( aPos.X() - nFontAscent );
        aTmp.setY( aPos.Y() );
        break;
    case 2700 :
        aSize.setHeight( nPostItsWidth );
        aSize.setWidth( nFontHeight );
        aTmp.setX( aPos.X() - nFontHeight +
                              nFontAscent );
        aTmp.setY( aPos.Y() );
        break;
    }

    SwRect aTmpRect( aTmp, aSize );

    if ( GetTextFrame()->IsRightToLeft() )
        GetTextFrame()->SwitchLTRtoRTL( aTmpRect );

    if ( GetTextFrame()->IsVertical() )
        GetTextFrame()->SwitchHorizontalToVertical( aTmpRect );

    const tools::Rectangle aRect( aTmpRect.SVRect() );
    SwViewOption::PaintPostIts( const_cast<OutputDevice*>(GetOut()), aRect, bScript );

}

void SwTextPaintInfo::DrawCheckBox(const SwFieldFormCheckboxPortion &rPor, bool bChecked) const
{
    SwRect aIntersect;
    CalcRect( rPor, &aIntersect );
    if ( aIntersect.HasArea() )
    {
        if (OnWin() && SwViewOption::IsFieldShadings() &&
                !GetOpt().IsPagePreview())
        {
            OutputDevice* pOut = const_cast<OutputDevice*>(GetOut());
            pOut->Push( PushFlags::LINECOLOR | PushFlags::FILLCOLOR );
            pOut->SetFillColor( SwViewOption::GetFieldShadingsColor() );
            pOut->SetLineColor();
            pOut->DrawRect( aIntersect.SVRect() );
            pOut->Pop();
        }
        const int delta=10;
        tools::Rectangle r(aIntersect.Left()+delta, aIntersect.Top()+delta, aIntersect.Right()-delta, aIntersect.Bottom()-delta);
        m_pOut->Push( PushFlags::LINECOLOR | PushFlags::FILLCOLOR );
        m_pOut->SetLineColor( Color(0, 0, 0));
        m_pOut->SetFillColor();
        m_pOut->DrawRect( r );
        if (bChecked)
        {
            m_pOut->DrawLine(r.TopLeft(), r.BottomRight());
            m_pOut->DrawLine(r.TopRight(), r.BottomLeft());
        }
        m_pOut->Pop();
    }
}

void SwTextPaintInfo::DrawBackground( const SwLinePortion &rPor ) const
{
    OSL_ENSURE( OnWin(), "SwTextPaintInfo::DrawBackground: printer pollution ?" );

    SwRect aIntersect;
    CalcRect( rPor, nullptr, &aIntersect, true );

    if ( aIntersect.HasArea() )
    {
        OutputDevice* pOut = const_cast<OutputDevice*>(GetOut());
        pOut->Push( PushFlags::LINECOLOR | PushFlags::FILLCOLOR );

        // For dark background we do not want to have a filled rectangle
        if ( GetVsh() && GetVsh()->GetWin() && lcl_IsDarkBackground( *this ) )
        {
            pOut->SetLineColor( SwViewOption::GetFontColor() );
        }
        else
        {
            pOut->SetFillColor( SwViewOption::GetFieldShadingsColor() );
            pOut->SetLineColor();
        }

        DrawRect( aIntersect, true );
        pOut->Pop();
    }
}

void SwTextPaintInfo::DrawBackBrush( const SwLinePortion &rPor ) const
{
    {
        SwRect aIntersect;
        CalcRect( rPor, &aIntersect, nullptr, true );
        if(aIntersect.HasArea())
        {
            SwPosition const aPosition(m_pFrame->MapViewToModelPos(GetIdx()));
            const ::sw::mark::IMark* pFieldmark =
                m_pFrame->GetDoc().getIDocumentMarkAccess()->getFieldmarkFor(aPosition);
            bool bIsStartMark = (TextFrameIndex(1) == GetLen()
                    && CH_TXT_ATR_FIELDSTART == GetText()[sal_Int32(GetIdx())]);
            if(pFieldmark) {
                SAL_INFO("sw.core", "Found Fieldmark " << pFieldmark->ToString());
            }
            if(bIsStartMark)
                SAL_INFO("sw.core", "Found StartMark");
            if (OnWin() && (pFieldmark!=nullptr || bIsStartMark) &&
                    SwViewOption::IsFieldShadings() &&
                    !GetOpt().IsPagePreview())
            {
                OutputDevice* pOutDev = const_cast<OutputDevice*>(GetOut());
                pOutDev->Push( PushFlags::LINECOLOR | PushFlags::FILLCOLOR );
                pOutDev->SetFillColor( SwViewOption::GetFieldShadingsColor() );
                pOutDev->SetLineColor( );
                pOutDev->DrawRect( aIntersect.SVRect() );
                pOutDev->Pop();
            }
        }
    }

    SwRect aIntersect;
    CalcRect( rPor, nullptr, &aIntersect, true );

    if ( aIntersect.HasArea() )
    {
        OutputDevice* pTmpOut = const_cast<OutputDevice*>(GetOut());

        // #i16816# tagged pdf support
        SwTaggedPDFHelper aTaggedPDFHelper( nullptr, nullptr, nullptr, *pTmpOut );

        Color aFillColor;

        if( m_pFnt->GetHighlightColor() != COL_TRANSPARENT )
        {
            aFillColor = m_pFnt->GetHighlightColor();
        }
        else
        {
            if( !m_pFnt->GetBackColor() )
                return;
            aFillColor = *m_pFnt->GetBackColor();
        }

        // tdf#104349 do not highlight portions of space chars before end of line if the compatibility option is enabled
        // for LTR mode only
        if ( !GetTextFrame()->IsRightToLeft() )
        {
            if (GetTextFrame()->GetDoc().getIDocumentSettingAccess().get(DocumentSettingId::MS_WORD_COMP_TRAILING_BLANKS))
            {
                bool           draw = false;
                bool           full = false;
                SwLinePortion *pPos = const_cast<SwLinePortion *>(&rPor);
                TextFrameIndex nIdx = GetIdx();
                TextFrameIndex nLen;

                do
                {
                    nLen = pPos->GetLen();
                    for (TextFrameIndex i = nIdx; i < (nIdx + nLen); ++i)
                    {
                        if (i < TextFrameIndex(GetText().getLength())
                            && GetText()[sal_Int32(i)] == CH_TXTATR_NEWLINE)
                        {
                            if ( i >= (GetIdx() + rPor.GetLen()) )
                            {
                                goto drawcontinue;
                            }
                        }
                        if (i >= TextFrameIndex(GetText().getLength())
                            || GetText()[sal_Int32(i)] != CH_BLANK)
                        {
                            draw = true;
                            if ( i >= (GetIdx() + rPor.GetLen()) )
                            {
                                full = true;
                                goto drawcontinue;
                            }
                        }
                    }
                    nIdx += nLen;
                    pPos = pPos->GetNextPortion();
                } while ( pPos );

            drawcontinue:

                if ( !draw )
                    return;

                if ( !full )
                {
                    pPos = const_cast<SwLinePortion *>(&rPor);
                    nIdx = GetIdx();

                    nLen = pPos->GetLen();
                    for (TextFrameIndex i = nIdx + nLen - TextFrameIndex(1);
                            i >= nIdx; --i)
                    {
                        if (i < TextFrameIndex(GetText().getLength())
                            && GetText()[sal_Int32(i)] == CH_TXTATR_NEWLINE)
                        {
                            continue;
                        }
                        if (i >= TextFrameIndex(GetText().getLength())
                            || GetText()[sal_Int32(i)] != CH_BLANK)
                        {
                            sal_uInt16 nOldWidth = rPor.Width();
                            sal_uInt16 nNewWidth = GetTextSize(m_pOut, nullptr,
                                GetText(), nIdx, (i + TextFrameIndex(1) - nIdx)).Width();

                            const_cast<SwLinePortion&>(rPor).Width( nNewWidth );
                            CalcRect( rPor, nullptr, &aIntersect, true );
                            const_cast<SwLinePortion&>(rPor).Width( nOldWidth );

                            if ( !aIntersect.HasArea() )
                            {
                                return;
                            }

                            break;
                        }
                    }
                }
            }
        }

        pTmpOut->Push( PushFlags::LINECOLOR | PushFlags::FILLCOLOR );

        pTmpOut->SetFillColor(aFillColor);
        pTmpOut->SetLineColor();

        DrawRect( aIntersect, false );

        pTmpOut->Pop();
    }
}

void SwTextPaintInfo::DrawBorder( const SwLinePortion &rPor ) const
{
    SwRect aDrawArea;
    CalcRect( rPor, &aDrawArea );
    if ( aDrawArea.HasArea() )
    {
        PaintCharacterBorder(*m_pFnt, aDrawArea, GetTextFrame()->IsVertical(),
                             GetTextFrame()->IsVertLRBT(), rPor.GetJoinBorderWithPrev(),
                             rPor.GetJoinBorderWithNext());
    }
}

void SwTextPaintInfo::DrawViewOpt( const SwLinePortion &rPor,
                                   PortionType nWhich ) const
{
    if( OnWin() && !IsMulti() )
    {
        bool bDraw = false;
        switch( nWhich )
        {
        case PortionType::Footnote:
        case PortionType::QuoVadis:
        case PortionType::Number:
        case PortionType::Field:
        case PortionType::Hidden:
        case PortionType::Tox:
        case PortionType::Ref:
        case PortionType::Meta:
        case PortionType::ControlChar:
            if ( !GetOpt().IsPagePreview()
                 && !GetOpt().IsReadonly()
                 && SwViewOption::IsFieldShadings()
                 && ( PortionType::Number != nWhich
                      || m_pFrame->GetTextNodeForParaProps()->HasMarkedLabel())) // #i27615#
            {
                bDraw = true;
            }
            break;
        case PortionType::InputField:
            // input field shading also in read-only mode
            if ( !GetOpt().IsPagePreview()
                 && SwViewOption::IsFieldShadings() )
            {
                bDraw = true;
            }
            break;
        case PortionType::Table:
            if ( GetOpt().IsTab() )     bDraw = true;
            break;
        case PortionType::SoftHyphen:
            if ( GetOpt().IsSoftHyph() )bDraw = true;
            break;
        case PortionType::Blank:
            if ( GetOpt().IsHardBlank())bDraw = true;
            break;
        default:
            {
                OSL_ENSURE( false, "SwTextPaintInfo::DrawViewOpt: don't know how to draw this" );
                break;
            }
        }
        if ( bDraw )
            DrawBackground( rPor );
    }
}

static void lcl_InitHyphValues( PropertyValues &rVals,
            sal_Int16 nMinLeading, sal_Int16 nMinTrailing )
{
    sal_Int32 nLen = rVals.getLength();

    if (0 == nLen)  // yet to be initialized?
    {
        rVals.realloc( 2 );
        PropertyValue *pVal = rVals.getArray();

        pVal[0].Name    = UPN_HYPH_MIN_LEADING;
        pVal[0].Handle  = UPH_HYPH_MIN_LEADING;
        pVal[0].Value   <<= nMinLeading;

        pVal[1].Name    = UPN_HYPH_MIN_TRAILING;
        pVal[1].Handle  = UPH_HYPH_MIN_TRAILING;
        pVal[1].Value   <<= nMinTrailing;
    }
    else if (2 == nLen) // already initialized once?
    {
        PropertyValue *pVal = rVals.getArray();
        pVal[0].Value <<= nMinLeading;
        pVal[1].Value <<= nMinTrailing;
    }
    else {
        OSL_FAIL( "unexpected size of sequence" );
    }
}

const PropertyValues & SwTextFormatInfo::GetHyphValues() const
{
    OSL_ENSURE( 2 == m_aHyphVals.getLength(),
            "hyphenation values not yet initialized" );
    return m_aHyphVals;
}

bool SwTextFormatInfo::InitHyph( const bool bAutoHyphen )
{
    const SwAttrSet& rAttrSet = GetTextFrame()->GetTextNodeForParaProps()->GetSwAttrSet();
    SetHanging( rAttrSet.GetHangingPunctuation().GetValue() );
    SetScriptSpace( rAttrSet.GetScriptSpace().GetValue() );
    SetForbiddenChars( rAttrSet.GetForbiddenRule().GetValue() );
    const SvxHyphenZoneItem &rAttr = rAttrSet.GetHyphenZone();
    MaxHyph() = rAttr.GetMaxHyphens();
    const bool bAuto = bAutoHyphen || rAttr.IsHyphen();
    if( bAuto || m_bInterHyph )
    {
        const sal_Int16 nMinimalLeading  = std::max(rAttr.GetMinLead(), sal_uInt8(2));
        const sal_Int16 nMinimalTrailing = rAttr.GetMinTrail();
        lcl_InitHyphValues( m_aHyphVals, nMinimalLeading, nMinimalTrailing);
    }
    return bAuto;
}

void SwTextFormatInfo::CtorInitTextFormatInfo( OutputDevice* pRenderContext, SwTextFrame *pNewFrame, const bool bNewInterHyph,
                                const bool bNewQuick, const bool bTst )
{
    CtorInitTextPaintInfo( pRenderContext, pNewFrame, SwRect() );

    m_bQuick = bNewQuick;
    m_bInterHyph = bNewInterHyph;

    //! needs to be done in this order
    m_bAutoHyph = InitHyph();

    m_bIgnoreFly = false;
    m_bFakeLineStart = false;
    m_bShift = false;
    m_bDropInit = false;
    m_bTestFormat = bTst;
    m_nLeft = 0;
    m_nRight = 0;
    m_nFirst = 0;
    m_nRealWidth = 0;
    m_nForcedLeftMargin = 0;
    m_pRest = nullptr;
    m_nLineHeight = 0;
    m_nLineNetHeight = 0;
    SetLineStart(TextFrameIndex(0));

    SvtCTLOptions::TextNumerals const nTextNumerals(
            SW_MOD()->GetCTLOptions().GetCTLTextNumerals());
    // cannot cache for NUMERALS_CONTEXT because we need to know the string
    // for the whole paragraph now
    if (nTextNumerals != SvtCTLOptions::NUMERALS_CONTEXT)
    {
        // set digit mode to what will be used later to get same results
        SwDigitModeModifier const m(*m_pRef, LANGUAGE_NONE /*dummy*/);
        assert(m_pRef->GetDigitLanguage() != LANGUAGE_NONE);
        SetCachedVclData(OutputDevice::CreateTextLayoutCache(*m_pText));
    }

    Init();
}

/**
 * If the Hyphenator returns ERROR or the language is set to NOLANGUAGE
 * we do not hyphenate.
 * Else, we always hyphenate if we do interactive hyphenation.
 * If we do not do interactive hyphenation, we only hyphenate if ParaFormat is
 * set to automatic hyphenation.
 */
bool SwTextFormatInfo::IsHyphenate() const
{
    if( !m_bInterHyph && !m_bAutoHyph )
        return false;

    LanguageType eTmp = GetFont()->GetLanguage();
    if( LANGUAGE_DONTKNOW == eTmp || LANGUAGE_NONE == eTmp )
        return false;

    uno::Reference< XHyphenator > xHyph = ::GetHyphenator();
    if (!xHyph.is())
        return false;

    if (m_bInterHyph)
        SvxSpellWrapper::CheckHyphLang( xHyph, eTmp );

    if (!xHyph->hasLocale(g_pBreakIt->GetLocale(eTmp)))
    {
        // TODO: Add an infobar for this case, tdf#128191
        SAL_WARN("sw", "missing hyphenation package for locale: "
                           << g_pBreakIt->GetLocale(eTmp).Language);
    }

    return xHyph->hasLocale( g_pBreakIt->GetLocale(eTmp) );
}

const SwFormatDrop *SwTextFormatInfo::GetDropFormat() const
{
    const SwFormatDrop *pDrop = &GetTextFrame()->GetTextNodeForParaProps()->GetSwAttrSet().GetDrop();
    if( 1 >= pDrop->GetLines() ||
        ( !pDrop->GetChars() && !pDrop->GetWholeWord() ) )
        pDrop = nullptr;
    return pDrop;
}

void SwTextFormatInfo::Init()
{
    // Not initialized: pRest, nLeft, nRight, nFirst, nRealWidth
    X(0);
    m_bArrowDone = m_bFull = m_bFootnoteDone = m_bErgoDone = m_bNumDone = m_bNoEndHyph =
        m_bNoMidHyph = m_bStop = m_bNewLine = m_bUnderflow = m_bTabOverflow = false;

    // generally we do not allow number portions in follows, except...
    if ( GetTextFrame()->IsFollow() )
    {
        const SwTextFrame* pMaster = GetTextFrame()->FindMaster();
        OSL_ENSURE(pMaster, "pTextFrame without Master");
        const SwLinePortion* pTmpPara = pMaster ? pMaster->GetPara() : nullptr;

        // there is a master for this follow and the master does not have
        // any contents (especially it does not have a number portion)
        m_bNumDone = ! pTmpPara ||
                   ! static_cast<const SwParaPortion*>(pTmpPara)->GetFirstPortion()->IsFlyPortion();
    }

    m_pRoot = nullptr;
    m_pLast = nullptr;
    m_pFly = nullptr;
    m_pLastTab = nullptr;
    m_pUnderflow = nullptr;
    m_cTabDecimal = 0;
    m_nWidth = m_nRealWidth;
    m_nForcedLeftMargin = 0;
    m_nSoftHyphPos = TextFrameIndex(0);
    m_nUnderScorePos = TextFrameIndex(COMPLETE_STRING);
    m_cHookChar = 0;
    SetIdx(TextFrameIndex(0));
    SetLen(TextFrameIndex(GetText().getLength()));
    SetPaintOfst(0);
}

SwTextFormatInfo::SwTextFormatInfo(OutputDevice* pRenderContext, SwTextFrame *pFrame, const bool bInterHyphL,
                                   const bool bQuickL, const bool bTst)
{
    CtorInitTextFormatInfo(pRenderContext, pFrame, bInterHyphL, bQuickL, bTst);
}

/**
 * There are a few differences between a copy constructor
 * and the following constructor for multi-line formatting.
 * The root is the first line inside the multi-portion,
 * the line start is the actual position in the text,
 * the line width is the rest width from the surrounding line
 * and the bMulti and bFirstMulti-flag has to be set correctly.
 */
SwTextFormatInfo::SwTextFormatInfo( const SwTextFormatInfo& rInf,
    SwLineLayout& rLay, SwTwips nActWidth ) :
    SwTextPaintInfo( rInf ),
    m_pRoot(&rLay),
    m_pLast(&rLay),
    m_pFly(nullptr),
    m_pUnderflow(nullptr),
    m_pRest(nullptr),
    m_pLastTab(nullptr),
    m_nSoftHyphPos(TextFrameIndex(0)),
    m_nLineStart(rInf.GetIdx()),
    m_nUnderScorePos(TextFrameIndex(COMPLETE_STRING)),
    m_nLeft(rInf.m_nLeft),
    m_nRight(rInf.m_nRight),
    m_nFirst(rInf.m_nLeft),
    m_nRealWidth(sal_uInt16(nActWidth)),
    m_nWidth(m_nRealWidth),
    m_nLineHeight(0),
    m_nLineNetHeight(0),
    m_nForcedLeftMargin(0),
    m_bFull(false),
    m_bFootnoteDone(true),
    m_bErgoDone(true),
    m_bNumDone(true),
    m_bArrowDone(true),
    m_bStop(false),
    m_bNewLine(true),
    m_bShift(false),
    m_bUnderflow(false),
    m_bInterHyph(false),
    m_bAutoHyph(false),
    m_bDropInit(false),
    m_bQuick(rInf.m_bQuick),
    m_bNoEndHyph(false),
    m_bNoMidHyph(false),
    m_bIgnoreFly(false),
    m_bFakeLineStart(false),
    m_bTabOverflow( false ),
    m_bTestFormat(rInf.m_bTestFormat),
    m_cTabDecimal(0),
    m_cHookChar(0),
    m_nMaxHyph(0)
{
    SetMulti( true );
    SetFirstMulti( rInf.IsFirstMulti() );
}

bool SwTextFormatInfo::CheckFootnotePortion_( SwLineLayout const * pCurr )
{
    const sal_uInt16 nHeight = pCurr->GetRealHeight();
    for( SwLinePortion *pPor = pCurr->GetNextPortion(); pPor; pPor = pPor->GetNextPortion() )
    {
        if( pPor->IsFootnotePortion() && nHeight > static_cast<SwFootnotePortion*>(pPor)->Orig() )
        {
            SetLineHeight( nHeight );
            SetLineNetHeight( pCurr->Height() );
            return true;
        }
    }
    return false;
}

TextFrameIndex SwTextFormatInfo::ScanPortionEnd(TextFrameIndex const nStart,
                                                TextFrameIndex const nEnd)
{
    m_cHookChar = 0;
    TextFrameIndex i = nStart;

    // Used for decimal tab handling:
    const sal_Unicode cTabDec = GetLastTab() ? GetTabDecimal() : 0;
    const sal_Unicode cThousandSep  = ',' == cTabDec ? '.' : ',';

    // #i45951# German (Switzerland) uses ' as thousand separator
    const sal_Unicode cThousandSep2 = ',' == cTabDec ? '.' : '\'';

    bool bNumFound = false;
    const bool bTabCompat = GetTextFrame()->GetDoc().getIDocumentSettingAccess().get(DocumentSettingId::TAB_COMPAT);

    for( ; i < nEnd; ++i )
    {
        const sal_Unicode cPos = GetChar( i );
        switch( cPos )
        {
        case CH_TXTATR_BREAKWORD:
        case CH_TXTATR_INWORD:
            if( !HasHint( i ))
                break;
            [[fallthrough]];

        case CHAR_SOFTHYPHEN:
        case CHAR_HARDHYPHEN:
        case CHAR_HARDBLANK:
        case CH_TAB:
        case CH_BREAK:
        case CHAR_ZWSP :
        case CHAR_ZWNBSP :
            m_cHookChar = cPos;
            return i;

        case CHAR_UNDERSCORE:
            if (TextFrameIndex(COMPLETE_STRING) == m_nUnderScorePos)
                m_nUnderScorePos = i;
            break;

        default:
            if ( cTabDec )
            {
                if( cTabDec == cPos )
                {
                    OSL_ENSURE( cPos, "Unexpected end of string" );
                    if( cPos ) // robust
                    {
                        m_cHookChar = cPos;
                        return i;
                    }
                }

                // Compatibility: First non-digit character behind a
                // a digit character becomes the hook character
                if ( bTabCompat )
                {
                    if ( ( 0x2F < cPos && cPos < 0x3A ) ||
                         ( bNumFound && ( cPos == cThousandSep || cPos == cThousandSep2 ) ) )
                    {
                        bNumFound = true;
                    }
                    else
                    {
                        if ( bNumFound )
                        {
                            m_cHookChar = cPos;
                            SetTabDecimal( cPos );
                            return i;
                        }
                    }
                }
            }
        }
    }

    // Check if character *behind* the portion has
    // to become the hook:
    if (i == nEnd && i < TextFrameIndex(GetText().getLength()) && bNumFound)
    {
        const sal_Unicode cPos = GetChar( i );
        if ( cPos != cTabDec && cPos != cThousandSep && cPos !=cThousandSep2 && ( 0x2F >= cPos || cPos >= 0x3A ) )
        {
            m_cHookChar = GetChar( i );
            SetTabDecimal( m_cHookChar );
        }
    }

    return i;
}

bool SwTextFormatInfo::LastKernPortion()
{
    if( GetLast() )
    {
        if( GetLast()->IsKernPortion() )
            return true;
        if( GetLast()->Width() || ( GetLast()->GetLen() &&
            !GetLast()->IsHolePortion() ) )
            return false;
    }
    SwLinePortion* pPor = GetRoot();
    SwLinePortion *pKern = nullptr;
    while( pPor )
    {
        if( pPor->IsKernPortion() )
            pKern = pPor;
        else if( pPor->Width() || ( pPor->GetLen() && !pPor->IsHolePortion() ) )
            pKern = nullptr;
        pPor = pPor->GetNextPortion();
    }
    if( pKern )
    {
        SetLast( pKern );
        return true;
    }
    return false;
}

SwTwips SwTextFormatInfo::GetLineWidth()
{
    SwTwips nLineWidth = Width() - X();

    const bool bTabOverMargin = GetTextFrame()->GetDoc().getIDocumentSettingAccess().get(
        DocumentSettingId::TAB_OVER_MARGIN);
    if (!bTabOverMargin)
        return nLineWidth;

    SwTabPortion* pLastTab = GetLastTab();
    if (!pLastTab)
        return nLineWidth;

    // Consider tab portions over the printing bounds of the text frame.
    if (pLastTab->GetTabPos() <= Width())
        return nLineWidth;

    // Calculate the width that starts at the left (or in case of first line:
    // first) margin, but ends after the right paragraph margin:
    //
    // +--------------------+
    // |LL|              |RR|
    // +--------------------+
    // ^ m_nLeftMargin (absolute)
    //    ^ nLeftMarginWidth (relative to m_nLeftMargin), X() is relative to this
    //                   ^ right margin
    //                      ^ paragraph right
    // <--------------------> is GetTextFrame()->getFrameArea().Width()
    //    <-------------->    is Width()
    //    <-----------------> is what we need to be able to compare to X() (nTextFrameWidth)
    SwTwips nLeftMarginWidth = m_nLeftMargin - GetTextFrame()->getFrameArea().Left();
    SwTwips nTextFrameWidth = GetTextFrame()->getFrameArea().Width() - nLeftMarginWidth;

    // If there is one such tab portion, then text is allowed to use the full
    // text frame area to the right (RR above, but not LL).
    nLineWidth = nTextFrameWidth - X();

    return nLineWidth;
}

SwTextSlot::SwTextSlot(
    const SwTextSizeInfo *pNew,
    const SwLinePortion *pPor,
    bool bTextLen,
    bool bExgLists,
    OUString const & rCh )
    : pOldText(nullptr)
    , m_pOldSmartTagList(nullptr)
    , m_pOldGrammarCheckList(nullptr)
    , nIdx(0)
    , nLen(0)
    , pInf(nullptr)
{
    if( rCh.isEmpty() )
    {
        bOn = pPor->GetExpText( *pNew, aText );
    }
    else
    {
        aText = rCh;
        bOn = true;
    }

    // The text is replaced ...
    if( bOn )
    {
        pInf = const_cast<SwTextSizeInfo*>(pNew);
        nIdx = pInf->GetIdx();
        nLen = pInf->GetLen();
        pOldText = &(pInf->GetText());
        m_pOldCachedVclData = pInf->GetCachedVclData();
        pInf->SetText( aText );
        pInf->SetIdx(TextFrameIndex(0));
        pInf->SetLen(bTextLen ? TextFrameIndex(pInf->GetText().getLength()) : pPor->GetLen());
        pInf->SetCachedVclData(nullptr);

        // ST2
        if ( bExgLists )
        {
            m_pOldSmartTagList = static_cast<SwTextPaintInfo*>(pInf)->GetSmartTags();
            if (m_pOldSmartTagList)
            {
                std::pair<SwTextNode const*, sal_Int32> pos(pNew->GetTextFrame()->MapViewToModel(nIdx));
                SwWrongList const*const pSmartTags(pos.first->GetSmartTags());
                if (pSmartTags)
                {
                    const sal_uInt16 nPos = pSmartTags->GetWrongPos(pos.second);
                    const sal_Int32 nListPos = pSmartTags->Pos(nPos);
                    if (nListPos == pos.second && pSmartTags->SubList(nPos) != nullptr)
                    {
                        m_pTempIter.reset(new sw::WrongListIterator(*pSmartTags->SubList(nPos)));
                        static_cast<SwTextPaintInfo*>(pInf)->SetSmartTags(m_pTempIter.get());
                    }
                    else if (!m_pTempList && nPos < pSmartTags->Count()
                                && nListPos < pos.second && !aText.isEmpty())
                    {
                        m_pTempList.reset(new SwWrongList( WRONGLIST_SMARTTAG ));
                        m_pTempList->Insert( OUString(), nullptr, 0, aText.getLength(), 0 );
                        m_pTempIter.reset(new sw::WrongListIterator(*m_pTempList));
                        static_cast<SwTextPaintInfo*>(pInf)->SetSmartTags(m_pTempIter.get());
                    }
                    else
                        static_cast<SwTextPaintInfo*>(pInf)->SetSmartTags(nullptr);
                }
                else
                    static_cast<SwTextPaintInfo*>(pInf)->SetSmartTags(nullptr);
            }
            m_pOldGrammarCheckList = static_cast<SwTextPaintInfo*>(pInf)->GetGrammarCheckList();
            if (m_pOldGrammarCheckList)
            {
                std::pair<SwTextNode const*, sal_Int32> pos(pNew->GetTextFrame()->MapViewToModel(nIdx));
                SwWrongList const*const pGrammar(pos.first->GetGrammarCheck());
                if (pGrammar)
                {
                    const sal_uInt16 nPos = pGrammar->GetWrongPos(pos.second);
                    const sal_Int32 nListPos = pGrammar->Pos(nPos);
                    if (nListPos == pos.second && pGrammar->SubList(nPos) != nullptr)
                    {
                        m_pTempIter.reset(new sw::WrongListIterator(*pGrammar->SubList(nPos)));
                        static_cast<SwTextPaintInfo*>(pInf)->SetGrammarCheckList(m_pTempIter.get());
                    }
                    else if (!m_pTempList && nPos < pGrammar->Count()
                                && nListPos < pos.second && !aText.isEmpty())
                    {
                        m_pTempList.reset(new SwWrongList( WRONGLIST_GRAMMAR ));
                        m_pTempList->Insert( OUString(), nullptr, 0, aText.getLength(), 0 );
                        m_pTempIter.reset(new sw::WrongListIterator(*m_pTempList));
                        static_cast<SwTextPaintInfo*>(pInf)->SetGrammarCheckList(m_pTempIter.get());
                    }
                    else
                        static_cast<SwTextPaintInfo*>(pInf)->SetGrammarCheckList(nullptr);
                }
                else
                    static_cast<SwTextPaintInfo*>(pInf)->SetGrammarCheckList(nullptr);
            }
        }
    }
}

SwTextSlot::~SwTextSlot()
{
    if( bOn )
    {
        pInf->SetCachedVclData(m_pOldCachedVclData);
        pInf->SetText( *pOldText );
        pInf->SetIdx( nIdx );
        pInf->SetLen( nLen );

        // ST2
        // Restore old smart tag list
        if (m_pOldSmartTagList)
            static_cast<SwTextPaintInfo*>(pInf)->SetSmartTags(m_pOldSmartTagList);
        if (m_pOldGrammarCheckList)
            static_cast<SwTextPaintInfo*>(pInf)->SetGrammarCheckList(m_pOldGrammarCheckList);
    }
}

SwFontSave::SwFontSave(const SwTextSizeInfo &rInf, SwFont *pNew,
        SwAttrIter* pItr)
    : pInf(nullptr)
    , pFnt(pNew ? const_cast<SwTextSizeInfo&>(rInf).GetFont() : nullptr)
    , pIter(nullptr)
{
    if( pFnt )
    {
        pInf = &const_cast<SwTextSizeInfo&>(rInf);
        // In these cases we temporarily switch to the new font:
        // 1. the fonts have a different magic number
        // 2. they have different script types
        // 3. their background colors differ (this is not covered by 1.)
        if( pFnt->DifferentFontCacheId( pNew, pFnt->GetActual() ) ||
            pNew->GetActual() != pFnt->GetActual() ||
            ( ! pNew->GetBackColor() && pFnt->GetBackColor() ) ||
            ( pNew->GetBackColor() && ! pFnt->GetBackColor() ) ||
            ( pNew->GetBackColor() && pFnt->GetBackColor() &&
              ( *pNew->GetBackColor() != *pFnt->GetBackColor() ) ) )
        {
            pNew->SetTransparent( true );
            pNew->SetAlign( ALIGN_BASELINE );
            pInf->SetFont( pNew );
        }
        else
            pFnt = nullptr;
        pNew->Invalidate();
        pNew->ChgPhysFnt( pInf->GetVsh(), *pInf->GetOut() );
        if( pItr && pItr->GetFnt() == pFnt )
        {
            pIter = pItr;
            pIter->SetFnt( pNew );
        }
    }
}

SwFontSave::~SwFontSave()
{
    if( pFnt )
    {
        // Reset SwFont
        pFnt->Invalidate();
        pInf->SetFont( pFnt );
        if( pIter )
        {
            pIter->SetFnt( pFnt );
            pIter->m_nPosition = COMPLETE_STRING;
        }
    }
}

bool SwTextFormatInfo::ChgHyph( const bool bNew )
{
    const bool bOld = m_bAutoHyph;
    if( m_bAutoHyph != bNew )
    {
        m_bAutoHyph = bNew;
        InitHyph( bNew );
        // Set language in the Hyphenator
        if( m_pFnt )
            m_pFnt->ChgPhysFnt( m_pVsh, *m_pOut );
    }
    return bOld;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
