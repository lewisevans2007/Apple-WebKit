/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#include "AXCoreObject.h"
#include "AXObjectCache.h"
#include "IntPoint.h"
#include "LayoutRect.h"
#include "Path.h"
#include "RenderStyleConstants.h"
#include <variant>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AXIsolatedTree;

class AXIsolatedObject final : public AXCoreObject {
    friend class AXIsolatedTree;
public:
    static Ref<AXIsolatedObject> create(const Ref<AccessibilityObject>&, AXIsolatedTree*);
    ~AXIsolatedObject();

    ProcessID processID() const override { return tree()->processID(); }

    void attachPlatformWrapper(AccessibilityObjectWrapper*);
    bool isDetached() const override;
    bool isTable() const final { return boolAttributeValue(AXPropertyName::IsTable); }
    bool isExposable() const final { return boolAttributeValue(AXPropertyName::IsExposable); }

    AXIsolatedObject* parentObject() const override { return parentObjectUnignored(); }
    AXIsolatedObject* editableAncestor() override { return Accessibility::editableAncestor(*this); };
    bool canSetFocusAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetFocusAttribute); }

private:
    void detachRemoteParts(AccessibilityDetachmentType) override;
    void detachPlatformWrapper(AccessibilityDetachmentType) override;

    AXID parent() const { return m_parentID; }
    void setParent(AXID axID) { m_parentID = axID; }

    AXIsolatedTree* tree() const { return m_cachedTree.get(); }

    AXIsolatedObject() = default;
    AXIsolatedObject(const Ref<AccessibilityObject>&, AXIsolatedTree*);
    bool isAXIsolatedObjectInstance() const override { return true; }
    AccessibilityObject* associatedAXObject() const;

    void initializeProperties(const Ref<AccessibilityObject>&);
    void initializePlatformProperties(const Ref<const AccessibilityObject>&);

    void setProperty(AXPropertyName, AXPropertyValueVariant&&, bool shouldRemove = false);
    void setObjectProperty(AXPropertyName, AXCoreObject*);
    void setObjectVectorProperty(AXPropertyName, const AccessibilityChildrenVector&);

    // FIXME: consolidate all AttributeValue retrieval in a single template method.
    bool boolAttributeValue(AXPropertyName) const;
    String stringAttributeValue(AXPropertyName) const;
    int intAttributeValue(AXPropertyName) const;
    unsigned unsignedAttributeValue(AXPropertyName) const;
    double doubleAttributeValue(AXPropertyName) const;
    float floatAttributeValue(AXPropertyName) const;
    AXCoreObject* objectAttributeValue(AXPropertyName) const;
    IntPoint intPointAttributeValue(AXPropertyName) const;
    Color colorAttributeValue(AXPropertyName) const;
    URL urlAttributeValue(AXPropertyName) const;
    uint64_t uint64AttributeValue(AXPropertyName) const;
    Path pathAttributeValue(AXPropertyName) const;
    std::pair<unsigned, unsigned> indexRangePairAttributeValue(AXPropertyName) const;
    template<typename T> T rectAttributeValue(AXPropertyName) const;
    template<typename T> Vector<T> vectorAttributeValue(AXPropertyName) const;
    template<typename T> OptionSet<T> optionSetAttributeValue(AXPropertyName) const;
    template<typename T> std::optional<T> optionalAttributeValue(AXPropertyName) const;
    template<typename T> T propertyValue(AXPropertyName) const;

    // The following method performs a lazy caching of the given property.
    // If the property is already in m_propertyMap, returns the existing value.
    // If not, retrieves the property from the main thread and cache it for later use.
    template<typename T> T getOrRetrievePropertyValue(AXPropertyName);

    void fillChildrenVectorForProperty(AXPropertyName, AccessibilityChildrenVector&) const;
    void setMathscripts(AXPropertyName, AXCoreObject&);
    void insertMathPairs(Vector<std::pair<AXID, AXID>>&, AccessibilityMathMultiscriptPairs&);
    template<typename U> void performFunctionOnMainThreadAndWait(U&& lambda) const
    {
        Accessibility::performFunctionOnMainThreadAndWait([&lambda, this] {
            if (RefPtr object = associatedAXObject())
                lambda(object.get());
        });
    }
    template<typename U> void performFunctionOnMainThread(U&& lambda) const
    {
        Accessibility::performFunctionOnMainThread([lambda = WTFMove(lambda), protectedThis = Ref { *this }] () mutable {
            if (RefPtr object = protectedThis->associatedAXObject())
                lambda(object.get());
        });
    }

    // Attribute retrieval overrides.
    bool isLink() const override { return boolAttributeValue(AXPropertyName::IsLink); }
    bool isSecureField() const override { return boolAttributeValue(AXPropertyName::IsSecureField); }
    bool isAttachment() const override { return boolAttributeValue(AXPropertyName::IsAttachment); }
    bool isInputImage() const override { return boolAttributeValue(AXPropertyName::IsInputImage); }
    bool isControl() const override { return boolAttributeValue(AXPropertyName::IsControl); }

    bool isList() const override { return boolAttributeValue(AXPropertyName::IsList); }
    bool isKeyboardFocusable() const override { return boolAttributeValue(AXPropertyName::IsKeyboardFocusable); }
    
    // Table support.
    AXIsolatedObject* exposedTableAncestor(bool includeSelf = false) const final { return Accessibility::exposedTableAncestor(*this, includeSelf); }
    bool supportsSelectedRows() const override { return boolAttributeValue(AXPropertyName::SupportsSelectedRows); }
    AccessibilityChildrenVector columns() final { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::Columns)); }
    AccessibilityChildrenVector rows() final { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::Rows)); }
    unsigned columnCount() final { return static_cast<unsigned>(columns().size()); }
    unsigned rowCount() final { return static_cast<unsigned>(rows().size()); }
    AccessibilityChildrenVector cells() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::Cells)); }
    AXCoreObject* cellForColumnAndRow(unsigned, unsigned) override;
    AccessibilityChildrenVector columnHeaders() override;
    AccessibilityChildrenVector rowHeaders() override;
    AccessibilityChildrenVector visibleRows() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::VisibleRows)); }
    AXCoreObject* headerContainer() override { return objectAttributeValue(AXPropertyName::HeaderContainer); }
    int axColumnCount() const override { return intAttributeValue(AXPropertyName::AXColumnCount); }
    int axRowCount() const override { return intAttributeValue(AXPropertyName::AXRowCount); }

    // Table cell support.
    bool isTableCell() const final;
    bool isExposedTableCell() const final { return boolAttributeValue(AXPropertyName::IsExposedTableCell); }
    // Returns the start location and row span of the cell.
    std::pair<unsigned, unsigned> rowIndexRange() const final { return indexRangePairAttributeValue(AXPropertyName::RowIndexRange); }
    // Returns the start location and column span of the cell.
    std::pair<unsigned, unsigned> columnIndexRange() const final { return indexRangePairAttributeValue(AXPropertyName::ColumnIndexRange); }
    int axColumnIndex() const override { return intAttributeValue(AXPropertyName::AXColumnIndex); }
    int axRowIndex() const override { return intAttributeValue(AXPropertyName::AXRowIndex); }
    bool isColumnHeader() const final { return boolAttributeValue(AXPropertyName::IsColumnHeader); }
    bool isRowHeader() const final { return boolAttributeValue(AXPropertyName::IsRowHeader); }
    String cellScope() const final { return stringAttributeValue(AXPropertyName::CellScope); }
    AXID rowGroupAncestorID() const final { return propertyValue<AXID>(AXPropertyName::RowGroupAncestorID); }

    // Table column support.
    bool isTableColumn() const override { return boolAttributeValue(AXPropertyName::IsTableColumn); }
    unsigned columnIndex() const override { return unsignedAttributeValue(AXPropertyName::ColumnIndex); }
    AXCoreObject* columnHeader() override { return objectAttributeValue(AXPropertyName::ColumnHeader); }

    // Table row support.
    bool isTableRow() const override { return boolAttributeValue(AXPropertyName::IsTableRow); }
    unsigned rowIndex() const override { return unsignedAttributeValue(AXPropertyName::RowIndex); }
    AXCoreObject* rowHeader() final { return objectAttributeValue(AXPropertyName::RowHeader); };

    // ARIA tree/grid row support.
    bool isARIATreeGridRow() const override { return boolAttributeValue(AXPropertyName::IsARIATreeGridRow); }
    AccessibilityChildrenVector disclosedRows() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::DisclosedRows)); }
    AXCoreObject* disclosedByRow() const override { return objectAttributeValue(AXPropertyName::DisclosedByRow); }

    bool isFieldset() const override { return boolAttributeValue(AXPropertyName::IsFieldset); }
    bool isChecked() const override { return boolAttributeValue(AXPropertyName::IsChecked); }
    bool isEnabled() const override { return boolAttributeValue(AXPropertyName::IsEnabled); }
    bool isSelected() const override { return boolAttributeValue(AXPropertyName::IsSelected); }
    bool isFocused() const override { return tree()->focusedNodeID() == objectID(); }
    bool isMultiSelectable() const override { return boolAttributeValue(AXPropertyName::IsMultiSelectable); }
    InsideLink insideLink() const final { return propertyValue<InsideLink>(AXPropertyName::InsideLink); }
    bool isRequired() const override { return boolAttributeValue(AXPropertyName::IsRequired); }
    bool supportsRequiredAttribute() const override { return boolAttributeValue(AXPropertyName::SupportsRequiredAttribute); }
    bool isExpanded() const override { return boolAttributeValue(AXPropertyName::IsExpanded); }
    FloatPoint screenRelativePosition() const final;
    FloatRect relativeFrame() const override;
