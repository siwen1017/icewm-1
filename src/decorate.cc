/*
 * IceWM
 *
 * Copyright (C) 1997-2002 Marko Macek
 */
#include "config.h"
#include "wmframe.h"

#include "wmtitle.h"
#include "wmapp.h"
#include "wmcontainer.h"
#include "wpixmaps.h"
#include "ymenuitem.h"
#include "yrect.h"
#include "prefs.h"

void YFrameWindow::updateMenu() {
    YMenu *windowMenu = this->windowMenu();
    // enable all commands
    windowMenu->setActionListener(this);
    windowMenu->enableCommand(actionNull);

    if (isMaximized())
        windowMenu->disableCommand(actionMaximize);
    if (isMinimized())
        windowMenu->disableCommand(actionMinimize);
    if (!(isMaximized() || isMinimized() || isHidden()))
        windowMenu->disableCommand(actionRestore);
    if (isMinimized() || isHidden() || isRollup() || !visibleNow())
        windowMenu->disableCommand(actionSize);
    if (isMinimized() || isHidden() || !visibleNow())
        windowMenu->disableCommand(actionMove);
    if (!canLower())
        windowMenu->disableCommand(actionLower);
    if (!canRaise())
        windowMenu->disableCommand(actionRaise);

    unsigned long func = frameFunctions();

    if (!(func & ffMove))
        windowMenu->disableCommand(actionMove);
    if (!(func & ffResize))
        windowMenu->disableCommand(actionSize);
    if (!(func & ffMinimize))
        windowMenu->disableCommand(actionMinimize);
    if (!(func & ffHide))
        windowMenu->disableCommand(actionHide);
    if (!(func & ffRollup) || !titlebar() || !titlebar()->visible())
        windowMenu->disableCommand(actionRollup);
    if (!(func & ffMaximize))
        windowMenu->disableCommand(actionMaximize);
    if (!(func & ffClose))
        windowMenu->disableCommand(actionClose);

    YMenuItem *item;

    if ((item = windowMenu->findAction(actionRollup)))
        item->setChecked(isRollup());
    if ((item = windowMenu->findAction(actionOccupyAllOrCurrent)))
        item->setChecked(isAllWorkspaces());
#if DO_NOT_COVER_OLD
    if ((item = windowMenu->findAction(actionDoNotCover)))
        item->setChecked(doNotCover());
#endif
    if ((item = windowMenu->findAction(actionFullscreen)))
        item->setChecked(isFullscreen());
    if ((item = windowMenu->findSubmenu(moveMenu)))
        item->setEnabled(!isAllWorkspaces());

    for (int i(0); i < moveMenu->itemCount(); i++) {
        item = moveMenu->getItem(i);
        for (int w(0); w < workspaceCount; w++)
            if (item && item->getAction() == workspaceActionMoveTo[w])
                item->setEnabled(w != getWorkspace());
    }

    for (int j(0); j < layerMenu->itemCount(); j++) {
        item = layerMenu->getItem(j);
        for (int layer(0); layer < WinLayerCount; layer++)
            if (item && item->getAction() == layerActionSet[layer]) {
                bool const e(layer == getActiveLayer());
                item->setEnabled(!e);
                item->setChecked(e);
            }
    }

///    if (trayMenu) {
        for (int k = 0; k < windowMenu->itemCount(); k++) {
            item = windowMenu->getItem(k);
            if (item->getAction() == actionToggleTray) {
                item->setChecked(getTrayOption() != WinTrayIgnore);
            }
        }
///    }
#if 0
    if (trayMenu) for (int k(0); k < trayMenu->itemCount(); k++) {
        item = trayMenu->getItem(k);
        for (int opt(0); opt < WinTrayOptionCount; opt++)
            if (item && item->getAction() == trayOptionActionSet[opt]) {
                bool const e(opt == getTrayOption());
                item->setEnabled(!e);
                item->setChecked(e);
            }
    }
#endif
}

