// small icon view (positional view, not list)

#include "ctlspriv.h"
#include "listview.h"

#ifdef  WIN32JV
void WINAPI SHDrawText();
#endif

void NEAR PASCAL ListView_SDrawItem(LV* plv, int i, HDC hdc, LPPOINT lpptOrg, RECT FAR* prcClip, UINT fDraw)
{
    RECT rcIcon;
    RECT rcLabel;
    RECT rcBounds;
    RECT rcT;

    ListView_GetRects(plv, i, &rcIcon, &rcLabel, &rcBounds, NULL);

    if (!prcClip || IntersectRect(&rcT, &rcBounds, prcClip))
    {
        TCHAR ach[CCHLABELMAX*2];
        LV_ITEM item;
        UINT fText;

	if (lpptOrg)
	{
	    OffsetRect(&rcIcon, lpptOrg->x - rcBounds.left,
	    			lpptOrg->y - rcBounds.top);
	    OffsetRect(&rcLabel, lpptOrg->x - rcBounds.left,
	    			lpptOrg->y - rcBounds.top);
	}

        item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = ach;
        item.cchTextMax = sizeof(ach);
        item.stateMask = LVIS_ALL;
        ListView_OnGetItem(plv, &item);

        fText = ListView_DrawImage(plv, &item, hdc,
            rcIcon.left, rcIcon.top, fDraw);

        // Don't draw label if it's being edited...
        //
        if (plv->iEdit != i)
        {
	    if (fDraw & LVDI_TRANSTEXT)
                fText |= SHDT_TRANSPARENT;

            if (item.pszText)
            {
                SHDrawText(hdc, item.pszText, &rcLabel, LVCFMT_LEFT, fText,
                               plv->cyLabelChar, plv->cxEllipses, plv->clrText, plv->clrTextBk);
            }

            if ((fDraw & LVDI_FOCUS) && (item.state & LVIS_FOCUSED))
                DrawFocusRect(hdc, &rcLabel);
        }
    }
}

int NEAR ListView_SItemHitTest(LV* plv, int x, int y, UINT FAR* pflags)
{

    int iHit;
    UINT flags;
    POINT pt;

    // Map window-relative coordinates to view-relative coords...
    //
    pt.x = x + plv->ptOrigin.x;
    pt.y = y + plv->ptOrigin.y;

    // If we find an uncomputed item, recompute them all now...
    //
    if (plv->rcView.left == RECOMPUTE)
        ListView_Recompute(plv);

    flags = 0;
    for (iHit = 0; iHit < ListView_Count(plv); iHit++)
    {
        LISTITEM FAR* pitem = ListView_FastGetZItemPtr(plv, iHit);
        POINT ptItem;
        RECT rcLabel;
        RECT rcIcon;

        ptItem.x = pitem->pt.x;
        ptItem.y = pitem->pt.y;

        rcIcon.top    = ptItem.y;
        rcIcon.bottom = ptItem.y + plv->cyItem;

        rcLabel.top    = rcIcon.top;
        rcLabel.bottom = rcIcon.bottom;

        // Quick, easy rejection test...
        //
        if (pt.y < rcIcon.top || pt.y >= rcIcon.bottom)
            continue;

        rcIcon.left   = ptItem.x;
        rcIcon.right  = ptItem.x + g_cxSmIcon;

        rcLabel.left   = rcIcon.right;
        rcLabel.right  = rcLabel.left + pitem->cxSingleLabel;

        if (PtInRect(&rcIcon, pt))
        {
            flags = LVHT_ONITEMICON;
            break;
        }

        if (PtInRect(&rcLabel, pt))
        {
            flags = LVHT_ONITEMLABEL;
            break;
        }
    }


    if (flags == 0)
    {
        flags = LVHT_NOWHERE;
        iHit = -1;
    }
    else
    {
        iHit = DPA_GetPtrIndex(plv->hdpa, (void FAR*)ListView_FastGetZItemPtr(plv, iHit));
    }

    *pflags = flags;
    return iHit;
}

void NEAR ListView_SGetRects(LV* plv, LISTITEM FAR* pitem, RECT FAR* prcIcon, RECT FAR* prcLabel)
{
    RECT rcIcon;
    RECT rcLabel;

    // Test for NULL item passed in
    if (pitem == NULL)
    {
        SetRectEmpty(prcIcon);
        SetRectEmpty(prcLabel);
        return;
    }

    if (pitem->pt.y == RECOMPUTE || pitem->cyMultiLabel == RECOMPUTE)
        ListView_Recompute(plv);

    rcIcon.left   = pitem->pt.x - plv->ptOrigin.x;
    rcIcon.right  = rcIcon.left + g_cxSmIcon;
    rcIcon.top    = pitem->pt.y - plv->ptOrigin.y;
    rcIcon.bottom = rcIcon.top + plv->cyItem;
    *prcIcon = rcIcon;

    rcLabel.left   = rcIcon.right;
    rcLabel.right  = rcLabel.left + pitem->cxSingleLabel;
    rcLabel.top    = rcIcon.top;
    rcLabel.bottom = rcIcon.bottom;
    *prcLabel = rcLabel;
}

// Return the index of the first item >= *pszLookup.
//
int NEAR ListView_LookupString(LV* plv, LPCTSTR pszLookup, UINT flags)
{
    int i, j;
    BOOL fExact;

    if (!pszLookup)
        return 0;

    i = 0;
    j = ListView_Count(plv) - 1;
    fExact = FALSE;
    while (i <= j)
    {
        int k = (i + j) / 2;
        LISTITEM FAR* pitem = ListView_FastGetItemPtr(plv, k);
        int result;

        result = ListView_CompareString(plv, pitem,
                k, pszLookup, flags);

        if (plv->style & LVS_SORTDESCENDING)
            result = -result;

        switch (result)
        {
        case 0:
            fExact = TRUE;
        case 1:
            j = k - 1;
            break;
        case -1:
            i = k + 1;
            break;
        }
    }
    // For substrings, return index only if exact match was found.
    //
    if ((flags & (LVFI_SUBSTRING | LVFI_PARTIAL)) && !fExact)
        return -1;

    if (i < 0)
        return 0;
    return i;
}