#if PLATFORM(MAC)
    FloatRect primaryScreenRect() const override;
#endif
    IntSize size() const final { return snappedIntRect(LayoutRect(relativeFrame())).size(); }
    FloatRect relativeFrameFromChildren() const;
    bool supportsDatetimeAttribute() const override { return boolAttributeValue(AXPropertyName::SupportsDatetimeAttribute); }
    String datetimeAttributeValue() const override { return stringAttributeValue(AXPropertyName::DatetimeAttributeValue); }
    bool canSetValueAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetValueAttribute); }
    bool canSetSelectedAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetSelectedAttribute); }
    bool canSetSelectedChildren() const override { return boolAttributeValue(AXPropertyName::CanSetSelectedChildren); }
    // We should never create an isolated object from an ignored live object, so we can hardcode this to false.
    bool accessibilityIsIgnored() const override { return false; }
    unsigned blockquoteLevel() const override { return unsignedAttributeValue(AXPropertyName::BlockquoteLevel); }
    unsigned headingLevel() const override { return unsignedAttributeValue(AXPropertyName::HeadingLevel); }
    AccessibilityButtonState checkboxOrRadioValue() const override { return propertyValue<AccessibilityButtonState>(AXPropertyName::ButtonState); }
    String valueDescription() const override { return stringAttributeValue(AXPropertyName::ValueDescription); }
    float valueForRange() const override { return floatAttributeValue(AXPropertyName::ValueForRange); }
    float maxValueForRange() const override { return floatAttributeValue(AXPropertyName::MaxValueForRange); }
    float minValueForRange() const override { return floatAttributeValue(AXPropertyName::MinValueForRange); }
    int layoutCount() const override;
    double loadingProgress() const override { return tree()->loadingProgress(); }
    bool supportsARIAOwns() const override { return boolAttributeValue(AXPropertyName::SupportsARIAOwns); }
    String popupValue() const override { return stringAttributeValue(AXPropertyName::PopupValue); }
    bool pressedIsPresent() const override;
    String invalidStatus() const override { return stringAttributeValue(AXPropertyName::InvalidStatus); }
    bool supportsExpanded() const override { return boolAttributeValue(AXPropertyName::SupportsExpanded); }
    AccessibilitySortDirection sortDirection() const override { return static_cast<AccessibilitySortDirection>(intAttributeValue(AXPropertyName::SortDirection)); }
    bool supportsRangeValue() const override { return boolAttributeValue(AXPropertyName::SupportsRangeValue); }
    String identifierAttribute() const override;
    String linkRelValue() const override;
    Vector<String> classList() const override;
    AccessibilityCurrentState currentState() const override { return static_cast<AccessibilityCurrentState>(intAttributeValue(AXPropertyName::CurrentState)); }
    bool supportsCurrent() const override { return boolAttributeValue(AXPropertyName::SupportsCurrent); }
    bool supportsKeyShortcuts() const override { return boolAttributeValue(AXPropertyName::SupportsKeyShortcuts); }
    String keyShortcuts() const override { return stringAttributeValue(AXPropertyName::KeyShortcuts); }
    bool supportsSetSize() const override { return boolAttributeValue(AXPropertyName::SupportsSetSize); }
    bool supportsPosInSet() const override { return boolAttributeValue(AXPropertyName::SupportsPosInSet); }
    int setSize() const override { return intAttributeValue(AXPropertyName::SetSize); }
    int posInSet() const override { return intAttributeValue(AXPropertyName::PosInSet); }
    bool supportsDropping() const override { return boolAttributeValue(AXPropertyName::SupportsDropping); }
    bool supportsDragging() const override { return boolAttributeValue(AXPropertyName::SupportsDragging); }
    bool isGrabbed() override { return boolAttributeValue(AXPropertyName::IsGrabbed); }
    Vector<String> determineDropEffects() const override;
    AXCoreObject* accessibilityHitTest(const IntPoint&) const override;
    AXCoreObject* focusedUIElement() const override;
    AXIsolatedObject* parentObjectUnignored() const override;
    AccessibilityChildrenVector linkedObjects() const override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::LinkedObjects)); }
    AXCoreObject* titleUIElement() const override { return objectAttributeValue(AXPropertyName::TitleUIElement); }
    AXCoreObject* scrollBar(AccessibilityOrientation) override;
    const String placeholderValue() const override { return stringAttributeValue(AXPropertyName::PlaceholderValue); }
    String expandedTextValue() const override { return stringAttributeValue(AXPropertyName::ExpandedTextValue); }
    bool supportsExpandedTextValue() const override { return boolAttributeValue(AXPropertyName::SupportsExpandedTextValue); }
    SRGBA<uint8_t> colorValue() const override;
    AccessibilityRole roleValue() const override { return static_cast<AccessibilityRole>(intAttributeValue(AXPropertyName::RoleValue)); }
    String rolePlatformString() const override { return stringAttributeValue(AXPropertyName::RolePlatformString); }
    String roleDescription() const override { return stringAttributeValue(AXPropertyName::RoleDescription); }
    String subrolePlatformString() const override { return stringAttributeValue(AXPropertyName::SubrolePlatformString); }
    LayoutRect elementRect() const override;
    IntPoint clickPoint() final;
    void accessibilityText(Vector<AccessibilityText>& texts) const override;
    String brailleLabel() const override { return stringAttributeValue(AXPropertyName::BrailleLabel); }
    String brailleRoleDescription() const override { return stringAttributeValue(AXPropertyName::BrailleRoleDescription); }
    String embeddedImageDescription() const override { return stringAttributeValue(AXPropertyName::EmbeddedImageDescription); }
    std::optional<AccessibilityChildrenVector> imageOverlayElements() override { return std::nullopt; }
    String extendedDescription() const override { return stringAttributeValue(AXPropertyName::ExtendedDescription); }
    String computedRoleString() const override;
    bool isValueAutofillAvailable() const override { return boolAttributeValue(AXPropertyName::IsValueAutofillAvailable); }
    AutoFillButtonType valueAutofillButtonType() const override { return static_cast<AutoFillButtonType>(intAttributeValue(AXPropertyName::ValueAutofillButtonType)); }
    void ariaTreeRows(AccessibilityChildrenVector& children) override { fillChildrenVectorForProperty(AXPropertyName::ARIATreeRows, children); }
    URL url() const override { return urlAttributeValue(AXPropertyName::URL); }
    String accessKey() const override { return stringAttributeValue(AXPropertyName::AccessKey); }
    String localizedActionVerb() const override { return stringAttributeValue(AXPropertyName::LocalizedActionVerb); }
    String actionVerb() const override { return stringAttributeValue(AXPropertyName::ActionVerb); }
    String autoCompleteValue() const override { return stringAttributeValue(AXPropertyName::AutoCompleteValue); }
    bool isMathElement() const override { return boolAttributeValue(AXPropertyName::IsMathElement); }
    bool isMathFraction() const override { return boolAttributeValue(AXPropertyName::IsMathFraction); }
    bool isMathFenced() const override { return boolAttributeValue(AXPropertyName::IsMathFenced); }
    bool isMathSubscriptSuperscript() const override { return boolAttributeValue(AXPropertyName::IsMathSubscriptSuperscript); }
    bool isMathRow() const override { return boolAttributeValue(AXPropertyName::IsMathRow); }
    bool isMathUnderOver() const override { return boolAttributeValue(AXPropertyName::IsMathUnderOver); }
    bool isMathRoot() const override { return boolAttributeValue(AXPropertyName::IsMathRoot); }
    bool isMathSquareRoot() const override { return boolAttributeValue(AXPropertyName::IsMathSquareRoot); }
    bool isMathTable() const override { return boolAttributeValue(AXPropertyName::IsMathTable); }
    bool isMathTableRow() const override { return boolAttributeValue(AXPropertyName::IsMathTableRow); }
    bool isMathTableCell() const override { return boolAttributeValue(AXPropertyName::IsMathTableCell); }
    bool isMathMultiscript() const override { return boolAttributeValue(AXPropertyName::IsMathMultiscript); }
    bool isMathToken() const override { return boolAttributeValue(AXPropertyName::IsMathToken); }
    std::optional<AccessibilityChildrenVector> mathRadicand() override;
    AXCoreObject* mathRootIndexObject() override { return objectAttributeValue(AXPropertyName::MathRootIndexObject); }
    AXCoreObject* mathUnderObject() override { return objectAttributeValue(AXPropertyName::MathUnderObject); }
    AXCoreObject* mathOverObject() override { return objectAttributeValue(AXPropertyName::MathOverObject); }
    AXCoreObject* mathNumeratorObject() override { return objectAttributeValue(AXPropertyName::MathNumeratorObject); }
    AXCoreObject* mathDenominatorObject() override { return objectAttributeValue(AXPropertyName::MathDenominatorObject); }
    AXCoreObject* mathBaseObject() override { return objectAttributeValue(AXPropertyName::MathBaseObject); }
    AXCoreObject* mathSubscriptObject() override { return objectAttributeValue(AXPropertyName::MathSubscriptObject); }
    AXCoreObject* mathSuperscriptObject() override { return objectAttributeValue(AXPropertyName::MathSuperscriptObject); }
    String mathFencedOpenString() const override { return stringAttributeValue(AXPropertyName::MathFencedOpenString); }
    String mathFencedCloseString() const override { return stringAttributeValue(AXPropertyName::MathFencedCloseString); }
    int mathLineThickness() const override { return intAttributeValue(AXPropertyName::MathLineThickness); }
    void mathPrescripts(AccessibilityMathMultiscriptPairs&) override;
    void mathPostscripts(AccessibilityMathMultiscriptPairs&) override;
