#pragma once

#include <string>
#include <vector>
#include <memory>
#include <string_view>

#include "basedef.h"

enum class TokenType
{
	String,
	Number,
	BooleanTrue,
	BooleanFalse,
	NullValue,
	OpenCurlyBrace,
	CloseCurlyBrace,
	OpenSquareBracket,
	CloseSquareBracket,
	Comma,
	Colon,
	EndOfFile,
	None
};

struct Token
{
	TokenType type;
	size_t startIdx;
	size_t endIdx;

	Token() : type(TokenType::None), startIdx(0), endIdx(0) {}
	Token(const TokenType type)
		: type(type)
		, startIdx(0)
		, endIdx(0)
	{
	}

	Token(const TokenType type, const size_t startIdx, const size_t endIdx)
		: type(type)
		, startIdx(startIdx)
		, endIdx(endIdx)
	{
	}
};

struct JsonValue
{
	JsonValue() : label("Null") {};
	~JsonValue() {};

	JsonValue(const std::string_view& val) : value(val) {};
	JsonValue(std::string_view&& val) : value(std::move(val)) {};

	const JsonValue& FindByLabel(const std::string_view& label) const;

	static JsonValue nullValue;

	std::string_view label;
	std::string_view value;
	std::unique_ptr<JsonValue> firstSubValue;
	std::unique_ptr<JsonValue> nextSibling;
};

class JsonParser
{
	public:
		void Read(const std::string fileName);
		std::vector<HaversinePair> Parse();

	private:
		std::unique_ptr<JsonValue> CreateTree();
		void DestroyTree(std::unique_ptr<JsonValue>& node);

		void ParsePairs(std::vector<HaversinePair>& pairsOut, const std::unique_ptr<JsonValue>& root);

		std::unique_ptr<JsonValue> GetJsonValue(const Token& token);

		Token GetNextToken() const;

	private:
		std::vector<char> buffer;

		mutable size_t bufIdx = 0;
};





