#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "lexer.h"
#include "utils.h"

const char* element_keywords[115] = {
    "a", "abbr", "address", "area", "article", "aside", "audio",
    "b", "base", "bdi", "bdo", "blockquote", "body", "br", "button",
    "canvas", "caption", "cite", "code", "col", "colgroup",
    "data", "datalist", "dd", "del", "details", "dfn", "dialog", "div", "dl", "dt",
    "em", "embed",
    "fieldset", "figcaption", "figure", "footer", "form",
    "h1", "h2", "h3", "h4", "h5", "h6", "head", "header", "hr", "html",
    "i", "iframe", "img", "input", "ins",
    "kbd", "keygen",
    "label", "legend", "li", "link",
    "main", "map", "mark", "menu", "menuitem", "meta", "meter",
    "nav", "noscript",
    "object", "ol", "optgroup", "option", "output",
    "p", "param", "pre", "progress",
    "q",
    "rp", "rt", "ruby",
    "s", "samp", "script", "section", "select", "small", "source", "span", "strong", "style", "sub", "summary", "sup",
    "table", "tbody", "td", "textarea", "tfoot", "th", "thead", "time", "title", "tr", "track",
    "u", "ul",
    "var", "video",
    "wbr"
};
const char* attribute_keywords[160] = {
    "accept", "accept-charset", "accesskey", "action", "align", "alt", "async",
    "autocomplete", "autofocus", "autoplay",
    "bgcolor", "border",
    "challenge", "charset", "checked", "cite", "class", "code", "codebase", "color", "cols", "colspan", "content", "contenteditable", "contextmenu", "controls", "coords",
    "data", "data-*", "datetime", "default", "defer", "dir", "dirname", "disabled", "download", "draggable",
    "enctype",
    "for", "form", "formaction", "headers", "height", "hidden", "high", "href", "hreflang", "http-equiv",
    "icon", "id", "ismap",
    "keytype", "kind",
    "label", "lang", "list", "loop", "low",
    "manifest", "max", "maxlength", "media", "method", "min", "multiple", "muted",
    "name", "novalidate",
    "open", "optimum",
    "pattern", "ping", "placeholder", "poster", "preload",
    "radiogroup", "readonly", "rel", "required", "reversed", "rows", "rowspan",
    "sandbox", "scope", "scoped", "seamless", "selected", "shape", "size", "sizes", "span", "spellcheck", "src", "srcdoc", "srclang", "srcset", "start", "step", "style", "subject", "summary",
    "tabindex", "target", "title", "type",
    "usemap",
    "value"
};

Token next_token(Lexer* lexer);
static void skip_whitespace(Lexer* lexer);
static uint8_t is_valid(uint8_t ch);
static void capture_token(Lexer* lexer, Token* token); 
static uint8_t bpeek(Lexer* lexer);
static uint8_t fpeek(Lexer* lexer);
static void forward(Lexer* lexer);
static void backward(Lexer* lexer);

Token* lex(char* path, size_t* len) {
    FILE* fp = fopen(path, "r");
    if (fp == NULL)
        return NULL;

    long file_size = get_file_size(path);
    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) {
        fclose(fp);
        perror("could not allocate memory");
        return NULL;
    }

    size_t read_size = fread(buffer, 1, file_size, fp); 

    /* Making sure the read size is the same as 
     * the size of the file 
     */
    if (read_size != file_size) {
        fclose(fp);
        free(buffer);
        perror("could not read file");
        return NULL;
    }

    buffer[file_size] = '\0';

    // initiate lexer
    Lexer lexer;
    lexer.input = (char*)malloc(strlen(buffer) + 1);
    strcpy(lexer.input, buffer);
    lexer.position = 0;
    lexer.ch = lexer.input[lexer.position];

    free(buffer);

    /* creating pointer to the stored tokens and keeping
     * track of the size 
     */
    Token* tokens = NULL;
    Token token;

    while ((token = next_token(&lexer)).type != EOF_TYPE) {
        tokens = (Token*)realloc(tokens, sizeof(Token) * (*len + 1)); 
        tokens[*len] = token; 
        ++(*len);
    }

    fclose(fp);
    free(lexer.input); 
    return tokens;
}