#if PLATFORM(COCOA)
    String speechHintAttributeValue() const override { return stringAttributeValue(AXPropertyName::SpeechHint); }
    String descriptionAttributeValue() const override;
    String helpTextAttributeValue() const override;
    String titleAttributeValue() const override;
#endif
#if PLATFORM(MAC)
    bool caretBrowsingEnabled() const override { return boolAttributeValue(AXPropertyName::CaretBrowsingEnabled); }
#endif
    AXIsolatedObject* focusableAncestor() override { return Accessibility::focusableAncestor(*this); }
    AXIsolatedObject* highestEditableAncestor() override { return Accessibility::highestEditableAncestor(*this); }
    AccessibilityOrientation orientation() const override { return static_cast<AccessibilityOrientation>(intAttributeValue(AXPropertyName::Orientation)); }
    unsigned hierarchicalLevel() const override { return unsignedAttributeValue(AXPropertyName::HierarchicalLevel); }
    String language() const override { return stringAttributeValue(AXPropertyName::Language); }
    AccessibilityChildrenVector selectedChildren() final { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::SelectedChildren)); }
    void setSelectedChildren(const AccessibilityChildrenVector&) override;
    AccessibilityChildrenVector visibleChildren() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::VisibleChildren)); }
    AtomString tagName() const override;
    void setChildrenIDs(Vector<AXID>&&);
    const AccessibilityChildrenVector& children(bool updateChildrenIfNeeded = true) override;
    void updateChildrenIfNecessary() override;
    bool isDetachedFromParent() override;
    AXIsolatedObject* liveRegionAncestor(bool excludeIfOff = true) const final { return Accessibility::liveRegionAncestor(*this, excludeIfOff); }
    const String liveRegionStatus() const override { return stringAttributeValue(AXPropertyName::LiveRegionStatus); }
    const String liveRegionRelevant() const override { return stringAttributeValue(AXPropertyName::LiveRegionRelevant); }
    bool liveRegionAtomic() const override { return boolAttributeValue(AXPropertyName::LiveRegionAtomic); }
    bool isBusy() const override { return boolAttributeValue(AXPropertyName::IsBusy); }
    bool isInlineText() const override { return boolAttributeValue(AXPropertyName::IsInlineText); }
    // Spin button support.
    AXCoreObject* incrementButton() override { return objectAttributeValue(AXPropertyName::IncrementButton); }
    AXCoreObject* decrementButton() override { return objectAttributeValue(AXPropertyName::DecrementButton); }
    AccessibilityChildrenVector documentLinks() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::DocumentLinks)); }
    bool supportsCheckedState() const override { return boolAttributeValue(AXPropertyName::SupportsCheckedState); }

    String stringValue() const override;
    std::optional<String> platformStringValue() const;

    // Parameterized attribute retrieval.
    Vector<SimpleRange> findTextRanges(const AccessibilitySearchTextCriteria&) const override;
    Vector<String> performTextOperation(const AccessibilityTextOperation&) override;
    void findMatchingObjects(AccessibilitySearchCriteria*, AccessibilityChildrenVector&) override;

