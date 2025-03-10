/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_VCL_SKIA_SKIAHELPER_HXX
#define INCLUDED_VCL_SKIA_SKIAHELPER_HXX

#include <vcl/dllapi.h>

#include <config_features.h>

// All member functions static and VCL_DLLPUBLIC. Basically a glorified namespace.
struct VCL_DLLPUBLIC SkiaHelper
{
    SkiaHelper() = delete; // Should not be instantiated

public:
    static bool isVCLSkiaEnabled();

#if HAVE_FEATURE_SKIA
    // Which Skia backend to use.
    enum RenderMethod
    {
        RenderRaster,
        RenderVulkan
    };
    static RenderMethod renderMethodToUse();
    static void disableRenderMethod(RenderMethod method);
#endif
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