#ifdef CONFIG_SHAPE
void YFrameWindow::setShape() {
    if (!shapesSupported)
        return ;

    if (client()->shaped()) {
        MSG(("setting shape w=%d, h=%d", width(), height()));
        if (isRollup() || isIconic()) {
            XRectangle full;
            full.x = 0;
            full.y = 0;
            full.width = width();
            full.height = height();
            XShapeCombineRectangles(xapp->display(), handle(),
                                    ShapeBounding,
                                    0, 0, &full, 1,
                                    ShapeSet, Unsorted);
        } else {
            XRectangle rect[6];
            int nrect = 0;

            if ((frameDecors() & (fdResize | fdBorder)) == fdResize + fdBorder) {
                rect[0].x = 0;
                rect[0].y = 0;
                rect[0].width = width();
                rect[0].height = borderY();

                rect[1] = rect[0];
                rect[1].y = height() - borderY();

                rect[2].x = 0;
                rect[2].y = borderY();
                rect[2].width = borderX();
                rect[2].height = height() - 2 * borderY();

                rect[3] = rect[2];
                rect[3].x = width() - borderX();

                nrect = 4;
            }

            if (titleY() > 0) {
                rect[nrect].x = borderX();
                rect[nrect].y = borderY();
                rect[nrect].width  = width() - 2 * borderX();
                rect[nrect].height = titleY();
                nrect++;
            }

            if (nrect !=  0)
                XShapeCombineRectangles(xapp->display(), handle(),
                                        ShapeBounding,
                                        0, 0, rect, nrect,
                                        ShapeSet, Unsorted);
            XShapeCombineShape(xapp->display(), handle(),
                               ShapeBounding,
                               borderX(),
                               borderY() + titleY(),
                               client()->handle(),
                               ShapeBounding, nrect ? ShapeUnion : ShapeSet);
        }
    }
}
#endif

void YFrameWindow::layoutShape() {

    if (fShapeWidth != width() ||
        fShapeHeight != height() ||
        fShapeTitleY != titleY() ||
        fShapeBorderX != borderX() ||
        fShapeBorderY != borderY())
    {
        fShapeWidth = width();
        fShapeHeight = height();
        fShapeTitleY = titleY();
        fShapeBorderX = borderX();
        fShapeBorderY = borderY();

#ifdef CONFIG_SHAPE
        if (shapesSupported &&
            (frameDecors() & fdBorder) &&
            !(isIconic() || isFullscreen()))
        {
            int const a(focused() ? 1 : 0);
            int const t((frameDecors() & fdResize) ? 0 : 1);

            Pixmap shape = XCreatePixmap(xapp->display(), desktop->handle(), width(), height(), 1);
            Graphics g(shape, width(), height(), 1);

            g.setColorPixel(1);
            g.fillRect(0, 0, width(), height());

            const int
                xTL(frameTL[t][a] != null ? frameTL[t][a]->width() : 0),
                xTR(width() - (frameTR[t][a] != null ? frameTR[t][a]->width() : 0)),
                xBL(frameBL[t][a] != null ? frameBL[t][a]->width() : 0),
                xBR(width() - (frameBR[t][a] != null ? frameBR[t][a]->width() : 0));
            const int
                yTL(frameTL[t][a] != null ? frameTL[t][a]->height() : 0),
                yBL(height() - (frameBL[t][a] != null ? frameBL[t][a]->height() : 0)),
                yTR(frameTR[t][a] != null ? frameTR[t][a]->height() : 0),
                yBR(height() - (frameBR[t][a] != null ? frameBR[t][a]->height() : 0));

            if (frameTL[t][a] != null) {
                g.copyDrawable(frameTL[t][a]->mask(), 0, 0,
                               frameTL[t][a]->width(), frameTL[t][a]->height(),
                               0, 0);
                if (protectClientWindow)
                    g.fillRect(borderX(), borderY(),
                               frameTL[t][a]->width() - borderX(),
                               frameTL[t][a]->height() - borderY());
            }
            if (frameTR[t][a] != null) {
                g.copyDrawable(frameTR[t][a]->mask(), 0, 0,
                               frameTR[t][a]->width(), frameTR[t][a]->height(),
                               xTR, 0);
                if (protectClientWindow)
                    g.fillRect(xTR, borderY(),
                               frameTR[t][a]->width() - borderX(),
                               frameTR[t][a]->height() - borderY());
            }
            if (frameBL[t][a] != null) {
                g.copyDrawable(frameBL[t][a]->mask(), 0, 0,
                               frameBL[t][a]->width(), frameBL[t][a]->height(),
                               0, yBL);
                if (protectClientWindow)
                    g.fillRect(borderX(), yBL,
                               frameBL[t][a]->width() - borderX(),
                               frameBL[t][a]->height() - borderY());
            }
            if (frameBR[t][a] != null) {
                g.copyDrawable(frameBR[t][a]->mask(), 0, 0,
                               frameBR[t][a]->width(), frameBL[t][a]->height(),
                               xBR, yBR);
                if (protectClientWindow)
                    g.fillRect(xBR, yBR,
                               frameBR[t][a]->width() - borderX(),
                               frameBR[t][a]->width() - borderY());
            }

            if (frameT[t][a] != null)
                g.repHorz(frameT[t][a]->mask(),
                          frameT[t][a]->width(), frameT[t][a]->height(),
                          xTL, 0, xTR - xTL);
            if (frameB[t][a] != null)
                g.repHorz(frameB[t][a]->mask(),
                          frameB[t][a]->width(), frameB[t][a]->height(),
                          xBL, height() - frameB[t][a]->height(), xBR - xBL);
            if (frameL[t][a] != null)
                g.repVert(frameL[t][a]->mask(),
                          frameL[t][a]->width(), frameL[t][a]->height(),
                          0, yTL, yBL - yTL);
            if (frameR[t][a] != null)
                g.repVert(frameR[t][a]->mask(),
                          frameR[t][a]->width(), frameR[t][a]->height(),
                          width() - frameR[t][a]->width(), yTR, yBR - yTR);

            if (titlebar() && titleY())
                titlebar()->renderShape(shape);
            XShapeCombineMask(xapp->display(), handle(),
                              ShapeBounding, 0, 0, shape, ShapeSet);
            XFreePixmap(xapp->display(), shape);
        } else {
            XShapeCombineMask(xapp->display(), handle(),
                              ShapeBounding, 0, 0, None, ShapeSet);
        }
#endif
#ifdef CONFIG_SHAPE
        setShape();
#endif
    }
}

