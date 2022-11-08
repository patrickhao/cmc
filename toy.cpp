#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

//=========
// Lexer
//=========

// 如果是未知的字符Lexer返回在[0, 255]内的token，否则是已知的
enum Token {
    tok_eof = -1,

    tok_def = -2,
    tok_extern = -3,

    tok_identifier = -4, // 标识符
    tok_number = -5
};

static std::string IdentifierStr;
static double NumVal;

// 从标准输入中返回下一个token
static int gettok() {
    static int LastChar = ' ';

    // 跳过空格
    while (isspace(LastChar)) {
        LastChar = getchar();
    }

    // 不能以数字开头，但是后续的可以出现数字，因此只有最开始判断isalpha
    // 实际中不允许以数字开头生命变量，可能也是这个原因，和第二部分的判断冲突
    if (isalpha(LastChar)) {
        IdentifierStr = LastChar;

        // 拿到一个完整的字母数字组合
        while (isalnum((LastChar = getchar()))) { // identifier: [a-zA-Z][a-zA-Z0-9]*
            IdentifierStr += LastChar;
        }

        if (IdentifierStr == "def") {
            return tok_def;
        }

        if (IdentifierStr == "extern") {
            return tok_extern;
        }

        return tok_identifier;
    }

    // 数组
    if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == '.');

        // 从数组开始的指针到空指针，即整个数组
        NumVal = strtod(NumStr.c_str(), nullptr);
        return tok_number;
    }

    // 注释
    if (LastChar == '#') {
        do {
            LastChar = getchar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        // 在编译阶段，注释会被编译器忽视，因此这个函数在读完注释这一行后，什么都不做
        // 若还没有到文件末尾，则返回下一个token
        if (LastChar != EOF) {
            return gettok();
        }
    }

    if (LastChar != EOF) {
        return tok_eof;
    }

    // 否则，返回字符的ascii码
    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}