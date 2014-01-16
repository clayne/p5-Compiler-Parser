#include <parser.hpp>

using namespace std;
namespace TokenType = Enum::Token::Type;
namespace SyntaxType = Enum::Parser::Syntax;
namespace TokenKind = Enum::Token::Kind;

template<typename T> T **Array_insert(T **array, size_t array_size, size_t idx, T *ptr)
{
	T **temp = (T **)realloc(array, sizeof(T *) * (array_size + 1));
	if (!temp) {
		fprintf(stderr, "[ERROR] Cannot allocate memrory");
		exit(EXIT_FAILURE);
	}
	array = temp;
	memmove(array+idx+1, array+idx, sizeof(T *) * (array_size - idx));
	array[idx] = ptr;
	return array;
}

Completer::Completer(void)
{
	named_unary_keywords = new vector<string>();
	named_unary_keywords->push_back("defined");
	named_unary_keywords->push_back("die");
	named_unary_keywords->push_back("ref");
	named_unary_keywords->push_back("shift");
	named_unary_keywords->push_back("write");
	//named_unary_keywords->push_back("bless");
	named_unary_keywords->push_back("sqrt");
	named_unary_keywords->push_back("abs");
	named_unary_keywords->push_back("int");
	named_unary_keywords->push_back("rand");
	named_unary_keywords->push_back("sin");
	named_unary_keywords->push_back("cos");
	named_unary_keywords->push_back("atan2");
	named_unary_keywords->push_back("chr");
	named_unary_keywords->push_back("close");
}

void Completer::complete(Token *root)
{
	completeTerm(root);
	// ->
	completePointerExpr(root);
	// ++, --, *
	completeIncDecGlobExpr(root);
	// **
	completePowerExpr(root);
	// !, ~, \, +, -, &, ~~
	completeSingleTermOperatorExpr(root);
	// =~, !~
	completeRegexpMatchExpr(root);
	// *, /, %, x
	completeHighPriorityDoubleOperatorExpr(root);
	// +, -, .
	completeLowPriorityDoubleOperatorExpr(root);
	// <<, >>
	completeShiftOperatorExpr(root);
	//handler, builtin functions
	completeNamedUnaryOperators(root);
	recoveryNamedUnaryOperatorsArgument(root);
	// <, >, <=, >=, lt, gt, le, ge
	completeHighPriorityCompareOperatorExpr(root);
	// ==, !=, <=>, eq, ne, cmp, ~~
	completeLowPriorityCompareOperatorExpr(root);
	// &, |, ^
	completeBitOperatorExpr(root);
	// &&, ||
	completeAndOrOperatorExpr(root);
	//?:
	completeThreeTermOperatorExpr(root);
	// =, +=, -=, *=, ...
	completeAssignExpr(root);
	// ,, =>
	completeCommaArrowExpr(root);
	// function args ....
	completeFunctionListExpr(root);
	recoveryFunctionArgument(root);
	// not, and, or, xor
	completeAlphabetBitOperatorExpr(root);
}

void Completer::insertExpr(Token *syntax, int idx, size_t grouping_num)
{
	size_t tk_n = syntax->token_num;
	Token **tks = syntax->tks;
	Token *tk = tks[idx];
	Tokens *expr = new Tokens();
	expr->push_back(tk);
	for (size_t i = 1; i < grouping_num; i++) {
		expr->push_back(tks[idx+i]);
	}
	Token *expr_ = new Token(expr);
	expr_->stype = SyntaxType::Expr;
	tks[idx] = expr_;
	if (tk_n == idx+grouping_num) {
		for (size_t i = 1; i < grouping_num; i++) {
			syntax->tks[idx+i] = NULL;
		}
	} else {
		memmove(syntax->tks+(idx+1), syntax->tks+(idx+grouping_num),
				sizeof(Token *) * (tk_n - (idx+grouping_num)));
		for (size_t i = 1; i < grouping_num; i++) {
			syntax->tks[tk_n-i] = NULL;
		}
	}
	syntax->token_num -= (grouping_num - 1);
}

void Completer::insertTerm(Token *syntax, int idx, size_t grouping_num)
{
	size_t tk_n = syntax->token_num;
	Token **tks = syntax->tks;
	Token *tk = tks[idx];
	Tokens *term = new Tokens();
	term->push_back(tk);
	for (size_t i = 1; i < grouping_num; i++) {
		term->push_back(tks[idx+i]);
	}
	Token *term_ = new Token(term);
	term_->stype = SyntaxType::Term;
	tks[idx] = term_;
	if (tk_n == idx+grouping_num) {
		for (size_t i = 1; i < grouping_num; i++) {
			syntax->tks[idx+i] = NULL;
		}
	} else {
		memmove(syntax->tks+(idx+1), syntax->tks+(idx+grouping_num),
				sizeof(Token *) * (tk_n - (idx+grouping_num)));
		for (size_t i = 1; i < grouping_num; i++) {
			syntax->tks[tk_n-i] = NULL;
		}
	}
	syntax->token_num -= (grouping_num - 1);
}

