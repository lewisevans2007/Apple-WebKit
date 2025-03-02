/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#include "config.h"
#include "WebExtensionContextProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "CocoaHelpers.h"
#include "WKNSData.h"
#include "WebExtensionAPINamespace.h"
#include "WebExtensionAPIPermissions.h"
#include "WebExtensionAPIWebNavigation.h"
#include "_WKWebExtensionLocalization.h"
#include <WebCore/ProcessQualified.h>
#include <wtf/ObjectIdentifier.h>

namespace WebKit {

using namespace WebCore;

RetainPtr<NSDictionary> WebExtensionContextProxy::parseJSON(API::Data& json)
{
    return dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:wrapper(json) options:0 error:nullptr]);
}

RetainPtr<_WKWebExtensionLocalization> WebExtensionContextProxy::parseLocalization(API::Data& json)
{
    NSDictionary *localizedDictionary = parseJSON(json).get();
    if (!localizedDictionary)
        return nil;

    _WKWebExtensionLocalization *localization = [[_WKWebExtensionLocalization alloc] initWithRegionalLocalization:localizedDictionary languageLocalization:nil defaultLocalization:nil withBestLocale:localizedDictionary[@"@@ui_locale"][@"message"] uniqueIdentifier:nil];
    return localization;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
