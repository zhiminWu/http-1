/*
    host.c -- Host class for all HTTP hosts

    The Host class is used for the default HTTP server and for all virtual hosts (including SSL hosts).
    Many objects are controlled at the host level. Eg. URL handlers.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Locals ***********************************/

static HttpHost *defaultHost;

/********************************** Forwards **********************************/

static void manageHost(HttpHost *host, int flags);

/*********************************** Code *************************************/

PUBLIC HttpHost *httpCreateHost()
{
    HttpHost    *host;

    if ((host = mprAllocObj(HttpHost, manageHost)) == 0) {
        return 0;
    }
    if ((host->responseCache = mprCreateCache(MPR_CACHE_SHARED)) == 0) {
        return 0;
    }
    mprSetCacheLimits(host->responseCache, 0, ME_MAX_CACHE_DURATION, 0, 0);

    host->routes = mprCreateList(-1, MPR_LIST_STABLE);
    host->flags = HTTP_HOST_NO_TRACE;
    host->streams = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STABLE);
    httpSetStreaming(host, "application/x-www-form-urlencoded", NULL, 0);
    httpSetStreaming(host, "application/json", NULL, 0);
    httpAddHost(host);
    return host;
}


PUBLIC HttpHost *httpCloneHost(HttpHost *parent)
{
    HttpHost    *host;

    if ((host = mprAllocObj(HttpHost, manageHost)) == 0) {
        return 0;
    }
    /*
        The dirs and routes are all copy-on-write.
        Don't clone ip, port and name
     */
    host->parent = parent;
    host->responseCache = parent->responseCache;
    host->routes = parent->routes;
    host->flags = parent->flags | HTTP_HOST_VHOST;
    host->streams = parent->streams;
    host->secureEndpoint = parent->secureEndpoint;
    host->defaultEndpoint = parent->defaultEndpoint;
    httpAddHost(host);
    return host;
}


static void manageHost(HttpHost *host, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(host->name);
        mprMark(host->parent);
        mprMark(host->responseCache);
        mprMark(host->routes);
        mprMark(host->defaultRoute);
        mprMark(host->defaultEndpoint);
        mprMark(host->secureEndpoint);
        mprMark(host->streams);
    }
}


PUBLIC HttpHost *httpCreateDefaultHost() 
{
    HttpHost    *host;
    HttpRoute   *route;

    if (defaultHost) {
        return defaultHost;
    }
    defaultHost = host = httpCreateHost();
    route = httpCreateRoute(host);
    httpSetRouteName(route, "default");
    httpSetHostDefaultRoute(host, route);
    route->limits = route->http->serverLimits;
    return host;
}


PUBLIC int httpStartHost(HttpHost *host)
{
    HttpRoute   *route;
    int         next;

    for (ITERATE_ITEMS(host->routes, route, next)) {
        httpStartRoute(route);
    }
    for (ITERATE_ITEMS(host->routes, route, next)) {
        if (!route->trace && route->parent && route->parent->trace) {
            route->trace = route->parent->trace;
        }
    }
    return 0;
}


PUBLIC void httpStopHost(HttpHost *host)
{
    HttpRoute   *route;
    int         next;

    for (ITERATE_ITEMS(host->routes, route, next)) {
        httpStopRoute(route);
    }
}


PUBLIC HttpRoute *httpGetHostDefaultRoute(HttpHost *host)
{
    return host->defaultRoute;
}


static void printRoute(HttpRoute *route, int next, bool full)
{
    HttpRouteOp *condition;
    HttpStage   *handler;
    HttpAuth    *auth;
    MprKey      *kp;
    cchar       *methods, *pattern, *target, *index;
    int         nextIndex;

    if (route->flags & HTTP_ROUTE_HIDDEN) {
        return;
    }
    auth = route->auth;
    methods = httpGetRouteMethods(route);
    methods = methods ? methods : "*";
    pattern = (route->pattern && *route->pattern) ? route->pattern : "^/";
    target = (route->target && *route->target) ? route->target : "$&";
    if (full) {
        printf("\n Route [%d]. %s\n", next, route->name);
        printf("    Pattern:      %s\n", pattern);
        if (route->prefix && *route->prefix) {
            printf("    RegExp:       %s\n", route->optimizedPattern);
            printf("    Prefix:       %s\n", route->prefix);
        }
        printf("    Methods:      %s\n", methods);
        printf("    Target:       %s\n", target);
        printf("    Auth:         %s\n", auth->type ? auth->type->name : "-");
        printf("    Home:         %s\n", route->home);
        printf("    Documents:    %s\n", route->documents);
        if (route->sourceName) {
            printf("    Source:       %s\n", route->sourceName);
        }
        if (route->tplate) {
            printf("    Template:     %s\n", route->tplate);
        }
        if (route->indexes) {
            for (ITERATE_ITEMS(route->indexes, index, nextIndex)) {
                printf("    Indexes:      %s \n", index);
            }
        }
        if (route->conditions) {
            for (next = 0; (condition = mprGetNextItem(route->conditions, &next)) != 0; ) {
                printf("    Condition:    %s %s\n", condition->name, condition->details ? condition->details : "");
            }
        }
        if (route->handler) {
            printf("    Handler:      %s\n", route->handler->name);
        }
        if (route->extensions) {
            for (ITERATE_KEYS(route->extensions, kp)) {
                handler = (HttpStage*) kp->data;
                printf("    Extension:    \"%s\" => %s\n", kp->key, handler->name);
            }
        }
        if (route->handlers) {
            for (ITERATE_ITEMS(route->handlers, handler, nextIndex)) {
                printf("    Handler:      %s\n", handler->name);
            }
        }
    } else {
        printf("%-18s %-12s %-8s %-46s %-14s\n", route->name, methods ? methods : "*", 
            auth->type ? auth->type->name : "none", pattern, target);
    }
}


