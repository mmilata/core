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

#include <com/sun/star/table/TableSortField.hpp>
#include <cppuhelper/exc_hlp.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <o3tl/any.hxx>
#include <osl/endian.h>
#include <rtl/ustrbuf.hxx>
#include <unotools/collatorwrapper.hxx>
#include <swtypes.hxx>
#include <hintids.hxx>
#include <cmdid.h>
#include <unomid.h>
#include <hints.hxx>
#include <IMark.hxx>
#include <frmfmt.hxx>
#include <doc.hxx>
#include <IDocumentUndoRedo.hxx>
#include <istyleaccess.hxx>
#include <ndtxt.hxx>
#include <ndnotxt.hxx>
#include <unocrsr.hxx>
#include <unocrsrhelper.hxx>
#include <swundo.hxx>
#include <rootfrm.hxx>
#include <flyfrm.hxx>
#include <ftnidx.hxx>
#include <sfx2/linkmgr.hxx>
#include <docary.hxx>
#include <paratr.hxx>
#include <pam.hxx>
#include <shellio.hxx>
#include <swerror.h>
#include <swtblfmt.hxx>
#include <fmtruby.hxx>
#include <docsh.hxx>
#include <docstyle.hxx>
#include <charfmt.hxx>
#include <txtfld.hxx>
#include <fmtfld.hxx>
#include <fmtpdsc.hxx>
#include <pagedesc.hxx>
#include <edimp.hxx>
#include <fchrfmt.hxx>
#include <fmtautofmt.hxx>
#include <cntfrm.hxx>
#include <pagefrm.hxx>
#include <doctxm.hxx>
#include <sfx2/docfilt.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/fcontnr.hxx>
#include <fmtrfmrk.hxx>
#include <txtrfmrk.hxx>
#include <unotextrange.hxx>
#include <unotextcursor.hxx>
#include <unomap.hxx>
#include <unosett.hxx>
#include <unoprnms.hxx>
#include <unodraw.hxx>
#include <unocoll.hxx>
#include <unostyle.hxx>
#include <unometa.hxx>
#include <fmtanchr.hxx>
#include <editeng/flstitem.hxx>
#include <svtools/ctrltool.hxx>
#include <flypos.hxx>
#include <txtftn.hxx>
#include <com/sun/star/text/WrapTextMode.hpp>
#include <com/sun/star/text/TextContentAnchorType.hpp>
#include <com/sun/star/text/TextMarkupType.hpp>
#include <com/sun/star/style/PageStyleLayout.hpp>
#include <com/sun/star/text/XTextDocument.hpp>
#include <com/sun/star/style/XStyleFamiliesSupplier.hpp>
#include <com/sun/star/drawing/XDrawPageSupplier.hpp>
#include <unoframe.hxx>
#include <fmthdft.hxx>
#include <vcl/svapp.hxx>
#include <unotools/syslocale.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <fmtflcnt.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/unolingu.hxx>
#include <fmtclds.hxx>
#include <dcontact.hxx>
#include <SwStyleNameMapper.hxx>
#include <sortopt.hxx>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/beans/NamedValue.hpp>
#include <com/sun/star/i18n/WordType.hpp>
#include <memory>
#include <unoparaframeenum.hxx>
#include <unoparagraph.hxx>
#include <iodetect.hxx>
#include <comphelper/servicehelper.hxx>
#include <comphelper/profilezone.hxx>

using namespace ::com::sun::star;

// Helper classes
SwUnoInternalPaM::SwUnoInternalPaM(SwDoc& rDoc) :
    SwPaM(rDoc.GetNodes())
{
}

SwUnoInternalPaM::~SwUnoInternalPaM()
{
    while( GetNext() != this)
    {
        delete GetNext();
    }
}

SwUnoInternalPaM&   SwUnoInternalPaM::operator=(const SwPaM& rPaM)
{
    const SwPaM* pTmp = &rPaM;
    *GetPoint() = *rPaM.GetPoint();
    if(rPaM.HasMark())
    {
        SetMark();
        *GetMark() = *rPaM.GetMark();
    }
    else
        DeleteMark();
    while(&rPaM != (pTmp = pTmp->GetNext()))
    {
        if(pTmp->HasMark())
            new SwPaM(*pTmp->GetMark(), *pTmp->GetPoint(), this);
        else
            new SwPaM(*pTmp->GetPoint(), this);
    }
    return *this;
}

void SwUnoCursorHelper::SelectPam(SwPaM & rPam, const bool bExpand)
{
    if (bExpand)
    {
        if (!rPam.HasMark())
        {
            rPam.SetMark();
        }
    }
    else if (rPam.HasMark())
    {
        rPam.DeleteMark();
    }
}

void SwUnoCursorHelper::GetTextFromPam(SwPaM & rPam, OUString & rBuffer,
        SwRootFrame const*const pLayout)
{
    if (!rPam.HasMark())
    {
        return;
    }
    SvMemoryStream aStream;
#ifdef OSL_BIGENDIAN
    aStream.SetEndian( SvStreamEndian::BIG );
#else
    aStream.SetEndian( SvStreamEndian::LITTLE );
#endif
    WriterRef xWrt;
    // TODO/MBA: looks like a BaseURL doesn't make sense here
    SwReaderWriter::GetWriter( FILTER_TEXT_DLG, OUString(), xWrt );
    if( !xWrt.is() )
        return;

    SwWriter aWriter( aStream, rPam );
    xWrt->m_bASCII_NoLastLineEnd = true;
    xWrt->m_bExportPargraphNumbering = false;
    SwAsciiOptions aOpt = xWrt->GetAsciiOptions();
    aOpt.SetCharSet( RTL_TEXTENCODING_UNICODE );
    xWrt->SetAsciiOptions( aOpt );
    xWrt->m_bUCS2_WithStartChar = false;
    // #i68522#
    const bool bOldShowProgress = xWrt->m_bShowProgress;
    xWrt->m_bShowProgress = false;
    xWrt->m_bHideDeleteRedlines = pLayout && pLayout->IsHideRedlines();

    if( ! aWriter.Write( xWrt ).IsError() )
    {
        const sal_uInt64 lUniLen = aStream.GetSize()/sizeof( sal_Unicode );
        if (lUniLen < static_cast<sal_uInt64>(SAL_MAX_INT32-1))
        {
            aStream.WriteUInt16( '\0' );

            aStream.Seek( 0 );
            aStream.ResetError();

            rtl_uString *pStr = rtl_uString_alloc(lUniLen);
            aStream.ReadBytes(pStr->buffer, lUniLen * sizeof(sal_Unicode));
            rBuffer = OUString(pStr, SAL_NO_ACQUIRE);
        }
    }
    xWrt->m_bShowProgress = bOldShowProgress;

}

/// @throws lang::IllegalArgumentException
/// @throws uno::RuntimeException
static void
lcl_setCharStyle(SwDoc *const pDoc, const uno::Any & rValue, SfxItemSet & rSet)
{
    SwDocShell *const pDocSh = pDoc->GetDocShell();
    if(pDocSh)
    {
        OUString uStyle;
        if (!(rValue >>= uStyle))
        {
            throw lang::IllegalArgumentException();
        }
        OUString sStyle;
        SwStyleNameMapper::FillUIName(uStyle, sStyle,
                SwGetPoolIdFromName::ChrFmt);
        SwDocStyleSheet *const pStyle = static_cast<SwDocStyleSheet*>(
            pDocSh->GetStyleSheetPool()->Find(sStyle, SfxStyleFamily::Char));
        if (!pStyle)
        {
            throw lang::IllegalArgumentException();
        }
        const SwFormatCharFormat aFormat(pStyle->GetCharFormat());
        rSet.Put(aFormat);
    }
};

/// @throws lang::IllegalArgumentException
static void
lcl_setAutoStyle(IStyleAccess & rStyleAccess, const uno::Any & rValue,
        SfxItemSet & rSet, const bool bPara)
{
    OUString uStyle;
    if (!(rValue >>= uStyle))
    {
         throw lang::IllegalArgumentException();
    }
    std::shared_ptr<SfxItemSet> pStyle = bPara ?
        rStyleAccess.getByName(uStyle, IStyleAccess::AUTO_STYLE_PARA ):
        rStyleAccess.getByName(uStyle, IStyleAccess::AUTO_STYLE_CHAR );
    if(!pStyle.get())
    {
         throw lang::IllegalArgumentException();
    }

    SwFormatAutoFormat aFormat( bPara
        ? sal::static_int_cast< sal_uInt16 >(RES_AUTO_STYLE)
        : sal::static_int_cast< sal_uInt16 >(RES_TXTATR_AUTOFMT) );
    aFormat.SetStyleHandle( pStyle );
    rSet.Put(aFormat);
};

void
SwUnoCursorHelper::SetTextFormatColl(const uno::Any & rAny, SwPaM & rPaM)
{
    SwDoc *const pDoc = rPaM.GetDoc();
    SwDocShell *const pDocSh = pDoc->GetDocShell();
    if(!pDocSh)
        return;
    OUString uStyle;
    rAny >>= uStyle;
    OUString sStyle;
    SwStyleNameMapper::FillUIName(uStyle, sStyle,
            SwGetPoolIdFromName::TxtColl );
    SwDocStyleSheet *const pStyle = static_cast<SwDocStyleSheet*>(
            pDocSh->GetStyleSheetPool()->Find(sStyle, SfxStyleFamily::Para));
    if (!pStyle)
    {
        throw lang::IllegalArgumentException();
    }

    SwTextFormatColl *const pLocal = pStyle->GetCollection();
    UnoActionContext aAction(pDoc);
    pDoc->GetIDocumentUndoRedo().StartUndo( SwUndoId::START, nullptr );
    SwPaM *pTmpCursor = &rPaM;
    do {
        pDoc->SetTextFormatColl(*pTmpCursor, pLocal);
        pTmpCursor = pTmpCursor->GetNext();
    } while ( pTmpCursor != &rPaM );
    pDoc->GetIDocumentUndoRedo().EndUndo( SwUndoId::END, nullptr );
}

bool
SwUnoCursorHelper::SetPageDesc(
        const uno::Any& rValue, SwDoc & rDoc, SfxItemSet & rSet)
{
    OUString uDescName;
    if (!(rValue >>= uDescName))
    {
        return false;
    }
    std::unique_ptr<SwFormatPageDesc> pNewDesc;
    const SfxPoolItem* pItem;
    if(SfxItemState::SET == rSet.GetItemState( RES_PAGEDESC, true, &pItem ) )
    {
        pNewDesc.reset(new SwFormatPageDesc(
                    *static_cast<const SwFormatPageDesc*>(pItem)));
    }
    if (!pNewDesc)
    {
        pNewDesc.reset(new SwFormatPageDesc());
    }
    OUString sDescName;
    SwStyleNameMapper::FillUIName(uDescName, sDescName,
            SwGetPoolIdFromName::PageDesc);
    if (!pNewDesc->GetPageDesc() ||
        (pNewDesc->GetPageDesc()->GetName() != sDescName))
    {
        bool bPut = false;
        if (!sDescName.isEmpty())
        {
            SwPageDesc *const pPageDesc = SwPageDesc::GetByName(rDoc, sDescName);
            if (!pPageDesc)
            {
                throw lang::IllegalArgumentException();
            }
            pNewDesc->RegisterToPageDesc(*pPageDesc);
            bPut = true;
        }
        if(!bPut)
        {
            rSet.ClearItem(RES_BREAK);
            rSet.Put(SwFormatPageDesc());
        }
        else
        {
            rSet.Put(*pNewDesc);
        }
    }
    return true;
}

static void
lcl_SetNodeNumStart(SwPaM & rCursor, uno::Any const& rValue)
{
    sal_Int16 nTmp = 1;
    rValue >>= nTmp;
    sal_uInt16 nStt = (nTmp < 0 ? USHRT_MAX : static_cast<sal_uInt16>(nTmp));
    SwDoc* pDoc = rCursor.GetDoc();
    UnoActionContext aAction(pDoc);

    if( rCursor.GetNext() != &rCursor )         // MultiSelection?
    {
        pDoc->GetIDocumentUndoRedo().StartUndo( SwUndoId::START, nullptr );
        SwPamRanges aRangeArr( rCursor );
        SwPaM aPam( *rCursor.GetPoint() );
        for( size_t n = 0; n < aRangeArr.Count(); ++n )
        {
          pDoc->SetNumRuleStart(*aRangeArr.SetPam( n, aPam ).GetPoint());
          pDoc->SetNodeNumStart(*aRangeArr.SetPam( n, aPam ).GetPoint(),
                    nStt );
        }
        pDoc->GetIDocumentUndoRedo().EndUndo( SwUndoId::END, nullptr );
    }
    else
    {
        pDoc->SetNumRuleStart( *rCursor.GetPoint());
        pDoc->SetNodeNumStart( *rCursor.GetPoint(), nStt );
    }
}

