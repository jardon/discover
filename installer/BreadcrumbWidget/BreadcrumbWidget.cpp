/***************************************************************************
 *   Copyright © 2010 Jonathan Thomas <echidnaman@kubuntu.org>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "BreadcrumbWidget.h"

#include "BreadcrumbItem.h"

BreadcrumbWidget::BreadcrumbWidget(QWidget *parent)
    : KHBox(parent)
{
    m_items.clear();
    m_breadcrumbArea = new KHBox(this);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

BreadcrumbWidget::~BreadcrumbWidget()
{
}

void BreadcrumbWidget::setRootItem(BreadcrumbItem *root)
{
    clearCrumbs();
    addLevel(root);
}

void BreadcrumbWidget::clearCrumbs()
{
    if (!m_items.isEmpty()) {
      foreach(BreadcrumbItem *item, m_items) {
          item->hide();
          item->deleteLater();
      }
    }

    m_items.clear();
}

void BreadcrumbWidget::addLevel(BreadcrumbItem *item)
{
    if (!m_items.isEmpty()) {
        m_items.last()->setChildItem(item);
    }

    item->setParent(m_breadcrumbArea);
    item->show();

    m_items.append(item);
    connect(item, SIGNAL(activated(BreadcrumbItem *)), this, SIGNAL(itemActivated(BreadcrumbItem *)));
}

void BreadcrumbWidget::removeItem(BreadcrumbItem *item)
{
    // Recursion ftw
    if (item->hasChildren()){
        removeItem(item->childItem());
    }

    item->hide();
    item->associatedWidget()->deleteLater();
    item->deleteLater();
    m_items.removeLast();
}

#include "BreadcrumbWidget.moc"
