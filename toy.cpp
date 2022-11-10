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

// 运算符的优先级
static std::map<char, int> BinopPrecedence;

// 获取运算符的优先级
static int GetTokPrecedence() {
    if (!isascii(CurTok)) { // 如果当前的token不是ascii码
        return -1;
    }

    // 保证token是一个声明了的binop
    int TokPrec = BinopPrecedence[CurTok];
    // 如果不在map中
    // 对于不是binop的运算符返回-1
    if (TokPrec <= 0) {
        return -1;
    }

    return TokPrec;
}

// 用于处理错误
std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "LogError: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}

static std::unique_ptr<ExprAST> ParseExpression();

// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken(); // 吞掉当前number
    return std::move(Result);
}

// 括号的情况
// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken(); // 吞掉'('
    auto V = ParseExpression();
    if (!V) {
        return nullptr;
    }

    if (CurTok != ')') {
        return LogError("expected ')'");
    }
    getNextToken(); // 吞掉')'
    return V;
}

// identifierexpr
// ::= identifier
// ::= identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;

    getNextToken(); // 吞掉identifier

    // 简单的变量引用
    if (CurTok != '(') {
        return std::make_unique<VariableExprAST>(IdName);
    }

    getNextToken(); // 吞掉'('
    std::vector<std::unique_ptr<ExprAST>> Args;
    // 排除()中没有表达式的情况
    if (CurTok != ')') {
        while (true) {
            if (auto Arg = ParseExpression()) {
                Args.push_back(std::move(Arg));
            } else {
                return nullptr;
            }
            
            if (CurTok == ')') {
                break;
            }

            if (CurTok != ',') {
                return LogError("Expected ')' or ',' in argument list");
            }
            getNextToken();
        }
    }

    getNextToken(); // 吞掉')'

    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

// primary
// ::= identifierexpr
// ::= numberexpr
// ::= parenexpr
static std::unique_ptr<ExprAST> ParsePrimay() {
    switch (CurTok) {
    default:
        return LogError("unknown token when expecting an expression");
    case tok_identifier:
        return ParseIdentifierExpr();
    case tok_number:
        return ParseNumberExpr();
    case '(':
        return ParseParenExpr();
    }
}

// binoprhs
// ::= ('+' primary)*
// LHS是当前已经转换的部分
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, 
                                              std::unique_ptr<ExprAST> LHS) {
    while (true) {
        int TokPrec = GetTokPrecedence();

        if (TokPrec < ExprPrec) {
            // 运算符的优先级小于当前的优先级，直接返回LHS
            return LHS; 
        }

        // 存储运算符
        int BinOp = CurTok;
        getNextToken();

        // 拿到下一个primary，运行后此时的token指向下一个运算符
        auto RHS = ParsePrimay();
        if (!RHS) {
            return nullptr;
        }

        // 现在已经处理完了LHS以及序列中的下一个RHS，接下处理两者如何连接
        // 一种有两种情况: (a+b) binop unparsed 或 a + (b binop unparsed)

        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            // 情况: a + (b binop unparsed) 
            // 当前运算符的优先级小于下一个运算符的优先级

            // 要保证后续的RHS都被正确转换，先递归的转换RHS，将转换完成的RHS与LHS挂在一起
            // 转换后续RHS的时候实际上是优先考虑的后续的运算符，即优先转换后面的部分
            // 这里传入的RHS实际上是下一次调用的LHS，即已经转换之后的
            // TokPrec + 1是因为后续的想要继续处理，那么后续运算符的优先级应该高于当前的运算符
            // 如果不加1，那么后续大于或者等于的都可以继续处理
            // 加1不会影响后续的函数功能，因为ExprPrec在后续函数没有继续做加1操作
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS) {
                return nullptr;
            }
        }

        // 情况: (a+b) binop unparsed
        // 上述if相反的情况，RHS的下一个op的优先级小于当前的op，那么LHS和RHS分别居于op的两侧
        // 使用下方代码连接

        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}

// expression
// ::= primary binoprhs
static std::unique_ptr<ExprAST> ParseExpression() {
    // 拿到一个表达式的第一个变量，然后将后续的交给ParseBinOPRHS
    auto LHS = ParsePrimay();
    if (!LHS) {
        return nullptr;
    }

    return ParseBinOpRHS(0, std::move(LHS));
}

// prototype
// ParseDefination调用
// ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != tok_identifier) {
        return LogErrorP("Expected function name in prototype");
    }

    std::string FnName = IdentifierStr;
    getNextToken(); // 吞掉'('

    if (CurTok != '(') {
        return LogErrorP("Expected '(' in prototype");
    }

    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier) {
        ArgNames.push_back(IdentifierStr);
    }

    if (CurTok != ')') {
        return LogErrorP("Expected ')' in prototype");
    }

    getNextToken(); // 吞掉')'

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

// defination ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefination() {
    getNextToken(); // 吞掉def
    auto Proto = ParsePrototype();
    if (!Proto) {
        return nullptr;
    }

    if (auto E = ParseExpression()) {
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }

    return nullptr;
}

// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        // 匿名的proto
        auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken(); // 吞掉extern
    return ParsePrototype();
}

//=========
// Top-Level parsing
//=========
static void HandleDefinition() {
    if (ParseDefination()) {
        fprintf(stderr, "Parsed a function defination.\n");
    } else {
        // 忽略错误的token
        getNextToken();
    }
}

static void HandleExtern() {
    if (ParseExtern()) {
        fprintf(stderr, "Parsed an extern\n");
    } else {
        // 忽略错误的token
        getNextToken();
    }
}

static void HandleTopLevelExpresison() {
    if (ParseTopLevelExpr()) {
        fprintf(stderr, "Parsed a top-level expr\n");
    } else {
        // 忽略错误的token
        getNextToken();
    }
}

// top ::= definition | external | expresison | ';'
static void MainLoop() {
    while (true) {
        fprintf(stderr, "ready>");
        switch (CurTok) {
        case tok_eof:
            return;
        case ';':
            getNextToken();
            break;
        case tok_def:
            HandleDefinition();
            break;
        case tok_extern:
            HandleExtern();
            break;
        default:
            break;
        }
    }
}

//=========
// Main driver code
//=========
int main() {
    // 1是最低的优先级
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 30;
    BinopPrecedence['*'] = 40;

    fprintf(stderr, "ready> ");
    getNextToken();

    MainLoop();
    return 0;
}