void Completer::completeExprFromLeft(Token *root, TokenType::Type type)
{
	using namespace TokenType;
	Token **tks = root->tks;
	size_t tk_n = root->token_num;
RESTART:;
	for (size_t i = 0; i < tk_n; i++) {
		if (tk_n > 3 && tk_n > i+2 && tks[i+1]->info.type == type) {
			insertExpr(root, i, 3);
			tk_n -= 2;
			goto RESTART;
		}
		if (tks[i]->token_num > 0) {
			completeExprFromLeft(tks[i], type);
		}
	}
}

void Completer::completeExprFromRight(Token *root, TokenType::Type type)
{
	using namespace TokenType;
	Token **tks = root->tks;
	size_t tk_n = root->token_num;
RESTART:;
	for (int i = tk_n - 1; i >= 0; i--) {
		if (tk_n > 3 && i-2 > 0 && tks[i-1]->info.type == type) {
			insertExpr(root, i - 2, 3);
			tk_n -= 2;
			goto RESTART;
		}
		if (tks[i]->token_num > 0) {
			completeExprFromRight(tks[i], type);
		}
	}
}

void Completer::completeExprFromRight(Token *root, TokenKind::Kind kind)
{
	Token **tks = root->tks;
	size_t tk_n = root->token_num;
RESTART:;
	for (size_t i = tk_n - 1; i > 0; i--) {
		if (tk_n > 3 && i-2 > 0 && tks[i-1]->info.kind == kind) {
			insertExpr(root, i - 2, 3);
			tk_n -= 2;
			goto RESTART;
		}
		if (tks[i]->token_num > 0) {
			completeExprFromRight(tks[i], kind);
		}
	}
}

void Completer::completePointerExpr(Token *root)
{
	insertPointerToken(root);
	completeExprFromLeft(root, TokenType::Pointer);
}

bool Completer::isPointerChain(Token *tk)
{
	using namespace TokenType;
	Type type = tk->info.type;
	SyntaxType::Type stype = tk->stype;
	if (type == GlobalVar || type == Var || type == Method ||
		type == Pointer || type == Namespace || type == SpecificKeyword ||
		(stype == SyntaxType::Expr &&
		 (tk->tks[0]->info.type == LeftBrace ||
		  tk->tks[0]->info.type == LeftBracket ||
		  tk->tks[0]->info.type == ArrayDereference)) ||
		 stype == SyntaxType::Term) {
		return true;
	}
	return false;
}

bool Completer::isArrayOrHashExpr(size_t start_idx, size_t idx, Token *tk, Token *next_tk)
{
	using namespace TokenType;
	if (start_idx != idx) return false;
	if (tk->info.type != Var && tk->info.type != GlobalVar) return false;
	if (next_tk->stype != SyntaxType::Expr) return false;
	Type type = next_tk->tks[0]->info.type;
	if (type == LeftBracket || type == LeftBrace) return true;
	return false;
}

void Completer::insertPointerToken(Token *root)
{
	using namespace TokenType;
	Token **tks = root->tks;
	size_t tk_n = root->token_num;
RESTART:;
	for (size_t i = 0; i < tk_n; i++) {
		Token *tk = tks[i];
		if (isPointerChain(tk)) {
			size_t start_idx = i;
			while (i + 1 < tk_n && isPointerChain(tks[i+1])) {
				Token *next_tk = tks[i+1];
				if (!isArrayOrHashExpr(start_idx, i, tks[i], next_tk) && next_tk->info.type != Pointer) {
					Token *pointer = new Token("->", next_tk->finfo);
					pointer->info.type = TokenType::Pointer;
					pointer->info.name = "Pointer";
					pointer->info.kind = TokenKind::Operator;
					tks = Array_insert(tks, tk_n, i + 1, pointer);
					root->tks = tks;
					tk_n++;
					root->token_num = tk_n;
				}
				i += 2;
			}
		}
		if (i < tk_n && tks[i]->token_num > 0) {
			insertPointerToken(tks[i]);
		}
	}
}

