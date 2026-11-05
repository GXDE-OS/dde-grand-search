// SPDX-FileCopyrightText: 2021 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "calculator.h"

#include "global/builtinsearch.h"

#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVariantHash>

#include <cmath>
#include <limits>

namespace GrandSearch {
namespace {

constexpr int MaxExpressionLength = 128;
constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;

// 把中文输入法常见的全角/数学符号统一成 ASCII，并规范化表达式。
// 返回空串表示该文本不可能是一个公式。
QString normalize(const QString &text)
{
    QString s = text.trimmed();
    if (s.isEmpty() || s.size() > MaxExpressionLength)
        return QString();

    s.replace(QChar(0x03C0), QStringLiteral("pi"));     // π

    for (int i = 0; i < s.size(); ++i) {
        switch (s.at(i).unicode()) {
        case 0xFF0B: s[i] = QLatin1Char('+'); break;             // ＋
        case 0xFF0D: case 0x2212: s[i] = QLatin1Char('-'); break; // － −
        case 0xFF0A: case 0x00D7: s[i] = QLatin1Char('*'); break; // ＊ ×
        case 0xFF0F: case 0x00F7: s[i] = QLatin1Char('/'); break; // ／ ÷
        case 0xFF05: s[i] = QLatin1Char('%'); break;             // ％
        case 0xFF3E: s[i] = QLatin1Char('^'); break;             // ＾
        case 0xFF08: s[i] = QLatin1Char('('); break;             // （
        case 0xFF09: s[i] = QLatin1Char(')'); break;             // ）
        case 0xFF0C: s[i] = QLatin1Char(','); break;             // ，
        case 0xFF0E: s[i] = QLatin1Char('.'); break;             // ．
        case 0xFF1D: s[i] = QLatin1Char('='); break;             // ＝
        case 0xFF01: s[i] = QLatin1Char('!'); break;             // ！
        default: break;
        }
    }

    // 去掉所有空白（包括全角空格）
    for (int i = s.size() - 1; i >= 0; --i) {
        if (s.at(i).isSpace())
            s.remove(i, 1);
    }

    // √9 / √(1+2) / √pi 统一改写为 sqrt(...)，避免出现 "sqrt9" 这种无法解析的形式
    static const QRegularExpression rootRe(
        QString::fromUtf8("√(\\d+(?:\\.\\d+)?|\\([^()]*\\)|[a-zA-Z]+)"));
    s.replace(rootRe, QStringLiteral("sqrt(\\1)"));

    // 兼容常见的输入习惯：=1+1、1+1=；其它位置的 '=' 视为普通搜索文本
    if (s.startsWith(QLatin1Char('=')))
        s.remove(0, 1);
    while (s.endsWith(QLatin1Char('=')))
        s.chop(1);
    if (s.contains(QLatin1Char('=')))
        return QString();

    s.replace(QLatin1String("**"), QLatin1String("^"));

    // 后缀百分号：20% / 100*20% 视作 0.2 / 20；10%3 仍是取模
    static const QRegularExpression percentRe(
        QString::fromUtf8(R"((\d+(?:\.\d+)?)%(?![\d.]))"));
    s.replace(percentRe, QStringLiteral("(\\1/100)"));

    return s.toLower();
}

enum class TokenType { Number, Identifier, Operator, LParen, RParen, Comma, Factorial, End };

struct Token
{
    TokenType type = TokenType::End;
    double number = 0.0;
    QString text;
    QChar op;
};

QList<Token> tokenize(const QString &s, bool &ok)
{
    QList<Token> tokens;
    ok = true;

    static const QRegularExpression numberRe(QString::fromUtf8(
        R"(^(?:0x[0-9a-f]+|0b[01]+|0o[0-7]+|(?:\d+\.?\d*|\.\d+)(?:e[+-]?\d+)?))"));

    int i = 0;
    while (i < s.size()) {
        const QChar c = s.at(i);

        if (c.isDigit() || c == QLatin1Char('.')) {
            const QRegularExpressionMatch match = numberRe.match(s.mid(i));
            if (!match.hasMatch()) {
                ok = false;
                return tokens;
            }

            const QString literal = match.captured(0);
            Token token;
            token.type = TokenType::Number;
            bool converted = true;
            if (literal.startsWith(QLatin1String("0x")))
                token.number = static_cast<double>(literal.mid(2).toULongLong(&converted, 16));
            else if (literal.startsWith(QLatin1String("0b")))
                token.number = static_cast<double>(literal.mid(2).toULongLong(&converted, 2));
            else if (literal.startsWith(QLatin1String("0o")))
                token.number = static_cast<double>(literal.mid(2).toULongLong(&converted, 8));
            else
                token.number = literal.toDouble(&converted);

            if (!converted) {
                ok = false;
                return tokens;
            }
            tokens.append(token);
            i += literal.size();
            continue;
        }

        if (c.isLetter() || c == QLatin1Char('_')) {
            int end = i;
            while (end < s.size() && (s.at(end).isLetterOrNumber() || s.at(end) == QLatin1Char('_')))
                ++end;
            Token token;
            token.type = TokenType::Identifier;
            token.text = s.mid(i, end - i);
            tokens.append(token);
            i = end;
            continue;
        }

        Token token;
        switch (c.unicode()) {
        case '(': token.type = TokenType::LParen; break;
        case ')': token.type = TokenType::RParen; break;
        case ',': token.type = TokenType::Comma; break;
        case '!': token.type = TokenType::Factorial; break;
        case '+': case '-': case '*': case '/': case '%': case '^':
        case '&': case '|': case '~':
            token.type = TokenType::Operator;
            token.op = c;
            break;
        default:
            ok = false;
            return tokens;
        }
        tokens.append(token);
        ++i;
    }

    Token end;
    end.type = TokenType::End;
    tokens.append(end);
    return tokens;
}

// 递归下降求值。仅处理上面 tokenize 产生的固定语法，不执行任何输入代码。
class Evaluator
{
public:
    explicit Evaluator(const QString &expr)
        : m_tokens(tokenize(expr, m_tokenOk))
    {
    }

