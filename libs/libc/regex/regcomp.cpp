#include <assert.h>
#include <bits/malloc.h>
#include <regex.h>

#include "regex_graph.h"
#include "regex_lexer.h"
#include "regex_parser.h"
#include "regex_value.h"

extern "C" int regcomp(regex_t* __restrict regex, const char* __restrict str, int cflags) {
    assert(!(cflags & REG_EXTENDED));

    int error = 0;
    RegexGraph* compiled = nullptr;
    RegexLexer lexer(str, cflags);
    RegexParser parser(lexer, cflags);

    if (!lexer.lex()) {
        error = lexer.error_code();
        goto regcomp_error;
    }

    if (!parser.parse()) {
        error = parser.error_code();
        goto regcomp_error;
    }

    if (!parser.result().is<ParsedRegex>()) {
        error = REG_BADPAT;
        goto regcomp_error;
    }

    compiled = static_cast<RegexGraph*>(malloc(sizeof(RegexGraph)));
    if (!compiled) {
        error = REG_ESPACE;
        goto regcomp_error;
    }

    new (compiled) RegexGraph(parser.result().as<ParsedRegex>(), cflags);

    regex->re_nsub = lexer.num_sub_expressions();
    regex->__re_compiled_data = static_cast<void*>(compiled);
    return 0;

regcomp_error:
    regex->__re_compiled_data = nullptr;
    return error;
}