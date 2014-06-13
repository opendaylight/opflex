/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 * @File list_util.c
 * @brief List Manipulation Utilities
 * 
 * @Revision History:  (latest changes at top) 
 *    DEK - Thur May 31 MST 2001 - created 
 * 
 * @remark Purpose:
 *    The list manipulation utilities are a set of general purpose functions
 *    used to build and access lists of items.  For example, the following
 *    fragment of code (i) inputs and save N lines from standard input,
 *    (ii) displays the N saved lines, and (iii) deletes the saved text:
 *
 *        #include  "list_util.h"
 *        ...
 *        char  buffer[MAXINPUT], *s;
 *        list_p  list = NULL;
 *        ...
 *        while (s = gets (buffer))		-- Input and save text.
 *            list_add (&list, -1, (void *) strdup (s));
 *        for (i = 1;  s = (char *) list_get (list, i);  i++)
 *            printf ("Line %d: %s\n", i, s);	-- Display text.
 *        while (list_delete (&list, 1) != NULL)
 *           ;					-- Delete text.
 *        ...
 *
 * Notes:
 *    These functions are reentrant under VxWorks.
 *
 * Procedures:
 *
 *    list_add() - adds an item to a list.
 *    list_delete() - deletes an item from a list.
 *    list_find() - finds an item in a list.
 *    list_get() - retrieves the value of an item from a list.
 *    list_length() - returns the number of items in a list.
 * 
 */
 
#include <errno.h>			/* System error definitions. */
#include <stdio.h>			/* Standard I/O definitions. */
#include <stdlib.h>			/* Standard C Library definitions. */
#include <stdbool.h>
#include "list-util.h"			/* List manipulation definitions. */
#include "util.h"
#include "vlog.h"
#include "dbug.h"

/*
 *    List Data Structures - each list is represented internally using a
 *       doubly-linked list of nodes.  The PREV link of the first item
 *       in a list points to the last item in the list; the NEXT link
 *       of the last item in a list is always NULL.
 */

typedef  struct  _list_node {
    struct _list_node  *prev;		/* Link to previous node in list. */
    struct _list_node  *next;		/* Link to next node in list. */
    void  *data;			/* Data value for item I. */
}  list_node;

/*
 *  @brief list_add () adds an item to a list.
 *
 *  Invocation:
 *      list_node_p  list = NULL;
 *      ...
 *      status = list_add (&list, position, (void *) item);
 *
 *  where
 * @param0 <list>		- I/O
 *          is the list of items.  If the list of items is empty, a new
 *           list is created and a pointer to the list is returned in LIST.
 * @param1  <position>	- I
 *          specifies the position in the list where ITEM will be inserted.
 *            The new item will normally be inserted to the right of POSITION.
 *           For example, if POSITION is 4, then the new item will become
 *           the 5th item in the list; old items 5..N become items 6..N+1.
 *           If POSITION equals 0, then ITEM is inserted at the front of
 *           the list.  If POSITION equals -1, then ITEM is added at the
 *           end of the list.
 * @param2 <item>		- I
 *           is the item, cast as a (VOID *) pointer, to be added to the list.
 * @return <status>	- O
 *           returns the status of adding ITEM to LIST, zero if there were no
 *           errors and ERRNO otherwise.
 *
 */
int list_add(list_node_p *list, int position, void *item)
{
    static char *mod = "list_add";
    list_node  *node, *prev;

    DBUG_ENTER(mod);
    /* Create a list node. */
    node = (list_node *) xzalloc(sizeof (list_node));
    node->prev = node->next = NULL;
    node->data = item;

    /* Add the item to the list. */
    prev = *list;
    if (prev == NULL) {				/* Brand new list? */
        node->prev = node;
        *list = node;
    } else {					/* Existing list? */
        if (position < 0) {				/* End of list? */
            node->next = prev;  prev = prev->prev;
            node->prev = prev;  prev->next = node;
            prev = node->next;  prev->prev = node;
            node->next = NULL;
        } else if (position == 0) {			/* Beginning of list? */
            node->next = prev;  node->prev = prev->prev;
            prev->prev = node;  *list = node;
        } else {					/* Position I in list? */
            while ((--position > 0) && (prev->next != NULL))
                prev = prev->next;
            node->prev = prev;  node->next = prev->next;  prev->next = node;
            if (node->next == NULL)			/* I at end of list? */
                (*list)->prev = node;
            else					/* I in middle of list? */
                (node->next)->prev = node;
        }
    }
    DBUG_RETURN(0);
}

/*
 * @brief list_delete - deletes an item from a list.  An item being
 *   deleted is denoted by its position, 1..N, in the list; deleting
 *   an item adjusts the positions of all the items that follow in the
 *   list.  To delete an entire list, just keep deleting item #1 until
 *   the list is empty:
 *
 *           while (list_delete (&list, 1) != NULL)
 *              ;
 *
 *   Invocation:
 *       item = list_delete (&list, position);
 *
 *   where
 * @param0 <list>		- I/O
 *           is the list of items.  If the item being deleted is the very first
 *           item in the list, LIST will be updated to point to the "2nd" item
 *           in the list.  If the item being deleted is the only remaining item
 *           in the list, a NULL pointer is returned in LIST.
 * @param1 <position>	- I
 *           specifies the item's position, 1..N, in the list.  Positions
 *           less than 1 or greater than N are ignored.
 * @return <item>		- O
 *           returns the deleted item, cast as a (VOID *) pointer.  NULL is
 *           returned if POSITION is outside of the range 1..N or if N = 0.
 *
 */