    bool evaluate(double &out)
    {
        if (!m_tokenOk)
            return false;

        const double value = parseBitOr();
        if (!m_ok || current().type != TokenType::End)
            return false;

        out = value;
        return true;
    }

private:
    const Token &current() const { return m_tokens.at(qMin(m_pos, m_tokens.size() - 1)); }
    void advance() { if (m_pos < m_tokens.size() - 1) ++m_pos; }

    bool takeOperator(QChar op)
    {
        if (current().type == TokenType::Operator && current().op == op) {
            advance();
            return true;
        }
        return false;
    }

    bool takeType(TokenType type)
    {
        if (current().type == type) {
            advance();
            return true;
        }
        return false;
    }

    double parseBitOr()
    {
        double value = parseBitAnd();
        while (m_ok && takeOperator(QLatin1Char('|'))) {
            const double right = parseBitAnd();
            value = static_cast<double>(static_cast<qint64>(value) | static_cast<qint64>(right));
        }
        return value;
    }

    double parseBitAnd()
    {
        double value = parseAdd();
        while (m_ok && takeOperator(QLatin1Char('&'))) {
            const double right = parseAdd();
            value = static_cast<double>(static_cast<qint64>(value) & static_cast<qint64>(right));
        }
        return value;
    }

    double parseAdd()
    {
        double value = parseMultiply();
        while (m_ok) {
            if (takeOperator(QLatin1Char('+')))
                value += parseMultiply();
            else if (takeOperator(QLatin1Char('-')))
                value -= parseMultiply();
            else
                break;
        }
        return value;
    }

    double parseMultiply()
    {
        double value = parseUnary();
        while (m_ok) {
            if (takeOperator(QLatin1Char('*')))
                value *= parseUnary();
            else if (takeOperator(QLatin1Char('/')))
                value /= parseUnary();
            else if (takeOperator(QLatin1Char('%'))) {
                const double right = parseUnary();
                value = std::fmod(value, right);
            } else {
                break;
            }
        }
        return value;
    }

    double parseUnary()
    {
        if (takeOperator(QLatin1Char('+')))
            return parseUnary();
        if (takeOperator(QLatin1Char('-')))
            return -parseUnary();
        if (takeOperator(QLatin1Char('~')))
            return static_cast<double>(~static_cast<qint64>(parseUnary()));
        return parsePower();
    }

