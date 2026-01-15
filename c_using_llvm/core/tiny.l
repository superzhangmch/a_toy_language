%{
/*
 * Lexer for tiny language
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "tiny.tab.h"

extern int yylineno;
%}

%option noyywrap
%option yylineno

%%

[ \t\r]+                { /* skip whitespace */ }
\n                      { /* skip newlines */ }
#.*                     { /* skip comments */ }

"var"                   { return VAR; }
"fun"                   { return FUN; }
"return"                { return RETURN; }
"if"                    { return IF; }
"else"                  { return ELSE; }
"while"                 { return WHILE; }
"for"                   { return FOR; }
"in"                    { return IN; }
"break"                 { return BREAK; }
"continue"              { return CONTINUE; }
"class"                 { return CLASS; }
"new"                   { return NEW; }
"not_in"                { return NOT_IN; }
"try"                   { return TRY; }
"catch"                 { return CATCH; }
"raise"                 { return RAISE; }
"assert"                { return ASSERT; }
"true"                  { yylval.bval = 1; return TRUE; }
"false"                 { yylval.bval = 0; return FALSE; }
"null"                  { return NULL_LITERAL; }
"NULL"                  { return NULL_LITERAL; }
"and"                   { return AND; }
"or"                    { return OR; }
"not"                   { return NOT; }

[a-zA-Z_][a-zA-Z0-9_]*  { yylval.sval = strdup(yytext); return IDENTIFIER; }

[0-9]+\.[0-9]*          { yylval.fval = atof(yytext); return FLOAT; }
[0-9]+                  { yylval.ival = atoi(yytext); return INTEGER; }

\"([^"\\]|\\.|\\n)*\"       {
                            yylval.sval = strndup(yytext + 1, strlen(yytext) - 2);
                            /* Process escape sequences */
                            char *p = yylval.sval;
                            char *q = yylval.sval;
                            while (*p) {
                                if (*p == '\\' && *(p+1)) {
                                    p++;
                                    switch (*p) {
                                        case 'n': *q++ = '\n'; break;
                                        case 't': *q++ = '\t'; break;
                                        case '\\': *q++ = '\\'; break;
                                        case '"': *q++ = '"'; break;
                                        default: *q++ = *p; break;
                                    }
                                    p++;
                                } else {
                                    *q++ = *p++;
                                }
                            }
                            *q = '\0';
                            return STRING;
                        }

'([^'\\]|\\.|\\n)*'       {
                            yylval.sval = strndup(yytext + 1, strlen(yytext) - 2);
                            return STRING;
                        }

\"\"\"                 {
                            int cap = 1024, len = 0;
                            char *buf = malloc(cap);
                            while (1) {
                                int c = input();
                                if (c == 0) break;
                                if (c == '\n') yylineno++;
                                if (c == '"') {
                                    int c1 = input();
                                    int c2 = input();
                                    if (c1 == '"' && c2 == '"') {
                                        break;
                                    } else {
                                        unput(c2);
                                        unput(c1);
                                        buf[len++] = '"';
                                    }
                                } else if (c == '\\') {
                                    int next = input();
                                    if (next == 'n') buf[len++] = '\n';
                                    else if (next == 't') buf[len++] = '\t';
                                    else if (next == '"') buf[len++] = '"';
                                    else if (next == '\\') buf[len++] = '\\';
                                    else { buf[len++] = next; }
                                } else {
                                    buf[len++] = c;
                                }
                                if (len >= cap - 1) { cap *= 2; buf = realloc(buf, cap); }
                            }
                            buf[len] = '\0';
                            yylval.sval = buf;
                            return STRING;
                        }

\'\'\'                 {
                            int cap = 1024, len = 0;
                            char *buf = malloc(cap);
                            while (1) {
                                int c = input();
                                if (c == 0) break;
                                if (c == '\n') yylineno++;
                                if (c == '\'') {
                                    int c1 = input();
                                    int c2 = input();
                                    if (c1 == '\'' && c2 == '\'') {
                                        break;
                                    } else {
                                        unput(c2);
                                        unput(c1);
                                        buf[len++] = '\'';
                                    }
                                } else if (c == '\\') {
                                    int next = input();
                                    if (next == 'n') buf[len++] = '\n';
                                    else if (next == 't') buf[len++] = '\t';
                                    else if (next == '\'') buf[len++] = '\'';
                                    else if (next == '\\') buf[len++] = '\\';
                                    else { buf[len++] = next; }
                                } else {
                                    buf[len++] = c;
                                }
                                if (len >= cap - 1) { cap *= 2; buf = realloc(buf, cap); }
                            }
                            buf[len] = '\0';
                            yylval.sval = buf;
                            return STRING;
                        }

"=>"                    { return ARROW; }
"+="                    { return PLUS_ASSIGN; }
"-="                    { return MINUS_ASSIGN; }
"*="                    { return MULTIPLY_ASSIGN; }
"/="                    { return DIVIDE_ASSIGN; }
\.\.                    { return DOTDOT; }

"+"                     { return PLUS; }
"-"                     { return MINUS; }
"*"                     { return MULTIPLY; }
"/"                     { return DIVIDE; }
"%"                     { return MODULO; }

"=="                    { return EQ; }
"!="                    { return NE; }
"!"                     { return NOT; }
"<="                    { return LE; }
">="                    { return GE; }
"<"                     { return LT; }
">"                     { return GT; }

"="                     { return ASSIGN; }

"("                     { return LPAREN; }
")"                     { return RPAREN; }
"{"                     { return LBRACE; }
"}"                     { return RBRACE; }
"["                     { return LBRACKET; }
"]"                     { return RBRACKET; }
","                     { return COMMA; }
":"                     { return COLON; }
";"                     { return SEMICOLON; }
"."                     { return DOT; }

.                       { fprintf(stderr, "Unexpected character: %s at line %d\n", yytext, yylineno); }

%%