void Completer::completeIncDecGlobExpr(Token *root)
{
	using namespace TokenType;
	Token **tks = root->tks;
	size_t tk_n = root->token_num;
RESTART:;
	for (size_t i = 0; i < tk_n; i++) {
		Token *tk = tks[i];
		Token *next_tk = tks[i+1];
		if (tk_n > 2 && tk_n > i+1 &&
			(tk->info.type == Inc || tk->info.type == Dec) &&
			(next_tk->info.kind == TokenKind::Term || next_tk->stype == SyntaxType::Expr)) {
			insertExpr(root, i, 2);
			tk_n -= 1;
			goto RESTART;
		} else if (tk_n > 2 && i > 0 &&
				   (tk->info.type == Inc || tk->info.type == Dec) &&
				   (tks[i-1]->info.kind == TokenKind::Term || tks[i-1]->stype == SyntaxType::Expr)) {
			insertExpr(root, i-1, 2);
			tk_n -= 1;
			goto RESTART;
		} else if (tk_n > 2 && tk->info.type == Glob && (next_tk->info.type == Key || next_tk->stype == SyntaxType::Expr)) {
			insertExpr(root, i, 2);
			tk_n -= 1;
			goto RESTART;
		} else if (tk_n > 2 && i == 0 && tk->info.type == Mul && next_tk->info.type == Key) {
			tk->info.type = Glob;
			insertExpr(root, i, 2);
			tk_n -= 1;
			goto RESTART;
		}
		if (tks[i]->token_num > 0) {
			completeIncDecGlobExpr(tks[i]);
		}
	}
}

void Completer::completePowerExpr(Token *root)
{
	completeExprFromRight(root, TokenType::Exp);
}

void Completer::completeSingleTermOperatorExpr(Token *root)
{
	// !, ~, \, +, -
	using namespace TokenType;
	Token **tks = root->tks;
	size_t tk_n = root->token_num;
RESTART:;
	for (size_t i = 0; i < tk_n; i++) {
		Token *tk = tks[i];
		TokenType::Type type = tk->info.type;
		if (tk_n > 3 && tk_n > i+2 &&
			(type == IsNot || type == Not || type == Ref || type == BitNot) && tks[i+1]->info.type != CallDecl) {
			insertExpr(root, i, 2);
			tk_n -= 1;
			goto RESTART;
		} else if (tk_n > 2 && tk_n > i+1 && i > 0 &&
				   (type == Add || type == Sub || type == BitAnd || type == PolymorphicCompare || type == ArraySize) &&
				   (tks[i-1]->info.kind != TokenKind::Term &&
					tks[i-1]->stype != SyntaxType::Term &&
					tks[i-1]->stype != SyntaxType::Expr)) {
			insertExpr(root, i, 2);
			tk_n -= 1;
		}
		if (tks[i]->token_num > 0) {
			completeSingleTermOperatorExpr(tks[i]);
		}
	}
}

void Completer::completeRegexpMatchExpr(Token *root)
{
	completeExprFromLeft(root, TokenType::RegOK);
	completeExprFromLeft(root, TokenType::RegNot);
}
void Completer::completeHighPriorityDoubleOperatorExpr(Token *root)
{
	completeExprFromLeft(root, TokenType::Mul);
	completeExprFromLeft(root, TokenType::Div);
	completeExprFromLeft(root, TokenType::Mod);
	completeExprFromLeft(root, TokenType::StringMul);
}

void Completer::completeLowPriorityDoubleOperatorExpr(Token *root)
{
	completeExprFromLeft(root, TokenType::Add);
	completeExprFromLeft(root, TokenType::Sub);
	completeExprFromLeft(root, TokenType::StringAdd);
}

void Completer::completeShiftOperatorExpr(Token *root)
{
	completeExprFromLeft(root, TokenType::LeftShift);
	completeExprFromLeft(root, TokenType::RightShift);
}