#if PLATFORM(COCOA)
    bool preventKeyboardDOMEventDispatch() const override { return boolAttributeValue(AXPropertyName::PreventKeyboardDOMEventDispatch); }
#endif

    // CharacterRange support.
    CharacterRange selectedTextRange() const override;
    int insertionPointLineNumber() const override;
    CharacterRange doAXRangeForLine(unsigned) const override;
    String doAXStringForRange(const CharacterRange&) const override;
    CharacterRange characterRangeForPoint(const IntPoint&) const override;
    CharacterRange doAXRangeForIndex(unsigned) const override;
    CharacterRange doAXStyleRangeForIndex(unsigned) const override;
    IntRect doAXBoundsForRangeUsingCharacterOffset(const CharacterRange&) const override;
    IntRect doAXBoundsForRange(const CharacterRange&) const override;
    unsigned doAXLineForIndex(unsigned) override;

    VisibleSelection selection() const override;
    VisiblePositionRange selectedVisiblePositionRange() const override;
    void setSelectedVisiblePositionRange(const VisiblePositionRange&) const override;

    std::optional<SimpleRange> simpleRange() const override;
    VisiblePositionRange visiblePositionRange() const override;
    AXTextMarkerRange textMarkerRange() const override;

    // TODO: Text ranges and selection.
    String selectedText() const override;
    VisiblePositionRange visiblePositionRangeForLine(unsigned) const override;
    VisiblePositionRange visiblePositionRangeForUnorderedPositions(const VisiblePosition&, const VisiblePosition&) const override;
    VisiblePositionRange positionOfLeftWord(const VisiblePosition&) const override;
    VisiblePositionRange positionOfRightWord(const VisiblePosition&) const override;
    VisiblePositionRange leftLineVisiblePositionRange(const VisiblePosition&) const override;
    VisiblePositionRange rightLineVisiblePositionRange(const VisiblePosition&) const override;
    VisiblePositionRange sentenceForPosition(const VisiblePosition&) const override;
    VisiblePositionRange paragraphForPosition(const VisiblePosition&) const override;
    VisiblePositionRange styleRangeForPosition(const VisiblePosition&) const override;
    VisiblePositionRange visiblePositionRangeForRange(const CharacterRange&) const override;
    VisiblePositionRange lineRangeForPosition(const VisiblePosition&) const override;
    std::optional<SimpleRange> rangeForCharacterRange(const CharacterRange&) const override;