void YFrameWindow::configure(const YRect &r) {
    MSG(("configure %d %d %d %d", r.x(), r.y(), r.width(), r.height()));

    YWindow::configure(r);

    performLayout();
    sendConfigure();
}

void YFrameWindow::performLayout()
{
    layoutTitleBar();
    layoutResizeIndicators();
    layoutClient();
    layoutShape();
    if (affectsWorkArea())
        manager->updateWorkArea();
}

void YFrameWindow::layoutTitleBar() {
    if (titlebar() == 0)
        return;

    if (titleY() == 0) {
        titlebar()->hide();
    } else {
        titlebar()->show();

        int title_width = width() - 2 * borderX();
        titlebar()->setGeometry(
            YRect(borderX(),
                  borderY(),
                  max(1, title_width),
                  titleY()));

        titlebar()->layoutButtons();
    }
}

void YFrameWindow::layoutResizeIndicators() {
    if (((frameDecors() & (fdResize | fdBorder)) == (fdResize | fdBorder)) &&
        !isRollup() && !isMinimized() && (frameFunctions() & ffResize) &&
        !isFullscreen())
    {
        if (!indicatorsCreated)
            createPointerWindows();
        if (!indicatorsVisible) {
            indicatorsVisible = true;

            XMapWindow(xapp->display(), topSide);
            XMapWindow(xapp->display(), leftSide);
            XMapWindow(xapp->display(), rightSide);
            XMapWindow(xapp->display(), bottomSide);

            XMapWindow(xapp->display(), topLeft);
            XMapWindow(xapp->display(), topRight);
            XMapWindow(xapp->display(), bottomLeft);
            XMapWindow(xapp->display(), bottomRight);
        }
    } else {
        if (indicatorsVisible) {
            indicatorsVisible = false;

            XUnmapWindow(xapp->display(), topSide);
            XUnmapWindow(xapp->display(), leftSide);
            XUnmapWindow(xapp->display(), rightSide);
            XUnmapWindow(xapp->display(), bottomSide);

            XUnmapWindow(xapp->display(), topLeft);
            XUnmapWindow(xapp->display(), topRight);
            XUnmapWindow(xapp->display(), bottomLeft);
            XUnmapWindow(xapp->display(), bottomRight);
        }
    }
    if (!indicatorsVisible)
        return;

    XMoveResizeWindow(xapp->display(), topSide,
                      0, 0, width(), borderY());
    XMoveResizeWindow(xapp->display(), leftSide,
                      0, 0, borderX(), height());
    XMoveResizeWindow(xapp->display(), rightSide,
                      width() - borderX(), 0, borderX(), height());
    XMoveResizeWindow(xapp->display(), bottomSide,
                      0, height() - borderY(), width(), borderY());

    XMoveResizeWindow(xapp->display(), topLeft,
                      0, 0, wsCornerX, wsCornerY);
    XMoveResizeWindow(xapp->display(), topRight,
                      width() - wsCornerX, 0, wsCornerX, wsCornerY);
    XMoveResizeWindow(xapp->display(), bottomLeft,
                      0, height() - wsCornerY, wsCornerX, wsCornerY);
    XMoveResizeWindow(xapp->display(), bottomRight,
                      width() - wsCornerX, height() - wsCornerY,
                      wsCornerX, wsCornerY);
}