void Completer::completeNamedUnaryOperators(Token *root)
{
	//SpecificFunction
	using namespace TokenType;
	Token **tks = root->tks;
	size_t tk_n = root->token_num;
RESTART:;
	for (size_t i = 0; i < tk_n; i++) {
		Token *tk = tks[i];
		if (tk_n > 2 && tk_n > i+1 &&
			((tk->info.type == BuiltinFunc && isUnaryKeyword(tk->data)) ||
			 (tks[0]->info.type != UseDecl && tk->info.type == Namespace)) &&
			(tks[i+1]->stype == SyntaxType::Expr || tks[i+1]->stype == SyntaxType::Term ||
			 tks[i+1]->info.kind == TokenKind::Term)) {
			insertExpr(root, i, 2);
			tk_n -= 1;
			goto RESTART;
		} else if (tk_n > i + 1 && tk_n != 2 &&
				   (tk->info.type == Redo || tk->info.type == Next || tk->info.type == Last) &&
				   tks[i+1]->info.type == Key) {
			insertExpr(root, i, 2);
			tk_n -= 1;
			goto RESTART;
		} else if (tk_n > i + 1 && tk_n != 2 && tk->info.type == Handle && 
				   (tks[i+1]->info.type == Key ||
					tks[i+1]->info.type == Var ||
					tks[i+1]->info.type == GlobalVar)) {
			insertExpr(root, i, 2);
			tk_n -= 1;
			goto RESTART;
		}
		if (tks[i]->token_num > 0) {
			completeNamedUnaryOperators(tks[i]);
		}
	}
	//FileHandle
}

bool Completer::isUnaryKeyword(string target)
{
	bool ret = false;
	vector<string> list = *named_unary_keywords;
	vector<string>::iterator it = find(list.begin(), list.end(), target);
	if (it != list.end()){
		ret = true;
	}
	return ret;
}

void Completer::completeHighPriorityCompareOperatorExpr(Token *root)
{
	completeExprFromLeft(root, TokenType::Greater);
	completeExprFromLeft(root, TokenType::Less);
	completeExprFromLeft(root, TokenType::GreaterEqual);
	completeExprFromLeft(root, TokenType::LessEqual);
	completeExprFromLeft(root, TokenType::StringGreater);
	completeExprFromLeft(root, TokenType::StringLess);
	completeExprFromLeft(root, TokenType::StringGreaterEqual);
	completeExprFromLeft(root, TokenType::StringLessEqual);
}

void Completer::completeLowPriorityCompareOperatorExpr(Token *root)
{
	completeExprFromLeft(root, TokenType::EqualEqual);
	completeExprFromLeft(root, TokenType::NotEqual);
	completeExprFromLeft(root, TokenType::Compare);
	completeExprFromLeft(root, TokenType::StringEqual);
	completeExprFromLeft(root, TokenType::StringNotEqual);
	completeExprFromLeft(root, TokenType::StringCompare);
	//completeExprFromLeft(root, TokenType::PolymorphicCompare);
}

void Completer::completeBitOperatorExpr(Token *root)
{
	completeExprFromLeft(root, TokenType::BitAnd);
	completeExprFromLeft(root, TokenType::BitOr);
	completeExprFromLeft(root, TokenType::BitNot);
}

void Completer::completeThreeTermOperatorExpr(Token *root)
{
	Token **tks = root->tks;
	size_t tk_n = root->token_num;
RESTART:;
	for (size_t i = tk_n - 1; i > 0; i--) {
		if (tk_n > 4 && i > 4 &&
			tks[i-1]->stype != SyntaxType::BlockStmt &&
			tks[i-2]->info.type == TokenType::Colon &&
			tks[i-3]->stype != SyntaxType::BlockStmt &&
			tks[i-4]->info.type == TokenType::ThreeTermOperator &&
			tks[i-5]->stype != SyntaxType::BlockStmt) {
			insertExpr(root, i - 5, 5);
			tk_n -= 4;
			goto RESTART;
		}
		if (tks[i]->token_num > 0) {
			completeThreeTermOperatorExpr(tks[i]);
		}
	}
}

void Completer::completeAndOrOperatorExpr(Token *root)
{
	completeExprFromLeft(root, TokenType::And);
	completeExprFromLeft(root, TokenType::Or);
}

void Completer::completeAssignExpr(Token *root)
{
	completeExprFromRight(root, TokenKind::Assign);
}

void Completer::completeCommaArrowExpr(Token *root)
{
	completeExprFromLeft(root, TokenType::Arrow);
	completeExprFromLeft(root, TokenType::Comma);
}

void Completer::completeFunctionListExpr(Token *root)
{
	using namespace TokenType;
	Token **tks = root->tks;
	size_t tk_n = root->token_num;
RESTART:;
	for (size_t i = 0; i < tk_n; i++) {
		Token *tk = tks[i];
		if (tk_n > 2 && tk_n > i+1 &&
			tk->info.type == TokenType::BuiltinFunc &&
			(tks[i+1]->stype == SyntaxType::Expr ||
			 tks[i+1]->info.kind == TokenKind::Term)) {
			insertExpr(root, i, 2);
			tk_n -= 1;
			goto RESTART;
		}
		if (tks[i]->token_num > 0) {
			completeFunctionListExpr(tks[i]);
		}
	}
}