void *list_delete (list_node_p *list, int position)
{
    static char *mod = "list_delete";
    list_node  *node, *prev;
    void  *data;

    /* Locate the item in the list. */
    DBUG_ENTER(mod);
    DBUG_PRINT("DEBUG", ("position=%d", position));
    node = *list;
    if ((node == NULL) || (position < 1)) {
        data = NULL;
        goto rtn_return;
    } else if (position == 1) {			/* Item 1 in list? */
        *list = node->next;
        if (node->next != NULL)  (node->next)->prev = node->prev;
    } else {					/* Item 2..N in list? */
        while ((--position > 0) && (node != NULL))
            node = node->next;
        if (node == NULL) {
            data = NULL;
            goto rtn_return;
        }
        prev = node->prev;  prev->next = node->next;
        if (node->next == NULL)			/* Very last item in list? */
            (*list)->prev = prev;
        else					/* Middle of list? */
            (node->next)->prev = prev;
    }

    data = node->data;
    free ((char *) node);
 rtn_return:
    DBUG_LEAVE;
    return (data);
}

/*
 * @breif list_find - finds an item in a list and returns its position
 *    in the list.
 *
 *    Invocation:
 *       position = list_find (list, item);
 *
 *   where
 * @param0 <list>		- I
 *           is the list of items.
 * @param1 <findFunc>		- I
 *           pointer to a fuctions that expects the item,s as a ptr
 *           this routine will return a True if a match occurs, else 
 *           false. By doing this the call will retain the knowledge of
 *           the data structure (i.e. item).  
 * @return <position>	- O
 *           returns the item's position, 1..N, in LIST.  If the item is
 *           not found or if the list is empty, zero is returned.
 */
int list_find (list_node_p list, void *item, bool (*findFunc)(void *, void *))
{
    static char *mod = "list_find";
    int  i;

    DBUG_ENTER(mod);
    for (i = 1;  list != NULL;  list = list->next, i++)
        if (findFunc(item, list->data)) break;

    DBUG_PRINT("DEBUG", ("%s: pos=%d", mod, (list == NULL) ? 0 : i));
    DBUG_LEAVE;
    return ((list == NULL) ? 0 : i);
}

/*
 * @breif list_find_with_dp - finds an item in a list and returns its position
 *    in the list using the data pointer
 *
 *    Invocation:
 *       position = list_find_with_dp (list, item, dp);
 *
 *   where
 * @param0 <list>		- I
 *           is the list of items.
 * @param1 <item>       - I
 *           pointer to the item we're looking for
 * @return <position>	- O
 *           returns the item's position, 1..N, in LIST.  If the item is
 *           not found or if the list is empty, zero is returned.
 */
int list_find_with_dp (list_node_p list, void *item)
{
    int  i;

    for (i = 1;  list != NULL;  list = list->next, i++)
        if (list->data == item)
            break;

    return ((list == NULL) ? 0 : i);
}

/*
 * @brief list_get ()
 *
 * Purpose:
 *   Function list_get() returns the I-th item from a list.
 *
 *   Invocation:
 *       item = (<type>) list_get (list, position);
 *
 * where
 * @param0 <list>		- I
 *           is the list of items.
 * @param1 <position>	- I
 *           specifies the desired item's position, 1..N, in the list.
 *           If POSITION is -1, LIST_GET() returns the last item in the
 *           list.  Positions 0 and N+1... are invalid.
 * @return <item>		- O
 *           returns the deleted item, cast as a (VOID *) pointer.  NULL is
 *           returned if POSITION > N or if POSITION = 0 or if N = 0.
 *
 */
void *list_get (list_node_p list, int position)
{
    static char *mod = "list_get";
    void *rtnp = NULL;
    
    DBUG_ENTER(mod);
    DBUG_PRINT("DEBUG", ("%s: position=%d", mod, position));
    if (list == NULL)			/* Empty list? */
        goto rtn_return;
    else if (position < 0) {		/* Return last item? */
        rtnp = (list->prev)->data;
        /* return ((list->prev)->data); */
        goto rtn_return;
    }
    else if (position == 0)		/* I = 0? */
        goto rtn_return;

    /* Position to the desired item in the list. */

    while ((--position > 0) && (list != NULL))
        list = list->next;
    if (list == NULL) {			/* I > N */
        rtnp = NULL;
        goto rtn_return;
    } else	{			/* 1 <= I <= N */
        rtnp = list->data;
        goto rtn_return;
    }

 rtn_return:
    DBUG_LEAVE;
    return(rtnp);
}

/*
 * list_length- returns the number of items in a list.
 *
 *   Invocation:
 *       numItems = list_length (list);
 *
 *   where
 * @param0 <list>		- I
 *           is the list of items.
 * @return <numItems>	- O
 *            returns the number of items in the list.
 */
int list_length (list_node_p list)
{ 
    static char *mod = "list_length";
    int  count;

    DBUG_ENTER(mod);
    /* Count the number of items in the list. */
    for (count = 0;  list != NULL;  count++)
        list = list->next;

    DBUG_PRINT("DEBUG", ("count=%d", count));
    DBUG_LEAVE;
    return(count);
}