static bool
lcl_setCharFormatSequence(SwPaM & rPam, uno::Any const& rValue)
{
    uno::Sequence<OUString> aCharStyles;
    if (!(rValue >>= aCharStyles))
    {
        return false;
    }

    for (sal_Int32 nStyle = 0; nStyle < aCharStyles.getLength(); nStyle++)
    {
        uno::Any aStyle;
        rPam.GetDoc()->GetIDocumentUndoRedo().StartUndo(SwUndoId::START, nullptr);
        aStyle <<= aCharStyles.getConstArray()[nStyle];
        // create a local set and apply each format directly
        SfxItemSet aSet(rPam.GetDoc()->GetAttrPool(),
                svl::Items<RES_TXTATR_CHARFMT, RES_TXTATR_CHARFMT>{});
        lcl_setCharStyle(rPam.GetDoc(), aStyle, aSet);
        // the first style should replace the current attributes,
        // all other have to be added
        SwUnoCursorHelper::SetCursorAttr(rPam, aSet, nStyle
                ? SetAttrMode::DONTREPLACE
                : SetAttrMode::DEFAULT);
        rPam.GetDoc()->GetIDocumentUndoRedo().EndUndo(SwUndoId::START, nullptr);
    }
    return true;
}

static void
lcl_setDropcapCharStyle(SwPaM const & rPam, SfxItemSet & rItemSet,
        uno::Any const& rValue)
{
    OUString uStyle;
    if (!(rValue >>= uStyle))
    {
        throw lang::IllegalArgumentException();
    }
    OUString sStyle;
    SwStyleNameMapper::FillUIName(uStyle, sStyle,
            SwGetPoolIdFromName::ChrFmt);
    SwDoc *const pDoc = rPam.GetDoc();
    //default character style must not be set as default format
    SwDocStyleSheet *const pStyle = static_cast<SwDocStyleSheet*>(
            pDoc->GetDocShell()
            ->GetStyleSheetPool()->Find(sStyle, SfxStyleFamily::Char));
    if (!pStyle || pStyle->GetCharFormat() == pDoc->GetDfltCharFormat())
    {
        throw lang::IllegalArgumentException();
    }
    std::unique_ptr<SwFormatDrop> pDrop;
    SfxPoolItem const* pItem(nullptr);
    if (SfxItemState::SET ==
            rItemSet.GetItemState(RES_PARATR_DROP, true, &pItem))
    {
        pDrop.reset(new SwFormatDrop(*static_cast<const SwFormatDrop*>(pItem)));
    }
    if (!pDrop)
    {
        pDrop.reset(new SwFormatDrop);
    }
    const rtl::Reference<SwDocStyleSheet> xStyle(new SwDocStyleSheet(*pStyle));
    pDrop->SetCharFormat(xStyle->GetCharFormat());
    rItemSet.Put(*pDrop);
}

static void
lcl_setRubyCharstyle(SfxItemSet & rItemSet, uno::Any const& rValue)
{
    OUString sTmp;
    if (!(rValue >>= sTmp))
    {
        throw lang::IllegalArgumentException();
    }

    std::unique_ptr<SwFormatRuby> pRuby;
    const SfxPoolItem* pItem;
    if (SfxItemState::SET ==
            rItemSet.GetItemState(RES_TXTATR_CJK_RUBY, true, &pItem))
    {
        pRuby.reset(new SwFormatRuby(*static_cast<const SwFormatRuby*>(pItem)));
    }
    if (!pRuby)
    {
        pRuby.reset(new SwFormatRuby(OUString()));
    }
    OUString sStyle;
    SwStyleNameMapper::FillUIName(sTmp, sStyle,
            SwGetPoolIdFromName::ChrFmt);
    pRuby->SetCharFormatName(sStyle);
    pRuby->SetCharFormatId(0);
    if (!sStyle.isEmpty())
    {
        const sal_uInt16 nId = SwStyleNameMapper::GetPoolIdFromUIName(
                sStyle, SwGetPoolIdFromName::ChrFmt);
        pRuby->SetCharFormatId(nId);
    }
    rItemSet.Put(*pRuby);
}

bool
SwUnoCursorHelper::SetCursorPropertyValue(
        SfxItemPropertySimpleEntry const& rEntry, const uno::Any& rValue,
        SwPaM & rPam, SfxItemSet & rItemSet)
{
    if (!(rEntry.nFlags & beans::PropertyAttribute::MAYBEVOID) &&
        (rValue.getValueType() == cppu::UnoType<void>::get()))
    {
        return false;
    }
    bool bRet = true;
    switch (rEntry.nWID)
    {
        case RES_TXTATR_CHARFMT:
            lcl_setCharStyle(rPam.GetDoc(), rValue, rItemSet);
        break;
        case RES_TXTATR_AUTOFMT:
            lcl_setAutoStyle(rPam.GetDoc()->GetIStyleAccess(),
                    rValue, rItemSet, false);
        break;
        case FN_UNO_CHARFMT_SEQUENCE:
            lcl_setCharFormatSequence(rPam, rValue);
        break;
        case FN_UNO_PARA_STYLE :
            SwUnoCursorHelper::SetTextFormatColl(rValue, rPam);
        break;
        case RES_AUTO_STYLE:
            lcl_setAutoStyle(rPam.GetDoc()->GetIStyleAccess(),
                    rValue, rItemSet, true);
        break;
        case FN_UNO_PAGE_STYLE:
            //FIXME nothing here?
        break;
        case FN_UNO_NUM_START_VALUE:
            lcl_SetNodeNumStart( rPam, rValue );
        break;
        case FN_UNO_NUM_LEVEL:
        // #i91601#
        case FN_UNO_LIST_ID:
        case FN_UNO_IS_NUMBER:
        case FN_UNO_PARA_NUM_AUTO_FORMAT:
        {
            // multi selection is not considered
            SwTextNode *const pTextNd = rPam.GetNode().GetTextNode();
            if (!pTextNd)
            {
                throw lang::IllegalArgumentException();
            }
            if (FN_UNO_NUM_LEVEL == rEntry.nWID)
            {
                sal_Int16 nLevel = 0;
                if (rValue >>= nLevel)
                {
                    if (nLevel < 0 || MAXLEVEL <= nLevel)
                    {
                        throw lang::IllegalArgumentException(
                            "invalid NumberingLevel", nullptr, 0);
                    }
                    pTextNd->SetAttrListLevel(nLevel);
                }
            }
            // #i91601#
            else if (FN_UNO_LIST_ID == rEntry.nWID)
            {
                OUString sListId;
                if (rValue >>= sListId)
                {
                    pTextNd->SetListId( sListId );
                }
            }
            else if (FN_UNO_IS_NUMBER == rEntry.nWID)
            {
                bool bIsNumber(false);
                if ((rValue >>= bIsNumber) && !bIsNumber)
                {
                    pTextNd->SetCountedInList( false );
                }
            }
            else if (FN_UNO_PARA_NUM_AUTO_FORMAT == rEntry.nWID)
            {
                uno::Sequence<beans::NamedValue> props;
                if (rValue >>= props)
                {
                    // TODO create own map for this, it contains UNO_NAME_DISPLAY_NAME? or make property readable so ODF export can map it to a automatic style?
                    SfxItemPropertySet const& rPropSet(*aSwMapProvider.GetPropertySet(PROPERTY_MAP_CHAR_AUTO_STYLE));
                    SfxItemPropertyMap const& rMap(rPropSet.getPropertyMap());
                    SfxItemSet items{rPam.GetDoc()->GetAttrPool(), aCharAutoFormatSetRange};

                    for (sal_Int32 i = 0; i < props.getLength(); ++i)
                    {
                        SfxItemPropertySimpleEntry const*const pEntry =
                            rMap.getByName(props[i].Name);
                        if (!pEntry)
                        {
                            throw beans::UnknownPropertyException(
                                "Unknown property: " + props[i].Name);
                        }
                        if (pEntry->nFlags & beans::PropertyAttribute::READONLY)
                        {
                            throw beans::PropertyVetoException(
                                "Property is read-only: " + props[i].Name);
                        }
                        rPropSet.setPropertyValue(*pEntry, props[i].Value, items);
                    }

                    SwFormatAutoFormat item(RES_PARATR_LIST_AUTOFMT);
                    // TODO: for ODF export we'd need to add it to the autostyle pool
                    item.SetStyleHandle(std::make_shared<SfxItemSet>(items));
                    pTextNd->SetAttr(item);
                }
            }
            //PROPERTY_MAYBEVOID!
        }
        break;
        case FN_NUMBER_NEWSTART:
        {
            bool bVal = false;
            if (!(rValue >>= bVal))
            {
                throw lang::IllegalArgumentException();
            }
            rPam.GetDoc()->SetNumRuleStart(*rPam.GetPoint(), bVal);
        }
        break;
        case FN_UNO_NUM_RULES:
            SwUnoCursorHelper::setNumberingProperty(rValue, rPam);
        break;
        case RES_PARATR_DROP:
        {
            if (MID_DROPCAP_CHAR_STYLE_NAME == rEntry.nMemberId)
            {
                lcl_setDropcapCharStyle(rPam, rItemSet, rValue);
            }
            else
            {
                bRet = false;
            }
        }
        break;
        case RES_TXTATR_CJK_RUBY:
        {
            if (MID_RUBY_CHARSTYLE == rEntry.nMemberId)
            {
                lcl_setRubyCharstyle(rItemSet, rValue);
            }
            else
            {
                bRet = false;
            }
        }
        break;
        case RES_PAGEDESC:
        {
            if (MID_PAGEDESC_PAGEDESCNAME == rEntry.nMemberId)
            {
                SwUnoCursorHelper::SetPageDesc(
                        rValue, *rPam.GetDoc(), rItemSet);
            }
            else
            {
                bRet = false;
            }
        }
        break;
        default:
            bRet = false;
    }
    return bRet;
}

SwFormatColl *
SwUnoCursorHelper::GetCurTextFormatColl(SwPaM & rPaM, const bool bConditional)
{
    static const sal_uLong nMaxLookup = 1000;
    SwFormatColl *pFormat = nullptr;
    bool bError = false;
    SwPaM *pTmpCursor = &rPaM;
    do
    {
        const sal_uLong nSttNd = pTmpCursor->Start()->nNode.GetIndex();
        const sal_uLong nEndNd = pTmpCursor->End()->nNode.GetIndex();

        if( nEndNd - nSttNd >= nMaxLookup )
        {
            pFormat = nullptr;
            break;
        }

        const SwNodes& rNds = rPaM.GetDoc()->GetNodes();
        for( sal_uLong n = nSttNd; n <= nEndNd; ++n )
        {
            SwTextNode const*const pNd = rNds[ n ]->GetTextNode();
            if( pNd )
            {
                SwFormatColl *const pNdFormat = bConditional
                    ? pNd->GetFormatColl() : &pNd->GetAnyFormatColl();
                if( !pFormat )
                {
                    pFormat = pNdFormat;
                }
                else if( pFormat != pNdFormat )
                {
                    bError = true;
                    break;
                }
            }
        }

        pTmpCursor = pTmpCursor->GetNext();
    } while ( pTmpCursor != &rPaM );
    return bError ? nullptr : pFormat;
}

class SwXTextCursor::Impl
{

public:
    const SfxItemPropertySet &  m_rPropSet;
    const CursorType            m_eType;
    const uno::Reference< text::XText > m_xParentText;
    sw::UnoCursorPointer m_pUnoCursor;

    Impl(   SwDoc & rDoc,
            const CursorType eType,
            uno::Reference<text::XText> const & xParent,
            SwPosition const& rPoint, SwPosition const*const pMark)
        : m_rPropSet(*aSwMapProvider.GetPropertySet(PROPERTY_MAP_TEXT_CURSOR))
        , m_eType(eType)
        , m_xParentText(xParent)
        , m_pUnoCursor(rDoc.CreateUnoCursor(rPoint))
    {
        if (pMark)
        {
            m_pUnoCursor->SetMark();
            *m_pUnoCursor->GetMark() = *pMark;
        }
    }

