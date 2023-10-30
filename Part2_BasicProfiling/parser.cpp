#include <iostream>
#include <fstream>
#include <assert.h>
#include <filesystem>

#include "parser.h"
#include "haversine.h"
#include "profiler.h"

JsonValue JsonValue::nullValue = {};

static bool CompareBuffer(const std::vector<char>& buffer, const size_t idx, const std::string_view str)
{
	if (idx + str.size() >= buffer.size()) {
		return false;
	}

	for (size_t i = 0; i < str.size(); ++i) {
		if (buffer[idx + i] != str[i]) {
			return false;
		}
	}

	return true;
}

static Token ReadBooleanOrNullToken(const char c, const std::vector<char>& buffer, const size_t at)
{
	assert(c == 'n' || c == 't' || c == 'f');

	Token resToken(TokenType::None);

	switch (c) {
		case 'n':
			if (CompareBuffer(buffer, at, "ull")) {
				resToken.type = TokenType::NullValue;
				resToken.endIdx = at + 3;
			}
			break;
		case 't':
			if (CompareBuffer(buffer, at, "rue")) {
				resToken.type = TokenType::BooleanTrue;
				resToken.endIdx = at + 3;
			}
			break;
		case 'f':
			if (CompareBuffer(buffer, at, "alse")) {
				resToken.type = TokenType::BooleanFalse;
				resToken.endIdx = at + 4;
			}
			break;
		default:
			std::cout << "Error in keywords" << std::endl;
			break;
	}

	return resToken;
}

static bool IsDigit(const char c)
{
	return (c >= '0') && (c <= '9');
}

static Token ReadNumberToken(const char c, const std::vector<char>& buffer, const size_t at)
{
	assert(c == '-' || IsDigit(c));

	const size_t bufferSize = buffer.size();
	const char* ptr = buffer.data();
	size_t idx = at;
	for (; idx < bufferSize ; ++idx) {
		const char n = *(ptr + idx);

		if (!IsDigit(n) && (n != '.')) {
			break;
		}
	}

	const size_t startIdx = at - 1;
	return Token{ TokenType::Number, startIdx, (idx - 1) };
}

static Token ReadStringToken(const char c, const std::vector<char>& buffer, const size_t startIdx)
{
	assert(c == '"');

	const size_t bufferSize = buffer.size();
	size_t idx = startIdx;
	for (; (idx < bufferSize) && (buffer[idx] != '"'); ++idx) {
		if ((idx + 1 < bufferSize) && (buffer[idx] == '\\' && (buffer[idx + 1] == '"'))) {
			++idx;
		}
	}

	return Token(TokenType::String, startIdx, (idx - 1));
}

bool CheckDigit(char c)
{
	u8 val = (u8)c - (u8)'0';
	return val < 10;
}

f64 ToFloat(const char* str) {
	f64 result = 0.0;
    f64 sign = 1;

    // Handle sign.
    if (*str == '-') {
        sign = -1;
        ++str;
    }

    // Process digits before decimal point.
    while (IsDigit(*str)) {
        result = result * 10.0 + static_cast<f64>(*str - '0');
        ++str;
    }

    // Process decimal point and digits after it.
    if (*str == '.') {
		f64 c = 0.0;

		// Use Kahan summation to minimize precision errors.
        f64 fraction = 0.1;
		for (++str; CheckDigit(*str); ++str) {
			const f64 digit = static_cast<f64>(*str - '0');
			const f64 y = digit * fraction - c;
			const f64 t = result + y;
			c = (t - result) - y;
			result = t;
			fraction *= 0.1;
		}
    }

    return result * sign;
}

void JsonParser::Read(const std::string fileName)
{
	PROFILE_BLOCK_FUNCTION;

	std::ifstream file(fileName);
	if (!file.is_open()) {
		return;
	}

	const uintmax_t size = std::filesystem::file_size(fileName);
	buffer.resize(size);
	file.read(buffer.data(), size);
	file.close();
}

std::vector<HaversinePair> JsonParser::Parse()
{
	PROFILE_BLOCK_FUNCTION;

	if (buffer.empty()) {
		return std::vector<HaversinePair>();
	}

	const size_t maxPairCount = buffer.size() / (24 * 4);
	std::vector<HaversinePair> pairs;
	pairs.reserve(maxPairCount);

	std::unique_ptr<JsonValue> root = CreateTree();

	ParsePairs(pairs, root);

	{
		PROFILE_BLOCK("Destroy Tree");
		DestroyTree(root);
	}

	return pairs;
}

void JsonParser::ParsePairs(std::vector<HaversinePair>& pairsOut, const std::unique_ptr<JsonValue>& root)
{
	PROFILE_BLOCK_FUNCTION;

	const JsonValue& pairs = root->FindByLabel("pairs");
	if (pairs.label == "Null") {
		return;
	}

	const std::unique_ptr<JsonValue>* currentPtr = &pairs.firstSubValue;
	while (*currentPtr) {
		const f64 x0 = ToFloat((*currentPtr)->FindByLabel("x0").value.data());
		const f64 y0 = ToFloat((*currentPtr)->FindByLabel("y0").value.data());
		const f64 x1 = ToFloat((*currentPtr)->FindByLabel("x1").value.data());
		const f64 y1 = ToFloat((*currentPtr)->FindByLabel("y1").value.data());
		pairsOut.emplace_back(HaversinePair(x0, y0, x1,y1));

		currentPtr = &(*currentPtr)->nextSibling;
	}
}

