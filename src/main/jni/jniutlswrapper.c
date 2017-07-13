/* -*-mode: C; compile-command: "cd ..; ../scripts/ndkbuild.sh -j3"; -*- */
/* 
 * Copyright 2001-2010 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#include "strutils.h"
#include "jniutlswrapper.h"
#include "andutils.h"


struct JNIUtilCtxt {
    JNIEnv** envp;
    jobject jjniutil;
    MPSLOT;
};

JNIUtilCtxt* 
makeJNIUtil( MPFORMAL JNIEnv** envp, jobject jniutls )
{
    JNIUtilCtxt* ctxt = (JNIUtilCtxt*)XP_CALLOC( mpool, sizeof( *ctxt ) );
    JNIEnv* env = *envp;
    ctxt->envp = envp;
    ctxt->jjniutil = (*env)->NewGlobalRef( env, jniutls );
    MPASSIGN( ctxt->mpool, mpool );
    return ctxt;
}

void
destroyJNIUtil( JNIUtilCtxt** ctxtp )
{
    JNIUtilCtxt* ctxt = *ctxtp;
    if ( !!ctxt ) {
        JNIEnv* env = *ctxt->envp;
        (*env)->DeleteGlobalRef( env, ctxt->jjniutil );
        XP_FREE( ctxt->mpool, ctxt );
        *ctxtp = NULL;
    }
}

/* These are called from anddict.c, not via vtable */
#ifndef DROP_BITMAPS
jobject
and_util_makeJBitmap( JNIUtilCtxt* jniutil, int nCols, int nRows, 
                      const jboolean* colors )
{
    jobject bitmap;
    JNIEnv* env = *jniutil->envp;
    jmethodID mid
        = getMethodID( env, jniutil->jjniutil, "makeBitmap", 
                       "(II[Z)Landroid/graphics/drawable/BitmapDrawable;" );

    jbooleanArray jcolors = makeBooleanArray( env, nCols*nRows, colors );

    bitmap = (*env)->CallObjectMethod( env, jniutil->jjniutil, mid,
                                       nCols, nRows, jcolors );
    deleteLocalRef( env, jcolors );

    return bitmap;
}
#endif

jobject
and_util_splitFaces( JNIUtilCtxt* jniutil, const XP_U8* bytes, jsize len,
                     XP_Bool isUTF8 )
{
    jobject strarray = NULL;
    JNIEnv* env = *jniutil->envp;
    jmethodID mid
        = getMethodID( env, jniutil->jjniutil, "splitFaces",
                       "([BZ)[[Ljava/lang/String;" );

    jbyteArray jbytes = makeByteArray( env, len, (jbyte*)bytes );
    strarray = 
        (*env)->CallObjectMethod( env, jniutil->jjniutil, mid, jbytes, isUTF8 );
    deleteLocalRef( env, jbytes );

    return strarray;
}

jstring
and_util_getMD5SumFor( JNIUtilCtxt* jniutil, const XP_UCHAR* name,
                       const XP_U8* bytes, jsize len )
{
    JNIEnv* env = *jniutil->envp;
    jmethodID mid = getMethodID( env, jniutil->jjniutil, "getMD5SumFor",
                                 "(Ljava/lang/String;[B)Ljava/lang/String;" );
    jstring jname = (*env)->NewStringUTF( env, name );
    jbyteArray jbytes = NULL == bytes? NULL
        : makeByteArray( env, len, (jbyte*)bytes );
    jstring result = 
        (*env)->CallObjectMethod( env, jniutil->jjniutil, mid, jname, jbytes );
    deleteLocalRefs( env, jname, jbytes, DELETE_NO_REF );
    return result;
}