PUBLIC void httpLogRoutes(HttpHost *host, bool full)
{
    HttpRoute   *route;
    int         next, foundDefault;

    if (!host) {
        host = httpGetDefaultHost();
    }
    printf("\n");
    if (!full) {
        printf("%-18s %-12s %-8s %-46s %-14s\n", "Route Name", "Methods", "Auth", "Pattern", "Target");
    }
    for (foundDefault = next = 0; (route = mprGetNextItem(host->routes, &next)) != 0; ) {
        printRoute(route, next - 1, full);
        if (route == host->defaultRoute) {
            foundDefault++;
        }
    }
    /*
        Add the default so LogRoutes can print the default route which has yet been added to host->routes
     */
    if (!foundDefault && host->defaultRoute) {
        printRoute(host->defaultRoute, next - 1, full);
    }
    printf("\n");
}


PUBLIC void httpSetHostName(HttpHost *host, cchar *name)
{
    host->name = sclone(name);
}


PUBLIC int httpAddRoute(HttpHost *host, HttpRoute *route)
{
    HttpRoute   *prev, *item, *lastRoute;
    int         i, thisRoute;

    assert(route);

    if (host->parent && host->routes == host->parent->routes) {
        host->routes = mprCloneList(host->parent->routes);
    }
    if (mprLookupItem(host->routes, route) < 0) {
        if (route->pattern[0] && (lastRoute = mprGetLastItem(host->routes)) && lastRoute->pattern[0] == '\0') {
            /* 
                Insert non-default route before last default route 
             */
            thisRoute = mprInsertItemAtPos(host->routes, mprGetListLength(host->routes) - 1, route);
        } else {
            thisRoute = mprAddItem(host->routes, route);
        }
        if (thisRoute > 0) {
            prev = mprGetItem(host->routes, thisRoute - 1);
            if (!smatch(prev->startSegment, route->startSegment)) {
                prev->nextGroup = thisRoute;
                for (i = thisRoute - 2; i >= 0; i--) {
                    item = mprGetItem(host->routes, i);
                    if (smatch(item->startSegment, prev->startSegment)) {
                        item->nextGroup = thisRoute;
                    } else {
                        break;
                    }
                }
            }
        }
    }
    httpSetRouteHost(route, host);
    return 0;
}


PUBLIC HttpRoute *httpLookupRoute(HttpHost *host, cchar *name)
{
    HttpRoute   *route;
    int         next;

    if (name == 0 || *name == '\0') {
        name = "default";
    }
    if (!host && (host = httpGetDefaultHost()) == 0) {
        return 0;
    }
    for (next = 0; (route = mprGetNextItem(host->routes, &next)) != 0; ) {
        assert(route->name);
        if (smatch(route->name, name)) {
            return route;
        }
    }
    return 0;
}


PUBLIC HttpRoute *httpLookupRouteByPattern(HttpHost *host, cchar *pattern)
{
    HttpRoute   *route;
    int         next;

    if (smatch(pattern, "/") || smatch(pattern, "^/") || smatch(pattern, "^/$")) {
        pattern = "";
    }
    if (!host && (host = httpGetDefaultHost()) == 0) {
        return 0;
    }
    for (next = 0; (route = mprGetNextItem(host->routes, &next)) != 0; ) {
        assert(route->pattern);
        if (smatch(route->pattern, pattern)) {
            return route;
        }
    }
    return 0;
}


PUBLIC void httpResetRoutes(HttpHost *host)
{
    host->routes = mprCreateList(-1, MPR_LIST_STABLE);
}


PUBLIC void httpSetHostDefaultRoute(HttpHost *host, HttpRoute *route)
{
    host->defaultRoute = route;
}


PUBLIC void httpSetDefaultHost(HttpHost *host)
{
    defaultHost = host;
}


PUBLIC void httpSetHostSecureEndpoint(HttpHost *host, HttpEndpoint *endpoint)
{
    host->secureEndpoint = endpoint;
}


PUBLIC void httpSetHostDefaultEndpoint(HttpHost *host, HttpEndpoint *endpoint)
{
    host->defaultEndpoint = endpoint;
}


PUBLIC HttpHost *httpGetDefaultHost()
{
    return defaultHost;
}


PUBLIC HttpRoute *httpGetDefaultRoute(HttpHost *host)
{
    if (host) {
        return host->defaultRoute;
    } else if (defaultHost) {
        return defaultHost->defaultRoute;
    }
    return 0;
}


PUBLIC bool httpGetStreaming(HttpHost *host, cchar *mime, cchar *uri)
{
    MprKey      *kp;

    assert(host);
    assert(host->streams);

    if (schr(mime, ';')) {
        mime = stok(sclone(mime), ";", 0);
    }
    if ((kp = mprLookupKeyEntry(host->streams, mime)) != 0) {
        if (kp->data == NULL || sstarts(uri, kp->data)) {
            /* Type is set to the enable value */
            return kp->type;
        }
    }
    return 1;
}


PUBLIC void httpSetStreaming(HttpHost *host, cchar *mime, cchar *uri, bool enable)
{
    MprKey  *kp;

    assert(host);
    if ((kp = mprAddKey(host->streams, mime, uri)) != 0) {
        /*
            We store the enable value in the key type to save an allocation
         */
        kp->type = enable;
    }
}

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2014. All Rights Reserved.

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