void JsonParser::DestroyTree(std::unique_ptr<JsonValue>& node)
{
	if (node.get() == nullptr) {
		return;
	}

	std::unique_ptr<JsonValue> ptr(node.release());
	while (ptr.get()) {
		std::unique_ptr<JsonValue> freeNode(ptr.release());
		ptr.swap(freeNode->nextSibling);

		DestroyTree(freeNode->firstSubValue);
		freeNode.reset();
	}
}

Token JsonParser::GetNextToken() const
{
	char c = buffer[bufIdx++];

	while (c == ' ' || c == '\n') {
		c = buffer[bufIdx++];
	}

	switch (c) {
		case 'EOF':
			return Token{ TokenType::EndOfFile };
		case '{':
			return Token{ TokenType::OpenCurlyBrace };
		case '}':
			return Token{ TokenType::CloseCurlyBrace };
		case '[':
			return Token{ TokenType::OpenSquareBracket };
		case ']':
			return Token{ TokenType::CloseSquareBracket };
		case ':':
			return Token{ TokenType::Colon };
		case ',':
			return Token{ TokenType::Comma };
		case '"': {
			const Token token = ReadStringToken(c, buffer, bufIdx);
			if (token.type == TokenType::None) {
				std::cout << "Invalid JSON, " << c << std::endl;
				return Token();
			}
			bufIdx = token.endIdx + 2;
			return token;
		}
		case 'n':
		case 'f':
		case 't': {
			const Token token = ReadBooleanOrNullToken(c, buffer, bufIdx);
			if (token.type == TokenType::None) {
				std::cout << "Invalid JSON at: " << c << std::endl;
				return Token();
			}
			bufIdx = token.endIdx + 1;
			return token;
		}
		default: {
			if ((c == '-') || IsDigit(c)) {
				const Token token = ReadNumberToken(c, buffer, bufIdx);
				bufIdx = token.endIdx + 1;
				return token;
			}

			std::cout << "Invalid JSON character: " << c << std::endl;
			break;
		}
	}

	return Token();
}

std::unique_ptr<JsonValue> JsonParser::CreateTree()
{
	PROFILE_BLOCK_FUNCTION;
	Token token = GetNextToken();
	return GetJsonValue(token);
}

std::unique_ptr<JsonValue> JsonParser::GetJsonValue(const Token& token)
{
	PROFILE_BLOCK_FUNCTION;

	switch (token.type) {
		case TokenType::BooleanTrue:
		case TokenType::BooleanFalse:
		case TokenType::NullValue:
		case TokenType::Number:
		case TokenType::String:
			return std::make_unique<JsonValue>(std::string_view(&buffer[token.startIdx], (token.endIdx - token.startIdx)));
		case TokenType::OpenCurlyBrace:
		case TokenType::OpenSquareBracket:
			return GetJsonList(token);

		default: {
			const size_t count = (token.endIdx + 1) - token.startIdx;
			const std::string errorString(&buffer[token.startIdx], count);
			std::cout << "Erronous JSON at: " << errorString << std::endl;
			break;
		}
	}

	return std::unique_ptr<JsonValue>();
}

std::unique_ptr<JsonValue> JsonParser::GetJsonList(const Token& token)
{
	switch (token.type) {
		case TokenType::OpenCurlyBrace: {
			auto res = std::make_unique<JsonValue>();
			std::unique_ptr<JsonValue>* lastPtr = &res->firstSubValue;

			for (Token nextToken = GetNextToken(); nextToken.type != TokenType::None; nextToken = GetNextToken()) {
				if (nextToken.type == TokenType::CloseCurlyBrace) {
					return res;
				}

				if (nextToken.type == TokenType::Comma) {
					continue;
				}

				if (nextToken.type != TokenType::String) {
					const size_t count = (nextToken.endIdx + 1) - nextToken.startIdx;
					const std::string errorString(&buffer[nextToken.startIdx], count);
					std::cout << "Erronous JSON, expected string but instead got -> " << errorString << std::endl;
					return nullptr;
				}

				const Token colonToken = GetNextToken();
				if (colonToken.type != TokenType::Colon) {
					const size_t count = (colonToken.endIdx + 1) - colonToken.startIdx;
					const std::string errorString(&buffer[token.startIdx], count);
					std::cout << "Erronous JSON, expected colon but instead got -> " << errorString << std::endl;
					return nullptr;
				}

				*lastPtr = GetJsonValue(GetNextToken());

				const size_t count = (nextToken.endIdx + 1) - nextToken.startIdx;
				(*lastPtr)->label = { &buffer[nextToken.startIdx], count };

				lastPtr = &(*lastPtr)->nextSibling;
			}

			std::cout << "Erronous EOF for JSON " << std::endl;
			break;
		}
		case TokenType::OpenSquareBracket: {
			auto res = std::make_unique<JsonValue>();
			std::unique_ptr<JsonValue>* lastPtr = &res->firstSubValue;

			for(Token nextToken = GetNextToken(); nextToken.type != TokenType::None; nextToken = GetNextToken()) {
				if (nextToken.type == TokenType::CloseSquareBracket) {
					return res;
				}

				if (nextToken.type == TokenType::Comma) {
					continue;
				}

				*lastPtr = GetJsonValue(nextToken);
				lastPtr = &(*lastPtr)->nextSibling;
			}

			std::cout << "Erronous EOF for JSON " << std::endl;
			break;
		}
	}

	return std::unique_ptr<JsonValue>();
}

const JsonValue& JsonValue::FindByLabel(const std::string_view& label) const
{
	const std::unique_ptr<JsonValue>* currentPtr = &firstSubValue;
	while (*currentPtr) {
		if ((*currentPtr)->label == label) {
			return *currentPtr->get();
		}

		currentPtr = &(*currentPtr)->nextSibling;
	}

	return JsonValue::nullValue;
}