#if PLATFORM(COCOA)
    AXTextMarkerRange textMarkerRangeForNSRange(const NSRange&) const override;
#endif
    String stringForRange(const SimpleRange&) const override;
    IntRect boundsForRange(const SimpleRange&) const override;
    VisiblePosition visiblePositionForPoint(const IntPoint&) const override;
    VisiblePosition nextLineEndPosition(const VisiblePosition&) const override;
    VisiblePosition previousLineStartPosition(const VisiblePosition&) const override;
    VisiblePosition nextSentenceEndPosition(const VisiblePosition&) const override;
    VisiblePosition previousSentenceStartPosition(const VisiblePosition&) const override;
    VisiblePosition nextParagraphEndPosition(const VisiblePosition&) const override;
    VisiblePosition previousParagraphStartPosition(const VisiblePosition&) const override;
    VisiblePosition visiblePositionForIndex(unsigned, bool lastIndexOK) const override;
    VisiblePosition visiblePositionForIndex(int) const override;
    int indexForVisiblePosition(const VisiblePosition&) const override;
    int lineForPosition(const VisiblePosition&) const override;
    std::optional<SimpleRange> visibleCharacterRange() const override;
    FloatRect unobscuredContentRect() const override;
    
    // Attribute setters.
    void setARIAGrabbed(bool) override;
    void setIsExpanded(bool) override;
    bool setValue(float) override;
    void setValueIgnoringResult(float) final;
    void setSelected(bool) override;
    void setSelectedRows(AccessibilityChildrenVector&&) override;
    void setFocused(bool) override;
    void setSelectedText(const String&) override;
    void setSelectedTextRange(CharacterRange&&) override;
    bool setValue(const String&) override;
    void setValueIgnoringResult(const String&) final;