    SwUnoCursor& GetCursorOrThrow() {
        if(!m_pUnoCursor)
            throw uno::RuntimeException("SwXTextCursor: disposed or invalid", nullptr);
        return *m_pUnoCursor;
    }
};

SwUnoCursor& SwXTextCursor::GetCursor()
    { return *m_pImpl->m_pUnoCursor; }

SwPaM const* SwXTextCursor::GetPaM() const
    { return m_pImpl->m_pUnoCursor.get(); }

SwPaM* SwXTextCursor::GetPaM()
    { return m_pImpl->m_pUnoCursor.get(); }

SwDoc const* SwXTextCursor::GetDoc() const
    { return m_pImpl->m_pUnoCursor ? m_pImpl->m_pUnoCursor->GetDoc() : nullptr; }

SwDoc* SwXTextCursor::GetDoc()
    { return m_pImpl->m_pUnoCursor ? m_pImpl->m_pUnoCursor->GetDoc() : nullptr; }

SwXTextCursor::SwXTextCursor(
        SwDoc & rDoc,
        uno::Reference< text::XText > const& xParent,
        const CursorType eType,
        const SwPosition& rPos,
        SwPosition const*const pMark)
    : m_pImpl( new Impl(rDoc, eType, xParent, rPos, pMark) )
{
}

SwXTextCursor::SwXTextCursor(uno::Reference< text::XText > const& xParent,
        SwPaM const& rSourceCursor, const CursorType eType)
    : m_pImpl( new Impl(*rSourceCursor.GetDoc(), eType,
                xParent, *rSourceCursor.GetPoint(),
                rSourceCursor.HasMark() ? rSourceCursor.GetMark() : nullptr) )
{
}

SwXTextCursor::~SwXTextCursor()
{
}

void SwXTextCursor::DeleteAndInsert(const OUString& rText,
        const bool bForceExpandHints)
{
    auto pUnoCursor = static_cast<SwCursor*>(m_pImpl->m_pUnoCursor.get());
    if (pUnoCursor)
    {
        // Start/EndAction
        SwDoc* pDoc = pUnoCursor->GetDoc();
        UnoActionContext aAction(pDoc);
        const sal_Int32 nTextLen = rText.getLength();
        pDoc->GetIDocumentUndoRedo().StartUndo(SwUndoId::INSERT, nullptr);
        auto pCurrent = pUnoCursor;
        do
        {
            if (pCurrent->HasMark())
            {
                pDoc->getIDocumentContentOperations().DeleteAndJoin(*pCurrent);
            }
            if(nTextLen)
            {
                const bool bSuccess(
                    SwUnoCursorHelper::DocInsertStringSplitCR(
                        *pDoc, *pCurrent, rText, bForceExpandHints ) );
                OSL_ENSURE( bSuccess, "Doc->Insert(Str) failed." );

                SwUnoCursorHelper::SelectPam(*pUnoCursor, true);
                pCurrent->Left(rText.getLength());
            }
            pCurrent = pCurrent->GetNext();
        } while (pCurrent != pUnoCursor);
        pDoc->GetIDocumentUndoRedo().EndUndo(SwUndoId::INSERT, nullptr);
    }
}

namespace {

enum ForceIntoMetaMode { META_CHECK_BOTH, META_INIT_START, META_INIT_END };

}

static bool
lcl_ForceIntoMeta(SwPaM & rCursor,
        uno::Reference<text::XText> const & xParentText,
        const enum ForceIntoMetaMode eMode)
{
    bool bRet( true ); // means not forced in META_CHECK_BOTH
    SwXMeta const * const pXMeta( dynamic_cast<SwXMeta*>(xParentText.get()) );
    OSL_ENSURE(pXMeta, "no parent?");
    if (!pXMeta)
        throw uno::RuntimeException();
    SwTextNode * pTextNode;
    sal_Int32 nStart;
    sal_Int32 nEnd;
    const bool bSuccess( pXMeta->SetContentRange(pTextNode, nStart, nEnd) );
    OSL_ENSURE(bSuccess, "no pam?");
    if (!bSuccess)
        throw uno::RuntimeException();
    // force the cursor back into the meta if it has moved outside
    SwPosition start(*pTextNode, nStart);
    SwPosition end(*pTextNode, nEnd);
    switch (eMode)
    {
        case META_INIT_START:
            *rCursor.GetPoint() = start;
            break;
        case META_INIT_END:
            *rCursor.GetPoint() = end;
            break;
        case META_CHECK_BOTH:
            if (*rCursor.Start() < start)
            {
                *rCursor.Start() = start;
                bRet = false;
            }
            if (*rCursor.End() > end)
            {
                *rCursor.End() = end;
                bRet = false;
            }
            break;
    }
    return bRet;
}

bool SwXTextCursor::IsAtEndOfMeta() const
{
    if (CursorType::Meta == m_pImpl->m_eType)
    {
        auto pCursor( m_pImpl->m_pUnoCursor );
        SwXMeta const*const pXMeta(
                dynamic_cast<SwXMeta*>(m_pImpl->m_xParentText.get()) );
        OSL_ENSURE(pXMeta, "no meta?");
        if (pCursor && pXMeta)
        {
            SwTextNode * pTextNode;
            sal_Int32 nStart;
            sal_Int32 nEnd;
            const bool bSuccess(
                    pXMeta->SetContentRange(pTextNode, nStart, nEnd) );
            OSL_ENSURE(bSuccess, "no pam?");
            if (bSuccess)
            {
                const SwPosition end(*pTextNode, nEnd);
                if (   (*pCursor->GetPoint() == end)
                    || (*pCursor->GetMark()  == end))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

OUString SwXTextCursor::getImplementationName()
{
    return "SwXTextCursor";
}

sal_Bool SAL_CALL SwXTextCursor::supportsService(const OUString& rServiceName)
{
    return cppu::supportsService(this, rServiceName);
}

uno::Sequence< OUString > SAL_CALL
SwXTextCursor::getSupportedServiceNames()
{
    return {
        "com.sun.star.text.TextCursor",
        "com.sun.star.style.CharacterProperties",
        "com.sun.star.style.CharacterPropertiesAsian",
        "com.sun.star.style.CharacterPropertiesComplex",
        "com.sun.star.style.ParagraphProperties",
        "com.sun.star.style.ParagraphPropertiesAsian",
        "com.sun.star.style.ParagraphPropertiesComplex",
        "com.sun.star.text.TextSortable"
    };
}

namespace
{
    class theSwXTextCursorUnoTunnelId : public rtl::Static< UnoTunnelIdInit, theSwXTextCursorUnoTunnelId > {};
}

const uno::Sequence< sal_Int8 > & SwXTextCursor::getUnoTunnelId()
{
    return theSwXTextCursorUnoTunnelId::get().getSeq();
}

sal_Int64 SAL_CALL
SwXTextCursor::getSomething(const uno::Sequence< sal_Int8 >& rId)
{
    const sal_Int64 nRet( ::sw::UnoTunnelImpl<SwXTextCursor>(rId, this) );
    return nRet ? nRet : OTextCursorHelper::getSomething(rId);
}

void SAL_CALL SwXTextCursor::collapseToStart()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    if (rUnoCursor.HasMark())
    {
        if (*rUnoCursor.GetPoint() > *rUnoCursor.GetMark())
        {
            rUnoCursor.Exchange();
        }
        rUnoCursor.DeleteMark();
    }
}

void SAL_CALL SwXTextCursor::collapseToEnd()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    if (rUnoCursor.HasMark())
    {
        if (*rUnoCursor.GetPoint() < *rUnoCursor.GetMark())
        {
            rUnoCursor.Exchange();
        }
        rUnoCursor.DeleteMark();
    }
}

sal_Bool SAL_CALL SwXTextCursor::isCollapsed()
{
    SolarMutexGuard aGuard;

    bool bRet = true;
    auto pUnoCursor(m_pImpl->m_pUnoCursor);
    if(pUnoCursor && pUnoCursor->GetMark())
    {
        bRet = (*pUnoCursor->GetPoint() == *pUnoCursor->GetMark());
    }
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::goLeft(sal_Int16 nCount, sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    bool bRet = rUnoCursor.Left( nCount);
    if (CursorType::Meta == m_pImpl->m_eType)
    {
        bRet = lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText,
                    META_CHECK_BOTH)
            && bRet;
    }
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::goRight(sal_Int16 nCount, sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    bool bRet = rUnoCursor.Right(nCount);
    if (CursorType::Meta == m_pImpl->m_eType)
    {
        bRet = lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText,
                    META_CHECK_BOTH)
            && bRet;
    }
    return bRet;
}

void SAL_CALL
SwXTextCursor::gotoStart(sal_Bool Expand)
{
    SolarMutexGuard aGuard;
    comphelper::ProfileZone aZone("gotoStart");

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    if (CursorType::Body == m_pImpl->m_eType)
    {
        rUnoCursor.Move( fnMoveBackward, GoInDoc );
        //check, that the cursor is not in a table
        SwTableNode * pTableNode = rUnoCursor.GetNode().FindTableNode();
        SwContentNode * pCNode = nullptr;
        while (pTableNode)
        {
            rUnoCursor.GetPoint()->nNode = *pTableNode->EndOfSectionNode();
            pCNode = GetDoc()->GetNodes().GoNext(&rUnoCursor.GetPoint()->nNode);
            pTableNode = pCNode ? pCNode->FindTableNode() : nullptr;
        }
        if (pCNode)
        {
            rUnoCursor.GetPoint()->nContent.Assign(pCNode, 0);
        }
        SwStartNode const*const pTmp =
            rUnoCursor.GetNode().StartOfSectionNode();
        if (pTmp->IsSectionNode())
        {
            SwSectionNode const*const pSectionStartNode =
                static_cast<SwSectionNode const*>(pTmp);
            if (pSectionStartNode->GetSection().IsHiddenFlag())
            {
                pCNode = GetDoc()->GetNodes().GoNextSection(
                        &rUnoCursor.GetPoint()->nNode, true, false);
                if (pCNode)
                {
                    rUnoCursor.GetPoint()->nContent.Assign(pCNode, 0);
                }
            }
        }
    }
    else if (   (CursorType::Frame   == m_pImpl->m_eType)
            ||  (CursorType::TableText == m_pImpl->m_eType)
            ||  (CursorType::Header  == m_pImpl->m_eType)
            ||  (CursorType::Footer  == m_pImpl->m_eType)
            ||  (CursorType::Footnote== m_pImpl->m_eType)
            ||  (CursorType::Redline == m_pImpl->m_eType))
    {
        rUnoCursor.MoveSection(GoCurrSection, fnSectionStart);
    }
    else if (CursorType::Meta == m_pImpl->m_eType)
    {
        lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText, META_INIT_START);
    }
}

void SAL_CALL
SwXTextCursor::gotoEnd(sal_Bool Expand)
{
    SolarMutexGuard aGuard;
    comphelper::ProfileZone aZone("gotoEnd");

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    if (CursorType::Body == m_pImpl->m_eType)
    {
        rUnoCursor.Move( fnMoveForward, GoInDoc );
    }
    else if (   (CursorType::Frame   == m_pImpl->m_eType)
            ||  (CursorType::TableText == m_pImpl->m_eType)
            ||  (CursorType::Header  == m_pImpl->m_eType)
            ||  (CursorType::Footer  == m_pImpl->m_eType)
            ||  (CursorType::Footnote== m_pImpl->m_eType)
            ||  (CursorType::Redline == m_pImpl->m_eType))
    {
        rUnoCursor.MoveSection( GoCurrSection, fnSectionEnd);
    }
    else if (CursorType::Meta == m_pImpl->m_eType)
    {
        lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText, META_INIT_END);
    }
}

void SAL_CALL
SwXTextCursor::gotoRange(
    const uno::Reference< text::XTextRange > & xRange, sal_Bool bExpand)
{
    SolarMutexGuard aGuard;

    if (!xRange.is())
    {
        throw uno::RuntimeException();
    }

    SwUnoCursor & rOwnCursor( m_pImpl->GetCursorOrThrow() );

    uno::Reference<lang::XUnoTunnel> xRangeTunnel( xRange, uno::UNO_QUERY);
    SwXTextRange* pRange = nullptr;
    OTextCursorHelper* pCursor = nullptr;
    if(xRangeTunnel.is())
    {
        pRange  = ::sw::UnoTunnelGetImplementation<SwXTextRange>(xRangeTunnel);
        pCursor =
            ::sw::UnoTunnelGetImplementation<OTextCursorHelper>(xRangeTunnel);
    }

    if (!pRange && !pCursor)
    {
        throw uno::RuntimeException();
    }

    SwPaM aPam(GetDoc()->GetNodes());
    const SwPaM * pPam(nullptr);
    if (pCursor)
    {
        pPam = pCursor->GetPaM();
    }
    else if (pRange)
    {
        if (pRange->GetPositions(aPam))
        {
            pPam = & aPam;
        }
    }

    if (!pPam)
    {
        throw uno::RuntimeException();
    }

    {
        SwStartNodeType eSearchNodeType = SwNormalStartNode;
        switch (m_pImpl->m_eType)
        {
        case CursorType::Frame:      eSearchNodeType = SwFlyStartNode;       break;
        case CursorType::TableText:    eSearchNodeType = SwTableBoxStartNode;  break;
        case CursorType::Footnote:   eSearchNodeType = SwFootnoteStartNode;  break;
        case CursorType::Header:     eSearchNodeType = SwHeaderStartNode;    break;
        case CursorType::Footer:     eSearchNodeType = SwFooterStartNode;    break;
            //case CURSOR_INVALID:
            //case CursorType::Body:
        default:
            ;
        }

        const SwStartNode* pOwnStartNode = rOwnCursor.GetNode().FindSttNodeByType(eSearchNodeType);
        while ( pOwnStartNode != nullptr
                && pOwnStartNode->IsSectionNode())
        {
            pOwnStartNode = pOwnStartNode->StartOfSectionNode();
        }

        const SwStartNode* pTmp =
            pPam->GetNode().FindSttNodeByType(eSearchNodeType);
        while ( pTmp != nullptr
                && pTmp->IsSectionNode() )
        {
            pTmp = pTmp->StartOfSectionNode();
        }

        if ( eSearchNodeType == SwTableBoxStartNode )
        {
            if (!pOwnStartNode || !pTmp)
            {
                throw uno::RuntimeException();
            }

            if ( pOwnStartNode->FindTableNode() != pTmp->FindTableNode() )
            {
                throw uno::RuntimeException();
            }
        }
        else
        {
            if ( pOwnStartNode != pTmp )
            {
                throw uno::RuntimeException();
            }
        }
    }

    if (CursorType::Meta == m_pImpl->m_eType)
    {
        SwPaM CopyPam(*pPam->GetMark(), *pPam->GetPoint());
        const bool bNotForced( lcl_ForceIntoMeta(
                    CopyPam, m_pImpl->m_xParentText, META_CHECK_BOTH) );
        if (!bNotForced)
        {
            throw uno::RuntimeException(
                "gotoRange: parameter range not contained in nesting"
                    " text content for which this cursor was created",
                static_cast<text::XWordCursor*>(this));
        }
    }

    // selection has to be expanded here
    if(bExpand)
    {
        // cursor should include its previous range plus the given range
        const SwPosition aOwnLeft(*rOwnCursor.Start());
        const SwPosition aOwnRight(*rOwnCursor.End());
        SwPosition const& rParamLeft  = *pPam->Start();
        SwPosition const& rParamRight = *pPam->End();

        // now there are four SwPositions,
        // two of them are going to be used, but which ones?
        if (aOwnRight > rParamRight)
            *rOwnCursor.GetPoint() = aOwnRight;
        else
            *rOwnCursor.GetPoint() = rParamRight;
        rOwnCursor.SetMark();
        if (aOwnLeft < rParamLeft)
            *rOwnCursor.GetMark() = aOwnLeft;
        else
            *rOwnCursor.GetMark() = rParamLeft;
    }
    else
    {
        // cursor should be the given range
        *rOwnCursor.GetPoint() = *pPam->GetPoint();
        if (pPam->HasMark())
        {
            rOwnCursor.SetMark();
            *rOwnCursor.GetMark() = *pPam->GetMark();
        }
        else
        {
            rOwnCursor.DeleteMark();
        }
    }
}

sal_Bool SAL_CALL SwXTextCursor::isStartOfWord()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    const bool bRet =
        rUnoCursor.IsStartWordWT( i18n::WordType::DICTIONARY_WORD );
    return bRet;
}

sal_Bool SAL_CALL SwXTextCursor::isEndOfWord()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    const bool bRet =
        rUnoCursor.IsEndWordWT( i18n::WordType::DICTIONARY_WORD );
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoNextWord(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    // problems arise when a paragraph starts with something other than a word
    bool bRet = false;
    // remember old position to check if cursor has moved
    // since the called functions are sometimes a bit unreliable
    // in specific cases...
    SwPosition  *const pPoint     = rUnoCursor.GetPoint();
    SwNode      *const pOldNode   = &pPoint->nNode.GetNode();
    sal_Int32 const nOldIndex  = pPoint->nContent.GetIndex();

    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    // end of paragraph
    if (rUnoCursor.GetContentNode() &&
            (pPoint->nContent == rUnoCursor.GetContentNode()->Len()))
    {
        rUnoCursor.Right(1);
    }
    else
    {
        const bool bTmp =
            rUnoCursor.GoNextWordWT( i18n::WordType::DICTIONARY_WORD );
        // if there is no next word within the current paragraph
        // try to go to the start of the next paragraph
        if (!bTmp)
        {
            rUnoCursor.MovePara(GoNextPara, fnParaStart);
        }
    }

    // return true if cursor has moved
    bRet =  (&pPoint->nNode.GetNode() != pOldNode)  ||
            (pPoint->nContent.GetIndex() != nOldIndex);
    if (bRet && (CursorType::Meta == m_pImpl->m_eType))
    {
        bRet = lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText,
                    META_CHECK_BOTH);
    }

    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoPreviousWord(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    // white spaces create problems on the paragraph start
    bool bRet = false;
    SwPosition  *const pPoint     = rUnoCursor.GetPoint();
    SwNode      *const pOldNode   = &pPoint->nNode.GetNode();
    sal_Int32 const nOldIndex  = pPoint->nContent.GetIndex();

    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    // start of paragraph?
    if (pPoint->nContent == 0)
    {
        rUnoCursor.Left(1);
    }
    else
    {
        rUnoCursor.GoPrevWordWT( i18n::WordType::DICTIONARY_WORD );
        if (pPoint->nContent == 0)
        {
            rUnoCursor.Left(1);
        }
    }

    // return true if cursor has moved
    bRet =  (&pPoint->nNode.GetNode() != pOldNode)  ||
            (pPoint->nContent.GetIndex() != nOldIndex);
    if (bRet && (CursorType::Meta == m_pImpl->m_eType))
    {
        bRet = lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText,
                    META_CHECK_BOTH);
    }

    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoEndOfWord(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    bool bRet = false;
    SwPosition  *const pPoint     = rUnoCursor.GetPoint();
    SwNode      &      rOldNode   = pPoint->nNode.GetNode();
    sal_Int32 const nOldIndex  = pPoint->nContent.GetIndex();

    const sal_Int16 nWordType = i18n::WordType::DICTIONARY_WORD;
    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    if (!rUnoCursor.IsEndWordWT( nWordType ))
    {
        rUnoCursor.GoEndWordWT( nWordType );
    }

    // restore old cursor if we are not at the end of a word by now
    // otherwise use current one
    bRet = rUnoCursor.IsEndWordWT( nWordType );
    if (!bRet)
    {
        pPoint->nNode       = rOldNode;
        pPoint->nContent    = nOldIndex;
    }
    else if (CursorType::Meta == m_pImpl->m_eType)
    {
        bRet = lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText,
                    META_CHECK_BOTH);
    }

    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoStartOfWord(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    bool bRet = false;
    SwPosition  *const pPoint     = rUnoCursor.GetPoint();
    SwNode      &      rOldNode   = pPoint->nNode.GetNode();
    sal_Int32 const nOldIndex  = pPoint->nContent.GetIndex();

    const sal_Int16 nWordType = i18n::WordType::DICTIONARY_WORD;
    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    if (!rUnoCursor.IsStartWordWT( nWordType ))
    {
        rUnoCursor.GoStartWordWT( nWordType );
    }

    // restore old cursor if we are not at the start of a word by now
    // otherwise use current one
    bRet = rUnoCursor.IsStartWordWT( nWordType );
    if (!bRet)
    {
        pPoint->nNode       = rOldNode;
        pPoint->nContent    = nOldIndex;
    }
    else if (CursorType::Meta == m_pImpl->m_eType)
    {
        bRet = lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText,
                    META_CHECK_BOTH);
    }

    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::isStartOfSentence()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    // start of paragraph?
    bool bRet = rUnoCursor.GetPoint()->nContent == 0;
    // with mark ->no sentence start
    // (check if cursor is no selection, i.e. it does not have
    // a mark or else point and mark are identical)
    if (!bRet && (!rUnoCursor.HasMark() ||
                    *rUnoCursor.GetPoint() == *rUnoCursor.GetMark()))
    {
        SwCursor aCursor(*rUnoCursor.GetPoint(),nullptr);
        SwPosition aOrigPos = *aCursor.GetPoint();
        aCursor.GoSentence(SwCursor::START_SENT );
        bRet = aOrigPos == *aCursor.GetPoint();
    }
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::isEndOfSentence()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    // end of paragraph?
    bool bRet = rUnoCursor.GetContentNode() &&
        (rUnoCursor.GetPoint()->nContent == rUnoCursor.GetContentNode()->Len());
    // with mark->no sentence end
    // (check if cursor is no selection, i.e. it does not have
    // a mark or else point and mark are identical)
    if (!bRet && (!rUnoCursor.HasMark() ||
                    *rUnoCursor.GetPoint() == *rUnoCursor.GetMark()))
    {
        SwCursor aCursor(*rUnoCursor.GetPoint(), nullptr);
        SwPosition aOrigPos = *aCursor.GetPoint();
        aCursor.GoSentence(SwCursor::END_SENT);
        bRet = aOrigPos == *aCursor.GetPoint();
    }
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoNextSentence(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    const bool bWasEOS = isEndOfSentence();
    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    bool bRet = rUnoCursor.GoSentence(SwCursor::NEXT_SENT);
    if (!bRet)
    {
        bRet = rUnoCursor.MovePara(GoNextPara, fnParaStart);
    }

    // if at the end of the sentence (i.e. at the space after the '.')
    // advance to next word in order for GoSentence to work properly
    // next time and have isStartOfSentence return true after this call
    if (!rUnoCursor.IsStartWordWT(css::i18n::WordType::ANYWORD_IGNOREWHITESPACES))
    {
        const bool bNextWord = rUnoCursor.GoNextWordWT(i18n::WordType::ANYWORD_IGNOREWHITESPACES);
        if (bWasEOS && !bNextWord)
        {
            bRet = false;
        }
    }
    if (CursorType::Meta == m_pImpl->m_eType)
    {
        bRet = lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText,
                    META_CHECK_BOTH)
            && bRet;
    }
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoPreviousSentence(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    bool bRet = rUnoCursor.GoSentence(SwCursor::PREV_SENT);
    if (!bRet)
    {
        bRet = rUnoCursor.MovePara(GoPrevPara, fnParaStart);
        if (bRet)
        {
            rUnoCursor.MovePara(GoCurrPara, fnParaEnd);
            // at the end of a paragraph move to the sentence end again
            rUnoCursor.GoSentence(SwCursor::PREV_SENT);
        }
    }
    if (CursorType::Meta == m_pImpl->m_eType)
    {
        bRet = lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText,
                    META_CHECK_BOTH)
            && bRet;
    }
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoStartOfSentence(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    // if we're at the para start then we won't move
    // but bRet is also true if GoSentence failed but
    // the start of the sentence is reached
    bool bRet = SwUnoCursorHelper::IsStartOfPara(rUnoCursor)
        || rUnoCursor.GoSentence(SwCursor::START_SENT)
        || SwUnoCursorHelper::IsStartOfPara(rUnoCursor);
    if (CursorType::Meta == m_pImpl->m_eType)
    {
        bRet = lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText,
                    META_CHECK_BOTH)
            && bRet;
    }
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoEndOfSentence(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    // bRet is true if GoSentence() succeeded or if the
    // MovePara() succeeded while the end of the para is
    // not reached already
    bool bAlreadyParaEnd = SwUnoCursorHelper::IsEndOfPara(rUnoCursor);
    bool bRet = !bAlreadyParaEnd
            &&  (rUnoCursor.GoSentence(SwCursor::END_SENT)
                 || rUnoCursor.MovePara(GoCurrPara, fnParaEnd));
    if (CursorType::Meta == m_pImpl->m_eType)
    {
        bRet = lcl_ForceIntoMeta(rUnoCursor, m_pImpl->m_xParentText,
                    META_CHECK_BOTH)
            && bRet;
    }
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::isStartOfParagraph()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    const bool bRet = SwUnoCursorHelper::IsStartOfPara(rUnoCursor);
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::isEndOfParagraph()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    const bool bRet = SwUnoCursorHelper::IsEndOfPara(rUnoCursor);
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoStartOfParagraph(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    if (CursorType::Meta == m_pImpl->m_eType)
    {
        return false;
    }
    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    bool bRet = SwUnoCursorHelper::IsStartOfPara(rUnoCursor);
    if (!bRet)
    {
        bRet = rUnoCursor.MovePara(GoCurrPara, fnParaStart);
    }

    // since MovePara(GoCurrPara, fnParaStart) only returns false
    // if we were already at the start of the paragraph this function
    // should always complete successfully.
    OSL_ENSURE( bRet, "gotoStartOfParagraph failed" );
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoEndOfParagraph(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    if (CursorType::Meta == m_pImpl->m_eType)
    {
        return false;
    }
    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    bool bRet = SwUnoCursorHelper::IsEndOfPara(rUnoCursor);
    if (!bRet)
    {
        bRet = rUnoCursor.MovePara(GoCurrPara, fnParaEnd);
    }

    // since MovePara(GoCurrPara, fnParaEnd) only returns false
    // if we were already at the end of the paragraph this function
    // should always complete successfully.
    OSL_ENSURE( bRet, "gotoEndOfParagraph failed" );
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoNextParagraph(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    if (CursorType::Meta == m_pImpl->m_eType)
    {
        return false;
    }
    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    const bool bRet = rUnoCursor.MovePara(GoNextPara, fnParaStart);
    return bRet;
}

sal_Bool SAL_CALL
SwXTextCursor::gotoPreviousParagraph(sal_Bool Expand)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    if (CursorType::Meta == m_pImpl->m_eType)
    {
        return false;
    }
    SwUnoCursorHelper::SelectPam(rUnoCursor, Expand);
    const bool bRet = rUnoCursor.MovePara(GoPrevPara, fnParaStart);
    return bRet;
}

uno::Reference< text::XText > SAL_CALL
SwXTextCursor::getText()
{
    SolarMutexGuard g;

    return m_pImpl->m_xParentText;
}

uno::Reference< text::XTextRange > SAL_CALL
SwXTextCursor::getStart()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    uno::Reference< text::XTextRange > xRet;
    SwPaM aPam(*rUnoCursor.Start());
    const uno::Reference< text::XText >  xParent = getText();
    if (CursorType::Meta == m_pImpl->m_eType)
    {
        // return cursor to prevent modifying SwXTextRange for META
        SwXTextCursor * const pXCursor(
            new SwXTextCursor(*rUnoCursor.GetDoc(), xParent, CursorType::Meta,
                *rUnoCursor.GetPoint()) );
        pXCursor->gotoStart(false);
        xRet = static_cast<text::XWordCursor*>(pXCursor);
    }
    else
    {
        xRet = new SwXTextRange(aPam, xParent);
    }
    return xRet;
}

uno::Reference< text::XTextRange > SAL_CALL
SwXTextCursor::getEnd()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    uno::Reference< text::XTextRange >  xRet;
    SwPaM aPam(*rUnoCursor.End());
    const uno::Reference< text::XText >  xParent = getText();
    if (CursorType::Meta == m_pImpl->m_eType)
    {
        // return cursor to prevent modifying SwXTextRange for META
        SwXTextCursor * const pXCursor(
            new SwXTextCursor(*rUnoCursor.GetDoc(), xParent, CursorType::Meta,
                *rUnoCursor.GetPoint()) );
        pXCursor->gotoEnd(false);
        xRet = static_cast<text::XWordCursor*>(pXCursor);
    }
    else
    {
        xRet = new SwXTextRange(aPam, xParent);
    }
    return xRet;
}

