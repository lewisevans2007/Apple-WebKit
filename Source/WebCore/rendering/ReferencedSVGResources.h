/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/FastMalloc.h>
#include <wtf/IsoMalloc.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class CSSSVGResourceElementClient;
class Document;
class RenderSVGResourceClipper;
class RenderSVGResourceContainer;
class ReferencePathOperation;
class ReferenceFilterOperation;
class RenderElement;
class RenderSVGResourceClipper;
class RenderSVGResourceContainer;
class RenderSVGResourceFilter;
class RenderStyle;
class QualifiedName;
class SVGElement;
class SVGFilterElement;
class TreeScope;

class ReferencedSVGResources {
    WTF_MAKE_ISO_ALLOCATED(ReferencedSVGResources);
public:
    ReferencedSVGResources(RenderElement&);
    ~ReferencedSVGResources();

    static Vector<std::pair<AtomString, QualifiedName>> referencedSVGResourceIDs(const RenderStyle&);
    void updateReferencedResources(TreeScope&, const Vector<std::pair<AtomString, QualifiedName>>&);

    // Clipping needs a renderer, filters use an element.
    static RenderSVGResourceClipper* referencedClipperRenderer(TreeScope&, const ReferencePathOperation&);
    static SVGFilterElement* referencedFilterElement(TreeScope&, const ReferenceFilterOperation&);

    static RenderSVGResourceContainer* referencedRenderResource(TreeScope&, const AtomString& fragment);

private:
    static SVGElement* elementForResourceID(TreeScope&, const AtomString& resourceID, const QualifiedName& tagName);

    void addClientForTarget(SVGElement& targetElement, const AtomString&);
    void removeClientForTarget(TreeScope&, const AtomString&);

    RenderElement& m_renderer;
    MemoryCompactRobinHoodHashMap<AtomString, std::unique_ptr<CSSSVGResourceElementClient>> m_elementClients;
};

} // namespace WebCore