#if PLATFORM(MAC)
    void setCaretBrowsingEnabled(bool) override;
#endif
#if PLATFORM(COCOA)
    void setPreventKeyboardDOMEventDispatch(bool) override;
#endif

    String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const override;
    std::optional<SimpleRange> misspellingRange(const SimpleRange&, AccessibilitySearchDirection) const override;
    FloatRect convertFrameToSpace(const FloatRect&, AccessibilityConversionSpace) const override;
    void increment() override;
    void decrement() override;
    bool performDismissAction() override;
    void performDismissActionIgnoringResult() final;
    void scrollToMakeVisible() const override;
    void scrollToMakeVisibleWithSubFocus(IntRect&&) const override;
    void scrollToGlobalPoint(IntPoint&&) const override;
    bool replaceTextInRange(const String&, const CharacterRange&) override;
    bool insertText(const String&) override;
    bool press() override;

    bool isAccessibilityObject() const override { return false; }

    // Functions that should never be called on an isolated tree object. ASSERT that these are not reached;
    bool isAccessibilityRenderObject() const override;
    bool isAccessibilityTableInstance() const override;
    bool isAccessibilityARIAGridInstance() const override { return false; }
    bool isAccessibilityARIAGridRowInstance() const override { return false; }
    bool isAccessibilityARIAGridCellInstance() const override { return false; }

    bool isNativeTextControl() const override;
    bool isListBoxOption() const override;
    bool isMockObject() const override;
    bool isNonNativeTextControl() const override;
    bool isIndeterminate() const override { return boolAttributeValue(AXPropertyName::IsIndeterminate); }
    bool isLoaded() const override { return loadingProgress() >= 1; }
    bool isOnScreen() const override;
    bool isOffScreen() const override;
    bool isPressed() const override;
    bool isVisible() const override { return boolAttributeValue(AXPropertyName::IsVisible); }
    bool isSelectedOptionActive() const override;
    bool hasBoldFont() const override { return boolAttributeValue(AXPropertyName::HasBoldFont); }
    bool hasItalicFont() const override { return boolAttributeValue(AXPropertyName::HasItalicFont); }
    bool hasMisspelling() const override;
    bool hasPlainText() const override { return boolAttributeValue(AXPropertyName::HasPlainText); }
    bool hasSameFont(const AXCoreObject&) const override;
    bool hasSameFontColor(const AXCoreObject&) const override;
    bool hasSameStyle(const AXCoreObject&) const override;
    bool hasUnderline() const override { return boolAttributeValue(AXPropertyName::HasUnderline); }
    bool hasHighlighting() const override { return boolAttributeValue(AXPropertyName::HasHighlighting); }
    AXTextMarkerRange textInputMarkedTextMarkerRange() const final;
    Element* element() const override;
    Node* node() const override;
    RenderObject* renderer() const override;

    AccessibilityChildrenVector relatedObjects(AXRelationType) const override;

    bool supportsHasPopup() const override;
    bool supportsPressAction() const final { return boolAttributeValue(AXPropertyName::SupportsPressAction); }
    bool supportsChecked() const override;
    bool isModalNode() const override;
    AXCoreObject* elementAccessibilityHitTest(const IntPoint&) const override;
    bool isDescendantOfRole(AccessibilityRole) const override;
    AXCoreObject* correspondingLabelForControlElement() const override;
    AXCoreObject* correspondingControlForLabelElement() const override;
    bool inheritsPresentationalRole() const override;
    void setAccessibleName(const AtomString&) override;

    String title() const override { return stringAttributeValue(AXPropertyName::Title); }
    String description() const override { return stringAttributeValue(AXPropertyName::Description); }

    std::optional<String> textContent() const override;

    String text() const override;
    unsigned textLength() const override;
