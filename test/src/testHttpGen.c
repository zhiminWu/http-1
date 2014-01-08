/**
    testHttpGen.c - tests for HTTP
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "http.h"

/*********************************** Locals ***********************************/

typedef struct TestHttp {
    Http        *http;
    HttpConn    *conn;
} TestHttp;

static void manageTestHttp(TestHttp *th, int flags);

/************************************ Code ************************************/

static int initHttp(MprTestGroup *gp)
{
    MprSocket   *sp;
    TestHttp     *th;

    gp->data = th = mprAllocObj(TestHttp, manageTestHttp);
    
    if (getenv("NO_INTERNET")) {
        gp->skip = 1;
        return 0;
    }
    sp = mprCreateSocket(NULL);
    
    /*
        Test if we have network connectivity. If not, then skip these tests.
     */
    if (mprConnectSocket(sp, "www.google.com", 80, 0) < 0) {
        static int once = 0;
        if (once++ == 0) {
            mprPrintf("%12s Disabling tests %s.*: no internet connection. %d\n", "[Notice]", gp->fullName, once);
        }
        gp->skip = 1;
    }
    mprCloseSocket(sp, 0);
    return 0;
}


static void manageTestHttp(TestHttp *th, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(th->http);
        mprMark(th->conn);
    }
}


static void testCreateHttp(MprTestGroup *gp)
{
    TestHttp    *th;
    Http        *http;

    th = gp->data;
    th->http = http = httpCreate(HTTP_SERVER_SIDE);
    assert(http != 0);
    httpDestroy(http);
}


static void testBasicHttpGet(MprTestGroup *gp)
{
    TestHttp    *th;
    Http        *http;
    HttpConn    *conn;
    MprOff      length;
    int         rc, status;

    th = gp->data;
    th->http = http = httpCreate(HTTP_CLIENT_SIDE);
    assert(http != 0);

    th->conn = conn = httpCreateConn(http, NULL, gp->dispatcher);

    rc = httpConnect(conn, "GET", "http://embedthis.com/index.html", NULL);
    assert(rc >= 0);
    if (rc >= 0) {
        httpFinalize(conn);
        httpWait(conn, HTTP_STATE_COMPLETE, 10 * 1000);
        status = httpGetStatus(conn);
        assert(status == 200 || status == 302);
        if (status != 200 && status != 302) {
            mprLog(0, "HTTP response status %d", status);
        }
        assert(httpGetError(conn) != 0);
        length = httpGetContentLength(conn);
        assert(length != 0);
    }
    httpDestroy(http);
}


#if BIT_FEATURE_SSL && (BIT_FEATURE_MATRIXSSL || BIT_FEATURE_OPENSSL)
static void testSecureHttpGet(MprTestGroup *gp)
{
    TestHttp    *th;
    Http        *http;
    HttpConn    *conn;
    int         rc, status;

    th = gp->data;
    th->http = http = httpCreate(HTTP_CLIENT_SIDE);
    assert(http != 0);
    th->conn = conn = httpCreateConn(http, NULL, gp->dispatcher);
    assert(conn != 0);

    rc = httpConnect(conn, "GET", "https://www.ibm.com/", NULL);
    assert(rc >= 0);
    if (rc >= 0) {
        httpFinalize(conn);
        httpWait(conn, HTTP_STATE_COMPLETE, MPR_TIMEOUT_SOCKETS);
        status = httpGetStatus(conn);
        assert(status == 200 || status == 301 || status == 302);
        if (status != 200 && status != 301 && status != 302) {
            mprLog(0, "HTTP response status %d", status);
        }
    }
    httpDestroy(http);
}
#endif


#if FUTURE && TODO
    httpRequest
#endif


MprTestDef testHttpGen = {
    "http", 0, initHttp, 0,
    {
        MPR_TEST(0, testCreateHttp),
        MPR_TEST(0, testBasicHttpGet),
#if BIT_FEATURE_SSL
        MPR_TEST(0, testSecureHttpGet),
#endif
        MPR_TEST(0, 0),
    },
};

/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2014. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2014. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */