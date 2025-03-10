/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 This file has been autogenerated by update_pch.sh. It is possible to edit it
 manually (such as when an include file has been moved/renamed/removed). All such
 manual changes will be rewritten by the next run of update_pch.sh (which presumably
 also fixes all possible problems, so it's usually better to use it).

 Generated on 2019-10-17 15:16:08 using:
 ./bin/update_pch sd sdui --cutoff=4 --exclude:system --include:module --include:local

 If after updating build fails, use the following command to locate conflicting headers:
 ./bin/update_pch_bisect ./sd/inc/pch/precompiled_sdui.hxx "make sd.build" --find-conflicts
*/

#if PCH_LEVEL >= 1
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <float.h>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iterator>
#include <limits.h>
#include <limits>
#include <list>
#include <map>
#include <math.h>
#include <memory>
#include <new>
#include <ostream>
#include <set>
#include <sstream>
#include <stddef.h>
#include <string.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>
#include <o3tl/optional.hxx>
#include <boost/property_tree/ptree.hpp>
#endif // PCH_LEVEL >= 1
#if PCH_LEVEL >= 2
#include <osl/diagnose.h>
#include <osl/doublecheckedlocking.h>
#include <osl/endian.h>
#include <osl/getglobalmutex.hxx>
#include <osl/interlck.h>
#include <osl/mutex.h>
#include <osl/mutex.hxx>
#include <rtl/alloc.h>
#include <rtl/instance.hxx>
#include <rtl/locale.h>
#include <rtl/math.h>
#include <rtl/math.hxx>
#include <rtl/ref.hxx>
#include <rtl/strbuf.h>
#include <rtl/strbuf.hxx>
#include <rtl/string.h>
#include <rtl/string.hxx>
#include <rtl/stringconcat.hxx>
#include <rtl/stringutils.hxx>
#include <rtl/textcvt.h>
#include <rtl/textenc.h>
#include <rtl/ustrbuf.h>
#include <rtl/ustrbuf.hxx>
#include <rtl/ustring.h>
#include <rtl/ustring.hxx>
#include <sal/config.h>
#include <sal/detail/log.h>
#include <sal/log.hxx>
#include <sal/macros.h>
#include <sal/mathconf.h>
#include <sal/saldllapi.h>
#include <sal/types.h>
#include <sal/typesizes.h>
#include <vcl/BitmapColor.hxx>
#include <vcl/EnumContext.hxx>
#include <vcl/GraphicExternalLink.hxx>
#include <vcl/GraphicObject.hxx>
#include <vcl/IDialogRenderable.hxx>
#include <vcl/NotebookBarAddonsMerger.hxx>
#include <vcl/Scanline.hxx>
#include <vcl/alpha.hxx>
#include <vcl/animate/Animation.hxx>
#include <vcl/animate/AnimationBitmap.hxx>
#include <vcl/bitmap.hxx>
#include <vcl/bitmapex.hxx>
#include <vcl/builder.hxx>
#include <vcl/cairo.hxx>
#include <vcl/checksum.hxx>
#include <vcl/ctrl.hxx>
#include <vcl/customweld.hxx>
#include <vcl/devicecoordinate.hxx>
#include <vcl/dllapi.h>
#include <vcl/dndhelp.hxx>
#include <vcl/dockwin.hxx>
#include <vcl/edit.hxx>
#include <vcl/errcode.hxx>
#include <vcl/floatwin.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/font.hxx>
#include <vcl/gfxlink.hxx>
#include <vcl/graph.hxx>
#include <vcl/idle.hxx>
#include <vcl/image.hxx>
#include <vcl/keycod.hxx>
#include <vcl/keycodes.hxx>
#include <vcl/lstbox.hxx>
#include <vcl/mapmod.hxx>
#include <vcl/menu.hxx>
#include <vcl/metaactiontypes.hxx>
#include <vcl/metric.hxx>
#include <vcl/outdev.hxx>
#include <vcl/outdevmap.hxx>
#include <vcl/outdevstate.hxx>
#include <vcl/region.hxx>
#include <vcl/salgtype.hxx>
#include <vcl/salnativewidgets.hxx>
#include <vcl/scopedbitmapaccess.hxx>
#include <vcl/svapp.hxx>
#include <vcl/syswin.hxx>
#include <vcl/task.hxx>
#include <vcl/textfilter.hxx>
#include <vcl/timer.hxx>
#include <vcl/uitest/factory.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/vclreferencebase.hxx>
#include <vcl/vectorgraphicdata.hxx>
#include <vcl/wall.hxx>
#include <vcl/weld.hxx>
#include <vcl/window.hxx>
#endif // PCH_LEVEL >= 2
#if PCH_LEVEL >= 3
#include <basegfx/basegfxdllapi.h>
#include <basegfx/color/bcolor.hxx>
#include <basegfx/numeric/ftools.hxx>
#include <basegfx/point/b2dpoint.hxx>
#include <basegfx/point/b2ipoint.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/range/b2drange.hxx>
#include <basegfx/range/basicrange.hxx>
#include <basegfx/tuple/b2dtuple.hxx>
#include <basegfx/tuple/b2i64tuple.hxx>
#include <basegfx/tuple/b2ituple.hxx>
#include <basegfx/tuple/b3dtuple.hxx>
#include <basegfx/vector/b2dsize.hxx>
#include <basegfx/vector/b2dvector.hxx>
#include <basegfx/vector/b2enums.hxx>
#include <basegfx/vector/b2ivector.hxx>
#include <com/sun/star/awt/GradientStyle.hpp>
#include <com/sun/star/awt/Key.hpp>
#include <com/sun/star/awt/KeyGroup.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/container/XNameContainer.hpp>
#include <com/sun/star/datatransfer/dnd/XDragGestureListener.hpp>
#include <com/sun/star/datatransfer/dnd/XDragSourceListener.hpp>
#include <com/sun/star/datatransfer/dnd/XDropTargetListener.hpp>
#include <com/sun/star/drawing/DashStyle.hpp>
#include <com/sun/star/drawing/HatchStyle.hpp>
#include <com/sun/star/drawing/LineCap.hpp>
#include <com/sun/star/drawing/TextFitToSizeType.hpp>
#include <com/sun/star/embed/XStorage.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/graphic/XPrimitive2D.hpp>
#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/lang/EventObject.hpp>
#include <com/sun/star/lang/Locale.hpp>
#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/lang/XTypeProvider.hpp>
#include <com/sun/star/lang/XUnoTunnel.hpp>
#include <com/sun/star/style/NumberingType.hpp>
#include <com/sun/star/style/XStyle.hpp>
#include <com/sun/star/text/textfield/Type.hpp>
#include <com/sun/star/uno/Any.h>
#include <com/sun/star/uno/Any.hxx>
#include <com/sun/star/uno/Exception.hpp>
#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/uno/RuntimeException.hpp>
#include <com/sun/star/uno/Sequence.h>
#include <com/sun/star/uno/Sequence.hxx>
#include <com/sun/star/uno/Type.h>
#include <com/sun/star/uno/Type.hxx>
#include <com/sun/star/uno/TypeClass.hdl>
#include <com/sun/star/uno/XAggregation.hpp>
#include <com/sun/star/uno/XInterface.hpp>
#include <com/sun/star/uno/XWeak.hpp>
#include <com/sun/star/uno/genfunc.h>
#include <com/sun/star/uno/genfunc.hxx>
#include <com/sun/star/util/Date.hpp>
#include <com/sun/star/util/DateTime.hpp>
#include <com/sun/star/util/Time.hpp>
#include <com/sun/star/util/XAccounting.hpp>
#include <comphelper/comphelperdllapi.h>
#include <comphelper/weak.hxx>
#include <cppu/cppudllapi.h>
#include <cppu/unotype.hxx>
#include <cppuhelper/basemutex.hxx>
#include <cppuhelper/compbase.hxx>
#include <cppuhelper/compbase_ex.hxx>
#include <cppuhelper/cppuhelperdllapi.h>
#include <cppuhelper/implbase.hxx>
#include <cppuhelper/implbase_ex.hxx>
#include <cppuhelper/implbase_ex_post.hxx>
#include <cppuhelper/implbase_ex_pre.hxx>
#include <cppuhelper/interfacecontainer.h>
#include <cppuhelper/weak.hxx>
#include <cppuhelper/weakagg.hxx>
#include <cppuhelper/weakref.hxx>
#include <drawinglayer/drawinglayerdllapi.h>
#include <drawinglayer/primitive2d/baseprimitive2d.hxx>
#include <editeng/editdata.hxx>
#include <editeng/editengdllapi.h>
#include <editeng/editstat.hxx>
#include <editeng/eedata.hxx>
#include <editeng/eeitem.hxx>
#include <editeng/outliner.hxx>
#include <editeng/paragraphdata.hxx>
#include <editeng/svxenum.hxx>
#include <editeng/svxfont.hxx>
#include <i18nlangtag/i18nlangtagdllapi.h>
#include <i18nlangtag/lang.h>
#include <o3tl/cow_wrapper.hxx>
#include <o3tl/deleter.hxx>
#include <o3tl/enumarray.hxx>
#include <o3tl/safeint.hxx>
#include <o3tl/sorted_vector.hxx>
#include <o3tl/strong_int.hxx>
#include <o3tl/typed_flags_set.hxx>
#include <o3tl/underlyingenumvalue.hxx>
#include <salhelper/simplereferenceobject.hxx>
#include <sfx2/basedlgs.hxx>
#include <sfx2/dllapi.h>
#include <sfx2/linksrc.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/shell.hxx>
#include <sot/formats.hxx>
#include <svl/SfxBroadcaster.hxx>
#include <svl/aeitem.hxx>
#include <svl/eitem.hxx>
#include <svl/hint.hxx>
#include <svl/intitem.hxx>
#include <svl/itempool.hxx>
#include <svl/itemset.hxx>
#include <svl/languageoptions.hxx>
#include <svl/lstner.hxx>
#include <svl/poolitem.hxx>
#include <svl/stritem.hxx>
#include <svl/style.hxx>
#include <svl/stylesheetuser.hxx>
#include <svl/svldllapi.h>
#include <svl/typedwhich.hxx>
#include <svl/undo.hxx>
#include <svtools/svtdllapi.h>
#include <svtools/unitconv.hxx>
#include <svtools/valueset.hxx>
#include <svx/DiagramDataInterface.hxx>
#include <svx/XPropertyEntry.hxx>
#include <svx/drawitem.hxx>
#include <svx/flagsdef.hxx>
#include <svx/itextprovider.hxx>
#include <svx/sdr/properties/defaultproperties.hxx>
#include <svx/sdr/properties/properties.hxx>
#include <svx/sdtaditm.hxx>
#include <svx/sdtaitm.hxx>
#include <svx/sdtakitm.hxx>
#include <svx/shapeproperty.hxx>
#include <svx/svddef.hxx>
#include <svx/svdoattr.hxx>
#include <svx/svdobj.hxx>
#include <svx/svdotext.hxx>
#include <svx/svdtext.hxx>
#include <svx/svdtrans.hxx>
#include <svx/svdtypes.hxx>
#include <svx/svxdllapi.h>
#include <svx/xdash.hxx>
#include <svx/xdef.hxx>
#include <svx/xgrad.hxx>
#include <svx/xhatch.hxx>
#include <svx/xtable.hxx>
#include <tools/color.hxx>
#include <tools/date.hxx>
#include <tools/datetime.hxx>
#include <tools/debug.hxx>
#include <tools/fldunit.hxx>
#include <tools/fontenum.hxx>
#include <tools/fract.hxx>
#include <tools/gen.hxx>
#include <tools/helpers.hxx>
#include <tools/lineend.hxx>
#include <tools/link.hxx>
#include <tools/mapunit.hxx>
#include <tools/poly.hxx>
#include <tools/ref.hxx>
#include <tools/solar.h>
#include <tools/stream.hxx>
#include <tools/time.hxx>
#include <tools/toolsdllapi.h>
#include <tools/urlobj.hxx>
#include <tools/weakbase.h>
#include <tools/weakbase.hxx>
#include <tools/wintypes.hxx>
#include <typelib/typeclass.h>
#include <typelib/typedescription.h>
#include <typelib/uik.h>
#include <uno/any2.h>
#include <uno/data.h>
#include <uno/sequence2.h>
#include <unotools/configitem.hxx>
#include <unotools/fontdefs.hxx>
#include <unotools/options.hxx>
#include <unotools/resmgr.hxx>
#include <unotools/syslocale.hxx>
#include <unotools/unotoolsdllapi.h>
#endif // PCH_LEVEL >= 3
#if PCH_LEVEL >= 4
#include <drawdoc.hxx>
#include <sddllapi.h>
#include <sdmod.hxx>
#include <sdpage.hxx>
#include <sdresid.hxx>
#endif // PCH_LEVEL >= 4

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