OUString SAL_CALL SwXTextCursor::getString()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    OUString aText;
    SwUnoCursorHelper::GetTextFromPam(rUnoCursor, aText);
    return aText;
}

void SAL_CALL
SwXTextCursor::setString(const OUString& aString)
{
    SolarMutexGuard aGuard;

    m_pImpl->GetCursorOrThrow(); // just to check if valid

    const bool bForceExpandHints( (CursorType::Meta == m_pImpl->m_eType)
        && dynamic_cast<SwXMeta*>(m_pImpl->m_xParentText.get())
                ->CheckForOwnMemberMeta(*GetPaM(), true) );
    DeleteAndInsert(aString, bForceExpandHints);
}

uno::Any SwUnoCursorHelper::GetPropertyValue(
    SwPaM& rPaM, const SfxItemPropertySet& rPropSet,
    const OUString& rPropertyName)
{
    uno::Any aAny;
    SfxItemPropertySimpleEntry const*const pEntry =
        rPropSet.getPropertyMap().getByName(rPropertyName);

    if (!pEntry)
    {
        throw beans::UnknownPropertyException(
            "Unknown property: " + rPropertyName,
            static_cast<cppu::OWeakObject *>(nullptr));
    }

    beans::PropertyState eTemp;
    const bool bDone = SwUnoCursorHelper::getCursorPropertyValue(
            *pEntry, rPaM, &aAny, eTemp );

    if (!bDone)
    {
        SfxItemSet aSet(
            rPaM.GetDoc()->GetAttrPool(),
            svl::Items<
                RES_CHRATR_BEGIN, RES_FRMATR_END - 1,
                RES_UNKNOWNATR_CONTAINER, RES_UNKNOWNATR_CONTAINER>{});
        SwUnoCursorHelper::GetCursorAttr(rPaM, aSet);

        rPropSet.getPropertyValue(*pEntry, aSet, aAny);
    }

    return aAny;
}

void SwUnoCursorHelper::SetPropertyValue(
    SwPaM& rPaM, const SfxItemPropertySet& rPropSet,
    const OUString& rPropertyName,
    const uno::Any& rValue,
    const SetAttrMode nAttrMode)
{
    uno::Sequence< beans::PropertyValue > aValues(1);
    aValues[0].Name = rPropertyName;
    aValues[0].Value = rValue;
    SetPropertyValues(rPaM, rPropSet, aValues, nAttrMode);
}

// FN_UNO_PARA_STYLE is known to set attributes for nodes, inside
// SwUnoCursorHelper::SetTextFormatColl, instead of extending item set.
// We need to get them from nodes in next call to GetCursorAttr.
// The rest could cause similar problems in theory, so we just list them here.
static bool propertyCausesSideEffectsInNodes(sal_uInt16 nWID)
{
    return nWID == FN_UNO_PARA_STYLE ||
           nWID == FN_UNO_CHARFMT_SEQUENCE ||
           nWID == FN_UNO_NUM_START_VALUE ||
           nWID == FN_UNO_NUM_RULES;
}

void SwUnoCursorHelper::SetPropertyValues(
    SwPaM& rPaM, const SfxItemPropertySet& rPropSet,
    const uno::Sequence< beans::PropertyValue > &rPropertyValues,
    const SetAttrMode nAttrMode)
{
    if (!rPropertyValues.hasElements())
        return;

    SwDoc *const pDoc = rPaM.GetDoc();
    OUString aUnknownExMsg, aPropertyVetoExMsg;

    // Build set of attributes we want to fetch
    const sal_uInt16 zero = 0;
    SfxItemSet aItemSet(pDoc->GetAttrPool(), &zero);
    std::vector<std::pair<const SfxItemPropertySimpleEntry*, const uno::Any&>> aEntries;
    aEntries.reserve(rPropertyValues.getLength());
    for (const auto& rPropVal : rPropertyValues)
    {
        const OUString &rPropertyName = rPropVal.Name;

        SfxItemPropertySimpleEntry const* pEntry =
            rPropSet.getPropertyMap().getByName(rPropertyName);

        // Queue up any exceptions until the end ...
        if (!pEntry)
        {
            aUnknownExMsg += "Unknown property: '" + rPropertyName + "' ";
            continue;
        }
        else if (pEntry->nFlags & beans::PropertyAttribute::READONLY)
        {
            aPropertyVetoExMsg += "Property is read-only: '" + rPropertyName + "' ";
            continue;
        }
        aItemSet.MergeRange(pEntry->nWID, pEntry->nWID);
        aEntries.emplace_back(pEntry, rPropVal.Value);
    }

    if (!aEntries.empty())
    {
        // Fetch, overwrite, and re-set the attributes from the core

        bool bPreviousPropertyCausesSideEffectsInNodes = false;
        for (size_t i = 0; i < aEntries.size(); ++i)
        {
            SfxItemPropertySimpleEntry const*const pEntry = aEntries[i].first;
            bool bPropertyCausesSideEffectsInNodes =
                propertyCausesSideEffectsInNodes(pEntry->nWID);

            // we need to get up-to-date item set from nodes
            if (i == 0 || bPreviousPropertyCausesSideEffectsInNodes)
            {
                aItemSet.ClearItem();
                SwUnoCursorHelper::GetCursorAttr(rPaM, aItemSet);
            }

            const uno::Any &rValue = aEntries[i].second;
            // this can set some attributes in nodes' mpAttrSet
            if (!SwUnoCursorHelper::SetCursorPropertyValue(*pEntry, rValue, rPaM, aItemSet))
                rPropSet.setPropertyValue(*pEntry, rValue, aItemSet);

            if (i + 1 == aEntries.size() || bPropertyCausesSideEffectsInNodes)
                SwUnoCursorHelper::SetCursorAttr(rPaM, aItemSet, nAttrMode, false/*bTableMode*/);

            bPreviousPropertyCausesSideEffectsInNodes = bPropertyCausesSideEffectsInNodes;
        }
    }

    if (!aUnknownExMsg.isEmpty())
        throw beans::UnknownPropertyException(aUnknownExMsg, static_cast<cppu::OWeakObject *>(nullptr));
    if (!aPropertyVetoExMsg.isEmpty())
        throw beans::PropertyVetoException(aPropertyVetoExMsg, static_cast<cppu::OWeakObject *>(nullptr));
}

namespace
{
    bool NotInRange(sal_uInt16 nWID, sal_uInt16 nStart, sal_uInt16 nEnd)
    {
        return nWID < nStart || nWID > nEnd;
    }
}