    double parsePower()
    {
        double value = parsePostfix();
        // 幂运算右结合，且指数允许带一元符号（2^-2）
        if (m_ok && takeOperator(QLatin1Char('^')))
            value = std::pow(value, parseUnary());
        return value;
    }

    double parsePostfix()
    {
        double value = parsePrimary();
        while (m_ok && current().type == TokenType::Factorial) {
            advance();
            value = factorial(value);
        }
        return value;
    }

    double parsePrimary()
    {
        switch (current().type) {
        case TokenType::Number: {
            const double value = current().number;
            advance();
            return value;
        }
        case TokenType::LParen: {
            advance();
            const double value = parseBitOr();
            if (!takeType(TokenType::RParen))
                m_ok = false;
            return value;
        }
        case TokenType::Identifier: {
            const QString name = current().text;
            advance();
            if (current().type == TokenType::LParen) {
                advance();
                QList<double> args;
                if (current().type != TokenType::RParen) {
                    args.append(parseBitOr());
                    while (m_ok && current().type == TokenType::Comma) {
                        advance();
                        args.append(parseBitOr());
                    }
                }
                if (!takeType(TokenType::RParen))
                    m_ok = false;
                return applyFunction(name, args);
            }
            return constant(name);
        }
        default:
            m_ok = false;
            return 0.0;
        }
    }

    double factorial(double value)
    {
        if (value < 0 || value > 170 || std::floor(value) != value) {
            m_ok = false;
            return 0.0;
        }
        double result = 1.0;
        for (int i = 2; i <= static_cast<int>(value); ++i)
            result *= i;
        return result;
    }

    double constant(const QString &name)
    {
        if (name == QLatin1String("pi")) return kPi;
        if (name == QLatin1String("e")) return kE;
        if (name == QLatin1String("tau")) return 2 * kPi;
        if (name == QLatin1String("phi")) return 1.61803398874989484820;
        m_ok = false;
        return 0.0;
    }

    double applyFunction(const QString &name, const QList<double> &args)
    {
        const double x = args.value(0, 0.0);
        if (args.size() == 1) {
            if (name == QLatin1String("sqrt")) return std::sqrt(x);
            if (name == QLatin1String("cbrt")) return std::cbrt(x);
            if (name == QLatin1String("abs") || name == QLatin1String("fabs")) return std::fabs(x);
            if (name == QLatin1String("floor")) return std::floor(x);
            if (name == QLatin1String("ceil")) return std::ceil(x);
            if (name == QLatin1String("round")) return std::round(x);
            if (name == QLatin1String("trunc")) return std::trunc(x);
            if (name == QLatin1String("exp")) return std::exp(x);
            if (name == QLatin1String("ln")) return std::log(x);
            if (name == QLatin1String("log") || name == QLatin1String("log10")) return std::log10(x);
            if (name == QLatin1String("log2")) return std::log2(x);
            if (name == QLatin1String("sin")) return std::sin(x);
            if (name == QLatin1String("cos")) return std::cos(x);
            if (name == QLatin1String("tan")) return std::tan(x);
            if (name == QLatin1String("asin")) return std::asin(x);
            if (name == QLatin1String("acos")) return std::acos(x);
            if (name == QLatin1String("atan")) return std::atan(x);
            if (name == QLatin1String("sinh")) return std::sinh(x);
            if (name == QLatin1String("cosh")) return std::cosh(x);
            if (name == QLatin1String("tanh")) return std::tanh(x);
            if (name == QLatin1String("deg")) return x * 180.0 / kPi;
            if (name == QLatin1String("rad")) return x * kPi / 180.0;
        } else if (args.size() == 2) {
            const double y = args.at(1);
            if (name == QLatin1String("pow")) return std::pow(x, y);
            if (name == QLatin1String("min")) return qMin(x, y);
            if (name == QLatin1String("max")) return qMax(x, y);
            if (name == QLatin1String("mod")) return std::fmod(x, y);
            if (name == QLatin1String("root")) return std::pow(y, 1.0 / x);
            if (name == QLatin1String("atan2")) return std::atan2(x, y);
        }
        m_ok = false;
        return 0.0;
    }

