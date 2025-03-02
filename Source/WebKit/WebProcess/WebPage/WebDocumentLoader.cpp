/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebDocumentLoader.h"

#include "WebFrame.h"
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameLoader.h>

namespace WebKit {
using namespace WebCore;

WebDocumentLoader::WebDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
    : DocumentLoader(request, substituteData)
{
}

void WebDocumentLoader::detachFromFrame()
{
    if (auto navigationID = std::exchange(m_navigationID, 0))
        WebFrame::fromCoreFrame(*frame())->documentLoaderDetached(navigationID);

    DocumentLoader::detachFromFrame();
}

void WebDocumentLoader::setNavigationID(uint64_t navigationID)
{
    ASSERT(navigationID);

    m_navigationID = navigationID;
}

RefPtr<WebDocumentLoader> WebDocumentLoader::loaderForWebsitePolicies(const LocalFrame& frame, CanIncludeCurrentDocumentLoader canIncludeCurrentDocumentLoader)
{
    RefPtr loader = static_cast<WebDocumentLoader*>(frame.loader().policyDocumentLoader());
    if (!loader)
        loader = static_cast<WebDocumentLoader*>(frame.loader().provisionalDocumentLoader());
    if (!loader && canIncludeCurrentDocumentLoader == CanIncludeCurrentDocumentLoader::Yes)
        loader = static_cast<WebDocumentLoader*>(frame.loader().documentLoader());
    return loader;
}

} // namespace WebKit
