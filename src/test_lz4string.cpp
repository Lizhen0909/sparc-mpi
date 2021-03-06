#include <exception>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cmath>

#include "acutest.h"
#include "sparc/LZ4String.h"
#include "sparc/Message.h"

using namespace std;

#define TEST_INT_EQUAL(a,b) \
		TEST_CHECK( (a) == (b)); \
		TEST_MSG("Expected: %d", (a)); \
		TEST_MSG("Produced: %d", (b));

#define TEST_DOUBLE_EQUAL(a,b) \
		TEST_CHECK( std::abs((double)(a) - (b))<1e-10); \
		TEST_MSG("Expected: %f", (double)(a)); \
		TEST_MSG("Produced: %f", (double)(b));

#define TEST_STR_EQUAL(a,b) \
		TEST_CHECK( string(a) == string(b)); \
		TEST_MSG("Expected: %s", (a)); \
		TEST_MSG("Produced: %s", (b));

void test_LZ4String(void) {
	char c[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	char c2[] = "BBBB";
	string _c3 = (string(c) + c2);
	const char *c3 = _c3.c_str();
	{
		LZ4String s;
		TEST_INT_EQUAL(0, s.length());
		TEST_INT_EQUAL(0, s.raw_length());
		TEST_STR_EQUAL("", s.toString().c_str());
	}
	{
		LZ4String s(c2);
		TEST_INT_EQUAL(strlen(c2), s.raw_length());
		//TEST_CHECK(s.length() < s.raw_length());
		TEST_STR_EQUAL(c2, s.toString().c_str());
	}

	{
		LZ4String s(c);
		TEST_INT_EQUAL(strlen(c), s.raw_length());
		TEST_CHECK(s.length() < s.raw_length());
		TEST_STR_EQUAL(c, s.toString().c_str());
	}
	{
		LZ4String s = c;
		TEST_INT_EQUAL(strlen(c), s.raw_length());
		TEST_CHECK(s.length() < s.raw_length());
		TEST_STR_EQUAL(c, s.toString().c_str());
	}
	{
		LZ4String s;
		s = c;
		TEST_INT_EQUAL(strlen(c), s.raw_length());
		TEST_CHECK(s.length() < s.raw_length());
		TEST_STR_EQUAL(c, s.toString().c_str());
	}

	{
		LZ4String s0(c);
		LZ4String s = s0 + c2;
		TEST_INT_EQUAL(strlen(c3), s.raw_length());
		TEST_CHECK(s.length() < s.raw_length());
		TEST_STR_EQUAL(c3, s.toString().c_str());
	}
	{
		LZ4String s(c);
		s += c2;
		TEST_INT_EQUAL(strlen(c3), s.raw_length());
		TEST_CHECK(s.length() < s.raw_length());
		TEST_STR_EQUAL(c3, s.toString().c_str());
	}
	{
		bool b = LZ4String() == LZ4String();
		TEST_CHECK(b);
		b = (LZ4String("A") == LZ4String("A"));
		TEST_CHECK(b);
		b = (LZ4String("") == LZ4String(""));
		TEST_CHECK(b);
		b = (LZ4String("") == LZ4String());
		TEST_CHECK(b);
		b = (LZ4String("ab") == LZ4String("a"));
		TEST_CHECK(!b);
	}
}

void test_LZ4String_and_zmq_message(void) {
	char c1[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	char c2[] = "ABCD";
	{
		LZ4String s(c1);
		TEST_INT_EQUAL(strlen(c1), s.raw_length());
		TEST_CHECK(s.length() < s.raw_length());
		TEST_STR_EQUAL(c1, s.toString().c_str());
		zmqpp::message message;
		message << s;
		LZ4String s2;
		message >> s2;
		bool b = s == s2;
		TEST_CHECK(b);
	}

	{
		LZ4String s(c2);
		TEST_INT_EQUAL(strlen(c2), s.raw_length());
		//TEST_CHECK(s.length() == s.raw_length());
		TEST_STR_EQUAL(c2, s.toString().c_str());
	}
}

void test_message(void) {
	uint32_t a = 12222;
	int8_t b = 12;
	float c = 1.2345;
	string s("ADFDSFSDFDS asafd ");
	uint32_t a2;
	int8_t b2;
	float c2;
	string s2;
	{
		Message msg;
		msg << a << b << s << c;

		msg >> a2 >> b2 >> s2 >> c2;
		TEST_INT_EQUAL(a, a2);
		TEST_INT_EQUAL(b, b2);
		TEST_DOUBLE_EQUAL(c, c2);
		TEST_STR_EQUAL(s, s2);
	}

	{
		Message msg;
		msg << b << c << a;

		msg >> b2 >> c2 >> a2;
		TEST_INT_EQUAL(a, a2);
		TEST_INT_EQUAL(b, b2);
		TEST_DOUBLE_EQUAL(c, c2);
	}

	{
		Message msg;
		msg << a << b << c;

		zmqpp::message message;
		message << msg;

		Message msg2;
		message >> msg2;
		msg2 >> a2 >> b2 >> c2;
		TEST_INT_EQUAL(a, a2);
		TEST_INT_EQUAL(b, b2);
		TEST_DOUBLE_EQUAL(c, c2);
	}
	{
		Message msg;
		for (int i = 0; i < 100; i++) {
			msg << a << b << c;
		}

		for (int i = 0; i < 100; i++) {
			msg >> a2 >> b2 >> c2;
			TEST_INT_EQUAL(a, a2);
			TEST_INT_EQUAL(b, b2);
			TEST_DOUBLE_EQUAL(c, c2);
		}

	}

}

void test_message_compressed(void) {
	uint32_t a = 12222;
	int8_t b = 12;
	float c = 1.2345;
	size_t d = 123213414;
	string s("ADFDSFSDFDS asafd ");
	uint32_t a2;
	int8_t b2;
	float c2;
	size_t d2;
	string s2;
	{
		Message msg(true);
		msg << a << s << b << c << d;

		zmqpp::message message;
		message << msg;

		Message msg2;
		message >> msg2;
		msg2 >> a2 >> s2 >> b2 >> c2 >> d2;
		TEST_INT_EQUAL(a, a2);
		TEST_INT_EQUAL(b, b2);
		TEST_DOUBLE_EQUAL(c, c2);
		TEST_INT_EQUAL(d, d2);
		TEST_STR_EQUAL(s, s2);

	}

	{
		Message msg(true);
		for (int i = 0; i < 100; i++) {
			msg << a << b << c;
		}

		zmqpp::message message;
		message << msg;

		Message msg2;
		message >> msg2;
		for (int i = 0; i < 100; i++) {
			msg2 >> a2 >> b2 >> c2;
			TEST_INT_EQUAL(a, a2);
			TEST_INT_EQUAL(b, b2);
			TEST_DOUBLE_EQUAL(c, c2);
		}

	}

}

void test_message_with_string(void) {
	{
		Message msg(true);
		char *longa =
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
		const char *longb = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";

		const string longc = "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC";
		string longd =
				"DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD";
		msg << longa << longb << longc << longd;

		std::string a, b, c, d;
		msg >> a >> b >> c >> d;
		TEST_STR_EQUAL(longa, a);
		TEST_STR_EQUAL(longb, b);
		TEST_STR_EQUAL(longc, c);
		TEST_STR_EQUAL(longd, d);
	}

	{
		Message msg;
		msg << "ABCD" << "CDE";
		std::string a, b;
		msg >> a >> b;
		TEST_STR_EQUAL("ABCD", a);
		TEST_STR_EQUAL("CDE", b);
	}
}

void test_message_with_bytes(void) {
	int a = -123;
	size_t b = 321;
	uint32_t c = 231;
	float d = -1.23;
	double e = 2.3445;
	bool f = true;
	char* longa="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	{
		void *p;
		size_t len;
		{
			Message msg;

			msg << "ABC" << "CDE" << a << b << c << d << e << f;
			p = msg.to_bytes(len);
		}
		{

			int a2;
			size_t b2;
			uint32_t c2;
			float d2;
			double e2;
			bool f2;
			Message msg;
			msg.from_bytes(p, len);
			std::string s1, s2;
			msg >> s1 >> s2 >> a2 >> b2 >> c2 >> d2 >> e2 >> f2;
			TEST_STR_EQUAL("ABC", s1);
			TEST_STR_EQUAL("CDE", s2);
			TEST_INT_EQUAL(a, a2);
			TEST_INT_EQUAL(b, b2);
			TEST_INT_EQUAL(c, c2);

			TEST_DOUBLE_EQUAL(d, d2);
			TEST_DOUBLE_EQUAL(e, e2);

			TEST_CHECK(f == f2);
		}
		free(p);
	}

	{
			void *p;
			size_t len;
			{
				Message msg(true);
				msg << longa << "CDE" << a << b << c << d << e << f;
				p = msg.to_bytes(len);
			}

			{

				int a2;
				size_t b2;
				uint32_t c2;
				float d2;
				double e2;
				bool f2;
				Message msg;
				msg.from_bytes(p, len);
				std::string s1, s2;
				msg >> s1 >> s2 >> a2 >> b2 >> c2 >> d2 >> e2 >> f2;
				TEST_STR_EQUAL(longa, s1);
				TEST_STR_EQUAL("CDE", s2);
				TEST_INT_EQUAL(a, a2);
				TEST_INT_EQUAL(b, b2);
				TEST_INT_EQUAL(c, c2);

				TEST_DOUBLE_EQUAL(d, d2);
				TEST_DOUBLE_EQUAL(e, e2);

				TEST_CHECK(f == f2);
			}
			free(p);
		}

}

void test_message_with_bytes2(void) {

	int8_t a=2;
	size_t b=3;
	uint32_t c = 1;
	uint32_t d = 2;
	uint32_t e = 3;
	{
		void *p;
		size_t len;
		{
			Message msg;

			msg << a << b << c << d <<e;
			p = msg.to_bytes(len);
		}
		{

			int8_t a2;
			size_t b2;
			uint32_t c2;
			uint32_t d2;
			uint32_t e2;
			Message msg;
			msg.from_bytes(p, len);
			msg >> a2 >> b2 >> c2 >> d2 >> e2 ;
			TEST_INT_EQUAL(a, a2);
			TEST_INT_EQUAL(b, b2);
			TEST_INT_EQUAL(c, c2);
			TEST_INT_EQUAL(d, d2);
			TEST_INT_EQUAL(e, e2);
		}
		free(p);
	}

}


TEST_LIST = { {"test_LZ4String", test_LZ4String},

	{	"test_LZ4String_and_zmq_message", test_LZ4String_and_zmq_message},

	{	"test_message", test_message},

	{	"test_message_compressed",test_message_compressed},

	{	"test_message_with_string",test_message_with_string},

	{	"test_message_with_bytes",test_message_with_bytes},

	{	"test_message_with_bytes2",test_message_with_bytes},


	{	NULL, NULL}};