uno::Sequence< beans::PropertyState >
SwUnoCursorHelper::GetPropertyStates(
            SwPaM& rPaM, const SfxItemPropertySet& rPropSet,
            const uno::Sequence< OUString >& rPropertyNames,
            const SwGetPropertyStatesCaller eCaller)
{
    const OUString* pNames = rPropertyNames.getConstArray();
    uno::Sequence< beans::PropertyState > aRet(rPropertyNames.getLength());
    beans::PropertyState* pStates = aRet.getArray();
    const SfxItemPropertyMap &rMap = rPropSet.getPropertyMap();
    std::unique_ptr<SfxItemSet> pSet;
    std::unique_ptr<SfxItemSet> pSetParent;

    for (sal_Int32 i = 0, nEnd = rPropertyNames.getLength(); i < nEnd; i++)
    {
        SfxItemPropertySimpleEntry const*const pEntry =
                rMap.getByName( pNames[i] );
        if(!pEntry)
        {
            if (pNames[i] == UNO_NAME_IS_SKIP_HIDDEN_TEXT ||
                pNames[i] == UNO_NAME_IS_SKIP_PROTECTED_TEXT)
            {
                pStates[i] = beans::PropertyState_DEFAULT_VALUE;
                continue;
            }
            else if (SW_PROPERTY_STATE_CALLER_SWX_TEXT_PORTION_TOLERANT ==
                        eCaller)
            {
                //this values marks the element as unknown property
                pStates[i] = beans::PropertyState::PropertyState_MAKE_FIXED_SIZE;
                continue;
            }
            else
            {
                throw beans::UnknownPropertyException(
                    "Unknown property: " + pNames[i],
                    static_cast<cppu::OWeakObject *>(nullptr));
            }
        }
        if (((SW_PROPERTY_STATE_CALLER_SWX_TEXT_PORTION == eCaller)  ||
             (SW_PROPERTY_STATE_CALLER_SWX_TEXT_PORTION_TOLERANT == eCaller)) &&
            NotInRange(pEntry->nWID, FN_UNO_RANGE_BEGIN, FN_UNO_RANGE_END) &&
            NotInRange(pEntry->nWID, RES_CHRATR_BEGIN, RES_TXTATR_END) )
        {
            pStates[i] = beans::PropertyState_DEFAULT_VALUE;
        }
        else
        {
            if ( pEntry->nWID >= FN_UNO_RANGE_BEGIN &&
                 pEntry->nWID <= FN_UNO_RANGE_END )
            {
                (void)SwUnoCursorHelper::getCursorPropertyValue(
                    *pEntry, rPaM, nullptr, pStates[i] );
            }
            else
            {
                if (!pSet)
                {
                    switch ( eCaller )
                    {
                        case SW_PROPERTY_STATE_CALLER_SWX_TEXT_PORTION_TOLERANT:
                        case SW_PROPERTY_STATE_CALLER_SWX_TEXT_PORTION:
                            pSet.reset(
                                new SfxItemSet( rPaM.GetDoc()->GetAttrPool(),
                                    svl::Items<RES_CHRATR_BEGIN,   RES_TXTATR_END>{} ));
                        break;
                        case SW_PROPERTY_STATE_CALLER_SINGLE_VALUE_ONLY:
                            pSet.reset(
                                new SfxItemSet( rPaM.GetDoc()->GetAttrPool(),
                                    {{pEntry->nWID, pEntry->nWID}} ));
                        break;
                        default:
                            pSet.reset( new SfxItemSet(
                                rPaM.GetDoc()->GetAttrPool(),
                                svl::Items<
                                    RES_CHRATR_BEGIN, RES_FRMATR_END - 1,
                                    RES_UNKNOWNATR_CONTAINER,
                                        RES_UNKNOWNATR_CONTAINER>{}));
                    }
                    // #i63870#
                    SwUnoCursorHelper::GetCursorAttr( rPaM, *pSet );
                }

                pStates[i] = ( pSet->Count() )
                    ? rPropSet.getPropertyState( *pEntry, *pSet )
                    : beans::PropertyState_DEFAULT_VALUE;

                //try again to find out if a value has been inherited
                if( beans::PropertyState_DIRECT_VALUE == pStates[i] )
                {
                    if (!pSetParent)
                    {
                        pSetParent = pSet->Clone( false );
                        // #i63870#
                        SwUnoCursorHelper::GetCursorAttr(
                                rPaM, *pSetParent, true, false );
                    }

                    pStates[i] = ( pSetParent->Count() )
                        ? rPropSet.getPropertyState( *pEntry, *pSetParent )
                        : beans::PropertyState_DEFAULT_VALUE;
                }
            }
        }
    }
    return aRet;
}

beans::PropertyState SwUnoCursorHelper::GetPropertyState(
    SwPaM& rPaM, const SfxItemPropertySet& rPropSet,
    const OUString& rPropertyName)
{
    uno::Sequence< OUString > aStrings { rPropertyName };
    uno::Sequence< beans::PropertyState > aSeq =
        GetPropertyStates(rPaM, rPropSet, aStrings,
                SW_PROPERTY_STATE_CALLER_SINGLE_VALUE_ONLY );
    return aSeq[0];
}

static void
lcl_SelectParaAndReset( SwPaM &rPaM, SwDoc & rDoc,
                        std::set<sal_uInt16> const &rWhichIds )
{
    // if we are resetting paragraph attributes, we need to select the full paragraph first
    SwPosition aStart = *rPaM.Start();
    SwPosition aEnd = *rPaM.End();
    auto pTemp ( rDoc.CreateUnoCursor(aStart) );
    if(!SwUnoCursorHelper::IsStartOfPara(*pTemp))
    {
        pTemp->MovePara(GoCurrPara, fnParaStart);
    }
    pTemp->SetMark();
    *pTemp->GetPoint() = aEnd;
    SwUnoCursorHelper::SelectPam(*pTemp, true);
    if(!SwUnoCursorHelper::IsEndOfPara(*pTemp))
    {
        pTemp->MovePara(GoCurrPara, fnParaEnd);
    }
    rDoc.ResetAttrs(*pTemp, true, rWhichIds);
}

void SwUnoCursorHelper::SetPropertyToDefault(
    SwPaM& rPaM, const SfxItemPropertySet& rPropSet,
    const OUString& rPropertyName)
{
    SwDoc & rDoc = *rPaM.GetDoc();
    SfxItemPropertySimpleEntry const*const pEntry =
        rPropSet.getPropertyMap().getByName(rPropertyName);
    if (!pEntry)
    {
        throw beans::UnknownPropertyException(
            "Unknown property: " + rPropertyName,
            static_cast<cppu::OWeakObject *>(nullptr));
    }

    if (pEntry->nFlags & beans::PropertyAttribute::READONLY)
    {
        throw uno::RuntimeException(
                "setPropertyToDefault: property is read-only: "
                + rPropertyName, nullptr);
    }

    if (pEntry->nWID < RES_FRMATR_END)
    {
        std::set<sal_uInt16> aWhichIds;
        aWhichIds.insert( pEntry->nWID );
        if (pEntry->nWID < RES_PARATR_BEGIN)
        {
            rDoc.ResetAttrs(rPaM, true, aWhichIds);
        }
        else
        {
            lcl_SelectParaAndReset ( rPaM, rDoc, aWhichIds );
        }
    }
    else
    {
        SwUnoCursorHelper::resetCursorPropertyValue(*pEntry, rPaM);
    }
}

uno::Any SwUnoCursorHelper::GetPropertyDefault(
    SwPaM const & rPaM, const SfxItemPropertySet& rPropSet,
    const OUString& rPropertyName)
{
    SfxItemPropertySimpleEntry const*const pEntry =
        rPropSet.getPropertyMap().getByName(rPropertyName);
    if (!pEntry)
    {
        throw beans::UnknownPropertyException(
            "Unknown property: " + rPropertyName, static_cast<cppu::OWeakObject *>(nullptr));
    }

    uno::Any aRet;
    if (pEntry->nWID < RES_FRMATR_END)
    {
        SwDoc & rDoc = *rPaM.GetDoc();
        const SfxPoolItem& rDefItem =
            rDoc.GetAttrPool().GetDefaultItem(pEntry->nWID);
        rDefItem.QueryValue(aRet, pEntry->nMemberId);
    }
    return aRet;
}

uno::Reference< beans::XPropertySetInfo > SAL_CALL
SwXTextCursor::getPropertySetInfo()
{
    SolarMutexGuard g;

    static uno::Reference< beans::XPropertySetInfo >  xRef = [&]()
    {
        static SfxItemPropertyMapEntry const aCursorExtMap_Impl[] =
        {
            { OUString(UNO_NAME_IS_SKIP_HIDDEN_TEXT), FN_SKIP_HIDDEN_TEXT, cppu::UnoType<bool>::get(), PROPERTY_NONE,     0},
            { OUString(UNO_NAME_IS_SKIP_PROTECTED_TEXT), FN_SKIP_PROTECTED_TEXT, cppu::UnoType<bool>::get(), PROPERTY_NONE,     0},
            { OUString(), 0, css::uno::Type(), 0, 0 }
        };
        const uno::Reference< beans::XPropertySetInfo >  xInfo =
            m_pImpl->m_rPropSet.getPropertySetInfo();
        // extend PropertySetInfo!
        const uno::Sequence<beans::Property> aPropSeq = xInfo->getProperties();
        return new SfxExtItemPropertySetInfo(
            aCursorExtMap_Impl,
            aPropSeq );
    }();
    return xRef;
}

void SAL_CALL
SwXTextCursor::setPropertyValue(
        const OUString& rPropertyName, const uno::Any& rValue)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    if (rPropertyName == UNO_NAME_IS_SKIP_HIDDEN_TEXT)
    {
        bool bSet(false);
        if (!(rValue >>= bSet))
        {
            throw lang::IllegalArgumentException();
        }
        rUnoCursor.SetSkipOverHiddenSections(bSet);
    }
    else if (rPropertyName == UNO_NAME_IS_SKIP_PROTECTED_TEXT)
    {
        bool bSet(false);
        if (!(rValue >>= bSet))
        {
            throw lang::IllegalArgumentException();
        }
        rUnoCursor.SetSkipOverProtectSections(bSet);
    }
    else
    {
        SwUnoCursorHelper::SetPropertyValue(rUnoCursor,
                m_pImpl->m_rPropSet, rPropertyName, rValue);
    }
}

uno::Any SAL_CALL
SwXTextCursor::getPropertyValue(const OUString& rPropertyName)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    uno::Any aAny;
    if (rPropertyName == UNO_NAME_IS_SKIP_HIDDEN_TEXT)
    {
        const bool bSet = rUnoCursor.IsSkipOverHiddenSections();
        aAny <<= bSet;
    }
    else if (rPropertyName == UNO_NAME_IS_SKIP_PROTECTED_TEXT)
    {
        const bool bSet = rUnoCursor.IsSkipOverProtectSections();
        aAny <<= bSet;
    }
    else
    {
        aAny = SwUnoCursorHelper::GetPropertyValue(rUnoCursor,
                m_pImpl->m_rPropSet, rPropertyName);
    }
    return aAny;
}

void SAL_CALL
SwXTextCursor::addPropertyChangeListener(
        const OUString& /*rPropertyName*/,
        const uno::Reference< beans::XPropertyChangeListener >& /*xListener*/)
{
    OSL_FAIL("SwXTextCursor::addPropertyChangeListener(): not implemented");
}

void SAL_CALL
SwXTextCursor::removePropertyChangeListener(
        const OUString& /*rPropertyName*/,
        const uno::Reference< beans::XPropertyChangeListener >& /*xListener*/)
{
    OSL_FAIL("SwXTextCursor::removePropertyChangeListener(): not implemented");
}

void SAL_CALL
SwXTextCursor::addVetoableChangeListener(
        const OUString& /*rPropertyName*/,
        const uno::Reference< beans::XVetoableChangeListener >& /*xListener*/)
{
    OSL_FAIL("SwXTextCursor::addVetoableChangeListener(): not implemented");
}

void SAL_CALL
SwXTextCursor::removeVetoableChangeListener(
        const OUString& /*rPropertyName*/,
        const uno::Reference< beans::XVetoableChangeListener >& /*xListener*/)
{
    OSL_FAIL("SwXTextCursor::removeVetoableChangeListener(): not implemented");
}

beans::PropertyState SAL_CALL
SwXTextCursor::getPropertyState(const OUString& rPropertyName)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    const beans::PropertyState eRet = SwUnoCursorHelper::GetPropertyState(
            rUnoCursor, m_pImpl->m_rPropSet, rPropertyName);
    return eRet;
}

uno::Sequence< beans::PropertyState > SAL_CALL
SwXTextCursor::getPropertyStates(
        const uno::Sequence< OUString >& rPropertyNames)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    return SwUnoCursorHelper::GetPropertyStates(
            rUnoCursor, m_pImpl->m_rPropSet, rPropertyNames);
}

void SAL_CALL
SwXTextCursor::setPropertyToDefault(const OUString& rPropertyName)
{
    // forward: need no solar mutex here
    uno::Sequence < OUString > aSequence ( &rPropertyName, 1 );
    setPropertiesToDefault ( aSequence );
}

uno::Any SAL_CALL
SwXTextCursor::getPropertyDefault(const OUString& rPropertyName)
{
    // forward: need no solar mutex here
    const uno::Sequence < OUString > aSequence ( &rPropertyName, 1 );
    return getPropertyDefaults ( aSequence ).getConstArray()[0];
}

