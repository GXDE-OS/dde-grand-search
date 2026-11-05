// SPDX-FileCopyrightText: 2021 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CALCULATOR_H
#define CALCULATOR_H

#include "global/matcheditem.h"

#include <QList>
#include <QPair>
#include <QString>

namespace GrandSearch {

/**
 * @brief 搜索框内置计算器
 *
 * 表达式在界面进程内识别并求值，不经过 daemon/D-Bus，也不依赖任何外部求值引擎，
 * 因此用户每输入一个字符都能立即看到结果。求值使用自实现的递归下降解析器，
 * 不会执行输入内容，也不使用 QScriptEngine / eval 之类的动态执行接口。
 */
class Calculator
{
public:
    struct Result
    {
        bool isExpression = false;      // 输入是否看起来像一个计算公式
        bool hasValue = false;          // 是否成功求出结果
        QString expression;             // 用户原始输入（去掉首尾空白）
        QString value;                  // 可复制的结果数值
        QString display;                // 结果列表中显示的主文本
        QString message;                // 无法计算时的提示
        QList<QPair<QString, QString>> details;    // 附加结果：(显示文本, 可复制值)
    };

    // 判断文本是否为一个计算公式（可能尚未输入完整）
    static bool isExpression(const QString &text);

    // 求值。非表达式、语法不完整时 hasValue 为 false，界面不展示任何计算器内容
    static Result evaluate(const QString &text);

    // 转换成可直接追加到结果列表的分组数据
    static MatchedItemMap matchedItems(const Result &result);
};

}

#endif // CALCULATOR_H