Token next_token(Lexer* lexer) {
    Token token;
    char current_ch = lexer->ch;

    if (lexer->position >= strlen(lexer->input) && lexer->ch == '\0') {
        token.type = EOF_TYPE;
        token.content = NULL;
        return token;
    }
    
    skip_whitespace(lexer);

    switch (lexer->ch) {
        case '<':
            token.type = L_ANGLE;
            break;
        case '>':
            token.type = R_ANGLE;
            break; 
        case '{':
            token.type = L_BRACKET;
            break;
        case '}':
            token.type = R_BRACKET;
            break;
        case '[':
            token.type = L_SQ_BRACKET;
            break;  
        case ']':
            token.type = R_SQ_BRACKET;
            break;  
        case '(':
            token.type = R_PARENT;
            break;
        case ')':
            token.type = L_PARENT;
            break;
        case '!':
            token.type = BANG;
            break;
        case '\'':
            token.type = S_QUOTE;
            break;
        case '\"':
            token.type = D_QUOTE;
            break;
        case ':':
            token.type = COLON;
            break;
        case ';':
            token.type = SEMI_COLON;
            break;
        case ',':
            token.type = COMMA;
            break;
        case '=':
            token.type = EQUALS;
            break;
        case '%':
            token.type = MOD;
            break;
        case '/':
            token.type = F_SLASH;
            break;
        case '#':
            token.type = HASH;
            break;
        case '.':
            token.type = DOT;
            break;
        case '@':
            token.type = AT;
            break;
        case '&':
            token.type = AND;
            break;
        case '\n':
            skip_whitespace(lexer);
            return next_token(lexer);
        default:
            if (isalnum(lexer->ch)) {
                capture_token(lexer, &token);
            }
            forward(lexer);
            return token;
    }
    
    /* allocating lexer->ch to a string and assigning token.content
     * unless lexer->ch is alphanumeric  
     */
    
    // note: suspected memory corruption bug here
    if (current_ch != '\"' && current_ch != '\'') {
        char token_content[2] = {current_ch};
        token.content = str_x_dup(token_content);
    }

    forward(lexer);
    return token;
}

static void skip_whitespace(Lexer* lexer) {
    while (isspace(lexer->ch) && lexer->position < strlen(lexer->input))
        forward(lexer);
}

static uint8_t is_valid(uint8_t ch) {
    if (isalnum(ch)) {
        return 1;
    }
    return 0;
}

static void capture_token(Lexer* lexer, Token* token) {
    size_t len = 0;

    while (lexer->position + len < strlen(lexer->input) && is_valid(lexer->input[lexer->position + len])) {
        ++len;
    }

    if (lexer->position > 0 && (bpeek(lexer) == '\"' || (bpeek(lexer) == '\'' && fpeek(lexer) == bpeek(lexer)))) {
        token->type = TEXT;
    }

    /* allocate the needed memory */
    token->content = (char*)malloc(len + 1);
    if (token->content == NULL)
        return;

    for (size_t i = 0; i < len; ++i) {
        token->content[i] = lexer->input[lexer->position + i];
    }

    token->content[len] = '\0';
    lexer->position += len; 
    lexer->ch = lexer->input[lexer->position];

    if (token->type != TEXT) {
        // iter element keywords
        for (size_t i = 0; i < 115; ++i) {
            if (strcmp(token->content, element_keywords[i]) == 0) {
                token->type = ELEMENT_KWORD;
                return;
            }
        }

        for (size_t i = 0; i < 160; ++i) {
            if (strcmp(token->content, attribute_keywords[i]) == 0) {
                token->type = ATTRIBUTE_KWORD;
                return;
            }
        }
    }

    token->type = OTHER;
}

static uint8_t fpeek(Lexer* lexer) {
    if (lexer->position + 1 < strlen(lexer->input)) {
        return lexer->input[lexer->position + 1];
    }

    return '\0';
}

static uint8_t bpeek(Lexer* lexer) {
    if (lexer->position > 0) {
        return lexer->input[lexer->position - 1];
    } 

    return '\0';
}

static void forward(Lexer* lexer) {
    if (lexer->ch != '\0' && lexer->position < strlen(lexer->input)) {
        lexer->ch = lexer->input[++lexer->position];
    }
}

static void backward(Lexer* lexer) {
    if (lexer->position > 0 && lexer->ch != '\0') {
        lexer->ch = lexer->input[--lexer->position];
    }
}
