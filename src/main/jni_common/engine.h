 /* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2002 by Eric House (xwords@eehouse.org).  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _ENGINE_H_
#define _ENGINE_H_

#include "comtypes.h"
#include "dictnry.h"

#ifdef CPLUS
extern "C" {
#endif

#ifdef XWFEATURE_SEARCHLIMIT
typedef struct BdHintLimits {
    XP_U16 left;
    XP_U16 top;
    XP_U16 right;
    XP_U16 bottom;
} BdHintLimits;
#endif

XP_U16 engine_getScoreCache( EngineCtxt* engine, XP_U16 row );

EngineCtxt* engine_make( MPFORMAL XW_UtilCtxt* util );

void engine_writeToStream( EngineCtxt* ctxt, XWStreamCtxt* stream );
EngineCtxt* engine_makeFromStream( MPFORMAL XWStreamCtxt* stream,
                                   XW_UtilCtxt* util );

void engine_init( EngineCtxt* ctxt );
void engine_reset( EngineCtxt* ctxt );
void engine_destroy( EngineCtxt* ctxt );

XP_Bool engine_findMove( EngineCtxt* ctxt, const ModelCtxt* model, 
                         XP_U16 turn, const Tile* tiles, 
                         XP_U16 nTiles, XP_Bool usePrev,
#ifdef XWFEATURE_BONUSALL
                         XP_U16 allTilesBonus, 
#endif
#ifdef XWFEATURE_SEARCHLIMIT
                         const BdHintLimits* boardLimits,
                         XP_Bool useTileLimits,
#endif
                         XP_U16 robotIQ, XP_Bool* canMove, MoveInfo* result );
XP_Bool engine_check( DictionaryCtxt* dict, Tile* buf, XP_U16 buflen );

#ifdef CPLUS
}
#endif

#endif /* _ENGINE_H_ */
