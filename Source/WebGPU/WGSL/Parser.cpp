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

#include "config.h"
#include "Parser.h"

#include "AST.h"
#include "Lexer.h"
#include "ParserPrivate.h"

#include <wtf/text/StringBuilder.h>

namespace WGSL {

#define START_PARSE() \
    auto _startOfElementPosition = m_lexer.currentPosition();

#define CURRENT_SOURCE_SPAN() \
    SourceSpan(_startOfElementPosition, m_lexer.currentPosition())

#define RETURN_NODE(type, ...) \
    do { \
        AST::type astNodeResult(CURRENT_SOURCE_SPAN(), __VA_ARGS__); \
        return { WTFMove(astNodeResult) }; \
    } while (false)

// Passing 0 arguments beyond the type to RETURN_NODE is invalid because of a stupid limitation of the C preprocessor
#define RETURN_NODE_NO_ARGS(type) \
    do { \
        AST::type astNodeResult(CURRENT_SOURCE_SPAN()); \
        return { WTFMove(astNodeResult) }; \
    } while (false)

#define RETURN_NODE_REF(type, ...) \
    return { adoptRef(*new AST::type(CURRENT_SOURCE_SPAN(), __VA_ARGS__)) };

#define RETURN_NODE_UNIQUE_REF(type, ...) \
    return { makeUniqueRef<AST::type>(CURRENT_SOURCE_SPAN(), __VA_ARGS__) };

// Passing 0 arguments beyond the type to RETURN_NODE_UNIQUE_REF is invalid because of a stupid limitation of the C preprocessor
#define RETURN_NODE_UNIQUE_REF_NO_ARGS(type) \
    return { makeUniqueRef<AST::type>(CURRENT_SOURCE_SPAN()) };

#define FAIL(string) \
    return makeUnexpected(Error(string, CURRENT_SOURCE_SPAN()));

// Warning: cannot use the do..while trick because it defines a new identifier named `name`.
// So do not use after an if/for/while without braces.
#define PARSE(name, element, ...) \
    auto name##Expected = parse##element(__VA_ARGS__); \
    if (!name##Expected) \
        return makeUnexpected(name##Expected.error()); \
    auto& name = *name##Expected;

// Warning: cannot use the do..while trick because it defines a new identifier named `name`.
// So do not use after an if/for/while without braces.
#define CONSUME_TYPE_NAMED(name, type) \
    auto name##Expected = consumeType(TokenType::type); \
    if (!name##Expected) { \
        StringBuilder builder; \
        builder.append("Expected a "); \
        builder.append(toString(TokenType::type)); \
        builder.append(", but got a "); \
        builder.append(toString(name##Expected.error())); \
        FAIL(builder.toString()); \
    } \
    auto& name = *name##Expected;

#define CONSUME_TYPE(type) \
    do { \
        auto expectedToken = consumeType(TokenType::type); \
        if (!expectedToken) { \
            StringBuilder builder; \
            builder.append("Expected a "); \
            builder.append(toString(TokenType::type)); \
            builder.append(", but got a "); \
            builder.append(toString(expectedToken.error())); \
            FAIL(builder.toString()); \
        } \
    } while (false)

template<typename Lexer>
Expected<AST::ShaderModule, Error> parse(const String& wgsl, const Configuration& configuration)
{
    Lexer lexer(wgsl);
    Parser parser(lexer);

    // FIXME: Add a top-level class so that we don't have to plumb all this
    // information until we parse the root node
    return parser.parseShader(wgsl, configuration);
}

Expected<AST::ShaderModule, Error> parseLChar(const String& wgsl, const Configuration& configuration)
{
    return parse<Lexer<LChar>>(wgsl, configuration);
}

Expected<AST::ShaderModule, Error> parseUChar(const String& wgsl, const Configuration& configuration)
{
    return parse<Lexer<UChar>>(wgsl, configuration);
}

template<typename Lexer>
Expected<Token, TokenType> Parser<Lexer>::consumeType(TokenType type)
{
    if (current().m_type == type) {
        Expected<Token, TokenType> result = { m_current };
        m_current = m_lexer.lex();
        return result;
    }
    return makeUnexpected(current().m_type);
}

template<typename Lexer>
void Parser<Lexer>::consume()
{
    m_current = m_lexer.lex();
}

template<typename Lexer>
Expected<AST::ShaderModule, Error> Parser<Lexer>::parseShader(const String& source, const Configuration& configuration)
{
    START_PARSE();

    AST::GlobalDirective::List directives;
    // FIXME: parse directives here.

    AST::Decl::List decls;
    while (!m_lexer.isAtEndOfFile()) {
        PARSE(globalDecl, GlobalDecl)
        decls.append(WTFMove(globalDecl));
    }

    RETURN_NODE(ShaderModule, source, configuration, WTFMove(directives), WTFMove(decls));
}

template<typename Lexer>
Expected<UniqueRef<AST::Decl>, Error> Parser<Lexer>::parseGlobalDecl()
{
    START_PARSE();

    PARSE(attributes, Attributes);

    switch (current().m_type) {
    case TokenType::KeywordStruct: {
        PARSE(structDecl, StructDecl, WTFMove(attributes));
        return { makeUniqueRef<AST::StructDecl>(WTFMove(structDecl)) };
    }
    case TokenType::KeywordVar: {
        PARSE(varDecl, VariableDeclWithAttributes, WTFMove(attributes));
        CONSUME_TYPE(Semicolon);
        return { makeUniqueRef<AST::VariableDecl>(WTFMove(varDecl)) };
    }
    case TokenType::KeywordFn: {
        PARSE(fn, FunctionDecl, WTFMove(attributes));
        return { makeUniqueRef<AST::FunctionDecl>(WTFMove(fn)) };
    }
    default:
        FAIL("Trying to parse a GlobalDecl, expected 'var', 'fn', or 'struct'."_s);
    }
}

template<typename Lexer>
Expected<AST::Attribute::List, Error> Parser<Lexer>::parseAttributes()
{
    AST::Attribute::List attributes;

    while (current().m_type == TokenType::Attribute) {
        PARSE(firstAttribute, Attribute);
        attributes.append(WTFMove(firstAttribute));
    }

    return { WTFMove(attributes) };
}

template<typename Lexer>
Expected<Ref<AST::Attribute>, Error> Parser<Lexer>::parseAttribute()
{
    START_PARSE();

    CONSUME_TYPE(Attribute);
    CONSUME_TYPE_NAMED(ident, Identifier);

    if (ident.m_ident == "group"_s) {
        CONSUME_TYPE(ParenLeft);
        // FIXME: should more kinds of literals be accepted here?
        CONSUME_TYPE_NAMED(id, IntegerLiteral);
        CONSUME_TYPE(ParenRight);
        RETURN_NODE_REF(GroupAttribute, id.m_literalValue);
    }

    if (ident.m_ident == "binding"_s) {
        CONSUME_TYPE(ParenLeft);
        // FIXME: should more kinds of literals be accepted here?
        CONSUME_TYPE_NAMED(id, IntegerLiteral);
        CONSUME_TYPE(ParenRight);
        RETURN_NODE_REF(BindingAttribute, id.m_literalValue);
    }

    if (ident.m_ident == "location"_s) {
        CONSUME_TYPE(ParenLeft);
        // FIXME: should more kinds of literals be accepted here?
        CONSUME_TYPE_NAMED(id, IntegerLiteral);
        CONSUME_TYPE(ParenRight);
        RETURN_NODE_REF(LocationAttribute, id.m_literalValue);
    }

    if (ident.m_ident == "builtin"_s) {
        CONSUME_TYPE(ParenLeft);
        CONSUME_TYPE_NAMED(name, Identifier);
        CONSUME_TYPE(ParenRight);
        RETURN_NODE_REF(BuiltinAttribute, name.m_ident);
    }

    // https://gpuweb.github.io/gpuweb/wgsl/#pipeline-stage-attributes
    if (ident.m_ident == "vertex"_s)
        RETURN_NODE_REF(StageAttribute, AST::StageAttribute::Stage::Vertex);
    if (ident.m_ident == "compute"_s)
        RETURN_NODE_REF(StageAttribute, AST::StageAttribute::Stage::Compute);
    if (ident.m_ident == "fragment"_s)
        RETURN_NODE_REF(StageAttribute, AST::StageAttribute::Stage::Fragment);

    FAIL("Unknown attribute. Supported attributes are 'group', 'binding', 'location', 'builtin', 'vertex', 'compute', 'fragment'."_s);
}

template<typename Lexer>
Expected<AST::StructDecl, Error> Parser<Lexer>::parseStructDecl(AST::Attribute::List&& attributes)
{
    START_PARSE();

    CONSUME_TYPE(KeywordStruct);
    CONSUME_TYPE_NAMED(name, Identifier);
    CONSUME_TYPE(BraceLeft);

    AST::StructMember::List members;
    while (current().m_type != TokenType::BraceRight) {
        PARSE(member, StructMember);
        members.append(makeUniqueRef<AST::StructMember>(WTFMove(member)));
        if (current().m_type == TokenType::Comma)
            consume();
        else
            break;
    }

    CONSUME_TYPE(BraceRight);

    RETURN_NODE(StructDecl, name.m_ident, WTFMove(members), WTFMove(attributes), AST::StructRole::UserDefined);
}

template<typename Lexer>
Expected<AST::StructMember, Error> Parser<Lexer>::parseStructMember()
{
    START_PARSE();

    PARSE(attributes, Attributes);
    CONSUME_TYPE_NAMED(name, Identifier);
    CONSUME_TYPE(Colon);
    PARSE(type, TypeDecl);

    RETURN_NODE(StructMember, name.m_ident, WTFMove(type), WTFMove(attributes));
}

template<typename Lexer>
Expected<Ref<AST::TypeDecl>, Error> Parser<Lexer>::parseTypeDecl()
{
    START_PARSE();

    if (current().m_type == TokenType::KeywordArray)
        return parseArrayType();
    if (current().m_type == TokenType::KeywordI32) {
        consume();
        RETURN_NODE_REF(NamedType, "i32"_s);
    }
    if (current().m_type == TokenType::KeywordF32) {
        consume();
        RETURN_NODE_REF(NamedType, "f32"_s);
    }
    if (current().m_type == TokenType::KeywordU32) {
        consume();
        RETURN_NODE_REF(NamedType, "u32"_s);
    }
    if (current().m_type == TokenType::KeywordBool) {
        consume();
        RETURN_NODE_REF(NamedType, "bool"_s);
    }
    if (current().m_type == TokenType::Identifier) {
        CONSUME_TYPE_NAMED(name, Identifier);
        return parseTypeDeclAfterIdentifier(WTFMove(name.m_ident), _startOfElementPosition);
    }

    FAIL("Tried parsing a type and it did not start with an identifier"_s);
}

template<typename Lexer>
Expected<Ref<AST::TypeDecl>, Error> Parser<Lexer>::parseTypeDeclAfterIdentifier(String&& name, SourcePosition _startOfElementPosition)
{
    if (auto kind = AST::ParameterizedType::stringToKind(name)) {
        CONSUME_TYPE(LT);
        PARSE(elementType, TypeDecl);
        CONSUME_TYPE(GT);
        RETURN_NODE_REF(ParameterizedType, *kind, WTFMove(elementType));
    }
    RETURN_NODE_REF(NamedType, WTFMove(name));
}

template<typename Lexer>
Expected<Ref<AST::TypeDecl>, Error> Parser<Lexer>::parseArrayType()
{
    START_PARSE();

    CONSUME_TYPE(KeywordArray);

    RefPtr<AST::TypeDecl> maybeElementType;
    std::unique_ptr<AST::Expression> maybeElementCount;

    if (current().m_type == TokenType::LT) {
        // We differ from the WGSL grammar here by allowing the type to be optional,
        // which allows us to use `parseArrayType` in `parseCallableExpression`.
        consume();

        PARSE(elementType, TypeDecl);
        maybeElementType = WTFMove(elementType);

        if (current().m_type == TokenType::Comma) {
            consume();
            // FIXME: According to https://www.w3.org/TR/WGSL/#syntax-element_count_expression
            // this should be: AdditiveExpression | BitwiseExpression.
            //
            // The WGSL grammar doesn't specify expression operator precedence so
            // until then just parse AdditiveExpression.
            PARSE(elementCountLHS, UnaryExpression);
            PARSE(elementCount, AdditiveExpression, WTFMove(elementCountLHS));
            maybeElementCount = elementCount.moveToUniquePtr();
        }
        CONSUME_TYPE(GT);
    }

    RETURN_NODE_REF(ArrayType, WTFMove(maybeElementType), WTFMove(maybeElementCount));
}

template<typename Lexer>
Expected<AST::VariableDecl, Error> Parser<Lexer>::parseVariableDecl()
{
    return parseVariableDeclWithAttributes(AST::Attribute::List { });
}

template<typename Lexer>
Expected<AST::VariableDecl, Error> Parser<Lexer>::parseVariableDeclWithAttributes(AST::Attribute::List&& attributes)
{
    START_PARSE();

    CONSUME_TYPE(KeywordVar);

    std::unique_ptr<AST::VariableQualifier> maybeQualifier = nullptr;
    if (current().m_type == TokenType::LT) {
        PARSE(variableQualifier, VariableQualifier);
        maybeQualifier = WTF::makeUnique<AST::VariableQualifier>(WTFMove(variableQualifier));
    }

    CONSUME_TYPE_NAMED(name, Identifier);

    RefPtr<AST::TypeDecl> maybeType = nullptr;
    if (current().m_type == TokenType::Colon) {
        consume();
        PARSE(typeDecl, TypeDecl);
        maybeType = WTFMove(typeDecl);
    }

    std::unique_ptr<AST::Expression> maybeInitializer = nullptr;
    if (current().m_type == TokenType::Equal) {
        consume();
        PARSE(initializerExpr, Expression);
        maybeInitializer = initializerExpr.moveToUniquePtr();
    }

    RETURN_NODE(VariableDecl, name.m_ident, WTFMove(maybeQualifier), WTFMove(maybeType), WTFMove(maybeInitializer), WTFMove(attributes));
}

template<typename Lexer>
Expected<AST::VariableQualifier, Error> Parser<Lexer>::parseVariableQualifier()
{
    START_PARSE();

    CONSUME_TYPE(LT);
    PARSE(storageClass, StorageClass);

    // FIXME: verify that Read is the correct default in all cases.
    AST::AccessMode accessMode = AST::AccessMode::Read;
    if (current().m_type == TokenType::Comma) {
        consume();
        PARSE(actualAccessMode, AccessMode);
        accessMode = actualAccessMode;
    }

    CONSUME_TYPE(GT);

    RETURN_NODE(VariableQualifier, storageClass, accessMode);
}

template<typename Lexer>
Expected<AST::StorageClass, Error> Parser<Lexer>::parseStorageClass()
{
    START_PARSE();

    if (current().m_type == TokenType::KeywordFunction) {
        consume();
        return { AST::StorageClass::Function };
    }
    if (current().m_type == TokenType::KeywordPrivate) {
        consume();
        return { AST::StorageClass::Private };
    }
    if (current().m_type == TokenType::KeywordWorkgroup) {
        consume();
        return { AST::StorageClass::Workgroup };
    }
    if (current().m_type == TokenType::KeywordUniform) {
        consume();
        return { AST::StorageClass::Uniform };
    }
    if (current().m_type == TokenType::KeywordStorage) {
        consume();
        return { AST::StorageClass::Storage };
    }

    FAIL("Expected one of 'function'/'private'/'storage'/'uniform'/'workgroup'"_s);
}

template<typename Lexer>
Expected<AST::AccessMode, Error> Parser<Lexer>::parseAccessMode()
{
    START_PARSE();

    if (current().m_type == TokenType::KeywordRead) {
        consume();
        return { AST::AccessMode::Read };
    }
    if (current().m_type == TokenType::KeywordWrite) {
        consume();
        return { AST::AccessMode::Write };
    }
    if (current().m_type == TokenType::KeywordReadWrite) {
        consume();
        return { AST::AccessMode::ReadWrite };
    }

    FAIL("Expected one of 'read'/'write'/'read_write'"_s);
}

template<typename Lexer>
Expected<AST::FunctionDecl, Error> Parser<Lexer>::parseFunctionDecl(AST::Attribute::List&& attributes)
{
    START_PARSE();

    CONSUME_TYPE(KeywordFn);
    CONSUME_TYPE_NAMED(name, Identifier);

    CONSUME_TYPE(ParenLeft);
    AST::Parameter::List parameters;
    while (current().m_type != TokenType::ParenRight) {
        PARSE(parameter, Parameter);
        parameters.append(makeUniqueRef<AST::Parameter>(WTFMove(parameter)));
        if (current().m_type == TokenType::Comma)
            consume();
        else
            break;
    }
    CONSUME_TYPE(ParenRight);

    AST::Attribute::List returnAttributes;
    RefPtr<AST::TypeDecl> maybeReturnType = nullptr;
    if (current().m_type == TokenType::Arrow) {
        consume();
        PARSE(parsedReturnAttributes, Attributes);
        returnAttributes = WTFMove(parsedReturnAttributes);
        PARSE(type, TypeDecl);
        maybeReturnType = WTFMove(type);
    }

    PARSE(body, CompoundStatement);

    RETURN_NODE(FunctionDecl, name.m_ident, WTFMove(parameters), WTFMove(maybeReturnType), WTFMove(body), WTFMove(attributes), WTFMove(returnAttributes));
}

template<typename Lexer>
Expected<AST::Parameter, Error> Parser<Lexer>::parseParameter()
{
    START_PARSE();

    PARSE(attributes, Attributes);
    CONSUME_TYPE_NAMED(name, Identifier)
    CONSUME_TYPE(Colon);
    PARSE(type, TypeDecl);

    RETURN_NODE(Parameter, name.m_ident, WTFMove(type), WTFMove(attributes), AST::ParameterRole::UserDefined);
}

template<typename Lexer>
Expected<UniqueRef<AST::Statement>, Error> Parser<Lexer>::parseStatement()
{
    START_PARSE();

    switch (current().m_type) {
    case TokenType::BraceLeft: {
        PARSE(compoundStmt, CompoundStatement);
        return { makeUniqueRef<AST::CompoundStatement>(WTFMove(compoundStmt)) };
    }
    case TokenType::Semicolon: {
        consume();
        AST::Statement::List statements;
        return { makeUniqueRef<AST::CompoundStatement>(CURRENT_SOURCE_SPAN(), WTFMove(statements)) };
    }
    case TokenType::KeywordReturn: {
        PARSE(returnStmt, ReturnStatement);
        CONSUME_TYPE(Semicolon);
        return { makeUniqueRef<AST::ReturnStatement>(WTFMove(returnStmt)) };
    }
    case TokenType::KeywordVar: {
        PARSE(varDecl, VariableDecl);
        CONSUME_TYPE(Semicolon);
        return { makeUniqueRef<AST::VariableStatement>(CURRENT_SOURCE_SPAN(), WTFMove(varDecl)) };
    }
    case TokenType::Identifier: {
        // FIXME: there will be other cases here eventually for function calls
        PARSE(lhs, LHSExpression);
        CONSUME_TYPE(Equal);
        PARSE(rhs, Expression);
        CONSUME_TYPE(Semicolon);
        RETURN_NODE_UNIQUE_REF(AssignmentStatement, lhs.moveToUniquePtr(), WTFMove(rhs));
    }
    default:
        FAIL("Not a valid statement"_s);
    }
}

template<typename Lexer>
Expected<AST::CompoundStatement, Error> Parser<Lexer>::parseCompoundStatement()
{
    START_PARSE();

    CONSUME_TYPE(BraceLeft);

    AST::Statement::List statements;
    while (current().m_type != TokenType::BraceRight) {
        PARSE(stmt, Statement);
        statements.append(WTFMove(stmt));
    }

    CONSUME_TYPE(BraceRight);

    RETURN_NODE(CompoundStatement, WTFMove(statements));
}

template<typename Lexer>
Expected<AST::ReturnStatement, Error> Parser<Lexer>::parseReturnStatement()
{
    START_PARSE();

    CONSUME_TYPE(KeywordReturn);

    if (current().m_type == TokenType::Semicolon) {
        RETURN_NODE(ReturnStatement, { });
    }

    PARSE(expr, Expression);
    RETURN_NODE(ReturnStatement, expr.moveToUniquePtr());
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseRelationalExpression(UniqueRef<AST::Expression>&& lhs)
{
    // FIXME: fill in
    return parseShiftExpression(WTFMove(lhs));
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseShiftExpression(UniqueRef<AST::Expression>&& lhs)
{
    // FIXME: fill in
    return parseAdditiveExpression(WTFMove(lhs));
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseAdditiveExpression(UniqueRef<AST::Expression>&& lhs)
{
    // FIXME: fill in
    START_PARSE();
    if (current().m_type == TokenType::Plus) {
        consume();
        PARSE(rhs, UnaryExpression);
        RETURN_NODE_UNIQUE_REF(BinaryExpression, WTFMove(lhs), WTFMove(rhs), AST::BinaryOperation::Add);
    }
    return parseMultiplicativeExpression(WTFMove(lhs));
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseMultiplicativeExpression(UniqueRef<AST::Expression>&& lhs)
{
    // FIXME: fill in
    START_PARSE();
    if (current().m_type == TokenType::Star) {
        consume();
        PARSE(rhs, UnaryExpression);
        RETURN_NODE_UNIQUE_REF(BinaryExpression, WTFMove(lhs), WTFMove(rhs), AST::BinaryOperation::Multiply);
    }
    return WTFMove(lhs);
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseUnaryExpression()
{
    START_PARSE();

    if (current().m_type == TokenType::Minus) {
        consume();
        PARSE(expression, SingularExpression);
        RETURN_NODE_UNIQUE_REF(UnaryExpression, WTFMove(expression), AST::UnaryOperation::Negate);
    }

    return parseSingularExpression();
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseSingularExpression()
{
    START_PARSE();
    PARSE(base, PrimaryExpression);
    return parsePostfixExpression(WTFMove(base), _startOfElementPosition);
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parsePostfixExpression(UniqueRef<AST::Expression>&& base, SourcePosition startPosition)
{
    START_PARSE();

    UniqueRef<AST::Expression> expr = WTFMove(base);
    // FIXME: add the case for array/vector/matrix access

    for (;;) {
        switch (current().m_type) {
        case TokenType::BracketLeft: {
            consume();
            PARSE(arrayIndex, Expression);
            CONSUME_TYPE(BracketRight);
            // FIXME: Replace with NODE_REF(...)
            SourceSpan span(startPosition, m_lexer.currentPosition());
            expr = makeUniqueRef<AST::ArrayAccess>(span, WTFMove(expr), WTFMove(arrayIndex));
            break;
        }

        case TokenType::Period: {
            consume();
            CONSUME_TYPE_NAMED(fieldName, Identifier);
            // FIXME: Replace with NODE_REF(...)
            SourceSpan span(startPosition, m_lexer.currentPosition());
            expr = makeUniqueRef<AST::StructureAccess>(span, WTFMove(expr), fieldName.m_ident);
            break;
        }

        default:
            return { WTFMove(expr) };
        }
    }
}

// https://gpuweb.github.io/gpuweb/wgsl/#syntax-primary_expression
// primary_expression:
//   | ident
//   | callable argument_expression_list
//   | const_literal
//   | paren_expression
//   | bitcast less_than type_decl greater_than paren_expression
template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parsePrimaryExpression()
{
    START_PARSE();

    switch (current().m_type) {
    // paren_expression
    case TokenType::ParenLeft: {
        consume();
        PARSE(expr, Expression);
        CONSUME_TYPE(ParenRight);
        return { WTFMove(expr) };
    }
    case TokenType::Identifier: {
        CONSUME_TYPE_NAMED(ident, Identifier);
        if (current().m_type == TokenType::LT || current().m_type == TokenType::ParenLeft) {
            PARSE(type, TypeDeclAfterIdentifier, WTFMove(ident.m_ident), _startOfElementPosition);
            PARSE(arguments, ArgumentExpressionList);
            RETURN_NODE_UNIQUE_REF(CallableExpression, WTFMove(type), WTFMove(arguments));
        }
        RETURN_NODE_UNIQUE_REF(IdentifierExpression, ident.m_ident);
    }
    case TokenType::KeywordArray: {
        PARSE(arrayType, ArrayType);
        PARSE(arguments, ArgumentExpressionList);
        RETURN_NODE_UNIQUE_REF(CallableExpression, WTFMove(arrayType), WTFMove(arguments));
    }

    // const_literal
    case TokenType::LiteralTrue:
        consume();
        RETURN_NODE_UNIQUE_REF(BoolLiteral, true);
    case TokenType::LiteralFalse:
        consume();
        RETURN_NODE_UNIQUE_REF(BoolLiteral, false);
    case TokenType::IntegerLiteral: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteral);
        RETURN_NODE_UNIQUE_REF(AbstractIntLiteral, lit.m_literalValue);
    }
    case TokenType::IntegerLiteralSigned: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteralSigned);
        RETURN_NODE_UNIQUE_REF(Int32Literal, lit.m_literalValue);
    }
    case TokenType::IntegerLiteralUnsigned: {
        CONSUME_TYPE_NAMED(lit, IntegerLiteralUnsigned);
        RETURN_NODE_UNIQUE_REF(Uint32Literal, lit.m_literalValue);
    }
    case TokenType::DecimalFloatLiteral: {
        CONSUME_TYPE_NAMED(lit, DecimalFloatLiteral);
        RETURN_NODE_UNIQUE_REF(AbstractFloatLiteral, lit.m_literalValue);
    }
    case TokenType::HexFloatLiteral: {
        CONSUME_TYPE_NAMED(lit, HexFloatLiteral);
        RETURN_NODE_UNIQUE_REF(AbstractFloatLiteral, lit.m_literalValue);
    }
    // TODO: bitcast expression

    default:
        break;
    }

    FAIL("Expected one of '(', a literal, or an identifier"_s);
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseExpression()
{
    // FIXME: Fill in
    PARSE(lhs, UnaryExpression);
    return parseRelationalExpression(WTFMove(lhs));
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseLHSExpression()
{
    START_PARSE();

    // FIXME: Add the possibility of a prefix
    PARSE(base, CoreLHSExpression);
    return parsePostfixExpression(WTFMove(base), _startOfElementPosition);
}

template<typename Lexer>
Expected<UniqueRef<AST::Expression>, Error> Parser<Lexer>::parseCoreLHSExpression()
{
    START_PARSE();

    switch (current().m_type) {
    case TokenType::ParenLeft: {
        consume();
        PARSE(expr, LHSExpression);
        CONSUME_TYPE(ParenRight);
        return { WTFMove(expr) };
    }
    case TokenType::Identifier: {
        CONSUME_TYPE_NAMED(ident, Identifier);
        RETURN_NODE_UNIQUE_REF(IdentifierExpression, ident.m_ident);
    }
    default:
        break;
    }

    FAIL("Tried to parse the left-hand side of an assignment and failed"_s);
}

template<typename Lexer>
Expected<AST::Expression::List, Error> Parser<Lexer>::parseArgumentExpressionList()
{
    START_PARSE();
    CONSUME_TYPE(ParenLeft);

    AST::Expression::List arguments;
    while (current().m_type != TokenType::ParenRight) {
        PARSE(expr, Expression);
        arguments.append(WTFMove(expr));
        if (current().m_type != TokenType::ParenRight) {
            CONSUME_TYPE(Comma);
        }
    }

    CONSUME_TYPE(ParenRight);
    return { WTFMove(arguments) };
}

} // namespace WGSL