void Completer::completeAlphabetBitOperatorExpr(Token *root)
{
	completeExprFromRight(root, TokenType::Not);
	completeExprFromLeft(root, TokenType::And);
	completeExprFromLeft(root, TokenType::Or);
	completeExprFromLeft(root, TokenType::XOr);
}

void Completer::templateEvaluatedFromLeft(Token *root, SyntaxCompleter *completer)
{
RESTART:;
	for (size_t i = 0; i < root->token_num; i++) {
		if (completer->complete(root, i)) {
			goto RESTART;
		}
		if (root->tks[i]->token_num > 0) {
			templateEvaluatedFromLeft(root->tks[i], completer);
		}
	}
}

SyntaxCompleter::SyntaxCompleter(void)
{

}

bool SyntaxCompleter::complete(Token *, size_t)
{
	return false;
}

void Completer::completeTerm(Token *root)
{
	TermCompleter completer;
	templateEvaluatedFromLeft(root, &completer);
}

void Completer::recoveryNamedUnaryOperatorsArgument(Token *root)
{
	using namespace TokenType;
	Token **tks = root->tks;
	size_t tk_n = root->token_num;
RESTART:;
	for (size_t i = 0; i < tk_n; i++) {
		Token *tk = tks[i];
		if (tk_n > 2 && tk_n > i+1 &&
			tk->stype == SyntaxType::Expr && isUnaryKeyword(tk->tks[tk->token_num-1]->data) &&
			(tks[i+1]->stype == SyntaxType::Expr || tks[i+1]->stype == SyntaxType::Term)) {
			Token *expr = tks[i+1];
			Token *function_argument = expr;
			Token **new_tks = (Token **)realloc(tk->tks, (tk->token_num + 1) * PTR_SIZE);
			assert(new_tks && "cannot allocate memory");
			tk->tks = new_tks;
			tk->tks[tk->token_num++] = function_argument;
			Tokens *recovery_tks = new Tokens();
			for (size_t j = 0; j < i + 1; j++) {
				recovery_tks->push_back(tks[j]);
			}
			for (size_t j = i + 2; j < tk_n; j++) {
				recovery_tks->push_back(tks[j]);
			}
			size_t size = recovery_tks->size();
			new_tks = (Token **)realloc(tks, size * PTR_SIZE);
			assert(new_tks && "cannot allocate memory");
			tks = new_tks;
			for (size_t j = 0; j < size; j++) {
				tks[j] = recovery_tks->at(j);
			}
			root->token_num = size;
			tk_n = size;
			goto RESTART;
		}
		if (tk->token_num > 0) {
			recoveryNamedUnaryOperatorsArgument(tks[i]);
		}
	}
}

void Completer::recoveryFunctionArgument(Token *root)
{
	using namespace TokenType;
	Token **tks = root->tks;
	size_t tk_n = root->token_num;
RESTART:;
	for (size_t i = 0; i < tk_n; i++) {
		Token *tk = tks[i];
		if (tk_n > 2 && tk_n > i+1 &&
			((tk->stype == SyntaxType::Expr &&
			  (tks[i+1]->stype == SyntaxType::Expr ||
			  tks[i+1]->stype == SyntaxType::Term ||
			  tks[i+1]->info.type == TokenType::ArrayVar)) ||
			 (tk->stype == SyntaxType::Term &&
			  (tks[i+1]->stype == SyntaxType::Term ||
			   tks[i+1]->info.type == TokenType::ArrayVar)))) {
			Token *expr = tks[i+1];
			Token *function_argument = expr;
			Token **new_tks = (Token **)realloc(tk->tks, (tk->token_num + 1) * PTR_SIZE);
			assert(new_tks && "cannot allocate memory");
			tk->tks = new_tks;
			tk->tks[tk->token_num++] = function_argument;
			Tokens *recovery_tks = new Tokens();
			for (size_t j = 0; j < i + 1; j++) {
				recovery_tks->push_back(tks[j]);
			}
			for (size_t j = i + 2; j < tk_n; j++) {
				recovery_tks->push_back(tks[j]);
			}
			size_t size = recovery_tks->size();
			new_tks = (Token **)realloc(tks, size * PTR_SIZE);
			assert(new_tks && "cannot allocate memory");
			tks = new_tks;
			for (size_t j = 0; j < size; j++) {
				tks[j] = recovery_tks->at(j);
			}
			root->token_num = size;
			tk_n = size;
			goto RESTART;
		}
		if (tk->token_num > 0) {
			recoveryFunctionArgument(tks[i]);
		}
	}
}