#if PLATFORM(COCOA)
    RetainPtr<NSAttributedString> attributedStringForTextMarkerRange(AXTextMarkerRange&&, SpellCheck) const override;
#endif
    AXObjectCache* axObjectCache() const override;
    Element* actionElement() const override;
    Path elementPath() const override { return pathAttributeValue(AXPropertyName::Path); };
    bool supportsPath() const override { return boolAttributeValue(AXPropertyName::SupportsPath); }

    bool isWidget() const override { return boolAttributeValue(AXPropertyName::IsWidget); }
    Widget* widget() const override;
    PlatformWidget platformWidget() const override;
    Widget* widgetForAttachmentView() const override;

    HashMap<String, AXEditingStyleValueVariant> resolvedEditingStyles() const override;
#if PLATFORM(COCOA)
    RemoteAXObjectRef remoteParentObject() const override;
    FloatRect convertRectToPlatformSpace(const FloatRect&, AccessibilityConversionSpace) const override;
#endif
    Page* page() const override;
    Document* document() const override;
    LocalFrameView* documentFrameView() const override;
    ScrollView* scrollView() const override;
    void detachFromParent() override;

    OptionSet<AXAncestorFlag> ancestorFlags() const;

    bool hasDocumentRoleAncestor() const override { return ancestorFlags().contains(AXAncestorFlag::HasDocumentRoleAncestor); }
    bool hasWebApplicationAncestor() const override { return ancestorFlags().contains(AXAncestorFlag::HasWebApplicationAncestor); }
    bool isInDescriptionListDetail() const override { return ancestorFlags().contains(AXAncestorFlag::IsInDescriptionListDetail); }
    bool isInDescriptionListTerm() const override { return ancestorFlags().contains(AXAncestorFlag::IsInDescriptionListTerm); }
    bool isInCell() const override { return ancestorFlags().contains(AXAncestorFlag::IsInCell); }

    std::optional<String> attributeValue(const String&) const override;