void SAL_CALL SwXTextCursor::setPropertyValues(
    const uno::Sequence< OUString >& aPropertyNames,
    const uno::Sequence< uno::Any >& aValues )
{
    if( aValues.getLength() != aPropertyNames.getLength() )
    {
        OSL_FAIL( "mis-matched property value sequences" );
        throw lang::IllegalArgumentException();
    }

    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    // a little lame to have to copy into this.
    uno::Sequence< beans::PropertyValue > aPropertyValues( aValues.getLength() );
    for ( sal_Int32 i = 0; i < aPropertyNames.getLength(); i++ )
    {
        if ( aPropertyNames[ i ] == UNO_NAME_IS_SKIP_HIDDEN_TEXT ||
             aPropertyNames[ i ] == UNO_NAME_IS_SKIP_PROTECTED_TEXT )
        {
            // the behaviour of these is hard to model in a group
            OSL_FAIL("invalid property name for batch setting");
            throw lang::IllegalArgumentException();
        }
        aPropertyValues[ i ].Name = aPropertyNames[ i ];
        aPropertyValues[ i ].Value = aValues[ i ];
    }
    try
    {
        SwUnoCursorHelper::SetPropertyValues( rUnoCursor, m_pImpl->m_rPropSet, aPropertyValues );
    }
    catch (const css::beans::UnknownPropertyException& e)
    {
        uno::Any a(cppu::getCaughtException());
        throw lang::WrappedTargetException(
            "wrapped Exception " + e.Message,
            uno::Reference<uno::XInterface>(), a);
    }
}

uno::Sequence< uno::Any > SAL_CALL
SwXTextCursor::getPropertyValues( const uno::Sequence< OUString >& aPropertyNames )
{
    // a banal implementation for now
    uno::Sequence< uno::Any > aValues( aPropertyNames.getLength() );
    std::transform(aPropertyNames.begin(), aPropertyNames.end(), aValues.begin(),
        [this](const OUString& rName) -> uno::Any { return getPropertyValue( rName ); });
    return aValues;
}

void SAL_CALL SwXTextCursor::addPropertiesChangeListener(
        const uno::Sequence< OUString >& /* aPropertyNames */,
        const uno::Reference< css::beans::XPropertiesChangeListener >& /* xListener */ )
{
    OSL_FAIL("SwXTextCursor::addPropertiesChangeListener(): not implemented");
}
void SAL_CALL SwXTextCursor::removePropertiesChangeListener(
        const uno::Reference< css::beans::XPropertiesChangeListener >& /* xListener */ )
{
    OSL_FAIL("SwXTextCursor::removePropertiesChangeListener(): not implemented");
}

void SAL_CALL SwXTextCursor::firePropertiesChangeEvent(
        const uno::Sequence< OUString >& /* aPropertyNames */,
        const uno::Reference< css::beans::XPropertiesChangeListener >& /* xListener */ )
{
    OSL_FAIL("SwXTextCursor::firePropertiesChangeEvent(): not implemented");
}

// para specific attribute ranges
static sal_uInt16 g_ParaResetableSetRange[] = {
    RES_FRMATR_BEGIN, RES_FRMATR_END-1,
    RES_PARATR_BEGIN, RES_PARATR_END-1,
    RES_PARATR_LIST_BEGIN, RES_PARATR_LIST_END-1,
    RES_UNKNOWNATR_BEGIN, RES_UNKNOWNATR_END-1,
    0
};

// selection specific attribute ranges
static sal_uInt16 g_ResetableSetRange[] = {
    RES_CHRATR_BEGIN, RES_CHRATR_END-1,
    RES_TXTATR_INETFMT, RES_TXTATR_INETFMT,
    RES_TXTATR_CHARFMT, RES_TXTATR_CHARFMT,
    RES_TXTATR_CJK_RUBY, RES_TXTATR_CJK_RUBY,
    RES_TXTATR_UNKNOWN_CONTAINER, RES_TXTATR_UNKNOWN_CONTAINER,
    0
};

static void
lcl_EnumerateIds(sal_uInt16 const* pIdRange, std::set<sal_uInt16> &rWhichIds)
{
    while (*pIdRange)
    {
        const sal_uInt16 nStart = *pIdRange++;
        const sal_uInt16 nEnd   = *pIdRange++;
        for (sal_uInt16 nId = nStart + 1;  nId <= nEnd;  ++nId)
        {
            rWhichIds.insert( rWhichIds.end(), nId );
        }
    }
}

void SAL_CALL
SwXTextCursor::setAllPropertiesToDefault()
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    std::set<sal_uInt16> aParaWhichIds;
    std::set<sal_uInt16> aWhichIds;
    lcl_EnumerateIds(g_ParaResetableSetRange, aParaWhichIds);
    lcl_EnumerateIds(g_ResetableSetRange, aWhichIds);
    if (!aParaWhichIds.empty())
    {
        lcl_SelectParaAndReset(rUnoCursor, *rUnoCursor.GetDoc(),
            aParaWhichIds);
    }
    if (!aWhichIds.empty())
    {
        rUnoCursor.GetDoc()->ResetAttrs(rUnoCursor, true, aWhichIds);
    }
}

void SAL_CALL
SwXTextCursor::setPropertiesToDefault(
        const uno::Sequence< OUString >& rPropertyNames)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    if ( rPropertyNames.hasElements() )
    {
        SwDoc & rDoc = *rUnoCursor.GetDoc();
        std::set<sal_uInt16> aWhichIds;
        std::set<sal_uInt16> aParaWhichIds;
        for (const OUString& rName : rPropertyNames)
        {
            SfxItemPropertySimpleEntry const*const  pEntry =
                m_pImpl->m_rPropSet.getPropertyMap().getByName( rName );
            if (!pEntry)
            {
                if (rName == UNO_NAME_IS_SKIP_HIDDEN_TEXT ||
                    rName == UNO_NAME_IS_SKIP_PROTECTED_TEXT)
                {
                    continue;
                }
                throw beans::UnknownPropertyException(
                    "Unknown property: " + rName,
                    static_cast<cppu::OWeakObject *>(this));
            }
            if (pEntry->nFlags & beans::PropertyAttribute::READONLY)
            {
                throw uno::RuntimeException(
                    "setPropertiesToDefault: property is read-only: " + rName,
                    static_cast<cppu::OWeakObject *>(this));
            }

            if (pEntry->nWID < RES_FRMATR_END)
            {
                if (pEntry->nWID < RES_PARATR_BEGIN)
                {
                    aWhichIds.insert( pEntry->nWID );
                }
                else
                {
                    aParaWhichIds.insert( pEntry->nWID );
                }
            }
            else if (pEntry->nWID == FN_UNO_NUM_START_VALUE)
            {
                SwUnoCursorHelper::resetCursorPropertyValue(*pEntry, rUnoCursor);
            }
        }

        if (!aParaWhichIds.empty())
        {
            lcl_SelectParaAndReset(rUnoCursor, rDoc, aParaWhichIds);
        }
        if (!aWhichIds.empty())
        {
            rDoc.ResetAttrs(rUnoCursor, true, aWhichIds);
        }
    }
}

uno::Sequence< uno::Any > SAL_CALL
SwXTextCursor::getPropertyDefaults(
        const uno::Sequence< OUString >& rPropertyNames)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    const sal_Int32 nCount = rPropertyNames.getLength();
    uno::Sequence< uno::Any > aRet(nCount);
    if ( nCount )
    {
        SwDoc & rDoc = *rUnoCursor.GetDoc();
        const OUString *pNames = rPropertyNames.getConstArray();
        uno::Any *pAny = aRet.getArray();
        for (sal_Int32 i = 0; i < nCount; i++)
        {
            SfxItemPropertySimpleEntry const*const pEntry =
                m_pImpl->m_rPropSet.getPropertyMap().getByName( pNames[i] );
            if (!pEntry)
            {
                if (pNames[i] == UNO_NAME_IS_SKIP_HIDDEN_TEXT ||
                    pNames[i] == UNO_NAME_IS_SKIP_PROTECTED_TEXT)
                {
                    continue;
                }
                throw beans::UnknownPropertyException(
                    "Unknown property: " + pNames[i],
                    static_cast<cppu::OWeakObject *>(nullptr));
            }
            if (pEntry->nWID < RES_FRMATR_END)
            {
                const SfxPoolItem& rDefItem =
                    rDoc.GetAttrPool().GetDefaultItem(pEntry->nWID);
                rDefItem.QueryValue(pAny[i], pEntry->nMemberId);
            }
        }
    }
    return aRet;
}

void SAL_CALL SwXTextCursor::invalidateMarkings(::sal_Int32 nType)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    SwNode& node = rUnoCursor.GetNode();

    SwTextNode* txtNode = node.GetTextNode();

    if (txtNode == nullptr) return;

    if ( text::TextMarkupType::SPELLCHECK == nType )
    {
        txtNode->SetWrongDirty(SwTextNode::WrongState::TODO);
        txtNode->SetWrong(nullptr);
    }
    else if( text::TextMarkupType::PROOFREADING == nType )
    {
        txtNode->SetGrammarCheckDirty(true);
        txtNode->SetGrammarCheck(nullptr);
    }
    else if ( text::TextMarkupType::SMARTTAG == nType )
    {
        txtNode->SetSmartTagDirty(true);
        txtNode->SetSmartTags(nullptr);
    }
    else return;

    SwFormatColl* fmtColl=txtNode->GetFormatColl();

    if (fmtColl == nullptr) return;

    SwFormatChg aNew( fmtColl );
    txtNode->NotifyClients( nullptr, &aNew );
}

void SAL_CALL
SwXTextCursor::makeRedline(
    const OUString& rRedlineType,
    const uno::Sequence< beans::PropertyValue >& rRedlineProperties)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    SwUnoCursorHelper::makeRedline(rUnoCursor, rRedlineType, rRedlineProperties);
}

void SAL_CALL SwXTextCursor::insertDocumentFromURL(const OUString& rURL,
    const uno::Sequence< beans::PropertyValue >& rOptions)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    SwUnoCursorHelper::InsertFile(&rUnoCursor, rURL, rOptions);
}

uno::Sequence< beans::PropertyValue >
SwUnoCursorHelper::CreateSortDescriptor(const bool bFromTable)
{
    uno::Sequence< beans::PropertyValue > aRet(5);
    beans::PropertyValue* pArray = aRet.getArray();

    uno::Any aVal;
    aVal <<= bFromTable;
    pArray[0] = beans::PropertyValue("IsSortInTable", -1, aVal,
                    beans::PropertyState_DIRECT_VALUE);

    aVal <<= u' ';
    pArray[1] = beans::PropertyValue("Delimiter", -1, aVal,
                    beans::PropertyState_DIRECT_VALUE);

    aVal <<= false;
    pArray[2] = beans::PropertyValue("IsSortColumns", -1, aVal,
                    beans::PropertyState_DIRECT_VALUE);

    aVal <<= sal_Int32(3);
    pArray[3] = beans::PropertyValue("MaxSortFieldsCount", -1, aVal,
                    beans::PropertyState_DIRECT_VALUE);

    uno::Sequence< table::TableSortField > aFields(3);
    table::TableSortField* pFields = aFields.getArray();

    lang::Locale aLang( SvtSysLocale().GetLanguageTag().getLocale());
    // get collator algorithm to be used for the locale
    uno::Sequence< OUString > aSeq(
            GetAppCollator().listCollatorAlgorithms( aLang ) );
    const bool bHasElements = aSeq.hasElements();
    OSL_ENSURE( bHasElements, "list of collator algorithms is empty!");
    OUString aCollAlg;
    if (bHasElements)
    {
        aCollAlg = aSeq.getConstArray()[0];
    }

    pFields[0].Field = 1;
    pFields[0].IsAscending = true;
    pFields[0].IsCaseSensitive = false;
    pFields[0].FieldType = table::TableSortFieldType_ALPHANUMERIC;
    pFields[0].CollatorLocale = aLang;
    pFields[0].CollatorAlgorithm = aCollAlg;

    pFields[1].Field = 1;
    pFields[1].IsAscending = true;
    pFields[1].IsCaseSensitive = false;
    pFields[1].FieldType = table::TableSortFieldType_ALPHANUMERIC;
    pFields[1].CollatorLocale = aLang;
    pFields[1].CollatorAlgorithm = aCollAlg;

    pFields[2].Field = 1;
    pFields[2].IsAscending = true;
    pFields[2].IsCaseSensitive = false;
    pFields[2].FieldType = table::TableSortFieldType_ALPHANUMERIC;
    pFields[2].CollatorLocale = aLang;
    pFields[2].CollatorAlgorithm = aCollAlg;

    aVal <<= aFields;
    pArray[4] = beans::PropertyValue("SortFields", -1, aVal,
                    beans::PropertyState_DIRECT_VALUE);

    return aRet;
}