    QList<Token> m_tokens;
    int m_pos = 0;
    bool m_tokenOk = true;
    bool m_ok = true;
};

QString formatNumber(double value)
{
    if (value == 0.0)
        return QStringLiteral("0");

    const double absValue = std::fabs(value);
    if (absValue < 1e15 && std::fabs(value - std::round(value)) < 1e-9)
        return QString::number(static_cast<qint64>(std::round(value)));

    // 'g' 精度会吸收浮点误差（0.1+0.2 -> 0.3），同时去掉尾部多余的 0
    return QString::number(value, 'g', 12);
}

QList<QPair<QString, QString>> buildDetails(double value)
{
    QList<QPair<QString, QString>> details;
    const double absValue = std::fabs(value);
    const bool isInteger = absValue < 9.0e18 && std::fabs(value - std::round(value)) < 1e-9;

    if (isInteger) {
        const qint64 integer = static_cast<qint64>(std::round(value));
        // INT64_MIN 取绝对值会溢出，直接跳过附加结果
        if (integer == std::numeric_limits<qint64>::min())
            return details;

        const qint64 magnitude = integer < 0 ? -integer : integer;
        const QString sign = integer < 0 ? QStringLiteral("-") : QString();
        const QString hex = sign + QStringLiteral("0x") + QString::number(magnitude, 16);
        const QString bin = sign + QStringLiteral("0b") + QString::number(magnitude, 2);
        const QString oct = sign + QStringLiteral("0o") + QString::number(magnitude, 8);
        details.append(qMakePair(QStringLiteral("HEX: ") + hex, hex));
        details.append(qMakePair(QStringLiteral("BIN: ") + bin, bin));
        details.append(qMakePair(QStringLiteral("OCT: ") + oct, oct));
    } else {
        const QString scientific = QString::number(value, 'e', 6);
        const QString percent = formatNumber(value * 100.0) + QLatin1Char('%');
        details.append(qMakePair(QStringLiteral("SCI: ") + scientific, scientific));
        details.append(qMakePair(QStringLiteral("PERCENT: ") + percent, percent));
    }

    return details;
}

MatchedItem makeCalculatorItem(const QString &key, const QString &name,
                               const QString &value, int weight, bool withHint)
{
    MatchedItem item;
    // GroupWidget 按 item 去重，计算器结果必须使用非空的唯一标识
    item.item = QStringLiteral("grandsearch://calculator/%1").arg(key);
    item.name = name;
    item.icon = QStringLiteral("accessories-calculator");
    item.type = QStringLiteral("application/x-dde-grand-search-calculator");
    item.searcher = GRANDSEARCH_CLASS_CALCULATOR;

    QVariantHash extra;
    // 带权重时 GroupWidget 保持传入顺序，主结果始终排在第一位
    extra.insert(GRANDSEARCH_PROPERTY_ITEM_WEIGHT, weight);
    extra.insert(GRANDSEARCH_PROPERTY_CALC_VALUE, value);
    if (withHint)
        extra.insert(GRANDSEARCH_PROPERTY_ITEM_TAILER,
                     QStringList{ QObject::tr("Copy the result") });
    item.extra = extra;
    return item;
}

} // namespace

bool Calculator::isExpression(const QString &text)
{
    const QString trimmed = text.trimmed();
    const QString expr = normalize(text);
    if (expr.isEmpty())
        return false;

    // 日期、版本号、IP、电话号码等含 '-' 或 '.' 的普通搜索词不当作公式
    static const QVector<QRegularExpression> nonFormulaPatterns {
        QRegularExpression(QString::fromUtf8(R"(^\d{4}[-/.]\d{1,2}([-/.]\d{1,2})?$)")),
        QRegularExpression(QString::fromUtf8(R"(^\d{1,2}[-/.]\d{1,2}[-/.]\d{2,4}$)")),
        QRegularExpression(QString::fromUtf8(R"(^\d+(\.\d+){2,}$)")),
        QRegularExpression(QString::fromUtf8(R"(^\d{3,4}-\d{3,4}-\d{3,4}$)"))
    };
    for (const QRegularExpression &pattern : nonFormulaPatterns) {
        if (pattern.match(trimmed).hasMatch())
            return false;
    }

    static const QRegularExpression allowed(
        QString::fromUtf8(R"(^[0-9a-z+\-*/%^!&|~(),.]+$)"));
    if (!allowed.match(expr).hasMatch())
        return false;

    bool hasDigit = false;
    bool hasOperator = false;
    for (const QChar c : expr) {
        if (c.isDigit())
            hasDigit = true;
        if (QStringLiteral("+-*/%^!&|~").contains(c))
            hasOperator = true;
    }
    if (!hasDigit)
        return false;

    // 只允许已知的函数名与常量，避免把 "report-2026.docx" 之类的文件名当成公式
    static const QRegularExpression numberLiteral(QString::fromUtf8(
        R"((?:0x[0-9a-f]+|0b[01]+|0o[0-7]+|(?:\d+\.?\d*|\.\d+)(?:e[+-]?\d+)?))"));
    static const QRegularExpression identifier(QStringLiteral("[a-z]+"));
    static const QSet<QString> knownNames {
        QStringLiteral("pi"), QStringLiteral("e"), QStringLiteral("tau"), QStringLiteral("phi"),
        QStringLiteral("sqrt"), QStringLiteral("cbrt"), QStringLiteral("abs"), QStringLiteral("fabs"),
        QStringLiteral("floor"), QStringLiteral("ceil"), QStringLiteral("round"), QStringLiteral("trunc"),
        QStringLiteral("exp"), QStringLiteral("ln"), QStringLiteral("log"), QStringLiteral("log10"),
        QStringLiteral("log2"), QStringLiteral("sin"), QStringLiteral("cos"), QStringLiteral("tan"),
        QStringLiteral("asin"), QStringLiteral("acos"), QStringLiteral("atan"), QStringLiteral("sinh"),
        QStringLiteral("cosh"), QStringLiteral("tanh"), QStringLiteral("deg"), QStringLiteral("rad"),
        QStringLiteral("pow"), QStringLiteral("min"), QStringLiteral("max"), QStringLiteral("mod"),
        QStringLiteral("root"), QStringLiteral("atan2")
    };

    QString remainder = expr;
    remainder.remove(numberLiteral);
    auto match = identifier.globalMatch(remainder);
    while (match.hasNext()) {
        if (!knownNames.contains(match.next().captured()))
            return false;
    }

    static const QRegularExpression functionCall(QStringLiteral("[a-z]+\\("));
    return hasOperator || expr.contains(functionCall);
}

Calculator::Result Calculator::evaluate(const QString &text)
{
    Result result;
    result.expression = text.trimmed();
    result.isExpression = isExpression(text);
    if (!result.isExpression)
        return result;

    const QString expr = normalize(text);
    Evaluator evaluator(expr);
    double value = 0.0;
    if (!evaluator.evaluate(value)) {
        // 表达式还没输入完整（或语法不合法）时不打扰用户，继续当作普通搜索词
        return result;
    }
    if (!std::isfinite(value)) {
        result.message = QObject::tr("Cannot compute");
        return result;
    }

    result.value = formatNumber(value);
    result.hasValue = true;
    result.display = QStringLiteral("%1 = %2").arg(result.expression, result.value);
    result.details = buildDetails(value);
    return result;
}

MatchedItemMap Calculator::matchedItems(const Result &result)
{
    MatchedItemMap map;
    if (!result.isExpression)
        return map;

    MatchedItems items;
    if (result.hasValue) {
        items.append(makeCalculatorItem(QStringLiteral("result"), result.display,
                                        result.value, 1000, true));
        int weight = 900;
        for (auto it = result.details.cbegin(); it != result.details.cend(); ++it) {
            items.append(makeCalculatorItem(QStringLiteral("detail%1").arg(weight),
                                            it->first, it->second, weight, false));
            weight -= 10;
        }
    } else if (!result.message.isEmpty()) {
        items.append(makeCalculatorItem(QStringLiteral("error"), result.message,
                                        QString(), 1000, false));
    }

    if (!items.isEmpty())
        map.insert(GRANDSEARCH_GROUP_CALCULATOR, items);
    return map;
}

} // namespace GrandSearch
