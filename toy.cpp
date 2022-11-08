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

//=========
// Abstract Syntax Tree
//=========

namespace {
// 基类，派生出各种ExprAST，让后续步骤知道在处理什么
class ExprAST {
public:
    virtual ~ExprAST() = default;
};

class NumberExprAST : public ExprAST {
    double Val;

public:
    NumberExprAST(double Val) : Val(Val) {}
};

class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string &Name) : Name(Name) {}
};

class BinaryExprAST : public ExprAST {
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;
public:
    // std::move()将对象的值直接移动过去，而不是复制，避免额外的内存空间开销
    BinaryExprAST(char op, std::unique_ptr<ExprAST> LHS,
                  std::unique_ptr<ExprAST> RHS)
        : Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    CallExprAST(const std::string &Callee,
                std::vector<std::unique_ptr<ExprAST>> Args)
        : Callee(Callee), Args(std::move(Args)) {}
};

// 表示函数原型的一些信息
class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;

public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args)
        : Name(Name), Args(std::move(Args)) {}
    
    const std::string &getName() const { return Name; }
};

class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto)), Body(std::move(Body)) {}
};
}; // end anonymous namespace

// =========
// Parser
// =========

// 提供一个简单的token缓冲区
// CurTok表示当前paser正在处理的token，即当前需要paser的token
// getNextToken()更新CurTok
static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

// 用于处理错误
std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "LogError: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}

static std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken();
    return std::move(Result);
}