uno::Sequence< beans::PropertyValue > SAL_CALL
SwXTextCursor::createSortDescriptor()
{
    SolarMutexGuard aGuard;

    return SwUnoCursorHelper::CreateSortDescriptor(false);
}

bool SwUnoCursorHelper::ConvertSortProperties(
    const uno::Sequence< beans::PropertyValue >& rDescriptor,
    SwSortOptions& rSortOpt)
{
    bool bRet = true;

    rSortOpt.bTable = false;
    rSortOpt.cDeli = ' ';
    rSortOpt.eDirection = SRT_COLUMNS;  //!! UI text may be contrary though !!

    std::unique_ptr<SwSortKey> pKey1(new SwSortKey);
    pKey1->nColumnId = USHRT_MAX;
    pKey1->bIsNumeric = true;
    pKey1->eSortOrder = SRT_ASCENDING;

    std::unique_ptr<SwSortKey> pKey2(new SwSortKey);
    pKey2->nColumnId = USHRT_MAX;
    pKey2->bIsNumeric = true;
    pKey2->eSortOrder = SRT_ASCENDING;

    std::unique_ptr<SwSortKey> pKey3(new SwSortKey);
    pKey3->nColumnId = USHRT_MAX;
    pKey3->bIsNumeric = true;
    pKey3->eSortOrder = SRT_ASCENDING;
    SwSortKey* aKeys[3] = {pKey1.get(), pKey2.get(), pKey3.get()};

    bool bOldSortdescriptor(false);
    bool bNewSortdescriptor(false);

    for (const beans::PropertyValue& rProperty : rDescriptor)
    {
        uno::Any aValue( rProperty.Value );
        const OUString& rPropName = rProperty.Name;

        // old and new sortdescriptor
        if ( rPropName == "IsSortInTable" )
        {
            if (auto b = o3tl::tryAccess<bool>(aValue))
            {
                rSortOpt.bTable = *b;
            }
            else
            {
                bRet = false;
            }
        }
        else if ( rPropName == "Delimiter" )
        {
            sal_Unicode uChar;
            sal_uInt16 nChar;
            if (aValue >>= uChar)
            {
                rSortOpt.cDeli = uChar;
            }
            else if (aValue >>= nChar)
            {
                // For compatibility with BASIC, also accept an ANY containing
                // an UNSIGNED SHORT:
                rSortOpt.cDeli = nChar;
            }
            else
            {
                bRet = false;
            }
        }
        // old sortdescriptor
        else if ( rPropName == "SortColumns" )
        {
            bOldSortdescriptor = true;
            bool bTemp(false);
            if (aValue >>= bTemp)
            {
                rSortOpt.eDirection = bTemp ? SRT_COLUMNS : SRT_ROWS;
            }
            else
            {
                bRet = false;
            }
        }
        else if ( rPropName == "IsCaseSensitive" )
        {
            bOldSortdescriptor = true;
            bool bTemp(false);
            if (aValue >>= bTemp)
            {
                rSortOpt.bIgnoreCase = !bTemp;
            }
            else
            {
                bRet = false;
            }
        }
        else if ( rPropName == "CollatorLocale" )
        {
            bOldSortdescriptor = true;
            lang::Locale aLocale;
            if (aValue >>= aLocale)
            {
                rSortOpt.nLanguage = LanguageTag::convertToLanguageType( aLocale);
            }
            else
            {
                bRet = false;
            }
        }
        else if (rPropName.startsWith("CollatorAlgorithm") &&
            rPropName.getLength() == 18 &&
            (rPropName[17] >= '0' && rPropName[17] <= '9'))
        {
            bOldSortdescriptor = true;
            sal_uInt16 nIndex = rPropName[17];
            nIndex -= '0';
            OUString aText;
            if ((aValue >>= aText) && nIndex < 3)
            {
                aKeys[nIndex]->sSortType = aText;
            }
            else
            {
                bRet = false;
            }
        }
        else if (rPropName.startsWith("SortRowOrColumnNo") &&
            rPropName.getLength() == 18 &&
            (rPropName[17] >= '0' && rPropName[17] <= '9'))
        {
            bOldSortdescriptor = true;
            sal_uInt16 nIndex = rPropName[17];
            nIndex -= '0';
            sal_Int16 nCol = -1;
            if (aValue.getValueType() == ::cppu::UnoType<sal_Int16>::get()
                && nIndex < 3)
            {
                aValue >>= nCol;
            }
            if (nCol >= 0)
            {
                aKeys[nIndex]->nColumnId = nCol;
            }
            else
            {
                bRet = false;
            }
        }
        else if (rPropName.startsWith("IsSortNumeric") &&
            rPropName.getLength() == 14 &&
            (rPropName[13] >= '0' && rPropName[13] <= '9'))
        {
            bOldSortdescriptor = true;
            sal_uInt16 nIndex = rPropName[13];
            nIndex = nIndex - '0';
            auto bTemp = o3tl::tryAccess<bool>(aValue);
            if (bTemp && nIndex < 3)
            {
                aKeys[nIndex]->bIsNumeric = *bTemp;
            }
            else
            {
                bRet = false;
            }
        }
        else if (rPropName.startsWith("IsSortAscending") &&
            rPropName.getLength() == 16 &&
            (rPropName[15] >= '0' && rPropName[15] <= '9'))
        {
            bOldSortdescriptor = true;
            sal_uInt16 nIndex = rPropName[15];
            nIndex -= '0';
            auto bTemp = o3tl::tryAccess<bool>(aValue);
            if (bTemp && nIndex < 3)
            {
                aKeys[nIndex]->eSortOrder = (*bTemp)
                    ? SRT_ASCENDING : SRT_DESCENDING;
            }
            else
            {
                bRet = false;
            }
        }
        // new sortdescriptor
        else if ( rPropName == "IsSortColumns" )
        {
            bNewSortdescriptor = true;
            if (auto bTemp = o3tl::tryAccess<bool>(aValue))
            {
                rSortOpt.eDirection = *bTemp ? SRT_COLUMNS : SRT_ROWS;
            }
            else
            {
                bRet = false;
            }
        }
        else if ( rPropName == "SortFields" )
        {
            bNewSortdescriptor = true;
            uno::Sequence < table::TableSortField > aFields;
            if (aValue >>= aFields)
            {
                sal_Int32 nCount(aFields.getLength());
                if (nCount <= 3)
                {
                    table::TableSortField* pFields = aFields.getArray();
                    for (sal_Int32 i = 0; i < nCount; ++i)
                    {
                        rSortOpt.bIgnoreCase = !pFields[i].IsCaseSensitive;
                        rSortOpt.nLanguage =
                            LanguageTag::convertToLanguageType( pFields[i].CollatorLocale );
                        aKeys[i]->sSortType = pFields[i].CollatorAlgorithm;
                        aKeys[i]->nColumnId =
                            static_cast<sal_uInt16>(pFields[i].Field);
                        aKeys[i]->bIsNumeric = (pFields[i].FieldType ==
                                table::TableSortFieldType_NUMERIC);
                        aKeys[i]->eSortOrder = (pFields[i].IsAscending)
                            ? SRT_ASCENDING : SRT_DESCENDING;
                    }
                }
                else
                {
                    bRet = false;
                }
            }
            else
            {
                bRet = false;
            }
        }
    }

    if (bNewSortdescriptor && bOldSortdescriptor)
    {
        OSL_FAIL("someone tried to set the old deprecated and "
            "the new sortdescriptor");
        bRet = false;
    }

    if (pKey1->nColumnId != USHRT_MAX)
    {
        rSortOpt.aKeys.push_back(std::move(pKey1));
    }
    if (pKey2->nColumnId != USHRT_MAX)
    {
        rSortOpt.aKeys.push_back(std::move(pKey2));
    }
    if (pKey3->nColumnId != USHRT_MAX)
    {
        rSortOpt.aKeys.push_back(std::move(pKey3));
    }

    return bRet && !rSortOpt.aKeys.empty();
}

void SAL_CALL
SwXTextCursor::sort(const uno::Sequence< beans::PropertyValue >& rDescriptor)
{
    SolarMutexGuard aGuard;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    if (!rUnoCursor.HasMark())
        return;

    SwSortOptions aSortOpt;
    if (!SwUnoCursorHelper::ConvertSortProperties(rDescriptor, aSortOpt))
    {
        throw uno::RuntimeException("Bad sort properties");
    }
    UnoActionContext aContext( rUnoCursor.GetDoc() );

    SwPosition & rStart = *rUnoCursor.Start();
    SwPosition & rEnd   = *rUnoCursor.End();

    SwNodeIndex aPrevIdx( rStart.nNode, -1 );
    const sal_uLong nOffset = rEnd.nNode.GetIndex() - rStart.nNode.GetIndex();
    const sal_Int32 nCntStt  = rStart.nContent.GetIndex();

    rUnoCursor.GetDoc()->SortText(rUnoCursor, aSortOpt);

    // update selection
    rUnoCursor.DeleteMark();
    rUnoCursor.GetPoint()->nNode.Assign( aPrevIdx.GetNode(), +1 );
    SwContentNode *const pCNd = rUnoCursor.GetContentNode();
    sal_Int32 nLen = pCNd->Len();
    if (nLen > nCntStt)
    {
        nLen = nCntStt;
    }
    rUnoCursor.GetPoint()->nContent.Assign(pCNd, nLen );
    rUnoCursor.SetMark();

    rUnoCursor.GetPoint()->nNode += nOffset;
    SwContentNode *const pCNd2 = rUnoCursor.GetContentNode();
    rUnoCursor.GetPoint()->nContent.Assign( pCNd2, pCNd2->Len() );

}

uno::Reference< container::XEnumeration > SAL_CALL
SwXTextCursor::createContentEnumeration(const OUString& rServiceName)
{
    SolarMutexGuard g;
    if (rServiceName != "com.sun.star.text.TextContent")
        throw uno::RuntimeException();
    SwUnoCursor& rUnoCursor( m_pImpl->GetCursorOrThrow() );
    return SwXParaFrameEnumeration::Create(rUnoCursor, PARAFRAME_PORTION_TEXTRANGE);
}

uno::Reference< container::XEnumeration > SAL_CALL
SwXTextCursor::createEnumeration()
{
    SolarMutexGuard g;

    SwUnoCursor & rUnoCursor( m_pImpl->GetCursorOrThrow() );

    SwXText* pParentText = comphelper::getUnoTunnelImplementation<SwXText>(m_pImpl->m_xParentText);
    OSL_ENSURE(pParentText, "parent is not a SwXText");
    if (!pParentText)
    {
        throw uno::RuntimeException();
    }

    auto pNewCursor(rUnoCursor.GetDoc()->CreateUnoCursor(*rUnoCursor.GetPoint()) );
    if (rUnoCursor.HasMark())
    {
        pNewCursor->SetMark();
        *pNewCursor->GetMark() = *rUnoCursor.GetMark();
    }
    const CursorType eSetType = (CursorType::TableText == m_pImpl->m_eType)
            ? CursorType::SelectionInTable : CursorType::Selection;
    SwTableNode const*const pStartNode( (CursorType::TableText == m_pImpl->m_eType)
            ? rUnoCursor.GetPoint()->nNode.GetNode().FindTableNode()
            : nullptr);
    SwTable const*const pTable(
            pStartNode ? & pStartNode->GetTable() : nullptr );
    return SwXParagraphEnumeration::Create(pParentText, pNewCursor, eSetType, pStartNode, pTable);
}

uno::Type SAL_CALL
SwXTextCursor::getElementType()
{
    return cppu::UnoType<text::XTextRange>::get();
}

sal_Bool SAL_CALL SwXTextCursor::hasElements()
{
    return true;
}

uno::Sequence< OUString > SAL_CALL
SwXTextCursor::getAvailableServiceNames()
{
    uno::Sequence<OUString> aRet { "com.sun.star.text.TextContent" };
    return aRet;
}

IMPLEMENT_FORWARD_REFCOUNT( SwXTextCursor,SwXTextCursor_Base )

uno::Any SAL_CALL
SwXTextCursor::queryInterface(const uno::Type& rType)
{
    return (rType == cppu::UnoType<lang::XUnoTunnel>::get())
        ? OTextCursorHelper::queryInterface(rType)
        : SwXTextCursor_Base::queryInterface(rType);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
