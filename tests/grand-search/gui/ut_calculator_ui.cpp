// SPDX-FileCopyrightText: 2021 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "business/calculator/calculator.h"
#include "global/builtinsearch.h"

#define private public
#define protected public
#include "gui/entrance/entrancewidget.h"
#include "gui/entrance/entrancewidget_p.h"
#include "gui/entrance/searchedit.h"
#include "gui/entrance/searchedit_p.h"
#include "gui/exhibition/exhibitionwidget.h"
#include "gui/exhibition/matchresult/listview/grandsearchlistview.h"
#include "gui/exhibition/matchresult/matchwidget.h"
#include "gui/mainwindow.h"
#include "gui/mainwindow_p.h"
#undef private
#undef protected

#include <QApplication>
#include <gtest/gtest.h>

using namespace GrandSearch;

TEST(CalculatorUI, inlineResultLabel)
{
    SearchEdit edit;
    edit.resize(740, 48);
    edit.show();
    qApp->processEvents();

    ASSERT_TRUE(edit.d->m_calcResultLabel);
    EXPECT_TRUE(edit.d->m_calcResultLabel->isHidden());

    edit.setCalculatorResult(QStringLiteral("2"));
    EXPECT_FALSE(edit.d->m_calcResultLabel->isHidden());
    EXPECT_EQ(edit.d->m_calcResultLabel->text(), QStringLiteral("= 2"));
    EXPECT_GT(edit.lineEdit()->textMargins().right(), 0);

    edit.setCalculatorResult(QString());
    EXPECT_TRUE(edit.d->m_calcResultLabel->isHidden());
    EXPECT_EQ(edit.lineEdit()->textMargins().right(), 0);
}

TEST(CalculatorUI, calculatorGroupIsAtTopAndReplaced)
{
    MatchWidget widget;
    widget.resize(740, 400);
    widget.show();
    qApp->processEvents();

    const auto result = Calculator::evaluate(QStringLiteral("1+1"));
    widget.setCalculatorItems(Calculator::matchedItems(result));

    GroupWidget *group = widget.m_groupWidgetMap.value(GRANDSEARCH_GROUP_CALCULATOR, nullptr);
    ASSERT_TRUE(group);
    EXPECT_FALSE(group->isHidden());
    EXPECT_EQ(group->itemCount(), 4);
    ASSERT_FALSE(widget.m_vGroupWidgets.isEmpty());
    EXPECT_EQ(widget.m_vGroupWidgets.first(), group);
    EXPECT_EQ(group->getListView()->model()->index(0, 0).data(DATA_ROLE)
                  .value<MatchedItem>().name,
              QStringLiteral("1+1 = 2"));

    // 表达式变化时旧结果不会残留
    widget.setCalculatorItems(Calculator::matchedItems(
        Calculator::evaluate(QStringLiteral("2+3"))));
    EXPECT_EQ(group->itemCount(), 4);
    EXPECT_EQ(group->getListView()->model()->index(0, 0).data(DATA_ROLE)
                  .value<MatchedItem>().name,
              QStringLiteral("2+3 = 5"));

    widget.setCalculatorItems(MatchedItemMap());
    EXPECT_TRUE(group->isHidden());
    EXPECT_EQ(group->itemCount(), 0);
}

TEST(CalculatorUI, mainWindowUpdatesResultOnEveryInput)
{
    MainWindow window;
    window.resize(760, 520);
    qApp->processEvents();

    auto *matchWidget = window.d_p->m_exhibitionWidget->m_matchWidget;
    auto *resultLabel = window.d_p->m_entranceWidget->d_p->m_searchEdit->d->m_calcResultLabel;
    ASSERT_TRUE(matchWidget);
    ASSERT_TRUE(resultLabel);

    window.onSearchTextChanged(QStringLiteral("1+2*3"));
    EXPECT_EQ(window.d_p->m_calcResult.value, QStringLiteral("7"));
    EXPECT_EQ(resultLabel->text(), QStringLiteral("= 7"));
    EXPECT_EQ(matchWidget->m_groupWidgetMap.value(GRANDSEARCH_GROUP_CALCULATOR)->itemCount(), 4);

    // 搜索任务重置（clearData）后，结果应自动重新注入
    window.onMissionChanged(QStringLiteral("mission"), QStringLiteral("1+2*3"));
    EXPECT_EQ(matchWidget->m_groupWidgetMap.value(GRANDSEARCH_GROUP_CALCULATOR)->itemCount(), 4);

    // 输入不再是表达式时，内联结果与分组都要消失
    window.onSearchTextChanged(QStringLiteral("hello"));
    EXPECT_TRUE(resultLabel->isHidden());
    EXPECT_TRUE(matchWidget->m_groupWidgetMap.value(GRANDSEARCH_GROUP_CALCULATOR)->isHidden());
}
