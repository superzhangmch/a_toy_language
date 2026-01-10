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
"func"                  { return FUNC; }
"return"                { return RETURN; }
"if"                    { return IF; }
"else"                  { return ELSE; }
"while"                 { return WHILE; }
"break"                 { return BREAK; }
"continue"              { return CONTINUE; }
"true"                  { yylval.bval = 1; return TRUE; }
"false"                 { yylval.bval = 0; return FALSE; }
"and"                   { return AND; }
"or"                    { return OR; }
"not"                   { return NOT; }

[a-zA-Z_][a-zA-Z0-9_]*  { yylval.sval = strdup(yytext); return IDENTIFIER; }

[0-9]+\.[0-9]+          { yylval.fval = atof(yytext); return FLOAT; }
[0-9]+                  { yylval.ival = atoi(yytext); return INTEGER; }

\"([^"\\]|\\.)*\"       {
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

\'([^'\\]|\\.)*\'       {
                            yylval.sval = strndup(yytext + 1, strlen(yytext) - 2);
                            return STRING;
                        }

"+="                    { return PLUS_ASSIGN; }
"-="                    { return MINUS_ASSIGN; }
"*="                    { return MULTIPLY_ASSIGN; }
"/="                    { return DIVIDE_ASSIGN; }

"+"                     { return PLUS; }
"-"                     { return MINUS; }
"*"                     { return MULTIPLY; }
"/"                     { return DIVIDE; }
"%"                     { return MODULO; }

"=="                    { return EQ; }
"!="                    { return NE; }
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

.                       { fprintf(stderr, "Unexpected character: %s at line %d\n", yytext, yylineno); }

%%