#if PLATFORM(COCOA)
    bool hasApplePDFAnnotationAttribute() const override { return boolAttributeValue(AXPropertyName::HasApplePDFAnnotationAttribute); }
#endif

#if PLATFORM(COCOA) && ENABLE(MODEL_ELEMENT)
    Vector<RetainPtr<id>> modelElementChildren() override;
#endif
    
    void updateBackingStore() override;

    String innerHTML() const override;
    String outerHTML() const override;

    // FIXME: Make this a ThreadSafeWeakPtr<AXIsolatedTree>.
    RefPtr<AXIsolatedTree> m_cachedTree;
    AXID m_parentID;
    bool m_childrenDirty { true };
    Vector<AXID> m_childrenIDs;
    Vector<RefPtr<AXCoreObject>> m_children;
    AXPropertyMap m_propertyMap;
    // Some objects (e.g. display:contents) form their geometry through their children.
    bool m_getsGeometryFromChildren { false };

#if PLATFORM(COCOA)
    RetainPtr<NSView> m_platformWidget;
    RetainPtr<RemoteAXObjectRef> m_remoteParent;
#else
    PlatformWidget m_platformWidget;
#endif
};

template<typename T>
inline T AXIsolatedObject::propertyValue(AXPropertyName propertyName) const
{
    auto it = m_propertyMap.find(propertyName);
    if (it == m_propertyMap.end())
        return { };

    auto value = it->value;
    return WTF::switchOn(value,
        [] (T& typedValue) { return typedValue; },
        [] (auto&) { ASSERT_NOT_REACHED();
            return T(); }
    );
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AXIsolatedObject, isAXIsolatedObjectInstance())

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE))
