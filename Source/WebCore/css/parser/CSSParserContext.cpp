/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSParserContext.h"

#include "CSSPropertyNames.h"
#include "CSSValuePool.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "OriginAccessPatterns.h"
#include "Page.h"
#include "Settings.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// https://drafts.csswg.org/css-values/#url-local-url-flag
bool ResolvedURL::isLocalURL() const
{
    return specifiedURLString.startsWith('#');
}

const CSSParserContext& strictCSSParserContext()
{
    static MainThreadNeverDestroyed<CSSParserContext> strictContext(HTMLStandardMode);
    return strictContext;
}

CSSParserContext::CSSParserContext(CSSParserMode mode, const URL& baseURL)
    : baseURL(baseURL)
    , mode(mode)
{
    // FIXME: We should turn all of the features on from their WebCore Settings defaults.
    if (mode == UASheetMode) {
        colorMixEnabled = true;
        focusVisibleEnabled = true;
        popoverAttributeEnabled = true;
        propertySettings.cssContainmentEnabled = true;
        propertySettings.cssIndividualTransformPropertiesEnabled = true;
        propertySettings.cssInputSecurityEnabled = true;
        propertySettings.cssCounterStyleAtRulesEnabled = true;
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
        transformStyleOptimized3DEnabled = true;
#endif
    }

    propertySettings.cssWhiteSpaceLonghandsEnabled = true;

    StaticCSSValuePool::init();
}

CSSParserContext::CSSParserContext(const Document& document, const URL& sheetBaseURL, const String& charset)
    : baseURL { sheetBaseURL.isNull() ? document.baseURL() : sheetBaseURL }
    , charset { charset }
    , mode { document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode }
    , isHTMLDocument { document.isHTMLDocument() }
    , hasDocumentSecurityOrigin { sheetBaseURL.isNull() || document.securityOrigin().canRequest(baseURL, OriginAccessPatternsForWebProcess::singleton()) }
    , useSystemAppearance { document.page() ? document.page()->useSystemAppearance() : false }
    , colorContrastEnabled { document.settings().cssColorContrastEnabled() }
    , colorMixEnabled { document.settings().cssColorMixEnabled() }
    , constantPropertiesEnabled { document.settings().constantPropertiesEnabled() }
    , counterStyleAtRuleImageSymbolsEnabled { document.settings().cssCounterStyleAtRuleImageSymbolsEnabled() }
    , cssColor4 { document.settings().cssColor4() }
    , relativeColorSyntaxEnabled { document.settings().cssRelativeColorSyntaxEnabled() }
    , springTimingFunctionEnabled { document.settings().springTimingFunctionEnabled() }
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
    , transformStyleOptimized3DEnabled { document.settings().cssTransformStyleOptimized3DEnabled() }
#endif
    , useLegacyBackgroundSizeShorthandBehavior { document.settings().useLegacyBackgroundSizeShorthandBehavior() }
    , focusVisibleEnabled { document.settings().focusVisibleEnabled() }
    , hasPseudoClassEnabled { document.settings().hasPseudoClassEnabled() }
    , cascadeLayersEnabled { document.settings().cssCascadeLayersEnabled() }
    , overflowClipEnabled { document.settings().overflowClipEnabled() }
    , gradientPremultipliedAlphaInterpolationEnabled { document.settings().cssGradientPremultipliedAlphaInterpolationEnabled() }
    , gradientInterpolationColorSpacesEnabled { document.settings().cssGradientInterpolationColorSpacesEnabled() }
    , subgridEnabled { document.settings().subgridEnabled() }
    , masonryEnabled { document.settings().masonryEnabled() }
    , cssNestingEnabled { document.settings().cssNestingEnabled() }
#if ENABLE(CSS_PAINTING_API)
    , cssPaintingAPIEnabled { document.settings().cssPaintingAPIEnabled() }
#endif
    , cssTextUnderlinePositionLeftRightEnabled { document.settings().cssTextUnderlinePositionLeftRightEnabled() }
    , cssTextWrapNewValuesEnabled { document.settings().cssTextWrapNewValuesEnabled() }
    , cssWordBreakAutoEnabled { document.settings().cssWordBreakAutoEnabled() }
    , popoverAttributeEnabled { document.settings().popoverAttributeEnabled() }
    , sidewaysWritingModesEnabled { document.settings().sidewaysWritingModesEnabled() }
    , propertySettings { CSSPropertySettings { document.settings() } }
{
}

void add(Hasher& hasher, const CSSParserContext& context)
{
    uint64_t bits = context.isHTMLDocument                  << 0
        | context.hasDocumentSecurityOrigin                 << 1
        | context.isContentOpaque                           << 2
        | context.useSystemAppearance                       << 3
        | context.colorContrastEnabled                      << 4
        | context.colorMixEnabled                           << 5
        | context.constantPropertiesEnabled                 << 6
        | context.cssColor4                                 << 7
        | context.relativeColorSyntaxEnabled                << 8
        | context.springTimingFunctionEnabled               << 9
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
        | context.transformStyleOptimized3DEnabled          << 10
#endif
        | context.useLegacyBackgroundSizeShorthandBehavior  << 11
        | context.focusVisibleEnabled                       << 12
        | context.hasPseudoClassEnabled                     << 13
        | context.cascadeLayersEnabled                      << 14
        | context.overflowClipEnabled                       << 15
        | context.gradientPremultipliedAlphaInterpolationEnabled << 16
        | context.gradientInterpolationColorSpacesEnabled   << 17
        | context.subgridEnabled                            << 18
        | context.masonryEnabled                            << 19
        | context.cssNestingEnabled                         << 20
        | context.cssPaintingAPIEnabled                     << 21
        | context.cssTextUnderlinePositionLeftRightEnabled  << 22
        | context.cssTextWrapNewValuesEnabled               << 23
        | context.cssWordBreakAutoEnabled                   << 24
        | context.popoverAttributeEnabled                   << 25
        | context.sidewaysWritingModesEnabled               << 26
        | (uint64_t)context.mode                            << 27; // This is multiple bits, so keep it last.
    add(hasher, context.baseURL, context.charset, context.propertySettings, bits);
}

ResolvedURL CSSParserContext::completeURL(const String& string) const
{
    auto result = [&] () -> ResolvedURL {
        // See also Document::completeURL(const String&)
        if (string.isNull())
            return { };

        if (CSSValue::isCSSLocalURL(string))
            return { string, URL { string } };

        if (charset.isEmpty())
            return { string, { baseURL, string } };
        auto encodingForURLParsing = PAL::TextEncoding { charset }.encodingForFormSubmissionOrURLParsing();
        return { string, { baseURL, string, encodingForURLParsing == PAL::UTF8Encoding() ? nullptr : &encodingForURLParsing } };
    }();

    if (mode == WebVTTMode && !result.resolvedURL.protocolIsData())
        return { };

    return result;
}

}
