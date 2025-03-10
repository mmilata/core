/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef VCL_INC_BITMAPFASTSCALEFILTER_HXX
#define VCL_INC_BITMAPFASTSCALEFILTER_HXX

#include <vcl/bitmapex.hxx>
#include <vcl/BitmapFilter.hxx>

class VCL_DLLPUBLIC BitmapFastScaleFilter final : public BitmapFilter
{
public:
    explicit BitmapFastScaleFilter(double fScaleX, double fScaleY)
        : mfScaleX(fScaleX)
        , mfScaleY(fScaleY)
    {
    }

    virtual BitmapEx execute(BitmapEx const& rBitmapEx) const override;

private:
    double const mfScaleX;
    double const mfScaleY;
    Size const maSize;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
