#include <gtest/gtest.h>

#include <string>

#include "util/Json.h"

using tft::json::Object;
using tft::json::ParseFlatObject;
using tft::json::ValueType;

TEST(Json, ParsesFlatObjectWithMixedValueTypes) {
    Object obj;
    std::string error;
    ASSERT_TRUE(ParseFlatObject(
        "{\"s\":\"texto\",\"b\":true,\"f\":false,\"n\":123,\"arr\":[1,2],"
        "\"obj\":{\"k\":\"v\"},\"nil\":null,\"neg\":-1.5e+2}", obj, &error)) << error;

    ASSERT_EQ(obj.size(), 9u);
    EXPECT_EQ(obj.at("s").type, ValueType::String);
    EXPECT_EQ(obj.at("s").text, "texto");
    EXPECT_EQ(obj.at("b").type, ValueType::Bool);
    EXPECT_TRUE(obj.at("b").bool_value);
    EXPECT_EQ(obj.at("f").type, ValueType::Bool);
    EXPECT_FALSE(obj.at("f").bool_value);
    EXPECT_EQ(obj.at("n").type, ValueType::Other);
    EXPECT_EQ(obj.at("arr").type, ValueType::Other);
    EXPECT_EQ(obj.at("obj").type, ValueType::Other);
    EXPECT_EQ(obj.at("nil").type, ValueType::Other);
    EXPECT_EQ(obj.at("neg").type, ValueType::Other);
}

TEST(Json, DecodesEscapesAndSurrogatePairs) {
    Object obj;
    std::string error;
    ASSERT_TRUE(ParseFlatObject(
        "{\"s\":\"a\\\"b\\\\c\\/d\\ne\\tf\",\"emoji\":\"\\uD83D\\uDE00\"}", obj, &error)) << error;

    EXPECT_EQ(obj.at("s").text, std::string("a\"b\\c/d\ne\tf"));
    EXPECT_EQ(obj.at("emoji").text, std::string("\xF0\x9F\x98\x80"));
}

TEST(Json, RejectsMalformedDocuments) {
    const char* invalid[] = {
        "[]",                            // objeto esperado
        "{\"a\":1,}",                    // vírgula final
        "{\"a\":1} extra",               // texto após o objeto
        "{\"a\":1,\"a\":2}",             // chave duplicada
        "{\"a\":\"linha\nquebrada\"}",   // controle literal dentro da string
        "{\"a\":01}",                    // zero à esquerda
        "{\"a\":truex}"                  // literal seguido de texto
    };
    for (const char* line : invalid) {
        Object obj;
        std::string error;
        EXPECT_FALSE(ParseFlatObject(line, obj, &error)) << line;
        EXPECT_NE(error.find("JSON inválido"), std::string::npos) << line << " => " << error;
    }
}

TEST(Json, RejectsMalformedEscapes) {
    const char* invalid[] = {
        "{\"a\":\"\\q\"}",
        "{\"a\":\"\\u12\"}",
        "{\"a\":\"\\uZZZZ\"}"
    };
    for (const char* line : invalid) {
        Object obj;
        EXPECT_FALSE(ParseFlatObject(line, obj, nullptr)) << line;
    }
}