void YFrameWindow::layoutClient() {
    if (!isRollup() && !isIconic()) {
        //int x = this->x() + borderX();
        //int y = this->y() + borderY();
        int w = this->width() - 2 * borderX();
        int h = this->height() - 2 * borderY() - titleY();



        fClientContainer->setGeometry(
            YRect(borderX(), borderY() + titleY(), w, h));
        fClient->setGeometry(YRect(0, 0, w, h));
    }
}

bool YFrameWindow::canClose() {
    if (frameFunctions() & ffClose)
        return true;
    else
        return false;
}

bool YFrameWindow::canMaximize() {
    if (frameFunctions() & ffMaximize)
        return true;
    else
        return false;
}

bool YFrameWindow::canMinimize() {
    if (frameFunctions() & ffMinimize)
        return true;
    else
        return false;
}

bool YFrameWindow::canRollup() {
    if ((frameFunctions() & ffRollup) && titleY() > 0)
        return true;
    else
        return false;
}

bool YFrameWindow::canHide() {
    if (frameFunctions() & ffHide)
        return true;
    else
        return false;
}

bool YFrameWindow::canLower() {
    if (this != manager->bottom(getActiveLayer()))
        return true;
    else
        return false;
}

bool YFrameWindow::canRaise() {
    for (YFrameWindow *w = prev(); w; w = w->prev()) {
        if (w->visibleNow()) {
            YFrameWindow *o = w;
            while (o) {
                o = o->owner();
                if (o == this)
                    break;
                else if (o == 0)
                    return true;
            }
        }
    }
    return false;
}

bool YFrameWindow::Overlaps(bool above) {
    YFrameWindow *f;
    int w1x2 , w1y2 , w2x2 , w2y2;
    long curWorkspace = manager->activeWorkspace();
    bool B,C,D,E,F,H;

    if (above)
        f = prev();
    else
        f = next();

    while (f) {
        if (!f->isMinimized() && !f->isHidden() && f->visibleOn(curWorkspace)) {
            w2x2 = f->x() + (int)f->width() - 1;
            w2y2 = f->y() + (int)f->height() - 1;
            w1x2 = x() + (int)width() - 1;
            w1y2 = y() + (int)height() - 1;
            B = w2x2 >= x();
            C = y() >= f->y();
            E = w1x2 >= f->x();
            F = w2x2 >= w1x2;
            H = w2y2 >= w1y2;
            if (w1y2 >= f->y()) {
                if (F) {
                    if (E && H) {
                        return true;
                    }
                } else {
                    if (B && !C) {
                        return true;
                    }
                }
            }
            D = w2y2 >= y();
            if (x() >= f->x()) {
                if (C) {
                    if (B && D) {
                        return true;
                    }
                } else {
                    if (F && !H) {
                        return true;
                    }
                }
            } else {
                if (H) {
                    if (C && !F) {
                        return true;
                    }
                } else {
                    if (E && D) {
                        return true;
                    }
                }
            }
        }

        if (above)
            f = f->prev();
        else
            f = f->next();
    }
    return false;
}

/// TODO #warning "should precalculate these"
int YFrameWindow::borderX() const {
    return
        isFullscreen() ? 0 : borderXN();
}

int YFrameWindow::borderXN() const {
    return
        ((frameDecors() & fdBorder) && (hideBordersMaximized ? !isMaximized() : true))
        ? ((frameDecors() & fdResize) ? wsBorderX : wsDlgBorderX)
        : 0;
}

int YFrameWindow::borderY() const {
    return
        isFullscreen() ? 0 : borderYN();
}

int YFrameWindow::borderYN() const {
    return
        ((frameDecors() & fdBorder) && (hideBordersMaximized ? !isMaximized() : true))
        ? ((frameDecors() & fdResize) ? wsBorderY : wsDlgBorderY)
        : 0;
}

int YFrameWindow::titleY() const {
    return isFullscreen() ? 0 : titleYN();
}

int YFrameWindow::titleYN() const {
    if (hideTitleBarWhenMaximized && isMaximized())
        return 0;
    return (frameDecors() & fdTitleBar) ? wsTitleBar : 0;
}

// vim: set sw=4 ts=4 et:
