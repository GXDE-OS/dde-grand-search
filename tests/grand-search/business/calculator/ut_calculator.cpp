// SPDX-FileCopyrightText: 2021 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "business/calculator/calculator.h"
#include "global/builtinsearch.h"

#include <gtest/gtest.h>

using namespace GrandSearch;

namespace {

QString valueOf(const QString &text)
{
    return Calculator::evaluate(text).value;
}

} // namespace

TEST(Calculator, basicArithmetic)
{
    EXPECT_EQ(valueOf(QStringLiteral("1+1")), QStringLiteral("2"));
    EXPECT_EQ(valueOf(QStringLiteral("1+2*3")), QStringLiteral("7"));
    EXPECT_EQ(valueOf(QStringLiteral("(1+2)*3")), QStringLiteral("9"));
    EXPECT_EQ(valueOf(QStringLiteral("2^10")), QStringLiteral("1024"));
    EXPECT_EQ(valueOf(QStringLiteral("2**8")), QStringLiteral("256"));
    EXPECT_EQ(valueOf(QStringLiteral("10/4")), QStringLiteral("2.5"));
    EXPECT_EQ(valueOf(QStringLiteral("0.1+0.2")), QStringLiteral("0.3"));
    EXPECT_EQ(valueOf(QStringLiteral("-2^2")), QStringLiteral("-4"));
    EXPECT_EQ(valueOf(QStringLiteral("2^-2")), QStringLiteral("0.25"));
}

TEST(Calculator, functionsAndConstants)
{
    EXPECT_EQ(valueOf(QStringLiteral("sqrt(16)")), QStringLiteral("4"));
    EXPECT_EQ(valueOf(QStringLiteral("pow(2,8)")), QStringLiteral("256"));
    EXPECT_EQ(valueOf(QStringLiteral("max(3,7)+1")), QStringLiteral("8"));
    EXPECT_EQ(valueOf(QStringLiteral("abs(-3.5)")), QStringLiteral("3.5"));
    EXPECT_EQ(valueOf(QStringLiteral("5!")), QStringLiteral("120"));
    EXPECT_EQ(valueOf(QStringLiteral("100*20%")), QStringLiteral("20"));
    EXPECT_EQ(valueOf(QStringLiteral("10%3")), QStringLiteral("1"));
    EXPECT_EQ(valueOf(QStringLiteral("0xff+1")), QStringLiteral("256"));
    EXPECT_EQ(valueOf(QStringLiteral("0b1010*2")), QStringLiteral("20"));
    EXPECT_EQ(valueOf(QStringLiteral("1e3+2")), QStringLiteral("1002"));
}

TEST(Calculator, localizedSymbolsAndInputHabits)
{
    EXPECT_EQ(valueOf(QStringLiteral("1＋2×3")), QStringLiteral("7"));
    EXPECT_EQ(valueOf(QStringLiteral("（1+2）÷2")), QStringLiteral("1.5"));
    EXPECT_EQ(valueOf(QString::fromUtf8("√9")), QStringLiteral("3"));
    EXPECT_EQ(valueOf(QStringLiteral("1+1=")), QStringLiteral("2"));
    EXPECT_EQ(valueOf(QStringLiteral("=1+1")), QStringLiteral("2"));
    EXPECT_EQ(valueOf(QStringLiteral("50%")), QStringLiteral("0.5"));
}

TEST(Calculator, incompleteAndInvalidExpressions)
{
    // 表达式尚未输入完整时不打扰用户，不展示任何内容
    const auto incomplete = Calculator::evaluate(QStringLiteral("1+"));
    EXPECT_TRUE(incomplete.isExpression);
    EXPECT_FALSE(incomplete.hasValue);
    EXPECT_TRUE(incomplete.message.isEmpty());
    EXPECT_TRUE(Calculator::matchedItems(incomplete).isEmpty());

    // 除零 / 域错误给出提示
    const auto divZero = Calculator::evaluate(QStringLiteral("1/0"));
    EXPECT_FALSE(divZero.hasValue);
    EXPECT_FALSE(divZero.message.isEmpty());
    EXPECT_FALSE(Calculator::matchedItems(divZero).isEmpty());

    const auto badSyntax = Calculator::evaluate(QStringLiteral("1+2abc"));
    EXPECT_FALSE(badSyntax.hasValue);
    EXPECT_TRUE(badSyntax.message.isEmpty());
}

TEST(Calculator, isExpressionRejectsOrdinarySearchWords)
{
    const QStringList ordinary {
        QStringLiteral("hello"), QStringLiteral("3.14"), QStringLiteral("123"),
        QStringLiteral("c++"), QStringLiteral("2026-11-05"),
        QStringLiteral("192.168.1.1"), QStringLiteral("1.2.3"),
        QStringLiteral("report-2026.docx")
    };
    for (const QString &text : ordinary)
        EXPECT_FALSE(Calculator::isExpression(text)) << text.toStdString();

    EXPECT_TRUE(Calculator::isExpression(QStringLiteral("1+1")));
    EXPECT_TRUE(Calculator::isExpression(QStringLiteral("sqrt(2)")));
    EXPECT_TRUE(Calculator::isExpression(QStringLiteral("5!")));
}

TEST(Calculator, matchedItems)
{
    const auto result = Calculator::evaluate(QStringLiteral("1+2*3"));
    const MatchedItemMap map = Calculator::matchedItems(result);

    ASSERT_EQ(map.size(), 1);
    ASSERT_TRUE(map.contains(GRANDSEARCH_GROUP_CALCULATOR));

    const MatchedItems &items = map.value(GRANDSEARCH_GROUP_CALCULATOR);
    ASSERT_EQ(items.size(), 4);   // 主结果 + 十六进制/二进制/八进制

    const MatchedItem &main = items.first();
    EXPECT_EQ(main.name, QStringLiteral("1+2*3 = 7"));
    EXPECT_EQ(main.icon, QStringLiteral("accessories-calculator"));
    EXPECT_EQ(main.searcher, QString(GRANDSEARCH_CLASS_CALCULATOR));
    EXPECT_FALSE(main.item.isEmpty());   // GroupWidget 会丢弃 item 为空的结果
    EXPECT_EQ(main.extra.toHash().value(GRANDSEARCH_PROPERTY_CALC_VALUE).toString(),
              QStringLiteral("7"));

    // 其余结果项拥有各自独立的 item 标识，避免被去重
    for (int i = 1; i < items.size(); ++i)
        EXPECT_NE(items.at(i).item, items.at(i - 1).item);

    EXPECT_TRUE(Calculator::matchedItems(Calculator::evaluate(QStringLiteral("hello"))).isEmpty());
}
